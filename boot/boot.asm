[BITS 16]               ; Tell NASM we're writing 16-bit code
[ORG 0x7C00]            ; Tell NASM our code will be loaded at 0x7C00

start:
    mov ah, 0x0E        ; BIOS teletype function
    mov al, 'A'         ; Character to print
    int 0x10            ; Call BIOS video interrupt
    
    jmp $               ; Infinite loop - hang here forever

; Pad the file to 510 bytes, then add the 2 byte boot signature
times 510-($-$$) db 0
dw 0xAA55