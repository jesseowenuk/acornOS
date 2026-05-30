[BITS 32]

global enter_usermode                   ; Make visible to C code

; enter_usermode(uint32_t entry, uint32_t stack)
; Arguments (C calling convention):
;   [esp+4] = entry - address of user program function
;   [esp+8] = stack - top of user mode stack
;
; To jump to ring 3 we use a carefully crafted iret
; iret pops: EIP, CS, EFLAGS, ESP, SS (in that order)
; If CS has RPL=3 the CPU switches to ring 3

enter_usermode:
    ; Read arguments before we touch the stack
    mov eax, [esp+4]                ; EAX = entry point address
    mov ecx, [esp+8]                ; ECX = user stack pointer

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
    ; iret in 32-bit protected mode with privilege change pops
    ;   EIP - where to start executing
    ;   CS - code segment (RPL = 3 triggers ring switch)
    ;   EFLAGS - CPU flags
    ;   ESP - user stack pointer (only on privilege change)
    ;   SS - user stack segment (only on privilege change)
    ; We push them in REVERSE order since the stack grows downwards

    push 0x23                       ; SS - user stack segment (RPL = 3)
    push ecx                        ; ESP - user stack pointer
    push 0x200                      ; EFLAGS - IF flag set (interrupts enabled)
                                    ; 0x200 = 0000 0010 0000 0000
                                    ; bit 9 = Interrupt Flag = 1
    push 0x1B                       ; CS - user code segment (RPL = 3)
                                    ; 0x1B = 0x18 | 3 (GDT entry 3 + RPL = 3)
    push eax                        ; EIP - entry point address

    iret                            ; Pop all 5 values and jump to ring 3!
                                    ; CPU sees CS RPL=3, switches privilege
                                    ; Loads SS and ESP for user stack
                                    ; Jumps to EIP in ring 3
                                    ; This instruction never returns