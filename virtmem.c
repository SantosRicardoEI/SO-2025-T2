//
// Created by Martijn Kuipers on 10/10/2025.
//

#include "virtmem_types.h"
#include "virtmem.h"

#include <stdio.h>
#include <stdlib.h>   // malloc, calloc, free
#include <stdint.h>

/**
 * This function creates and initializes the frame table
 * @param num_frames the number of frames in the frame table
 * @return pointer to the created frame table, or NULL on failure
 */
frame_table_t *create_frame_table(int num_frames) {
    if (num_frames <= 0) {
        printf("create_frame_table: invalid num_frames=%d\n", num_frames);
        return NULL;
    }

    frame_table_t *ft = (frame_table_t *)malloc(sizeof(frame_table_t));
    if (!ft) {
        printf("Cannot allocate memory for frame table\n");
        return NULL;
    }

    // Allocate and zero-initialize frame descriptors
    ft->frames = (frame_desc_t *)calloc((size_t)num_frames, sizeof(frame_desc_t));
    if (!ft->frames) {
        printf("Cannot allocate memory for frame descriptors\n");
        free(ft);
        return NULL;
    }

    // The meaning of 'number' in your project is used as TABLE CAPACITY in loops.
    // Keep it equal to the total number of frames available.
    ft->no_frames = num_frames;

    if (init_free_stack(&ft->free_stack, num_frames) < 0) {
        free(ft->frames);
        free(ft);
        return NULL;
    }

    if (init_fifo_eviction(&ft->eviction_order, num_frames) < 0) {
        printf("Cannot allocate memory for FIFO eviction order\n");
        free(ft->frames);
        free(ft);
        return NULL;
    }
    return ft;
}


/**
 * This function initializes the page table
 * @param pt the page table to initialize
 * @param max_size maximum number of pages in the page table
 * @return 0 on success, -1 on failure
 */
int create_page_table(page_table_t *pt, int max_size) {
    if (pt == NULL || max_size <= 0) {
        printf("Invalid page table\n");
        return -1;
    }
    pt->vp = (pte_t *)malloc((size_t)max_size * sizeof(pte_t));
    if (pt->vp == NULL) {
        printf("Cannot allocate memory for page table\n");
        return -1;
    }
    pt->nvalid = max_size - 1;
    // Initialize all page table entries
    for (int i = 0; i < max_size; ++i) {
        pt->vp[i].frame_id = INVALID_FRAME;
        pt->vp[i].present = 0;
        pt->vp[i].referenced = 0;
        pt->vp[i].dirty = 0;
        pt->vp[i].last_accessed = 0;
    }
    return 0;
}

/**
 * Check if the given page table entry is active (present in RAM)
 * @param page the page table entry
 * @return 1 if active, 0 otherwise
 */
int is_active(pte_t *page) {
    if (page == NULL) return 0;
    return page->present;
}

/**
 * Check if the given page table entry is valid (mapped to a frame)
 * @param page the page table entry
 * @return 1 if valid, 0 otherwise
 */
int is_valid(pte_t *page) {
    if (page == NULL) {
        return 0;
    }
    return page->frame_id != INVALID_FRAME;
}

/**
 *  Find the page table entry for the given virtual frame number
 * @param pt  the page table
 * @param vfn  the virtual frame number
 * @return  pointer to the page table entry, or NULL if not found
 */
pte_t *find_page(page_table_t *pt, int32_t vfn) {
    if (pt == NULL || vfn < 1 || vfn >= pt->nvalid) {
        return NULL;
    }
    return &pt->vp[vfn-1];
}

/**
 * Initialize the free stack
 * @param stack the free stack to initialize
 * @param num_frames the number of frames to allocate space for
 * @return 0 on success, -1 on failure
 */
int init_free_stack(free_stack_t *stack, int num_frames) {
    if (stack == NULL || num_frames <= 0) {
        printf("Invalid free stack\n");
        return -1;
    }
    stack->ids = (uint32_t *)malloc((size_t)num_frames * sizeof(uint32_t));
    if (stack->ids == NULL) {
        printf("Cannot allocate memory for free stack\n");
        return -1;
    }
    stack->top = num_frames - 1;
    stack->max_size = num_frames;
    for (int i = 0; i < num_frames; ++i) stack->ids[i] = i;
    return 0;
}

/**
 *  Push a free frame ID onto the stack
 * @param stack  the free stack
 * @param frame_id  the frame ID to push
 * @return  1 on success, 0 on failure
 */
int push_free_frame(free_stack_t *stack, int frame_id) {
    if (stack == NULL || frame_id < 0 || stack->top >= (stack->max_size - 1)) {
        return 0;
    }
    stack->ids[++(stack->top)] = (uint32_t)frame_id;
    return 1;
}


/**
 * Pop a free frame ID from the stack
 * @param stack the free stack
 * @return the popped frame ID, or INVALID_FRAME if the stack is empty
 */
int pop_free_frame(free_stack_t *stack) {
    if (stack == NULL || stack->top < 0) return INVALID_FRAME;
    return (int)(stack->ids[(stack->top)--]);
}

/**
 * Initialize the FIFO eviction structure
 * @param fifo the FIFO structure to initialize
 * @param num_frames the number of frames to allocate space for
 * @return 0 on success, -1 on failure
 */
int init_fifo_eviction(fifo_t *fifo, int num_frames) {
    if (fifo == NULL || num_frames <= 0) {
        return -1;
    }
    fifo->ids = (uint32_t *)malloc((size_t)num_frames * sizeof(uint32_t));
    if (fifo->ids == NULL) {
        printf("Cannot allocate memory for FIFO eviction\n");
        return -1;
    }
    fifo->top = -1;
    fifo->max_size = num_frames;
    return 0;
}

/**
 * Push a frame ID onto the FIFO eviction structure
 * @param fifo the FIFO structure
 * @param frame_id the frame ID to push
 * @return 1 on success, 0 on failure
 */
int push_fifo_eviction(fifo_t *fifo, int frame_id) {
    if (fifo == NULL || frame_id < 0 || fifo->top >= (fifo->max_size - 1)) {
        return 0;
    }
    fifo->ids[++(fifo->top)] = (uint32_t)frame_id;
    return 1;
}

/**
 * Pop a frame ID from the FIFO eviction structure
 * @param fifo the FIFO structure
 * @return the popped frame ID, or INVALID_FRAME if the structure is empty
 */
int32_t pop_fifo_eviction(fifo_t *fifo) {
    if (fifo == NULL || fifo->top < 0) return INVALID_FRAME;
    int32_t frame_id = fifo->ids[0];
    // Shift all elements to the left
    for (int i = 1; i <= fifo->top; ++i) {
        fifo->ids[i - 1] = fifo->ids[i];
    }
    fifo->top--;
    return frame_id;
}

/**
 * Swap out a page to the swap hash
 * @param swap the swap hash
 * @param fd the frame descriptor of the page to swap out
 * @return 0 on success, -1 on failure
 */
int swap_out(swap_hash_t *swap, frame_desc_t *fd) {
    pte_t *vp = fd->vp;
    uint64_t page_key = (((uint64_t)fd->pid) << 32) | ((uint64_t)fd->vfn);
    swapped_frame_t *swapped_page = (swapped_frame_t *)malloc(sizeof(swapped_frame_t));
    if (!swapped_page) {
        printf("Cannot allocate memory for swapped frame\n");
        return -1;
    }
    swapped_page->page_id = page_key;
    swapped_page->dirty = vp->dirty;
    swapped_page->last_accessed = vp->last_accessed;
    HASH_ADD(hh, swap->pages, page_id, sizeof(uint64_t), swapped_page);
    swap->num_swapped += 1;
    return 0;
}

/**
 * Swap in a page from the swap hash
 * @param swap the swap hash
 * @param fd the frame descriptor of the page to swap in
 * @return 0 on success, -1 on failure
 */
int swap_in(swap_hash_t *swap, frame_desc_t *fd) {
    pte_t *vp = fd->vp;
    uint64_t page_key = (((uint64_t)fd->pid) << 32) | ((uint64_t)fd->vfn);
    swapped_frame_t *swapped_page = NULL;
    HASH_FIND(hh, swap->pages, &page_key, sizeof(uint64_t), swapped_page);
    if (!swapped_page) {
        printf("Page not found in swap\n");
        return -1;
    }
    // Restore page properties
    vp->dirty = swapped_page->dirty;
    vp->last_accessed = swapped_page->last_accessed;
    // Remove from swap
    HASH_DEL(swap->pages, swapped_page);
    free(swapped_page);
    swap->num_swapped -= 1;
    return 0;
}

/**
 * This function handles a page request for a given process
 * @param pcb Process Control Block of the requesting process
 * @param frame_table The frame table
 * @param swap The swap
 * @param vfn The virtual frame number requested
 * @return Pointer to the page table entry of the requested page, or NULL on failure
 */
pte_t *page_request(pcb_t *pcb, frame_table_t *frame_table, swap_hash_t *swap, int vfn) {
    printf("Requesting page %d for process %d\n", vfn, pcb->pid);
    pte_t *vp = find_page(&pcb->page_table, vfn);
    if (is_active(vp)) {                // Page is present in RAM
        printf("Page %d is active in RAM, just bookkeeping\n", vfn);
        return vp;
    }
    if (is_valid(vp)) {                 // Page is swapped out
        // Assume there is a free frame, so get one
        printf("Swap in page %d for process %d\n", vfn, pcb->pid);
        int32_t next_frame = pop_free_frame(&frame_table->free_stack);
        frame_desc_t *fd = &frame_table->frames[next_frame];
        if (swap_in(swap,fd) < 0) {
            printf("ERROR: Failed to swap in page %d for process %d\nTrying to continue\n", vfn, pcb->pid);
        }
        vp->frame_id = next_frame;
        push_fifo_eviction(&frame_table->eviction_order, next_frame);
        return vp;
    }
    // Page not valid, need to allocate
    printf("Allocating page %d for process %d\n", vfn, pcb->pid);
    int32_t next_frame = pop_free_frame(&frame_table->free_stack);
    frame_desc_t *fd = &frame_table->frames[next_frame];
    vp->frame_id = next_frame;
    fd->vp = vp;
    fd->pid = pcb->pid;
    fd->vfn = vfn;
    push_fifo_eviction(&frame_table->eviction_order, next_frame);
    return vp;
}

/**
 * This function evicts pages from the frame table until there are enough free pages
 * @param frame_table The frame table
 * @param swap The swap
 * @return 0 on success, -1 on failure
 */
int page_eviction(frame_table_t *frame_table, swap_hash_t *swap, int32_t min_pages_threshold) {
    while (frame_table->free_stack.top < min_pages_threshold) {
        printf("Eviction (only %d pages left)\n", frame_table->free_stack.top);
        int evict_frame = pop_fifo_eviction(&frame_table->eviction_order);
        if (evict_frame == INVALID_FRAME) {
            printf("No frame to evict!\n");
            return -1;
        }
        frame_desc_t *fd = &frame_table->frames[evict_frame];
        pte_t *vp = fd->vp;
        if (!vp) {
            printf("Frame %d has no valid page to evict!\n", evict_frame);
            continue;
        }
        printf("Evicting page %d of process %d from frame %d\n", fd->vfn, fd->pid, evict_frame);
        // Mark page as not present
        vp->present = 0;
        // Add to swap
        if (swap_out(swap, fd) < 0) {
            printf("Failed to swap out page %d of process %d\nFreeing the frame anyway\n", fd->vfn, fd->pid);
        }
        // Free the frame
        push_free_frame(&frame_table->free_stack, evict_frame);
    }
    return 0;
}
