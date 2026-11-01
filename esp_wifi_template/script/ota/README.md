# Nạp firmware qua IP (OTA)

Board esp_wifi_template hỗ trợ OTA qua mạng LAN, không cần cắm cáp USB.

## Vì sao dùng được

- `THIoT_ESPConfig.h` bật `OTA_ARDUINO_ENABLE = 1`.
- `THIoT_ESPWifiHandle.cpp` gọi `ArduinoOTA.begin()` (có cả `ArduinoOTA.handle()` trong `loop()`).
- Board lắng nghe gói mời OTA trên **UDP port 3232** (mặc định của `ArduinoOTA`).
- Hiện **chưa đặt password** (`// ArduinoOTA.setPassword("1234");` đang comment) → bỏ trống `-a`.

## Công cụ

Dùng `espota.exe` (hoặc `espota.py`) — chính là thứ Arduino IDE gọi khi bạn chọn cổng mạng:

```
C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\espota.exe
C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\espota.py
```

Lệnh thô tương đương:

```powershell
espota.exe -i 192.168.3.102 -p 3232 --auth= `
  -f ".build\esp_wifi_template.ino.bin" -t 30 -r
```

- `-i` IP board, `-p` port UDP OTA (3232), `-f` file `.bin`, `-t` timeout mỗi gói, `-r` hiện tiến độ.
- `-s` nếu muốn nạp **LittleFS** thay vì app.
- `-d` để bật log debug.

## Script trong repo

| File | Mô tả |
| --- | --- |
| `script/ota/Flash-OTA.ps1` | Script chính (PowerShell). Tự tìm `espota`, tự đọc `.vscode/arduino.yaml`, kiểm tra ping, build tùy chọn, rồi nạp. |
| `script/ota/Flash_OTA.bat` | Wrapper nhập IP bằng tay cho người quen dùng `.bat`. |

### Cách dùng nhanh

```powershell
# Nạp file .bin đang có trong .build
.\script\ota\Flash-OTA.ps1

# Build lại rồi nạp (không dùng --clean)
.\script\ota\Flash-OTA.ps1 -Build

# Đổi IP / port / timeout
.\script\ota\Flash-OTA.ps1 -Ip 192.168.3.150 -Port 3232 -Timeout 60

# Nạp ảnh LittleFS
.\script\ota\Flash-OTA.ps1 -Target littlefs

# Chỉ định file .bin cụ thể + log debug
.\script\ota\Flash-OTA.ps1 -Bin .\script\esp32\flash_raw\bin\app.bin -OtaDebug -SkipPing
```

Hoặc chạy `script\ota\Flash_OTA.bat` rồi nhập IP theo hướng dẫn.

### Tham số `Flash-OTA.ps1`

| Tham số | Mặc định | Ý nghĩa |
| --- | --- | --- |
| `-Ip` | `192.168.3.102` | IP board |
| `-Port` | `3232` | UDP port OTA |
| `-Auth` | rỗng | password OTA (firmware hiện chưa đặt) |
| `-Bin` | `.build\esp_wifi_template.ino.bin` | file `.bin` cần nạp |
| `-Build` | tắt | biên dịch bằng `arduino-cli` trước khi nạp |
| `-Target` | `app` | `app` hoặc `littlefs` |
| `-Timeout` | `20` | timeout mỗi gói (giây) |
| `-OtaDebug` | tắt | bật `-d` của espota |
| `-SkipPing` | tắt | bỏ qua bước ping |

Biến môi trường tùy chọn: `ARDUINO_CLI` (đường dẫn `arduino-cli.exe`), `ESPOTA_EXE` (đường dẫn `espota.exe`).

## Lưu ý

- **Chỉ nạp được app**, không nạp được bootloader / partition table qua OTA (giới hạn của ESP32 — phải nạp qua COM/UART, xem `.clinerules/flash-device.md`).
- Windows Firewall có thể hỏi cho phép `espota.exe` mở socket TCP — phải đồng ý.
- Chỉ 1 board được dùng IP đó; đảm bảo chạy đúng firmware có OTA đang chạy.
- Log phía board (qua Serial Monitor / Tera Term) sẽ hiện `[OTA] Start updating application`, `[OTA] Progress: n%`, `[OTA] Succeed`.
- Sau khi OTA xong board tự reset; kiểm tra log boot có `==== Firmware version ... ====` và `[OTA_MANAGER] ... App is valid`.
- Đây **không phải** ESP-IDF — vẫn dùng `arduino-cli`, không dùng `idf.py`.
