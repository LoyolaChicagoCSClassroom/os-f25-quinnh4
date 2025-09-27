#include <stdint.h>
#include "io.h"
#include "rprintf.h"   

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) =
    {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12};

#define VGA_ADDRESS 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

unsigned char keyboard_map[128] =
{
   0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
 '9', '0', '-', '=', '\b',     /* Backspace */
 '\t',                 /* Tab */
 'q', 'w', 'e', 'r',   /* 19 */
 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
   0,                  /* 29   - Control */
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
'\'', '`',   0,                /* Left shift */
'\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
 'm', ',', '.', '/',   0,                              /* Right shift */
 '*',
   0,  /* Alt */
 ' ',  /* Space bar */
   0,  /* Caps lock */
   0,  /* 59 - F1 key ... > */
   0,   0,   0,   0,   0,   0,   0,   0,  
   0,  /* < ... F10 */
   0,  /* 69 - Num lock*/
   0,  /* Scroll Lock */
   0,  /* Home key */
   0,  /* Up Arrow */
   0,  /* Page Up */
 '-',
   0,  /* Left Arrow */
   0,  
   0,  /* Right Arrow */
 '+',
   0,  /* 79 - End key*/
   0,  /* Down Arrow */
   0,  /* Page Down */
   0,  /* Insert Key */
   0,  /* Delete Key */
   0,   0,   0,  
   0,  /* F11 Key */
   0,  /* F12 Key */
   0,  /* All other keys are undefined */
};

struct termbuf {
    char ascii;
    char color;
};

static struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;

// col
int x = 0;
// row
int y = 0;

void scroll() {
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            vram[(row - 1) * VGA_COLS + col] = vram[row * VGA_COLS + col];
        }
    }
    for (int col = 0; col < VGA_COLS; col++) {
        vram[(VGA_ROWS - 1) * VGA_COLS + col].ascii = ' ';
        vram[(VGA_ROWS - 1) * VGA_COLS + col].color = 7;
    }
}

int putc(int data) {
    if (data == '\n') {
        x = 0;
	y++;
    } else {
        vram[y * VGA_COLS + x].ascii = (char)data;
        vram[y * VGA_COLS + x].color = 7;
        x++;
        if (x >= VGA_COLS) {
            x = 0;
            y++;
        }
    }
    if (y >= VGA_ROWS) {
        scroll();
        y = VGA_ROWS - 1;
    }
    return 0;
}

void main() {
    esp_printf(putc, "Scancodes:\n");

    while (1) {
        uint8_t status = inb(0x64);        
        if (status & 0x01) {               
            uint8_t scancode = inb(0x60);
	    if (scancode > 128) {
	       continue;
	    }

            esp_printf(putc, "0x%02x    %c\n", scancode, keyboard_map[scancode]);
        }
    }
}

