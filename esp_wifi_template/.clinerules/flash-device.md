# esp_wifi_template — Nạp firmware lên thiết bị: OTA trước, UART khi OTA không tới

> Rule chuyên cho việc nạp app / ảnh FS lên board. Việc nạp **một trang web** là rule khác
> (`.clinerules/upload-web-page.md`, không cần flash). Quy trình build/monitor đầy đủ:
> `.clinerules/arduino-esp32-build-flash-monitor.md`. Skill: `.dsh/skills/flash-device/SKILL.md`.
> Xác nhận cuối: 2026-10-10 — OTA `Flash-OTA.ps1 -Build` chạy 100% và board trả lời lại sau ~3 s (fw
> `V3.1.9BT9`, Build_Date `Thu Oct 10 2026 10:25:56`); UART `esptool flash_id` trên COM10 tự vào download
> mode và tự hard-reset (ESP32-D0WD-V3 rev v3.1, flash 4 MB) **không cần bấm nút**.

## 1. Thứ tự ưu tiên (bắt buộc)

1. **OTA qua IP trước** — board `192.168.3.102`, ArduinoOTA trên **UDP 3232**, không password.
2. **Không kết nối được với board → UART.** Khi đó khả năng cao board đã **reset (mất cấu hình WiFi)**
   hoặc **không vào được local network** (khác subnet / AP / router chặn). UART **luôn luôn nạp được** vì
   esptool điều khiển chân **RST/DTR** để đưa ESP32 vào chế độ program — không phụ thuộc app đang chạy,
   WiFi, hay OTA.

Không hỏi lại người dùng "nạp bằng cách nào": tự chọn theo thứ tự trên. Chỉ hỏi **một câu xác nhận** trước
khi nạp app (board sẽ khởi động lại, trạm đang chạy) — và bỏ qua câu đó nếu chính người dùng vừa yêu cầu nạp.

## 2. Chẩn đoán chọn đường nạp — một lệnh (chỉ đọc)

```powershell
node script/flash/board_status.js          # in ping/web/firmware/cổng COM/công cụ + KẾT LUẬN kèm lệnh nạp
```

| Quan sát | Kết luận |
| --- | --- |
| Web trả lời `device_info` | **OTA** |
| Không web nhưng còn cổng `network` của IP đó (`arduino-cli board list`) | **UART** (app treo/crash, OTA không đáng tin) |
| Không ping + không cổng `network` sau khi chờ | **UART** (board reset mất WiFi hoặc không vào LAN) |

**Phải chờ 20–60 s sau reset/OTA rồi mới kết luận** (board cần thời gian vào lại WiFi; script đã chờ sẵn
tối đa 45 s, đổi bằng `--wait`). Kết luận "UART" khi board vừa reset xong là **sai** — đã gặp thực tế này.

⚠️ `arduino-cli board list` có thể thấy **nhiều board**: hiện có `192.168.3.102` và `192.168.3.203`.
Trước khi nạp phải chắc đúng board: `GET /get?param_wifi=device_info` → phải là trạm cần nạp
(hiện tại `CỬA HÀNG XĂNG DẦU SỐ 1`), COM mặc định `COM10` theo `.vscode/arduino.yaml`. Đừng nạp mù theo IP.

## 3. Đường ưu tiên — OTA

```powershell
.\script\ota\Flash-OTA.ps1 -Build              # build + nạp app (khuyến nghị)
.\script\ota\Flash-OTA.ps1                     # nạp .build\esp_wifi_template.ino.bin đang có
.\script\ota\Flash-OTA.ps1 -Target littlefs    # nạp ảnh LittleFS (espota -s)
```

- Thành công: `Uploading: … 100% Done...` + script in `OK OTA THANH CONG - board se tu khoi dong lai`.
- OTA **không** nạp được bootloader / partition table (giới hạn ESP32) — việc đó chỉ UART.
- Board tự reset; **kiểm chứng ngay** (§5) chứ không kết luận từ log espota.

## 4. Đường dự phòng — UART (luôn thành công)

```powershell
# kiểm tra cáp/port mà KHÔNG ghi gì (tự vào download mode, tự hard-reset)
& "C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\esptool\esptool.exe" --chip esp32 --port COM10 --baud 921600 --before default_reset --after hard_reset flash_id

# nạp app (đường chuẩn, esptool tự đúng offset app0/app1)
arduino-cli compile -b $fqbn --upload --port COM10 --build-path ".build" "./esp_wifi_template.ino"
arduino-cli upload  -b $fqbn --port COM10 --build-path ".build" "./esp_wifi_template.ino"   # .bin có sẵn
```

`$fqbn` = `board` + `configuration` nguyên chuỗi trong `.vscode/arduino.yaml`. Chỉ **một** tiến trình được
giữ `COM10`: đóng Serial Monitor / Tera Term / Teleplot trước khi nạp.

Nạp **full** (bootloader + partition + app, chỉ khi board trắng/partition hỏng): esptool với
`.build\esp_wifi_template.ino.bootloader.bin` @`0x1000`, `.build\esp_wifi_template.ino.partitions.bin` @`0x8000`,
`<core>\tools\partitions\boot_app0.bin` @`0xe000`, `.build\esp_wifi_template.ino.bin` @`0x10000`
(`--flash_mode qio --flash_freq 80m` cho khớp FQBN).

### ⚠️ LittleFS (scheme `web485_v2`, 4 MB): offset `0x310000`, size `0xF0000`

Phân vùng thật (`web485_v2` — theo `.vscode/arduino.yaml`, `FlashSize=4M`): `nvs 0x9000/0x5000` ·
`otadata 0xe000/0x2000` · `app0 0x10000/0x180000` (1.5 MB) · `app1 0x190000/0x180000` (1.5 MB) ·
**`spiffs 0x310000/0xF0000`** (960 KB). Board báo `totalBytes` = 983.040 = `0xF0000`.

```powershell
$mk  = "C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mklittlefs\mklittlefs.exe"
$esp = "C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\esptool\esptool.exe"
& $mk  -c .\data\ -d 0 -b 4096 -p 256 -s 0xF0000 .\littlefs.bin
& $esp --chip esp32 --port COM10 --baud 921600 --before default_reset --after hard_reset `
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x310000 .\littlefs.bin
```

`script\esp32\flash_raw\` là bộ ảnh gốc **đúng layout này**: `bin\partitions.csv` = `web485_v2`
(app0 `0x10000`, app1 `0x190000`, spiffs `0x310000/0xF0000`), `bin\littlefs.bin` = 983.040 B = `0xF0000`,
`bin\app.bin` = 1.027.792 B. `flash_esp32_qio.bat` ghi boot_app0 `0xe000` · bootloader `0x1000` · app
`0x10000` · partition `0x8000` · LittleFS `0x310000` → **offset đúng cho `web485_v2`**; chỉ vướng `set /p`
hỏi COM nên không tự động hoá được (nạp tay bằng esptool như trên). Nạp FS thì ưu tiên OTA `-Target littlefs`
(board tự biết phân vùng).

## 5. Sau khi nạp — bắt buộc kiểm chứng

1. `node script/flash/board_status.js` → `web + device_info OK` và `firmware` có `Build_Date` **mới**.
2. Endpoint mới (nếu bản nạp có thêm): `GET /get?param_wifi=<tên>` phải trả JSON — endpoint lạ chỉ trả dòng
   echo `URI: /get … VALUE:…` nghĩa là firmware cũ vẫn đang chạy.
3. Log boot serial theo checklist `.clinerules/arduino-esp32-build-flash-monitor.md` §3
   (`==== Firmware version … ====`, `build_time`, `App is valid`, `LittleFS Init succeed`, `got IP address`).

## 6. Nạp cái gì bằng đường nào

| Việc | Đường |
| --- | --- |
| Một trang web trong `data_org/` | **Không flash** — dùng `script/web_upload/upload_page.js` (skill `upload-web-page`) |
| App (C++/endpoint/handler) | OTA trước, UART khi OTA không tới |
| LittleFS dựng lại | OTA `-Target littlefs`, UART chỉ khi buộc (0x310000) |
| Bootloader / partition table, board trắng | UART (esptool) |
