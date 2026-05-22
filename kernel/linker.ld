ENTRY(kernel_main)

SECTIONS {
    . = 0x1000;              /* Load address - where the bootloader puts us */

    .text : {               /* Code section */
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
        *(.bss)
        *(COMMON)
    }
}