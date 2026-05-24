[BITS 16]                       ; Tell NASM we're writing 16-bit code
[ORG 0x7C00]                    ; Tell NASM our code will be loaded at 0x7C00 (BIOS always loads the bootloader here)

KERNEL_OFFSET equ 0x1000        ; Where we'll load the kernel in memory (well below our bootloader)
MEMORY_MAP_COUNT_ADDRESS equ 0x600    ; Store count here - just after the map

start:
    mov [boot_drive], dl        ; BIOS puts boot drive in DL - save it immediatley

    ; -- Detect memory via BIOS INT 0x15, E820 --------------------------
    ; E820 asks the BIOS for a map of physical memory
    ; It fills a table at MEMORY_MAP_ADDRESS with entries describing
    ; each region - usable RAM, reserved, ACPI etc.
    ; We must do this in real mode before entering protected mode
    ; because INT 0x15 is a BIOS call and BIOS only works in real mode

    xor ax, ax
    mov es, ax                  ; ES = 0

    xor ebx, ebx                ; Continuation value - 0 = start of map
    mov word [0x600], 0         ; Zero the entry count
    mov di, 0x500               ; Buffer address for entries

.e820_loop:
    mov eax, 0xE820             ; Function code - must reset each iteration
    mov edx, 0x534D4150         ; 'SMAP' magic - must reset each iteration
    mov ecx, 24                 ; Entry size - must reset each iteration
    mov word [di + 20], 1       ; ACPI extended attribute
    mov word [di + 22], 0       ; Zero upper word
    int 0x15

    jc .e820_done                ; Carry = no more entries or not supported
    cmp eax, 0x534D4150          ; Verify BIOS returned SMAP
    jne .e820_done               ; If not - something went wrong

    cmp ecx, 0                  ; Skip zero length entries
    je .e820_next

    inc word [0x600]            ; Increment entry count
    add di, 24                  ; Advance buffer pointer

.e820_next:
    test ebx, ebx               ; EBX = 0 means last entry
    je .e820_done
    jmp .e820_loop               ; Get next entry

.e820_done:
    ; --- Now set up segments and stack ------------------------

    ; Segment register setup - zero them all out
    xor ax, ax                  ; AX = 0 (xor is the idiomatic way to zero a register)
    mov ds, ax                  ; Data segment = 0 - we're working at address 0
    mov es, ax                  ; Extra segment = 0 - same reason
    mov ss, ax                  ; Stack segment = 0 - stack lives at segment 0
    mov sp, 0x7C00              ; Stack pointer just below our bootloader in memory
                                ; Stack grows downwards so it won't hit our code

    mov ah, 0x00                ; BIOS function: set video mode
    mov al, 0x03                ; Mode 3 = 80x25 colour text mode
    int 0x10                    ; Call BIOS video interrupt - clears the screen

    mov si, msg_load            ; Point SI register at our message
    call print                  ; Call our print routine

    call load_kernel            ; Load kernel from disk

    cli                         ; Disable interrupts - critial before switching
                                ; modes, we don't want an interrupt firing halfway
                                ; through the switch

    lgdt [gdt_descriptor]       ; Load our Global Descriptor Table into the CPU
                                ; The CPU needs this before entering protected mode

    mov eax, cr0                ; Read Control Register 0 into EAX
                                ; CR0 controls fundamental CPU operating modes
    or eax, 1                   ; Set bit 0 - the Protection Enable (PE) bit
    mov cr0, eax                ; Write it back - CPU is now in protected mode!

    jmp 0x08:protected_mode     ; Far jump to flush the CPU pipeline
                                ; 0x08 = code segment selector in our GDT   
                                ; This jump makes the code switch take effect

; --- Print routine -----------------------------------------------------
print:
    mov ah, 0x0E                ; BIOS teletype function
.loop:
    lodsb                       ; Load byte at [SI] into AL, then increment SI
    cmp al, 0                   ; Is it the null terminator?
    je .done                    ; If yes, we're finished
    int 0x10                    ; Otherwise, print the character
    jmp .loop                   ; And loop

.done: 
    ret                         ; Return to caller

; --- Disk load -----------------------------------------------------
load_kernel:
    mov si, msg_disk            ; Print the loading message 
    call print

    mov ah, 0x02                ; BIOS read sectors function
    mov al, 50                  ; Number of sectors to read
    mov ch, 0                   ; Cylinder number 0
    mov cl, 2                   ; Start from sector 2 (sector 1 is our bootloader)
    mov dh, 0                   ; Head number 0
    mov dl, [boot_drive]        ; Use the saved drive number
    mov bx, KERNEL_OFFSET       ; Load into memory at 0x1000
    int 0x13                    ; Call BIOS disk interrupt
    jc disk_error               ; Jump if carry flag set (error)
    ret

disk_error:
    mov si, msg_error           ; Print the error message
    call print
    jmp $                       ; Hang forever

boot_drive db 0                 ; 1 byte to store the drive 

msg_load db 'acornOS booting...', 13, 10, 0
msg_disk db 'Loading kernel from disk...', 13, 10, 0
msg_error db 'Disk read error!', 13, 10, 0
; 13 = carriage return, 10 = newline, 0 = null terminator

; --- GDT -----------------------------------------------------
gdt_start:
    ; Null descriptor - required by CPU spec, must be all zeros
    dd 0x00000000               ; First 4 bytes: zero
    dd 0x00000000               ; Next 4 bytes: zero

    ; Code segment descriptor - the CPU fetches instructions from here
    dw 0xFFFF                   ; Limit bits 0-15: segment covers full 4GB
    dw 0x0000                   ; Base bits 0-15: segment starts at address 0
    db 0x00                     ; Base bits 16-23: still address 0
    db 10011010b                ; Access byte:
                                ;   bit 7: present in memory (1)
                                ;   bits 5-6: ring 0 / kernel privilege (00)
                                ;   bit 4: code/data descriptor (1)
                                ;   bit 3: code segment (1)
                                ;   bit 1: readable (1)
    db 11001111b                ; Flags + limit bits 16-19:
                                ;   bit 7: 4KB granulaity (limit is in 4KB pages)
                                ;   bit 6: 32-bit segment
                                ;   bits 0-3: limit bits 16-19 (0x0F)
    db 0x00                     ; Base bits 24-31: still address 0

    ; Data segment descriptor - the CPU reads/writes data from here
    dw 0xFFFF                   ; Limit: full 4GB
    dw 0x0000                   ; Base: address 0
    db 0x00
    db 10010010b                ; Access: same as code but bit 3 = 0 (data)
                                ; and bit 1 = writable instead of readable
    db 11001111b                ; Same flags as code segment
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size minus 1
                                ; CPU adds 1 back, this is just the spec
    dd gdt_start                ; Physical address of the GDT in memory

; --- Protected mode entry -----------------------------------------------------

[BITS 32]                       ; Everthing below here is 32-bit code
protected_mode:
    mov ax, 0x10                ; 0x10 = offset of data descriptor in GDT
                                ; (null=0x00, code=0x80, data=0x10, each 8 bytes)
    mov ds, ax                  ; Point all data segments at our data descriptor
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000            ; Set up a fresh 32-bit stack in high memory
                                ; well away from our bootloader and kernel

    ; Push memory map info as arguments to kernel_main
    ; These will be on the stack when kernel_main is called
    ; C calling convention: arguments pushed right to left
    movzx eax, word [0x600]     ; Read count from the fixed address
                                ; Read the count we saved during boot
                                ; memory_map_count is a 16-bit value in the bootloader

    push eax                    ; Push count as second argument
    push dword 0x500            ; Push map address as first argument

    call KERNEL_OFFSET          ; jump to the kernel
                                ; this is where kernel_main() is loaded

    jmp $                       ; Infinite loop - hang here forever if the kernel returns

;--- Boot Signature ---------------------------------------------------
times 510-($-$$) db 0           ; Pad with zeroes up to byte 510
dw 0xAA55                       ; Magic bytes - BIOS checks for these to confirm
                                ; this is a valid bootable sector