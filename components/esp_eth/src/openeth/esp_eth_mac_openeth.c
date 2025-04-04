/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// This is a driver for OpenCores Ethernet MAC (https://opencores.org/projects/ethmac).
// Espressif chips do not use this MAC, but it is supported in QEMU
// (see hw/net/opencores_eth.c). Since the interface of this MAC is a relatively
// simple one, it is used for the purpose of running IDF apps in QEMU.
// The QEMU driver also emulates the DP83848C PHY, which is supported in IDF.
// Note that this driver is written with QEMU in mind. For example, it doesn't
// handle errors which QEMU will not report, and doesn't wait for TX to be
// finished, since QEMU does this instantly.

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_cpu.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "openeth.h"
#include "esp_mac.h"
#include "esp_eth_mac_openeth.h"

static const char *TAG = "opencores.emac";

// Driver state structure
typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    intr_handle_t intr_hdl;
    TaskHandle_t rx_task_hdl;
    int cur_rx_desc;
    int cur_tx_desc;
    uint8_t *rx_buf[RX_BUF_COUNT];
    uint8_t *tx_buf[TX_BUF_COUNT];
} emac_opencores_t;


// Interrupt handler and the receive task

static esp_err_t emac_opencores_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length);

static IRAM_ATTR void emac_opencores_isr_handler(void *args)
{
    emac_opencores_t *emac = (emac_opencores_t *) args;
    BaseType_t high_task_wakeup;

    uint32_t status = REG_READ(OPENETH_INT_SOURCE_REG);

    if (status & OPENETH_INT_RXB) {
        // Notify receive task
        vTaskNotifyGiveFromISR(emac->rx_task_hdl, &high_task_wakeup);
        if (high_task_wakeup) {
            portYIELD_FROM_ISR();
        }
    }

    if (status & OPENETH_INT_BUSY) {
        ESP_EARLY_LOGW(TAG, "%s: RX frame dropped (0x%" PRIx32 ")", __func__, status);
    }

    // Clear interrupt
    REG_WRITE(OPENETH_INT_SOURCE_REG, status);
}

static void emac_opencores_rx_task(void *arg)
{
    emac_opencores_t *emac = (emac_opencores_t *)arg;
    uint8_t *buffer = NULL;
    uint32_t length = 0;
    while (1) {
        if (ulTaskNotifyTake(pdFALSE, portMAX_DELAY)) {
            while (true) {
                length = ETH_MAX_PACKET_SIZE;
                buffer = malloc(length);
                if (!buffer) {
                    ESP_LOGE(TAG, "no mem for receive buffer");
                } else if (emac_opencores_receive(&emac->parent, buffer, &length) == ESP_OK) {
                    // pass the buffer to the upper layer
                    if (length) {
                        emac->eth->stack_input(emac->eth, buffer, length);
                    } else {
                        free(buffer);
                    }
                } else {
                    free(buffer);
                    break;
                }
            }
        }
    }
    vTaskDelete(NULL);
}


// Below functions implement the driver interface

static esp_err_t emac_opencores_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac's mediator to null");
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    emac->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    ESP_LOGV(TAG, "%s: addr=%" PRIu32 " reg=0x%" PRIx32 " val=0x%04" PRIx32, __func__, phy_addr, phy_reg, reg_value);
    REG_SET_FIELD(OPENETH_MIIADDRESS_REG, OPENETH_FIAD, phy_addr);
    REG_SET_FIELD(OPENETH_MIIADDRESS_REG, OPENETH_RGAD, phy_reg);
    REG_WRITE(OPENETH_MIITX_DATA_REG, reg_value & OPENETH_MII_DATA_MASK);
    REG_SET_BIT(OPENETH_MIICOMMAND_REG, OPENETH_WCTRLDATA);
    return ESP_OK;
}

static esp_err_t emac_opencores_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(reg_value, ESP_ERR_INVALID_ARG, err, TAG, "can't set reg_value to null");
    REG_SET_FIELD(OPENETH_MIIADDRESS_REG, OPENETH_FIAD, phy_addr);
    REG_SET_FIELD(OPENETH_MIIADDRESS_REG, OPENETH_RGAD, phy_reg);
    REG_SET_BIT(OPENETH_MIICOMMAND_REG, OPENETH_RSTAT);
    *reg_value = (REG_READ(OPENETH_MIIRX_DATA_REG) & OPENETH_MII_DATA_MASK);
    ESP_LOGV(TAG, "%s: addr=%" PRIu32 " reg=0x%" PRIx32 " val=0x%04" PRIx32, __func__, phy_addr, phy_reg, *reg_value);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    ESP_LOGV(TAG, "%s: " MACSTR, __func__, MAC2STR(addr));
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac addr to null");
    const uint8_t mac0[4] = {addr[5], addr[4], addr[3], addr[2]};
    const uint8_t mac1[4] = {addr[1], addr[0]};
    uint32_t mac0_u32, mac1_u32;
    memcpy(&mac0_u32, &mac0, 4);
    memcpy(&mac1_u32, &mac1, 4);
    REG_WRITE(OPENETH_MAC_ADDR0_REG, mac0_u32);
    REG_WRITE(OPENETH_MAC_ADDR1_REG, mac1_u32);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    ESP_LOGV(TAG, "%s: " MACSTR, __func__, MAC2STR(addr));
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac addr to null");
    uint32_t mac0_u32 = REG_READ(OPENETH_MAC_ADDR0_REG);
    uint32_t mac1_u32 = REG_READ(OPENETH_MAC_ADDR1_REG);
    const uint8_t mac_addr[ETH_ADDR_LEN] = {
        (mac1_u32 >> 8) & 0xFF,
        mac1_u32 & 0xFF,
        (mac0_u32 >> 24) & 0xFF,
        (mac0_u32 >> 16) & 0xFF,
        (mac0_u32 >> 8) & 0xFF,
        mac0_u32 & 0xFF,
    };
    memcpy(addr, mac_addr, ETH_ADDR_LEN);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    ESP_LOGV(TAG, "%s: %s", __func__, link == ETH_LINK_UP ? "up" : "down");
    esp_err_t ret = ESP_OK;
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    switch (link) {
    case ETH_LINK_UP:
        ESP_GOTO_ON_ERROR(esp_intr_enable(emac->intr_hdl), err, TAG, "enable interrupt failed");
        openeth_enable();
        break;
    case ETH_LINK_DOWN:
        ESP_GOTO_ON_ERROR(esp_intr_disable(emac->intr_hdl), err, TAG, "disable interrupt failed");
        openeth_disable();
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "unknown link status");
        break;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    /* QEMU doesn't emulate PHY speed, so accept any value */
    return ESP_OK;
}

static esp_err_t emac_opencores_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    /* QEMU doesn't emulate full/half duplex, so accept any value */
    return ESP_OK;
}

static esp_err_t emac_opencores_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    if (enable) {
        REG_SET_BIT(OPENETH_MODER_REG, OPENETH_PRO);
    } else {
        REG_CLR_BIT(OPENETH_MODER_REG, OPENETH_PRO);
    }
    return ESP_OK;
}

static esp_err_t emac_opencores_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable)
{
    /* QEMU doesn't emulate flow control function, so accept any value */
    return ESP_OK;
}

static esp_err_t emac_opencores_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability)
{
    /* QEMU doesn't emulate PAUSE function, so accept any value */
    return ESP_OK;
}

static esp_err_t emac_opencores_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    ESP_GOTO_ON_FALSE(length < DMA_BUF_SIZE * TX_BUF_COUNT, ESP_ERR_INVALID_SIZE, err, TAG, "insufficient TX buffer size");

    uint32_t bytes_remaining = length;
    // In QEMU, there never is a TX operation in progress, so start with descriptor 0.

    ESP_LOGV(TAG, "%s: len=%" PRIu32, __func__, length);
    while (bytes_remaining > 0) {
        uint32_t will_write = MIN(bytes_remaining, DMA_BUF_SIZE);
        memcpy(emac->tx_buf[emac->cur_tx_desc], buf, will_write);
        openeth_tx_desc_t *desc_ptr = openeth_tx_desc(emac->cur_tx_desc);
        openeth_tx_desc_t desc_val = *desc_ptr;
        desc_val.wr = (emac->cur_tx_desc == TX_BUF_COUNT - 1);
        desc_val.len = will_write;
        desc_val.rd = 1;
        // TXEN is already set, and this triggers a TX operation for the descriptor
        ESP_LOGV(TAG, "%s: desc %d (%p) len=%" PRIu32 " wr=%" PRIu16, __func__, emac->cur_tx_desc, desc_ptr, will_write, desc_val.wr);
        *desc_ptr = desc_val;
        bytes_remaining -= will_write;
        buf += will_write;
        emac->cur_tx_desc = (emac->cur_tx_desc + 1) % TX_BUF_COUNT;
    }

    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);

    openeth_rx_desc_t *desc_ptr = openeth_rx_desc(emac->cur_rx_desc);
    openeth_rx_desc_t desc_val = *desc_ptr;
    ESP_LOGV(TAG, "%s: desc %d (%p) e=%" PRIu16 " len=%" PRIu16" wr=%" PRIu16, __func__, emac->cur_rx_desc, desc_ptr, desc_val.e, desc_val.len, desc_val.wr);
    if (desc_val.e) {
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
    size_t rx_length = desc_val.len;
    ESP_GOTO_ON_FALSE(*length >= rx_length, ESP_ERR_INVALID_SIZE, err, TAG, "RX length too large");
    *length = rx_length;
    memcpy(buf, desc_val.rxpnt, *length);
    desc_val.e = 1;
    *desc_ptr = desc_val;

    emac->cur_rx_desc = (emac->cur_rx_desc + 1) % RX_BUF_COUNT;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_opencores_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL), err, TAG, "lowlevel init failed");

    // Sanity check
    if (REG_READ(OPENETH_MODER_REG) != OPENETH_MODER_DEFAULT) {
        ESP_LOGE(TAG, "CONFIG_ETH_USE_OPENETH should only be used when running in QEMU.");
        ESP_LOGE(TAG, "When running the app on the ESP32, use CONFIG_ETH_USE_ESP32_EMAC instead.");
        abort();
    }
    // Initialize the MAC
    openeth_reset();
    openeth_set_tx_desc_cnt(TX_BUF_COUNT);

    // Check if MAC address has been set in QEMU
    uint8_t mac_addr[ETH_ADDR_LEN];
    emac_opencores_get_addr(mac, mac_addr);
    const uint8_t zero_mac[ETH_ADDR_LEN] = {0};
    if (memcmp(mac_addr, zero_mac, ETH_ADDR_LEN) != 0) {
        ESP_LOGD(TAG, "Using MAC address " MACSTR " set in QEMU", MAC2STR(mac_addr));
    } else {
        // Fall back to the default MAC address
        ESP_GOTO_ON_ERROR(esp_read_mac(mac_addr, ESP_MAC_ETH), err, TAG, "fetch ethernet mac address failed");
        ESP_LOGD(TAG, "Using MAC address " MACSTR " from esp_read_mac", MAC2STR(mac_addr));
        emac_opencores_set_addr(mac, mac_addr);
    }

    return ESP_OK;
err:
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ret;
}

static esp_err_t emac_opencores_deinit(esp_eth_mac_t *mac)
{
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

static esp_err_t emac_opencores_start(esp_eth_mac_t *mac)
{
    openeth_enable();
    return ESP_OK;
}

static esp_err_t emac_opencores_stop(esp_eth_mac_t *mac)
{
    openeth_disable();
    return ESP_OK;
}

static esp_err_t emac_opencores_del(esp_eth_mac_t *mac)
{
    emac_opencores_t *emac = __containerof(mac, emac_opencores_t, parent);
    esp_intr_free(emac->intr_hdl);
    vTaskDelete(emac->rx_task_hdl);
    for (int i = 0; i < RX_BUF_COUNT; i++) {
        free(emac->rx_buf[i]);
    }
    for (int i = 0; i < TX_BUF_COUNT; i++) {
        free(emac->tx_buf[i]);
    }
    free(emac);
    return ESP_OK;
}

esp_eth_mac_t *esp_eth_mac_new_openeth(const eth_mac_config_t *config)
{
    esp_eth_mac_t *ret = NULL;
    emac_opencores_t *emac = NULL;
    ESP_GOTO_ON_FALSE(config, NULL, out, TAG, "can't set mac config to null");
    emac = calloc(1, sizeof(emac_opencores_t));
    ESP_GOTO_ON_FALSE(emac, NULL, out, TAG, "calloc emac failed");

    // Allocate DMA buffers
    for (int i = 0; i < RX_BUF_COUNT; i++) {
        emac->rx_buf[i] = heap_caps_calloc(1, DMA_BUF_SIZE, MALLOC_CAP_DMA);
        if (!(emac->rx_buf[i])) {
            goto out;
        }
        openeth_init_rx_desc(openeth_rx_desc(i), emac->rx_buf[i]);
    }
    openeth_rx_desc(RX_BUF_COUNT - 1)->wr = 1;
    emac->cur_rx_desc = 0;

    for (int i = 0; i < TX_BUF_COUNT; i++) {
        emac->tx_buf[i] = heap_caps_calloc(1, DMA_BUF_SIZE, MALLOC_CAP_DMA);
        if (!(emac->tx_buf[i])) {
            goto out;
        }
        openeth_init_tx_desc(openeth_tx_desc(i), emac->tx_buf[i]);
    }
    openeth_tx_desc(TX_BUF_COUNT - 1)->wr = 1;
    emac->cur_tx_desc = 0;

    emac->parent.set_mediator = emac_opencores_set_mediator;
    emac->parent.init = emac_opencores_init;
    emac->parent.deinit = emac_opencores_deinit;
    emac->parent.start = emac_opencores_start;
    emac->parent.stop = emac_opencores_stop;
    emac->parent.del = emac_opencores_del;
    emac->parent.write_phy_reg = emac_opencores_write_phy_reg;
    emac->parent.read_phy_reg = emac_opencores_read_phy_reg;
    emac->parent.set_addr = emac_opencores_set_addr;
    emac->parent.get_addr = emac_opencores_get_addr;
    emac->parent.set_speed = emac_opencores_set_speed;
    emac->parent.set_duplex = emac_opencores_set_duplex;
    emac->parent.set_link = emac_opencores_set_link;
    emac->parent.set_promiscuous = emac_opencores_set_promiscuous;
    emac->parent.set_peer_pause_ability = emac_opencores_set_peer_pause_ability;
    emac->parent.enable_flow_ctrl = emac_opencores_enable_flow_ctrl;
    emac->parent.transmit = emac_opencores_transmit;
    emac->parent.receive = emac_opencores_receive;

    // Initialize the interrupt
    ESP_GOTO_ON_FALSE(esp_intr_alloc(OPENETH_INTR_SOURCE, ESP_INTR_FLAG_IRAM, emac_opencores_isr_handler, emac, &(emac->intr_hdl)) == ESP_OK, NULL, out, TAG, "alloc emac interrupt failed");

    // Create the RX task
    BaseType_t core_num = tskNO_AFFINITY;
    if (config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
        core_num = esp_cpu_get_core_id();
    }
    BaseType_t xReturned = xTaskCreatePinnedToCore(emac_opencores_rx_task, "emac_rx", config->rx_task_stack_size, emac,
                           config->rx_task_prio, &emac->rx_task_hdl, core_num);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, NULL, out, TAG, "create emac_rx task failed");
    return &(emac->parent);

out:
    if (emac) {
        if (emac->rx_task_hdl) {
            vTaskDelete(emac->rx_task_hdl);
        }
        if (emac->intr_hdl) {
            esp_intr_free(emac->intr_hdl);
        }
        for (int i = 0; i < TX_BUF_COUNT; i++) {
            free(emac->tx_buf[i]);
        }
        for (int i = 0; i < RX_BUF_COUNT; i++) {
            free(emac->rx_buf[i]);
        }
        free(emac);
    }
    return ret;
}
