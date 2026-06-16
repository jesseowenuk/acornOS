ENTRY(_start)

SECTIONS {
    . = 0xFFFFFFFF80100000;             /* Higher half kernel virtual address */
                                        /* Physical address is 0x100000 (1MB) */
                                        /* Stage 2 maps virtual -> physical for us */ 

    .text : {                           /* Code section */
        *(.text.boot)
        *(.text)
    }

    .rodata : {                         /* Read only data - string literals live here */
        *(.rodata)
        *(.rodata.*)                    /* Compiler sometimes splits rodata into subsections */
    }

    .data : {                           /* Initialised global variables */
        *(.data)
    }

    .bss : {                            /* Uninitialised global variables - zeroed at boot */
        __bss_start = .;                /* Mark start of BSS */
        *(.bss)
        *(COMMON)
        __bss_end = .;                  /* Mark end of BSS */
    }
}