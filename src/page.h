#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

struct ppage {
   struct ppage *next;
   struct ppage *prev;
   void *physical_addr;
};

//initializing first free page list
void init_pfa_list(void);


//allocating npages from free list
struct ppage *allocate_physical_pages(unsigned int npages);

//list of pages freed, back into free list
void free_physical_pages(struct ppage *ppage_list);

#endif
