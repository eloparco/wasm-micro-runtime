/*
 * Copyright (C) 2022 Amazon.com Inc. or its affiliates. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>

#include "wasi_thread_start.h"

typedef enum {
    BLOCKING_TASK_BUSY_WAIT,
    BLOCKING_TASK_ATOMIC_WAIT,
    BLOCKING_TASK_POLL_ONEOFF
} blocking_task_type_t;

/* Parameter to change test behavior */
static bool termination_by_trap;
static bool termination_in_main_thread;
static blocking_task_type_t blocking_task_type;

#define TIMEOUT_SECONDS 10ll
#define NUM_THREADS 3
static sem_t sem;

typedef struct {
    start_args_t base;
    bool throw_exception;
} shared_t;

void
run_long_task()
{
    if (blocking_task_type == BLOCKING_TASK_BUSY_WAIT) {
        for (int i = 0; i < TIMEOUT_SECONDS; i++)
            sleep(1);
    }
    else if (blocking_task_type == BLOCKING_TASK_ATOMIC_WAIT) {
        __builtin_wasm_memory_atomic_wait32(
            0, 0, TIMEOUT_SECONDS * 1000 * 1000 * 1000);
    }
    else {
        sleep(TIMEOUT_SECONDS);
    }
}

void
start_job()
{
    sem_post(&sem);
    run_long_task(); /* Wait to be interrupted */
    assert(false && "Unreachable");
}

void
terminate_process()
{
    /* Wait for all other threads (including main thread) to be ready */
    printf("Waiting before terminating\n");
    for (int i = 0; i < NUM_THREADS; i++)
        sem_wait(&sem);

    printf("Force termination\n");
    if (termination_by_trap)
        __builtin_trap();
    else
        __wasi_proc_exit(33);
}

void
__wasi_thread_start_C(int thread_id, int *start_arg)
{
    shared_t *data = (shared_t *)start_arg;

    if (data->throw_exception) {
        terminate_process();
    }
    else {
        printf("Thread running\n");
        start_job();
    }
}

void
test_termination(bool trap, bool main, blocking_task_type_t task_type)
{
    termination_by_trap = trap;
    termination_in_main_thread = main;
    blocking_task_type = task_type;

    int thread_id = -1, i;
    shared_t data[NUM_THREADS] = { 0 };
    assert(sem_init(&sem, 0, 0) == 0 && "Failed to init semaphore");

    for (i = 0; i < NUM_THREADS; i++) {
        /* No graceful memory free to simplify the test */
        assert(start_args_init(&data[i].base)
               && "Failed to allocate thread's stack");
    }

    /* Create a thread that forces termination through trap or `proc_exit` */
    data[0].throw_exception = !termination_in_main_thread;
    thread_id = __wasi_thread_spawn(&data[0]);
    assert(thread_id > 0 && "Failed to create thread");

    /* Create two additional threads to test exception propagation */
    data[1].throw_exception = false;
    thread_id = __wasi_thread_spawn(&data[1]);
    assert(thread_id > 0 && "Failed to create thread");
    data[2].throw_exception = false;
    thread_id = __wasi_thread_spawn(&data[2]);
    assert(thread_id > 0 && "Failed to create thread");

    if (termination_in_main_thread) {
        printf("Force termination (main thread)\n");
        terminate_process();
    }
    else {
        printf("Main thread running\n");
        start_job();
    }
}