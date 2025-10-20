//
// Created by Martijn Kuipers on 10/10/2025.
//

#include "swap.h"

#include "uthash.h"

typedef struct {
    int page_id;            // Key: Page ID
    int frame_id;          // Value: Frame ID
    UT_hash_handle hh;     // Makes this structure hashable
} page_table_entry_t;

