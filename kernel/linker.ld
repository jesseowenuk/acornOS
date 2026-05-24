ENTRY(kernel_main)

SECTIONS {
    . = 0x1000;              /* Load address - where the bootloader puts us */

    .text : {               /* Code section */
        start.o(.text)
        kernel.o(.text)
        *(.text)
    }

    .rodata : {             /* Read only data - string literals live here */
        *(.rodata)
        *(.rodata.*)        /* Compiler sometimes splits rodata into subsections */
    }

    .data : {               /* Initialised global variables
        *(.data)
    }

    .bss : {                /* Uninitialised global variables - zeroed at boot */
        __bss_start = .;    /* Mark start of BSS */
        *(.bss)
        *(COMMON)
        __bss_end = .;      /* Mark end of BSS */
    }
}