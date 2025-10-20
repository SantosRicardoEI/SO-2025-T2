//
// Created by Martijn Kuipers on 10/10/2025.
//

#ifndef VIRTMEM_H
#define VIRTMEM_H

#include "virtmem_types.h"
#include "pcb.h"

int create_page_table(page_table_t *pt, int max_size);
frame_table_t *create_frame_table(int num_frames);

pte_t *find_page(page_table_t *pt, int32_t vfn);
int is_active(pte_t *page);
int is_valid(pte_t *page);

int init_free_stack(free_stack_t *stack, int num_frames);
int push_free_frame(free_stack_t *stack, int frame_id);
int pop_free_frame(free_stack_t *stack);

int init_fifo_eviction(fifo_t *fifo, int num_frames);
int push_fifo_eviction(fifo_t *fifo, int frame_id);
int pop_fifo_eviction(fifo_t *fifo);

int swap_out(swap_hash_t *swap, frame_desc_t *fd);
int swap_in(swap_hash_t *swap, frame_desc_t *fd);

int page_eviction(frame_table_t *frame_table, swap_hash_t *swap, int32_t min_pages_threshold);
pte_t *page_request(pcb_t *pcb, frame_table_t *frame_table, swap_hash_t *swap, int vfn);

#endif //VIRTMEM_H
