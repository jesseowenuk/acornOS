[BITS 64]

global gdt_flush

gdt_flush:            
    lgdt [rdi]                  ; Load the GDT

    ; Reload data segment registers
    ; In 64-bit mode DS, ES, FS, GS and SS are mostly ignored
    ; but we still set them for correctness
    mov ax, 0x10                    ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with code descriptor (0x08)
    pop rdi                         ; Save return address
    push qword 0x08                 ; Push code segment selector
    push rdi                        ; Push return address
    retfq                           ; Far return - reloads CS and RIP

.flush:
    ret