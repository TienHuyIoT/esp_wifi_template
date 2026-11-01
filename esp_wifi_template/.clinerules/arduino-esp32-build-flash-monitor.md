# esp_wifi_template — Verify / Upload / Serial Monitor / OTA (ESP32 · Arduino)

> Bộ nhớ quy trình cho dự án. Đọc trước khi refactor/build/flash để verify, nạp firmware và debug qua serial monitor mà không phải dò lại.
> Xác nhận: 2026-10-04 — Verify + Upload + Monitor + **OTA qua IP** chạy thành công (exit 0, log boot sạch).

## 1. Thông tin môi trường (cố định — không tự đổi)

| Hạng mục | Giá trị |
|---|---|
| Sketch | `esp_wifi_template.ino` tại sketch root (thư mục chứa repo); là **positional arg** — `--sketch` không hợp lệ |
| CLI | `C:\ArduinoCLI\arduino-cli.exe` v1.5.2-rc.1 (cũng có trong PATH) |
| Core | `espressif:esp32` 2.0.17 |
| Port / baud | `COM10` / `115200` |
| Build dir | `.build` (đã gitignore; chứa `.bin/.elf/.map` + cache incremental) |
| Nguồn cấu hình | `.vscode/arduino.yaml` (extension `electropol-fr.arduino-vscode-ide-2026.9.5` đọc file này) |
| Board IP (OTA) | `192.168.3.102`, UDP port `3232`, không password |

FQBN đầy đủ (kèm board options — lấy nguyên chuỗi từ `.vscode/arduino.yaml`):

```powershell
$fqbn = "espressif:esp32:esp32:JTAGAdapter=default,PSRAM=disabled,PartitionScheme=web485_v2,CPUFreq=240,FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,LoopCore=1,EventsCore=1,DebugLevel=none,EraseFlash=none"
```

⚠️ Port/FQBN luôn lấy từ `.vscode/arduino.yaml`. Extension truyền FQBN qua `-b "<fqbn:opt=val,...>"` (ghép config vào chuỗi FQBN), **không** dùng `--board-options`.

## 2. Các command của extension ↔ lệnh CLI tương đương

Extension `electropol-fr.arduino-vscode-ide-2026.9.5` chạy `arduino-cli` phía dưới. Agent tự động hóa không bấm được Command Palette → gọi trực tiếp CLI với đúng tham số như extension:

| VS Code command | Lệnh tương đương |
|---|---|
| `Arduino: Verify` (`arduino.verify`) | `arduino-cli compile -b $fqbn --build-path ".build" "./esp_wifi_template.ino"` |
| `Arduino: Upload` (`arduino.upload`) | `arduino-cli compile -b $fqbn --upload --port COM10 --build-path ".build" "./esp_wifi_template.ino"` |
| `Arduino: CLI Upload` (`arduino.cliUpload`) | `arduino-cli upload -b $fqbn --port COM10 --build-path ".build" "./esp_wifi_template.ino"` (không build lại) |
| `Arduino: Open Serial Monitor` (`arduino.openSerialMonitor`) | Mở panel Serial Monitor (`ms-vscode.vscode-serial-monitor`): chọn COM10 @ 115200 |

> **OTA qua IP không có command trong extension.** Arduino IDE có mục chọn cổng mạng; extension `electropol-fr.arduino-vscode-ide` không expose. Tương đương CLI là gọi trực tiếp `espota.exe` — xem §2.4.

### 2.1. Verify (Arduino: Verify)

```powershell
Set-Location "<sketch root>"            # thư mục chứa esp_wifi_template.ino
arduino-cli compile -b $fqbn --build-path ".build" "./esp_wifi_template.ino" 2>&1 | Tee-Object -FilePath '.build\verify_log.txt'
```

- Thành công: exit code **0** + `Sketch uses N bytes (P%) of program storage space` + `Global variables use N bytes (P%) of dynamic memory`.
- Incremental: file không đổi sẽ không recompile (cache `.build` warm); log chỉ có summary = không có gì phải build lại.
- Full rebuild khi cần: thêm `--clean`. Soi lệnh gcc chi tiết: thêm `--verbose`.

### 2.2. Upload / Flash (Arduino: Upload)

```powershell
arduino-cli compile -b $fqbn --upload --port COM10 --build-path ".build" "./esp_wifi_template.ino" 2>&1 | Tee-Object -FilePath '.build\upload_log.txt'
```

- Command Upload tự build lại (nếu cần) rồi nạp luôn — không cần chạy Verify trước.
- Thành công: `Wrote ... bytes ... at 0x00010000` + `Hash of data verified.` + `Hard resetting via RTS pin...` (board tự chạy firmware mới).
- Lỗi mở port ("could not open port"): chắc chắn không app nào đang giữ COM10 (đóng Serial Monitor/Teleplot trước khi nạp); kiểm tra port bằng `arduino-cli board list`.

### 2.3. Serial Monitor (COM10 @ 115200)

- Trong VS Code: `Arduino: Open Serial Monitor` → chọn COM10, 115200.
- Từ CLI (agent — thu log có giới hạn thời gian, không treo):

```powershell
$p = Start-Process -FilePath 'C:\ArduinoCLI\arduino-cli.exe' `
     -ArgumentList 'monitor','-p','COM10','-c','baudrate=115200','-q' `
     -RedirectStandardOutput "$PWD\.build\monitor_out.txt" -RedirectStandardError "$PWD\.build\monitor_err.txt" `
     -PassThru -NoNewWindow
Start-Sleep -Seconds 15
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Get-Content "$PWD\.build\monitor_out.txt"
```

- `-q` chỉ in luồng dữ liệu (không TUI); thêm `--timestamp` để có mốc thời gian; `--describe` để xem cấu hình port.
- Mở port sẽ toggle DTR/RTS → ESP32 reset → log bắt đầu từ bootloader (tốt để soi boot).
- **Luôn kill monitor sau khi thu xong**: chỉ 1 tiến trình được giữ COM10, nếu không upload sẽ fail.

### 2.4. OTA (nạp qua IP — không cần cáp USB)

> **Thứ tự ưu tiên khi nạp: OTA trước, UART khi OTA không tới được** — chẩn đoán, lệnh UART và offset
> LittleFS `0x310000/0xF0000`: `.clinerules/flash-device.md`.

Firmware đã bật `OTA_ARDUINO_ENABLE 1` (`THIoT_ESPConfig.h`) và gọi `ArduinoOTA.begin()/handle()` (`THIoT_ESPWifiHandle.cpp`). Board lắng nghe gói mời OTA trên **UDP port 3232**, hiện **chưa đặt password** (`ArduinoOTA.setPassword` còn comment) → `--auth=` bỏ trống.

Tool là `espota.exe` / `espota.py` của ESP32 platform (chính là tool Arduino IDE dùng khi chọn cổng mạng):

```text
C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\espota.exe
C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\espota.py   (Python 3.13 có sẵn)
```

**Ưu tiên script trong repo** (`script/ota/`):

```powershell
.\script\ota\Flash-OTA.ps1              # nạp .build\esp_wifi_template.ino.bin đang có
.\script\ota\Flash-OTA.ps1 -Build       # build lại (KHÔNG --clean) rồi nạp
.\script\ota\Flash-OTA.ps1 -Ip 192.168.3.150 -Timeout 60   # đổi IP / timeout
.\script\ota\Flash-OTA.ps1 -Target littlefs                # nạp ảnh LittleFS (-s)
.\script\ota\Flash-OTA.ps1 -Bin .\script\esp32\flash_raw\bin\app.bin   # chỉ định file .bin cụ thể
.\script\ota\Flash-OTA.ps1 -OtaDebug                       # bật -d của espota
```

Script tự: tìm `espota` (repo → `Documents\Arduino` → `%LOCALAPPDATA%\Arduino15`), đọc `.vscode/arduino.yaml`, kiểm tra ping, build tùy chọn, rồi nạp. Biến môi trường ghi đè: `ARDUINO_CLI`, `ESPOTA_EXE`.

**Lệnh thô tương đương** (không cần script):

```powershell
& "$env:USERPROFILE\Documents\Arduino\hardware\espressif\esp32\tools\espota.exe" `
  -i 192.168.3.102 -p 3232 --auth= -f ".build\esp_wifi_template.ino.bin" -t 30 -r
```

- `-i` IP board · `-p` UDP port OTA (3232) · `-f` file `.bin` · `-t` timeout mỗi gói · `-r` tiến độ · `-s` = LittleFS · `-d` = debug.
- Thành công: exit code **0** + `Uploading: [====] 100% Done...`; board tự reset và chạy app mới.
- Windows Firewall có thể hỏi cho phép `espota.exe` mở socket TCP — phải đồng ý, nếu không OTA fail.
- **Giới hạn**: OTA chỉ nạp được **app**, không nạp được bootloader / partition table → phải nạp qua COM (UART).
- Log phía board: `[OTA] Start updating application`, `[OTA] Progress: n%`, `[OTA] Succeed` (thu qua Serial Monitor §2.3).
- Chi tiết đầy đủ + bảng tham số: `script/ota/README.md`.

## 3. Checklist log boot khỏe mạnh

- `rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)` — board vừa reset chạy app.
- `[OTA_MANAGER] handlePendingVerify … No pending verify state` + `esp_ota_power_on_res … App is valid, no action needed`.
- `[MAIN] setup … ==== Firmware version VX.Y.Z ====` + `build_time: <ngày giờ build>` → nếu `build_time` không đổi nghĩa là không có gì được build lại (nguồn chưa đổi hoặc chưa compile).
- `LittleFS Init succeed` + `SD_CARD INIT SUCCEED`.
- `Boot run from app0 partition (Valid)`.
- `[WIFI] [EVENT] connected …` + `got IP address: …` (LAN) + `AP IP address: 192.168.4.1` (AP mode).
- `[SNTP] callback settimeofday` + `Time: …`.
- Dấu hiệu LỖI cần điều tra: `Guru Meditation`, `abort()`, assert failed, `rst:0x…` lặp lại liên tục, `Task watchdog`, `Brownout detector`, `stack overflow`, `LoadProhibited`.

## 4. Debug runtime qua serial

- Định dạng log: `I [TAG] func line N: msg` (mức E/W/I/D/V); phát đồng thời ra: Serial 115200, file `/DEBUG_TRACE.TXT` trên LittleFS, WebSocket console.
- Bật/tắt + mức log (thời điểm build): `THIoT_APPConfig.h` → `CONSOLE_TRACE_TASK_ENABLE` (hiện =0); `THIoT_PFConsoleTrace.h` → `gConsoleDebugLevel` (0=OFF, 1=E, 2=+W, 3=+I, 4=+D; V cần >4).
- In kèm free heap mỗi dòng log (tìm memory leak): `THIoT_APPConfig.h` → `SERIAL_HEAP_TRACE_ENABLE` (hiện =1). Khi =1, dòng `SERIAL_LOG*`/`SERIAL_CONSOLE_*` có prefix `H<free heap> | ` (vd `H213456 | I WIFI: ...`); heap tăng/giảm đều đặn qua thời gian là dấu hiệu rò rỉ. Helper dùng template nên không lệch `%u`/`%lu`.
- Cần plot dữ liệu đo dạng đồ thị: `Arduino: Open Serial Tracer` (Teleplot).
- Grep log đã thu (file trong `.build/`, đã gitignore): `Select-String -Path '.build\monitor_out.txt' -Pattern 'error|fail|rst:|watchdog'`.

## 5. Quy tắc khi refactor

1. Sửa code → Verify (exit 0 + storage summary) → Upload (hash verified) → Monitor 15s + đối chiếu checklist §3 → chỉ kết luận OK khi cả 3 bước đạt.
2. Chỉ build ở sketch root chứa `esp_wifi_template.ino`; không dùng ESP-IDF tooling.
3. Không đổi FQBN/board options/partition scheme nếu chưa được yêu cầu (phải khớp `.vscode/arduino.yaml`).
4. Chỉ 1 tiến trình giữ COM10: đóng monitor/Teleplot trước khi upload; kill monitor ngay sau khi thu log.
5. Lỗi upload kiểu not-in-sync/port busy → `arduino-cli board list`, đóng app đang giữ port, thử lại.
6. Nghi cache sai → xóa `.build` hoặc thêm `--clean` (build lạnh lâu hơn); bình thường giữ incremental cho nhanh.
7. Không cần cáp USB / board ở xa → dùng OTA (§2.4): Verify → `.\script\ota\Flash-OTA.ps1` → Monitor xác nhận. Nhớ OTA không nạp được bootloader/partition. Không kết nối được với board (đã reset mất WiFi / khác LAN) → UART luôn nạp được: `.clinerules/flash-device.md`.
8. Khi dùng `-Build` trong `Flash-OTA.ps1`, script **không** truyền `--clean` → giữ incremental cho nhanh (trừ khi cần build lạnh).

## 6. Quy ước khi làm web UI (`data_org/*.htm`)

1. **Không render lại giao diện** (không chụp screenshot bằng Chrome/headless) trừ khi được yêu cầu rõ. Kiểm chứng bằng: kiểm tra tĩnh, unit test, và verify byte-level với thiết bị.
2. Quy trình một trang: sửa `data_org/<page>.htm` → gzip thành `data/<page>.htm.gz` → nạp qua `POST /edit` → so SHA-256 với bản trên thiết bị. Chi tiết + API thấp: `.clinerules/upload-web-page.md`.
3. `data/` là bản đã deploy, `data_org/` là nguồn; trang được phục vụ từ `.htm.gz` qua `serveStatic` (tự thêm `Content-Encoding: gzip`).
4. `mystyle.css` là stylesheet dùng chung lâu đời; các trang mới (`update.htm`, `ws_console.htm`, `home.htm`) tự chứa CSS theo design token trong trang.
5. Sau khi nạp phải **Ctrl+F5** — trình duyệt giữ cache bản cũ.
6. Thiết bị `192.168.3.102:25130` (basic auth `admin/25123`) có lúc **từ chối kết nối TCP** vài chục giây; lỗi mạng thoáng qua không kết luận là lỗi trang.
7. Sửa web UI không cần build firmware — chỉ gzip lại `data/*.gz` rồi nạp qua `POST /edit` (xem `.clinerules/upload-web-page.md`); chỉ dựng lại toàn bộ ảnh LittleFS khi thật cần (xem `.clinerules/flash-device.md`).
8. `home.htm` / `home_user.htm`: **KHÔNG tự động kết nối lại** khi mất socket — đây là thiết kế, không phải lỗi. Thiết bị chỉ phục vụ **2 WebSocket**; client thứ 3 xuất hiện thì thiết bị chủ động ngắt client đầu tiên, nên nếu trang tự reconnect các client sẽ giành socket của nhau. Mất kết nối ⇒ hiện "Đứt Kết Nối", đóng luôn `/events` và chờ người dùng F5. Watchdog chỉ kết luận "mất kết nối" **sau khi đã nhận frame `Page=1`** (nhịp ~2.4 s); trang chi tiết có thể im lặng hợp lệ nên không bị tính. **Đừng thêm lại logic reconnect.**

