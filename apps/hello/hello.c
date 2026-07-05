// Hello World
// First acornOS user space program!
// Standalone ELF64 binary, no libc, no crt0
//
// Uses the SYSCALL instruction (64-bit convention):
// rax = syscall number, rdi = arg1, rsi = arg2, rdx = arg3
// return value comes back in RAX
// SYSCALL clobbers RCX (return RIP) and R11 (saved RFLAGS)

static const char msg[] = "Hello from user space!\n";

void _start()
{
    // SYS_WRITE - print message
    __asm__ volatile(
        "syscall\n\t"
        :
        : "a"(1UL), "D"(msg), "S"(23UL)
        : "rcx", "r11", "memory"
    );

    // SYS_EXIT
    __asm__ volatile(
        "syscall\n\t"
        :
        : "a"(0UL), "D"(0UL)
        : "rcx", "r11", "memory"
    );

    for(;;);
}