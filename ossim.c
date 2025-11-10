#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/errno.h>
#include <stdlib.h>
#include "scheduler.h"
#include "virtmem.h"
#include <time.h>
#include "ossim.h"

#include "msg.h"
#include "queue.h"

// Contadores globais de estatísticas (reiniciar OSSIM para resetar)
int total_page_faults = 0;
int total_swaps_in = 0;
int total_swaps_out = 0;
int total_page_accesses = 0;
int access_counter = 0;


static volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    printf("\n[Signal] Caught signal %d — stopping scheduler...\n", sig);
    keep_running = 0; // tell main loop to exit
}

int parse_args(int argc, char *argv[], int *pages, int *frames, int *threshold) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pages") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                errno = 0;
                long val = strtol(argv[++i], &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val <= 0) {
                    fprintf(stderr, "Error: invalid number for --pages: %s\n", argv[i]);
                    return -1;
                }
                *pages = (int) val;
            } else {
                fprintf(stderr, "Error: --pages requires a number\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                errno = 0;
                long val = strtol(argv[++i], &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val <= 0) {
                    fprintf(stderr, "Error: invalid number for --frames: %s\n", argv[i]);
                    return -1;
                }
                *frames = (int) val;
            } else {
                fprintf(stderr, "Error: --frames requires a number\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--threshold") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                errno = 0;
                long val = strtol(argv[++i], &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val < 0) {
                    fprintf(stderr, "Error: invalid number for --threshold: %s\n", argv[i]);
                    return -1;
                }
                *threshold = (int) val;
            } else {
                fprintf(stderr, "Error: --threshold requires a number\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--pages <num>] [--frames <num>] [--threshold <num>]\n", argv[0]);
            return 1;  // signal "show help"
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Try --help\n");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    clock_t start_time = clock();
    int num_pages = 20;
    int num_frames = 30;
    int min_pages_threshold = 4;

    int res = parse_args(argc, argv, &num_pages, &num_frames, &min_pages_threshold);
    if (res > 0) { // help shown
        return EXIT_SUCCESS;
    } else if (res < 0) {
        return EXIT_FAILURE;
    }

    // Catch CTRL-C and termination signals to exit gracefully
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("OSSIM Scheduler configured with %d pages and %d frames\n", num_pages, num_frames);

    // We set up 3 queues: 1 for the simulator and 2 for scheduling
    // - COMMAND queue: for PCBs that are waiting for (new) instructions from the app
    // - READY queue: for PCBs that are ready to run on the CPU
    // - BLOCKED queue: for PCBs that are blocked waiting for I/O
    queue_t command_queue = {.head = NULL, .tail = NULL};
    queue_t ready_queue = {.head = NULL, .tail = NULL};
    queue_t blocked_queue = {.head = NULL, .tail = NULL};

    // We only have a single CPU that is a pointer to the actively running PCB on the CPU
    pcb_t *CPU = NULL;

    frame_table_t *frame_table = create_frame_table(num_frames);
    swap_hash_t swap = {.last_swap_time_ms = 0, .num_swapped = 0, .pages = NULL};

    int server_fd = setup_server_socket(SOCKET_PATH);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up server socket\n");
        return 1;
    }
    printf("Scheduler server listening on %s...\n", SOCKET_PATH);
    uint32_t current_time_ms = 0;

    while (keep_running) {
        // Check for new connections and/or instructions
        check_new_commands(&command_queue, &blocked_queue, &ready_queue, server_fd, current_time_ms);
        check_blocked_queue(&blocked_queue, &command_queue, current_time_ms);

        if (current_time_ms%1000 == 0) {
            printf("Current time: %d s\n", current_time_ms/1000);
        }
        usleep(TICKS_MS * 1000/2);

        // Tasks from the blocked queue could be moved to the command queue, check again
        check_new_commands(&command_queue, &blocked_queue, &ready_queue, server_fd, current_time_ms);
        check_blocked_queue(&blocked_queue, &command_queue, current_time_ms);

        // The scheduler handles the READY queue
        if (scheduler(current_time_ms, &ready_queue, &command_queue, &CPU) > 0 && CPU) {
            for (uint32_t i = 0; i < CPU->requested_pages.count; i++) {
                total_page_accesses++;
                int vfn = CPU->requested_pages.ids[i];
                int is_dirty = 0;

                // Negative page number means write; normalize
                if (vfn < 0) {
                    is_dirty = 1;
                    vfn = -vfn;
                }
                page_eviction(frame_table, &swap, min_pages_threshold);

                pte_t *vp = page_request(current_time_ms,CPU, frame_table, &swap, vfn);
                vp->last_accessed = current_time_ms;

                if (!vp) {
                    printf("ERROR: Cannot request a page %d for process %d\n", vfn, CPU->pid);
                    continue;
                }

                vp->referenced = 1;
                vp->present = 1;
                vp->last_accessed = current_time_ms;
                vp->dirty = is_dirty ? 1 : vp->dirty;
            }
        }

        // Simulate a tick
        usleep(TICKS_MS * 1000/2);
        current_time_ms += TICKS_MS;
    }

    printf("[Scheduler] Cleaning up and shutting down...\n");
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("[Scheduler] Shutdown complete.\n");

    double fault_rate = (total_page_accesses > 0)
    ? (100.0 * total_page_faults / total_page_accesses)
    : 0.0;
    printf("\n================== Dados de execução do OSSIM =================\n");
    printf("Algoritmo utilizado: %s\n", policy_to_string(current_policy));
    printf("Páginas: %d, Frames: %d, Threshold: %d\n", num_pages, num_frames, min_pages_threshold);
    printf("Acessos a Páginas: %d\n", total_page_accesses);
    printf("Page Faults: %d\n", total_page_faults);
    printf("Taxa de Page Faults: %.2f%%\n", fault_rate);
    printf("Swaps In: %d\n", total_swaps_in);
    printf("Swaps Out: %d\n", total_swaps_out);
    printf("Evictions: %d\n", total_swaps_out);

    clock_t end_time = clock();
    double elapsed_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Tempo total de execução (simulador): %.3f segundos\n", elapsed_seconds);

    return 0;
}
