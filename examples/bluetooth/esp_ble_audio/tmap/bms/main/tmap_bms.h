/*
 * SPDX-FileCopyrightText: 2023 NXP
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"

#include "sdkconfig.h"

#include "host/ble_hs.h"

#include "esp_ble_audio_cap_api.h"
#include "esp_ble_audio_tmap_api.h"
#include "esp_ble_audio_bap_lc3_preset_defs.h"

#include "ble_audio_example_init.h"
#include "ble_audio_example_utils.h"

#define TAG "TMAP_BMS"

int cap_initiator_setup(void);

int cap_initiator_update(void);

int cap_initiator_stop(void);

int cap_initiator_init(void);
