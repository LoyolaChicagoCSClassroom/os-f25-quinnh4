#include "paging.h"
#include "rprintf.h"

#define PAGE_SIZE 4096

// Must be global and 4KB-aligned
struct page_directory_entry pd[1024] __attribute__((aligned(4096)));
struct page pt[1024] __attribute__((aligned(4096)));

void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd) {
    uint32_t vaddr_u32 = (uint32_t)vaddr;
    uint32_t dir_index = vaddr_u32 >> 22;                // top 10 bits
    uint32_t table_index = (vaddr_u32 >> 12) & 0x3FF;    // next 10 bits

    // Setup PDE if not present
    pd[dir_index].present = 1;
    pd[dir_index].rw = 1;
    pd[dir_index].user = 0;
    pd[dir_index].frame = ((uint32_t)pt) >> 12; // physical address of page table

    struct ppage *current = pglist;
    while (current) {
        pt[table_index].present = 1;
        pt[table_index].rw = 1;
        pt[table_index].user = 0;
        pt[table_index].frame = ((uint32_t)current->physical_addr) >> 12;

        table_index++;
        if (table_index >= 1024)
            break;

        current = current->next;
    }

    return vaddr;
}

void loadPageDirectory(struct page_directory_entry *pd) {
    asm volatile("mov %0, %%cr3" :: "r"(pd));
}

void enable_paging(void) {
    asm volatile(
        "mov %cr0, %eax\n"
        "or $0x80000001, %eax\n"
        "mov %eax, %cr0"
    );
}

