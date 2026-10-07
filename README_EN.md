# MTTL-W01 Standalone Firmware

[한국어](README.md) | English

Direct Wi-Fi web control and Home Assistant MQTT integration for the LG U+
MTTL-W01, without a separate backend or router DNAT.

## Download

**2.0.0 / build `0006a05288fbbb45` · October 7, 2026 · Experimental**

[Download comMTTL-W01_2.0.0.fwr](firmware/comMTTL-W01_2.0.0.fwr)

Compatibility with every hardware variant is not guaranteed. Failed installation
may require debugger recovery. Overload/overtemperature protection and measurement
accuracy are unverified; do not rely on this firmware as a safety system.

## Features

- SoftAP Wi-Fi setup/scan, LAN Wi-Fi changes and automatic reconnection
- Individual/all channel control, physical buttons and LEDs
- Total power/energy/voltage/current and per-channel power/energy/current/temperature
- Editable device/channel names, Korean/English UI, NTP and browser-session logs
- HA MQTT Discovery: five switches and twenty sensors
- SSE updates, optimistic web controls and confirmed logical-state reporting to HA
- FWR web OTA, persistent settings/energy and enabled watchdog

Sensors update approximately every ten seconds. Channel current subtracts 0.019 A
and clamps negative results to zero. Relay state is logical, not physical contact
feedback. It is saved ten seconds after the last completed control; earlier power
cuts may restore an older displayed state.

## Requirements

MTTL-W01, 2.4 GHz open/WPA2 Personal Wi-Fi, and a Wi-Fi-capable Windows x64 PC or
Apple silicon Mac for initial installation. HA requires an MQTT broker/integration.

## Initial installation: Windows / macOS OTA tool

Use [ttaengz's OTA tools](https://github.com/ttaengz/mttl-w01-matterbridge#펌웨어-업데이트).
Download the **OTA tool**, not the separate Wi-Fi configuration tool. Follow the
upstream instructions for supported operating systems and tool controls.

1. Download this repository's FWR and the OTA tool for your operating system.
2. Hold the stock device's main button for at least ten seconds to enter setup.
3. Connect the PC to `TONLY_TAP_xxxxxxx`, using password `LGU_xxxxxxx` with the
   same suffix shown in the AP name.
4. Select **this repository's FWR** in the tool and transfer it following its prompts.
   You do not need the MatterBridge server or that project's firmware.
5. Keep power connected. After reboot, check for `MTTL-W01-SETUP`.

Successful first-time OTA installation of this standalone build has not yet been
verified. The development device was initially installed using a debugger, then
updated repeatedly through web OTA. Transfer completion alone is not proof of
successful installation: verify the setup AP and web access.

## Wi-Fi configuration

1. Join the open `MTTL-W01-SETUP` AP and visit `http://192.168.4.1`.
2. In Wi-Fi, scan/select an SSID, enter its password and connect.
3. Find the assigned IP in your router and open `http://DEVICE_IP`.
   `192.168.0.153` is only the development address, not a fixed device address.

Hold the main button for ten seconds to reopen setup. LAN Wi-Fi changes are also
supported; find the new address after switching networks. Router outages trigger
saved-network retries, while manually entered setup mode stays available.

## Home Assistant

1. Configure HA's MQTT integration.
2. Enter broker address, port, credentials and topic prefix in the device MQTT tab.
3. Enable MQTT and save; Discovery registers the device and entities.

Discovery uses `homeassistant`. IDs follow `switch.mttl_<last 7 MAC digits>_sw1`;
the aggregate switch ends in `_all`. Existing conflicts may cause HA to add a suffix.
HA switches use `optimistic: false` and device-reported logical state.

## Subsequent updates: device web OTA

1. Download the latest standalone `.fwr`.
2. Open the device's Firmware tab, select the file and upload.
3. Keep power connected, reconnect after automatic reboot and verify the build.

Do not upload BIN files, full dumps or stock firmware to the standalone updater.
No manual SHA-256 or token is required; internal image/slot checks remain enabled.
If transfer disconnects, close other device tabs, check the running build, then retry.

## Verification and safety

Repeated web OTA, settings/state persistence, Wi-Fi changes, MQTT broker restart
and router-reboot recovery were verified on the development device. Actual watchdog
stall recovery, all variants, measurement accuracy and long-term stability remain unverified.

Trusted LANs only: HTTP/MQTT are unencrypted; there is no web login, OTA token or
firmware signature authentication. Never expose ports to the Internet. Origin checks
are not user authentication.

**Never connect ST-Link, UART, USB or a PC while connected to the AC board.**
Wired recovery requires complete separation of the Wi-Fi board and qualified
verification of supply/ground/signal levels. Never write another device's full dump.

This repository distributes firmware and documentation, not the complete buildable SDK source.
