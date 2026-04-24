| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BAP Unicast Client Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Basic Audio Profile (BAP) Unicast Client** functionality. It scans for a BAP Unicast Server (advertising with the ASCS service data), connects to it, performs GATT service discovery and MTU exchange, then discovers sink and source ASEs, configures streams with an LC3 unicast preset, sets QoS, enables and connects the streams, and establishes unicast audio. For source (TX) streams the client starts the stream and can send audio data; sink streams are started by the server. Run it together with the [unicast_server](../unicast_server) example on another device as the peer.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-ISO, and ESP-BLE-AUDIO APIs (BAP unicast client discover, config, QoS, enable, connect, start; stream send). It is intended for chips that support BLE 5.2 ISO and LE Audio (e.g. ESP32-H4). The client uses the LC3 16_2_1 unicast preset and initiates pairing after the ACL connection.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* Another device running the [unicast_server](../unicast_server) example, which advertises with ASCS and acts as BAP Unicast Server

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

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks. Register stream ops for sink and source streams, register BAP unicast client callbacks, then start the audio stack.
2. **Scanning**: Start passive extended scanning. On scan report, parse advertising for connectable packets containing the ASCS (Audio Stream Control Service) service data; when found, initiate an ACL connection to that device.
3. **Connection and security**: On ACL connection, initiate pairing. After MTU exchange and GATT service discovery complete, start BAP discovery (sinks first, then sources).
4. **Discovery**: Discover sink ASEs then source ASEs on the server. For each discovered endpoint, the endpoint callback records the EP; when source discovery completes, start configuring streams.
5. **Configure, QoS, enable**: For each sink and source stream, configure with the LC3 16_2_1 codec config. When all streams are configured, create a unicast group, set QoS for the group, then enable each stream with metadata. When all streams are enabled, connect each stream.
6. **Connect and start**: When each stream is connected, connect the next; when all are connected, start the source (TX) streams. Sink streams are started by the server. When a TX stream is started, it is registered for sending; the example can then send audio on that stream.

## Example Output

```
I (xxx) BAP_UCL: Unicast server found, type ...
I (xxx) BAP_UCL: connection established, status 0
I (xxx) BAP_UCL: Sink #0: ep 0x...
I (xxx) BAP_UCL: Source #0: ep 0x...
I (xxx) BAP_UCL: Discover sinks complete
I (xxx) BAP_UCL: Discover sources complete
I (xxx) BAP_UCL: Sink stream[0] configured
I (xxx) BAP_UCL: Source stream[0] configured
I (xxx) BAP_UCL: Stream 0x... QoS set
I (xxx) BAP_UCL: Stream 0x... enabled
I (xxx) BAP_UCL: Stream 0x... connected
I (xxx) BAP_UCL: Stream 0x... started
...
```

If connection or discovery fails, relevant error messages are logged.
