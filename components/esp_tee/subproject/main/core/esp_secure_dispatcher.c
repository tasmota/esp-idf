/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdarg.h>
#include "esp_log.h"
#include "esp_tee.h"
#include "secure_service_num.h"

#define ESP_TEE_MAX_INPUT_ARG 10

static const char *TAG = "esp_tee_sec_disp";

/**
 * @brief Entry point to the TEE binary during secure service call. It decipher the call and dispatch it
 *        to corresponding Secure Service API in secure world.
 * TODO: Fix the assembly routine here for compatibility with all levels of compiler optimizations
 */
#pragma GCC push_options
#pragma GCC optimize ("Og")

int esp_tee_service_dispatcher(int argc, va_list ap)
{
    if (argc > ESP_TEE_MAX_INPUT_ARG) {
        ESP_LOGE(TAG, "Input arguments overflow! Received %d, Permitted %d",
                 argc, ESP_TEE_MAX_INPUT_ARG);
        return -1;
    }

    int ret = -1;
    void *fp_secure_service;
    uint32_t argv[ESP_TEE_MAX_INPUT_ARG], *argp;

    uint32_t sid = va_arg(ap, uint32_t);
    argc--;

    if (sid >= MAX_SECURE_SERVICES) {
        ESP_LOGE(TAG, "Invalid Service ID!");
        va_end(ap);
        return -1;
    }

    fp_secure_service = (void *)tee_secure_service_table[sid];

    for (int i = 0; i < argc; i++) {
        argv[i] = va_arg(ap, uint32_t);
    }
    argp = &argv[0];
    va_end(ap);

    asm volatile(
        "mv t0, %1 \n"
        "beqz t0, service_call \n"

        "lw a0, 0(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a1, 4(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a2, 8(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a3, 12(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a4, 16(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a5, 20(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a6, 24(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "lw a7, 28(%3) \n"
        "addi t0, t0, -1 \n"
        "beqz t0, service_call \n"

        "addi %3, %3, 32 \n"
        "mv t2, sp \n"
        "loop: \n"
        "lw t1, 0(%3) \n"
        "sw t1, 0(t2) \n"
        "addi t0, t0, -1 \n"
        "addi t2, t2, 4 \n"
        "addi %3, %3, 4 \n"
        "bnez t0, loop \n"

        "service_call: \n"
        "mv t1, %2 \n"
        "jalr 0(t1) \n"
        "mv %0, a0 \n"
        : "=r"(ret)
        : "r"(argc), "r"(fp_secure_service), "r"(argp)
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2"
    );

    return ret;
}
#pragma GCC pop_options
