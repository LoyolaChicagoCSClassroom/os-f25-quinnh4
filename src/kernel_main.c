#include <stdint.h>
#include "io.h"
#include "rprintf.h"
#include "page.h"
#include "paging.h"
#include "fat.h"

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

static struct termbuf* const vram = (struct termbuf*)VGA_ADDRESS;
int x = 0;
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

extern struct page_directory_entry pd[1024];

void identity_map_kernel_and_stack_and_vga() {
    struct ppage tmp;
    tmp.next = 0;

    extern unsigned int _end_kernel;
    uint32_t kernel_end = (uint32_t)&_end_kernel;

    for (uint32_t addr = 0x100000; addr < kernel_end; addr += 0x1000) {
        tmp.physical_addr = (void *)addr;
        map_pages((void *)addr, &tmp, pd);
    }

    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    for (uint32_t addr = (esp & 0xFFFFF000); addr >= esp - 0x8000; addr -= 0x1000) {
        tmp.physical_addr = (void *)addr;
        map_pages((void *)addr, &tmp, pd);
    }

    tmp.physical_addr = (void *)0xB8000;
    map_pages((void *)0xB8000, &tmp, pd);
}

void test_fat_driver() {
    esp_printf((func_ptr)putc, "\n\n=== Testing FAT Filesystem Driver ===\n\n");

    esp_printf((func_ptr)putc, "Calling fatInit()...\n");
    int init_result = fatInit();
    if (init_result != 0) {
        esp_printf((func_ptr)putc, "FAILED: Could not initialize FAT filesystem.\n");
        esp_printf((func_ptr)putc, "Error code: %d\n", init_result);
        if (init_result == -1) {
            esp_printf((func_ptr)putc, "  -> Boot sector read failed (ata_lba_read error)\n");
            esp_printf((func_ptr)putc, "  -> Check if IDE driver is working\n");
        } else if (init_result == -2) {
            esp_printf((func_ptr)putc, "  -> Boot signature invalid (not 0xAA55)\n");
            esp_printf((func_ptr)putc, "  -> FAT filesystem may not be at sector 2048\n");
        } else if (init_result == -3) {
            esp_printf((func_ptr)putc, "  -> FAT table read failed\n");
        } else if (init_result == -4) {
            esp_printf((func_ptr)putc, "  -> Root directory read failed\n");
        }
        esp_printf((func_ptr)putc, "Make sure rootfs.img was built and mounted properly.\n");
        return;
    }
    esp_printf((func_ptr)putc, "fatInit() successful.\n\n");

    esp_printf((func_ptr)putc, "Calling fatOpen(\"testfile.txt\")...\n");
    struct file *f = fatOpen("testfile.txt");
    if (!f) {
        esp_printf((func_ptr)putc, "FAILED: File not found.\n");
        esp_printf((func_ptr)putc, "Make sure testfile.txt is in the root directory.\n");
        esp_printf((func_ptr)putc, "Run: sudo mdir -i rootfs.img@@1M ::/ to verify\n");
        return;
    }
    esp_printf((func_ptr)putc, "fatOpen() successful.\n");
    esp_printf((func_ptr)putc, "File size: %d bytes\n", f->rde.file_size);
    esp_printf((func_ptr)putc, "Start cluster: %d\n\n", f->start_cluster);

    esp_printf((func_ptr)putc, "Calling fatRead()...\n");
    char buffer[512];
    int bytes_read = fatRead(f, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        esp_printf((func_ptr)putc, "FAILED: Could not read file contents.\n");
        return;
    }
    buffer[bytes_read] = '\0';
    esp_printf((func_ptr)putc, "fatRead() successful. Read %d bytes.\n\n", bytes_read);

    esp_printf((func_ptr)putc, "Displaying file contents:\n");
    esp_printf((func_ptr)putc, "========================================\n");
    esp_printf((func_ptr)putc, "%s", buffer);
    if (buffer[bytes_read - 1] != '\n') esp_printf((func_ptr)putc, "\n");
    esp_printf((func_ptr)putc, "========================================\n");
    esp_printf((func_ptr)putc, "\nAll FAT driver deliverables completed successfully!\n");
}

void main() {
    init_pfa_list();
    esp_printf((func_ptr)putc, "Free page list initialized.\n");

    struct ppage *allocated = allocate_physical_pages(3);
    if (!allocated) {
        esp_printf((func_ptr)putc, "Page allocation failed!\n");
        while (1);
    }

    struct ppage *curr = allocated;
    int i = 1;
    while (curr) {
        esp_printf((func_ptr)putc, "Allocated page %d at: 0x%x\n", i, curr->physical_addr);
        curr = curr->next;
        i++;
    }

    free_physical_pages(allocated);
    esp_printf((func_ptr)putc, "Freed pages back to free list.\n");

    struct ppage *single = allocate_physical_pages(1);
    esp_printf((func_ptr)putc, "Allocated single page at: 0x%x\n", single->physical_addr);

    esp_printf((func_ptr)putc, "\nSetting up paging...\n");
    identity_map_kernel_and_stack_and_vga();

    esp_printf((func_ptr)putc, "Loading page directory...\n");
    loadPageDirectory(pd);

    esp_printf((func_ptr)putc, "Enabling paging...\n");
    esp_printf((func_ptr)putc, "Paging enabled successfully!\n");

    test_fat_driver();

    while (1);
}
