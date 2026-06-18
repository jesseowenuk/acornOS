[BITS 64]

global switch_context           ; Make this function visible to C code

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
;   stack = 152
;   stack_top = 160
;   page_dir = 168

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

    ; --- Switch page directory -------------------------------------
    ; Get new process's page_dir (offset 168 in process_t)
    mov rax, [rsi+168]              ; RAX = new->page_dir
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

    ; Restore segment registers first
    mov cx, word [rsi+136]          ; Load new DS value
    mov ds, cx                      ; Restore DS
    mov cx, word [rsi+144]          ; Load new SS value
    mov ss, cx                      ; Restore SS
                                    ; CS will be restored by the far return

    ; Restore RSP - switch to the new process's stack
    mov rsp, [rsi+104]              ; Load new->cpu.rsp
                                    ; From this point we're on the new stack!

    ; Restore general purpose registers
    mov rbx, [rsi+56]               ; Restore RBX
    mov rcx, [rsi+64]               ; Restore RCX
    mov rdx, [rsi+72]               ; Restore RDX
    mov rbp, [rsi+96]               ; Restore RBP

    ; Note RSI restored last since we need it for indexing

    ; Restore RFLAGS
    push qword [rsi+120]            ; Push new->cpu.rflags
    popfq                           ; Pop into RFLAGS register

    ; Restore RIP by pushing return address and doing ret
    push qword [rsi+112]             ; Push new->cpu.rip

    ; Restore RAX and RSI last
    mov rax, [rsi+48]               ; Restore RAX
    mov rsi, [rsi+80]               ; Restore RSI - after this we lose PCB pointer

    ret                             ; Pop RIP from stack and jump there
                                    ; This resumes the new process