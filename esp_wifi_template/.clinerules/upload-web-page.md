# esp_wifi_template — Nạp trang web (`data_org/*.htm`) lên thiết bị

> Rule chuyên cho việc cập nhật trang web trên board đang chạy. Rule build/flash firmware nằm ở
> `.clinerules/arduino-esp32-build-flash-monitor.md`.
> Xác nhận: 2026-10-10 — nạp một trang qua `POST /edit`, SHA-256 trùng byte với `data/<trang>.htm.gz`.

## 1. Quy trình một trang

1. Sửa nguồn `data_org/<trang>.htm`.
2. Gzip thành `data/<trang>.htm.gz` (bản deploy) — board chỉ có `.gz` và phục vụ `/<trang>.htm` từ chính file đó.
3. Nạp: `POST /edit`, multipart field `data`, filename = `/<trang>.htm.gz` (xem §3).
4. **Kiểm chứng bằng hash** (không phải `HTTP 200`): tải lại file trên board, SHA-256 phải trùng
   `data/<trang>.htm.gz` và giải nén phải khớp `data_org/<trang>.htm`.
5. Trả editor về trạng thái khoá ban đầu (§3).

Board mặc định `192.168.3.102:25130`, Basic auth `admin` / `25123`. **Không hỏi lại người dùng IP/tài khoản**
cho board mặc định; chỉ đổi khi họ nói rõ board khác. IP board xem thêm bằng `arduino-cli board list`
(mục `network`).

## 2. Quy tắc bắt buộc

1. **Sửa `data_org/<trang>.htm` là phải đóng gói lại** `data/<trang>.htm.gz` trước khi nạp.
   Không nạp file `.htm` thô: board chỉ có `.gz` và phục vụ `/<trang>.htm` từ chính file đó.
2. **Chỉ nạp trang thì KHÔNG nạp lại LittleFS và KHÔNG OTA.** `POST /edit` ghi đúng một file, không xoá
   log/cấu hình. Chỉ dùng `Flash-OTA.ps1 -Target littlefs` khi thực sự cần dựng lại toàn bộ ảnh FS.
3. **Thành công phải chứng minh bằng hash**, không phải `HTTP 200`: board trả về đúng
   `data/<trang>.htm.gz` (SHA-256 trùng) và giải nén khớp `data_org/<trang>.htm`.
4. **Trả editor về trạng thái khoá ban đầu.** Mặc định editor đang khoá (`{"lock":true}`); nếu buộc phải để
   mở thì nhắc người dùng khoá lại: `GET /get?param_wifi=editor_lock&lock=1`.
5. **Không gửi lệnh ghi xuống thiết bị để "thử trang"** (`hw_info` ghi, `u8_pack`, `i2c_data_command`,
   `restart`, reset). Thiết bị đang chạy thật: chỉ đọc. Chỉ ghi khi người dùng yêu cầu rõ.
6. **Không chụp screenshot giao diện** (Chrome/headless) trừ khi người dùng yêu cầu; kiểm chứng bằng kiểm tra
   tĩnh + hash byte-level. Khi cần kiểm tra render thì **đọc DOM** qua CDP, đừng lưu ảnh.
7. **Sửa web UI không cần build firmware.** Nhưng nếu thay đổi kèm sửa C++ (endpoint/handler/macro) thì phải
   nạp firmware riêng — hỏi người dùng trước vì board sẽ khởi động lại (§ rule build/flash).
8. Sau khi nạp, người dùng mở trang cần **Ctrl+F5** (trình duyệt giữ cache bản cũ). Trang mới chưa có liên
   kết trong menu là bình thường — mở trực tiếp `http://<ip>:25130/<trang>.htm`.

## 3. API của FS editor

| Việc | Request |
| --- | --- |
| Tải file lên | `POST /edit`, multipart field `data`, filename = đường dẫn đích **có `/` đầu** (`/update.htm.gz`) |
| Tạo file/thư mục rỗng | `PUT /edit`, form `path=/<đường dẫn>` |
| Đổi tên | `PUT /edit`, form `path=/<đích>` + `src=/<nguồn>` |
| Xoá (LittleFS) | `DELETE /edit`, form `path=/<đường dẫn>` |
| Xoá (SD) | `GET /edit_sdfs?remove25123=/<đường dẫn>&backup=false` |
| Liệt kê / dung lượng | `GET /edit?list=/` · `GET /edit?status=1` (LittleFS) · `GET /edit_sdfs?list=/` · `GET /edit_sdfs?status=1` (SD) |
| Khoá / mở khoá ghi | `GET /get?param_wifi=editor_lock[&lock=0|1]` (dùng chung cho cả hai editor) |

> **Hai editor độc lập** (`THIoT_ESPFSEditor.cpp` — `FS_EDITOR_SD_TYPE` → `_uri = "/edit_sdfs"`, còn lại `/edit` =
> LittleFS). File log nằm trên SD (`/history_log.csv`, `/card_http_history_log1.csv`, `/pump_log_*`, …) **không**
> thấy và không xoá được qua `/edit`. `DELETE /edit_sdfs` + form `path=` trả **404** vì `handleFileDelete()` đọc
> `getParam("path", true)` (post param) mà request DELETE không mang body tới handler; đường xoá đúng là nhánh
> legacy `GET /edit_sdfs?remove25123=<path>&backup=false` (gọi `_fs.remove()`).

Middleware của editor: Basic auth (`authenticationCallBack`) + rate-limit **20 request/10 s**
(`AsyncRateLimitMiddleware`) → nạp nhiều trang phải giãn nhịp (chờ ~700 ms/trang), gặp `429` thì chờ
~10 s. Editor bị khoá trả **`423 LOCKED`** cho mọi thao tác ghi.

## 4. Lỗi thường gặp

| Hiện tượng | Xử lý |
| --- | --- |
| `401` | Sai tài khoản Basic auth |
| `423` | Editor đang khoá — mở khoá rồi chạy lại |
| `429` | Vượt rate-limit — chờ ~10 s |
| Không kết nối được | Board đôi lúc từ chối TCP vài chục giây (§6.6 rule build/flash): thử lại, `ping`, `arduino-cli board list` |
| Trang vẫn là bản cũ | Sai tên file (thiếu `.htm.gz`) hoặc cache trình duyệt — đối chiếu hash, đừng chỉ F5 |
| Chrome headless không lên cổng debug | `--user-data-dir` **không được chứa dấu cách** (Chrome báo "Multiple targets are not supported in headless mode") |

## 5. Nếu thay đổi kèm firmware (chỉ khi thực sự cần)

Mặc định dùng **OTA qua IP** — không hỏi lại cách nạp, chỉ hỏi **một câu xác nhận** vì board sẽ khởi động
lại. Thứ tự ưu tiên đầy đủ, cách chẩn đoán và đường UART dự phòng: `.clinerules/flash-device.md`.

```powershell
.\script\ota\Flash-OTA.ps1 -Build          # build + nạp app qua UDP 3232 (board 192.168.3.102)
```

Chỉ dùng cáp COM10 (`arduino-cli compile --upload --port COM10`) khi OTA không dùng được, hoặc khi cần nạp
bootloader/partition (OTA không làm được). Sau khi nạp: kiểm tra endpoint mới bằng
`GET /get?param_wifi=<tên>` rồi mới kết luận. Chi tiết build/flash/monitor:
`.clinerules/arduino-esp32-build-flash-monitor.md`.

## 6. Kiểm chứng một trang sau khi nạp (tuỳ mức cần thiết)

1. **Hash trùng** sau khi nạp (bắt buộc, §1 bước 4).
2. Kiểm tra tĩnh: `GET /<trang>.htm` → 200, không lỗi JS hiển nhiên.
3. Cần kiểm tra render thật: Chrome headless + CDP, URL `http://admin:25123@<ip>:25130/<trang>.htm`, đọc DOM
   và `Runtime.exceptionThrown` (xem §4 nếu headless không lên được cổng debug).
4. Trang có endpoint mới của firmware thì kiểm tra endpoint trước (`GET /get?param_wifi=<tên>`) để biết board
   đã chạy firmware mới hay chưa — endpoint lạ trả về dòng echo `URI: /get … VALUE:…`.
