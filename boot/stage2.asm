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
BOOT_STACK_TOP equ 0x1FFFF

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

; --- Data ----------------------------------------------------------------
boot_drive db 0
msg_hello db 'Stage 2 running!', 13, 10, 0