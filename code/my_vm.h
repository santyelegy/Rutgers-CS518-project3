#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

// Assume the address space is 32 bits, so the max memory size is 4GB
// Page size is 4KB

// Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL * 1024 * 1024 * 1024

// Size of "physcial memory"
#define MEMSIZE 1024 * 1024 * 1024

#define ADDRES_SIZE 32

#define OFFSET_BITS ((int)log2(PGSIZE))

// fill inner page first to save memory
#define PDE_INDEX_BITS ((int)log2(PGSIZE / 4))

#define PTE_INDEX_BITS ADDRES_SIZE - OFFSET_BITS - PDE_INDEX_BITS

// in case that we need these two value at compile time to determine the size of struct
// we are not going to calculate it using index_bits macro
#define PDE_NUM 1024

#define PTE_NUM 1024

// Represents a page table entry
// 4 byte, 32 bits, store the physical address
// offset is 12 bits, so we only need 20 bits to store the physical address, use the last bit to store the valid bit
typedef unsigned long pte_t;

// Represents a page directory entry
// 4 byte, 32 bits, store the starting address of page table
// offset is 12 bits, so we only need 20 bits to store the starting address, use the last bit to store the valid bit
typedef unsigned long pde_t;

typedef struct
{
    pte_t entries[PTE_NUM];
} page_table;

typedef struct
{
    pde_t entries[PDE_NUM];
} page_directory;

#define TLB_ENTRIES 512

// Structure to represents TLB
typedef struct
{
    unsigned long vpn;   // Virtual Page Number (VPN)
    unsigned long pfn;   // Physical Frame Number (PFN)
    unsigned char valid; // Valid bit
    unsigned char dirty; // Dirty bit to indicate if the page has been written to
} tlb_entry;
struct tlb
{
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
     * Think about the size of each TLB entry that performs virtual to physical
     * address translation.
     */
    tlb_entry entries[TLB_ENTRIES];
};
struct tlb tlb_store;
struct page_directory *pgdir;
int initialized = 0;

void *physical_memory;
unsigned char *physical_bitmap;
unsigned char *virtual_bitmap;

void set_physical_mem();
pte_t *translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void *pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();

#endif
