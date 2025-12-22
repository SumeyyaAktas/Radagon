#include <stdint.h>
#include "include/ports.h"
#include <stdbool.h>
#include "vga.h"
#include "serial.h"

void hcf(void)
{
    for (;;)
    {
        asm("hlt");
    }
}

void kernel_main(void) 
{
    vga_init();
    serial_init();

    serial_print("\n64-bit kernel running!\n");
    vga_print("\n\n\n\n\n\n64-bit kernel running!\n");
    
    hcf();
}