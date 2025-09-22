#include <stdint.h>
#include "rprintf.h"

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) =
    {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

#define VGA_ADDRESS 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

struct termbuf {
    char ascii;
    char color;
};

static struct termbuf *vram = (struct termbuf*)VGA_ADDRESS;

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

int putc(int c) {
    if (c == '\n') {
        x = 0;
        y++;
    } else {
        vram[y * VGA_COLS + x].ascii = (char)c;
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
    // to show scrolling pushes this off screen
    esp_printf(putc, "Hello World!\r\n");
    esp_printf(putc, "Execution level: %d\n");
   
    // set the loop to go to 23 so you can see the execution level
    for (int i = 0; i < 23; i++) {
         esp_printf(putc, "Scrolling\n");
    }

    while (1);
}

