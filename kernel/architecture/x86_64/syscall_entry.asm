[BITS 64]

global syscall_entry
extern syscall_handler

; --- SYSCALL entry point -----------------------------------------------
;
; On entry (set)