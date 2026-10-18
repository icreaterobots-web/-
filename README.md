# PocketChat OS — T-Embed CC1101 prototype

A starter PlatformIO project for an ESP32-S3 T-Embed CC1101 that starts a password-protected local Wi-Fi AP, shows its credentials on the TFT, serves a local chat UI, stores profiles/messages in LittleFS, and enters deep sleep after holding the user button for 3 seconds.

## Defaults
- SSID: `PocketChat`
- Password: `Pc-7mK9-xQ2v` (change `AP_PASSWORD` in `src/main.cpp` before building)
- Site: `http://192.168.4.1`
- Button: GPIO6 (active-low)
- Backlight: GPIO21

## Build from a phone
1. Upload this project to a GitHub repository (keep folder structure).
2. In repository Settings → Actions → General, allow workflows if disabled.
3. Push to `main` or open Actions → Build PocketChat OS → Run workflow.
4. Download artifact `PocketChat-OS-ESP32-S3`; it contains `firmware.bin`.
5. Confirm your multi-firmware loader's expected format and offset before importing. Some loaders need a merged image/manifest or partition files; this project artifact is a PlatformIO application image, not guaranteed to be a loader-ready merged image.

## Notes / limitations
- Prototype, not hardware-tested in this environment. Verify TFT orientation and button wake behavior on the exact board revision.
- Chat uses polling every 1.8 s and stores the last 80 messages and up to 40 nickname profiles in LittleFS. A nickname is not a secure identity: anyone can reuse another nickname. Do not use for private/secure conversations.
- Captive portal auto-open depends on phone OS; HTTPS pages cannot be transparently redirected. If it doesn't open automatically, connect to the AP and visit `http://192.168.4.1`.
- The AP password is visible on the device screen by design; only people who can read it can join. Change it before sharing.
- Long-press enters deep sleep and turns off Wi-Fi/backlight. It waits for release and uses GPIO6 as wake source; if wake doesn't work on the installed board revision, use the reset button to reboot.
