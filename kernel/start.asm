[BITS 64]

global _start
extern kernel_main

extern __bss_start
extern __bss_end

section .text
_start:
    mov rax, 0xB8000     ; VGA virtual address via kernel mapping
    mov dword [rax], 0x0241

    ; We arrive here from stage 2 in 64-bit long mode
    ; Stage 2 passed arguments in registers
    ;   RDI = mem_map_addr (E820 map address)
    ;   RSI = mem_map_count (E820 entry count)
    ;   RDX = highest_ram (highest physical RAM address)
    ; Save them before we clobber any registers

    ; Set up stack
    mov rsp, 0xFFFF800000009000
    and rsp, 0xFFFFFFFFFFFFFFF0 ; 16-byte align

    ; Save arguments
    push rdi                    ; Save mem_map_addr
    push rsi                    ; Save mem_map_count
    push rdx                    ; Save highest_ram

    ; Zero the BSS section
    ; BSS contains uninitalised globals - must be zeroed
    ; before any C code runs
    mov rdi, __bss_start        ; Start of BSS (64-bit address)
    mov rcx, __bss_end          ; End of BSS
    sub rcx, rdi                ; Size in bytes
    shr rcx, 3                  ; Divide by 8 - stosq writes 8 bytes
    xor rax, rax                ; Zero value
    rep stosq                   ; Fill BSS with zeroes (8 bytes at a time)

    ; Restore arguments
    pop rdx                     ; highest_ram
    pop rsi                     ; mem_map_count
    pop rdi                     ; mem_map_addr

    ; Call kernel_main(mem_map_addr, mem_map_count, highest_ram)
    ; Arguments already in correct registers
    ;   RDI = mem_map_addr
    ;   RSI = mem_map_count
    ;   RDX = highest_ram
    call kernel_main

    ; If kernel_main returns - halt forever
    cli

.hang:
    jmp .hang