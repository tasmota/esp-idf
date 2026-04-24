| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# TMAP Broadcast Media Sender (BMS) Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example implements the **Telephone and Media Audio Profile (TMAP) Broadcast Media Sender (BMS)** role. The BMS acts as a BAP Broadcast Source: it starts non-connectable extended advertising and periodic advertising. Extended advertising includes the **TMAS (Telephone and Media Audio Service)** UUID with the **BMS** role and the Broadcast Audio Announcement service (broadcast ID and device name); periodic advertising carries the BASE (Broadcast Audio Source Endpoint). After creating the broadcast source and starting the BIG, it sends broadcast audio on the BIS at the configured interval. TMAP BMR receivers (e.g. the [bmr](../bmr) example) that scan for both TMAS BMS and Broadcast Audio will discover and sync to this source.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-AUDIO (TMAP BMS role, CAP initiator broadcast). It creates a single subgroup with one stream using the LC3 48_2_1 broadcast preset (48 kHz, stereo, media context). A periodic TX scheduler (based on `k_work_delayable`) drives ISO SDU transmission in the ISO task context; the stream sent callback logs packet counts and drift. All parameters (broadcast ID, broadcast code, device name) are hardcoded in the source and can be changed by editing the source code constants.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Optionally, a TMAP BMR or BAP Broadcast Sink (e.g. the [bmr](../bmr) example) to receive the broadcast stream

## How to Use Example

Before project configuration and build, set the correct chip target:

```bash
idf.py set-target esp32h4
```

### Build and Flash

Run the following to build, flash and monitor:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF.

## Example Flow

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`). Register TMAP role BMS (`esp_ble_audio_tmap_register(ESP_BLE_AUDIO_TMAP_ROLE_BMS)`). Initialize CAP initiator broadcast: register stream ops (started, stopped, sent), create broadcast audio TX scheduler.
2. **Setup**: Create the broadcast source with one subgroup and one stream (LC3 48_2_1 preset), build extended adv data (TMAS + BMS role, Broadcast Audio UUID + broadcast ID, device name) and periodic adv data (BASE). Start periodic and extended advertising, add the ext adv to the BIG, then start the broadcast audio. The stream started callback starts the periodic TX scheduler and sends the first SDU.
3. **Streaming**: The TX scheduler, based on `k_work_delayable`, fires at the stream QoS interval in the ISO task context and sends ISO SDUs; the sent callback counts packets and logs periodically. Drift and untransmitted packets are reported in the callback. Note that the scheduler timer resolution is in milliseconds, which may not match the exact SDU interval for all configurations.
4. **Optional**: `cap_initiator_update` updates broadcast metadata; `cap_initiator_stop` stops and deletes the broadcast source.

## Example Output

```
I (xxx) TMAP_BMS: Creating broadcast source
I (xxx) TMAP_BMS: Extended adv instance 0 started
I (xxx) TMAP_BMS: Stream 0x... started
I (xxx) TMAP_BMS: Transmitted 1000 ISO data packets (stream 0x...)
...
```

If the controller reports drift or untransmitted packets:

```
W (xxx) TMAP_BMS: ISO data packets ... drifted and ... not txd
```
