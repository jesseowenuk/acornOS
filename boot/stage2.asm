[BITS 16]
[ORG 0x7E00]

; --- Stage 2 Bootloader --------------------------------------------
; Loaded by Stage 1 at 0x7E00
; We start in 16-bit real mode - same as stage 1 left us
; Our job is to get from here to 64-bit long mode

; --- Constants ------------------------------------------------------
; We'll load kernel here (1MB physical)
KERNEL_PHYSICAL_BASE equ 0x100000

; Temporary stack during boot
BOOT_STACK_TOP equ 0x7000

; We'll store E820 map here
E820_MAP_ADDRESS equ 0x800

; We'll store E820 count here
E820_COUNT_ADDRESS equ 0x600

; TEMPORARY:::: 
; hello.elf loaded here (3MB physical)
HELLO_ELF_PHYSICAL_BASE equ 0x300000
HELLO_ELF_SECTOR equ 512

; --- Entry point -----------------------------------------------------
start:
    ; Stage 1 jumped here with DL = boot drive
    ; Save it immediatley
    mov [boot_drive], dl

    ; Set up segments - all zero for flat real mode
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov sp, BOOT_STACK_TOP              ; Temporary stack during real mode

    ; Print hello message so we know stage 2 is running
    mov si, msg_hello
    call print
    
    ; Enable A20 Line
    call enable_a20
    mov si, msg_a20
    call print

    ; Detect memory via E820
    call detect_memory
    mov si, msg_memory
    call print

    ; Load kernel from disk
    call load_kernel
    mov si, msg_kernel
    call print

    ; Load hello.elf from disk (temporary test program)
    call load_hello_elf

    mov eax, dword [0xA000]
    mov dword [0xA000], eax
    mov eax, dword [0xA004]
    mov dword [0xA004], eax

    ; Save kernel sectors
    mov eax, dword [kernel_sectors]
    mov dword [0xA008], eax

    ; Disable interrupts - critical before mode switch
    cli

    ; Enter Protected mode
    call enter_protected_mode

    ; We never get here - enter_protected_mode jumps away

    jmp $

; --- Print routine -----------------------------------------------------
print:
    mov ah, 0x0E                ; BIOS teletype function
.loop:
    lodsb                       ; Load byte at [SI] into AL, then increment SI
    cmp al, 0                   ; Is it the null terminator?
    je .done                    ; If yes, we're finished
    int 0x10                    ; Otherwise, print the character
    jmp .loop                   ; And loop

.done: 
    ret                         ; Return to caller

; --- Enable A20 line -----------------------------------------------------
; The A20 line must be enabled to access memory above 1MB
; We try three methods in order of preference
;   1. BIOS method (cleanest)
;   2. Fast A20 via port 0x92 (most common)
;   3. Keyboard controller method (old but reliable)

enable_a20:
    ; Method 1: BIOS INT 0x15 AX=0x2401
    mov ax, 0x2401
    int 0x15
    jnc .done                   ; Carry clear = success

    ; Method 2: Fast A20 via port 0x92
    in al, 0x92                 ; Read current value
    test al, 2                  ; Is A20 already set?
    jnz .done                   ; Yes - already enabled
    or al, 2                    ; Set bit 1 = A20 enable
    and al, 0xFE                ; Clear bit 0 to avoid reset
    out 0x92, al                ; Write back

.done:
    ; Verify A20 is actually enabled
    ; Write a value to 0x000500 and read it back from 0x100500
    ; If they're different A20 is enabled (different memory locations)
    ; Otherwise, A20 is not enabled (same location due to memory wrap)
    push ds
    push es

    xor ax, ax
    mov ds, ax                      ; DS = 0x0000
    mov ax, 0xFFFF              
    mov es, ax                      ; ES = 0xFFFF

    mov word [ds:0x0500], 0xAA55    ; Write to 0x000500
    mov word [es:0x0510], 0x55AA    ; Write to 0x100500
    
    cmp word [ds:0x0500], 0xAA55    ; Did first write survive?
    jne .a20_ok                     ; Different = A20 working

    ; A20 still disabled - try keyboard controller method
    call enable_a20_keyboard

.a20_ok:
    pop es
    pop ds
    ret

; Keyboard controller A20 enable (last resort)
enable_a20_keyboard:
    cli
    call .wait_keyboard
    mov al, 0xAD                ; Disable keyboard
    out 0x64, al
    call .wait_keyboard
    mov al, 0xD0                ; Read output port
    out 0x64, al
    call .wait_data
    in al, 0x60                 ; Read data
    push ax
    call .wait_keyboard
    mov al, 0xD1                ; Write to output port
    out 0x64, al
    call .wait_keyboard
    pop ax
    or al, 2                    ; Set A20 bit
    out 0x60, al
    call .wait_keyboard
    mov al, 0xAE                ; Enable keyboard
    out 0x64, al
    call .wait_keyboard
    sti
    ret

.wait_keyboard:
    in al, 0x64
    test al, 2
    jnz .wait_keyboard
    ret

.wait_data:
    in al, 0x64
    test al, 1
    jz .wait_data
    ret

;--- E820 Memory Detection ------------------------------------------------
; Asks BIOS for complete memory map
; Stores entries at E820_MAP_ADDRESS
; Stores count at E820_COUNT_ADDRESS

detect_memory:
    xor ax, ax
    mov ds, ax
    mov es, ax                          ; ES = 0 for buffer addressing
    xor ebx, ebx                        ; EBX = 0 to start enumeration

    mov word [E820_COUNT_ADDRESS], 0    ; Zero the count
    mov di, E820_MAP_ADDRESS            ; DI points to our buffer
    mov dword [highest_ram], 0          ; Initialise highest RAM to 0
    mov dword [highest_ram+4], 0        ; High 32 bits also zero

.loop:
    mov eax, 0xE820                     ; Function code - reset each iteration
    mov edx, 0x534D4150                 ; 'SMAP' magic - reset each iteration
    mov ecx, 24                         ; Entry size - reset each iteration
    mov dword [di+20], 1                ; ACPI extention attribute
    int 0x15                            ; Call BIOS

    jc .done                            ; Carry = end of list error
    cmp eax, 0x534D4150                 ; Verify BIOS returned SMAP
    jne .done                           ; Something went wrong

    test ecx, ecx                       ; Skip zero length entries
    jz .next

    inc word [E820_COUNT_ADDRESS]       ; Increment entry count

    ; Check if this is a usable RAM entry (type 1)
    cmp dword [di+16], 1                ; Type field at offset 16
    jne .skip_highest                   ; Not usable RAM - skip

    ; Calculate end address = base + length
    ; Both are 64-bit values at [di+0] (base) and [di+8] (length) 
    mov eax, dword [di+0]               ; Base low 32 bits
    mov edx, dword [di+4]               ; Base high 32 bits
    add eax, dword [di+8]               ; Add length low 32 bits
    adc edx, dword [di+12]              ; Add length high 32 bits with carry     

    ; Is this end address = base + length
    cmp edx, dword [0xA004]      ; Compare high 32 bits
    ja .new_highest                     ; Definitley higher
    jb .skip_highest                    ; Definitiley lower
    cmp eax, dword [0xA000]        ; High bits equal - compare low bits
    jbe .skip_highest                   ; Not higher

.new_highest:
    mov dword [0xA000], eax        ; Update highest RAM low 32 bits
    mov dword [0xA004], edx    ; Update highest RAM high 32 bits


.skip_highest:
    add di, 24               

.next:
    test ebx, ebx                       ; EBX = 0 means last entry
    jz .done
    jmp .loop

.done:
    ret

; --- Load Kernel ---------------------------------------------------------
; Reads kernel sector count from sector 63
; Loads kernel to KERNEL_PHYSICAL_BASE (0x100000)

load_kernel:
    ; Step 1: read kernel sector count from sector 63
    ; We store it as a 4-byte little-endian value
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, packet_meta
    int 0x13
    jc .disk_error

    ; Step 2: read sector count from buffer
    ; It was loaded to 0x5000
    mov eax, [0x5000]               ; Read 4-byte sector count
    mov [kernel_sectors], eax       ; Save it
    test eax, eax                   ; Sanity check - not zero?
    jz .disk_error

    ; Step 3: build LBA packet for kernel load
    ; Kernel lives at sector 64 on disk
    ; Kernel physical address = 0x100000
    ; As we're in 16-bit can only access 64KB segments so only
    ; load 50 sector chunks

    ; We use segment:offset addressing
    ; Physical address = segment_address * 16 + offset
    ; 0x100000 = 0x1000 * 16 + 0 = segment 0x1000, offset 0

    mov dword [packet_kernel + 8], 64           ; Start LBA = sector 64
    mov dword [packet_kernel + 12], 0           ; LBA high = 0

    ; Set up buffer at segment 0x1000 offset 0 = physical address 0x100000
    mov word [packet_kernel + 4], 0x0000        ; Buffer offset = 0
    mov word [packet_kernel + 6], 0x1000        ; Buffer segment = 0x10000

    ; How many sectors can we load at once?
    ; Each sector = 512 bytes
    ; Segment is 64KB = 128 sectors max before we wrap
    ; We'll load 50 sectors at a time to be safe

    mov ecx, [kernel_sectors]                   ; Total sectors to load
    mov ebx, 64                                 ; Current LBA sector
    mov edx, 0x1000                             ; Current segment

.load_loop:
    test ecx, ecx                               ; Any sectors left?
    jz .done                                    ; No? Then we're done

    ; How many to load this iteration
    cmp ecx, 50                                 ; More than 50 remaining
    jle .last_chunk                             
    mov ax, 50                                  ; Load 50 sectors this pass
    jmp .do_load

.last_chunk:
    movzx ax, cl                                ; Load remaining sectors

.do_load:
    ; Update packet
    mov word [packet_kernel + 2],ax             ; Sector count
    mov dword [packet_kernel + 8], ebx          ; LBA start
    mov word [packet_kernel + 4], 0x0000        ; Offset always 0
    mov word [packet_kernel + 6], dx            ; Current segment

    ; Do the read
    push ax                                     ; Save sector count
    push ecx                                    ; Save remaining count
    push edx                                    ; Current segment

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, packet_kernel
    int 0x13
    jc .disk_error

    pop edx                                     ; Restore segment
    pop ecx                                     ; Restore remaining
    pop ax                                      ; Restore count loaded

    ; Advance pointers
    ; EAX = sectors loaded this pass (from pop ax above)
    movzx ebx, ax                               ; zero extend AX to 32-bit

    ; Advance LBA by sectors loaded
    add dword [packet_kernel+8], eax

    ; Advance segment: sectors * 512 / 16 = sectors * 32
    push eax                                    ; Save sector count
    shl eax, 5                                  ; EAX = sectors * 32 = segment advance
    add edx, eax                                ; Advance segment
    pop eax

    ; Reduce remaining count
    sub ecx, eax

    ; Reload LBA for next iteration
    mov ebx, dword [packet_kernel+8]

    jmp .load_loop

.done:
    ret

.disk_error:
    mov si, msg_disk_error
    call print
    jmp $

; --- Load hello.elf -----------------------------------------------------
; Loads our first user space program from disk
; TEMPORARY: will be replaced by proper ELF loading from a filesystem
; once we have persistent storage (barkFS)
;
; Loaded to HELLO_ELF_PHYSICAL_BASE (0x300000 / 3MB mark)
; Same segment:offset trick as kernel - load to 0x1000:0 (physical location)
; then copied up to 0x300000 in protected mode.

load_hello_elf:
    ; Read just 4 sectors (2KB) - plenty for our tiny test program
    mov dword [packet_hello + 8], HELLO_ELF_SECTOR          ; LBA start
    mov dword [packet_hello + 12], 0                        ; LBA high = 0
    mov word [packet_hello + 2], 128                        ; 128 sectors
    mov word [packet_hello + 4], 0x0000                     ; Buffer offset
    mov word [packet_hello + 6], 0x2000                     ; Physical = 0x2000*16 = 0x20000

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, packet_hello
    int 0x13
    jc .disk_error

    ret

.disk_error:
    mov si, msg_disk_error
    call print
    jmp $

; --- Enter Protected Mode -----------------------------------------------
; Sets up a temporary GDT and switches to 32-bit protected mode
; This is a stepping stone to 64-bit long mode

enter_protected_mode:
    ; Load our temporary GDT
    lgdt [gdt32_descriptor]

    ; Set PE bit in CR0 (bit 0) to enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush pipeline and load CS with 32-bit code segment
    ; This is where we actually enter protected mode
    jmp 0x08:protected_mode_entry

; --- Data ----------------------------------------------------------------

; --- Disk Packets --------------------------------------------------------
align 4
packet_meta:
    db 0x10, 0x00                               ; Packet size, reserved
    dw 1                                        ; Read 1 sector
    dw 0x0000                                   ; Buffer offset
    dw 0x0500                                   ; Buffer segment = physical = 0x5000
    dq 63                                       ; LBA sector 63 (metadata)

align 4
packet_kernel:
    db 0x10, 0x00                               ; Packet size, reserved
    dw 0                                        ; Sector count (filled in dynamically)
    dw 0x0000                                   ; Buffer offset (filled in dynamically)
    dw 0x0000                                   ; Buffer segment (filled in dynamically)
    dq 0                                        ; LBA start (filled in dynamically)

align 4
packet_hello:
    db 0x10, 0x00                               ; Packey size, reserved
    dw 0                                        ; Sector count (filled in)
    dw 0x0000                                   ; Buffer offset (filled in)
    dw 0x0000                                   ; Buffer segment (filled in)
    dq 0                                        ; LBA start (filled in)

; --- Temporary 32-bit GDT -----------------------------------------
; Just enough to get us into protected mode
; We'll replace this with a proper 64-bit GDT later

align 8
gdt32:
    ; Null descriptor
    dq 0

    ; 32-bit code segment
    ; Base=0, Limit=4GB, Ring 0, Execute/Read
    dw 0xFFFF                               ; Limit low
    dw 0x0000                               ; Base low
    db 0x00                                 ; Base middle
    db 10011010b                            ; Access: present, ring 0, code, executable, readable
    db 11001111b                            ; Flags: 4KB granularity, 32-bit limit high = 0xF
    db 0x00                                 ; Base high

    ; 32-bit data segment
    ; Base=0, Limit=4GB, Ring 0, Read/Write
    dw 0xFFFF                               ; Limit low
    dw 0x0000                               ; Base low
    db 0x00                                 ; Base middle
    db 10010010b                            ; Access: present, ring 0 data, writable
    db 11001111b                            ; Flags: 4KB granularity, 32-bit, limit high = 0xF
    db 0x00                                 ; Base high

gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32 - 1                ; GDT size - 1
    dd gdt32                                ; GDT physical address

; --- Variables ----------------------------------------------------
kernel_sectors dd 0                             ; Number of kernel sectors to load
highest_ram dq 0                                ; Highest physical RAM address (64-bit)

; --- Messages ----------------------------------------------------
boot_drive db 0
msg_hello db 'Stage 2 running!', 13, 10, 0
msg_a20 db 'A20 enabled', 13, 10, 0
msg_memory db 'Memory map detected!', 13, 10, 0
msg_kernel db 'Kernel loaded!', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0

; --- 32-bit protected mode entry
[BITS 32]
protected_mode_entry:
    ; Load data segment registers with data segment selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 32-bit stack
    mov esp, 0x9000                         ; Temporary 32-bit stack

    ; Copy kernel from 0x10000 to 0x100000
    mov ecx, dword [0xA008]                 ; kernel_sector_count
    shl ecx, 9                              ; * 512 = byte count
    mov esi, 0x10000                        ; source
    mov edi, 0x100000                       ; destination
    rep movsb                               ; copy

    ; Copy hello.elf from 0x20000 to 0x300000
    mov ecx, 128 * 512                      ; 128 sectors * 512
    mov esi, 0x20000                        ; source
    mov edi, 0x300000                       ; destination
    rep movsb                               ; copy

    ; Load highest_ram into EDX:EAX and call setup
    mov eax, dword [0xA000]            ; Low 32 bits
    mov edx, dword [0xA004]              ; High 32 bits
    call setup_page_tables

    ; Enter long mode
    call enter_long_mode

    ; Hang for now
    jmp $

; --- Setup Page Tables -----------------------------------------------------
; Sets up 4-level page tables for 64-bit long mode
;
; Virtual memory layout:
;   0x0000000000000000 -> identity map (2GB temporary)
;   0xFFFF800000000000 -> direct physical map (all RAM, 1GB pages)
;   0xFFFFFFFF80100000 -> kernel (actual kernel size, 2MB pages)
;
; Page table locations:
;   0x1000 - PML4
;   0x2000 - Identity map PDPT
;   0x3000 - Identity map PD (2MB pages)
;   0x4000 - Direct map PDPT (1GB pages, covers all RAM)
;   0x5000 - Kernel PDPT
;   0x6000 - Kenrel PD
;   0x7000 - Kernel PT
;
; Called from 32-bit protected mode
; EAX = highest physical RAM address (low 32 bits)
; EDX = highest physical RAM address (high 32 bits)

setup_page_tables:
    ; Clear page tables 0x1000 - 0x7000
    mov dword [0x900], eax              ; Save highest_ram low to scratch
    mov dword [0x904], edx              ; Save highest_ram high to scratch

    ; Clear page table 0x1000 to 0x8000 (7 tables x 4KB)
    ; 0x1000=PML4, 0x2000=ident PDPT, 0x3000=ident PD,
    ; 0x4000=direct PDPT, 0x5000=kernel PDPT, 0x6000=kernel PD
    ; 0x7000=direct map PD (2MB pages)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, (0x6000 / 4)               ; 24KB
    rep stosd

    ; Clear direct map PD at 0x7000 (up to 0x7E00 - stage 2 starts there!)
    mov edi, 0x7000
    xor eax, eax
    mov ecx, (0xE00 / 4)                ; 3584 bytes         
    rep stosd                           ; Clobbers EAX, ECX, EDI

    ; Restore highest_ram
    mov eax, dword [0x900]                                
    mov edx, dword [0x904]                               

    ; --- PML4 -------------------------------------------------------
    ; PML4[0] -> identity PDPT (0x2000) temporary
    ; PML4[256] -> direct map PDPT (0x4000) ALL RAM
    ; PML4[511] -> kernel PDPT (0x5000)
    mov dword [0x1000 + 0 * 8], 0x2003
    mov dword [0x1000 + 256 * 8], 0x4003
    mov dword [0x1000 + 511 * 8], 0x5003

    ; --- Identity map (at 0x2000) -----------------------------------
    ; Must cover all of low memory we use:
    ; 0x0 to 0x200000 (2MB) covers stage2, page_tables, kernel load
    mov dword [0x2000], 0x3003

    ; PD - map first 2MB (enough to execute from during transition)
    ; This is temporary - kernel will remove it after boot
    mov dword [0x3000], 0x0083              ; 2MB at physical 0x0

    ; --- Direct physical map -----------------------------------------
    ; Map ALL RAM using 1GB pages
    ; Entry N -> physical N * 1GB
    ; Number of entries = ceil(highest_ram / 1GB)
    mov dword [0x4000], 0x7003              ; PDPT[0] -> PD at 0x7000

    ; Fill PD at 0x7000 with 2MB pages covering all RAM
    ; entries = ceil(highest_ram / 2MB)
    ; Reload highest_ram from scratch
    mov eax, dword [0x900]
    mov edx, dword [0x904] 

    shrd eax, edx, 21                       ; Divide by 2MB (shift right by 21)
    shr edx, 21
    inc eax                                 ; Round up
    mov ecx, eax                            ; ECX = number of 2MB entries

    ; Fill direct map PDPT at 0x4000
    xor esi, esi                            ; Entry index
    xor ebx, ebx                            ; Current physical address low
    xor edx, edx                            ; Current physical address high

.direct_map_loop:
    mov dword [0xA010], ecx

    cmp esi, ecx                            ; Done all entries?
    jge .direct_map_done

    ; Write PDPT entry: physical_address | 0x83
    ; Each entry is 8 bytes (64-bit)
    ; Low 32 bits: physical_address_low | 0x83
    ; High 32 bits: physical_address_high 
    mov eax, ebx
    or eax, 0x83                            ; Present + writable + 1GB page
    mov dword [0x7000 + esi * 8], eax       ; Low 32 bits
    mov dword [0x7000 + esi * 8 + 4], edx   ; High 32 bits

    ; Advance to next 2MB boundary
    add ebx, 0x200000                       ; next 2MB boundary
    adc edx, 0                              ; Carry to high address
    inc esi
    jmp .direct_map_loop

.direct_map_done:
    ; --- Kernel PDPT (at 0x5000) ---------------------------------------
    ; Kernel virtual address: 0xFFFFFFFF80100000
    ; 
    ; Breakdown:
    ;   PML4[511] -> PDPT at 0x5000
    ;   PDPT[510] -> PD at 0x6000
    ;   PD[0] -> 2MB page at physical 0x0
    mov dword [0x5000 + 510 * 8], 0x6003    ; PD at 0x6000

    ; --- Kernel PD (at 0x6000) -----------------------------------------
    ; PD[0] -> 2MB page at physical 0x000000
    ; Flags: present(1) + writable(2) + PS/2MB(0x80) = 0x83
    ; Physical address 0x000000 with 2MB page flag
    mov dword [0x6000], 0x0083

    ret

; --- Enter Long Mode ---------------------------------------------------
; Transitions from 32-bit protected mode to 64-bit long mode
; After this we jump to the kernel at 0xFFFFFFFF80100000

enter_long_mode:
    ; Step 1: Load PML4 into CR3
    mov eax, 0x1000                         ; PML4 is at physical 0x1000
    mov cr3, eax

    ; Step 2: Enable PAE (Physical Address Extension)
    ; Required for long mode - allows 64-bit addresses
    mov eax, cr4
    or eax, (1 << 5)                        ; Set PAE bit (bit 5)
    mov cr4, eax

    ; Step 3: Enable long mode in EFER MSR
    ; EFER = Extended Feature Enable Register
    ; MSR address: 0xC0000080
    mov ecx, 0xC0000080                     ; EFER MSR number
    rdmsr                                   ; Read into EDX:EAX
    or eax, (1 << 8)                        ; Set LME bit (Long Mode Enable, bit 8)
    wrmsr                                   ; Write back

    ; Step 4: Enable paging (CR0.PG = 1)
    ; This activates long mode (CPU switches from protected to compatibility)
    mov eax, cr0
    or eax, (1 << 31)                       ; Set PG bit (bit 31)
    mov cr0, eax

    ; CPU is now in 64-bit compatability mode
    ; We need a far jump to load a 64-bit code segment
    ; This puts us in full 64-bit long mode

    ; Step 5: Load 64-bit GDT
    lgdt [gdt64_descriptor]

    ; Write 'G' to confirm GDT loaded
    mov dword [0xB8008], 0x07470700

    ; Step 6: Far jump via memory to 64-bit code segment
    ; Must use indirect jump - direct far jump doesn't work reliably
    mov dword [far_jump_target], long_mode_entry
    mov word [far_jump_target + 4], 0x08
    jmp far [far_jump_target]

; --- 64-bit GDT --------------------------------------------------------
; Minimal GDT for 64-bit long mode
; In 64-bit most GDT fields are ignored - only the L bit matters

align 8
gdt64:
    ; Null descriptor
    dq 0

    ; 64-bit code segment
    ; L bit (bit 53) must be set for 64-bit code
    dw 0x0000                           ; Limit (ignored for 64-bit)
    dw 0x0000                           ; Base low (ignored)
    db 0x00                             ; Base middle (ignored)
    db 10011010b                        ; Access: present, ring=0, code, executable, readable
    db 00100000b                        ; Flags: L=1 (64-bit), D=0 (must be 0 for 64-bit)
    db 0x00                             ; Base high (ignored)

    ; 64-bit data segment
    dw 0x0000                           ; Limit (ignored for 64-bit)
    dw 0x0000                           ; Base low (ignored)
    db 0x00                             ; Base middle (ignored)
    db 10010010b                        ; Access: present, ring=0, data, executable, readable
    db 00100000b                        ; Flags: L=1 (64-bit), D=0 (must be 0 for 64-bit)
    db 0x00                             ; Base high (ignored)

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64 - 1            ; GDT size - 1
    dq gdt64                            ; 64-bit GDT address (8 bytes in 64 bit!)

far_jump_target:
    dd 0                                ; Offset filled in at runtime
    dw 0                                ; Selector filled in at runtime

; --- 64-bit long mode entry ------------------------------------------------
[BITS 64]
long_mode_entry:
    ; We're now in 64-bit long mode
    ; Load 64-bit data segments
    mov ax, 0x10                        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 64-bit stack
    mov rsp, 0xFFFF800000009000                     

    ; Signal we're in long mode - write 'LM' to VGA
    mov rbx, 0xB8000
    mov dword [rbx], 0x074A074A     ; 'JJ' at columns 0,1

    ; Pass E820 info to kernel
    ; kernel_main(mem_map_addr, mem_map_count, highest_ram)
    mov rdi, 0x800                          ; E820 map address
    movzx rsi, word [0x600]                 ; E820 entry count
    mov edx, dword [0xA000]                 ; highest_ram low (from fixed address)
    mov ecx, dword [0xA004]                 ; Highest_ram high

    ; Check PD entry 17 at 0x7000 + 17*8 = 0x7088
    ;mov eax, dword [0xA010]
    ;mov dword [0xB8000], eax
    ;jmp $

    ; Jump to kernel virtual address
    mov rax, 0xFFFFFFFF80100000             ; Kernel entry point
    jmp rax                                 ; Jump to higher half kernel