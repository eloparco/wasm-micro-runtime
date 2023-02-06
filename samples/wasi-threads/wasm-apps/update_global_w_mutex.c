/*
 * Copyright (C) 2023 Amazon.com Inc. or its affiliates. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef __wasi__
#error This example only compiles to WASM/WASI target
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#include "wasi_thread_start.h"

enum CONSTANTS {
    MAX_NUM_THREADS = 4,
    NUM_ITER = 10,
    NUM_RETRY = 5,
    SECOND = 1000 * 1000 * 1000, /* 1 second */
    TIMEOUT = 10LL * SECOND
};

pthread_mutex_t mutex;
int g_count = 0;

typedef struct {
    start_args_t base;
    int th_done;
} shared_t;

void
__wasi_thread_start_C(int thread_id, int *start_arg)
{
    shared_t *data = (shared_t *)start_arg;

    for (int i = 0; i < NUM_ITER; i++) {
        pthread_mutex_lock(&mutex);
        g_count++;
        pthread_mutex_unlock(&mutex);
    }

    data->th_done = 1;
    __builtin_wasm_memory_atomic_notify(&data->th_done, 1);
}

int
main(int argc, char **argv)
{
    shared_t data[MAX_NUM_THREADS] = { 0 };
    int thread_ids[MAX_NUM_THREADS];

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Failed to init mutex.\n");
        return -1;
    }

    for (int i = 0; i < MAX_NUM_THREADS; i++) {
        assert(start_args_init(&data[i].base));
        thread_ids[i] = __wasi_thread_spawn(&data[i]);
        assert(thread_ids[i] > 0 && "Thread creation failed");
    }

    printf("Wait for threads to finish\n");
    for (int i = 0; i < MAX_NUM_THREADS; i++) {
        if (__builtin_wasm_memory_atomic_wait32(&data[i].th_done, 0, TIMEOUT)
            == 2) {
            assert("Wait should not time out");
        }

        start_args_deinit(&data[i].base);
    }

    printf("Value of count after update: %d\n", g_count);
    assert(g_count == (MAX_NUM_THREADS * NUM_ITER)
           && "Global count not updated correctly");

    if (pthread_mutex_destroy(&mutex) != 0) {
        printf("Failed to init mutex.\n");
        return -1;
    }

    return EXIT_SUCCESS;
}
