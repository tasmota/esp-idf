| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# TMAP Broadcast Media Receiver (BMR) Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example implements the **Telephone and Media Audio Profile (TMAP) Broadcast Media Receiver (BMR)** role. The BMR scans for broadcast sources that advertise both the Broadcast Audio service (broadcast ID) and the TMAP role TMAS with **BMS (Broadcast Media Sender)**. When such a source is found, it establishes periodic advertising sync, creates a BAP Broadcast Sink, and synchronizes to the BIG to receive broadcast audio. It also registers as a **VCP Volume Renderer** (Volume Control Profile) for local volume and mute. The BMR can work with a TMAP BMS or any BAP Broadcast Source that includes the TMAP BMS role in advertising (e.g. a device running the [broadcast_source](../../bap/broadcast_source) or [CAP initiator](../../cap/initiator) in broadcast mode, if configured as BMS).

The implementation uses the NimBLE host stack with ISO and LE Audio support, ESP-BLE-AUDIO (TMAP BMR role, VCP volume renderer, PACS sink, BAP broadcast sink, BAP scan delegator). PACS advertises LC3 sink capability (e.g. 48 kHz, 10 ms, mono, media context). After startup the device starts scanning; on matching scan results it creates PA sync, then creates the broadcast sink and syncs to the BIS streams; received audio is counted in the stream recv callback.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* A broadcast source that advertises as TMAP BMS (Broadcast Media Sender) and sends broadcast audio (e.g. another board running a broadcast source example with TMAP BMS role)

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

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP callback. Register TMAP role BMR (`esp_ble_audio_tmap_register(ESP_BLE_AUDIO_TMAP_ROLE_BMR)`). Register VCP Volume Renderer (volume step, mute, volume, state/flags callbacks). Initialize BAP broadcast sink: register PACS (sink only), register broadcast sink callbacks (base_recv, syncable), register PACS sink capability (LC3), register scan delegator callbacks, set stream ops (started, stopped, recv). Start the audio stack.
2. **Scanning**: Start extended scanning. On scan result, parse for Broadcast Audio UUID (broadcast ID) and TMAS with BMS role; if both found and not already syncing, create periodic advertising sync with the source.
3. **PA sync**: When PA sync is established, stop scanning, create the BAP broadcast sink for the sync handle and broadcast ID. When BASE is received (base_recv), extract BIS indexes; when syncable callback is invoked, sync to the BIG with the chosen BIS and stream pointers.
4. **Streaming**: When streams start, reset packet counters. Received ISO SDUs are delivered in the stream recv callback; the example counts valid, error, lost, and zero-length SDU and logs periodically.
5. **VCP**: The Volume Renderer exposes volume and mute state; VCS state/flags callbacks log volume and mute changes when queried.

## Example Output

```
I (xxx) TMAP_BMR: Found TMAP BMS
I (xxx) TMAP_BMR: PA synced, handle 0x... status 0x00
I (xxx) TMAP_BMR: PA sync ... synced with broadcast ID 0x...
I (xxx) TMAP_BMR: Broadcast source PA synced, waiting for BASE
I (xxx) TMAP_BMR: BASE received, creating broadcast sink
I (xxx) TMAP_BMR: Stream 0x... started
I (xxx) TMAP_BMR: Received 1000(...) ISO data packets (stream 0x...)
...
```

If PA sync is lost:

```
I (xxx) TMAP_BMR: PA sync lost, handle 0x... reason ...
```
