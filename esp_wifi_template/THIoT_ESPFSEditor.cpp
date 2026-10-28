#include <Arduino.h>
#include <atomic>
#include <new>
#include <sdkconfig.h>
#include "esp_task_wdt.h"
#include <FS.h>
#include "AsyncJson.h"
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPFSEditor.h"
#include "THIoT_ESPMessageList.h"
#include "THIoT_ESPTaskMonitor.h"
#include "THIoT_ESPTaskHelper.h"
#include "THIoT_ESPLogTrace.h"
#if (FS_EDITOR_FS_PAGE == 0)
#include "THIoT_ESPFSEditorHtml.h"
#endif

#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
#define FS_EDITOR_CHUNKED_TAG_CONSOLE(...)    SERIAL_TAG_LOGI("[FS_CHUNKED]", __VA_ARGS__)
#define FS_EDITOR_CHUNKED_TAG_CONSOLE_E(...)  SERIAL_TAG_LOGI("E [FS_CHUNKED]", __VA_ARGS__)
#define FS_EDITOR_LOCK_TAG_CONSOLE(...)       SERIAL_TAG_LOGI("[FS_LOCK]", __VA_ARGS__)
#define FS_EDITOR_DBG_PRINTF_I(...)           SERIAL_TAG_LOGI("[EDITOR]", __VA_ARGS__)
#define FS_EDITOR_DBG_PRINTF_E(...)           SERIAL_TAG_LOGI("E [EDITOR]", __VA_ARGS__)
#else
#define FS_EDITOR_CHUNKED_TAG_CONSOLE(...)    //SERIAL_TAG_LOGI("[FS_CHUNKED]", __VA_ARGS__)
#define FS_EDITOR_CHUNKED_TAG_CONSOLE_E(...)  //SERIAL_TAG_LOGI("E [FS_CHUNKED]", __VA_ARGS__)
#define FS_EDITOR_LOCK_TAG_CONSOLE(...)       //SERIAL_TAG_LOGI("[FS_LOCK]", __VA_ARGS__)
#define FS_EDITOR_DBG_PRINTF_I(...)
#define FS_EDITOR_DBG_PRINTF_E(...)
#endif

#define FS_EDITOR_TAG_MUTEX                 "[EDITOR]"
#define FS_EDITOR_TAG_LOG(...)              FS_TAG_LOGI("[EDITOR]", __VA_ARGS__)

#define EDITOR_NAND_FS_SYSTEM               NAND_FS_SYSTEM
#define EDITOR_SD_FS_SYSTEM                 SD_FS_SYSTEM

static const char TEXT_PLAIN[] PROGMEM = "text/plain";
static const char FS_INIT_ERROR[] PROGMEM = "FS INIT ERROR";
static const char FILE_NOT_FOUND[] PROGMEM = "FileNotFound";
static bool fsOK = true;
String unsupportedFiles = String();
////////////////////////////////
// Utils to return HTTP codes, and determine content-type
#define FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MAX          (uint32_t)(5 * 60)            // Second unit
#define FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MIN          (uint32_t)(10)                // Second unit
#define FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_SEC      (uint32_t)(5U)                // 5 s per 100 KB
#define FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_BYTES    (uint32_t)(100U * 1024U)      // 100 KiB chunk
#define FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_MS       (FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_SEC * 1000U)
#define FS_EDITOR_CONNECTION_TIMEOUT_FOR_SIZE(size)     ((((size) / FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_BYTES) + 1U) * FS_EDITOR_CONNECTION_TIMEOUT_PER_100KB_MS)
#define FS_EDITOR_DOWNLOAD_CLIENT_RX_TIMEOUT            ASYNCWEBSERVER_RX_TIMEOUT
#if (FS_EDITOR_CHUNKED_RESPONSE == 1)
/* 2026-10-15: the listing is streamed by the response filler itself (async_tcp).  The
 * per-request producer task + ESPMessageQueue were removed - a directory walk is the same
 * class of FS work the download filler and serveStatic()/AsyncFileResponse already do on
 * async_tcp (a 22 MB SD file = ~4 000 read() calls on that task), and it is bounded per
 * filler call so the task's occupancy stays in the tens of milliseconds.
 *   EDITOR_LIST_CHUNK_BYTES  - max JSON bytes produced per filler call (work bound)
 *   EDITOR_LIST_ENTRY_MAX    - room reserved for one whole entry (name <= 255 + fields)
 *   EDITOR_LIST_RX_TIMEOUT_S - client stall detector (was CHUNKED_RESPONSE_TIMEOUT/1000) */
#define EDITOR_LIST_CHUNK_BYTES             CONFIG_TCP_MSS
#define EDITOR_LIST_ENTRY_MAX               384U
#define EDITOR_LIST_RX_TIMEOUT_S            15
#if (ESP_IDF_VERSION_MAJOR >= 4) // IDF 4+
#define EDITOR_MAX_TASK_SLEEP (pdMS_TO_TICKS(1000 * CONFIG_ESP_TASK_WDT_TIMEOUT_S) / 4)
#else
#define EDITOR_MAX_TASK_SLEEP (pdMS_TO_TICKS(1000 * CONFIG_TASK_WDT_TIMEOUT_S) / 4)
#endif
/* ============================================================
 * 2026-10-15 - per-request listing context, streamed by the response filler.
 *
 * A listing used to run a dedicated FreeRTOS task (core 0, prio 3) that walked the
 * directory and pushed JSON fragments through an ESPMessageQueue, plus a one-shot delete
 * ticker and a two-way teardown handshake (ownerGone/producerDead/freeClaimed) so that the
 * task, the queue and the request could not tear each other down (the C2 class).
 *
 * The walk now runs inside the response filler, i.e. on async_tcp, exactly like the
 * download filler and serveStatic()/AsyncFileResponse already do.  The work per filler call
 * is bounded by EDITOR_LIST_CHUNK_BYTES and the task watchdog is fed between entries, so the
 * occupancy stays in the tens of ms while the whole listing lives on one task: no task, no
 * queue, no carry buffer, no [FINISH] sentinel, no per-listing watchdog, no cross-task
 * teardown - and ~10 KB less heap churn per listing.
 * ============================================================ */
struct EditorListCtx {
  fs::FS *fs = nullptr;
  File dir;                       /* kept open for the whole response */
  String path;                    /* trace log only */
  bool lastWrite = false;
  bool started = false;           /* '[' already emitted */
  bool firstEntry = true;         /* ',' separator handling */
  bool done = false;              /* walk finished ('[' ... ']' complete) */
  char pending[EDITOR_LIST_ENTRY_MAX];  /* entry bytes not handed over yet */
  size_t pendingLen = 0;
  size_t pendingOff = 0;
};

static std::atomic<uint8_t> g_editorListActive{0};

/* Build one JSON entry byte-identically to the old producer:
 *   {"type":"file","size":"<n>","name":"<name>"[,"last_write":"<ts>"]}
 *   {"type":"dir","name":"<name>"[,"last_write":"<ts>"]}
 * `separator` prepends the ',' that the producer used to append after the previous entry.
 * Returns the number of bytes written (clamped to outCap - 1). */
static size_t editorListBuildEntry(File &entry, bool lastWrite, bool separator,
                                   char *out, size_t outCap) {
  if (outCap == 0U) { return 0; }
  int n = 0;
  if (separator) {
    n += snprintf(out + n, outCap - n, ",");
  }
  const char *name = strrchr(entry.name(), '/');
  if (nullptr == name) { name = entry.name(); }
  if (name[0] == '/') { name++; }
  if (entry.isDirectory()) {
    n += snprintf(out + n, outCap - n, "{\"type\":\"dir\",\"name\":\"%s\"", name);
  } else {
    n += snprintf(out + n, outCap - n, "{\"type\":\"file\",\"size\":\"%u\",\"name\":\"%s\"",
                  (unsigned)entry.size(), name);
  }
  if (lastWrite) {
    time_t t = entry.getLastWrite();
    struct tm *tm_struct = localtime(&t);
    char timeBuff[30];
    snprintf(timeBuff, 30, "%d-%02d-%02d %02d:%02d:%02d", (tm_struct->tm_year) + 1900,
             (tm_struct->tm_mon) + 1, tm_struct->tm_mday, tm_struct->tm_hour,
             tm_struct->tm_min, tm_struct->tm_sec);
    n += snprintf(out + n, outCap - n, ",\"last_write\":\"%s\"", timeBuff);
  }
  n += snprintf(out + n, outCap - n, "}");
  if ((n < 0) || ((size_t)n >= outCap)) {
    /* A name longer than EDITOR_LIST_ENTRY_MAX would truncate the JSON - surface it. */
    FS_EDITOR_TAG_LOG("listing: entry text truncated (%d >= %u)", n, (unsigned)outCap);
    return outCap - 1U;
  }
  return (size_t)n;
}
#endif // #if (FS_EDITOR_CHUNKED_RESPONSE == 1)

/* Force as stop handle upload and actually responding don't send */
static void replyEnd(AsyncWebServerRequest *request) {
  request->send(200);
}

static void replyOK(AsyncWebServerRequest *request) {
  request->send(200, FPSTR(TEXT_PLAIN), "");
}

static void replyOKWithMsg(AsyncWebServerRequest *request, String msg) {
  request->send(200, FPSTR(TEXT_PLAIN), msg);
}

static void replyNotFound(AsyncWebServerRequest *request, String msg) {
  request->send(404, FPSTR(TEXT_PLAIN), msg);
}

static void replyBadRequest(AsyncWebServerRequest *request, String msg) {
  FS_EDITOR_DBG_PRINTF_I("%s", msg.c_str());
  request->send(400, FPSTR(TEXT_PLAIN), msg + "\r\n");
}

static void replyServerError(AsyncWebServerRequest *request, String msg) {
  FS_EDITOR_DBG_PRINTF_I("%s", msg.c_str());
  request->send(500, FPSTR(TEXT_PLAIN), msg + "\r\n");
}

/*
  W1: content type for the editor's chunked file responses.  Mirrors what the library's
  AsyncFileResponse derived from the file name for the file kinds this FS stores.
*/
static const char *editorGetContentType(const String &name) {
  String n = name;
  n.toLowerCase();
  const int dot = n.lastIndexOf('.');
  const String ext = (dot >= 0) ? n.substring(dot + 1) : String();
  if (ext == "htm" || ext == "html") { return "text/html"; }
  if (ext == "txt" || ext == "log")   { return "text/plain"; }
  if (ext == "csv")                   { return "text/csv"; }
  if (ext == "json")                  { return "application/json"; }
  if (ext == "xml")                   { return "application/xml"; }
  if (ext == "css")                   { return "text/css"; }
  if (ext == "js")                    { return "application/javascript"; }
  if (ext == "jpg" || ext == "jpeg")  { return "image/jpeg"; }
  if (ext == "png")                   { return "image/png"; }
  if (ext == "gif")                   { return "image/gif"; }
  if (ext == "ico")                   { return "image/x-icon"; }
  if (ext == "mp3")                   { return "audio/mpeg"; }
  if (ext == "wav")                   { return "audio/wav"; }
  if (ext == "pdf")                   { return "application/pdf"; }
  if (ext == "gz")                    { return "application/gzip"; }
  if (ext == "zip")                   { return "application/zip"; }
  return "application/octet-stream";
}

/*
  W3/C4: per-request upload state.  Stored in request->_tempObject, which the library
  releases with free() (WebRequest.cpp:121-122) - allocate with malloc and never free it
  here.  The old handler-level _progress/_filesize/_startTime mixed concurrent uploads.
*/
struct EditorUploadState {
  uint32_t progress;
  uint32_t fileSize;
  uint32_t startMs;
  bool     failed;
};

#ifdef USE_SPIFFS
/*
   Checks filename for character combinations that are not supported by FSBrowser (alhtough valid on SPIFFS).
   Returns an empty String if supported, or detail of error(s) if unsupported
*/
String checkForUnsupportedPath(String filename) {
  String error = String();
  if (!filename.startsWith("/")) { error += F("!NO_LEADING_SLASH! "); }
  if (filename.indexOf("//") != -1) { error += F("!DOUBLE_SLASH! "); }
  if (filename.endsWith("/")) { error += F("!TRAILING_SLASH! "); }
  return error;
}
#endif

AsyncDownloadClient::AsyncDownloadClient(AsyncWebServerRequest* r, ESPFSEditor *editor)
: _request(r), _editor(editor), _cancel(false) {
  _startTime = millis();
  size_t fSize = _request->_tempFile.size();
  if (_request->hasParam("download"))
  {
    String path = _request->arg("download");
    FS_EDITOR_TAG_LOG("%s request %u from %s:%u Start download path: %.100s , size=%u",
                      _editor->_fsName,
                      _request,
                      _request->client()->remoteIP().toString().c_str(),
                      _request->client()->remotePort(),
                      path.c_str(),
                      fSize);
  }
  /* W1/C3: the per-transfer cap no longer uses a ticker.  It is enforced inside the
   * response filler, which runs on async_tcp - the only task allowed to touch the
   * client.  The old design aborted the client from a loopTask ticker callback, after
   * this very object (and the request) could already have been destroyed. */
}

AsyncDownloadClient::~AsyncDownloadClient() {
  if (_request->hasParam("download") && !_cancel)
  {
    String path = _request->arg("download");
    FS_EDITOR_TAG_LOG("%s request %u Completed download path: %.100s, time = %u(ms)",
                      _editor->_fsName,
                      _request,
                      path.c_str(),
                      millis() - _startTime);
  }
}

ESPFSEditor::ESPFSEditor(const FS& fs, const int fsType, const String& username, const String& password)
:_fs(fs)
,_fsType(fsType)
,_username(username)
,_password(password)
,_startTime(0)
,_filesize(0)
,_renameCounter(1)
,_downloadCounter(0)
,_isLocked(true)
,_isDownloadLimited(true)
,_progress(0)
,_progress_callback(NULL)
,_status_callback(NULL)
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
,_semaphoreTakeCallback(NULL)
,_semaphoreGiveCallback(NULL)
#endif
{
  if(FS_EDITOR_SD_TYPE == fsType)
  {
    _uri = "/edit_sdfs";
    _fsName = "SDFS";
    _pathPage = "/edit_sdfs.htm";
#if (FS_EDITOR_FS_PAGE == 0)
    _edit_htm = (uint8_t*)edit_sd_htm_gz;
    _size_htm = edit_sd_htm_gz_len;
#endif
  }
  else
  {
    _uri = "/edit";
    _fsName = "LittleFS";
    _pathPage = "/edit_littlefs.htm";
#if (FS_EDITOR_FS_PAGE == 0)
    _edit_htm = (uint8_t *)edit_htm_gz;
    _size_htm = edit_htm_gz_len;
#endif
  }
#if (EDITOR_DOWNLOAD_FILE_LIMIT == 1)
  ticker_begin(&_downloadTimeout, EDITOR_DOWNLOAD_TIMEOUT_INIT);
#endif
}

ESPFSEditor& ESPFSEditor::onProgress(THandlerFunction_Progress fn) {
    _progress_callback = fn;
    return *this;
}

ESPFSEditor& ESPFSEditor::onStatus(fs_status fn)
{
  _status_callback = fn;
  return *this;
}

#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
bool ESPFSEditor::canHandle(AsyncWebServerRequest *request) const
#else
bool ESPFSEditor::canHandle(AsyncWebServerRequest *request)
#endif
{
  if(!request->url().equalsIgnoreCase(_uri)) {
    return false;
  }

  String path = "";
#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
  FS_EDITOR_DBG_PRINTF_I("canHandle %s", request->url().c_str());
#endif
  if(request->method() == HTTP_GET) {
    if(request->hasParam("list"))
      return true;
    if(request->hasParam("status"))
      return true;
    if(request->hasParam("remove25123")) {
      return true;
    }
    if(request->hasParam("rmdir25123")) {
      return true;
    }
    if(request->hasParam("mkdir25123")) {
      return true;
    }
    if(request->hasParam("cleanup25123")) {
      return true;
    }
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    if(request->hasParam("edit") || request->hasParam("download")) {
      request->addInterestingHeader("If-Modified-Since");
      return true;
    }
#else
    if(request->hasParam("edit")) {
      path = request->arg("edit");
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/"))
      {
        path = "/" + path;
      }
#endif
      request->_tempFile = _fs.open(path, "r");
      if(!request->_tempFile) {
        return false;
      }
#ifdef ESP32
      if(request->_tempFile.isDirectory()) {
        request->_tempFile.close();
        return false;
      }
#endif
    }
    if(request->hasParam("download")) {
      path = request->arg("download");
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/"))
      {
        path = "/" + path;
      }
#endif
      request->_tempFile = _fs.open(path, "r");
      if(!request->_tempFile) {
        return false;
      }
#ifdef ESP32
      if(request->_tempFile.isDirectory()) {
        request->_tempFile.close();
        return false;
      }
#endif
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
      /* V3.1.7 */
      if (_semaphoreTakeCallback) {
        FS_EDITOR_DBG_PRINTF_I("[HTTP_GET] Path: %s", path.c_str());
        if (_semaphoreTakeCallback(path.c_str())) {
          request->onDisconnect([&]() {
            if (_semaphoreGiveCallback) {
              _semaphoreGiveCallback(nullptr);
            }
            FS_EDITOR_DBG_PRINTF_I("onDisconnect");
          });
        }
      }
#endif
    }
#endif // #if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
    request->addInterestingHeader("If-Modified-Since");
    return true;
  }
  else if(request->method() == HTTP_POST)
  {
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
    _progress = 0;
    _filesize = request->contentLength();
#endif
    return true;
  }      
  else if(request->method() == HTTP_DELETE) {
    return true;
  }
  else if(request->method() == HTTP_PUT) {
    return true;
  }

  return false;
}

void ESPFSEditor::handleRequest(AsyncWebServerRequest *request) {
  if (FS_EDITOR_SD_TYPE == _fsType && !SDCard.statusIsOk()) {
    /* W3/H6b: a 302 redirect would silently drop the body of POST/PUT/DELETE and turn
     * an upload into a body-less GET - answer those with a real status instead. */
    if (request->method() != HTTP_GET) {
      return request->send(503, FPSTR(TEXT_PLAIN), "SD DOWN");
    }
    // edit_sdfs?download=/2025/02/20250210.csv
    // --> edit?download=/2025/02/20250210.csv
    String paramsRequest = "";
    if(request->method() == HTTP_GET) {
      int params = request->params();
      for (size_t i = 0; i < params;) {
        const AsyncWebParameter *p = request->getParam(i);
        paramsRequest += p->name();
        paramsRequest += "=";
        paramsRequest += p->value();
        if (++i < params) {
          paramsRequest += "&";
        }
      }
    }

    String RedirectUrl = request->url();
    RedirectUrl.replace("/edit_sdfs", "/edit");
    if (!RedirectUrl.endsWith("?")) {
      RedirectUrl += "?";
    }
    RedirectUrl += paramsRequest;
    FS_EDITOR_DBG_PRINTF_I("SD is ERROR, redirect from SD to littleFS, url: %s", RedirectUrl.c_str());
    request->redirect(RedirectUrl);
    return;
  }

  String path = "";
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
  if(_username.length() && _password.length()) {
    if(!request->authenticate(_username.c_str(), _password.c_str())) {
      return request->requestAuthentication();
    } 
  } else {
    if(_onAuthenticate) {
      if(!_onAuthenticate(request)) {
        return;
      }
    }
  }
#endif

#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
  FS_EDITOR_DBG_PRINTF_I("handleRequest");
  debug(request);
#endif

  if(request->method() == HTTP_GET) {
    if(request->hasParam("list")) {
      handleFileList(request);
    }
    else if(request->hasParam("status"))
    {
      if (_status_callback) {
        _status_callback(request);
      }
      else {
        handleStatus(request);
      }
    }
    else if(request->hasParam("edit") || request->hasParam("download")) {
      bool isDownloadRecord = false;
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
      if(request->hasParam("edit")) {
        path = request->arg("edit");
      }
      else {
        path = request->arg("download");
        isDownloadRecord = true;
#if (EDITOR_DOWNLOAD_FILE_LIMIT == 1)
        if (ticker_status(&_downloadResetCounterTimeout) == TICKER_STOP) {
          _downloadCounter = 0;
        }
        ticker_begin(&_downloadResetCounterTimeout, EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT);
        ++_downloadCounter;

        if (ticker_status(&_downloadTimeout) == TICKER_RUNNING) {
          if (_isDownloadLimited) {
            FS_EDITOR_LOCK_TAG_CONSOLE("Server Busy");
            return replyServerError(request, "SERVER BUSY");
          }
        }

        FS_EDITOR_LOCK_TAG_CONSOLE("_downloadCounter %u", _downloadCounter);
        if (_downloadCounter >= EDITOR_DOWNLOAD_COUNTER_LIMIT && ticker_status(&_downloadTimeout) == TICKER_STOP) {
          ticker_begin(&_downloadTimeout, EDITOR_DOWNLOAD_TIMEOUT_LIMIT);
          if (_isDownloadLimited) {
            FS_EDITOR_TAG_LOG("Limit download: path %s , %u(s), counter = %u", path.c_str(), EDITOR_DOWNLOAD_TIMEOUT_LIMIT / 1000, _downloadCounter);
          } else {
            FS_EDITOR_TAG_LOG("Too much download path %s , %u(s), counter = %u", path.c_str(), EDITOR_DOWNLOAD_TIMEOUT_LIMIT / 1000, _downloadCounter);
          }
          _downloadCounter = 0;
        }
#endif
      }

#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
#endif

#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1)
      if (_semaphoreTakeCallback) {
        _semaphoreTakeCallback(path.c_str());
      }
#endif
      /* mutex lock timeout has to be less than WDT task CONFIG_TASK_WDT_TIMEOUT_S
        sdkconfig.h current default configure is 5S
      */
      bool isDownloadMutex = mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX);
      if (!isDownloadMutex) {
        FS_EDITOR_LOCK_TAG_CONSOLE("Mutex list take FAIL");
        return replyServerError(request, F("MUTEX BUSY"));
      }

      /* F2: the row must also be released when the transfer COMPLETES, not only when the
       * client disconnects.  `released` is per request (heap, freed by the disconnect
       * callback); the filler and the disconnect callback both give, and the module refuses
       * the second give safely (NOT_OWNER).  If the flag cannot be allocated the old
       * disconnect-only behaviour is kept. */
      bool *released = new (std::nothrow) bool(false);

      request->onDisconnect([this, path, released, request, isDownloadRecord]() 
      {
        FS_EDITOR_DBG_PRINTF_I("onDisconnect");

        _clients.remove_if([request](const AsyncDownloadClient &c) {
          return c.request() == request;
        });
        /* W1: the response is chunked by the editor itself now, so the file handle is
         * ours (per request, via _tempFile) - close it here too for the abort/timeout
         * paths (the filler closes it on EOF). */
        if (request->_tempFile) {
          request->_tempFile.close();
          request->_tempFile = File();
        }

        /* F2: defensive give - a no-op when the filler already released the row. */
        if ((released == nullptr) || (!*released)) {
          mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
          if (released != nullptr) { *released = true; }
        }
        delete released;   /* per-request flag, freed exactly once (here) */
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1)
        if (_semaphoreGiveCallback) {
          _semaphoreGiveCallback(nullptr);
        }
#endif
      });

      // Place here to allow onDisconnect active.
      request->_tempFile = _fs.open(path, "r");
      if (!request->_tempFile)
      {
        FS_EDITOR_LOCK_TAG_CONSOLE("Open file FAIL");
        mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
        request->onDisconnect(nullptr); // deinit callback
        delete released;                /* F2: the response was never sent - free the flag */
        return replyServerError(request, F("OPEN FILE FAIL"));
      }

#ifdef ESP32
      if (request->_tempFile.isDirectory())
      {
        request->_tempFile.close();
        FS_EDITOR_LOCK_TAG_CONSOLE("Path is a dir");
        mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
        request->onDisconnect(nullptr); // deinit callback
        delete released;                /* F2: the response was never sent - free the flag */
        return replyServerError(request, F("PATH IS DIR"));
      }
#endif // #ifdef ESP32
#endif // #if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)

      String fName;
      if (request->hasParam("filename")) {
        fName = request->arg("filename");
      }
      else {
        fName = request->_tempFile.name();
      }
      
      _startTime = millis();
      size_t fSize = request->_tempFile.size();

      // Cancel downloading established before
      const auto iter = std::find_if(std::begin(_clients), std::end(_clients), [fName](const AsyncDownloadClient &c) {
         return fName == c.request()->_tempFile.name();
      });
      if (iter != std::end(_clients)) {
        FS_EDITOR_DBG_PRINTF_I("duplicate download of %s, aborting the older request", fName.c_str());
        FS_EDITOR_TAG_LOG("cancelled download of request %u due to duplicated", iter->request());
        iter->cancel(true);
        iter->request()->client()->abort();
      }

      // insert new download handle (bookkeeping only, no OS resource attached) - cap it (M2)
      while (_clients.size() >= EDITOR_DOWNLOAD_CLIENT_MAX) {
        FS_EDITOR_DBG_PRINTF_E("Client list full, dropping the oldest request %u", _clients.front().request());
        _clients.pop_front();
      }
      _clients.emplace_back(request, this);
      /* W1/D9: the disarmed `_tempObject` ticker branch that lived here was removed - it
       * could only be reached with EDITOR_DOWNLOAD_FILE_LIMIT == 1 (which is 0), and the
       * filler-based cap below replaces it for good. */

      request->client()->setRxTimeout(FS_EDITOR_DOWNLOAD_CLIENT_RX_TIMEOUT); // stall detector (no packets at all)

      /* W1/C3 - the per-transfer cap (same budget as the old ticker: 5 s per 100 KB,
       * clamped to [10 s, 300 s]) is evaluated by the response filler below, i.e. on
       * async_tcp only.  No other task ever touches the client or this request. */
      uint32_t capMs = UINT32_MAX;
#if (EDITOR_CONNECTION_TIMEOUT == 1)
      capMs = FS_EDITOR_CONNECTION_TIMEOUT_FOR_SIZE(fSize);
      if (capMs > FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MAX * 1000U) { capMs = FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MAX * 1000U; }
      if (capMs < FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MIN * 1000U) { capMs = FS_EDITOR_CONNECTION_TIMEOUT_CLOSE_MIN * 1000U; }
      FS_EDITOR_DBG_PRINTF_I("download cap %u ms for file size %u bytes", (unsigned)capMs, (unsigned)fSize);
#endif
      const uint32_t dlStartMs = millis();

      /* W1/M3 - the editor view of /LOG_TRACE.TXT shows only the newest 128 KB; the
       * window is produced by the filler instead of the old seek+setContentLength trick. */
      bool isTraceView = false;
      size_t viewStart = 0;
      size_t viewLen = 0;
      String pathUpper = path;
      pathUpper.toUpperCase();
      if (request->hasParam("edit") && pathUpper == LOG_TRACE_SD_FILE_PATH) {
        if (fSize > LOG_TRACE_EDITOR_VIEW_SIZE) {
          isTraceView = true;
          viewStart = fSize - LOG_TRACE_EDITOR_VIEW_SIZE;
          viewLen = LOG_TRACE_EDITOR_VIEW_SIZE;
        }
      }

      /* W1-fix: the length announced in the response.  W1 served this file through a
       * CHUNKED response; with ESPAsyncWebServer 3.7.7 (ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
       * default 1) that path is gated by the _in_flight_credit counter
       * (WebResponses.cpp:385: `if (_chunked && !_in_flight_credit) return 0;`).  When the
       * credits drain, _ack() refuses every later ack AND poll, so the transfer stalls
       * forever; no packet leaves the device for FS_EDITOR_DOWNLOAD_CLIENT_RX_TIMEOUT
       * seconds and the AsyncTCP rx-timeout (AsyncTCP.cpp:1599) aborts the socket with a
       * RST - the browser reports a failed download after ~6-8 s.  A NON-chunked callback
       * response with a Content-Length uses the same flow-control class as
       * serveStatic()/AsyncFileResponse (which never stalls) and keeps the filler, the
       * per-transfer cap and the 128 KB trace window. */
      const size_t dlLen = isTraceView ? viewLen : fSize;
      const bool isDownload = request->hasParam("download");
      AsyncWebServerResponse *dlResponse = request->beginResponse(
        editorGetContentType(fName),
        dlLen,
        [request, path, released, dlStartMs, capMs, isTraceView, viewStart, viewLen, dlLen](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
          if (maxLen == 0U) { 
            /* Content-Length already satisfied: no room left is a clean end, not an abort. */
            return 0; 
          } /* degenerate; the library always hands us space (R2) */
          if ((capMs != UINT32_MAX) && ((millis() - dlStartMs) > capMs)) {
            FS_EDITOR_TAG_LOG("download cap %u ms hit, aborting the client from the filler", (unsigned)capMs);
            FS_EDITOR_DBG_PRINTF_E("download cap %u ms hit, aborting the client from the filler", (unsigned)capMs);
            if (request->_tempFile) { request->_tempFile.close(); request->_tempFile = File(); }
            /* F2: the transfer ends here - release the file-lock row now. */
            if ((released != nullptr) && (!*released)) { *released = true; mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX); }
            request->client()->abort();
            return 0;
          }
          if (!request->_tempFile) { return 0; }
          if (isTraceView) {
            if (index == 0U) { request->_tempFile.seek(viewStart); }
            if (index >= viewLen) {
              FS_EDITOR_DBG_PRINTF_E("download view window fully streamed, releasing the row now");
              request->_tempFile.close();
              request->_tempFile = File();
              /* F2: view window fully streamed - release the row now. */
              if ((released != nullptr) && (!*released)) { *released = true; mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX); }
              return 0;
            }
            const size_t left = viewLen - index;
            if (maxLen > left) { maxLen = left; }
          }
          const size_t n = request->_tempFile.read(buffer, maxLen);
          if (n == 0U) {
            if (index < dlLen) {
              /* The file turned out shorter than the size() announced in the
               * Content-Length (truncated / replaced while streaming).  The library would
               * wait for the missing bytes forever, so fail fast and loudly instead of
               * hanging until the rx-timeout kills the socket. */
              FS_EDITOR_TAG_LOG("download EOF at %u/%u bytes, aborting the client from the filler",
                                (unsigned)index, (unsigned)dlLen);
              if (request->_tempFile) { request->_tempFile.close(); request->_tempFile = File(); }
              if ((released != nullptr) && (!*released)) { *released = true; mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX); }
              request->client()->abort();
              return 0;
            }
            FS_EDITOR_DBG_PRINTF_E("download EOF, releasing the row now");
            request->_tempFile.close(); /* EOF: the handle is ours, close it here */
            request->_tempFile = File();
            /* F2: normal completion - release the row here instead of waiting for the
             * disconnect event (a missed callback used to leak the row permanently). */
            if ((released != nullptr) && (!*released)) { *released = true; mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX); }
          } else if ((released != nullptr) && (!*released) && ((index + n) >= dlLen)) {
            /* F2: every announced byte has been handed over - release the file-lock row
             * now instead of waiting for the disconnect event. */
            request->_tempFile.close();
            request->_tempFile = File();
            *released = true;
            mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
          }
          return n;
        });
      /* Mirror the library's attachment handling: basename when downloading, inline otherwise. */
      {
        const int nameStart = fName.lastIndexOf('/') + 1;
        const String dlName = fName.substring(nameStart);
        if (isDownload) {
          dlResponse->addHeader("Content-Disposition", "attachment; filename=\"" + dlName + "\"");
          FS_EDITOR_DBG_PRINTF_I("download: Content-Disposition: attachment; filename=\"%s\"", dlName.c_str());
        } else {
          dlResponse->addHeader("Content-Disposition", "inline");
        }
      }
      dlResponse->addHeader("Server", "ESP Async Web Server");
      request->send(dlResponse);
    }
    else if(request->hasParam("remove25123"))
    {
      path = request->getParam("remove25123")->value();
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
#endif

      String backup = "true";
      if(request->hasParam("backup")) {
        backup = request->getParam("backup")->value();
      }

      String des = "";
      if(request->hasParam("des", true)) {
        des = request->getParam("des", true)->value();
      }

      /* W3/H1: report the real result - the old code answered 200 even when the mutex
       * was busy or remove() failed. */
      bool removeOk = false;
      if (backup != "false" && isBackupFile(path, false)) {
        removeOk = backupPath(path, des);
      }
      else {
        if (mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX)) { // locked file
          removeOk = _fs.remove(path);
          mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
        }
      }
      if (removeOk) {
        request->send(200, "text/html", "REMOVE: " + request->getParam("remove25123")->value());
      } else {
        replyServerError(request, F("REMOVE FAIL"));
      }
    }
    else if(request->hasParam("rmdir25123"))
    {
      path = request->getParam("rmdir25123")->value();
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
#endif
      request->client()->setRxTimeout(30);
      bool rmdirOk = deleteRecursive(path); /* W3/H1: real result */
      request->client()->setRxTimeout(FS_EDITOR_DOWNLOAD_CLIENT_RX_TIMEOUT);
      if (rmdirOk) {
        request->send(200, "text/html", "RMDIR: " + request->getParam("rmdir25123")->value());
      } else {
        replyServerError(request, F("RMDIR FAIL"));
      }
    }
    else if(request->hasParam("mkdir25123"))
    {
      path = request->getParam("mkdir25123")->value();
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
#endif
      bool mkdirOk = _fs.mkdir(path);
      if (!mkdirOk && !_fs.exists(path)) { /* W3/H1: an existing directory counts as success */
        return replyServerError(request, F("MKDIR FAIL"));
      }
      request->send(200, "text/html", "MKDIR: " + request->getParam("mkdir25123")->value());
    }
    else if(request->hasParam("cleanup25123")) 
    {
      /**http://192.168.1.101:25130/edit_sdfs?cleanup25123=/&folder=/RECYCLE_BIN&file_length=25&file_num=50&search=BK_
       * http://192.168.1.101:25130/edit_sdfs?cleanup25123=/
      */
      path = request->getParam("cleanup25123")->value();
      String recycleFolder = "/RECYCLE_BIN";
      String search = "";
      int fileLength = 25;
      int fileNum    = 50;
      if(request->hasParam("folder")) {
        recycleFolder = request->getParam("folder")->value();
      }
      if(request->hasParam("file_length")) {
        fileLength = request->getParam("file_length")->value().toInt();
      }
      if(request->hasParam("file_num")) {
        fileNum = request->getParam("file_num")->value().toInt();
      }
      if(request->hasParam("search")) {
        search = request->getParam("search")->value();
      }
      /* W3: cleanUp() is best-effort (per-file renames, no aggregated result) - keep the
       * 200 contract; real failures surface in the trace log. */
      cleanUp(path, recycleFolder.c_str(), search, fileLength, fileNum);
      request->send(200, "text/html", "CLEANUP: " + request->getParam("cleanup25123")->value());
    }
    else {
      const char * buildTime = __DATE__ " " __TIME__ " GMT";
      if (request->header("If-Modified-Since").equals(buildTime)) {
        request->send(304);
      } else {
#if (FS_EDITOR_FS_PAGE == 0)
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", _edit_htm, _size_htm);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Last-Modified", buildTime);
        request->send(response);
#else
        request->send(EDITOR_NAND_FS_SYSTEM, _pathPage, "text/html");
#endif
      }
    }
  } else if(request->method() == HTTP_DELETE) { /* Handle a file/folder deletion request */
    if(request->hasParam("path", true)) {
      handleFileDelete(request);
    } else {
      request->send(404);
    }
  } else if(request->method() == HTTP_POST) { /* Handle a upload request */
    if(request->hasParam("data", true, true))
    {
      path = request->getParam("data", true, true)->value();
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
#endif
      request->_tempFile = File(); /* Prevent potential writing */
      bool exist = _fs.exists(path);
      FS_EDITOR_DBG_PRINTF_I("Exist(%u)", exist);
      if (exist) {
        FS_EDITOR_DBG_PRINTF_I("UPLOADED");
        /* W3/H2: the verdict comes from the real file, not from _progress compared with
         * the whole multipart contentLength - which made the failure branch unreachable
         * (progress is payload only, contentLength includes boundaries/headers). */
        EditorUploadState *st = (EditorUploadState *)request->_tempObject;
        bool ok = false;
        if (st && !st->failed && (st->progress > 0U)) {
          File f = _fs.open(path, "r");
          if (f && (f.size() == st->progress)) { ok = true; }
          if (f) { f.close(); }
        }
        if (ok) {
          replyOK(request);
        }
        else if (st && (0U == st->progress)) {
          replyServerError(request, F("FILE PROHIBITED"));
        } else {
          replyServerError(request, F("FILE UPLOAD FAIL"));
        }
      } else {
        replyServerError(request, F("FILE PROPERTY FAIL"));
      }
    }
    else {
      replyServerError(request, F("NO DATA"));
    }
  } else if(request->method() == HTTP_PUT) { /* Handle the creation/rename of a new file */
    if(request->hasParam("path", true)) { 
      handleFileCreate(request);
    } else {
      request->send(400);
    }
  }
}

void ESPFSEditor::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if(0 == index) {
    String path = filename;
    /* W3/C4: per-request upload state (malloc; request->_tempObject is freed by the
     * library with free(), see WebRequest.cpp:121-122). */
    EditorUploadState *st = (EditorUploadState *)request->_tempObject;
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    if (!st) {
      st = (EditorUploadState *)malloc(sizeof(EditorUploadState));
      if (!st) { return replyEnd(request); }
      request->_tempObject = st;
    }
    st->progress = 0;
    st->fileSize = request->contentLength();
    st->startMs  = millis();
    st->failed   = false;
#endif
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
    if (!path.startsWith("/")) {
      path = "/" + path;
    }
#endif
    if (isProhibitedFile(path)) {
      FS_EDITOR_TAG_LOG("File prohibited");
      /* H3: a real status; sending it still aborts the rest of the multipart body. */
      return request->send(403, FPSTR(TEXT_PLAIN), "FILE PROHIBITED");
    }

    if (FS_EDITOR_SD_TYPE == _fsType && isBackupFile(path, false)) {
      backupPath(path, "/BACKUP");
    }

#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    /* H2: refuse early when the payload cannot fit - the full multipart size is a safe
     * upper bound for the file itself. */
    if (st) {
      const uint64_t totalB = (FS_EDITOR_NAND_TYPE == _fsType) ? EDITOR_NAND_FS_SYSTEM.totalBytes() : EDITOR_SD_FS_SYSTEM.totalBytes();
      const uint64_t usedB  = (FS_EDITOR_NAND_TYPE == _fsType) ? EDITOR_NAND_FS_SYSTEM.usedBytes()  : EDITOR_SD_FS_SYSTEM.usedBytes();
      if ((totalB > usedB) && ((totalB - usedB) < (uint64_t)st->fileSize)) {
        FS_EDITOR_TAG_LOG("upload refused: free %llu < content %llu",
                          (unsigned long long)(totalB - usedB), (unsigned long long)st->fileSize);
        return request->send(507, FPSTR(TEXT_PLAIN), "NO SPACE");
      }
    }
#endif

    /* mutex lock timeout has to be less than WDT task CONFIG_TASK_WDT_TIMEOUT_S
      sdkconfig.h current default configure is 5S
    */
    bool isUploadMutex = mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX);
    if (!isUploadMutex) {
      FS_EDITOR_LOCK_TAG_CONSOLE("Mutex list take FAIL");
      /* H3: 423 Locked tells the UI the truth. */
      return request->send(423, FPSTR(TEXT_PLAIN), "LOCKED");
    }

#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1)
    if (_semaphoreTakeCallback) {
      _semaphoreTakeCallback(path.c_str());
    }
#endif
    request->onDisconnect([this, path, request]() 
    {
      FS_EDITOR_LOCK_TAG_CONSOLE("onDisconnect");
      EditorUploadState *st = (EditorUploadState *)request->_tempObject; /* freed by the library */
      FS_EDITOR_TAG_LOG("%s request %u Uploaded file %s, size = %u/%u, time = %u(ms), failed=%u",
                        _fsName,
                        request,
                        path.c_str(),
                        st ? (unsigned)st->progress : 0U,
                        st ? (unsigned)st->fileSize : 0U,
                        (unsigned)(st ? (millis() - st->startMs) : 0U),
                        (unsigned)(st ? st->failed : 0));

      mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1)
      if (_semaphoreGiveCallback) {
        _semaphoreGiveCallback(nullptr);
      }
#endif
    });

    // Place here to allow onDisconnect active.
    request->_tempFile = _fs.open(path, "w");
    if (!request->_tempFile) {
      FS_EDITOR_LOCK_TAG_CONSOLE("File can't open");
      mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
      request->onDisconnect(nullptr); // deinit callback
      return request->send(500, FPSTR(TEXT_PLAIN), "OPEN FAIL");
    }

    FS_EDITOR_TAG_LOG("%s request %u from %s:%u Start upload path: %.100s , size=%u",
                      _fsName,
                      request,
                      request->client()->remoteIP().toString().c_str(),
                      request->client()->remotePort(),
                      path.c_str(),
                      (unsigned)(st ? st->fileSize : 0U));

#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
    FS_EDITOR_DBG_PRINTF_I("Start file upload (%u)ms", millis());
#endif
  }
  EditorUploadState *st = (EditorUploadState *)request->_tempObject;
  if(request->_tempFile) {
    if(len) {
      esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
      /* W3/H2: a short/failed write used to be invisible (_progress += len was
       * unconditional and the write() result was dropped). */
      const size_t written = request->_tempFile.write(data, len);
      if (st) {
        st->progress += (uint32_t)written;
        if (written != len) { st->failed = true; }
      }
      if (_progress_callback && st) {
        _progress_callback(st->progress, st->fileSize);
      }
      /* Watch dog timer feed */
      wdt_reset();
#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
      FS_EDITOR_DBG_PRINTF_I("Write %u/%u", st ? (unsigned)st->progress : 0U, st ? (unsigned)st->fileSize : 0U);
#endif      
    }
    if(final) {
      esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
      request->_tempFile.close();
      request->_tempFile = File(); /* Prevent potential writing */
      if (_progress_callback && st) {
        _progress_callback(st->fileSize, st->fileSize);
      }
#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
      FS_EDITOR_DBG_PRINTF_I("Finish file upload time = %u(ms)", millis());
#endif
    }
  }
}

#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
void ESPFSEditor::debug(AsyncWebServerRequest *request) {
  if(request->method() == HTTP_GET)
    FS_EDITOR_DBG_PRINTF_I("GET");
  else if(request->method() == HTTP_POST)
    FS_EDITOR_DBG_PRINTF_I("POST");
  else if(request->method() == HTTP_DELETE)
    FS_EDITOR_DBG_PRINTF_I("DELETE");
  else if(request->method() == HTTP_PUT)
    FS_EDITOR_DBG_PRINTF_I("PUT");
  else if(request->method() == HTTP_PATCH)
    FS_EDITOR_DBG_PRINTF_I("PATCH");
  else if(request->method() == HTTP_HEAD)
    FS_EDITOR_DBG_PRINTF_I("HEAD");
  else if(request->method() == HTTP_OPTIONS)
    FS_EDITOR_DBG_PRINTF_I("OPTIONS");
  else
    FS_EDITOR_DBG_PRINTF_I("UNKNOWN");
  FS_EDITOR_DBG_PRINTF_I(" http://%s%s\n", request->host().c_str(), request->url().c_str());

  if(request->contentLength()) {
    FS_EDITOR_DBG_PRINTF_I("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    FS_EDITOR_DBG_PRINTF_I("_CONTENT_LENGTH: %u\n", request->contentLength());
  }

  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++) {
    const AsyncWebHeader* h = request->getHeader(i);
    FS_EDITOR_DBG_PRINTF_I("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }

  int params = request->params();
  for(i=0;i<params;i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()) {
      FS_EDITOR_DBG_PRINTF_I("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()) {
      FS_EDITOR_DBG_PRINTF_I("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      FS_EDITOR_DBG_PRINTF_I("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }
}
#endif

/*
   Return the FS type, status and size info
*/
void ESPFSEditor::handleStatus(AsyncWebServerRequest *request) {
  uint64_t totalBytes, usedBytes;

  String status = request->getParam("status")->value();
  FS_EDITOR_DBG_PRINTF_I("handleStatus: %s", status.c_str());

  if(FS_EDITOR_NAND_TYPE == _fsType)
  {
#ifdef ESP8266
    FSInfo fs_info;
    EDITOR_NAND_FS_SYSTEM.info(fs_info);
    totalBytes = fs_info.totalBytes;
    usedBytes = fs_info.usedBytes;
#elif defined(ESP32)
    if (_totalBytes) {
      totalBytes = _totalBytes;
      usedBytes = _usedBytes;
      _totalBytes = 0;
    } else {
      totalBytes = EDITOR_NAND_FS_SYSTEM.totalBytes();
      usedBytes = EDITOR_NAND_FS_SYSTEM.usedBytes();
    }
#endif
    FS_EDITOR_DBG_PRINTF_I("NandFlash Total space: %lu", totalBytes);
    FS_EDITOR_DBG_PRINTF_I("NandFlash Used space: %lu", usedBytes);
  }
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  else
  {
#ifdef ESP8266
#if (HTH_SFDS_HANDLE)
    FSInfo64 fs_info64;
    EDITOR_SD_FS_SYSTEM.info64(fs_info64);
    totalBytes = fs_info64.totalBytes;
    usedBytes = fs_info64.usedBytes;
#else
    totalBytes = SD.size64();
    usedBytes = 0x40000000ULL; // add for fun
#endif
#elif defined(ESP32)
    if (_totalBytes) {
      totalBytes = _totalBytes;
      usedBytes = _usedBytes;
      _totalBytes = 0;
    } else {
      totalBytes = EDITOR_SD_FS_SYSTEM.totalBytes();
      usedBytes = EDITOR_SD_FS_SYSTEM.usedBytes();
    }
#endif
    FS_EDITOR_DBG_PRINTF_I("SD Total space: %llu", totalBytes);
    FS_EDITOR_DBG_PRINTF_I("SD Used space: %llu", usedBytes);
  }
#endif

  /* W5/M5c: 20 digits + NUL covers the full uint64 range (the old 15 truncated). */
  char buf_ttb[21];
  char buf_udb[21];
  snprintf(buf_ttb, sizeof(buf_ttb), "%llu", totalBytes);
  snprintf(buf_udb, sizeof(buf_udb), "%llu", usedBytes);

  String json;
  json.reserve(128);

  json = "{\"type\":\"";
  json += _fsName;
  json += "\", \"isOk\":";
  /* W5/H5: real booleans - "\"false\"" as a string is truthy in JS, so the UI's
   * INIT ERROR branch could never fire. */
  if (fsOK) {
    json += F("true, \"totalBytes\":\"");
    json += buf_ttb;
    json += F("\", \"usedBytes\":\"");
    json += buf_udb;
    json += "\"";
  } else {
    json += F("false");
  }
  json += F(",\"unsupportedFiles\":\"");
  json += unsupportedFiles;
  json += "\"}";

  request->send(200, "application/json", json);
}

/* W2/C2: _create_chunked_response_task() was removed - handleFileList() now creates the
 * producer task together with its own per-request EditorListCtx (see the ctor/teardown
 * helpers near the top of this file). */

/*
  Return the list of files in the directory specified by the "dir" query string parameter.
  Also demonstrates the use of chunked responses.
  [
    {
        "type": "file",
        "size": "29535",
        "name": "LOG_TRACE.TXT"
    },
    {
        "type": "file",
        "size": "130116",
        "name": "PUMP_RECORD.csv"
    }
  ]
*/
void ESPFSEditor::handleFileList(AsyncWebServerRequest *request) {

  String path = request->getParam("list")->value();
  if (path != "/" && !_fs.exists(path)) { return replyBadRequest(request, F("BAD PATH")); }

  bool lastWrite = false;
  if(request->hasParam("last_write")) {
    lastWrite = request->getParam("last_write")->value().toInt();
  }

  FS_EDITOR_DBG_PRINTF_I("handleFileList: %s", path.c_str());
#ifdef ESP32
  File dir = _fs.open(path);
  if (!dir.isDirectory()) {
    dir.close();
    return replyBadRequest(request, F("NOT DIR"));
  }
  dir.rewindDirectory();
#else
  Dir dir = _fs.openDir(path);
#endif

#if (FS_EDITOR_CHUNKED_RESPONSE == 1)
  dir.close();

  /* Load memory info in advance to avoid LittleFS fail while listing files */
  if (FS_EDITOR_NAND_TYPE == _fsType) {
    _totalBytes = EDITOR_NAND_FS_SYSTEM.totalBytes();
    _usedBytes = EDITOR_NAND_FS_SYSTEM.usedBytes();
  } else {
    _totalBytes = EDITOR_SD_FS_SYSTEM.totalBytes();
    _usedBytes = EDITOR_SD_FS_SYSTEM.usedBytes();
  }

  /* 2026-10-15: one context per request, bounded by EDITOR_LIST_CTX_MAX.  The counter now
   * only tracks live listing contexts (incremented here, decremented in onDisconnect), so it
   * can no longer be held up by a deferred teardown - a request over the cap gets an honest
   * 500 SERVER BUSY instead of a broken stream. */
  if (g_editorListActive.load() >= EDITOR_LIST_CTX_MAX) {
    return replyServerError(request, F("SERVER BUSY"));
  }

  EditorListCtx *ctx = new (std::nothrow) EditorListCtx();
  if (!ctx) {
    return replyServerError(request, F("SERVER ERROR"));
  }
  ctx->fs = &_fs;
  ctx->path = path;
  ctx->lastWrite = lastWrite;
  ctx->dir = _fs.open(path);          /* keep the handle for the whole response */
  if (!ctx->dir || !ctx->dir.isDirectory()) {
    if (ctx->dir) { ctx->dir.close(); }
    delete ctx;
    return replyBadRequest(request, F("NOT DIR"));
  }
  ctx->dir.rewindDirectory();

  ++g_editorListActive;
  FS_EDITOR_CHUNKED_TAG_CONSOLE("Start listing response (chunk %u B, rx timeout %u s)",
                                (unsigned)EDITOR_LIST_CHUNK_BYTES, (unsigned)EDITOR_LIST_RX_TIMEOUT_S);
  request->client()->setRxTimeout(EDITOR_LIST_RX_TIMEOUT_S); // stall detector (no packets at all)
  /** Response streamed by the filler below, NON-chunked on purpose: ESPAsyncWebServer 3.7.7
   * gates chunked responses with the ASYNCWEBSERVER_USE_CHUNK_INFLIGHT credit counter
   * (WebResponses.cpp:385) which can stall the transfer for good once the credits drain -
   * the same failure the file download hit (see the W1-fix note in handleRequest).  A
   * callback response without Content-Length (len = 0 -> _sendContentLength = false) has no
   * such gate and ends with a graceful close (`client()->close(true)` -> FIN) once the filler
   * returns 0 after the closing ']'; the library already sends `Connection: close`, so the
   * body is close-delimited. */
  AsyncWebServerResponse *response = request->beginResponse("application/json", 0, [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    (void)index;
    if (maxLen == 0U || ctx->done) { return 0; }
    esp_task_wdt_reset(); /* async_tcp is subscribed to the task WDT */

    FS_EDITOR_CHUNKED_TAG_CONSOLE("Listing filler maxLen %u", (unsigned)maxLen);

    size_t n = 0;
    if (!ctx->started) {
      ctx->started = true;
      buffer[n++] = '[';              /* maxLen >= 1 here */
    }

    /* 1) hand over the remainder of the previous entry (or the closing ']') first */
    if (ctx->pendingOff < ctx->pendingLen) {
      const size_t left = ctx->pendingLen - ctx->pendingOff;
      const size_t room = maxLen - n;
      const size_t take = (left > room) ? room : left;
      memcpy(buffer + n, ctx->pending + ctx->pendingOff, take);
      n += take;
      ctx->pendingOff += take;
      if (ctx->pendingOff < ctx->pendingLen) { return n; }
      ctx->pendingLen = 0;
      ctx->pendingOff = 0;
    }
    if (ctx->done) { return n; }      /* the ']' flushed above was the last byte */

    /* 2) walk the directory, bounded by EDITOR_LIST_CHUNK_BYTES per filler call so async_tcp
     *    is never held for more than a few entries - the download filler and
     *    serveStatic()/AsyncFileResponse do the same kind of FS work on this very task. */
    while (n < EDITOR_LIST_CHUNK_BYTES) {
      esp_task_wdt_reset();
      File entry = ctx->dir.openNextFile();
      if (!entry) {
        ctx->done = true;
        ctx->dir.close();
        if (n < maxLen) { buffer[n++] = ']'; }
        else { ctx->pending[0] = ']'; ctx->pendingLen = 1U; ctx->pendingOff = 0; }
        FS_EDITOR_CHUNKED_TAG_CONSOLE("Listing response finished");
        return n;
      }
      const size_t len = editorListBuildEntry(entry, ctx->lastWrite, !ctx->firstEntry,
                                              ctx->pending, sizeof(ctx->pending));
      ctx->firstEntry = false;
      const size_t room = maxLen - n;
      const size_t take = (len > room) ? room : len;
      memcpy(buffer + n, ctx->pending, take);
      n += take;
      if (take < len) {                /* keep the tail for the next call */
        ctx->pendingOff = take;
        ctx->pendingLen = len;
        return n;
      }
      ctx->pendingLen = 0;
      ctx->pendingOff = 0;
    }
    return n;

  });

  request->onDisconnect([ctx]() 
  {
    FS_EDITOR_CHUNKED_TAG_CONSOLE("onDisconnect");
    /* 2026-10-15: the whole listing runs on async_tcp, so this callback is the only owner of
     * the context - no cross-task handshake, no ticker, nothing to defer. */
    if (ctx->dir) { ctx->dir.close(); }
    if (g_editorListActive.load() > 0U) { --g_editorListActive; }
    delete ctx;
  });

  response->addHeader("Server","ESP Async Web Server");
  request->send(response);
#else
#error "FS_EDITOR_CHUNKED_RESPONSE=0 (full-JSON listing) is unsupported: it materialises the whole directory into one AsyncJsonResponse and is OOM-prone (review M5b). Flip the flag back to 1 or revive the old body deliberately."
  path.clear();
  /** Response use json buffer */
  AsyncJsonResponse * response = new AsyncJsonResponse(true);
  response->addHeader("Server","ESP Async Web Server");
  JsonArray& root = response->getRoot();

#ifdef ESP32
  File entry = dir.openNextFile();
  while(entry) {
#else
  while(dir.next()) {
    fs::File entry = dir.openFile("r");
#endif
#ifdef USE_SPIFFS
    String error = checkForUnsupportedPath(dir.fileName());
    if (error.length() > 0) {
      FS_EDITOR_DBG_PRINTF_I("Ignoring %s %s", error.c_str(), dir.fileName().c_str());
      continue;
    }
#endif
    esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
    JsonObject& root_0 = root.createNestedObject();

    if (entry.isDirectory()) {
      root_0["type"] = "dir";
    } else {
      root_0["type"] = "file";
      root_0["size"] = String(entry.size());
    }

    const char* name = strrchr(entry.name(), '/');
    if (nullptr == name) {
      name = entry.name();
    }
    // Always return names without leading "/"
    if (name[0] == '/') {
      name++;
    }

    /*
      Should not use root_0["name"] = name;
      Due to root_0["name"] shall refer to memory of entry.name() and destroy later on entry.close().
    */
    root_0["name"] = String(name);

    if (lastWrite) {
      time_t t = entry.getLastWrite();
      struct tm *tm_struct = localtime(&t);
      char timeBuff[30];
      snprintf(timeBuff, 30, "%d-%02d-%02d %02d:%02d:%02d", (tm_struct->tm_year) + 1900, (tm_struct->tm_mon) + 1, tm_struct->tm_mday, tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);
      root_0["last_write"] = timeBuff;
    }

    entry.close();
#ifdef ESP32
    entry = dir.openNextFile();
#endif
  }

#ifdef ESP32
  dir.close();
#endif

  response->setLength();
  request->send(response);
#endif // #if (FS_EDITOR_CHUNKED_RESPONSE == 1)
}

/* 2026-10-15: handleFileListTask() - the per-request producer task that walked the directory
 * and pushed ~1.4 KB JSON fragments through an ESPMessageQueue - was removed.  The walk now
 * runs inside the response filler on async_tcp, bounded by EDITOR_LIST_CHUNK_BYTES per call
 * (see EditorListCtx / editorListBuildEntry near the top of this file).  This deleted: the
 * task, the queue + carry buffer, the [FINISH] sentinel, the delete-ticker handshake and the
 * per-listing watchdog - i.e. the whole cross-task teardown (the C2 class) with them. */


/*
   As some FS (e.g. LittleFS) delete the parent folder when the last child has been removed,
   return the path of the closest parent still existing
*/
String ESPFSEditor::lastExistingParent(String path) {
  while (!path.isEmpty() && !_fs.exists(path)) {
    if (path.lastIndexOf('/') > 0) {
      path = path.substring(0, path.lastIndexOf('/'));
    } else {
      path = String();  // No slash => the top folder does not exist
    }
  }
  FS_EDITOR_DBG_PRINTF_I("Last existing parent: %s", path.c_str());
  return path;
}

/*
   Handle the creation/rename of a new file
   Operation      | req.responseText
   ---------------+--------------------------------------------------------------
   Create file    | parent of created file
   Create folder  | parent of created folder
   Rename file    | parent of source file
   Move file      | parent of source file, or remaining ancestor
   Rename folder  | parent of source folder
   Move folder    | parent of source folder, or remaining ancestor
*/
void ESPFSEditor::handleFileCreate(AsyncWebServerRequest *request) {
  String path = request->getParam("path", true)->value();
  if (path.isEmpty()) { return replyBadRequest(request, F("PATH ARG MISSING")); }

#ifdef USE_SPIFFS
  if (checkForUnsupportedPath(path).length() > 0) { return replyServerError(request, F("INVALID FILENAME")); }
#endif

  if (path == "/") { return replyBadRequest(request, F("BAD PATH")); }
  if (_fs.exists(path)) { return replyBadRequest(request, F("PATH FILE EXISTS")); }

  String src = request->arg("src");
  if (src.isEmpty()) {
    // No source specified: creation
    FS_EDITOR_DBG_PRINTF_I("handleFileCreate: %s", path.c_str());
    if (path.endsWith("/")) {
      // Create a folder
      path.remove(path.length() - 1);
      if (!_fs.mkdir(path)) { return replyServerError(request, F("MKDIR FAILED")); }
      FS_EDITOR_TAG_LOG("%s Create a folder %s", _fsName, path.c_str());
    } else {
      // Create a file
      fs::File file = _fs.open(path, "w");
      if (file) {
        file.write((uint8_t)0x00);
        file.close();
      } else {
        return replyServerError(request, F("CREATE FAILED"));
      }
      FS_EDITOR_TAG_LOG("%s Create a file %s", _fsName, path.c_str());
    }
    if (path.lastIndexOf('/') > -1) { path = path.substring(0, path.lastIndexOf('/')); }
    replyOKWithMsg(request, path);
  } else {
    // Source specified: rename
    if (src == "/") { return replyBadRequest(request, F("BAD SRC")); }
    if (!_fs.exists(src)) { return replyBadRequest(request, F("SRC FILE NOT FOUND")); }

    FS_EDITOR_DBG_PRINTF_I("handleFileCreate: %s from %s", path.c_str(), src.c_str());

    if (path.endsWith("/")) { path.remove(path.length() - 1); }
    if (src.endsWith("/")) { src.remove(src.length() - 1); }

    /* Inspect the SOURCE, not the destination: for a rename `path` is the new
     * name and does not exist yet, so opening it always failed and every rename
     * answered HTTP 500 (FILE OPEN FAILED). */
    File file = _fs.open(src, "r");
    if (!file) {
      return replyServerError(request, F("FILE OPEN FAILED"));
    }
    bool isDir = file.isDirectory();
    file.close();
    if (!isDir) {
      /* is file */
      if (isProhibitedFile(src)) {
        FS_EDITOR_TAG_LOG("%s FAIL rename %s to %s", _fsName, src.c_str(), path.c_str());
        return replyServerError(request, F("FILE RENAME PROHIBITED"));
      }
    }

    bool status = mutexFileList.take(src.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX); // locked file
    if (status) {
      status = _fs.rename(src, path);
      mutexFileList.give(src.c_str(), FS_EDITOR_TAG_MUTEX);
    }
    if (!status) { return replyServerError(request, F("RENAME FAILED")); }
    replyOKWithMsg(request, lastExistingParent(src));
    FS_EDITOR_TAG_LOG("%s Rename %s to %s", _fsName, src.c_str(), path.c_str());
  }
}

bool ESPFSEditor::isProhibitedFile(String path) {
  bool status = false;
  String fileName = path.substring(path.lastIndexOf('/'));
  FS_EDITOR_LOCK_TAG_CONSOLE("fileName %s", fileName.c_str());
  if (fileName.startsWith("/BK_") || fileName == LOG_TRACE_SD_FILE_PATH) {
    status = true;
  }
  return (status & _isLocked);
}

bool ESPFSEditor::isBackupFile(String path, bool isDir) {
  if (isDir) {
    File file = _fs.open(path, "r");
    if (!file) {
      FS_EDITOR_DBG_PRINTF_I("File %s can't open", path.c_str());
      return false;
    }
    isDir = file.isDirectory();
    file.close();
  }

  path.toLowerCase();
  // Path is a Folder or extension file with (".csv" or ".txt") and path exclude "/del_".
  bool status = (isDir || path.endsWith(".csv") || path.endsWith(".txt")) && (path.indexOf("/del_") == -1);
  return (status & _isLocked);
}

bool ESPFSEditor::backupPath(String path, String des) {
  if (path == "/") return false;
  if (!path.startsWith("/")) return false;
  if (path.endsWith("/")) { path.remove(path.length() - 1); }

  if (des == "/") return false;
  if (!des.startsWith("/") && des.length() >= 3) return false;
  if (des.endsWith("/")) { des.remove(des.length() - 1); }

  if(!(_fs.exists(path))) {
      return false;
  }

  File file = _fs.open(path, "r");
  if (!file) {
    return false;
  }
  
  bool isDir = file.isDirectory();
  file.close();
  if (isDir) {
    FS_EDITOR_DBG_PRINTF_I("Rejected due to the path %s is a folder", path.c_str());
    return false;
  }

  int index = path.lastIndexOf('/');
  index++; /* pass '/' */

  String fileName = path.substring(index);

  if (fileName.startsWith("BK_")) {
    // reject delete with prefix BK_
    FS_EDITOR_TAG_LOG("%s Rejected Delete file %s", _fsName, path.c_str());
    return false;
  }
  
  if (fileName.startsWith("DEL_")) {
    // If it's a plain file, delete it
    if (mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX)) { // locked file
      _fs.remove(path);
      FS_EDITOR_TAG_LOG("%s Delete file %s", _fsName, path.c_str());
      mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
      return true;
    }
    return false;
  }

  String folderPath;
  String timeName;
  if (FS_EDITOR_SD_TYPE == _fsType) {
    int dot = path.lastIndexOf('.');
    folderPath = path.substring(0, dot); /* Without '.' */
    timeName = "/BK_";
    timeName += ESPTime.yymmddToStr();
    timeName += "_";
    timeName += ESPTime.hhMMssToStr();
    timeName += '_';
    timeName += String(_renameCounter++);
    timeName += '_';
    timeName += fileName;
  } else {
    folderPath = "/BACKUP";
    timeName = "/";
    timeName += ESPTime.hhMMssToStr();
    timeName += '_';
    timeName += fileName;
  }

  if (des.length() >= 3) { /* 3: "/0X" */
    folderPath = des;
  }

  FS_EDITOR_DBG_PRINTF_I("Folder path = %s", folderPath.c_str());
  if(!(_fs.exists(folderPath))) {
      FS_EDITOR_DBG_PRINTF_I("mkdir %s", folderPath.c_str());
      _fs.mkdir(folderPath);
  }

  String timePath = folderPath + timeName;

  bool status = mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX); // locked file
  if (status) {
    status = _fs.rename(path, timePath);
    mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
  }

  if (status) {
    FS_EDITOR_TAG_LOG("%s Backup file %s to %s", _fsName, path.c_str(), timePath.c_str());
  } else {
    FS_EDITOR_TAG_LOG("%s Fail to backup file %s to %s", _fsName, path.c_str(), timePath.c_str());
  }
  return status;
}

/*
   Delete the file or folder designed by the given path.
   If it's a file, delete it.
   If it's a folder, delete all nested contents first then the folder itself

   IMPORTANT NOTE: using recursion is generally not recommended on embedded devices and can lead to crashes (stack overflow errors).
   This use is just for demonstration purpose, and FSBrowser might crash in case of deeply nested filesystems.
   Please don't do this on a production system.
*/
bool ESPFSEditor::deleteRecursive(String path) {
  FS_EDITOR_DBG_PRINTF_I("Delete path %s", path.c_str());
  if (path.isEmpty() || path == "/") return false;

  if(!(_fs.exists(path))) {
    FS_EDITOR_DBG_PRINTF_I("Path is not exist");
    return false;
  }

  File file = _fs.open(path, "r");
  if (!file) {
    FS_EDITOR_DBG_PRINTF_I("File %s can't open", path.c_str());
    return false;
  }
  bool isDir = file.isDirectory();
  file.close();

  // If it's a plain file, delete it
  if (!isDir) {
    if (mutexFileList.take(path.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX)) { // locked file
      _fs.remove(path);
      mutexFileList.give(path.c_str(), FS_EDITOR_TAG_MUTEX);
      FS_EDITOR_TAG_LOG("%s Delete file %s", _fsName, path.c_str());
      return true;
    } else {
      FS_EDITOR_TAG_LOG("%s Fail to delete file %s", _fsName, path.c_str());
      return false;
    }
  }

  // Otherwise delete its contents first
#ifdef ESP32
  File dir = _fs.open(path);
  if (!dir) {
    FS_EDITOR_TAG_LOG("deleteRecursive: cannot open dir %s", path.c_str());
    return false;
  }
  File entry = dir.openNextFile();
  while (entry) { 
    esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
    String name = entry.name();
    /* W3x: this FS reports child names as base names ("/test.txt", no parent prefix) -
     * rebuild the absolute path from the walked directory. */
    const char *entryName = entry.name();
    const char *entryBase = strrchr(entryName, '/');
    entryBase = entryBase ? (entryBase + 1) : entryName;
    name = (path == "/") ? (String("/") + entryBase) : (path + "/" + entryBase);
    isDir = entry.isDirectory();
    entry.close();
    if (isDir) {
      deleteRecursive(name); 
    }
    else {
      if (mutexFileList.take(name.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX)) { // locked file
        /* W3x: surface the result - this branch used to be silent. */
        if (_fs.remove(name)) {
          FS_EDITOR_TAG_LOG("deleteRecursive: removed %s", name.c_str());
        } else {
          FS_EDITOR_TAG_LOG("deleteRecursive: remove FAILED %s", name.c_str());
        }
        mutexFileList.give(name.c_str(), FS_EDITOR_TAG_MUTEX);
      } else {
        FS_EDITOR_TAG_LOG("deleteRecursive: LOCKED, skip %s", name.c_str());
      }
    }
    entry = dir.openNextFile();  
  }
  dir.close();
#else
  Dir dir = _fs.openDir(path);
  while (dir.next()) { deleteRecursive(path + '/' + dir.fileName()); }
#endif

  // Then delete the folder itself
  bool status = _fs.rmdir(path);
  if (status) {
    FS_EDITOR_TAG_LOG("%s Delete folder %s", _fsName, path.c_str());
  } else {
    FS_EDITOR_TAG_LOG("%s rmdir FAILED %s", _fsName, path.c_str());
  }
  return status;
}

/**http://192.168.1.101:25130/edit_sdfs?cleanup25123=/&folder=/RECYCLE_BIN&file_length=25&file_num=50&search=BK_
 * http://192.168.1.101:25130/edit_sdfs?cleanup25123=/
*/
void ESPFSEditor::cleanUp(String path, const char* recycleFolder, String search, int fileLength, int fileNum) {
  FS_EDITOR_DBG_PRINTF_I("Clean up path %s to folder %s, f_length = %u, f_num = %u, search = %s", path.c_str(), recycleFolder, fileLength, fileNum, search.c_str());
  if (path.isEmpty()) return;

  if(!(_fs.exists(path))) {
    FS_EDITOR_DBG_PRINTF_I("Path is not exist");
    return;
  }

  File file = _fs.open(path, "r");
  if (!file) {
    FS_EDITOR_DBG_PRINTF_I("File %s can't open", path.c_str());
    return;
  }
  bool isDir = file.isDirectory();
  file.close();
  if (!isDir) {
    return;
  }

  String folderPath = recycleFolder;
  if (folderPath.length() < 5 /*any*/ || fileLength < 5 /*.csv*/) {
    return;
  }
  if(!(_fs.exists(folderPath))) {
      FS_EDITOR_DBG_PRINTF_I("mkdir %s", folderPath.c_str());
      _fs.mkdir(folderPath);
  }

  int cntLimit = fileNum;  /* Definition a maximum number of file once clean up */
  File dir = _fs.open(path);
  if (!dir) {
    FS_EDITOR_DBG_PRINTF_I("Dir %s can't open", path.c_str());
    return;
  }
  dir.rewindDirectory();
  File entry = dir.openNextFile();
  while (entry && cntLimit > 0) {
    esp_task_wdt_reset(); /* Avoid reset by wdt of the task */
    String name = entry.name();
    /* W3x: same base-name reconstruction as deleteRecursive() - rename() needs the
     * absolute path of the entry. */
    {
      const char *entryName = entry.name();
      const char *entryBase = strrchr(entryName, '/');
      entryBase = entryBase ? (entryBase + 1) : entryName;
      name = (path == "/") ? (String("/") + entryBase) : (path + "/" + entryBase);
    }
    isDir = entry.isDirectory();
    entry.close();
    if (!isDir) {
      int index = name.lastIndexOf('/');
      index++; /* pass '/' */
      String fileName = name.substring(index);
      FS_EDITOR_DBG_PRINTF_I("Filename length = %u", fileName.length());
      if (fileName.length() >= fileLength || (search.length() > 0 && fileName.indexOf(search) != -1)) { /* Limitation the length of backup file */
        --cntLimit;
        String recyclePath = folderPath + "/" + fileName;
        FS_EDITOR_DBG_PRINTF_I("Rename file %s to %s", name.c_str(), recyclePath.c_str());
        if (mutexFileList.take(name.c_str(), EDITOR_MAX_TASK_SLEEP, FS_EDITOR_TAG_MUTEX)) { // locked file
          _fs.rename(name, recyclePath);
          mutexFileList.give(name.c_str(), FS_EDITOR_TAG_MUTEX);
        }
      }
    }
    entry = dir.openNextFile();  
  }
  dir.close();
}

/*
   Handle a file/folder deletion request
   Operation      | req.responseText
   ---------------+--------------------------------------------------------------
   Delete file    | parent of deleted file, or remaining ancestor
   Delete folder  | parent of deleted folder, or remaining ancestor
*/
void ESPFSEditor::handleFileDelete(AsyncWebServerRequest *request) {
  String path = request->getParam("path", true)->value();
  if (path.isEmpty() || path == "/") { return replyBadRequest(request, F("BAD PATH")); }

  if (!_fs.exists(path)) { return replyNotFound(request, FPSTR(FILE_NOT_FOUND)); }

  String backup = "true";
  if(request->hasParam("backup", true)) {
    backup = request->getParam("backup", true)->value();
  }

  String des = "";
  if(request->hasParam("des", true)) {
    des = request->getParam("des", true)->value();
  }

  FS_EDITOR_DBG_PRINTF_I("handleFileDelete: %s, backup = %s", path.c_str(), backup.c_str());

  bool status;
  if (backup != "false" && isBackupFile(path, true)) {
    status = backupPath(path, des);
  }
  else {
    /* backup == false */
    request->client()->setRxTimeout(30);
    status = deleteRecursive(path);
    request->client()->setRxTimeout(FS_EDITOR_DOWNLOAD_CLIENT_RX_TIMEOUT);
  }

  if (status) {
    replyOKWithMsg(request, lastExistingParent(path));
  } else {
    /* W3/H6d: distinguish the causes - a busy lock is not "Prohibited PATH". */
    if (_fs.exists(path)) {
      replyServerError(request, F("DELETE FAILED (locked or busy)"));
    } else {
      replyNotFound(request, FPSTR(FILE_NOT_FOUND));
    }
  }
}

bool ESPFSEditor::isLockFile() { 
  return _isLocked; 
}

void ESPFSEditor::lockFile() { 
  _isLocked = true; 
}

void ESPFSEditor::unlockFile() { 
  _isLocked = false; 
}

bool ESPFSEditor::isDownloadLimited() {
  return _isDownloadLimited; 
}

void ESPFSEditor::downloadLimited(bool limit) {
  _isDownloadLimited = limit;
}

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
ESPFSEditor sdCardEditor(EDITOR_SD_FS_SYSTEM, FS_EDITOR_SD_TYPE);
#endif
ESPFSEditor littleFsEditor(EDITOR_NAND_FS_SYSTEM, FS_EDITOR_NAND_TYPE);