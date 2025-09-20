#include <stdint.h>
#include "rprintf.h"

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) =
    {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12};

#define VGA_ADDRESS 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

struct termbuf {
    char ascii;
    char color;
};

static int cursor_row = 0;
static int cursor_col = 0;

static struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;

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
        cursor_row++;
        cursor_col = 0;
    } else {
        vram[cursor_row * VGA_COLS + cursor_col].ascii = (char)data;
        vram[cursor_row * VGA_COLS + cursor_col].color = 7;
        cursor_col++;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_ROWS) {
        scroll();
        cursor_row = VGA_ROWS - 1;
    }

    return 0;
}


void main() {
    esp_printf(putc, "Hello World!\n");
    esp_printf(putc, "Execution level: %d\n", 0);

    for (int i = 0; i < 23; i++) {
    	esp_printf(putc, "Scrolling at %d\n", i);
    }

    while (1);
}

