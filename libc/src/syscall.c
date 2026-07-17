#include <acorn/syscall.h>
#include <unistd.h>

ssize_t write(int fd, const void* buf, size_t count)
{
    return (ssize_t)__syscall3(SYS_WRITE, fd, (long)buf, (long)count);
}

ssize_t read(int fd, void* buf, size_t count)
{
    return (int)__syscall3(SYS_READ, fd, (long)buf, (long)count);
}

void exit(int status)
{
    __syscall1(SYS_EXIT, status);

    // SYS_EXIT never returns, but the compiler doesn't know that
    for(;;) { }
}

pid_t getpid(void)
{
    return (pid_t)__syscall0(SYS_GETPID);
}

void yield(void)
{
    __syscall0(SYS_YIELD);
}

pid_t fork(void)
{
    return (pid_t)__syscall0(SYS_FORK);
}

int wait(void)
{
    return (int)__syscall0(SYS_WAIT);
}

int exec(const char* path, char* const argv[])
{
    return (int)__syscall2(SYS_EXEC, (long)path, (long)argv);
}

int open(const char* path, int flags)
{
    return (int)__syscall2(SYS_OPEN, (long)path, (long)flags);
}

int close(int fd)
{
    return (int)__syscall1(SYS_CLOSE, (long)fd);
}

int seek(int fd, int offset, int whence)
{
    return (int)__syscall3(SYS_SEEK, (long)fd, (long)offset, (long)whence);
}