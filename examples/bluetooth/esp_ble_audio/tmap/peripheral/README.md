| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# TMAP Peripheral Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example implements the **Telephone and Media Audio Profile (TMAP) Peripheral** roles: **Call Terminal (CT)** and **Unicast Media Receiver (UMR)**. The peripheral advertises connectable extended advertising with ASCS, CAS, and **TMAS (Telephone and Media Audio Service)** with **UMR and CT** roles, plus targeted unicast announcement (sink/source contexts) and device name; optionally CSIS RSI when built as a set member (e.g. duo earbuds). A TMAP Central (e.g. the [central](../central) example) that scans for TMAS UMR will discover and connect to this device. After connection, the central performs TMAP discovery; the peripheral also runs TMAP discovery to detect peer roles (CG/UMS), then discovers TBS (Telephone Bearer Service) on the central for call control. The peripheral acts as **BAP Unicast Server** (responds to codec config, QoS, enable, start; receives/sends unicast audio), **VCP Volume Renderer** (local volume/mute), **TBS client** (originate/terminate calls via central), and **MCP controller** (Media Control). Run it together with the [central](../central) example on another device as the TMAP Central.

The implementation uses the NimBLE host stack with ISO and LE Audio support, ESP-BLE-AUDIO (TMAP CT+UMR, CAP acceptor, BAP unicast server, VCP volume renderer, TBS client, optional CSIP set member, MCP/MCC). Optional Kconfig: earbuds type (single / duo with set member), device rank, earbud location, device name.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Another device running the [central](../central) example as TMAP Central (CG + UMS)

## How to Use Example

Before project configuration and build, set the correct chip target:

```bash
idf.py set-target esp32h4
```

### Configure the Project

Open the configuration menu to set earbuds type, location, and device name:

```bash
idf.py menuconfig
```

Under **Example: TMAP Peripheral (CT & UMR)** you can set:

* **Earbuds type** — Single ear headset or Duo headset (duo enables CSIP set member and adds RSI to advertising)
* **Device rank in set** — Rank of this device in set (when duo is selected)
* **Earbud Location** — Left Ear or Right Ear

### Build and Flash

Run the following to build, flash and monitor:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF.

## Example Flow

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks. Register TMAP roles CT and UMR (`esp_ble_audio_tmap_register(ESP_BLE_AUDIO_TMAP_ROLE_CT | ESP_BLE_AUDIO_TMAP_ROLE_UMR)`). Optionally initialize CSIP set member and generate RSI for advertising. Initialize VCP volume renderer. Initialize BAP unicast server (PACS, ASCS callbacks, stream ops). Initialize CCP call control (TBS client). Initialize MCP controller. Start the audio stack (with optional CSIS instance when set member) and set the device name.
2. **Advertising**: Start connectable extended advertising with flags, appearance (earbud), ASCS/CAS/TMAS UUIDs, ASCS targeted + contexts, CAS targeted, TMAS with UMR|CT, device name, and optionally CSIS RSI.
3. **Central connection**: When the central connects, store the connection handle. On MTU change, start GATT service discovery. On discovery complete, start TMAP discovery (`tmap_discover_tmas`).
4. **TMAP discovery complete**: Store whether the peer is CG and/or UMS; start TBS discovery on the central (`ccp_discover_tbs`). The central will then run CAP initiator setup and configure unicast streams.
5. **Unicast**: The central configures and starts streams; the peripheral handles ASCS config/QoS/enable/start, starts sink streams when enabled, and receives/sends audio on stream callbacks.
6. **Call control**: The peripheral (TBS client) can originate or terminate calls via the central’s TBS (CCP server); discover and URI list read complete in callbacks.

## Example Output

```
I (xxx) TMAP_PER: Extended adv instance 0 started
I (xxx) TMAP_PER: connection established, status 0
I (xxx) TMAP_PER: gatt mtu change, conn_handle 1, mtu ...
I (xxx) TMAP_PER: gattc disc cmpl, status 0, conn_handle 1
I (xxx) TMAP_PER: TMAP discovery done
I (xxx) TMAP_PER: CCP: Discovered GTBS
...
I (xxx) TMAP_PER: Stream 0x... started
...
```

If the connection is lost:

```
I (xxx) TMAP_PER: connection disconnected, reason 0x...
```
