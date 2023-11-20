#include "my_vm.h"
// Global or static bitmap array
#define TOTAL_PAGES (MEMSIZE / PGSIZE)
unsigned char phys_page_bitmap[TOTAL_PAGES / 8] = {0};
int clock_hand_index=0;
struct tlb tlb_store;
pde_t *pgdir;
int initialized = 0;

void *physical_memory;
unsigned char *physical_bitmap;
unsigned char *virtual_bitmap;
int miss_count;
int total_count;
pthread_mutex_t tlb_lock;
pthread_mutex_t pgdir_lock;
pthread_mutex_t free_lock;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    printf("setting physical memory\n");

    physical_memory = mmap(NULL, MEMSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (physical_memory == MAP_FAILED) {
        perror("mmap failed to allocate physical memory");
        exit(EXIT_FAILURE);
    }
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    unsigned long num_physical_pages = MEMSIZE / PGSIZE;
    // Initialize physical bitmap (1 bit per page)
    physical_bitmap = calloc(num_physical_pages, sizeof(unsigned char));
    if (physical_bitmap == NULL) {
        perror("Failed to allocate physical bitmap");
        exit(EXIT_FAILURE);
    }
    // Initialize virtual bitmap (1 bit per page)
    virtual_bitmap = calloc(num_physical_pages, sizeof(unsigned char));
    if (virtual_bitmap == NULL) {
        perror("Failed to allocate virtual bitmap");
        exit(EXIT_FAILURE);
    }
    // initialize page directory in physical memory
    //  TODO: set physical bitmap to valid at the location of page directory
    pgdir = get_next_avail(1);

    // zero out the page directory
    memset(pgdir, 0, PGSIZE);
    // initialize tlb
    memset(&tlb_store, 0, sizeof(tlb_store));
    total_count = 0;
    miss_count = 0;
    // printf("page directory: %x\n", pgdir);
    printf("finished setting physical memory\n");
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa)
{
    miss_count++;
    pthread_mutex_lock(&tlb_lock);
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    for(int i = 0 ; i<TLB_ENTRIES;i++){
        if(tlb_store.entries[i].valid == 0){
            tlb_store.entries[i].valid = 1;
            tlb_store.entries[i].va = (unsigned long)va & ~OFFSET_MASK;
            tlb_store.entries[i].pa = (unsigned long)pa & ~OFFSET_MASK;
            pthread_mutex_unlock(&tlb_lock);
            return 0;
        }
    }
    // run clock algorthim
    while(1){
        if(clock_hand_index == TLB_ENTRIES){
            // reset clock hand
            clock_hand_index = 0;
        }
        if(tlb_store.entries[clock_hand_index].used == 1){
            // ready to be thrown out
            tlb_store.entries[clock_hand_index].used = 0;
        }else{
            // Throw out + replace
            tlb_store.entries[clock_hand_index].valid = 1;
            tlb_store.entries[clock_hand_index].va = (unsigned long)va & ~OFFSET_MASK;
            tlb_store.entries[clock_hand_index].pa = (unsigned long)pa & ~OFFSET_MASK;
            clock_hand_index++;
            break;
        }
        clock_hand_index++;
    }
    pthread_mutex_unlock(&tlb_lock);
    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t * check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    total_count++;
    pthread_mutex_lock(&tlb_lock);
    unsigned long virtual_address = (unsigned long)va;

    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (((unsigned long)virtual_address& ~OFFSET_MASK) == tlb_store.entries[i].va && tlb_store.entries[i].valid) {
            // printf("physical address: %x\n", tlb_store.entries[i].pa);
            tlb_store.entries[i].used = 1;
            pthread_mutex_unlock(&tlb_lock);
            return (pte_t*)((uintptr_t)(tlb_store.entries[i].pa) + ((uintptr_t)va & OFFSET_MASK));
        }
    }
    pthread_mutex_unlock(&tlb_lock);

   /*This function should return a pte_t pointer (pa in tlb)*/
   // assume there won't be any pa = 0
   return NULL;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = (double)miss_count / (double)total_count;

    // call add = miss count ++
    // call check = total count ++


    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
    // get first PDE_INDEX_BITS bits of virtual address
    pte_t *pa = check_TLB(va);
    if(pa != NULL){
        return pa;
    }
    pthread_mutex_lock(&pgdir_lock);
    unsigned long index = (unsigned long)va >> (ADDRES_SIZE - PDE_INDEX_BITS);
    // get page directory entry
    pde_t *page_directory = pgdir + index;
    // printf("page directory: %x\n", page_directory);
    // printf("page directory entry: %x\n", *page_directory);
    // check if page directory entry is valid (last bit is 1)
    if (!(*page_directory & 1)) {
        // Page directory entry is not valid, so return NULL
        pthread_mutex_unlock(&pgdir_lock);
        return NULL;
    }
    // set last bit to 0 to get page table base address
    unsigned long page_table_base = *page_directory & ~1;
    // get page table
    index = (unsigned long)va << PDE_INDEX_BITS >> (ADDRES_SIZE - PTE_INDEX_BITS);
    pte_t *page_table = (pte_t *)page_table_base + index;
    // printf("page table: %x\n", page_table);
    // printf("page table entry: %x\n", *page_table);
    // check if page table entry is valid
    if (!(*page_table & 1)) {
        // Page table entry is not valid, so return NULL
        pthread_mutex_unlock(&pgdir_lock);
        return NULL;
    }
    // get physical address
    unsigned long physical_address = *page_table & ~1;
    // get offset
    unsigned long offset = (unsigned long)va << (ADDRES_SIZE - OFFSET_BITS) >> (ADDRES_SIZE - OFFSET_BITS);
    physical_address += offset;
    // printf("physical address: %x\n", physical_address);
    // add to tlb
    pthread_mutex_unlock(&pgdir_lock);
    add_TLB(va, (void *)physical_address);
    return (pte_t *)physical_address;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    // get page directory
    // if page directory entry is not valid, create a new page table
    // get page table
    // if page table entry is not valid, create a new page at pa
    unsigned long index = (unsigned long)va >> (ADDRES_SIZE - PDE_INDEX_BITS);
    // get page directory entry
    pde_t *page_directory = pgdir + index;
    // printf("page directory: %x\n", page_directory);
    // check if page directory entry is valid (last bit is 1)
    if (!(*page_directory & 1)) {
        // Page directory entry is not valid, so create a new page table
        // get next available physical address
        pte_t *page_table = get_next_avail(1);
        // zero out the page table
        memset(page_table, 0, PGSIZE);
        // set page table entry to point to page table
        *page_directory = (unsigned long)page_table | 1;
    }
    // set last bit to 0 to get page table base address
    unsigned long page_table_base = *page_directory & ~1;
    // get page table
    index = (unsigned long)va << PDE_INDEX_BITS >> (ADDRES_SIZE - PTE_INDEX_BITS);
    pte_t *page_table = (pte_t *)page_table_base + index;
    // printf("page table: %x\n", page_table);
    // check if page table entry is valid
    if (!(*page_table & 1)) {
        // Page table entry is not valid, so create a new page at pa
        // set page table entry to point to pa
        *page_table = (unsigned long)pa | 1;
        // printf("page table entry: %x\n", *page_table);
    }else{
        // Page table entry is valid, so return -1
        return -1;
    }
    return 1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
    int current_free = 0; // To count consecutive free pages
    int start_index = 0;  // To remember the starting index of free pages

    for (int i = 0; i < TOTAL_PAGES; ++i) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        // Check if the bit for the page is not set (0 means it's free)
        if (!(phys_page_bitmap[byte_index] & (1 << bit_index))) {
            //printf("find one free page at index %d\n", i);
            if (current_free == 0) {
                start_index = i;  // Remember the start of a potential free block
            }
            current_free++;
            // printf("current_free: %d num_pages: %d \n", current_free, num_pages);
            // If we've found enough pages, mark them as used and return the starting page number
            if (current_free == num_pages) {
                // Mark the pages as used in the bitmap
                for (int j = start_index; j < start_index + num_pages; ++j) {
                    int mark_byte_index = j / 8;
                    int mark_bit_index = j % 8;
                    phys_page_bitmap[mark_byte_index] |= (1 << mark_bit_index);
                }
                return (void *)((unsigned long)physical_memory + PGSIZE * start_index);
            }
        } else {
            // Reset the counter if a used page is found
            current_free = 0;
        }
    }
    
    // If we reach here, there were not enough consecutive pages free
    printf("get_next_avail failed\n");
    return NULL;
}
void *get_next_avail_virt(int num_pages) {
    int current_free = 0; // To count consecutive free pages
    int start_index = 0;  // To remember the starting index of free pages

    for (int i = 0; i < TOTAL_PAGES; ++i) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        // Check if the bit for the page is not set (0 means it's free)
        if (!(virtual_bitmap[byte_index] & (1 << bit_index))) {
            if (current_free == 0) {
                start_index = i;  // Remember the start of a potential free block
            }
            current_free++;

            // If we've found enough pages, mark them as used and return the starting page number
            if (current_free == num_pages) {
                // Mark the pages as used in the bitmap
                for (int j = start_index; j < start_index + num_pages; ++j) {
                    int mark_byte_index = j / 8;
                    int mark_bit_index = j % 8;
                    virtual_bitmap[mark_byte_index] |= (1 << mark_bit_index);
                }
                return (void *)(PGSIZE * start_index);
            }
        } else {
            // Reset the counter if a used page is found
            current_free = 0;
        }
    }
    
    // If we reach here, there were not enough consecutive pages free
    return NULL;
}



/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
    // initialize physical memory
    printf("calling t_malloc\n");
    pthread_mutex_lock(&pgdir_lock);
    if (!initialized) {
        set_physical_mem();
        initialized = 1;
    }
    // get next available virtual address
    // for page in range(num_/bytes / PGSIZE)
    // Assume that num_bytes is a multiple of PGSIZE
    int page_needed = num_bytes / PGSIZE;
    if(num_bytes % PGSIZE != 0){
        page_needed++;
    }
    void *va = get_next_avail_virt(page_needed);
    // printf("va: %x\n", va);
    // do something when no free page avaliable
    // note that when va is 0, if we compare va == NULL, it will be true
    // so we do not use va == NULL to check if no free page avaliable
    for(int i = 0; i <  page_needed; i++){
        // FIXME: need another function for get next available physic address
        void *pa = get_next_avail(1);
        // printf("map page at virtual address: %x, physical address: %x\n", va, pa);
        page_map(pgdir, (void *)((unsigned long)va+ i * PGSIZE), pa);
    }
    pthread_mutex_unlock(&pgdir_lock);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {
    pthread_mutex_lock(&pgdir_lock);
    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
    // for page in range(num_bytes / PGSIZE)
    // get page directory
    // get page table
    void* pva = va;
    int page_needed = size / PGSIZE;
    if(size % PGSIZE != 0){
        page_needed++;
    }
    for(int i = 0; i < page_needed; i++){
        // get page table entry
        // check if page table entry is valid
        // if page table entry is valid, free the page

        unsigned long index = (unsigned long)pva >> (ADDRES_SIZE - PDE_INDEX_BITS);
        // get page directory entry
        pde_t *page_directory = (pde_t *)((void *)pgdir) + index;
        // check if page directory entry is valid (last bit is 1)
        if (!(*page_directory & 1)) {
            // Page directory entry is not valid, so return NULL
            return;
        }
        // set last bit to 0 to get page table base address
        unsigned long page_table_base = *page_directory & ~1;
        // get page table
        index = (unsigned long)pva << PDE_INDEX_BITS >> (ADDRES_SIZE - PTE_INDEX_BITS);
        pte_t *page_table = (pte_t *)page_table_base + index;
        // set the last bit to 0 to free the page
        void* ppa = (void *)(*page_table & ~1);
        *page_table = *page_table & ~1;
        // TODO: write funciton for this
        // mark page as free in physical bitmap
        // mark page as free in virtual bitmap
        set_phys(((unsigned long)ppa - (unsigned long)physical_memory) / PGSIZE, 1, 0);
        set_virt((unsigned long)pva / PGSIZE, 1, 0);
        pva += PGSIZE;
    }
    pthread_mutex_unlock(&pgdir_lock);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    /*return -1 if put_value failed and 0 if put is successfull*/
    //printf("put value at virtual address: %x\n", va);
    char *value_ptr = (char *)val;
    char *virt_addr = (char *)va;
    int bytes_written = 0;

    while (size > 0) {
        // Translate the virtual address to physical address
        pte_t *pte = translate(pgdir, virt_addr);
        //printf("pte: %x\n", pte);
        if (!pte) return -1; // Translation failed

        // Calculate the offset within the page
        unsigned long offset = (unsigned long)virt_addr % PGSIZE;
        // Calculate how many bytes can be written in this page
        int bytes_in_page = PGSIZE - offset;
        int bytes_to_write = (size < bytes_in_page) ? size : bytes_in_page;

        // Copy the contents
        memcpy((void *)((char *)pte + offset), value_ptr, bytes_to_write);

        // Update pointers and counters
        size -= bytes_to_write;
        bytes_written += bytes_to_write;
        value_ptr += bytes_to_write;
        virt_addr += bytes_to_write;
    }

    return (bytes_written > 0) ? 0 : -1;

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

   char *value_ptr = (char *)val;
    char *virt_addr = (char *)va;

    while (size > 0) {
        // Translate the virtual address to physical address
        pte_t *pte = translate(pgdir /* page directory */, virt_addr);
        if (!pte) {
            // Handle the translation failure (e.g., by throwing an error or returning)
            // For now, we'll assume the translation cannot fail.
        }

        // Calculate the offset within the page
        unsigned long offset = (unsigned long)virt_addr % PGSIZE;
        // Calculate how many bytes can be read from this page
        int bytes_in_page = PGSIZE - offset;
        int bytes_to_read = (size < bytes_in_page) ? size : bytes_in_page;

        // Copy the contents
        memcpy(value_ptr, (void *)((char *)pte + offset), bytes_to_read);

        // Update pointers and counters
        size -= bytes_to_read;
        value_ptr += bytes_to_read;
        virt_addr += bytes_to_read;
    }

}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}



void set_virt(int index, int size, int value) {
    int start_index = (int)index;
    for (int i = 0; i < size; i++) {
        int byte_index = (start_index + i) / 8;
        int bit_index = (start_index + i) % 8;
        if (value) {
            virtual_bitmap[byte_index] |= (1 << bit_index);
        } else {
            virtual_bitmap[byte_index] &= ~(1 << bit_index);
        }
    }
}

void set_phys(int index, int size, int value) {
    int start_index = (int)index;
    // printf("start_index: %d\n", start_index);
    // printf("phys_page_bitmap value binary: %x\n", phys_page_bitmap[start_index]);
    for (int i = 0; i < size; i++) {
        int byte_index = (start_index + i) / 8;
        int bit_index = (start_index + i) % 8;
        if (value) {
            virtual_bitmap[byte_index] |= (1 << bit_index);
        } else {
            virtual_bitmap[byte_index] &= ~(1 << bit_index);
        }
    }
    // printf("phys_page_bitmap value binary: %x\n", phys_page_bitmap[start_index]);
}
