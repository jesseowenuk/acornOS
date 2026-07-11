[BITS 64]

global syscall_entry
extern syscall_handler
extern current_process

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
;
; process_t offsets used below (must match include/kernel/processes/process.h)
;   stack_top = 160
;   user_esp = 192
;
; Each syscall uses the CALLING PROCESS'S OWN kernel stack (stack_top)
; and its OWN saved-user-RSP field (user_esp), instead of a single
; shared scratch variable. A syscall can block (SYS_WAIT does) and let
; another process run its own syscall before this one resumes - shared
; global scratch would be clobbered by the time this one continues.
; Per-process storage survives that correctly.

syscall_entry:
    ; Interrupts are off (CPU clears IF on SYSCALL via FMASK)
    ; RSP = user stack - dangerous! switch immediatley

    ; RCX (return RIP) and R11 (returns RFLAGS) are stashed on the still
    ; valid user stack so both registers are free to use as scratch for
    ; reaching current_process, without disturbing RAX (syscall number)
    ; or any of the argument registers
    push rcx                        ; stash return RIP
    push r11                        ; stach return RFLAGS

    mov rcx, [current_process]      ; RCX = process_t* of the caller
    lea r11, [rsp + 16]             ; r11 = TRUE user RSP (undo the 2 pushes above)
    mov [rcx + 192], r11            ; process->user_esp = true user RSP

    ; R11 (true user RSP) also rells us where the 2 stashed values live
    ; "push rcx" ran first, so it sits closer to the top (-8); "push r11"
    ; ran second, so it sits one slot further down (-16):
    ;   [r11 - 8]   = stashed return RIP (pushed 1st)
    ;   [r11 - 16]  = stashed RFLAGS (pushed 2nd)
    ; Both addresses are still in this process's own address space, so
    ; they're readable from here even after we move our own RSP away.
    mov rsp, [rcx + 160]            ; Switch to process->stack_top (kernel stack)

    ; Now safe to use the stack!
    ; Build a registers_t frame so syscall_handler can read args
    ; Must match registers_t layout in interrupts.h exactly

    push 0x23                               ; SS (user stack segment)
    push qword [rcx + 192]                  ; RSP (true user stack pointer)
    push qword [r11 - 16]                   ; rflags (stashed, read back from user stack)
    push 0x2B                               ; CS (user code segment)
    push qword [r11 - 8]                    ; RIP (stashed return address)

    push rax                                ; int_no = syscall number
    push 0                                  ; err_code = 0

    push rax                                ; RAX (syscall number)
    push rbx
    push qword [r11 - 8]                    ; RCX = user RIP
    push rdx                                ; RDX = arg3
    push rsi                                ; RSI = arg2
    push rdi                                ; RDI = arg1
    push rbp
    push r8                                 ; R8 = arg5
    push r9                                 ; R9 = arg6
    push r10                                ; R10 = arg4
    push qword [r11 - 16]                   ; R11 = saved rflags
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

    ; RSP now points at the frame's int_no field. The RSP field the
    ; entry code stashed (the true user RSP) sits 5 slots further up:
    ; int_no, err_code, rip, cs, rflags, [rsp field is next]. Read it
    ; straight out of the frame rather than re-touching current_process
    ; RAX already holds this syscall's real return value now (from the 
    ; pop above) and must not be clobbered before sysret.
    mov rsp, [rsp + 40]

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
