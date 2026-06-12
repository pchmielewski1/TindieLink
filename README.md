<p align="center">
  <img src="img/logo.png" alt="TindieLink" width="520">
</p>

<h1 align="center">TindieLink — M5StickC Plus2</h1>

Live product monitor for your Tindie seller store on **M5StickC Plus2** (landscape 240×135). Boot screens show the logo sprite as background; diagnostics draw on top with opaque bars. Product detail shows a cached CDN thumbnail (refreshed at most every `THUMB_CACHE_TTL_SEC`, not on each product poll).

## Setup

1. Copy config template and fill credentials:
   ```bash
   cp include/config.h.example include/config.h
   ```
   - `WIFI_SSID` / `WIFI_PASSWORD` — 2.4 GHz network
   - `TINDIE_USERNAME` — **store slug**, not login email
   - `TINDIE_API_KEY` — from Tindie seller tools

2. Install PlatformIO (venv recommended):
   ```bash
   python3 -m venv .venv
   .venv/bin/pip install -U platformio
   ```

3. Build and upload (device on USB):
   ```bash
   .venv/bin/pio run -e m5stickc-plus2 -t upload
   .venv/bin/pio device monitor
   ```

## Controls

| View | BtnA (front) | BtnB (side) |
|------|--------------|-------------|
| List | open detail | next product |
| Detail | next product | back to list |

Poll interval: `POLL_INTERVAL_SEC` in `config.h` (default 60s).

Product list holds up to **64** offers; thumbnail cache uses `THUMB_CACHE_TTL_SEC` (default 1800s) and `THUMB_CACHE_SLOTS` (default 6) baked RGB565 thumbs in PSRAM.

## API notes

- Seller products: `GET /api/v2/products/?store={store_id}&limit=50`
- `store_id` is resolved at runtime from `store_username` + v2 product detail
- Detail thumbnails: `GET /api/v2/products/{id}/images/` → `images[0].sizes.medium`

## Build note

`lib_ignore = DFRobot_GP8XXX` in `platformio.ini` — workaround for M5StickCPlus2 dependency compile error on current ESP32 Arduino core.

## License

MIT — see [LICENSE](LICENSE).
