[BITS 16]
[ORG 0x7C00]

; --- Stage 1 Bootloader ------------------------------------
; Fits exactly 512 bytes (one disk sector)
; Job: save boot drive, load stage 2, jump to it

; Load stage 2 right after stage 1
STAGE2_LOAD_ADDRESS equ 0x7E00

; Load 63 sectors = 31.5KB stage 2
STAGE2_SECTORS equ 63

start:
    ; Disable interrupts during setup
    cli

    ; Save boot drive - BIOS puts it in DL on entry
    ; Must save immediatley before anything corrupts it
    mov [boot_drive], dl

    ; Set up segments and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; stack grows downwards from bootloader

    ; Re-enable interrupts
    sti

    ; Print loading message
    mov si, msg_stage1
    call print

    ; Load stage 2 from disk using LBA
    ; Stage 2 lives at sectors 1-63 on disk
    xor ax, ax
    mov ds, ax                  ; Ensures DS=0 for LBA packet

    mov ah, 0x42                ; INT 13h extended read
    mov dl, [boot_drive]
    mov si, lba_packet
    int 0x13
    jc disk_error               ; Carry set on error

    ; Jump to stage 2
    mov si, msg_ok
    call print

    ; Jump to stage 2!
    ; DL still contains boot drive
    jmp STAGE2_LOAD_ADDRESS

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

; --- Disk error handler -------------------------------------------------
disk_error:
    mov si, msg_error
    call print
    jmp $                       ; Hang forever

; --- Data ---------------------------------------------------------------
boot_drive db 0

msg_stage1 db 'acornOS Stage 1...', 13, 10, 0
msg_ok db 'Stage 2 loaded!', 13, 10, 0
msg_error db 'Disk error!', 13, 10, 0

; --- LBA Address Packet -------------------------------------------
align 4
lba_packet:
    db 0x10                     ; Packet size (16 bytes)
    db 0x00                     ; Reserved
    dw STAGE2_SECTORS           ; Sectors to read
    dw STAGE2_LOAD_ADDRESS      ; Buffer offset
    dw 0x0000                   ; Buffer segment
    dq 1                        ; LBA start sector (sector 1)

; --- Boot signature -------------------------------------------------
times 510-($-$$) db 0
dw 0xAA55