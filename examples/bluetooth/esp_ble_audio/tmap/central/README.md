| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# TMAP Central Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example implements the **Telephone and Media Audio Profile (TMAP) Central** roles: **Call Gateway (CG)** and **Unicast Media Sender (UMS)**. The central scans for devices that advertise the TMAS (Telephone and Media Audio Service) with the **UMR (Unicast Media Receiver)** role, connects to the first such device, performs pairing, MTU exchange, and GATT service discovery, then runs TMAP discovery and VCP discovery. After TMAP discovery completes, it sets up unicast audio streams (CAP initiator): discovers ASEs, configures codec (e.g. LC3 48_2_1), sets QoS, enables and connects streams, then starts them and sends audio to the peer’s sink. It also acts as **VCP Volume Controller** (volume/mute on the peer), **MCP server** (Media Control Profile / media proxy), and **CCP server** (Call Control Profile with TBS – Telephone Bearer Service) for call originate/terminate. Run it with a device that advertises TMAP UMR (e.g. a TMAP peripheral or CAP acceptor with unicast).

The implementation uses the NimBLE host stack with ISO and LE Audio support, ESP-BLE-AUDIO (TMAP CG+UMS, CAP initiator unicast client, VCP volume controller, TBS, CSIP set coordinator, MCP/MCS/MCC, media proxy). Optional Kconfig: device name.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* A device advertising TMAP UMR (Unicast Media Receiver), e.g. a CAP Acceptor or TMAP peripheral with unicast

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

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks. Register TMAP roles CG and UMS (`esp_ble_audio_tmap_register(ESP_BLE_AUDIO_TMAP_ROLE_CG | ESP_BLE_AUDIO_TMAP_ROLE_UMS)`). Initialize CAP initiator (unicast client, stream callbacks, TX timer). Initialize VCP volume controller. Initialize MCP server (media proxy). Initialize CCP server (TBS bearer, originate/terminate callbacks). Start the audio stack and set the device name.
2. **Scanning**: Start extended scanning. On scan result with connectable advertising, parse for TMAS with UMR role; when found, stop scan and connect to the peer.
3. **Connection**: On ACL connect, store connection handle and initiate pairing. On MTU change, start GATT service discovery. On discovery complete (and when MTU has been exchanged), start TMAP discovery and VCP discovery.
4. **TMAP discovery complete**: Call `cap_initiator_setup()` to create the unicast group, discover ASEs, configure codec, set QoS, enable and connect streams, then start streams.
5. **Unicast streaming**: When a sink-direction stream (TX from central to peer) is started, a periodic TX scheduler based on `k_work_delayable` runs in the ISO task context and sends ISO SDUs at the stream QoS interval; the sent callback logs packet counts. Source streams (RX) can be used to receive audio from the peer. Note that the scheduler timer resolution is in milliseconds, which may not match the exact SDU interval for all configurations.
6. **VCP**: After VCP discovery, the example can send mute/volume commands; state and flags callbacks log volume and mute.

## Example Output

```
I (xxx) TMAP_CEN: Found TMAS in peer adv data!
I (xxx) TMAP_CEN: connection established, status 0
I (xxx) TMAP_CEN: gatt mtu change, conn_handle 1, mtu ...
I (xxx) TMAP_CEN: gattc disc cmpl, status 0, conn_handle 1
I (xxx) TMAP_CEN: TMAP discovery done
I (xxx) TMAP_CEN: Configured stream 0x...
I (xxx) TMAP_CEN: Unicast stream 0x... started
I (xxx) TMAP_CEN: Transmitted 1000 ISO data packets (stream 0x...)
...
```

If the connection is lost:

```
I (xxx) TMAP_CEN: connection disconnected, reason 0x...
```
