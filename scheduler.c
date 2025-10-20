#include "scheduler.h"
#include "virtmem.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"
#include <unistd.h>

/**
 * @brief Scheduling algorithm.
 *
 * This function implements the scheduling algorithm.
 * It is a simple RR (Round Robin) scheduler with time slices and preemption.
 *
 * @param current_time_ms The current time in milliseconds.
 * @param rq Pointer to the ready queue containing tasks that are ready to run.
 * @param cpu_task Double pointer to the currently running task. This will be updated
 *                 to point to the next task to run.
 * @return int Returns 0 if the same task continues, 1 if a new task was scheduled onto the CPU.
 */
int scheduler(uint32_t current_time_ms, queue_t *rq, queue_t *cq, pcb_t **cpu_task) {
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;      // Add to the running time of the application/task
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // Task finished
            // Send msg to application
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            // Burst is finished
            enqueue_pcb(cq, *cpu_task);
            (*cpu_task) = NULL;
        } else if ((current_time_ms - (*cpu_task)->slice_start_ms) >= TIME_SLICE_MS) {
            // Time slice expired, preempt and put back in ready queue
            (*cpu_task)->slice_start_ms = 0;
            enqueue_pcb(rq, *cpu_task);  // Add to tail of ready queue
            (*cpu_task) = NULL;
        }
    }
    if (*cpu_task == NULL) {            // If CPU is idle
        *cpu_task = dequeue_pcb(rq);   // Get next task from ready queue (dequeue from head)
        // TODO: Handle the swapping, if any add a 50ms penalty to the slice time
        if (*cpu_task) {
            (*cpu_task)->slice_start_ms = current_time_ms;
            return 1;
        }
    }
}