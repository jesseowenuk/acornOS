[BITS 16]               ; Tell NASM we're writing 16-bit code
[ORG 0x7C00]            ; Tell NASM our code will be loaded at 0x7C00

KERNEL_OFFSET equ 0x1000    ; Where we'll load the kernel in memory

start:
    mov [boot_drive], dl    ; BIOS puts boot drive in DL - save it immediatley

    ; Segment register setup - zero them all out
    xor ax, ax          ; AX = 0 (xor is the idiomatic way to zero a register)
    mov ds, ax          ; Data segment = 0
    mov es, ax          ; Extra segment = 0
    mov ss, ax          ; Stack segment = 0
    mov sp, 0x7C00      ; Stack pointer just below our bootloader in memory

    ; Clear the screen
    mov ah, 0x00        ; BIOS function: set video mode
    mov al, 0x03        ; Mode 3 = 80x25 colour text
    int 0x10

    mov si, msg_load    ; Point SI register at our message
    call print          ; Call our print routine

    call load_kernel    ; Load kernel from disk

    ; Disable interrupts before switching modes
    cli

    ; Load our GDT
    lgdt [gdt_descriptor]

    ; Set the Protection Enable bit in CR0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush the CPU pipeline and land in 32-bit mode
    jmp 0x08:protected_mode

; --- Print routine -----------------------------------------------------
print:
    mov ah, 0x0E        ; BIOS teletype function
.loop:
    lodsb               ; Load byte at [SI] into AL, then increment SI
    cmp al, 0           ; Is it the null terminator?
    je .done            ; If yes, we're finished
    int 0x10            ; Otherwise, print the character
    jmp .loop           ; And loop

.done: 
    ret

; --- Disk load -----------------------------------------------------
load_kernel:
    mov si, msg_disk
    call print

    mov ah, 0x02        ; BIOS read sectors function
    mov al, 5          ; Number of sectors to read
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2 (sector 1 is our bootloader)
    mov dh, 0           ; Head 0
    mov dl, [boot_drive]    ; Use the saved drive number
    mov bx, KERNEL_OFFSET   ; Load into memory at 0x1000
    int 0x13
    jc disk_error       ; Jump if carry flag set (error)
    ret

disk_error:
    mov si, msg_error
    call print
    jmp $

boot_drive db 0 ; 1 byte to store the drive 

msg_load db 'acornOS booting...', 13, 10, 0
msg_disk db 'Loading kernel from disk...', 13, 10, 0
msg_error db 'Disk read error!', 13, 10, 0
; 13 = carriage return, 10 = newline, 0 = null terminator

; --- GDT -----------------------------------------------------
gdt_start:
    ; Null descriptor
    dd 0x00000000
    dd 0x00000000

    ; Code segment descriptor
    dw 0xFFFF                   ; Limit low
    dw 0x0000                   ; Base low
    db 0x00                     ; Base mid
    db 10011010b                ; Access: present, ring 0, code, readable
    db 11001111b                ; Flags + limit high: 32-bit, 4KB granularity
    db 0x00                     ; Base high

    ; Data segment descriptor
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                ; Access: present, ring 0, data, writable
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size minus 1
    dd gdt_start                ; GDT address

; --- Protected mode entry -----------------------------------------------------

[BITS 32]
protected_mode:
    ; update all segment registers to use our data descriptor (offset 0x10)
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000            ; Set up a fresh 32-bit stack

    call KERNEL_OFFSET          ; jump to the kernel

    jmp $               ; Infinite loop - hang here forever

; Pad the file to 510 bytes, then add the 2 byte boot signature
times 510-($-$$) db 0
dw 0xAA55