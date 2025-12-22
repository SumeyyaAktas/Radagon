// kernel64.c
#include <stdint.h>
#include "include/ports.h"
#include <stdbool.h>

#define COM1 0x3F8

static bool serial_initialized = false;

static inline int is_transmit_empty(void) 
{
    return inb(COM1 + 5) & 0x20;
}

bool serial_init(void) 
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);

    serial_initialized = true;
}

void serial_write_char(char c) 
{
    if (!serial_initialized) 
    {
        return;
    }
    
    while (!is_transmit_empty());
    outb(COM1, c);
}

void serial_print(const char* str) 
{
    while (*str) 
    {
        serial_write_char(*str++);
    }
}

__attribute__((noreturn))
void _start(void) {
    serial_init();
    serial_print("64-bit kernel running!\n");
    for (;;) __asm__("hlt");
}
