[bits 16]

gdt_start:

gdt_null:
    dd 0x00000000           
    dd 0x00000000           

gdt_code:
    dw 0xFFFF                                  
    dw 0x0000                       
    db 0x00                             
    db 10011010B                    
                                    
    db 11001111B                    
                                    
    db 0x00                             


gdt_data:
    dw 0xFFFF                              
    dw 0x0000                        
    db 0x00                         
    db 10010010B                    
                                    
    db 11001111B                       
    db 0x00                            

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1      
    dd gdt_start                   

CODE_SEG equ gdt_code - gdt_start 
DATA_SEG equ gdt_data - gdt_start   

; 64-bit GDT
gdt64_start:

gdt64_null:
    dq 0x0000000000000000

gdt64_code:
    dq 0x00209A0000000000  ; 64-bit code segment
    ; Bits: P=1, DPL=0, S=1, Type=1010 (Execute/Read)
    ; L=1 (64-bit), D=0, G=0

gdt64_data:
    dq 0x0000920000000000  ; 64-bit data segment
    ; Bits: P=1, DPL=0, S=1, Type=0010 (Read/Write)

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

CODE64_SEG equ gdt64_code - gdt64_start
DATA64_SEG equ gdt64_data - gdt64_start