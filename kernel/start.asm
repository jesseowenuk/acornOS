[BITS 32]

global _start           ; Entry point symbol
extern kernel_main      ; Defined in kernel.c

extern __bss_start      ; Defined in linker script
extern __bss_end        ; Defined in linker script

section .text
_start:
    ; When bootloader called us it pushed
    ;   [esp+0] = return address (from call KERNEL_OFFSET)
    ;   [esp+4] = mem_map_addr (0x500)
    ;   [esp+8] = mem_map_count
    ; We need to pass these to kernel_main
    mov eax, [esp+8]    ; Get mem_map_count from stack
    mov ebx, [esp+4]    ; Get mem_map_addr from stack

    ; Zero the BSS
    mov edi, __bss_start    ; Start of BSS
    mov ecx, __bss_end      ; End of BSS
    sub ecx, edi            ; Size of BSS in bytes
    shr ecx, 2              ; Divide by 4 - stosd writes 4 bytes at a time
    xor edx, edx            ; Zero value
    push eax                ; Save mem_map_count - ECX will be clobbered
    push ebx                ; Save mem_map_addr
    xor eax, eax            ; Zero for stosd
    rep stosd               ; Fill BSS with zeroes
    pop ebx                 ; Restore mem_map_addr
    pop eax                 ; Restore mem_map_count

    ; Set up a completely fresh stack
    ; Use a known good address well above our kernel
    mov esp, 0x9FFFC    ; Fresh stack - stack at 640KB mark

    and esp, 0xFFFFFFF0 ; Align stack to 16-byte boundary
                        ; GCC expects this for all function calls
                        ; misaligned stack causes subtle corruption
    
    ; Push arguments for kernel_main(mem_map_addr, mem_map_count)
    ; C calling convention: rightmost argument pushed first

    push eax            ; Push mem_map_count as second argument
    push ebx            ; Push mem_map_addr as first argument
    call kernel_main    ; Call our C kernel entry point
    jmp $               ; If kernel_main returns, hang
