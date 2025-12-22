#ifndef VGA_H
#define VGA_H

#define VIDEO_MEMORY 0xB8000
#define WHITE_ON_BLACK 0x0F
#define BLACK 0x0
#define WHITE 0xF

#define VGA_COLOR(fg, bg) ((bg << 4) | fg)

void vga_init(void);
void vga_print(const char* str);

#endif