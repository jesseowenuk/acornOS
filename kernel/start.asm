[BITS 32]

global _start           ; Entry point symbol
extern kernel_main      ; Defined in kernel.c

section .text
_start:
    ; When bootloader called us it pushed
    ;   [esp+0] = return address (from call KERNEL_OFFSET)
    ;   [esp+4] = mem_map_addr (0x500)
    ;   [esp+8] = mem_map_count
    ; We need to pass these to kernel_main
    mov eax, [esp+8]    ; Get mem_map_count from stack
    mov ebx, [esp+4]    ; Get mem_map_addr from stack

    ; Set up a completely fresh stack
    ; Use a known good address well above our kernel
    mov esp, 0x8FFFC    ; Fresh stack - just below 0x90000
                        ; 0xFFFC ensures 16-byte alignment
                        ; (0x8FFFC & 0xFFFFFFF0 = 0x8FFF0, close enough)

    and esp, 0xFFFFFFF0 ; Align stack to 16-byte boundary
                        ; GCC expects this for all function calls
                        ; misaligned stack causes subtle corruption
    
    ; Push arguments for kernel_main(mem_map_addr, mem_map_count)
    ; C calling convention: rightmost argument pushed first

    push eax            ; Push mem_map_count as second argument
    push ebx            ; Push mem_map_addr as first argument
    call kernel_main    ; Call our C kernel entry point
    jmp $               ; If kernel_main returns, hang
