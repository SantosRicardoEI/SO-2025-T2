#ifndef FIFO_H
#define FIFO_H

#include "queue.h"

#define TIME_SLICE_MS 500

int scheduler(uint32_t current_time_ms, queue_t *rq, queue_t *cq, pcb_t **cpu_task);

#endif //FIFO_H
