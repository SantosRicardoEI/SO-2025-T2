cmake_minimum_required(VERSION 3.25)
project(OSSIM C)

set(CMAKE_C_STANDARD 11)

add_executable(ossim ossim.c queue.c scheduler.c virtmem.c swap.c
        ossim.h)

add_executable(app-io app-io.c burst_queue.c)
