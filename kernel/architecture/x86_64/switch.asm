[BITS 64]

global switch_context           ; Make this function visible to C code
extern enter_ring3

; switch_context(process_t* old, process_t* new)
; Arguments are on the stack per C calling convention:
;   RDI = pointer to old process_t
;   RSI = pointer to new process_t
;
; process_t offsets:
;   cpu.rax = 48
;   cpu.rbx = 56
;   cpu.rcx = 64
;   cpu.rdx = 72
;   cpu.rsi = 80
;   cpu.rdi = 88
;   cpu.rbp = 96
;   cpu.rsp = 104
;   cpu.rip = 112
;   cpu.rflags = 120
;   cpu.cs = 128
;   cpu.ds = 136
;   cpu.ss = 144
;   cpu.r12 = 152
;   cpu.r13 = 160
;   cpu.r14 = 168
;   cpu.r15 = 176
;   stack = 184
;   stack_top = 192
;   page_dir = 200

switch_context:
    ; --- Save old process state ---------------------------------------

    ; Save general purpose registers into old->cpu
    ; Note: RDI currently holds the old process pointer so we save it last
    mov [rdi+56], rbx               ; Save RBX into old->cpu.rbx
    mov [rdi+64], rcx               ; Save RCX into old->cpu.rcx    
    mov [rdi+72], rdx               ; Save RDX into old->cpu.rdx
    mov [rdi+80], rsi               ; Save RSI into old->cpu.rsi
                                    ; (RSI = new process ptr, saved for reference)

    mov [rdi+88], rdi               ; Save RDI into old->cpu.rdi (old process pointer)
    mov [rdi+96], rbp               ; Save RBP into olf->cpu.rbp
    mov qword [rdi+48], 0           ; Save RAX = 0 (RAX clobbered by call)

    ; Save RAX - we need to recover the real RAX value
    ; Current RSP points to return address
    ; Add 8 to get RSP as it was before the call
    lea rcx, [rsp+8]                ; RCX = RSP before call
    mov [rdi+104], rcx              ; Save old into old->cpu.rsp

    ; Save RIP - return address is what we want
    mov rcx, [rsp]                  ; RCX = return address
    mov [rdi+112], rcx              ; Save into old->cpu.rip

    ; Save RFLAGS
    pushfq                          ; Push RFLAGS onto stack
    pop rcx                         ; Pop into RCX
    mov [rdi+120], rcx              ; Save into old->cpu.rflags

    ; Save segment registers
    mov [rdi+128], cs               ; Save CS
    mov word [rdi+136], ds          ; Save DS
    mov word [rdi+144], ss          ; Save SS

    ; Save callee-saved registers r12 - r15 
    ; Any C caller of scheuler_yield() may have a
    ; live local variable in one of these, and is entitled to assume
    ; it survives the call. Missing before this fix - whichever process
    ; ran next between switch-out and switch-back-in would silently
    ; clobber them, corrupting the resuming process's own C state
    mov [rdi+152], r12
    mov [rdi+160], r13
    mov [rdi+168], r14
    mov [rdi+176], r15

    ; --- Switch page directory -------------------------------------
    ; Get new process's page_dir (offset 168 in process_t)
    mov rax, [rsi+200]              ; RAX = new->page_dir
    test rax, rax                   ; Is it NULL?
    jz .no_switch                   ; If NULL keep current CR3

    ; Convert virtual to physical via direct map
    ; Physical = virtual + PHYSICAL_MAP_BASE (0xFFFF800000000000)
    mov rcx, 0xFFFF800000000000
    sub rax, rcx                    ; RAM = physical address
    mov cr3, rax

    mov cr3, rax                    ; Load new page directory
                                    ; Flushes TLB, switches address space

    jmp .done_switch

.no_switch:
    ; Load kernel directory address
    ; If page_dir is NULL we keep the current CR3
    nop                             ; Do nothing - keep current page directory

.done_switch:
    ; --- Restore new process state ---------------------------------
    ; RSI = new process pointer

    ; Check if new process is ring 3 (CS RPL bits non-zero)
    mov rax, [rsi+128]              ; Load new->cpu.cs
    and rax, 3                      ; Check RPL bits
    jnz .ring3_restore              ; Non-zero = ring 3

    ; --- Ring 0 restore ------------------------------------------
    mov rsp, [rsi+104]              ; Switch to new process stack

    mov rbx, [rsi+56]
    mov rcx, [rsi+64]
    mov rdx, [rsi+72]
    mov rbp, [rsi+96]

    ; Restore r12-r15 before we destroy our RSI (the new-process
    ; pointer) on the next line - see the matching save comment above
    mov r12, [rsi+152]
    mov r13, [rsi+160]
    mov r14, [rsi+168]
    mov r15, [rsi+176]

    push qword [rsi+112]            ; Push RIP
    mov rax, [rsi+48]
    mov rsi, [rsi+80]
    ret

.ring3_restore:
    ; --- Ring 3 restore - shared with process_exec via enter_ring3 -----------
    mov rdi, rsi                        ; enter_ring3 takes the process pointer in RDI
    jmp enter_ring3