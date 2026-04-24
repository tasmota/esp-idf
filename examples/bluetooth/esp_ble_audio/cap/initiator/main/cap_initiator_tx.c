/*
 * SPDX-FileCopyrightText: 2024 Nordic Semiconductor ASA
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "cap_initiator.h"

static struct tx_stream tx_streams[IS_ENABLED(CONFIG_EXAMPLE_UNICAST) + IS_ENABLED(CONFIG_EXAMPLE_BROADCAST)];

static bool cap_stream_is_streaming(const esp_ble_audio_cap_stream_t *cap_stream)
{
    esp_ble_audio_bap_ep_info_t ep_info = {0};
    int err;

    if (cap_stream == NULL) {
        return false;
    }

    if (cap_stream->bap_stream.ep == NULL) {
        return false;
    }

    err = esp_ble_audio_bap_ep_get_info(cap_stream->bap_stream.ep, &ep_info);
    if (err) {
        return false;
    }

    return (ep_info.state == ESP_BLE_AUDIO_BAP_EP_STATE_STREAMING);
}

static void cap_initiator_tx_send(struct tx_stream *tx_stream)
{
    int err;

    if (tx_stream == NULL || tx_stream->stream == NULL) {
        return;
    }

    if (cap_stream_is_streaming(tx_stream->stream) == false) {
        return;
    }

    if (tx_stream->stream->bap_stream.qos == NULL ||
            tx_stream->stream->bap_stream.qos->sdu == 0) {
        ESP_LOGE(TAG, "Invalid stream qos");
        return;
    }

    if (tx_stream->data == NULL) {
        ESP_LOGE(TAG, "TX buffer unavailable, SDU %u (stream %p)",
                 tx_stream->stream->bap_stream.qos->sdu, &tx_stream->stream->bap_stream);
        return;
    }

    memset(tx_stream->data, (uint8_t)tx_stream->seq_num, tx_stream->stream->bap_stream.qos->sdu);

    err = esp_ble_audio_cap_stream_send(tx_stream->stream, tx_stream->data,
                                        tx_stream->stream->bap_stream.qos->sdu,
                                        tx_stream->seq_num);
    if (err) {
        ESP_LOGD(TAG, "Failed to transmit data on stream %p, err %d",
                 &tx_stream->stream->bap_stream, err);
        return;
    }

    tx_stream->seq_num++;
}

void cap_initiator_tx_stream_sent(esp_ble_audio_bap_stream_t *stream, void *user_data)
{
    for (size_t i = 0; i < ARRAY_SIZE(tx_streams); i++) {
        if (tx_streams[i].stream && &tx_streams[i].stream->bap_stream == stream) {
            example_audio_tx_scheduler_on_sent(&tx_streams[i].scheduler, user_data, TAG, "stream", stream);
            break;
        }
    }
}

static void tx_scheduler_cb(void *arg)
{
    struct tx_stream *tx_stream = arg;

    cap_initiator_tx_send(tx_stream);
}

int cap_initiator_tx_register_stream(esp_ble_audio_cap_stream_t *cap_stream)
{
    int err;

    if (cap_stream == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ARRAY_SIZE(tx_streams); i++) {
        if (tx_streams[i].stream == NULL) {
            if (cap_stream->bap_stream.qos == NULL || cap_stream->bap_stream.qos->sdu == 0) {
                ESP_LOGE(TAG, "Invalid stream qos");
                return -EINVAL;
            }

            if (tx_streams[i].data == NULL) {
                tx_streams[i].data = calloc(1, cap_stream->bap_stream.qos->sdu);
                if (tx_streams[i].data == NULL) {
                    ESP_LOGE(TAG, "Failed to alloc TX buffer, SDU %u (stream %p)",
                             cap_stream->bap_stream.qos->sdu, &cap_stream->bap_stream);
                    return -ENOMEM;
                }
            }

            tx_streams[i].stream = cap_stream;
            tx_streams[i].seq_num = 0;
            example_audio_tx_scheduler_reset(&tx_streams[i].scheduler);

            err = example_audio_tx_scheduler_start(&tx_streams[i].scheduler, cap_stream->bap_stream.qos->interval);
            if (err) {
                ESP_LOGE(TAG, "Failed to start tx scheduler, err %d", err);
                tx_streams[i].stream = NULL;
                return err;
            }

            cap_initiator_tx_send(&tx_streams[i]);
            return 0;
        }
    }

    ESP_LOGE(TAG, "No free TX stream slot");

    return -ENOMEM;
}

int cap_initiator_tx_unregister_stream(esp_ble_audio_cap_stream_t *cap_stream)
{
    int err;

    if (cap_stream == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ARRAY_SIZE(tx_streams); i++) {
        if (tx_streams[i].stream == cap_stream) {
            err = example_audio_tx_scheduler_stop(&tx_streams[i].scheduler);
            if (err) {
                ESP_LOGE(TAG, "Failed to stop tx scheduler, err %d", err);
                return err;
            }

            tx_streams[i].stream = NULL;
            if (tx_streams[i].data != NULL) {
                free(tx_streams[i].data);
                tx_streams[i].data = NULL;
            }

            return 0;
        }
    }

    return -ENODATA;
}

void cap_initiator_tx_init(void)
{
    int err;

    memset(tx_streams, 0, sizeof(tx_streams));

    for (size_t i = 0; i < ARRAY_SIZE(tx_streams); i++) {
        err = example_audio_tx_scheduler_init(&tx_streams[i].scheduler,
                                              tx_scheduler_cb,
                                              &tx_streams[i]);
        if (err) {
            ESP_LOGE(TAG, "Failed to initialize tx scheduler[%u], err %d", i, err);
            return;
        }
    }
}
