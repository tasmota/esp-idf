/*
 * SPDX-FileCopyrightText: 2023 NXP
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"

#include "sdkconfig.h"

#include "host/ble_hs.h"

#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_cap_api.h"
#include "esp_ble_audio_pacs_api.h"
#include "esp_ble_audio_tmap_api.h"
#include "esp_ble_audio_bap_lc3_preset_defs.h"

#include "ble_audio_example_init.h"
#include "ble_audio_example_utils.h"

#define TAG "TMAP_BMR"

int vcp_vol_renderer_init(void);

void bap_broadcast_scan_recv(esp_ble_audio_gap_app_event_t *event);

void bap_broadcast_pa_sync(esp_ble_audio_gap_app_event_t *event);

void bap_broadcast_pa_lost(esp_ble_audio_gap_app_event_t *event);

int bap_broadcast_sink_scan(void);

int bap_broadcast_sink_init(void);
