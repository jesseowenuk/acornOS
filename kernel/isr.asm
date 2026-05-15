[BITS 32]

extern isr_handler

; Macro for ISRs that don't push an error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push byte 0             ; Dummy error code
    push byte %1            ; Interrupt number
    jmp isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3

isr_common_stub:
    pusha                   ; Push all general purpose registers
    push ds
    mov ax, 0x10            ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call isr_handler        ; Call our C handler
    pop ds
    popa
    add esp, 8              ; Clean up int_no and err_code
    sti
    iret