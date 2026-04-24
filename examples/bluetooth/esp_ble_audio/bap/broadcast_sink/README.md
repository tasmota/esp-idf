| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BAP Broadcast Sink Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Basic Audio Profile (BAP) Broadcast Sink** functionality. It scans for BAP Broadcast Sources (advertising with the Broadcast Audio service data), establishes periodic advertising synchronization with the first matching source, creates a BAP Broadcast Sink, and synchronizes to the BIG to receive broadcast audio streams. It listens until the source stops or sync is lost. The scan filters by the hardcoded source name (`BAP Broadcast Source`). Optionally, you can run in a mode where a Broadcast Assistant (e.g. a phone app) controls when to sync via the Scan Delegator.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-ISO, and ESP-BLE-AUDIO APIs (PACS, BAP broadcast sink, scan delegator). It is intended for chips that support BLE 5.2 ISO and LE Audio (e.g. ESP32-H4). Sink capabilities are registered with LC3 (16 kHz and 24 kHz, 10 ms frame duration, 1 channel). For encrypted broadcasts, the broadcast code is hardcoded as `1234` in the source, or can be provided by a Broadcast Assistant.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* A BAP Broadcast Source (e.g. another device or sample running as broadcast source) that is advertising and sending broadcast audio

## How to Use Example

Before project configuration and build, set the correct chip target:

```bash
idf.py set-target esp32h4
```

### Configure the Project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the **Example: Broadcast Sink** menu:

* **Whether to wait for a Broadcast Assistant**: If enabled, the device advertises connectable and waits for a Broadcast Assistant to connect and send PA sync, broadcast code, and BIS sync requests via the Scan Delegator; the sink then syncs according to those requests.

### Build and Flash

Run the following to build, flash and monitor:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF.

## Example Flow

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`). Register PACS with sink capabilities (LC3), register BAP scan delegator callbacks and broadcast sink callbacks, then start the audio stack. Set device name if needed.
2. **Scanning**: Start extended scanning. On scan report, look for Broadcast Audio service data (UUID + broadcast ID). Filter by the hardcoded target device name (`BAP Broadcast Source`). When a matching advertiser with periodic advertising is found and not already syncing, create periodic advertising synchronization.
3. **PA sync**: When periodic sync is established, create a BAP Broadcast Sink (`esp_ble_audio_bap_broadcast_sink_create`) for that sync handle and broadcast ID.
4. **BASE and syncable**: When the BASE is received, the sink gets subgroup count and BIS index bitfield. When BIGInfo indicates the sink is syncable, call `esp_ble_audio_bap_broadcast_sink_sync` with the chosen BIS bitfield and broadcast code (from menuconfig or from the scan delegator). Streams are bound to the registered stream array.
5. **Streams**: As each BIS stream starts, the stream started callback runs; when all requested streams have started, the sink is considered running. Incoming audio data is delivered in the stream receive callback; the example counts valid, error, and lost packets per stream and logs periodically. When streams stop or PA sync is lost, the sink is cleaned up.

## Example Output

```
I (xxx) BAP_BSNK: Broadcast source PA synced, creating Broadcast Sink
I (xxx) BAP_BSNK: Received BASE with 1 subgroups from broadcast sink 0x...
I (xxx) BAP_BSNK: Broadcast sink (0x...) is syncable, BIG not encrypted
I (xxx) BAP_BSNK: Stream 0x... started (1/1)
I (xxx) BAP_BSNK: Received 1000(1000/0/0) ISO data packets (stream 0x...)
...
```

If PA sync is lost:

```
I (xxx) BAP_BSNK: PA sync lost, reason ...
```
