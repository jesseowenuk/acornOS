// Hello World
// First acornOS user space program!
// Standalone ELF64 binary, no libc, no crt0
//
// NOTE: Currently uses INT 0x80 for syscalls - this is the 32-bit
// convention and has limitations in 64-bit mode:
//  - Only lower 32-bits of registers are used for arguments
//  - Slower than SYSCALL instruction
//  - Arguments: rax=num, rbx=arg1, arc=arg2 (32-bit convention)
//
// TODO: Switch to SYSCALL instruction when proper ring 3 user space
// is implemented. 

static const char msg[] = "Hello from user space!\n";

void _start()
{
    // SYS_WRITE - print message
    __asm__ volatile(
        "int $0x80\n\t"
        :
        : "a"(1UL), "b"(msg), "c"(23UL)
        :
    );

    // SYS_EXIT
    __asm__ volatile(
        "int $0x80\n\t"
        :
        : "a"(0UL), "b"(0UL)
        :
    );

    for(;;);
}