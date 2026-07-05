[BITS 64]

extern isr_handler
extern irq_handler

; Macro for ISRs that don't push an error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push qword 0             ; Dummy error code
    push qword %1            ; Interrupt number
    jmp isr_common_stub
%endmacro


; Macro for CPU exceptions that DO push an error code automatically
; We do NOT push a dummy one - the CPU already pushed the real one
%macro ISR_ERR 1
global isr%1
isr%1:
    cli                     ; Disable interrupts
                            ; Note no dummy error code push here!
                            ; CPU already pushed the real error code
    push qword %1           ; Push interrupt number
    jmp isr_common_stub     ; Jump to shared handler
%endmacro

; Macro for hardware IRQs
%macro IRQ 2
global irq%1
irq%1:
    cli
    push qword 0            ; Dummy error code - CPU pushed real one
    push qword %2           ; Interrupt number
    jmp irq_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 128               ; INT 0x80 (system call)

; Page fault (interrupt 14) pushes a real error code
ISR_ERR 8
ISR_ERR 13
ISR_ERR 14

IRQ 0, 32           ; Timer
IRQ 1, 33           ; Keyboard

; Stack layout when isr_common_stub is entered:
;   [rsp+0]     int_no      (we pushed last)
;   [rsp+8]     err_code    (we pushed first, or CPU pushed)
;   [rsp+16]    RIP         (CPU)
;   [rsp+24]    CS          (CPU)
;   [rsp+32]    RFLAGS      (CPU)
;   [rsp+40]    RSP         (CPU, user stack)
;   [rsp+48]    SS          (CPU) 

isr_common_stub:
    ; Save all the general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save data segment
    mov ax, ds
    push rax                    ; Save DS

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass pointer to registers struct as argument
    mov rdi, rsp                ; RDI = pointer to saved regisers
    call isr_handler

    ; Restore data segment
    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore general purpoose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up int_no and err_code
    add rsp, 16

    ; Re-enable interrupts
    sti

    ; 64-bit iret
    iretq

irq_common_stub:
    ; Identical structure to isr_common_stub

    ; Save all the general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save data segment
    mov ax, ds
    push rax                    ; Save DS

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass pointer to registers struct as argument
    mov rdi, rsp                ; RDI = pointer to saved regisers
    call irq_handler

    ; Restore data segment
    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore general purpoose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up int_no and err_code
    add rsp, 16

    ; Re-enable interrupts
    sti

    ; 64-bit iret
    iretq