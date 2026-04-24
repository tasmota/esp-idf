| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BAP Broadcast Source Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Basic Audio Profile (BAP) Broadcast Source** functionality. It starts extended advertising with the Broadcast Audio service data (UUID and Broadcast ID) and device name, periodic advertising with the Broadcast Audio Source Endpoint (BASE), then starts the BAP Broadcast Source so that BIGInfo and (mock) audio data are sent over the BIG. Sinks such as the [broadcast_sink](../broadcast_sink) example can discover this source, establish periodic sync, and synchronize to the BIG to receive the streams.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-ISO, and ESP-BLE-AUDIO APIs (BAP broadcast source create/start, stream send, LC3 presets). It is intended for chips that support BLE 5.2 ISO and LE Audio (e.g. ESP32-H4). The source is configured with the LC3 16_2_1 broadcast preset, a hardcoded broadcast ID (`0x123456`), and a hardcoded broadcast code (`1234`) for encrypted broadcasts. These values can be changed by editing the source code constants.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Optionally, a device running the [broadcast_sink](../broadcast_sink) example to receive and play the broadcast streams

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

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`). No PACS or GAP callback needed for the source role.
2. **Broadcast source setup**: Register broadcast source callbacks (started/stopped). Configure subgroups and streams using the selected LC3 preset (codec config, QoS, channel allocation). Create the BAP Broadcast Source (`esp_ble_audio_bap_broadcast_source_create`) with optional encryption and sequential packing. Register stream callbacks (started, stopped, sent) for each stream.
3. **Extended and periodic advertising**: Set extended advertising data: Broadcast Audio UUID + Broadcast ID (static or random) + complete device name. Set periodic advertising data: Broadcast Audio UUID + encoded BASE from the broadcast source. Start periodic advertising then extended advertising.
4. **Start BIG**: Add the advertising set for BIG and start the BAP Broadcast Source (`esp_ble_audio_bap_broadcast_source_start`) with the same adv handle. BIGInfo is sent in the periodic advertising; BIS streams are created.
5. **Stream and send**: When each BIS stream starts, the stream started callback allocates an SDU buffer and starts a periodic TX scheduler based on `k_work_delayable`. The scheduler posts work items to the ISO task at the stream QoS interval to send mock audio data; sent and drift are reported in the stream sent callback. When a stream stops, resources are freed and the scheduler is stopped. Note that the scheduler timer resolution is in milliseconds, which may not match the exact SDU interval for all configurations.

## Example Output

```
I (xxx) BAP_BSRC: Creating broadcast source with 1 subgroups & 2 streams per subgroup
I (xxx) BAP_BSRC: Extended adv instance 0 started
I (xxx) BAP_BSRC: Broadcast source 0x... started
I (xxx) BAP_BSRC: Stream 0x... started
I (xxx) BAP_BSRC: Transmitted 1000 ISO data packets (stream 0x...)
...
```

If the broadcast source stops (e.g. stream stopped):

```
I (xxx) BAP_BSRC: Stream 0x... stopped, reason 0x...
I (xxx) BAP_BSRC: Broadcast source 0x... stopped, reason 0x...
```
