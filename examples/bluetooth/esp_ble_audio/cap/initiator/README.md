| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# CAP Initiator Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Common Audio Profile (CAP) Initiator** functionality. It operates in one of two mutually exclusive modes selected at build time: **unicast** or **broadcast**. In unicast mode, the initiator scans for connectable advertising that includes the CAS/ASCS service data, connects to the acceptor, performs service discovery and MTU exchange, discovers sink and source ASEs, configures streams with an LC3 16_2_1 unicast preset, sets QoS, enables and connects the streams, then starts the streams and can send or receive audio. In broadcast mode, the initiator starts extended and periodic advertising with the Broadcast Audio Announcement service data and BASE, so that Broadcast Sinks (e.g. the [acceptor](../acceptor) in broadcast mode) can sync. Run it together with the [acceptor](../acceptor) example on another device as the CAP Acceptor.

The implementation uses the NimBLE host stack with ISO and BAP support, ESP-BLE-AUDIO (CAP initiator, BAP unicast client, BAP broadcast source). Unicast uses the LC3 16_2_1 preset and initiates pairing after connection. Broadcast uses an LC3 16_2_1 broadcast preset with a hardcoded broadcast ID. The device name is hardcoded as `CAP Initiator`.

## Requirements

* A board with Bluetooth LE 5.2, ISO, and LE Audio support (e.g. ESP32-H4)
* For unicast or as Broadcast Assistant: another device running the [acceptor](../acceptor) example (advertising as CAP Acceptor)

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

Under **Example: CAP Initiator**, select one of:

* **Unicast** — scan for a CAP Acceptor and set up unicast audio streams after connection
* **Broadcast** — act as a Broadcast Source; start extended and periodic advertising so that Broadcast Sinks can sync

### Build and Flash

Run the following to build, flash and monitor:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF.

## Example Flow

1. **Initialization**: NVS, Bluetooth stack, and LE Audio common layer (`esp_ble_audio_common_init`) with GAP and GATT callbacks (unicast mode). In unicast mode, initialize the CAP initiator unicast part (BAP unicast client, CAP unicast group, stream callbacks). In broadcast mode, initialize the CAP initiator broadcast part (broadcast source, stream ops). Register the TX module for sending audio. Start the audio stack and set the device name (`CAP Initiator`).
2. **Unicast mode**: Start extended scanning. On scan result with connectable advertising and CAS/ASCS service data, connect to the acceptor. On connection, start pairing. On MTU exchange, start GATT service discovery. After discovery, discover ASEs, configure codec (LC3 16_2_1), set QoS, enable and connect streams, then start streams; the initiator registers the TX stream for sending and can receive on sink streams.
3. **Broadcast mode**: Start extended and periodic advertising with Broadcast Audio Announcement UUID, hardcoded broadcast ID, and BASE. When the broadcast stream is started, register it for TX and send audio. Sinks (e.g. acceptor in broadcast mode) can sync to this broadcast.
4. **Stream lifecycle**: Stream started/stopped callbacks register/unregister streams with the TX module; sent callbacks log periodic packet counts.

## Example Output

```
I (xxx) CAP_INI: connection established, status 0
I (xxx) CAP_INI: Configured stream 0x...
I (xxx) CAP_INI: Started stream 0x...
...
I (xxx) CAP_INI: Started broadcast stream 0x...
I (xxx) CAP_INI: Sent 1000 HCI ISO data packets
...
```

If the connection is lost:

```
I (xxx) CAP_INI: connection disconnected, reason 0x...
```
