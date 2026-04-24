| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BLE BIG Broadcaster Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates the **Bluetooth LE Isochronous Broadcaster** functionality. It acts as a Broadcast Isochronous Stream (BIS) source: it starts extended advertising and periodic advertising, creates a Broadcast Isochronous Group (BIG), and sends isochronous data on the BIS channels. Receivers can synchronize to this BIG using the [big_receiver](../big_receiver) example.

The implementation uses the NimBLE host stack with ISO support and the ESP-BLE-ISO APIs (CIG/BIG, data path, channel operations). It is intended for chips that support BLE 5.2 ISO (e.g. ESP32-H4). The advertised device name is hardcoded as `BIG Broadcaster` and the broadcast code is hardcoded as `1234`; these can be changed by editing the source code constants.

## Requirements

* A board with Bluetooth LE 5.2 and ISO support (e.g. ESP32-H4)
* Optionally, a second device running the [big_receiver](../big_receiver) example to receive and sync to the BIG

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

1. **Initialization**: NVS, Bluetooth stack (NimBLE), and ISO common layer (`esp_ble_iso_common_init`).
2. **Extended and periodic advertising**: Configure and start extended advertising with a fixed interval; attach periodic advertising with name `BIG Broadcaster` for receivers to discover.
3. **Create BIG**: Register the advertising set for BIG, then create a BIG with two BIS channels (hardcoded as `BIS_ISO_CHAN_COUNT` in the source; `CONFIG_BT_ISO_MAX_CHAN` must be >= 2), SDU interval 10 ms, latency 10 ms, sequential packing, unframed, and encryption with the hardcoded broadcast code `1234`. The [big_receiver](../big_receiver) must use the same broadcast code.
4. **Send ISO data**: Once all BIS channels are connected and their data paths are set up, a periodic TX scheduler based on `k_work_delayable` sends the same SDU on all BIS channels at the configured interval in the ISO task context; sequence numbers and drift handling are applied. Note that the scheduler timer resolution is in milliseconds, which may not match the exact SDU interval for all configurations.

## Example Output

```
I (xxx) BIG_BRD: Extended adv instance 0 started
I (xxx) BIG_BRD: ISO channel 0x0001 connected
I (xxx) BIG_BRD: ISO channel 0x0002 connected
I (xxx) BIG_BRD: Transmitted 1000 ISO data packets (chan 0x...)
...
```
