#include <unistd.h>

extern int main(int argc, char** argv);

// ELF entry point - the kernel jumps here directly after setting up the 
// inital ring 3 stack, with RDI = argc and RSI = argv already set (see
// elf_load()/build_argv_stack() in kernel/core/elf.c) exactly as if
// _start had been called normally with twon arguments - no seperate
// stack-based argv convention needed.
void _start(int argc, char** argv)
{
    int status = main(argc, argv);
    exit(status);
}