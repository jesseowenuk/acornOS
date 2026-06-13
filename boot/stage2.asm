[BITS 16]
[ORG 0x7E00]

; --- Stage 2 Bootloader --------------------------------------------
; Loaded by Stage 1 at 0x7E00
; We start in 16-bit real mode - same as stage 1 left us
; Our job is to get from here to 64-bit long mode

; --- Constants ------------------------------------------------------
; We'll load kernel here (1MB physical)
KERNEL_PHYSICAL_BASE equ 0x100000

; Temporary stack during boot
BOOT_STACK_TOP equ 0x7000

; We'll store E820 map here
E820_MAP_ADDRESS equ 0x700

; We'll store E820 count here
E820_COUNT_ADDRESS equ 0x500

; --- Entry point -----------------------------------------------------
start:
    ; Stage 1 jumped here with DL = boot drive
    ; Save it immediatley
    mov [boot_drive], dl

    ; Set up segments - all zero for flat real mode
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov sp, BOOT_STACK_TOP              ; Temporary stack during real mode

    ; Print hello message so we know stage 2 is running
    mov si, msg_hello
    call print
    
    ; Enable A20 Line
    call enable_a20
    mov si, msg_a20
    call print

    ; Detect memory via E820
    call detect_memory
    mov si, msg_memory
    call print

    ; Load kernel from disk
    call load_kernel
    mov si, msg_kernel
    call print

    ; Disable interrupts - critical before mode switch
    cli

    ; Enter Protected mode
    call enter_protected_mode

    ; We never get here - enter_protected_mode jumps away

    jmp $

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

; --- Enable A20 line -----------------------------------------------------
; The A20 line must be enabled to access memory above 1MB
; We try three methods in order of preference
;   1. BIOS method (cleanest)
;   2. Fast A20 via port 0x92 (most common)
;   3. Keyboard controller method (old but reliable)

enable_a20:
    ; Method 1: BIOS INT 0x15 AX=0x2401
    mov ax, 0x2401
    int 0x15
    jnc .done                   ; Carry clear = success

    ; Method 2: Fast A20 via port 0x92
    in al, 0x92                 ; Read current value
    test al, 2                  ; Is A20 already set?
    jnz .done                   ; Yes - already enabled
    or al, 2                    ; Set bit 1 = A20 enable
    and al, 0xFE                ; Clear bit 0 to avoid reset
    out 0x92, al                ; Write back

.done:
    ; Verify A20 is actually enabled
    ; Write a value to 0x000500 and read it back from 0x100500
    ; If they're different A20 is enabled (different memory locations)
    ; Otherwise, A20 is not enabled (same location due to memory wrap)
    push ds
    push es

    xor ax, ax
    mov ds, ax                      ; DS = 0x0000
    mov ax, 0xFFFF              
    mov es, ax                      ; ES = 0xFFFF

    mov word [ds:0x0500], 0xAA55    ; Write to 0x000500
    mov word [es:0x0510], 0x55AA    ; Write to 0x100500
    
    cmp word [ds:0x0500], 0xAA55    ; Did first write survive?
    jne .a20_ok                     ; Different = A20 working

    ; A20 still disabled - try keyboard controller method
    call enable_a20_keyboard

.a20_ok:
    pop es
    pop ds
    ret

; Keyboard controller A20 enable (last resort)
enable_a20_keyboard:
    cli
    call .wait_keyboard
    mov al, 0xAD                ; Disable keyboard
    out 0x64, al
    call .wait_keyboard
    mov al, 0xD0                ; Read output port
    out 0x64, al
    call .wait_data
    in al, 0x60                 ; Read data
    push ax
    call .wait_keyboard
    mov al, 0xD1                ; Write to output port
    out 0x64, al
    call .wait_keyboard
    pop ax
    or al, 2                    ; Set A20 bit
    out 0x60, al
    call .wait_keyboard
    mov al, 0xAE                ; Enable keyboard
    out 0x64, al
    call .wait_keyboard
    sti
    ret

.wait_keyboard:
    in al, 0x64
    test al, 2
    jnz .wait_keyboard
    ret

.wait_data:
    in al, 0x64
    test al, 1
    jz .wait_data
    ret

;--- E820 Memory Detection ------------------------------------------------
; Asks BIOS for complete memory map
; Stores entries at E820_MAP_ADDRESS
; Stores count at E820_COUNT_ADDRESS

detect_memory:
    xor ax, ax
    mov es, ax                          ; ES = 0 for buffer addressing
    xor ebx, ebx                        ; EBX = 0 to start enumeration
    mov word [E820_COUNT_ADDRESS], 0    ; Zero the count
    mov di, E820_MAP_ADDRESS            ; DI points to our buffer

.loop:
    mov eax, 0xE820                     ; Function code - reset each iteration
    mov edx, 0x534D4150                 ; 'SMAP' magic - reset each iteration
    mov ecx, 24                         ; Entry size - reset each iteration
    mov dword [di+20], 1                ; ACPI extention attribute
    int 0x15                            ; Call BIOS

    jc .done                            ; Carry = end of list error
    cmp eax, 0x534D4150                 ; Verify BIOS returned SMAP
    jne .done                           ; Something went wrong

    test ecx, ecx                       ; Skip zero length entries
    jz .next

    inc word [E820_COUNT_ADDRESS]       ; Increment entry count
    add di, 24                          ; Advance buffer pointer
    jz .next                            

.next:
    test ebx, ebx                       ; EBX = 0 means last entry
    jz .done
    jmp .loop

.done:
    ret

; --- Load Kernel ---------------------------------------------------------
; Reads kernel sector count from sector 63
; Loads kernel to KERNEL_PHYSICAL_BASE (0x100000)

load_kernel:
    ; Step 1: read kernel sector count from sector 63
    ; We store it as a 4-byte little-endian value
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, packet_meta
    int 0x13
    jc .disk_error

    ; Step 2: read sector count from buffer
    ; It was loaded to 0x5000
    mov eax, [0x5000]               ; Read 4-byte sector count
    mov [kernel_sectors], eax       ; Save it
    test eax, eax                   ; Sanity check - not zero?
    jz .disk_error

    ; Step 3: build LBA packet for kernel load
    ; Kernel lives at sector 64 on disk
    ; Kernel physical address = 0x100000
    ; As we're in 16-bit can only access 64KB segments so only
    ; load 50 sector chunks

    ; We use segment:offset addressing
    ; Physical address = segment_address * 16 + offset
    ; 0x100000 = 0x1000 * 16 + 0 = segment 0x1000, offset 0

    mov dword [packet_kernel + 8], 64           ; Start LBA = sector 64
    mov dword [packet_kernel + 12], 0           ; LBA high = 0

    ; Set up buffer at segment 0x1000 offset 0 = physical address 0x100000
    mov word [packet_kernel + 4], 0x0000        ; Buffer offset = 0
    mov word [packet_kernel + 6], 0x1000        ; Buffer segment = 0x10000

    ; How many sectors can we load at once?
    ; Each sector = 512 bytes
    ; Segment is 64KB = 128 sectors max before we wrap
    ; We'll load 50 sectors at a time to be safe

    mov ecx, [kernel_sectors]                   ; Total sectors to load
    mov ebx, 64                                 ; Current LBA sector
    mov edx, 0x1000                             ; Current segment

.load_loop:
    test ecx, ecx                               ; Any sectors left?
    jz .done                                    ; No? Then we're done

    ; How many to load this iteration
    cmp ecx, 50                                 ; More than 50 remaining
    jle .last_chunk                             
    mov ax, 50                                  ; Load 50 sectors this pass
    jmp .do_load

.last_chunk:
    movzx ax, cl                                ; Load remaining sectors

.do_load:
    ; Update packet
    mov word [packet_kernel + 2],ax             ; Sector count
    mov dword [packet_kernel + 8], ebx          ; LBA start
    mov word [packet_kernel + 4], 0x0000        ; Offset always 0
    mov word [packet_kernel + 6], dx            ; Current segment

    ; Do the read
    push ax                                     ; Save sector count
    push ecx                                    ; Save remaining count
    push edx                                    ; Current segment

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, packet_kernel
    int 0x13
    jc .disk_error

    pop edx                                     ; Restore segment
    pop ecx                                     ; Restore remaining
    pop ax                                      ; Restore count loaded

    ; Advance pointers
    ; EAX = sectors loaded this pass (from pop ax above)
    movzx ebx, ax                               ; zero extend AX to 32-bit

    ; Advance LBA by sectors loaded
    add dword [packet_kernel+8], eax

    ; Advance segment: sectors * 512 / 16 = sectors * 32
    push eax                                    ; Save sector count
    shl eax, 5                                  ; EAX = sectors * 32 = segment advance
    add edx, eax                                ; Advance segment
    pop eax

    ; Reduce remaining count
    sub ecx, eax

    ; Reload LBA for next iteration
    mov ebx, dword [packet_kernel+8]

    jmp .load_loop

.done:
    ret

.disk_error:
    mov si, msg_disk_error
    call print
    jmp $

; --- Enter Protected Mode -----------------------------------------------
; Sets up a temporary GDT and switches to 32-bit protected mode
; This is a stepping stone to 64-bit long mode

enter_protected_mode:
    ; Load our temporary GDT
    lgdt [gdt32_descriptor]

    ; Set PE bit in CR0 (bit 0) to enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush pipeline and load CS with 32-bit code segment
    ; This is where we actually enter protected mode
    jmp 0x08:protected_mode_entry

; --- Data ----------------------------------------------------------------

; --- Disk Packets --------------------------------------------------------
align 4
packet_meta:
    db 0x10, 0x00                               ; Packet size, reserved
    dw 1                                        ; Read 1 sector
    dw 0x0000                                   ; Buffer offset
    dw 0x0500                                   ; Buffer segment = physical = 0x5000
    dq 63                                       ; LBA sector 63 (metadata)

align 4
packet_kernel:
    db 0x10, 0x00                               ; Packet size, reserved
    dw 0                                        ; Sector count (filled in dynamically)
    dw 0x0000                                   ; Buffer offset (filled in dynamically)
    dw 0x0000                                   ; Buffer segment (filled in dynamically)
    dq 0                                        ; LBA start (filled in dynamically)

; --- Temporary 32-bit GDT -----------------------------------------
; Just enough to get us into protected mode
; We'll replace this with a proper 64-bit GDT later

align 8
gdt32:
    ; Null descriptor
    dq 0

    ; 32-bit code segment
    ; Base=0, Limit=4GB, Ring 0, Execute/Read
    dw 0xFFFF                               ; Limit low
    dw 0x0000                               ; Base low
    db 0x00                                 ; Base middle
    db 10011010b                            ; Access: present, ring 0, code, executable, readable
    db 11001111b                            ; Flags: 4KB granularity, 32-bit limit high = 0xF
    db 0x00                                 ; Base high

    ; 32-bit data segment
    ; Base=0, Limit=4GB, Ring 0, Read/Write
    dw 0xFFFF                               ; Limit low
    dw 0x0000                               ; Base low
    db 0x00                                 ; Base middle
    db 10010010b                            ; Access: present, ring 0 data, writable
    db 11001111b                            ; Flags: 4KB granularity, 32-bit, limit high = 0xF
    db 0x00                                 ; Base high

gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32 - 1                ; GDT size - 1
    dd gdt32                                ; GDT physical address

; --- Variables ----------------------------------------------------
kernel_sectors dd 0                             ; Number of kernel sectors to load

; --- Messages ----------------------------------------------------
boot_drive db 0
msg_hello db 'Stage 2 running!', 13, 10, 0
msg_a20 db 'A20 enabled', 13, 10, 0
msg_memory db 'Memory map detected!', 13, 10, 0
msg_kernel db 'Kernel loaded!', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0

; --- 32-bit protected mode entry
[BITS 32]
protected_mode_entry:
    ; Load data segment registers with data segment selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 32-bit stack
    mov esp, 0x7000                         ; Temporary 32-bit stack

    ; Signal we made it
    ; Write 'PM' to VGA text buffer directly (BIOS int no longer works)
    mov dword [0xB8000], 0x074D0750 ; 'P', 'M' in white on black

    ; Hang for now
    jmp $