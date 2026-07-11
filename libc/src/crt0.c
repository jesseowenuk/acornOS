#include <unistd.h>

extern int main(void);

// ELF entry point - the kernel jumps here directly after setting up the 
// inital ring 3 stack. No argc/argv/envp support yet.
void _start(void)
{
    int status = main();
    exit(status);
}