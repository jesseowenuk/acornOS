#ifndef ACORN_SYSCALL_H
#define ACORN_SYSCALL_H

// --- Syscall numbers - must match kernel/processes/syscall.h -------------------
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_READ        2
#define SYS_GETPID      3
#define SYS_YIELD       4
#define SYS_FORK        5
#define SYS_WAIT        6
#define SYS_EXEC        7
#define SYS_OPEN        8
#define SYS_CLOSE       9
#define SYS_SEEK        10
#define SYS_MKDIR       11
#define SYS_READDIR     12
#define SYS_DELETE      13
#define SYS_HEAP_GROW   14      

// --- Raw syscall wrappers ------------------------------------------
// One per argument count actually in use today (max 3 args)
// rax = syscall number, rdi/rsi/rdx = args, return value comes back in RAX
// SYSCALL clobbers RCX (return RIP) and R11 (saved RFLAGS)

static inline long __syscall0(long num)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long __syscall1(long num, long a1)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long __syscall2(long num, long a1, long a2)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long __syscall3(long num, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif