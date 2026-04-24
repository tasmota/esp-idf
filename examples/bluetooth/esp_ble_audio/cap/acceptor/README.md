| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# CAP Acceptor Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Common Audio Profile (CAP) Acceptor** functionality. It advertises so that a CAP Initiator can connect and set up available audio streams. The acceptor operates in one of two mutually exclusive modes selected at build time: as a **BAP Unicast Server** (unicast mode, for use with a CAP Initiator) or as a **BAP Broadcast Sink** (broadcast mode). In broadcast mode, it can be configured to scan for broadcast sources by itself, or to wait for a Broadcast Assistant to connect and direct the sync. Run it together with the [initiator](../initiator) example on another device as the CAP Initiator.

The implementation uses the NimBLE host stack with ISO and LE Audio support, ESP-BLE-AUDIO (PACS, CAP acceptor, BAP unicast server, BAP broadcast sink, BAP scan delegator). Advertising includes the CAS (Common Audio Service) UUID and service data; when unicast is enabled it also advertises ASCS with targeted announcement and sink/source contexts; when broadcast is enabled it can include BASS (Broadcast Assistant) service data. PACS advertises LC3 sink and source capabilities (e.g. 7.5 ms / 10 ms frame, stereo, multiple contexts). The example supports optional Kconfig: unicast, broadcast, self-scan for broadcast sources, device name, and (when self-scan) target broadcast device name and broadcast code.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Optionally, another device running the [initiator](../initiator) example as CAP Initiator (for unicast and/or as Broadcast Assistant)

## How to Use Example

Before project configuration and build, set the correct chip target:

```bash
idf.py set-target esp32h4
```

### Configure the Project

Open the configuration menu to select the mode:

```bash
idf.py menuconfig
```

Under **Example: CAP Acceptor**, select one of:

* **Unicast** — act as a BAP Unicast Server; start connectable advertising for a CAP Initiator
* **Broadcast** — act as a BAP Broadcast Sink; receive broadcast audio. When this mode is selected, you can additionally enable:
  * **Scan for Broadcast Sources without Broadcast Assistant** — start scanning for broadcast sources independently, without waiting for a Broadcast Assistant to connect

### Build and Flash

Run the following to build, flash and monitor:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF.

## Example Flow

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks. Register PACS (sink and source PAC and location), register sink and source capabilities (LC3), set PACS location and supported/available contexts (`init_cap_acceptor`). In unicast mode, initialize the CAP acceptor unicast part (BAP unicast server, stream alloc/release). In broadcast mode, initialize the CAP acceptor broadcast part (BAP broadcast sink, scan delegator). Start the audio stack and set the device name.
2. **Scan (optional)**: In broadcast mode with self-scan enabled, start scanning for broadcast sources before or in addition to advertising.
3. **Advertising**: Start connectable extended advertising with flags, CAS service data, and additionally ASCS service data in unicast mode or BASS service data in broadcast mode, plus device name (`cap_acceptor`).
4. **Initiator connection (unicast mode)**: When a CAP Initiator connects, the peer connection handle is stored. On MTU exchange the acceptor may start GATT service discovery. The initiator discovers ASEs and sets up unicast streams; the acceptor allocates streams and reports stream released when appropriate.
5. **Broadcast (broadcast mode)**: On PA sync events the acceptor syncs to the broadcast stream; on PA sync lost it handles disconnection. With self-scan, scan results are processed to sync to the chosen broadcast source.

## Example Output

```
I (xxx) CAP_ACC: Extended adv instance 0 started
I (xxx) CAP_ACC: connection established, status 0
I (xxx) CAP_ACC: gatt mtu change, conn_handle 1, mtu  ...
...
I (xxx) CAP_ACC: PA synced, handle 0x... status 0x00
...
I (xxx) CAP_ACC: Sink stream released
```

If the connection is lost:

```
I (xxx) CAP_ACC: connection disconnected, reason 0x...
I (xxx) CAP_ACC: PA sync lost, handle 0x... reason ...
```
