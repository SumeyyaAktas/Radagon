#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ports.h"
#include "driver/vga.h"
#include "driver/serial.h"
#include "driver/pci.h"
#include "driver/ahci.h"

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

    vga_print("                                    RADAGON\n");
    vga_print("          Custom 64-bit x86 Bootloader with AHCI (SATA) Storage Driver\n\n");
    
    serial_print("\n64-bit kernel running\n");

    pci_init();
    
    serial_print("\nKernel initialization complete\n");
    
    hcf();
}