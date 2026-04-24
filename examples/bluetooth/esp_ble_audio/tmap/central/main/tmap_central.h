/*
 * SPDX-FileCopyrightText: 2023 NXP
 * SPDX-FileCopyrightText: 2024 Nordic Semiconductor ASA
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <errno.h>

#include "esp_log.h"

#include "sdkconfig.h"

#include "esp_ble_audio_lc3_defs.h"
#include "esp_ble_audio_aics_api.h"
#include "esp_ble_audio_cap_api.h"
#include "esp_ble_audio_pacs_api.h"
#include "esp_ble_audio_vcp_api.h"
#include "esp_ble_audio_tbs_api.h"
#include "esp_ble_audio_tmap_api.h"
#include "esp_ble_audio_csip_api.h"
#include "esp_ble_audio_mcs_defs.h"
#include "esp_ble_audio_mcc_api.h"
#include "esp_ble_audio_media_proxy_api.h"
#include "esp_ble_audio_vocs_api.h"
#include "esp_ble_audio_tbs_api.h"

#include "ble_audio_example_utils.h"

#define TAG "TMAP_CEN"

#define CONN_HANDLE_INIT    0xFFFF

uint16_t default_conn_handle_get(void);

int mcp_server_init(void);

int ccp_server_init(void);

int vcp_vol_ctlr_init(void);

int vcp_vol_ctlr_discover(void);

int cap_initiator_init(void);

int cap_initiator_setup(void);

int unicast_group_delete(void);
