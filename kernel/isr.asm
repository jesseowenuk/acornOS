[BITS 32]

extern isr_handler

; Macro for ISRs that don't push an error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0             ; Dummy error code
    push dword %1            ; Interrupt number
    jmp isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 128               ; INT 0x80 (system call)

isr_common_stub:
    pusha                   ; Push all general purpose registers
    mov ax, ds              ; Save current data segment value
    push eax                ; Push it as a 32-bit value
    mov ax, 0x10            ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp                ; Push stack pointer as register_t* argument
    call isr_handler        ; Call our C handler
    pop eax                 ; Clean up push esp
    pop eax                 ; Restore saved data segment value
    mov ds, ax              ; Restore all segment registers from it
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa                    ; Restore general purpose registers
    add esp, 8              ; Clean up int_no and err_code
    sti
    iret

; Macro for hardware IRQs
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

; Macro for CPU exceptions that DO push an error code automatically
; We do NOT push a dummy one - the CPU already pushed the real one
%macro ISR_ERR 1
global isr%1
isr%1:
    cli                     ; Disable interrupts
                            ; Note no dummy error code push here!
                            ; CPU already pushed the real error code
    push dword %1           ; Push interrupt number
    jmp isr_common_stub     ; Jump to shared handler
%endmacro

IRQ 0, 32           ; Timer
IRQ 1, 33           ; Keyboard

; Page fault (interrupt 14) pushes a real error code
ISR_ERR 14

extern irq_handler

irq_common_stub:
    pusha                       ; Save all general purpose registers
    mov ax, ds                  ; Save current data segment value
    push eax                    ; Push it as a 32-bit value
    mov ax, 0x10                ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp                    ; Push stack pointer as registers_t* argument
    call irq_handler            ; Call C handler
    pop eax                     ; Clean up push esp
    pop eax                     ; Restore saved data segment value
    mov ds, ax                  ; Restore all segement registers from it
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa                        ; Restore general purpose registers
    add esp, 8                  ; Clean up int_no and err_code
    sti
    iret
