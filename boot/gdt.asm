; Even though 64-bit mode uses a flat address space and 
; ignores segment bases/limits, the processor still requires 
; a GDT to define the privilege level (ring 0 vs ring 3) 
; and the instruction set (32-bit compatibility vs 64-bit long mode).

[bits 16]

gdt_start:

; The null descriptor is the mandatory first entry. The processor 
; hardware expects the first 8 bytes to be zero. Using index 0 results 
; in a general protection fault, which helps catch null pointer errors.
gdt_null:
    dd 0x00000000           
    dd 0x00000000           

; This is the code segment descriptor where 0x08 is the first entry 
; after null (8 bytes), used for code segment register.
gdt_code:
    dw 0xFFFF                ; Limit - Sets bits 0-15 of the segment size                          
    dw 0x0000                ; Base - Bits 0-15 of the starting address (0x0)         
    db 0x00                  ; Base - Bits 16-23                   
    
    ; This is the access byte. Ring 0 is required for kernel-level EHCI/hardware access.
    ; 1 (Present) | 00 (Privilege: Ring 0) | 1 (S: Code/Data) | 1 (Type: Code) 
    ; | 0 (Conforming) | 1 (Readable) | 0 (Accessed)
    db 10011010B                    

    ; This is the flags & limit. Granularity bit is needed; it turns 0xFFFF into 4GB total limit.
    ; 1 (Granularity: 4KiB blocks) | 1 (Size: 32-bit PM) | 0 (L: 64-bit) | 0 (AVL)
    ; | 1111 (Limit bits 16-19)                       
    db 11001111B                    
                                    
    db 0x00                  ; Base - Bits 24-31             

; This is the data segment descriptor where 0x10 is 
; the second entry (16 bytes), used for DS, SS, ES, FS, GS registers.
gdt_data:
    dw 0xFFFF                              
    dw 0x0000                        
    db 0x00 
    ; This is the same as code segment, but Type bit 
    ; is 0 (Data) and 1 (Writable). Code segments are not writable 
    ; but data segments are.                         
    db 10010010B                    
                                    
    db 11001111B                       
    db 0x00                            

gdt_end:

; This is the pointer passed to the LGDT instruction.
; The size is always (length - 1) because the 
; max value 0xFFFF represents a 65536-byte table.
gdt_descriptor:
    dw gdt_end - gdt_start - 1      
    dd gdt_start                   

; These are the constants for loading segment registers during 
; the protected mode switch. These define the offset 
; from the start of the GDT.
CODE_SEG equ gdt_code - gdt_start 
DATA_SEG equ gdt_data - gdt_start   

gdt64_start:

gdt64_null:
    dq 0x0000000000000000

; In long mode, the 'L' bit (long bit) tells the CPU 
; to treat instructions as 64-bit code.
; Base/limit: Ignored by hardware (forced to 0 and 4GB+ respectively).
; Access byte (0x9A): 1 (Present) | 00 (Ring 0) | 1 (System) | 1 (Code) | 0 (Non-conforming) | 1 (Readable)
; Flags (0x2): 0 (Available) | 1 (Long Mode Bit) | 0 (Default Operation Size) | 0 (Granularity)
gdt64_code:
    dw 0xFFFF                ; Ignored limit
    dw 0x0000                ; Ignored base
    db 0x00                  ; Ignored base
    db 10011010B             ; Access byte (execute/read)
    db 00100000B             ; Long mode bit set (Bit 5 of this byte/Bit 21 of descriptor)
    db 0x00                  ; Ignored base 

; While the CPU doesn't use the data segment for offset 
; calculations anymore, we still need it for consistency
; in the stack (SS) and data (DS) registers 
; when transitioning between privilege levels.
gdt64_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010B             ; Access byte (read/write)
    db 00000000B             ; L-bit must be 0 for data segments
    db 0x00 

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

CODE64_SEG equ gdt64_code - gdt64_start
DATA64_SEG equ gdt64_data - gdt64_start