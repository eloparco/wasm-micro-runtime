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

enum CONSTANTS {
    NUM_THREADS = 5,
    NUM_ITER = 30,
    STACK_SIZE = 128,
    SECOND = 1000 * 1000 * 1000, /* 1 second */
    TIMEOUT = 10LL * SECOND
};

typedef struct {
    char *stack;
    int th_done;
    int *count;
    int iteration;
    int *pval;
} shared_t;

int *vals[NUM_THREADS];

void
__wasi_thread_start_C(int thread_id, int *start_arg)
{
    shared_t *data = (shared_t *)start_arg;

    for (int i = 0; i < NUM_ITER; i++)
        __atomic_fetch_add(data->count, 1, __ATOMIC_SEQ_CST);

    vals[data->iteration] = malloc(sizeof(int));
    *vals[data->iteration] = data->iteration;

    data->th_done = 1;
    __builtin_wasm_memory_atomic_notify(&data->th_done, 1);
}

int
main(int argc, char **argv)
{
    shared_t data[NUM_THREADS] = { 0 };
    int thread_ids[NUM_THREADS];
    int *count = calloc(1, sizeof(int));

    for (int i = 0; i < NUM_THREADS; i++) {
        data[i].stack = malloc(STACK_SIZE);
        data[i].count = count;
        data[i].iteration = i;

        thread_ids[i] = __wasi_thread_spawn(&data[i]);
        assert(thread_ids[i] > 0 && "Thread creation failed");
    }

    printf("Wait for threads to finish\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        if (__builtin_wasm_memory_atomic_wait32(&data[i].th_done, 0, TIMEOUT)
            == 2) {
            assert("Wait should not time out");
        }

        free(data[i].stack);
    }

    assert(*count == (NUM_THREADS * NUM_ITER) && "Count not updated correctly");

    for (int i = 0; i < NUM_THREADS; i++) {
        printf("val=%d\n", *vals[i]);
        assert(*vals[i] == i && "Value not updated correctly");
        free(vals[i]);
    }

    free(count);
    return EXIT_SUCCESS;
}
