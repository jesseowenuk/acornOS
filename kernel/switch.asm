[BITS 32]

global switch_context           ; Make this function visible to C code

; switch_context(process_t* old, process_t* new)
; Saves the current CPU state into old's PCB
; Restores CPU state from new's PCB
; Arguments are on the stack per C calling convention:
;   [esp+4] = pointer to old process_t
;   [esp+8] = pointer to new process_t

switch_context:
    ; --- Save old process state ---------------------------------------

    mov eax, [esp+4]                ; EAX = pointer to old process_t
                                    ; We'll use this to write into old's cpu_state_t
                                    ; cpu_state_t is at offset 0 in process_t
                                    ; (it's the first field after pid, name, state)

    ; We need to find the cpu_state_t offset in process_t
    ; Looking at our struct:
    ;   pid             = 4 bytes   (offset 0)
    ;   name            = 32 bytes  (offset 4)
    ;   state           = 4 bytes   (offset 36)
    ;   cpu             = starts at offset 40
    ;       cpu.eax     = offset 40
    ;       cpu.ebx     = offset 44
    ;       cpu.ecx     = offset 48
    ;       cpu.edx     = offset 52
    ;       cpu.esi     = offset 56
    ;       cpu.edi     = offset 60
    ;       cpu.ebp     = offset 64
    ;       cpu.esp     = offset 68
    ;       cpu.eip     = offset 72
    ;       cpu.eflags  = offset 76
    ;       cpu.cs      = offset 80
    ;       cpu.ds      = offset 84
    ;       cpu.ss      = offset 88

    ; Save general purpose registers into old->cpu
    ; Note: EAX currently holds the old process pointer so we save it last
    mov [eax+44], ebx               ; Save EBX into old->cpu.ebx
    mov [eax+48], ecx               ; Save ECX into old->cpu.ecx    
    mov [eax+52], edx               ; Save EDX into old->cpu.edx
    mov [eax+56], esi               ; Save ESI into old->cpu.esi
    mov [eax+60], edi               ; Save EDI into old->cpu.edi
    mov [eax+64], ebp               ; Save EBP into old->cpu.ebp

    ; Save EAX - we need to recover the real EAX value
    ; Right now EAX holds the old process pointer, not the real EAX
    ; The real EAX is lost so we save 0 for now
    ; When we restore we'll get whatever was there before
    mov dword [eax+40], 0           ; Save 0 for EAX - will fix properly later

    ; Save ESP - but not the current ESP which includes our call frame
    ; We want the ESP that the process had before we were called
    ; That's the current ESP plus 4 (return address) plus 8 (two arguments)
    lea ecx, [esp+12]               ; ECX = ESP before this function was called
                                    ; +4 for return address
                                    ; +4 for old pointer argument
                                    ; +4 for new pointer argument
    mov [eax+68], ecx               ; Save into old->cpu.esp

    ; Save EIP - the return address is what we want
    ; When this process resumes it should return from switch_context
    mov ecx, [esp]                  ; ECX = return address (top of stack)
    mov [eax+72], ecx               ; Save into old->cpu.eip

    ; Save EFLAGS
    pushfd                          ; Push EFLAGS onto stack
    pop ecx                         ; Pop into ECX
    mov [eax+76], ecx               ; Save into old->cpu.eflags

    ; Save segment registers
    mov [eax+80], cs                ; Save CS (code segment)
    mov word [eax+84], ds           ; Save DS (data segment)
    mov word [eax+88], ss           ; Save SS (stack segment)

    ; --- Restore new process state ---------------------------------

    mov eax, [esp+8]                ; EAX = pointer to new process_t

    ; Restore segment registers first
    mov cx, [eax+84]                ; Load new DS value
    mov ds, cx                      ; Restore DS
    mov cx, [eax+88]                ; Load new SS value
    mov ss, cx                      ; Restore SS
                                    ; CS will be restored by the far return

    ; Restore ESP - switch to the new process's stack
    mov esp, [eax+68]               ; Load new->cpu.esp
                                    ; From this point we're on the new stack!

    ; Restore general purpose registers
    mov ebx, [eax+44]               ; Restore EBX
    mov ecx, [eax+48]               ; Restore ECX
    mov edx, [eax+52]               ; Restore EDX
    mov esi, [eax+56]               ; Restore ESI
    mov edi, [eax+60]               ; Restore EDI
    mov ebp, [eax+64]               ; Restore EBP

    ; Restore EFLAGS
    push dword [eax+76]             ; Push new->cpu.eflags
    popfd                           ; Pop into EFLAGS register

    ; Restore EIP by pushing it and doing a ret
    ; This jumps to whereever the new process was last executing
    push dword [eax+72]             ; Push new->cpu.eip

    ; Restore EAX last - after this we lose our pointer to the PCB
    mov eax, [eax+40]               ; Restore EAX from new->cpu.eax

    ret                             ; Pop EIP from stack and jump there
                                    ; This resumes the new process