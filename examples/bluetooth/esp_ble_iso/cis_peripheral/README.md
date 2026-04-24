| Supported Targets | ESP32-H4 |
| ----------------- | -------- |

# BLE CIS Peripheral Example

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example demonstrates how to use a **Connected Isochronous Stream (CIS)** as a peripheral. It starts connectable extended advertising with the name `CIS Peripheral`, waits for a central to connect and set up an isochronous channel, and then receives and counts isochronous data on the CIS. Run it together with the [cis_central](../cis_central) example on another device as the central.

The implementation uses the NimBLE host stack with ISO support and the ESP-BLE-ISO APIs (ISO server register, accept callback, data path, channel receive). It is intended for chips that support BLE 5.2 ISO (e.g. ESP32-H4). The connection security level is hardcoded as `ESP_BLE_ISO_SECURITY_MITM` (Level 3: encryption and authentication); it must match the [cis_central](../cis_central) side for pairing to succeed.

## Requirements

* A board with Bluetooth LE 5.2 and ISO support (e.g. ESP32-H4)
* Another device running the [cis_central](../cis_central) example, which scans, connects, and creates the CIS

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

1. **Initialization**: NVS, Bluetooth stack (NimBLE), and ISO common layer (`esp_ble_iso_common_init`) with GAP callback for ACL and security events.
2. **ISO server**: Register an ISO server with an accept callback and the configured security level. When a central requests a CIS, the accept callback returns the single CIS channel.
3. **Extended advertising**: Start connectable extended advertising with the local name `CIS Peripheral` so the central can discover and connect.
4. **ACL connection**: When the central connects, the connection and optional pairing/security change are handled; the central then creates the CIG and CIS.
5. **CIS connected**: When the CIS is connected, set up the output data path (receive direction). Incoming ISO SDUs are delivered in the receive callback; the example counts valid, error, and lost packets and logs periodically.

## Example Output

```
I (xxx) CIS_PER: Extended adv instance 0 started
I (xxx) CIS_PER: connection established, handle 0 status 0x00
I (xxx) CIS_PER: Incoming request from 0x0000
I (xxx) CIS_PER: ISO channel 0x... connected
I (xxx) CIS_PER: Received 1000(1000/0/0) ISO data packets (chan 0x...)
...
```

If connection or security fails, relevant status or error messages are logged.
