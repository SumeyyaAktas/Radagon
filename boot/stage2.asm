[org 0x7E00]
[bits 16]

%define KERNEL_OFFSET 0x1000
%define KERNEL_SEGMENT 0x0000
%define KERNEL_SECTORS 32 
%define VGA_THIRD_LINE_OFFSET 480          

start_stage2:
    mov [boot_drive], dl
    
    mov bx, real_mode_str
    call print16_string
    call print16_newline

    call check_long_mode
    cmp ax, 0
    je .no_long_mode

    call enable_a20
    
    mov bx, loading_kernel_str
    call print16_string
    call print16_newline

    mov cx, 3                                     
                       
.load_retry:
    push cx                    

    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13

    mov ax, KERNEL_SEGMENT
    mov es, ax
    mov bx, KERNEL_OFFSET
    
    mov ah, 0x02               
    mov al, KERNEL_SECTORS     
    mov ch, 0                  
    mov cl, 32               
    mov dh, 0                  
    mov dl, [boot_drive]
    int 0x13
    
    pop cx                
    jnc .load_success         

    mov bx, retry_str
    call print16_string
    loop .load_retry      
    jmp .disk_error

.load_success:
    mov ax, [KERNEL_OFFSET]
    cmp ax, 0
    je .disk_error
    
    mov bx, kernel_loaded_str
    call print16_string
    call print16_newline
    
    mov bx, switching_pm_str
    call print16_string
    call print16_newline

    mov dword [cursor_pos], VGA_THIRD_LINE_OFFSET 
    
    call switch_to_pm

.disk_error:
    mov bx, kernel_error_str
    call print16_string
    call print16_newline
    cli
    hlt
    jmp $

.no_long_mode:
    mov bx, no_long_mode_str
    call print16_string
    call print16_newline
    cli
    hlt
    jmp $

check_long_mode:
    pusha

    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    jz .no_cpuid

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    
    popa
    mov ax, 1
    ret

.no_cpuid:
.no_long_mode:
    popa
    mov ax, 0
    ret

enable_a20:
    pusha
    in al, 0x92
    or al, 2
    out 0x92, al
    popa
    ret

[bits 32]

begin_pm:
    mov ebx, protected_mode_str
    call print32_string
    call print32_newline
    
    mov ebx, setting_up_paging_str
    call print32_string
    call print32_newline

    call setup_page_tables

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov eax, pml4_table
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_descriptor]
    
    mov ebx, jumping_long_mode_str
    call print32_string
    call print32_newline

    jmp CODE64_SEG:long_mode_start

setup_page_tables:
    mov edi, pml4_table
    mov ecx, 4096 * 3 / 4  
    xor eax, eax
    cld
    rep stosd

    mov edi, pml4_table
    mov eax, pdp_table
    or eax, 0b11  
    mov [edi], eax

    mov edi, pdp_table
    mov eax, pd_table
    or eax, 0b11
    mov [edi], eax

    mov edi, pd_table
    mov eax, 0x0
    or eax, 0b10000011  
    mov [edi], eax
    
    ret

[bits 64]

long_mode_start:
    mov ax, DATA64_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rbp, 0x90000
    mov rsp, rbp

    mov rax, KERNEL_OFFSET
    jmp rax

%include "boot/print16_string.asm"
%include "boot/print32_string.asm"
%include "boot/gdt.asm"
%include "boot/switch_to_pm.asm"

real_mode_str: db 'Running in 16-bit real mode', 0
loading_kernel_str: db 'Loading kernel from disk...', 0
kernel_loaded_str: db 'Kernel loaded successfully', 0
kernel_error_str: db 'Error: Failed to load kernel', 0
retry_str: db 'Retry...', 0
switching_pm_str: db 'Switching to protected mode...', 0
protected_mode_str: db 'Now in 32-bit protected mode', 0
setting_up_paging_str: db 'Setting up paging for long mode...', 0
jumping_long_mode_str: db 'Jumping to 64-bit long mode...', 0
no_long_mode_str: db 'Error: CPU does not support 64-bit long mode', 0

boot_drive: db 0
cursor_pos: dd 0       

align 4096
pml4_table:
    times 4096 db 0
    
pdp_table:
    times 4096 db 0
    
pd_table:
    times 4096 db 0

times 16384 - ($ - $$) db 0