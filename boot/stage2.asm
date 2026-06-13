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
    
    ; Enable A20 Line
    call enable_a20
    mov si, msg_a20
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

; --- Data ----------------------------------------------------------------
boot_drive db 0
msg_hello db 'Stage 2 running!', 13, 10, 0
msg_a20 db 'A20 enabled', 13, 10, 0