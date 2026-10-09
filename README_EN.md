# MTTL-W01 Standalone Firmware

[한국어](README.md) | English

Direct Wi-Fi web control and Home Assistant MQTT integration for the LG U+
MTTL-W01, without a separate backend or router DNAT.

![MTTL-W01 web dashboard](docs/dashboard.png)

## Download

**2.0.7 / build `d3af2896285623ca` · October 9, 2026**

[Download comMTTL-W01_2.0.7.fwr](firmware/comMTTL-W01_2.0.7.fwr)

SHA-256: `acc3a35811bf662e039edb0180aa7f6a9383d469dbde096704befce71f246d65`

### Changes in 2.0.7

- Remove HA Discovery registrations before disconnecting when MQTT is disabled
- Retry cleanup after broker connectivity returns; rediscover when enabled again
- Show ON channel buttons with a green background and white text in the web dashboard

### Changes in 2.0.6

- Publish confirmed aggregate and channel ON/OFF states with `retain=true` so HA receives them after a late subscription
- Republish unchanged channel states when HA reconnects
- Keep sensor readings transient with existing expiry behavior

### Changes in 2.0.5

- Optimized relay drive/release for faster successive channel and all-channel control
- Firmware-tab restore support for stock `1.0.66` and local-patched `1.0.68` FWR
- Format, checksum and flash-readback validation without a fixed restore-file SHA-256 allowlist

Existing standalone users can update through the device's **Firmware** tab.

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
- Restore to stock 1.0.66 or local-patched 1.0.68 FWR

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

## Wi-Fi configuration

1. Join the open `MTTL-W01-SETUP` AP and visit `http://192.168.4.1`.
2. In Wi-Fi, scan/select an SSID, enter its password and connect.
3. Find the assigned IP in your router and open `http://DEVICE_IP`.

Hold the main button for ten seconds to reopen setup. LAN Wi-Fi changes are also
supported; find the new address after switching networks. Router outages trigger
saved-network retries, while manually entered setup mode stays available.

## Home Assistant

1. Configure HA's MQTT integration.
2. Enter broker address, port and credentials in the device MQTT tab.
3. Enable MQTT and save; Discovery registers the device and entities.

From 2.0.7, disabling MQTT and saving removes HA Discovery registrations.
Cleanup retries when broker connectivity returns; enabling MQTT again registers
the entities again. Keep the address and credentials of the broker holding the
existing registrations so cleanup can reach it.

Discovery uses `homeassistant`. IDs follow `switch.mttl_<last 7 MAC digits>_sw1`;
the aggregate switch ends in `_all`. Existing conflicts may cause HA to add a suffix.

## Subsequent updates: device web OTA

1. Download the latest standalone `.fwr`.
2. Open the device's Firmware tab, select the file and upload.
3. Keep power connected, reconnect after automatic reboot and verify the build.

Do not upload BIN files or full-flash dumps to the standalone updater.
No manual SHA-256 or token is required; internal image/slot checks remain enabled.
If transfer disconnects, close other device tabs, check the running build, then retry.

## Restore to stock / local firmware

Stock `1.0.66` FWR is available in the [original repository's stock firmware directory](https://github.com/af950833/mttl_w01/tree/main/work/firmware/1.0.66).

On **2.0.5 or later**, select a stock `1.0.66` or backend-oriented local-patched
`1.0.68` FWR in the Firmware tab and upload it. Keep power connected throughout
transfer and reboot. Other versions and arbitrary formats are unsupported.

- Restoration ends the standalone web dashboard and direct HA MQTT integration.
- Stock firmware may require its original provisioning/service setup again.
- Local 1.0.68 requires a file generated for the correct backend IP and that backend server.
- Remove remaining standalone HA entities manually if needed.
- To return to standalone, use the **initial-installation OTA tool** described above.

There is no fixed file-digest allowlist. Image format, size, checksum, SHA-256
receive/readback consistency and slot compatibility are still checked. These
checks are not signature authentication; use FWR files from trusted sources only.
The OTA/restore path passed 142 ARM-execution tests with mocked flash;
physical 1.0.66/1.0.68 restore boot still needs separate verification.

## Verification and safety

Repeated web OTA, settings/state persistence, Wi-Fi changes, MQTT broker restart
and router-reboot recovery were verified on the development device. Actual watchdog
stall recovery, all variants, measurement accuracy and long-term stability remain unverified.

Trusted LANs only: HTTP/MQTT are unencrypted; there is no web login, OTA token or
firmware signature authentication. Never expose ports to the Internet. Origin checks
are not user authentication.

This repository distributes firmware and documentation, not the complete buildable SDK source.

## Version History

### 2.0.7 — 2026-10-09

- Remove Discovery registrations for five switches and twenty sensors, plus retained availability, when MQTT is disabled
- Fence deletion delivery with a broker response, retry after connection failures, and rediscover when enabled again
- Green ON buttons in the web dashboard; unchanged OFF styling

### 2.0.6 — 2026-10-08

- Publish aggregate and channel switch states with `retain=true`
- Republish unchanged channel states when HA reconnects
- Keep sensor publications at `retain=false`
- Verified 2.0.6 OTA, reboot, MQTT reconnection and preserved channel states on the development device

### 2.0.5 — 2026-10-08

- Optimized relay drive/release for faster successive channel and all-channel control
- Web restore support for stock 1.0.66 and local-patched 1.0.68 FWR
- Retained image-format, checksum and flash-readback validation without a fixed restore-file digest allowlist

### 2.0.0 — 2026-10-07

- Initial public standalone firmware release
- SoftAP/LAN Wi-Fi setup and automatic reconnection
- Individual/all relay control, physical buttons/LEDs and persistent channel state
- Total/per-channel sensors and Home Assistant MQTT Discovery integration
- Korean/English web dashboard, SSE updates and FWR web OTA
- Persistent settings/energy and enabled watchdog
