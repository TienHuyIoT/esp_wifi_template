# Vòng đời task & tiết kiệm RAM (ESP32 · Arduino · esp_wifi_template)

> Rule bắt buộc cho mọi agent khi **analysis / design / implement / review**.
> Áp dụng cho mọi code có `xTaskCreate*`, `vTaskDelete`, task động (recreate), ticker, queue liên task.
> Xác nhận: 2026-10-06.

## 0. Ưu tiên số 1: không lãng phí RAM

RAM (heap) là tài nguyên khan hiếm nhất. Một task tồn tại = stack + TCB (heap), **kể cả khi không làm gì**.

- Không tạo task khi chưa có việc thật sự cần làm. Không nuôi task thường trú cho việc thưa thớt.
- Không giữ task sống sau khi hết việc — dùng idle-exit + recreate (§2) để trả RAM.
- Mọi cấp phát phải có chủ; mọi API cấp phát phải kiểm tra kết quả.
- Muốn tăng stack/số task/queue size ⇒ **đo trước** (`uxTaskGetStackHighWaterMark`, task monitor
  `THIoT_ESPTaskMonitor.cpp` đã `addTask(&_http_log_task_handle)`), không đoán.

Số liệu tham chiếu:

| Hạng mục | Giá trị | Nguồn |
|---|---|---|
| `TASK_HTTP_LOG_STACK` | 1024×4+512 = **4608 B** | `THIoT_ESPTaskConfig.h:119` |
| 1 chu kỳ tạo→xoá `http_log` | ≈ **4.7 KB heap** | 4608 + TCB |
| Idle timeout worker `http_log` | **10 s** | `THIoT_APPHttpDataReport.cpp:132` |
| Interval producer mặc định | pump/card **1000 ms** + `random(0,1000)` ms | template `THIoT_FileHandleHttpAPI.cpp:33-34`; arm ticker `THIoT_APPHttpDataReport.cpp:345-347,399-402` |
| Slot ticker tối đa | `TICKER_FUNCTION_HANDLE_MAX = 20` | `THIoT_PFTicker.h:10` |
| Đơn vị stack (ESP-IDF) | **BYTE**, không phải word | — |

## 1. BẮT BUỘC: tách "enqueue" khỏi "ensure consumer"

Behavior chung của dự án. Hai vai trò khác context — không được gộp:

| Vai trò | Chạy ở đâu | Được phép |
|---|---|---|
| **enqueue** | producer / completion callback / `async_tcp` (prio 10) | CHỈ `xQueueSend(..., pdMS_TO_TICKS(0))` + log. **Không** tạo task, **không** `delay()`, **không** `portMAX_DELAY` |
| **ensure consumer** | task khác (`i2c_data` / `loopTask`), chu kỳ retry, **có guard "còn việc"** | `_start_xxx_task()` chỉ khi queue còn item |

```cpp
/* Queue rỗng = 0 byte RAM: không tạo consumer. */
static inline bool http_log_pending() {
  return _http_log_queue && uxQueueMessagesWaiting(_http_log_queue) > 0;
}
...
if (http_log_pending()) { _start_http_log_task(); }
```

Bắt buộc:
1. Callback `putXxxToQueue()` (trên `async_tcp`) chỉ `xQueueSend` timeout **0**; queue đầy → drop + `LOG_E`
   (cursor chưa nhích → gửi lại sau). Không bao giờ block.
2. Ensure consumer phải **có guard**, và phải có **nguồn định kỳ** (ticker/producer). Nếu chỉ dựa vào
   event ACK thì một lần create fail + không còn ACK ⇒ item nằm trong queue **vô hạn**.
3. Null-check queue trước khi gọi API queue (`uxQueueMessagesWaiting` assert nếu NULL).
4. Gọi ensure **trước** khi chạm log store / HTTP client — không giữ lock trong lúc chờ create.

Ví dụ chuẩn: `THIoT_APPHttpDataReport.cpp` — `putPumpLogConfirmToQueue()` / `putCardLedgerToQueue()`
(async_tcp) chỉ enqueue; `pumpLogReportSend()` / `cardDataReportSend()` (i2c_data) là nơi ensure.

**Anti-pattern đã xảy ra thật:** start từ producer **không guard** ⇒ start call không reset idle timer —
worker vẫn thoát sau 10 s rỗng và được tick producer kế tiếp (≤ 2.5 s) tạo lại ⇒ churn tạo/xoá ~4.7 KB
mỗi ~10–12,5 s liên tục (heap gần như luôn bị chiếm + phân mảnh), kể cả khi server chết / không có record.

## 2. Mẫu create/delete task có recreate (đã chạy OK — đừng đổi)

1. Body thoát vòng lặp → `allocate_ticker_once_ms(TASK_DELETE_DELAY_MS, cb)` → **`vTaskDelay(portMAX_DELAY)`** (park).
   Body **KHÔNG** tự `vTaskDelete(NULL)`.
2. Callback ticker (chạy ở `loopTask` qua `ticker_schedule_handler()`) → take mutex → **`vTaskDelete(handle)`
   (external delete)** → `handle = nullptr` → `free(xTaskBuffer)` → `free(xStack)` → give mutex →
   `task_delete_sem_signal(sem)`.
3. `_start_xxx_task()` chờ `task_wait_pending_delete(mutex, sem)` rồi tạo lại.

**Vì sao không self-delete:** `vTaskDelete(NULL)` đẩy TCB vào `xTasksWaitingTermination`, chỉ idle task
unlink; free trong ticker 10 ms sau vẫn quá sớm → idle `vListRemove()` trên vùng đã free →
`LoadProhibited` core 0, `PC 0x400900fd`, `LBEG=LEND=LCOUNT=0`.

**Fallback bắt buộc:** `task_alloc_delete_ticker()` trả `false` → `task_delete_fallback(mutex, &handle)`
(take mutex **có timeout**) rồi `vTaskDelete(NULL)`. Bỏ bước này ⇒ `task_wait_pending_delete()`
(portMAX_DELAY) treo vĩnh viễn.

Helper: `THIoT_ESPTaskHelper.h`.

## 3. BẮT BUỘC kiểm tra deadlock same-task

`ticker_schedule_handler()` chỉ được gọi từ `loop()` ⇒ **mọi callback ticker chạy ở `loopTask`**.
Nếu đường ensure consumer **cũng ở loopTask** ⇒ **KHÔNG dùng** `task_wait_pending_delete()` (waiter block
thì clearer không chạy). Xử lý như `THIoT_ESPPingService`: `if (handle != NULL) return;`, dựa vào chu kỳ
(≥ 1 s ≫ `TASK_DELETE_DELAY_MS = 10 ms`).

Trước khi viết `_start_xxx_task()`: xác định task nào gọi nó. Same-task với clearer ⇒ bỏ wait.

## 4. Static hay dynamic?

- **Mặc định: dynamic** + idle-exit + free ⇒ trả RAM.
- **Static** (`.bss`) = RAM **vĩnh viễn** dù chưa từng chạy — chỉ dùng khi task không bao giờ chết.
- `TASK_HTTP_LOG_STATIC=1` hiện `malloc(TASK_HTTP_LOG_STACK)` (byte, đúng) — giữ nguyên.
- `malloc(TASK_X_STACK)` (byte), **không** nhân `sizeof(StackType_t)` (thừa 4×).
- Idle-exit tạo/xoá 4.7 KB liên tục có thể phân mảnh heap ⇒ cân nhắc "resident sau lần dùng đầu".

## 5. Checklist

- [ ] Không `xTaskCreate*` trong `async_tcp` / ISR / completion path.
- [ ] Không `delay()` / `portMAX_DELAY` trên `async_tcp` (prio 10 = web UI + WebSocket).
- [ ] `_start_xxx_task()` có guard "còn việc".
- [ ] Có nguồn ensure **định kỳ** → tự retry khi create fail.
- [ ] Waiter và clearer của delete **khác task** (hoặc đã bỏ wait theo §3).
- [ ] Mọi cấp phát có đường giải phóng; mọi kết quả API cấp phát được kiểm tra.
- [ ] Số ticker không vượt 20.
- [ ] Comment/tài liệu mô tả đúng context thực tế (nhiều comment cũ đã lệch code).
- [ ] Verify sạch + boot log không `Guru Meditation` / reset lặp.

## 6. Tham chiếu

- `THIoT_ESPTaskHelper.h`, `THIoT_ESPTaskConfig.h`
- `THIoT_APPHttpDataReport.cpp` (enqueue/ensure + worker `http_log`)
- `THIoT_APPAsyncHttpAPI.cpp` (`waitClientIdle()` thay `delay()`)
- `docs/HTTP_LOG_WORKER_START_ANALYSIS.vi.md` (phân tích + số liệu)
- `docs/HTTP_REPORT_ARCHITECTURE_REVIEW.md` (T3, T12, T13)
- `.github/instructions/task-lifecycle-ram.instructions.md` (bản cho Copilot)
