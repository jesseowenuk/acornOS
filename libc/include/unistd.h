#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>

typedef long ssize_t;
typedef long pid_t;

// --- File open flags - must match include/file_system/vfs.h ----------------------
#define O_RDONLY    0x0
#define O_WRONLY    0x1
#define O_RDWR      0x2
#define O_CREAT     0x4
#define O_APPEND    0x8
#define O_TRUNC     0x10

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// --- Standard file descriptors ---------------------------------------------------
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

// Write 'count' bytes from 'buf' to file descriptor 'fd'. fd 1/2
// (stdin/stderr) go to the screen; any fd returned by open() works too.
ssize_t write(int fd, const void* buf, size_t count);

// Read up to 'count' bytes from file desceriptor 'fd' into 'buf'. fd 0
// (stdin) blocks until a keypress; any fd returned by open() works too.
// Returns bytes read, or -1 on error
ssize_t read(int fd, void* buf, size_t count);

// Terminate the current process with the given status - never returns
void exit(int status) __attribute__((noreturn));

// Get the current process's PID
pid_t getpid(void);

// voluntarily give up the rest of this process's time slice
void yield(void);

// Fork the current process - returns child PID to parent, 0 to child, -1 on error
pid_t fork(void);

// Block until any child process exits - returns exit code
int wait(void);

// Replace the current process image with the ELF at 'path'. argv is a 
// NULL-terminated array of argument strings - argv[0] is conventionally
// the program's own name (your job to set, not exec's). Never returns
// on success.
int exec(const char* path, char* const argv[]);

// Open a file - returns a file descriptor, or -1 on error
int open(const char* path, int flags);

// Close a file descriptor
int close(int fd);

// Seek within a file
int seek(int fd, int offset, int whence);

#endif