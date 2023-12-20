#pragma once

#include <stdio.h>

// ---------------------------------------
// VARIABLE TO PLAY WITH
// --------------------------------------
#define ROW 64        // together with column define the size of the heap created.
#define COLUMN 128    // together with row define the size of the heap created
#define MIN_ALLOC_SIZE 16 
#define MAX_ALLOC_SIZE 512
#define PTR_NUMBER 32 // max number of consecutive alloc before a free is called.

// ---------------------------------------
// DO NOT EDIT WHAT IS AFTER
// --------------------------------------
#define HEAP_T_SIZE 20
#define MHZ_TO_HZ 1000000

#define HEAP_T_ASCII '#'
#define METADATA_ASCII '%'
#define DEFAULT_FREE_ASCII '_'
#define POISON_FREE_ASCII 'f'
#define DEFAULT_USED_ASCII 'U'
#define POISON_USED_ASCII 'a'
#define PADDING_ASCII 'X'
#define BLOCK_HEADER_ASCII 'H'
#define BLOCK_FOOTER_ASCII 'F'
#define CANARY_ASCII 'C'

#define MALLOC_FILL_PATTERN 0xce
#define FREE_FILL_PATTERN 0xfe

#define tlsf_cast(t, exp)	((t) (exp))

typedef enum {
    color_unknown = 1,
    color_red,
    color_blue,
    color_green,
    color_yellow,
    color_default
} color_t;

typedef struct multi_heap_info {
    void *lock;
    size_t free_bytes;
    size_t minimum_free_bytes;
    size_t pool_size;
    void* heap_data;
} heap_t;

typedef struct block_header_t
{
	/* Points to the previous physical block. */
	struct block_header_t* prev_phys_block;

	/* The size of this block, excluding the block header. */
	size_t size;

	/* Next and previous free blocks. */
	struct block_header_t* next_free;
	struct block_header_t* prev_free;
} block_header_t;

typedef struct control_t
{
    /* Empty lists point at this block to indicate they are free. */
    block_header_t block_null;

    /* Local parameter for the pool. Given the maximum
	 * value of each field, all the following parameters
	 * can fit on 4 bytes when using bitfields
	 */
    unsigned int fl_index_count : 5; // 5 cumulated bits
    unsigned int fl_index_shift : 3; // 8 cumulated bits
    unsigned int fl_index_max : 6; // 14 cumulated bits
    unsigned int sl_index_count : 6; // 20 cumulated bits

	/* log2 of number of linear subdivisions of block sizes. Larger
	** values require more memory in the control structure. Values of
	** 4 or 5 are typical.
	*/
    unsigned int sl_index_count_log2 : 3; // 23 cumulated bits
    unsigned int small_block_size : 8; // 31 cumulated bits

	/* size of the metadata ( size of control block,
	 * sl_bitmap and blocks )
	 */
    size_t size;

    /* Bitmaps for free lists. */
    unsigned int fl_bitmap;
    unsigned int *sl_bitmap;

    /* Head of free lists. */
    block_header_t** blocks;
} control_t;

static const size_t block_size_min = 
	sizeof(block_header_t) - sizeof(block_header_t*);

static const size_t block_header_free_bit = 1 << 0;
static const size_t block_header_prev_free_bit = 1 << 1;

size_t block_size(const block_header_t* block)
{
	return block->size & ~(block_header_free_bit | block_header_prev_free_bit);
}
int block_is_prev_free(const block_header_t* block)
{
	return tlsf_cast(int, block->size & block_header_prev_free_bit);
}

int block_is_free(const block_header_t* block)
{
	return tlsf_cast(int, block->size & block_header_free_bit);
}

typedef struct {
    uint32_t head_canary;
    size_t alloc_size;
} poison_head_t;

typedef struct {
    uint32_t tail_canary;
} poison_tail_t;

#define POISON_OVERHEAD (sizeof(poison_head_t) + sizeof(poison_tail_t))

typedef struct info_t
{
    size_t alloc_miss;
    size_t size_of_last_failed;
    uint32_t average_free_ticks;
    uint32_t average_alloc_ticks;
}info_t;

inline void init_info(info_t* info) {
        info->alloc_miss = 0;
        info->size_of_last_failed = 0;
        info->average_free_ticks = 0;
        info->average_alloc_ticks = 0;
}

inline void clear_screen() { printf("\033[2J"); }

void update_heap_representation(control_t* control, void* heap_memory, char* heap_representation, size_t heap_memory_size);
void print_update_heap_representation(char* current, char* previous, size_t size);
void print_alloc_info(info_t info);
void set_print_color(const color_t color, const bool update_current_color);