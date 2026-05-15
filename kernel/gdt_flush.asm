[BITS 32]

global gdt_flush

gdt_flush:
    mov eax, [esp+4]            ; Get descriptor pointer passed as argument
    lgdt [eax]                  ; Load it

    ; Reload segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with code descriptor (0x08)
    jmp 0x08:.flush

.flush:
    ret