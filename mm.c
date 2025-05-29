/*
 * mm.c
 * 
 * Block Format
 * 
 *   Header and Footer format. (8 bytes)  
 *    
 *          63                         3      2      1      0
 *          ----------------------------------------------------
 *          |    PAYLOAD SIZE       |  0      0     pa/pf  a/f |
 *          ----------------------------------------------------
 *          
 *             Bit 0
 *          a = 0 = allocated
 *          f = 1 = free
 *             
 *             Bit 1
 *          pa = 0 = prev block is allocated
 *          pf = 1 = prev block is free
 * 
 *          Bit 3 and Bit 2 are not be used. 
 * 
 *          Bit 4 to bit 63 tell the size of the payload part of the block.
 * 
 *  
 *  Block Format 
 *           [HEADER] [PAYLOAD] [FOOTER]
 *  
 *  Minimum block size: 32
 * 
 *  The payload part must store two pointers for doubly linked list when a block is free. 
 *  Thus the minimum payload size is 16 bytes, and minimum block size is 8 + 16 + 8 = 32.
 *                 
 *  
 * Alignment. The payload needs to always be aligned at 16 bytes. How to achieve this? 
 * First, the payload size will always be a multiple of 16, and each payload is separated
 * by a footer and a header (16 bytes).
 * 
 * However, the first block has no previous block and only has a header.
 * This will cause an alignment issue. To fix this, padding of 8 bytes is added at the start of the heap.
 * The starting address of the heap should also be aligned to 16 bytes.
 * 
 * The following diagram illustrates what the starting address of a block part is aligned to. 
 * Notice that the payload is always aligned to 16 in this configuration.
 * 
 *                                                                                                     |
 *                                                                                                     |<---- current sbrk break
 *                                                                                                     |
 *  Aligned to       16               8            16          8           16       16        8        16 
 *   (bytes)         -----------------------------------------------------------------------------------
 *                   | 8 Byte padding |  Header    |  Footer   |   Header  | PAYLOAD | FOOTER | HEADER |
 *                   -----------------------------------------------------------------------------------
 *                   ^                             ^                                                   |
 *                   |                             |                                                   |
 *                   |                             ----- Zero size payload. Zero is a multiple of 16.  |
 *                   |                                                                                 |
 *                   |
 *                   -----Starting address of the heap.
 * 
 * 
 * 
 * Prologue and Epilogue blocks
 * The first block after the padding is the prologue block with a payload size of 0.
 * The last block is the epilogue block containing only the header and a zero-size payload.
 * Both of these blocks are allocated, so we can ignore them in coalescing.
 * The user blocks are placed between these special blocks.
 * 
 * 
 * 
 * 
 * Creating a new block.
 * Imagine there are no free blocks available in any freelist. How do we fulfill the malloc request?
 * We extend the heap just enough to accommodate a new block. Note that the current break (sbrk) will
 * already be at the starting address of the payload of the new block.
 * To ensure proper alignment, the break needs to be incremented by a multiple of 16 for the payload size,
 * then incremented for a footer, and finally, incremented for a header.
 * The header of the previous epilogue block will become the header of the new block,
 * and the last header will become the new epilogue block.
 * 
 * Let's define "incr" as the number of bytes to increase the heap by. This will be:
 * incr = size of payload + size of footer + size of header.
 * 
 * The following diagram illustrates how the heap looks like after allocating a space for a new block.
 * 
 *                                                                                       old sbrk ---->|                           | <- new sbrk
 *                                                                                                     |                           |
 *  Aligned to       16               8            16          8           16       16        8        16       16        8       16
 *   (bytes)         ---------------------------------------------------------------------------------------------------------------
 *                   | 8 Byte padding |  Header    |  Footer   |   Header  | PAYLOAD | FOOTER | HEADER | PAYLOAD | FOOTER | HEADER |
 *                   ---------------------------------------------------------------------------------------------------------------
 *                   ^                                                                            |    |                           |
 *                   |                                                                            |    |--------<incr bytes>-------|
 *                   |                                                           old epilogue <---|    |                           |
 *                   |                                                                block            |                           |
 *                   |                                                                                 |                           |
 *                   |
 *                   -----Starting address of the heap.
 * 
 * Segregated Freelist
 * In the segregated freelist structure, 
 * each free block is stored in one of the 15 available freelist categories, each representing a specific size class.
 * 
 * +-------------+------------+
 * | Freelist ID | size class |
 * +-------------+------------+
 * | 0           |         16 |
 * | 1           |         32 |
 * | 2           |         48 |
 * | 3           |         64 |
 * | 4           |         80 |
 * | 5           |         96 |
 * | 6           |        112 |
 * | 7           |        128 |
 * | 8           |        144 |
 * | 9           |        160 |
 * | 10          |        176 |
 * | 11          |        192 |
 * | 12          |        208 |
 * | 13          |        224 |
 * | 14          |        0   |
 * +-------------+------------+
 * 
 * 
 * The size class of the last freelist is 0, designated for blocks of sizes not stored in other freelist categories.
 * Blocks are inserted into the freelist in Last-In-First-Out (LIFO) order. 
 * The payload size of the block determines which freelist it's assigned to.
 * If the payload size falls outside the range of 16-224, it's inserted into the last freelist.
 * 
 * 
 * Finding a free block ? 
 * Notice that the availble space in a block will be equal to size of payload + 8. The extra 
 * 8 bytes is due to footer optimization. To find a free block We will search freelist 0 to freelist 13. If the required size falls
 * within the available space, we return the first block in the freelist. 
 * If the required size exceeds the available space in these categories, we perform a first-fit search within freelist 14.
 * 
 * 
 * Allocating a block and spliting a block.
 * Once a free block is located, we proceed to allocate it and potentially split it if the payload size is large enough.
 * Notice that due to footer optimization, 8 bytes from the required size can be subtracted and the remanining size
 * should be statified by the payload. The extra space can be used to create a new block. 
 * 
 * 
 * payload_size = size of free block's payload
 * required_payload_size = max(align(size - 8), MIN_PAYLOAD_SIZE)
 * extra_space = payload_size - required_payload_size
 * if extra_space >= MIN_BLOCK_SIZE:
 *     Create a new block by spliting.
 * 
 * 
 * >> Note: required_payload_size = max(align(size - 8), MIN_PAYLOAD_SIZE)
 *    We cannot have zero size payloads. Payload must be atleast of size 16 bytes. 
 *    so it can store the two linked list pointers. 
 * 
 * 
 * Freeing and Coalescing Blocks.
 * When freeing a block, we mark it as free by setting bit 0 of the header.
 * Additionally, we set bit 1 of the next block's header.
 * Notice that due to footer optimization the footer space might 
 * have been utilized, potentially corrupting it. It is important to copy the header value to the footer.
 * Subsequently, we perform coalescing first with the right block and then with the left block.
 * 
 * 
 * Realloc. 
 * When reallocating memory, if the available space in the block is greater than or equal to the required size,
 * we don't shrink the block; it's returned as it is.
 * However, if the required size is larger, we allocate a new block by calling malloc,
 * copy the data from the old payload to the new payload, and then free the old block.
 * Finally, the payload of the new block is returned.
 * 
 * 
 * Footer optimization:
 * Due to footer optimization, we gain an extra 8 bytes to utilize. These bytes are not recorded in the header.
 * The header only records the payload size. The allocation functions all adjust the size parameter such that 8 bytes are satisfied by the 
 * footer, and the remaining bytes are satisfied by the payload.
 * 
 * Lets see a table of amount of space required for the payload to be to satisfy a malloc request. 
 * The second column is without footer optimization and the third column is with footer optimization.
 * 
 * 
 *  +--------------+--------------------------+-----------------------------------+
 *  | malloc(size) |     Payload size         |          Payload size             |
 *  |              |    size = align(size)    |     size = align(size - 8)        |
 *  +--------------+--------------------------+-----------------------------------+
 *  |            8 |                       16 |                                16 |
 *  |           16 |                       16 |                                16 |
 *  |           24 |                       32 |                                16 |
 *  |           32 |                       32 |                                32 |
 *  |           40 |                       48 |                                32 |
 *  +--------------+--------------------------+-----------------------------------+
 * 
 * 
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
// When debugging is enabled, the underlying functions get called
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
// When debugging is disabled, no code gets generated
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

// do not change the following!
#ifdef DRIVER
// create aliases for driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define ALIGNMENT 16

typedef struct DList_Type{
    struct DList_Type *prev;
    struct DList_Type *next;
} DList;


// Implicit freelist.
void *heap = NULL;

/**
 * There will be a total of 15 explicit freelists, each with a size class that is a multiple of 16.
 * The last freelist will store blocks of any size, but it must not be the same size class as other freelists.
 * 
 * Why 15 freelists ?
 * 
 * We have a total of 128 bytes available for global storage.. A pointer to the above heap takes away 8 bytes.
 * Remaining 120 bytes are left.
 * 
 * The size of a pointer is 8 bytes.
 * Therefore, the total number of freelists is 120 / 8 = 15.
 */
DList *freelists[15];


// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x) {
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p) {
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p) {
    return p <= mm_heap_hi() && p >= mm_heap_lo();
}


/**
 * As alignment is 16, the last 4 bits will be 0.
 * we can use one of the bit for allocated/free flag.
 * 
*/


///////////////////////////////////////////////////////////////////////////////
/**
 * These functions manage block metadata, its size and the two flags.
 * */
///////////////////////////////////////////////////////////////////////////////
static size_t   GET_SIZE(void *header_or_footer);
static bool     GET_FREE_BIT(void *header_or_footer);
static bool     GET_PREV_BLOCK_FREE_BIT(void *header_or_footer);

static void     SET_VALUE(void *header_or_footer, size_t payload_size, bool is_free, bool prev_block_free);
static void     SET_FREE_BIT(void *header_or_footer, bool is_free);
static void     SET_PREV_BLOCK_FREE_BIT(void *header_or_footer, bool prev_flag_free_bit);

/**
 * Retrieves the size of the block.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @return The size of the block.
 */
static size_t GET_SIZE(void *header_or_footer) {
    dbg_assert(in_heap(header_or_footer));
    size_t *ptr = (size_t*) header_or_footer;
    size_t mask = 3;
    mask = ~mask;
    return (*ptr) & mask;
}

/**
 * Retrieves the value of bit 0 from the header or footer.
 * This bit indicates whether the block is free or not.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @return The value of bit 0.
 */
static bool GET_FREE_BIT(void *header_or_footer) {
    dbg_assert(in_heap(header_or_footer));
    size_t *ptr = (size_t*) header_or_footer;
    size_t mask = 1;
    return (bool) ((*ptr) & mask);
}

/**
 * Retrieves the value of bit 1 from the header or footer.
 * This bit indicates whether the previous block is free or not.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @return The value of bit 1.
 */
static bool GET_PREV_BLOCK_FREE_BIT(void *header_or_footer) {
    size_t *ptr = (size_t*)header_or_footer;
    size_t value = *ptr;
    return (bool) (value & (1 << 1));
}

/**
 * Sets the value of the header or footer.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @param payload_size The size of the payload part of the block.
 * @param is_free Indicates whether the block is free or not.
 * @param prev_block_free Indicates whether the previous block is free or not.
 */
static void SET_VALUE(void *header_or_footer, size_t payload_size, bool is_free, bool prev_block_free) {

    dbg_assert(aligned((void*)payload_size));
    dbg_assert(in_heap(header_or_footer));

    size_t *ptr = (size_t*) header_or_footer;
    size_t flags = prev_block_free?2: 0;
    flags |= is_free?1: 0;
    (*ptr) = payload_size | flags;
}

/**
 * Sets the value of bit 0 in the header or footer.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @param is_free Value to set for the free bit (bit 0).
 */
static void SET_FREE_BIT(void *header_or_footer, bool is_free) {
    size_t payload_size = GET_SIZE(header_or_footer);
    bool prev_flag_free_bit = GET_PREV_BLOCK_FREE_BIT(header_or_footer);
    SET_VALUE(header_or_footer, payload_size, is_free, prev_flag_free_bit);
}

/**
 * Sets the value of bit 1 in the header or footer.
 * @param header_or_footer Pointer to the header or footer of the block.
 * @param prev_flag_free_bit Value to set for the bit indicating whether the previous block is free or not.
 */
static void SET_PREV_BLOCK_FREE_BIT(void *header_or_footer, bool prev_flag_free_bit) {
    size_t payload_size = GET_SIZE(header_or_footer);
    bool is_free = GET_FREE_BIT(header_or_footer);
    SET_VALUE(header_or_footer, payload_size, is_free, prev_flag_free_bit);
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions to work with Header, Payload and Footer.
///////////////////////////////////////////////////////////////////////////////
static size_t   Header_size();
static size_t   Footer_size();

static void*    Header_get_prev_header(void *header);
static void*    Header_get_payload(void *header);
static void*    Header_get_footer(void *header);
static void*    Header_get_next_header(void *header);
static DList*   Header_get_list(void *header);

static void *   Payload_get_header(void *payload);
static size_t   Payload_get_size(void *payload);
static void*    Payload_get_footer(void *payload);
static size_t   Payload_min_size();
static void*    Footer_get_header(void* footer);
static size_t   Block_min_size();


static size_t Header_size() {
    return 8;
}

static size_t Footer_size() {
    return 8;
}

/**
 * Returns the previous header from the given header.
 * @param header The header whose previous header is to be returned.
 * @return A pointer to the previous header.
 */
static void* Header_get_prev_header(void *header) {
    void *prev_footer = (char*)header - Footer_size();
    return Footer_get_header(prev_footer);
}

/**
 * Returns the payload part of the block.
 * @param header The header of the block.
 * @return A pointer to the payload part of the block.
 */
static void * Header_get_payload(void *header) {
    return (void*)((char*)header + Header_size());
}

/**
 * Returns the footer part of the block.
 * @param header The header part of the block.
*/
static void * Header_get_footer(void *header) {
    return Payload_get_footer(Header_get_payload(header));
}

/**
 * Returns the header of the next block.
 * @param header The header of the current block.
 * @return A pointer to the header of the next block.
 */
static void * Header_get_next_header(void *header) {
    void *footer = Header_get_footer(header);
    return (void*) ((char*)footer + Footer_size());
}


/**
 * Returns the doubly linked list stored in the payload part of the block.
 * @param header The header of the block containing the list.
 * @return A pointer to the doubly linked list.
 */
static DList* Header_get_list(void *header) {
    dbg_assert(in_heap(header));
    dbg_assert(GET_SIZE(header) != 0);
    void *payload = Header_get_payload(header);
    return (DList*)payload;
}

/**
 * Returns the header of the block containing the given payload.
 * @param payload The payload part of the block.
 * @return A pointer to the header of the block.
 */
static void * Payload_get_header(void *payload) {
    return (void*) ((char*)payload - Header_size());
}

/**
 * Returns the size of the payload part of the block.
 * @param payload The payload part of the block.
 * @return The size of the payload.
 */
static size_t Payload_get_size(void *payload) {
    void *header = Payload_get_header(payload);
    size_t payload_size = GET_SIZE(header);
    return payload_size;
}

/**
 * Returns the footer of the block containing the given payload.
 * @param payload The payload part of the block.
 * @return A pointer to the footer of the block.
 */
static void * Payload_get_footer(void *payload) {
    size_t payload_size = Payload_get_size(payload);
    return (void*) ((char*)payload + payload_size);
}

/**
 * Returns the minimum size required for the payload part to be.
 * The payload must be able to store the linked list.
 * @return The minimum size for the payload. 
*/
static size_t Payload_min_size() {
    return align(sizeof(DList));
}

/**
 * Retrieves the header corresponding to the given footer.
 * @param footer The footer of the block.
 * @return A pointer to the header of the block.
 */
static void* Footer_get_header(void* footer) {
    size_t payload_size = GET_SIZE(footer);
    void *payload = (char*)footer - payload_size;
    return Payload_get_header(payload);
}

/**
 * Returns the minimum size required for a block.
 * A block consists of a header, payload, and footer.
 * The minimum size of the block must be aligned to 16 bytes.
 * @return The minimum size for a block.
 */
static size_t Block_min_size() {
    return Header_size() + (Payload_min_size()) + Footer_size();
}

////////////////////////////////////////////////////////////////////////////////
// Functions for linked list manipulation.
////////////////////////////////////////////////////////////////////////////////

void    LinkedList_insert_at_front(DList **head, DList *list);
void    LinkedList_remove(DList **head, DList *list);

/**
 * Inserts the given list at the front of the list.
 * @param head Pointer to the head of the list.
 * @param list Pointer to the list to be inserted.
 */
void LinkedList_insert_at_front(DList **head, DList *list) {
    dbg_assert(in_heap(list));
    if (*head) {
        (*head)->prev = list;
    }
    list->prev = NULL;
    list->next = *head;
    *head = list;
}

/**
 * Removes the list from the doubly linked list.
 * @param head Pointer to the head of the list.
 * @param list Pointer to the list to be removed.
 */
void LinkedList_remove(DList **head, DList *list) {
    /**
     * we have to remove list.
     * 
     * [prev_list] [list] [next_list]
     * 
    */
   dbg_assert(in_heap(list));
   if (list->next) {
    list->next->prev = list->prev;
   }

   if (list->prev) {
    list->prev->next = list->next;
   }
   else {
    *head = list->next;
   }
}

////////////////////////////////////////////////////////////////////////////////
// Functions for managing the explicit freelists.
////////////////////////////////////////////////////////////////////////////////

void*   find_free_block_explicit_list_firstfit(DList* freelist, size_t size);
void    Segregated_get_size_classes(size_t sizes[15]);
int     Segregated_get_bucket(size_t size);
bool    Segregated_bucket_header_exists(int bucket, void *target_header);
bool    Segregated_header_exists(void *header);
void    Segregated_insert_header(void *header);
void    Segregated_remove_header(void *header);
void*   Segregated_find_free_block(size_t size);

/**
 * Finds a free block in the given freelist using first fit policy.
*/
void *find_free_block_explicit_list_firstfit(DList* freelist, size_t size) {

    DList* list = freelist;
    while (list) {
        void *payload = (void*)list;
        void *header = Payload_get_header(payload);
        if (GET_SIZE(header) + Footer_size() >= size) {
            return header;
        }
        list = list->next;
    }
    return NULL;
}

/**
 * Retrieves the size class of each bucket and stores it in the provided parameter.
 * 
 * @param sizes The array where bucket sizes will be stored. 
 */
void Segregated_get_size_classes(size_t sizes[15]) {
    sizes[0] = 16;
    sizes[1] = 32;
    sizes[2] = 48;
    sizes[3] = 64;
    sizes[4] = 80;
    sizes[5] = 96;
    sizes[6] = 112;
    sizes[7] = 128;
    sizes[8] = 144;
    sizes[9] = 160;
    sizes[10] = 176;
    sizes[11] = 192;
    sizes[12] = 208;
    sizes[13] = 224;
    sizes[14] = 0;      // free size bucket.
    return;
}

/**
 * Gets the bucket index for a given block size.
 * @param size - The size of the block.
 * @return The bucket index corresponding to the given size.
 * If no exact match is found, returns the index of the last bucket.
*/
int Segregated_get_bucket(size_t size) {
    size_t sizes[15];
    Segregated_get_size_classes(sizes);
    for (int i = 0; i < 15; i++) {
        if (sizes[i] == size)
            return i;
    }
    return 14; // Not found. Last bucket is used for other sizes.
}


/**
 * Checks if the header exists in the given bucket.
 * 
 * @param target_header The header to search.
 * @return True if the header exists, false otherwise.
 */
bool Segregated_bucket_header_exists(int bucket, void *target_header) {
    DList *node = freelists[bucket];
    while (node) {
        void *payload = (void*)node;
        void *header = Payload_get_header(payload);
        if (header == target_header) {
            return true;
        }
        node = node->next;
    }
    return false;
}

/**
 * Checks if the header exists in any explicit freelist.
 * 
 * @param target_header The header to search.
 * @return True if the header exists in any explicit freelist, false otherwise.
 */
bool Segregated_header_exists(void *header) {
    for (int bucket = 0; bucket < 15; bucket++) {
        if (Segregated_bucket_header_exists(bucket, header)) {
            return true;
        }
    }
    return false;
}

/**
 * Inserts the block, indicated by the header, into the correct bucket.
 * The block is inserted in LIFO order, at the front of the list.
 * @param header The header of the block to be inserted.
 */
void Segregated_insert_header(void *header) {
    size_t size = GET_SIZE(header);
    size_t bucket = Segregated_get_bucket(size);
    DList **head = &freelists[bucket];
    DList *list = Header_get_list(header);
    LinkedList_insert_at_front(head, list);
}

/**
 * Removes the block indicated by the header from the segregated freelist. 
 * The block must already be present in the correct bucket based on the block's size class. 
 * @param header The header of the block to be removed.
 */
void Segregated_remove_header(void *header) {
    size_t size = GET_SIZE(header);
    size_t bucket = Segregated_get_bucket(size);
    DList **head = &freelists[bucket];
    DList *list = Header_get_list(header);
    LinkedList_remove(head, list);
}

/**
 * Searches segregated freelist to find a free block which can hold atlease size bytes in its available space.
 * 
 * 
*/
void *Segregated_find_free_block(size_t size) {

    // Adjust size, such that 8 bytes are satified by the footer.    
    size_t search_size = align(size - 8);
    search_size = search_size>=Payload_min_size()?search_size: Payload_min_size();

    for (int i = Segregated_get_bucket(search_size); i < 14; i++) {
        // Return the free block using LIFO.
        DList *head = freelists[i];
        if (head) {
            void *payload = (void*)head;
            void *header = Payload_get_header(payload);
            return header;
        }
    }

    const int others_index = 14;
    return find_free_block_explicit_list_firstfit(freelists[others_index], size);
}


//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

/**
 * Coalesce the block with the previous block.
 * If the given block is not free or if the previous block is not free, then do nothing.
 * Note: If either block is free, it must exist in the freelist.
 * @param header The header of the block.
 * 
*/
void coalesce_left(void *header) {

    // The given block is not free. Do nothing.
    if (GET_FREE_BIT(header) == false) {
        return;
    }

    // Previous block is not free. Do nothing.
    if (GET_PREV_BLOCK_FREE_BIT(header) == false) {
        return;
    }

    /**
     * At this point, both blocks are free and exist in the freelist.
     * Coalescing Steps:
     * 1. Remove both the blocks from the freelist.
     * 2. Create a new block by combining them.
     * 3. Finally insert the new block into the freelist. 
     */
    void *prev_header = Header_get_prev_header(header);

    // Remove both the blocks from the freelist.
    Segregated_remove_header(header);
    Segregated_remove_header(prev_header);

    /**
     * Combine the blocks to create a new block.
     * 
     *   Previous block         current block
     * [HDR1][PAYLOAD1][FTR1]  [HDR2][PAYLOAD2][FTR2]
     * 
     * After combining, previous block looks like this
     * 
     *        [HDR1] [PAYLOAD1 = PAYLOAD1 + HDR2 + PAYLOAD2 + FTR2] [FTR1]
     * 
    */

    // size of the current block
    size_t size2 = Header_size() + GET_SIZE(header) + Footer_size();  

    // new size of the payload of previous block
    size_t size1 = GET_SIZE(prev_header) + size2;

    bool prev_prev_free_block_bit = GET_PREV_BLOCK_FREE_BIT(prev_header);

    // Update the header and footer
    SET_VALUE(prev_header, size1, true, prev_prev_free_block_bit);

    void *prev_footer = Header_get_footer(prev_header);
    SET_VALUE(prev_footer, size1, true, prev_prev_free_block_bit);

    // Insert the new block into the freelist.
    Segregated_insert_header(prev_header);    
}


/**
 * coalesce the block with its left and right neibhnouring blocks.
 * Note: If the block is free, then it must exist in the freelist 
 * 
*/
void coalesce(void *header) {
    // Coalesce current block and the next block.
    void *next_header = Header_get_next_header(header);
    coalesce_left(next_header);

    // Coalesce current block and previous block.
    coalesce_left(header);
}
 


/**
 * Extend the heap.
 * Basically adds a new block which can support atleast size bytes.
 * @param size 
*/
bool extend_memory(size_t size) {
    /**
     * 
     * Our implicit list currently looks like this: 
     *          
     *        Prologue Block                         Epilogue Block  
     * 
     *                                                       |<----- Sbrk Break is after this header. 
     *                                                       |
     *         [HDR][FTR] .........users blocks........ [HDR]|
     *                                                       |
     *                                                       |
     * 
     * We need to add a new block to this list.
     * Notice that the last epilogue block only consists of a header. This epilogue header will become the header of the new
     * block. We will add a footer and a another header. This header will now become the epilogue header.
     * 
     * 
     * We will increase the sbrk by payload size + footer + header. Now the implicit freelist will look like
     * this. 
     * 
     *        Prologue Block                                            Epilogue Block  
     * 
     *                                                       |                  |
     *                               Old sbrk break  ------->|                  | <----- New sbrk break
     *                                                       |                  |
     *         [HDR][FTR] .........users blocks........ [HDR][PAYLOAD][FTR][HDR]|
     *                                                       |                  |
     *                                                       |                  |
     *                                                       |   <---incr--->   |
     * 
    */

    // Find the payload size of the new block.
    // Footer Optimization: Adjust the size parameter such that 8 bytes will be satisfied by the footer.
    size = align(size - 8);
    size = size>Payload_min_size()?size: Payload_min_size();

    // Create space for new block aswell as new epilogue block
    size_t incr = size + Footer_size() + Header_size();
    void* payload = mm_sbrk(incr);
    if (payload == (void *)-1) {
        return false;
    }

    // Update the new blocks header and footer.
    void* header = Payload_get_header(payload);
    bool prev_block_free = GET_PREV_BLOCK_FREE_BIT(header);
    SET_VALUE(header, size, true, prev_block_free);
    void* footer = Header_get_footer(header);
    SET_VALUE(footer, size, true, prev_block_free);

    // Update the epilogue block  
    // The added new block is free. so the epilogue block previous block free bit should be true.
    void* next_header = Header_get_next_header(header);
    SET_VALUE(next_header, 0, false, true);

    // Insert the new block into the freelist.
    dbg_assert(Segregated_header_exists(header) == false);
    Segregated_insert_header(header);

    // Extend memory is called when a malloc allocation fails. So there might be
    // blocks which are free but do not satisfy the size requirments. The blocks 
    // before the added new blocks might be free. So coalesce with them. 
    coalesce(header);
    
    return true;
}


/**
 * Mark the block as allocated, as well as break the block if breakable.
 * 
 * @param
*/
void* allocate(void* header, size_t size) {

    /**
     * Here we can assume that this block is capable of holding size bytes. 
     * 
     * The passed in size will be aligned by 8 bytes. 
     * With footer optimization, the footer is usable for allocation.
     * So we can reduce 8 bytes from the size. 
     * If the size is aligned to 8, we can subtract 8 from it. The resulting
     * size is aligned to 16 bytes for which we can allocate a payload of that size. The subtracted 
     * 8 bytes will be satisfied by the footer.
     * 
     * If the size is aligned to 16 bytes, we will just create a block with that payload size. In this
     * case the footer space will be not used. 
     * 
     * The allocated block will be a header + payload + footer.
     * Total required size is 8 + align(size - 8) + footer.
     * The remaning size will be used to create a new block. The new block 
     * should be atleast (Header = 8) + (payload = 16) + (footer = 8) = 32. If the remaining size
     * is atleast 32, we will break it to create a new block.
     *  
     * */ 
    
    //
    size = align(size - 8);
    size = size>Payload_min_size()?size: Payload_min_size();

    if (GET_SIZE(header) - size >= Block_min_size()) {
        // The block size wille be changed. Remove from the freelist.
        Segregated_remove_header(header);

        size_t old_size = GET_SIZE(header);

        // Resize the block and add to a new freelist.
        size_t new_size = size;
        SET_VALUE(header, new_size, true, false);
        void *footer = Header_get_footer(header);
        SET_VALUE(footer, new_size, true, false);
        Segregated_insert_header(header);
        
        // Create a new block with the remanining space and insert into the freelist.
        size_t free_space = old_size - new_size;
        size_t new_payload_size = free_space - (Header_size() + Footer_size());
        void *new_header = Header_get_next_header(header);
        SET_VALUE(new_header, new_payload_size, true, false); 
        void *new_footer = Header_get_footer(new_header);
        SET_VALUE(new_footer, new_payload_size, true, false);
        Segregated_insert_header(new_header);
    }

    // Remove from the freelist.
    Segregated_remove_header(header);

    // Clear the free bit of this block
    // Clear the next block 'prev block free bit'
    void *next_header = Header_get_next_header(header);
    SET_FREE_BIT(header, false);
    SET_PREV_BLOCK_FREE_BIT(next_header, false);
    void *footer = Header_get_footer(header);
    SET_FREE_BIT(footer, false);


    return Header_get_payload(header);
}



/**
 * Copy the contents of the header into footer.
 * 
*/
void mirror_header_to_footer(void *header) {
    void *footer = Header_get_footer(header);
    size_t size = GET_SIZE(header);
    bool is_free = GET_FREE_BIT(header);
    bool is_prev_free = GET_PREV_BLOCK_FREE_BIT(header);
    SET_VALUE(footer, size, is_free, is_prev_free);
}


/////////////////////////////////////////////////////////////////////////////////////
// Allocation functions
/////////////////////////////////////////////////////////////////////////////////////
/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    /**
     * Initially we need to allocate freelist. 
     * It includes a Header + PAYLOAD + footer + header. 
     * The last header marks the end.
     * 
     * We need to make sure the first call to mm_sbrk gives us a 16byte alignment memory.
     * 
    */

   // I am not sure if the starting address returned by mm_sbrk is aligned. So we will 
   // find the starting address to use from mm_sbrk which is 16-byte aligned.
   void *start = NULL;
   do {
    start = mm_sbrk(1);
    if (start == (void *) -1) {
        return false;
    }
   }
   while (!aligned(start));

   // Initial memory to allocate. 
   // We need a PADDING + HEADER + (O SIZE PAYLOAD) + (FOOTER) + (HEADER -> For next heap increments) and -1 for above increment.
   // As the starting address is 16 bytes aligned, we need a padding of 8 bytes before the first header. So the first payload 
   // is 16 byte aligned.
   /**
    * 
    *    [PADDING] [HEADER] [FOOTER] [HEADER]
    *    
    *    The first header and footer has the payload size as 0. It marks the beginning of the list. 
    *    The last header also has the payload size of 0. It marks the end of the list. 
    * 
    *    All new blocks will be in between.
    *    
   */
   size_t size = 8 + Header_size() + Footer_size() + Header_size() - 1;

   if (mm_sbrk(size) == (void*)-1) {
    return false;
   }

   heap = (char*)start + 8;
   for (int i = 0; i < 15; i++) {
    freelists[i] = NULL;
   }
   
   void *header = heap;
   SET_VALUE(header, 0, false, false);

   void *footer = Header_get_footer(header);
   SET_VALUE(footer, 0, false, false);

   void *next_header = Header_get_next_header(header);
   SET_VALUE(next_header, 0, false, false);

   return true;
}


/*
 * malloc
 */
void* malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    // Find a free block
    void *header = Segregated_find_free_block(size);

    // No free block found. Extend the heap.
    if (!header) {
        bool extended = extend_memory(size);
        if (extended) {
            return malloc(size);
        }
        return NULL;
    }

    // Free block is found, allocate it.
    void *result = allocate(header, size);

    mm_checkheap(1057);
    return result;
}



/*
 * free
 */
void free(void* ptr)
{
    // Get the block header
    void *payload = ptr;
    void *header = Payload_get_header(payload);

    // Mark the block as free.
    // Due to footer optimization, the footer was probably being used hence
    // being corrupted. So We will copy the header to the footer.
    SET_FREE_BIT(header, true);    
    mirror_header_to_footer(header);

    // This block is free. So we will set the next block 
    // 'prev block free bit'.
    void *next_header = Header_get_next_header(header);
    SET_PREV_BLOCK_FREE_BIT(next_header, true);

    // Insert this free block into the freelist.
    Segregated_insert_header(header);

    coalesce(header);

    mm_checkheap(1088);

    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    if (oldptr == NULL) {
        return malloc(size);
    }
    else if (size == 0) {
        free(oldptr);
        return NULL;
    }
    else {

        // If the actual available free space in the block is able 
        // to hold the given size, then we will just return the oldptr.
        void *payload = oldptr;
        void *header = Payload_get_header(payload);
        size_t payload_size = GET_SIZE(header);
        if (payload_size + Footer_size() >= size) {
            return oldptr;
        }

        // Else we will allocate a new block and copy the contents
        // from the old block to the new block and free the old block.
        void *new_payload = malloc(size);
        if (!new_payload) {
            return NULL;
        }
        mem_memcpy(new_payload, payload, payload_size + Footer_size());
        free(payload);

        mm_checkheap(1125);
        return new_payload;
    }
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}



/*
 * mm_checkheap
 * You call the function via mm_checkheap(__LINE__)
 * The line number can be used to print the line number of the calling
 * function where there was an invalid heap.
 */
bool mm_checkheap(int line_number)
{
#ifdef DEBUG

    // Here, we will store the count of blocks marked as free in the implicit free list.
    // At the end, we will compare this value with the number of blocks in the explicit free lists.
    // They should match.
    size_t num_free_blocks_implicit_list = 0;

    if (in_heap(heap) == false) {
        dbg_printf("Implicit list head is not within the heap address\n");
        return false;
    }
    
    
    // The first header is the prologue header, which must have a size of 0 and be allocated.
    void *prologue_header = heap;

    if (GET_SIZE(prologue_header) != 0) {
        dbg_printf("Prologue header size is not zero\n");
        return false;
    }

    if (GET_FREE_BIT(prologue_header) != false) {
        dbg_printf("Prologue header is marked as free\n");
        return false;
    }

    // Traverse the implicit freelist
    void *prev_header = prologue_header;
    size_t block_number = 0; 
    while (true) {
        void *header = Header_get_next_header(prev_header);
        block_number++;

        // Check: Verify the setting of bit number 1 in this block. 
        // It should be set if the previous block is free, otherwise it should remain unset.
        bool prev_block_free_bit = GET_PREV_BLOCK_FREE_BIT(header);
        if (GET_FREE_BIT(prev_header) == true) {
            // Since the previous block is free, the bit must be set.
            if (prev_block_free_bit == false) {
                dbg_printf("Previous block is marked as free, but bit 1 of block %ld is not set\n", block_number);
                return false;
            }
        }
        else {
            // Since the previous block is not free, the bit should be unset.
            if (prev_block_free_bit == true) {
                dbg_printf("Previous block is not free, but bit 1 of block %ld is set\n", block_number);
                return false;
            }
        }


        /**
         * This block is labeled as free and must adhere to the following invariants:
         * 1. Is every free block actually present in the explicit free list?
         * 2. Are there any contiguous free blocks that have somehow avoided coalescing?
         * 3. The header and footer must precisely match.
         * 4. If the block is not free, it should not exist in the explicit free list.
         */
        if (GET_FREE_BIT(header)) {  

            num_free_blocks_implicit_list += 1;

            // Check: Is the current block actually in the free list ?
            if (Segregated_header_exists(header) == false) {
                dbg_printf("Block %ld is marked as free but doesnt exists in the freelist\n", block_number);
                return false;
            }

            // Check: Did it escaped coalescing with previous block.
            if (GET_FREE_BIT(prev_header) == true) {
                dbg_printf("Block %ld is marked as free but did not coalescing with the previous block which is also free\n", block_number);
                return false;
            }

            // Check: It is crucial that the header and footer values match precisely.
            // When free() is invoked, it updates the header, and it's vital to mirror this header value to the footer.
            // This is essential due to footer optimization, where the footer area is allocated to the user when the block is allocated,
            // potentially leading to it being overwritten.
            void *footer = Header_get_footer(header);
            if (*(size_t*)header != *(size_t*)footer) {
                dbg_printf("Block %ld is marked as free and the header and footer values dont match\n", block_number);
                return false;
            }
        }
        else {
            // Since the block is not free, it should not be present in any explicit free list.
            if (Segregated_header_exists(header)) {
                dbg_printf("Block %ld is not free but it is present in the freelist\n", block_number);
                return false;
            }
        }

        // We have reached the epilogue block.
        // Stop the loop.
        if (GET_SIZE(header) == 0) {
            break;
        }

        prev_header = header;
    }


    /**
     * In our segregated storage, we have 15 freelists, each requiring consistency checks.
     * The last freelist is for free size, accommodating blocks of any size,
     * while the others should only contain blocks of specific sizes, retrievable from the Segregated_get_size_classes function.
     * 
     * Consistency checks:
     * 1. Are all blocks in the free list marked as free?
     * 2. Do the pointers in the free list point to valid free blocks?
     * 3. Do the pointers in a heap block point to valid heap addresses?
     * 4. The size of the block should match the freelist size class.
     */
    size_t size_classes[15];
    Segregated_get_size_classes(size_classes);


    // Here, we will store the total number of blocks found in all the freelists.
    // This value must match the number of free blocks found while traversing the implicit list.
    size_t num_free_blocks_explicit_list = 0;

    // For each freelist 
    for (int i = 0; i < 15; i++) {
        DList *head = freelists[i];
        DList *prev = NULL;

        while (head) {
            // Check if the linked list pointer is within the heap address space.
            if (in_heap(head) == false) {
                dbg_printf("Linked list node %p is not within the heap address\n", (void*)head);
                return false;
            }

            // Verify if the 'prev' member of the linked list correctly points to the previous linked list node.
            if (head->prev != prev) {
                dbg_printf("Linked list node prev ptr doesn't points to correct previous node\n");
                return false;
            }

            // Verify if the block is marked as free.
            // The linked list is located in the payload part of the block.
            // Therefore, the starting address of the payload is also the starting address of the linked list node.
            void *payload = (void*) head;
            void *header = Payload_get_header(payload);
            if (GET_FREE_BIT(header) == false) {
                dbg_printf("Block in the freelist is not marked as free\n");
                return false;
            }

            // Verify if the payload size matches the freelist size class.
            // Note: The last freelist is of free size, capable of containing blocks of any size.
            if (i != 14 && GET_SIZE(header) != size_classes[i]) {
                dbg_printf("Block is not in the correct freelist. The size of the block doesn't matches the freelist size class\n");
                return false;
            }


            num_free_blocks_explicit_list++;

            prev = head;
            head = head->next;
        }
    }


    // Verify if the number of free blocks in the implicit list matches the
    // number of free blocks in the explicit freelists.
    if (num_free_blocks_implicit_list != num_free_blocks_explicit_list) {
        dbg_printf("Number of free blocks in the implicit list is not matching the total blocks in the freelists\n");
        dbg_printf("Free blocks in implicit list = %ld\n", num_free_blocks_implicit_list);
        dbg_printf("Free blocks in explicit list = %ld\n", num_free_blocks_explicit_list);
        return false;
    }

#endif // DEBUG
    return true;
}


