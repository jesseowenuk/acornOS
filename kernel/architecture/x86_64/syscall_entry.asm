[BITS 64]

global syscall_entry
extern syscall_handler
extern syscall_set_kernel_stack

; --- SYSCALL entry point -----------------------------------------------
;
; Called when user space executes the SYSCALL instruction
;
; CPU automatically:
;   RCS <- user RIP (return address)
;   R11 <- user RFLAGS
;   RIP <- IA32_LSTAR (this function)
;   CS <- kernel code from IA32_STAR
;   SS <- kernel data from IA32_STAR
;
; RSP still points to USER stack - must switch to kernel stack
;
; Syscall convention (64-bit):
;   RAX = syscall number
;   RDI = arg1
;   RSI = arg2
;   RDX = arg3
;   R10 = arg4 (RCX clobbered by CPU)
;   R8 = arg5
;   R9 = arg6
;
; Return value in RAX

syscall_entry:
    ; Interrupts are off (CPU clears IF on SYSCALL via FMASK)
    ; RSP = user stack - dangerous! switch immediatley

    ; Save user RSP and switch to kernel stack
    ; We use a per-CPU scratch area at a fixed address
    ; TSS rsp0 holds our kernel stack top
    mov [user_rsp_scratch], rsp

    ; Load kernel stack from TSS rsp0
    ; We stored it at a fixed virtual address
    mov rsp, [kernel_rsp_scratch]           ; Switch to the kernel stack

    ; Now safe to use the stack!
    ; Build a registers_t frame so syscall_handler can read args
    ; Must match registers_t layout in interrupts.h exactly

    push 0x23                               ; SS (user stack segment)
    push qword [user_rsp_scratch]           ; RSP (user stack pointer)
    push r11                                ; rflags  (saved by CPU)
    push 0x2B                               ; CS (user code segment)
    push rcx                                ; RIP (return address, saved by CPU)

    push rax                                ; int_no = syscall number
    push 0                                  ; err_code = 0

    push rax                                ; RAX (syscall number)
    push rbx
    push rcx                                ; RCX = user RIP
    push rdx                                ; RDX = arg3
    push rsi                                ; RSI = arg2
    push rdi                                ; RDI = arg1
    push rbp
    push r8                                 ; R8 = arg5
    push r9                                 ; R9 = arg6
    push r10                                ; R10 = arg4
    push r11                                ; R11 = saved rflags
    push r12
    push r13
    push r14
    push r15

    ; Save data segment
    mov ax, ds
    push rax

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; Call C syscall handler
    ; RDI = pointer to registers_t
    mov rdi, rsp
    call syscall_handler

    ; Restore data segment
    pop rax
    mov ds, ax
    mov es, ax

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

    ; Skip into_no, err_code, rip, cs, rflags, rsp, ss
    add rsp, 56

    ; Restore user RSP
    mov rsp, [user_rsp_scratch]

    ; Return to user space
    o64 sysret

; Set the kernel stack pointer for SYSCALL entries
; RDI = kernel stack top address
syscall_set_kernel_stack:
    mov [kernel_rsp_scratch], rdi
    ret

; --- Scrach storage -------------------------------------------
; Used to save user RSP before switching stacks
; TODO: replace with proper per-CPU data when we have SMP
section .data
user_rsp_scratch: dq 0
kernel_rsp_scratch: dq 0
