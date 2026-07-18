[BITS 64]

global enter_usermode                   ; Make visible to C code
global iret_to_usermode
global enter_ring3

; enter_usermode(uint64_t entry, uint64_t stack)
; Arguments (C calling convention):
;   RDI = entry - address of user program
;   RSI = stack - top of user mode stack
;

enter_usermode:
    ; Load user data segment into all data segment registers
    ; 0x23 = GDT user data selector with RPL=3
    ; We must do this before iret so data accesses work in ring 3
    mov dx, 0x23                    ; User data segment selector
    mov ds, dx                      ; Data segment
    mov es, dx                      ; Extra segment
    mov fs, dx                      ; Extra segment F
    mov gs, dx                      ; Extra segment G
                                    ; SS will be set by iret automatically

    ; Build the iret stack frame
    ; iret in 64-bit pops
    ;   RIP - where to start executing
    ;   CS - code segment (RPL = 3 triggers ring switch)
    ;   RFLAGS - CPU flags
    ;   RSP - user stack pointer (only on privilege change)
    ;   SS - user stack segment (only on privilege change)
    ; We push them in REVERSE order since the stack grows downwards

    push 0x23                       ; SS - user stack segment (RPL = 3)
    push rsi                        ; RSP - user stack pointer
    push 0x200                      ; EFLAGS - IF flag set (interrupts enabled)
                                    ; 0x200 = 0000 0010 0000 0000
                                    ; bit 9 = Interrupt Flag = 1
    push 0x2B                       ; CS - user code segment (RPL = 3)
                                    ; 0x2B = 0x28 | 5 (GDT entry 5 + RPL = 3)
    push rdi                        ; RIP - entry point address

    iretq                           ; Pop all 5 values and jump to ring 3!
                                    ; CPU sees CS RPL=3, switches privilege
                                    ; Loads SS and ESP for user stack
                                    ; Jumps to EIP in ring 3
                                    ; This instruction never returns

iret_to_usermode:
    ; At this point the kernel stack has our iret frame
    ;   [rsp+0] = RAX value (fork return value)
    ;   [rsp+8] = RIP entry point
    ;   [rsp+16] = CS (0x1B user code)
    ;   [rsp+24] = RFLAGS
    ;   [esp+32] = RSP (user stack)
    ;   [rsp+40] = SS (0x23 user stack segment)

    ; Load user data segment into all segment registers
    mov ax, 0x23                    ; User data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    pop rax                         ; Restore RAX - fork return value
                                    ; For child this is 0, child PID for parent

    ; Pop EIP, CS, EFLAGS, ESP, SS
    ; CPU sees CS RPL=3 and switches to ring 3
    iretq

; enter_ring3(process_t* proc) - never returns
; RDI = pointer to a process_t whose cpu.rsp already points at a fully
; built 5-field iretq frame (RIP, CS, RFLAGS, RSP, SS), as built by
; elf_load(). Restores general registers from proc->cpu and iret's in.
; This is the same restore sequence switch_context's ring 3 path uses
; factored out here so process_exec can resuse it too.
enter_ring3:
    mov ax, 0x23                    ; User data segment
    mov ds, ax
    mov es, ax

    mov rsp, [rdi+104]              ; cpu.rsp -> top of iret frame
    mov rbx, [rdi+56]
    mov rcx, [rdi+64]
    mov rdx, [rdi+72]
    mov rsi, [rdi+80]
    mov rbp, [rdi+96]

    ; Restore r12-r15 before we destroy our own RDI (the process pointer)
    ; on the last line below - matches switch_context's ring0 restore
    ; path, so cpu_state_t is handled consistently everywhere it's 
    ; restored from, not just on the path that happened to hit the bug
    mov r12, [rdi+152]
    mov r13, [rdi+160]
    mov r14, [rdi+168]
    mov r15, [rdi+176]

    mov rax, [rdi+48]
    mov rdi, [rdi+88]               ; cpu.rdi resored LAST - clobbers our pointer

    iretq