#ifndef VIRTMEM_TYPES_H
#define VIRTMEM_TYPES_H

#include <stdint.h>
#include "uthash.h"

#define INVALID_FRAME -1
typedef enum { VM_RANDOM=0, VM_FIFO, VM_NRU, VM_LRU } vm_policy_t;

/* Page table entry */
typedef struct pte_st {
    int32_t  frame_id;
    uint8_t  present:1;
    uint8_t  referenced:1;
    uint8_t  dirty:1;
    uint32_t last_accessed;
} pte_t;

/* Per-process page table (TAGGED so it can be forward-declared) */
typedef struct page_table_st {
    uint8_t  nvalid;
    pte_t   *vp;          // index = VPN
} page_table_t;

/* Physical frame descriptor */
typedef struct frame_desc_st {
    pte_t    *vp;         // which process/page owns this frame
    int32_t   pid;        // owner
    uint32_t  vfn;        // which virtual page
} frame_desc_t;

/* Free-frame stack + FIFO eviction queue */
typedef struct free_stack_st {
    uint32_t *ids;
    int       max_size;
    int       top;
} free_stack_t;

typedef struct fifo_st {
    uint32_t *ids;
    int       max_size;
    int       top;
} fifo_t;

/* Frame table */
typedef struct frame_table_st {
    int           no_frames;
    frame_desc_t *frames;
    free_stack_t  free_stack;

    fifo_t        eviction_order;   // Used for FIFO eviction
} frame_table_t;

typedef struct swapped_frame_st {
    uint64_t page_id;        // key: (pid<<32)|vpn
    uint8_t  dirty:1;        // was the page dirty on eviction
    uint32_t last_accessed;  // optional: for stats/aging
    UT_hash_handle hh;       // makes this structure hashable
} swapped_frame_t;

typedef struct swap_hash_st {
    int num_swapped;                 // number of swapped frames
    uint32_t last_swap_time_ms;      // last time a swap occurred
    swapped_frame_t *pages;        // hash table of swapped pages
} swap_hash_t;

#endif
