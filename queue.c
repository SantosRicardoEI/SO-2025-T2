#include "queue.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "virtmem.h"

#include "debug.h"

static uint32_t PID = 0;

pcb_t *new_pcb(pid_t pid, uint32_t sockfd, uint32_t time_ms) {
    pcb_t * new_task = malloc(sizeof(pcb_t));
    if (!new_task) return NULL;

    new_task->pid = pid;
    new_task->status = TASK_COMMAND;
    new_task->slice_start_ms = 0;
    new_task->sockfd = sockfd;
    new_task->time_ms = time_ms;
    new_task->ellapsed_time_ms = 0;
    // Initialize the allocated pages
    new_task->requested_pages.count = 0;
    for (int i = 0; i < MAX_PAGES; i++) {
        new_task->requested_pages.ids[i] = 0;
    }
    if (create_page_table(&new_task->page_table, MAX_PAGES) < 0) return NULL;
    return new_task;
}

int enqueue_pcb(queue_t* q, pcb_t* task) {
    queue_elem_t* elem = malloc(sizeof(queue_elem_t));
    if (!elem) return 0;

    elem->pcb = task;
    elem->next = NULL;

    if (q->tail) {
        q->tail->next = elem;
    } else {
        q->head = elem;
    }
    q->tail = elem;
    return 1;
}

pcb_t* dequeue_pcb(queue_t* q) {
    if (!q || !q->head) return NULL;

    queue_elem_t* node = q->head;
    pcb_t* task = node->pcb;

    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    free(node);
    return task;
}

queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem) {
    queue_elem_t* it = q->head;
    queue_elem_t* prev = NULL;
    while (it != NULL) {
        if (it == elem) {
            // Remove elem from queue
            if (prev) {
                prev->next = it->next;
            } else {
                q->head = it->next;
            }
            if (it == q->tail) {
                q->tail = prev;
            }
            return it;
        }
        prev = it;
        it = it->next;
    }
    printf("Queue element not found in queue\n");
    return NULL;
}

/**
 * @brief Set up the server socket for the scheduler.
 *
 * This function creates a UNIX domain socket, binds it to a specified path,
 * and sets it to listen for incoming connections. It also sets the socket to
 * non-blocking mode.
 *
 * @param socket_path The path where the socket will be created
 * @return int Returns the server file descriptor on success, or -1 on failure
 */
int setup_server_socket(const char *socket_path) {
    int server_fd;
    struct sockaddr_un addr;

    // Clean up old socket file
    unlink(socket_path);

    // Create UNIX socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bind
    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    // Set the socket to non-blocking mode
    int flags = fcntl(server_fd, F_GETFL, 0); // Get current flags
    if (flags != -1) {
        if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl: set non-blocking");
        }
    }
    return server_fd;
}

/**
 * When using read you can get partial reads, so we need to loop until we have read the full message
 * @param sockfd   Socket file descriptor
 * @param msg      Pointer to the message structure to fill
 * @param msg_len  Length of the message structure
 * @return Number of bytes read, 0 if no data available, -1 on error or connection closed
 */
ssize_t receive_msg(int sockfd, void *msg, ssize_t msg_len) {
    ssize_t want = msg_len;
    ssize_t off = 0;

    while (off < want) {
        ssize_t n = read(sockfd, ((char*)msg) + off, want - off);
        if (n > 0) {
            off += n;
            if (off == want) return off;
        }
        if (n == 0)  return -1; // peer closed
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            perror("read");
            return -1;
        }
    }
    return off;
}

/**
 * @brief Check for new client connections and add them to the queue.
 *
 * This function accepts new client connections on the server socket,
 * sets the client sockets to non-blocking mode, and enqueues them
 * into the provided queue.
 *
 * @param command_queue The queue to which new pcb will be added
 * @param server_fd The server socket file descriptor
 */
void check_new_commands(queue_t *command_queue, queue_t *blocked_queue, queue_t *ready_queue,
                        int server_fd, uint32_t current_time_ms)
{
    // Accept new client connections
    int client_fd;
    do {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EMFILE || errno == ENFILE) {
                perror("accept: too many fds");
                break;
            }
            if (errno == EINTR)        continue;   // interrupted -> retry
            if (errno == ECONNABORTED) continue;   // aborted handshake -> next
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                perror("accept");
            }
            // No more clients to accept right now
            break;
        }
        int flags = fcntl(client_fd, F_GETFL, 0); // Get current flags
        if (flags != -1) {
            if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("fcntl: set non-blocking");
            }
        }
        // Set close-on-exec flag
        int fdflags = fcntl(client_fd, F_GETFD, 0);
        if (fdflags != -1) {
            fcntl(client_fd, F_SETFD, fdflags | FD_CLOEXEC);
        }

        DBG("[Scheduler] New client connected: fd=%d\n", client_fd);

        // New PCBs do not have a time yet; set when we receive RUN
        pcb_t *pcb = new_pcb(++PID, client_fd, 0);
        enqueue_pcb(command_queue, pcb);
    } while (client_fd > 0);

    // Walk the command queue looking for messages
    queue_elem_t *elem = command_queue->head;
    while (elem != NULL) {
        pcb_t *current_pcb = elem->pcb;
        msg_t msg;

        ssize_t n = receive_msg(current_pcb->sockfd, &msg, sizeof(msg_t));
        if (n == 0) {
            // No data available right now on this non-blocking socket; check next
            elem = elem->next;
            continue;
        }

        if (n < 0) {
            // Peer closed or fatal read error
            DBG("Connection closed by client (fd=%d)\n", current_pcb->sockfd);
            close(current_pcb->sockfd);

            // Save next before unlinking/freeing this node
            queue_elem_t *next = elem->next;

            // Unlink this node from the command queue, then free it and the PCB
            remove_queue_elem(command_queue, elem);
            free(current_pcb);
            free(elem);

            // Continue from saved next
            elem = next;
            continue;
        }

        // We have received a full message
        if (msg.request == PROCESS_REQUEST_RUN) {
            current_pcb->pid = msg.pid; // Set the pid from the message
            current_pcb->time_ms = msg.time_ms;
            current_pcb->ellapsed_time_ms = 0;
            current_pcb->status = TASK_RUNNING;
            current_pcb->requested_pages = msg.pages;

            // Move PCB to READY (do not free PCB)
            enqueue_pcb(ready_queue, current_pcb);
            DBG("Process %d requested RUN for %d ms\n", current_pcb->pid, current_pcb->time_ms);

        } else if (msg.request == PROCESS_REQUEST_BLOCK) {
            current_pcb->pid = msg.pid;
            current_pcb->time_ms = msg.time_ms;
            current_pcb->status = TASK_BLOCKED;

            // Move PCB to BLOCKED (do not free PCB)
            enqueue_pcb(blocked_queue, current_pcb);
            DBG("Process %d requested BLOCK for %d ms\n", current_pcb->pid, current_pcb->time_ms);

        } else {
            // Unexpected message → skip this entry safely
            printf("Unexpected message received from client\n");
            elem = elem->next;
            continue;
        }

        // Remove the queue node from COMMAND after moving the PCB elsewhere
        queue_elem_t *next = elem->next;         // save next first
        remove_queue_elem(command_queue, elem);  // unlink this node
        free(elem);                              // free only the node, NOT the PCB
        elem = next;

        // Send ACK back to the client
        msg_t ack_msg = {
            .pid = current_pcb->pid,
            .request = PROCESS_REQUEST_ACK,
            .time_ms = current_time_ms
        };
        if (write(current_pcb->sockfd, &ack_msg, sizeof(msg_t)) != sizeof(msg_t)) {
            perror("write");
        }
        DBG("Send ACK message to process %d with time %d\n", current_pcb->pid, current_time_ms);
    }
}

/**
 * @brief Check the blocked queue for messages from clients.
 *
 * This function iterates through the blocked queue, checking each client
 * socket for incoming messages. If a RUN message is received, the corresponding
 * pcb is moved to the command queue and an ACK message is sent back to the client.
 * If a client disconnects or an error occurs, the client is removed from the blocked queue.
 *
 * @param blocked_queue The queue containing PCBs in I/O wait stated (blocked) from CPU
 * @param command_queue The queue where PCBs ready for new instructions will be moved
 * @param current_time_ms The current time in milliseconds
 */
void check_blocked_queue(queue_t * blocked_queue, queue_t * command_queue, uint32_t current_time_ms) {
    // Check all elements of the blocked queue for new messages
    queue_elem_t * elem = blocked_queue->head;
    while (elem != NULL) {
        pcb_t *pcb = elem->pcb;

        // Make sure the time is updated only once per cycle
        if (pcb->last_update_time_ms < current_time_ms) {
            if (pcb->time_ms > TICKS_MS) {
                pcb->time_ms -= TICKS_MS;
            } else {
                pcb->time_ms = 0;
            }
        }

        if (pcb->time_ms == 0) {
            // Send DONE message to the application
            msg_t msg = {
                .pid = pcb->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write(pcb->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            DBG("Process %d finished BLOCK, sending DONE\n", pcb->pid);
            pcb->status = TASK_COMMAND;
            pcb->last_update_time_ms = current_time_ms;
            enqueue_pcb(command_queue, pcb);

            // Remove from blocked queue
            remove_queue_elem(blocked_queue, elem);
            queue_elem_t *tmp = elem;
            elem = elem->next;  // Do this here, because we free it in the next line
            free(tmp);
        } else {
            elem = elem->next;  // If not done already, do it now
        }
    }
}
