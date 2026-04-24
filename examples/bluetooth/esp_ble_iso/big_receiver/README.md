| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BLE BIG Receiver Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Bluetooth LE Synchronized Receiver** functionality. It acts as a BIS (Broadcast Isochronous Stream) sink: it scans for extended advertising, establishes periodic advertising synchronization with a broadcaster, receives BIGInfo from the periodic advertising, and synchronizes to the BIG to receive isochronous data on the BIS channels. Use it together with the [big_broadcaster](../big_broadcaster) example on another device as the source.

The implementation uses the NimBLE host stack with ISO support and the ESP-BLE-ISO APIs (periodic sync, BIG sync, data path, channel operations). It is intended for chips that support BLE 5.2 ISO (e.g. ESP32-H4). The target broadcaster name is hardcoded as `BIG Broadcaster` and the broadcast code is hardcoded as `1234`; these must match the [big_broadcaster](../big_broadcaster) defaults.

## Requirements

* A board with Bluetooth LE 5.2 and ISO support (e.g. ESP32-H4)
* Another device running the [big_broadcaster](../big_broadcaster) example, which performs periodic advertising and creates the BIG

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

1. **Initialization**: NVS, Bluetooth stack (NimBLE), and ISO common layer (`esp_ble_iso_common_init`) with GAP callback for scan, periodic sync, and BIGInfo events.
2. **Extended scan**: Start passive extended scanning. On extended scan report, parse advertising data for the complete local name.
3. **Periodic advertising sync**: When the advertised name matches the hardcoded target name `BIG Broadcaster` and the advertiser has periodic advertising, create a periodic advertising synchronization to that advertiser.
4. **BIGInfo and BIG sync**: When BIGInfo is received in the periodic advertising, create a BIG sync (`esp_ble_iso_big_sync`) with two BIS channels (hardcoded in this example; must match the [big_broadcaster](../big_broadcaster)) and the hardcoded broadcast code `1234`.
5. **Receive ISO data**: When each BIS channel is connected, set up the output data path. Incoming ISO SDUs are reported in the receive callback; the example counts valid, error, and lost packets and logs periodically.

## Example Output

```
I (xxx) BIG_SNC: Extended scan started
I (xxx) BIG_SNC: ISO channel 0x0001 connected
I (xxx) BIG_SNC: ISO channel 0x0002 connected
I (xxx) BIG_SNC: Received 1000(1000/0/0) ISO data packets (chan 0x...)
...
```

If periodic sync is lost:

```
I (xxx) BIG_SNC: PA sync lost, reason ...
```
