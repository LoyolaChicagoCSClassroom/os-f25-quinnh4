#include "page.h"
#include <stdint.h>

#define NUM_PAGES 128
#define PAGE_SIZE (2 * 1024 * 1024)  

//physical page array
struct ppage physical_page_array[NUM_PAGES];

//head pointer
static struct ppage *free_list_head = 0;

void init_pfa_list(void) {
    for (int i = 0; i < NUM_PAGES; i++) {
        physical_page_array[i].physical_addr = (void *)(i * PAGE_SIZE);

        if (i > 0) {
            physical_page_array[i].prev = &physical_page_array[i-1];
        } else {
            physical_page_array[i].prev = 0;
        }

        if (i < NUM_PAGES - 1) {
            physical_page_array[i].next = &physical_page_array[i+1];
        } else {
            physical_page_array[i].next = 0;
        }
    }

    //head of free list points to first physical page
    free_list_head = &physical_page_array[0];
}

struct ppage *allocate_physical_pages(unsigned int npages) {
    if (free_list_head == 0 || npages == 0) {
        return 0; 
    }

    struct ppage *allocated_head = free_list_head;
    struct ppage *current = free_list_head;

    //moving npages forward
    for (unsigned int i = 1; i < npages && current->next != 0; i++) {
        current = current->next;
    }
    
    //updating head
    free_list_head = current->next;
    if (free_list_head) {
        free_list_head->prev = 0;
    }

    current->next = 0;

    return allocated_head;
}

void free_physical_pages(struct ppage *ppage_list) {
    if (!ppage_list) 
        return;

    //tail of list being freed
    struct ppage *tail = ppage_list;
    while (tail->next) {
        tail = tail->next;
    }

    // freed block linking to head of free list
    tail->next = free_list_head;
    if (free_list_head) {
        free_list_head->prev = tail;
    }

    free_list_head = ppage_list;
    ppage_list->prev = 0;
}

