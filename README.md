[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/sncDcQ7e)
# Scheduler Simulator

The objective of this project is to simulate various CPU scheduling algorithms and
analyze their performance based on different metrics. The simulator allows users to
input a set of processes with their respective arrival times, burst times, and priorities,
and then applies different scheduling algorithms to determine the order of execution.

## Scheduling Algorithms

### Round Robin
The Round Robin scheduling algorithm assigns a fixed time slice to each task in the queue. Each task
is executed for a maximum of the time slice before being moved to the back of the queue.

The diagram used here is the same as for MLFQ in the previous excercise, as it includes not only RUN
messages, but also BLOCK messages. The BLOCK messages are used to simulate I/O operations.

```
Simulator                           Applications
   |                                    |
   | <---- App1 RUN (run time) -------- |
   |                                    |
   | ---- App1 ACK (current time) ----> |
   |                                    |
   | <---- App2 RUN (run time) -------- |
   |                                    |
   | ---- App2 ACK (current time) ----> |
   |                                    |
   | ---- App1 DONE (current time) ---> | 
   |                                    |
   | <---- App1 BLOCK (run time) ------ |
   |                                    |
   | ---- App1 ACK (current time) ----> |
   |                                    |
   | ---- App2 DONE (current time) ---> | 
   |                                    |
   | <---- App2 BLOCK (run time) ------ |
   |                                    |
   | ---- App2 ACK (current time) ----> |
   |                                    |
   | ---- App1 DONE (current time) ---> | 
   |                                    |
   | ---- App2 DONE (current time) ---> | 
```

