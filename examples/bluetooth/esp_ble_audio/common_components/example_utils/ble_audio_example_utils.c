/*
 * SPDX-FileCopyrightText: 2021-2024 Nordic Semiconductor ASA
 * SPDX-FileCopyrightText: 2024 Demant A/S
 * SPDX-FileCopyrightText: 2015-2016 Intel Corporation
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "esp_log.h"

#include "ble_audio_example_utils.h"

#include "esp_ble_audio_common_api.h"

#define TAG         "UTILS"

#define LOG_GREEN   "\033[0;" "32" "m"
#define LOG_RESET   "\033[0m"

int example_audio_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_EXT_DISC ||
            event->type == BLE_GAP_EVENT_PERIODIC_SYNC ||
            event->type == BLE_GAP_EVENT_PERIODIC_REPORT ||
            event->type == BLE_GAP_EVENT_PERIODIC_SYNC_LOST ||
            event->type == BLE_GAP_EVENT_CONNECT ||
            event->type == BLE_GAP_EVENT_DISCONNECT ||
            event->type == BLE_GAP_EVENT_ENC_CHANGE) {
        esp_ble_audio_gap_app_post_event(event->type, event);
    } else if (event->type == BLE_GAP_EVENT_MTU ||
               event->type == BLE_GAP_EVENT_NOTIFY_RX ||
               event->type == BLE_GAP_EVENT_NOTIFY_TX ||
               event->type == BLE_GAP_EVENT_SUBSCRIBE) {
        esp_ble_audio_gatt_app_post_event(event->type, event);
    }

    return 0;
}

static void print_hex(const uint8_t *ptr, size_t len)
{
    while (len--) {
        printf(LOG_GREEN "%02x" LOG_RESET, *ptr++);
    }
}

static bool print_cb(uint8_t type, const uint8_t *data,
                     uint8_t data_len, void *user_data)
{
    const char *str = (const char *)user_data;

    printf(LOG_GREEN "I (%lu) %s: %s, type 0x%02x len %u data " LOG_RESET,
           esp_log_timestamp(), TAG, str, type, data_len);
    print_hex(data, data_len);
    printf("\n");

    return true;
}

void example_print_codec_cfg(const esp_ble_audio_codec_cfg_t *codec_cfg)
{
    esp_err_t err;

    ESP_LOGI(TAG, "codec_cfg, id 0x%02x cid 0x%04x vid 0x%04x count %u",
             codec_cfg->id, codec_cfg->cid,
             codec_cfg->vid, codec_cfg->data_len);

    if (codec_cfg->id == ESP_BLE_ISO_CODING_FORMAT_LC3) {
        esp_ble_audio_codec_cfg_frame_dur_t frame_dur;
        esp_ble_audio_location_t chan_allocation;
        esp_ble_audio_codec_cfg_freq_t freq;
        uint16_t octets_per_frame;
        uint32_t frame_dur_us;
        uint8_t frame_blocks;
        uint32_t freq_hz;

        /* LC3 uses the generic LTV format - other codecs might do as well */

        err = esp_ble_audio_data_parse(codec_cfg->data, codec_cfg->data_len, print_cb, "data");
        if (err) {
            ESP_LOGE(TAG, "Failed to parse codec_cfg data");
            return;
        }

        err = esp_ble_audio_codec_cfg_get_freq(codec_cfg, &freq);
        if (err) {
            ESP_LOGE(TAG, "Failed to get frequency");
            return;
        }

        err = esp_ble_audio_codec_cfg_freq_to_freq_hz(freq, &freq_hz);
        if (err) {
            ESP_LOGE(TAG, "Failed to get frequency hz");
            return;
        }

        ESP_LOGI(TAG, "Frequency: %lu Hz", freq_hz);

        err = esp_ble_audio_codec_cfg_get_frame_dur(codec_cfg, &frame_dur);
        if (err) {
            ESP_LOGE(TAG, "Failed to get frame duration");
            return;
        }

        err = esp_ble_audio_codec_cfg_frame_dur_to_frame_dur_us(frame_dur, &frame_dur_us);
        if (err) {
            ESP_LOGE(TAG, "Failed to get frame duration us");
            return;
        }

        ESP_LOGI(TAG, "Frame Duration: %lu us", frame_dur_us);

        err = esp_ble_audio_codec_cfg_get_chan_allocation(codec_cfg, &chan_allocation, false);
        if (err) {
            ESP_LOGE(TAG, "Failed to get channel allocation");
            return;
        }

        ESP_LOGI(TAG, "Channel allocation: 0x%08lx", chan_allocation);

        err = esp_ble_audio_codec_cfg_get_octets_per_frame(codec_cfg, &octets_per_frame);
        if (err) {
            ESP_LOGE(TAG, "Failed to get octets per frame");
            return;
        }

        ESP_LOGI(TAG, "Octets per frame: %u", octets_per_frame);

        err = esp_ble_audio_codec_cfg_get_frame_blocks_per_sdu(codec_cfg, &frame_blocks, true);
        if (err) {
            ESP_LOGE(TAG, "Failed to get frame blocks per sdu");
            return;
        }

        ESP_LOGI(TAG, "Frames per SDU: %u", frame_blocks);
    } else {
        print_hex(codec_cfg->data, codec_cfg->data_len);
    }

    err = esp_ble_audio_data_parse(codec_cfg->meta, codec_cfg->meta_len, print_cb, "meta");
    if (err) {
        ESP_LOGE(TAG, "Failed to parse codec_cfg meta");
        return;
    }
}

void example_print_codec_cap(const esp_ble_audio_codec_cap_t *codec_cap)
{
    esp_err_t err;

    ESP_LOGI(TAG, "codec_cap, id 0x%02x cid 0x%04x vid 0x%04x count %u",
             codec_cap->id, codec_cap->cid,
             codec_cap->vid, codec_cap->data_len);

    if (codec_cap->id == ESP_BLE_ISO_CODING_FORMAT_LC3) {
        err = esp_ble_audio_data_parse(codec_cap->data, codec_cap->data_len, print_cb, "data");
        if (err) {
            ESP_LOGE(TAG, "Failed to parse codec_cap data");
            return;
        }
    } else {
        printf(LOG_GREEN "I (%lu) %s: data: " LOG_RESET, esp_log_timestamp(), TAG);
        print_hex(codec_cap->data, codec_cap->data_len);
        printf("\n");
    }

    err = esp_ble_audio_data_parse(codec_cap->meta, codec_cap->meta_len, print_cb, "meta");
    if (err) {
        ESP_LOGE(TAG, "Failed to parse codec_cap meta");
        return;
    }
}

void example_print_qos(const esp_ble_audio_bap_qos_cfg_t *qos)
{
    ESP_LOGI(TAG, "QoS: interval %u framing 0x%02x phy 0x%02x", qos->interval, qos->framing, qos->phy);
    ESP_LOGI(TAG, "     sdu %u rtn %u latency %u pd %u", qos->sdu, qos->rtn, qos->latency, qos->pd);
}

void example_print_qos_pref(const esp_ble_audio_bap_qos_cfg_pref_t *pref)
{
    ESP_LOGI(TAG, "QoS pref: unframed %s, phy %u, rtn %u, latency %u",
             pref->unframed_supported ? "supported" : "not supported",
             pref->phy, pref->rtn, pref->latency);
    ESP_LOGI(TAG, "          pd_min %u, pd_max %u",
             pref->pd_min, pref->pd_max);
    ESP_LOGI(TAG, "          pref_pd_min %u, pref_pd_max %u",
             pref->pref_pd_min, pref->pref_pd_max);
}

bool example_is_substring(const char *substr, const char *str)
{
    const size_t sub_str_len = strlen(substr);
    const size_t str_len = strlen(str);

    if (sub_str_len > str_len) {
        return false;
    }

    for (size_t pos = 0; pos < str_len; pos++) {
        if (pos + sub_str_len > str_len) {
            return false;
        }

        if (strncasecmp(substr, &str[pos], sub_str_len) == 0) {
            return true;
        }
    }

    return false;
}

static void example_audio_tx_work_handler(struct k_work *work)
{
    example_audio_tx_scheduler_t *scheduler = work->user_data;
    size_t count;

    assert(scheduler);
    assert(scheduler->cb);

    count = 1 + scheduler->drift;
    scheduler->drift = 0;

    for (size_t i = 0; i < count; i++) {
        scheduler->cb(scheduler->arg);
    }
}

int example_audio_tx_scheduler_init(example_audio_tx_scheduler_t *scheduler,
                                    example_audio_tx_send_cb_t cb,
                                    void *arg)
{
    assert(scheduler);
    assert(cb);

    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->cb = cb;
    scheduler->arg = arg;

    k_work_init_delayable(&scheduler->timer, example_audio_tx_work_handler);
    scheduler->timer.work.user_data = scheduler;

    return 0;
}

void example_audio_tx_scheduler_reset(example_audio_tx_scheduler_t *scheduler)
{
    assert(scheduler);

    scheduler->drift = 0;
    scheduler->count = 0;
}

int example_audio_tx_scheduler_start(example_audio_tx_scheduler_t *scheduler,
                                     uint64_t period_us)
{
    assert(scheduler);
    assert(period_us > 0);

    return k_work_schedule_periodic(&scheduler->timer, (uint32_t)(period_us / 1000));
}

int example_audio_tx_scheduler_stop(example_audio_tx_scheduler_t *scheduler)
{
    assert(scheduler);

    return k_work_cancel_delayable(&scheduler->timer);
}

void example_audio_tx_scheduler_on_sent(example_audio_tx_scheduler_t *scheduler,
                                        const esp_ble_iso_tx_cb_info_t *info,
                                        const char *tag,
                                        const char *obj_name,
                                        const void *obj)
{
    size_t drift_count = 0;

    assert(scheduler);
    assert(info);

    for (size_t i = 0; i < info->pkt_cnt; i++) {
        if (info->pkt[i].drift) {
            drift_count++;
        }
    }

    scheduler->drift += drift_count;
    scheduler->count++;

    if (scheduler->count % 1000 == 0) {
        ESP_LOGI(tag, "Transmitted %u ISO data packets (%s %p)",
                 scheduler->count, (obj_name ? obj_name : ""), obj);
    }
}

void example_audio_rx_metrics_reset(example_audio_rx_metrics_t *metrics)
{
    assert(metrics);

    metrics->recv_count = 0;
    metrics->valid_count = 0;
    metrics->error_count = 0;
    metrics->lost_count = 0;
    metrics->null_sdu_count = 0;
    metrics->last_sdu_len = 0;
}

void example_audio_rx_metrics_on_recv(const esp_ble_iso_recv_info_t *info,
                                      example_audio_rx_metrics_t *metrics,
                                      const char *tag,
                                      const char *obj_name,
                                      const void *obj)
{
    assert(info);
    assert(metrics);

    if (info->flags & ESP_BLE_ISO_FLAGS_ERROR) {
        metrics->error_count++;
    }

    if (info->flags & ESP_BLE_ISO_FLAGS_LOST) {
        metrics->lost_count++;
    }

    if (info->flags & ESP_BLE_ISO_FLAGS_VALID) {
        metrics->valid_count++;
        if (metrics->last_sdu_len == 0) {
            metrics->null_sdu_count++;
        }
    }

    metrics->recv_count++;

    if (metrics->recv_count % 1000 == 0) {
        ESP_LOGI(tag, "Received %u ISO data packets (%s %p)",
                 metrics->recv_count, (obj_name ? obj_name : ""), obj);
    }
}
