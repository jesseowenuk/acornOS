[BITS 16]               ; Tell NASM we're writing 16-bit code
[ORG 0x7C00]            ; Tell NASM our code will be loaded at 0x7C00

start:
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

    mov si, msg         ; Point SI register at our message
    call print          ; Call our print routine

    jmp $               ; Infinite loop - hang here forever

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

msg db 'Hello from AcornOS!', 13, 10, 0
; 13 = carriage return, 10 = newline, 0 = null terminator

; Pad the file to 510 bytes, then add the 2 byte boot signature
times 510-($-$$) db 0
dw 0xAA55