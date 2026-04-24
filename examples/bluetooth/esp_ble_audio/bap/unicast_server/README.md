| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BAP Unicast Server Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Basic Audio Profile (BAP) Unicast Server** functionality. It starts connectable extended advertising with the ASCS (Audio Stream Control Service) UUID and service data (targeted announcement, sink and source audio contexts), then waits for a BAP Unicast Client to connect. After connection, the server responds to client requests: codec configuration, QoS, enable, and start. The server starts sink (RX) streams when the client enables them; source (TX) streams are started by the client. The server receives audio on sink streams and can send on source streams. Run it together with the [unicast_client](../unicast_client) example on another device as the client.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-ISO, and ESP-BLE-AUDIO APIs (PACS, BAP unicast server register and callbacks, stream start, stream recv). It is intended for chips that support BLE 5.2 ISO and LE Audio (e.g. ESP32-H4). PACS advertises sink and source capabilities with LC3 (any frequency, 10 ms frame, up to 2 channels, various contexts). The number of sink and source ASEs is set via `CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT` and `CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT`.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Another device running the [unicast_client](../unicast_client) example, which scans for ASCS and connects to this server

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

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks. Register PACS (sink and source PAC and location), register the BAP unicast server (sink and source ASE counts), register unicast server callbacks (config, reconfig, QoS, enable, start, metadata, disable, stop, release), register PACS capabilities (LC3 sink and source), register stream callbacks for sink and source streams, set PACS location and supported/available contexts, then start the audio stack and set the device name.
2. **Advertising**: Start connectable extended advertising with flags, ASCS UUID, ASCS service data (targeted, sink/source contexts), and device name `bap_unicast_server`.
3. **Client connection**: When a client connects, ACL and optional security change are handled. On MTU exchange the server may start GATT service discovery (as needed for the stack).
4. **ASCS operations**: The client discovers ASEs, then sends codec config, QoS, enable, and start. The server allocates a stream per config (sink or source), returns QoS preference, validates enable (codec params), and on start resets stream counters. For sink streams, when the client enables the stream the server calls `esp_ble_audio_bap_stream_start`; for source streams the client starts them.
5. **Streaming**: When a sink stream is started, received ISO SDUs are delivered in the stream recv callback; the example counts valid, error, and lost packets and logs periodically. Source streams can send audio when started by the client.

## Example Output

```
I (xxx) BAP_USR: Extended adv instance 0 started
I (xxx) BAP_USR: connection established, status 0
I (xxx) BAP_USR: ASE Codec Config: conn 0x... ep 0x... dir ...
I (xxx) BAP_USR: Stream 0x... enabled
I (xxx) BAP_USR: Stream 0x... started
I (xxx) BAP_USR: Received 1000(1000/0/0) ISO data packets (stream 0x...)
...
```

If the client disconnects:

```
I (xxx) BAP_USR: connection disconnected, reason 0x...
```
