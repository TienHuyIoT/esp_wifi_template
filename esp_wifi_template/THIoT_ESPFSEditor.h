#ifndef __ESP_FS_EDITOR_H
#define __ESP_FS_EDITOR_H

/*
Usage ESP32 Sketch Data Upload
1. Download esp32fs.zip
  - link: https://github.com/lorol/arduino-esp32fs-plugin/releases
  - Unpack the tool into "C:\Users\HUY-GAMING\Documents\Arduino\tools\ESP32FS\tool\esp32fs.jar"

2. Download x86_64-w64-mingw32-mklittlefs-295fe9b.zip
  - Link: https://github.com/earlephilhower/mklittlefs/releases
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mklittlefs\mklittlefs.exe

3. Download mkfatfs_v1.0.exe
  - Link: https://github.com/labplus-cn/mkfatfs/releases/tag/v1.0
  - Unpack the file into C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkfatfs\mkfatfs.exe

4. mkspiffs.exe is already in directory
  - C:\Users\HUY-GAMING\Documents\Arduino\hardware\espressif\esp32\tools\mkspiffs\mkspiffs.exe

5. Install LITTLEFS
  - Link: https://github.com/lorol/LITTLEFS
  - Or Manage Libraries with search "LITTLEFS"

6. Upload FS
  - Restart Arduino IDE after any steps above
  - Tools --> ESP32 Sketch Data Upload --> [LITTLEFS, SPIFFS, FATFS, Erase Flash]

List file 
http://192.168.1.10:25130/edit_sdfs?list=/

Read file
http://192.168.1.102:25130/edit_sdfs?edit=/2023/12/20231207.csv
or
http://192.168.1.102:25130/onsd/2023/12/20231207.csv

Download a file
http://192.168.1.102:25130/edit_sdfs?download=/2023/12/20231207.csv

Creation a folder
http://192.168.1.102:25130/edit_sdfs?mkdir25123=/2023/12

Delete File or Folder with backup
http://192.168.1.102:25130/edit_sdfs?remove25123=/2023/12/20231207.csv
http://192.168.1.102:25130/edit_sdfs?rmdir25123=/2023/12

Delete File or Folder without backup
http://192.168.1.102:25130/edit_sdfs?remove25123=/2023/12/20231207.csv&backup=false
http://192.168.1.102:25130/edit_sdfs?rmdir25123=/2023/12&backup=false

Request to edit file from sd card
method get
IP/edit_sdfs?edit=/YLTD.MP3

Request to delete file from sd card
method delete
IP/edit_sdfs , path=/YLTD.MP3

Request to download file from spiffs
method get
IP/edit?download=/log_report.csv

Request to edit file from spiffs
method get
IP/edit?edit=/log_report.txt

Request to delete file from spiffs
method delete
IP/edit , path=/YLTD.MP3

Handle upload:
update.htm
*/

#include <Arduino.h>
#include <functional>
#include <FS.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_APPConfig.h"
#include "THIoT_ESPMessageQueue.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_PFTicker.h"

/* 1: verbose editor diagnostics - [EDITOR] header/param dump per request plus the
 *    [FS_CHUNKED]/[FS_LOCK] console lines.  0 (production, v319 baseline): the
 *    FS_EDITOR_TAG_LOG() trace lines (mutex take/give failures, download start/completion,
 *    duplicate-cancel, listing errors, chunked carry) stay compiled in either way. */
#define FS_EDITOR_DEBUG             0
#define FS_EDITOR_NAND_TYPE         0
#define FS_EDITOR_SD_TYPE           1
#define FS_EDITOR_CHUNKED_RESPONSE  1

/**
 * @brief Usd fs page instead of hex page
 * 1: FS page
 * 0: hex page
*/
#define FS_EDITOR_FS_PAGE           1

#define EDITOR_CONNECTION_TIMEOUT           1
#define EDITOR_DOWNLOAD_FILE_LIMIT          0
#define EDITOR_DOWNLOAD_CLIENT_MAX          8U    /* W1/M2: cap of the download bookkeeping list */
#define EDITOR_LIST_CTX_MAX                 2U    /* W2/C2 + 2026-10-15: max concurrent chunked listings.
                                                     * Measured on 192.168.3.101 (SD root, 172 entries, fw V3.1.9BT9):
                                                     * 1 stream = 5/5 OK, 2 = 10/10 OK, 3 = 9/15 OK, 4 = 1/12 OK.
                                                     * From 3 streams up several responses are reset mid-stream at the
                                                     * same instant (~1.4 KB sent, free heap still ~96 KB, no abort() in
                                                     * this module) - the lwIP/WiFi TX path cannot serve that many
                                                     * concurrent bursts, and the library aborts the short write.  Requests
                                                     * beyond the cap now get an honest 500 SERVER BUSY instead of a broken
                                                     * stream (the editor page lists one directory at a time anyway). */
#define EDITOR_DOWNLOAD_COUNTER_LIMIT       20U
#define EDITOR_DOWNLOAD_COUNTER_RESET_TIMEOUT 10000U   /* ms */
#define EDITOR_DOWNLOAD_TIMEOUT_LIMIT       (60 * 60000U)  /* ms */
#define EDITOR_DOWNLOAD_TIMEOUT_INIT        30000  /* ms */

#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
typedef std::function<bool(AsyncWebServerRequest *request)> fs_ArRequestAuthenticateFunction;
#endif
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
typedef std::function<bool(const char*)> semaphoreFunction;
#endif

class ESPFSEditor;

class AsyncDownloadClient {
    public:

    AsyncDownloadClient(AsyncWebServerRequest *r, ESPFSEditor *editor);
    ~AsyncDownloadClient();

    /* M1: W1 removed the per-client ticker, so the class owns no OS resource any more.
     * Copying stays refused so a future owning member cannot be duplicated silently. */
    AsyncDownloadClient(const AsyncDownloadClient &) = delete;
    AsyncDownloadClient &operator=(const AsyncDownloadClient &) = delete;

    AsyncWebServerRequest *request() const {
        return _request;
    }

    void cancel(bool state) {
        _cancel = state;
    }

    private:
    AsyncWebServerRequest *_request;
    ESPFSEditor *_editor;
    uint32_t _startTime;
    bool _cancel;
};

/* Per-request context of one listing (defined in THIoT_ESPFSEditor.cpp).  2026-10-15: it now
 * only carries the open directory handle + the JSON emission state - no task, no queue. */
struct EditorListCtx;

class ESPFSEditor: public AsyncWebHandler {
  using File = fs::File;
  using FS = fs::FS;
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
    typedef std::function<void(AsyncWebServerRequest*)> fs_status;
    FS _fs;

    ESPFSEditor(const FS& fs, const int fsType, const String& username=String(), const String& password=String());
    /*
      This callback will be called when Update is receiving data
    */
    ESPFSEditor& onProgress(THandlerFunction_Progress fn);
    ESPFSEditor& onStatus(fs_status fn);
    ESPFSEditor& setAuthentication(const char *username, const char *password){  _username = String(username);_password = String(password); return *this; };
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
    ESPFSEditor& onAuthenticate(fs_ArRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}
#endif
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
    ESPFSEditor& onSemaphoreTake(semaphoreFunction fn) { _semaphoreTakeCallback = fn;  return *this;}
    ESPFSEditor& onSemaphoreGive(semaphoreFunction fn) { _semaphoreGiveCallback = fn;  return *this;}
#endif
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 1)
    virtual bool canHandle(AsyncWebServerRequest *request) const override final;
    virtual bool isRequestHandlerTrivial() const override final {return false;}
#else
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
#endif
#if (defined FS_EDITOR_DEBUG) && (FS_EDITOR_DEBUG == 1)
    void debug(AsyncWebServerRequest *request);
#endif
#if (FS_EDITOR_CHUNKED_RESPONSE == 1)
    /* 2026-10-15: the listing is streamed by the response filler (async_tcp); the per-request
     * producer task + queue were removed, so there is no handleFileListTask() any more.  All
     * listing state lives in the per-request EditorListCtx defined in the .cpp. */
#endif
    bool isLockFile();
    void lockFile();
    void unlockFile();
    bool isDownloadLimited();
    void downloadLimited(bool limit);

  private:
    friend AsyncDownloadClient;
    std::list<AsyncDownloadClient> _clients;
    String _username;
    String _password; 
    const char* _uri;
    uint32_t _startTime;
    uint32_t _filesize;
    uint32_t _progress;
#if (FS_EDITOR_FS_PAGE == 0)
    uint8_t *_edit_htm;
    uint32_t _size_htm;
#endif
    uint32_t _renameCounter;
    uint32_t _downloadCounter;
    uint64_t _totalBytes;
    uint64_t _usedBytes;
    uint8_t _fsType;
    bool _isLocked;
    bool _isDownloadLimited;
#if (EDITOR_DOWNLOAD_FILE_LIMIT == 1)
    ticker_t _downloadTimeout;
    ticker_t _downloadResetCounterTimeout;
#endif
    const char* _fsName;
    const char* _pathPage;
    fs_status _status_callback;
    THandlerFunction_Progress _progress_callback;
#if (defined ASYNC_WEB_SERVER_MIDDLEWARE_USED) && (ASYNC_WEB_SERVER_MIDDLEWARE_USED == 0)
    fs_ArRequestAuthenticateFunction _onAuthenticate;
#endif
#if (DATE_LOG_APP_OPTION == MT_API) && (PUMP_LOG_TOTAL_MUTEX_ON == 1) 
    semaphoreFunction _semaphoreTakeCallback;
    semaphoreFunction _semaphoreGiveCallback;
#endif
    void handleStatus(AsyncWebServerRequest *request);
    void handleFileList(AsyncWebServerRequest *request);
    void handleFileCreate(AsyncWebServerRequest *request);
    String lastExistingParent(String path);
    void handleFileDelete(AsyncWebServerRequest *request);
    bool deleteRecursive(String path);
    void cleanUp(String path, const char* recycleFolder = "/RECYCLE_BIN", String search = "", int fileLength = 25, int fileNum = 50);
    bool backupPath(String path, String des = "");
    bool isProhibitedFile(String path);
    bool isBackupFile(String path, bool isDir = true);
};

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
extern ESPFSEditor sdCardEditor;
#endif
extern ESPFSEditor littleFsEditor;

#endif // __ESP_FS_EDITOR_H
