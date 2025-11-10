#ifndef VIRTMEM_TYPES_H
#define VIRTMEM_TYPES_H

#include <stdint.h>
#include "uthash.h"

#define INVALID_FRAME -1
typedef enum { VM_RANDOM=0, VM_FIFO, VM_NRU, VM_LRU, VM_CLOCK } vm_policy_t;
const char *policy_to_string(vm_policy_t policy);

// =========================================== Paginas virtuais ========================================================
// Estruturas que descrevem cada página virtual (PTE) e a lista completa de páginas de um processo (page table)

// Page Table Entry (PTE): uma ENTRADA da page table que descreve UMA *PAGINA VIRTUAL*
typedef struct pte_st {
    int32_t  frame_id;
    uint8_t  present:1;
    uint8_t  referenced:1;
    uint8_t  dirty:1;
    uint32_t last_accessed;
} pte_t;

// Page table (cada processo tem uma): array de PTEs que mapeia páginas virtuais -> frames físicas
typedef struct page_table_st {
    uint8_t  nvalid;
    // Lista de ptes
    // O índice da página virtual (VPN) corresponde à posição neste array
    pte_t   *vp;
} page_table_t;

// =============================================== FIFO coisas =========================================================

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

// =========================================== Frames (memória física) =================================================

// Representa um frame físico (bloco de RAM) e a que página/processo pertence
typedef struct frame_desc_st {
    pte_t    *vp;         // pagina virtual correspondente
    int32_t   pid;        // ID do processo dono
    uint32_t  vfn;        // qual a posicao da pagina virtual na page table do processo
} frame_desc_t;

// Representa toda a memória física (lista de frames)
typedef struct frame_table_st {
    int           no_frames;     // Quantidade de frames físicos
    frame_desc_t *frames;        // lista dos frames
    free_stack_t  free_stack;    // pilha que guarda frames livres, redundante mas mais eficiente e prático

    fifo_t        eviction_order;   // Used for FIFO eviction
} frame_table_t;

// =============================================== SWAP ================================================================

// Representa uma página que foi removida da RAM e colocada no disco (swap)
typedef struct swapped_frame_st {
    uint64_t page_id;        // key: (pid<<32)|vpn
    uint8_t  dirty:1;        // was the page dirty on eviction
    uint32_t last_accessed;  // optional: for stats/aging
    UT_hash_handle hh;       // makes this structure hashable
} swapped_frame_t;

// Representa a estrutura global do swap (todas as páginas que estão no disco)
typedef struct swap_hash_st {
    int num_swapped;                 // number of swapped frames
    uint32_t last_swap_time_ms;      // last time a swap occurred
    swapped_frame_t *pages;        // hash table of swapped pages
} swap_hash_t;

#endif
