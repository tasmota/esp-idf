| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BLE CIS Central Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates how to use a **Connected Isochronous Stream (CIS)** as a central. It scans for a peripheral, establishes an ACL connection, creates a Connected Isochronous Group (CIG) and a CIS to the peer, and then sends isochronous data over the CIS every 10 ms. Run it together with the [cis_peripheral](../cis_peripheral) example on another device as the peer.

The implementation uses the NimBLE host stack with ISO support and the ESP-BLE-ISO APIs (CIG create, CIS connect, data path, channel send). It is intended for chips that support BLE 5.2 ISO (e.g. ESP32-H4). The target peripheral name is hardcoded as `CIS Peripheral` and the connection security level is hardcoded as `ESP_BLE_ISO_SECURITY_MITM` (Level 3: encryption and authentication); the central initiates pairing after the ACL connection.

## Requirements

* A board with Bluetooth LE 5.2 and ISO support (e.g. ESP32-H4)
* Another device running the [cis_peripheral](../cis_peripheral) example, which advertises and accepts the ACL and CIS

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

1. **Initialization**: NVS, Bluetooth stack (NimBLE), and ISO common layer (`esp_ble_iso_common_init`) with GAP callback for scan, ACL, and security events.
2. **Extended scan**: Start passive extended scanning. On scan report, parse the complete local name; when it matches the hardcoded target name `CIS Peripheral`, initiate an ACL connection to that device.
3. **ACL connection**: On connection established, initiate pairing (security level is hardcoded as `ESP_BLE_ISO_SECURITY_MITM`); the CIG and CIS are created after security is established.
4. **Security**: On security change (after pairing), create the CIG and CIS.
5. **CIG and CIS**: Create a CIG with one CIS (10 ms latency each direction, 10 ms SDU interval, sequential packing, unframed), then connect the CIS over the ACL handle.
6. **Send ISO data**: When the CIS is connected and the input data path is set up, a periodic TX scheduler based on `k_work_delayable` sends an SDU on the CIS every 10 ms in the ISO task context; sequence numbers and drift handling are applied. Note that the scheduler timer resolution is in milliseconds, which may not match the exact SDU interval for all configurations.

## Example Output

```
I (xxx) CIS_CEN: connection established, handle 0 status 0x00
I (xxx) CIS_CEN: ISO channel 0x... connected
I (xxx) CIS_CEN: Transmitted 1000 ISO data packets (chan 0x...)
...
```

If connection or security fails, relevant status or error messages are logged.
