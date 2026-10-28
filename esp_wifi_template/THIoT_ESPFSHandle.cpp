#include <FS.h>
#include <new>
#include "esp_task_wdt.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_ESPTimeSystem.h" 
#include "THIoT_ESPMessageList.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1)
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define FS_HANDLE_CONSOLE(...)      //CONSOLE_LOGI(__VA_ARGS__)
#define FS_HANDLE_TAG_CONSOLE(...)  //CONSOLE_TAG_LOGI("[FS_HANDLE]", __VA_ARGS__)
#define FS_HANDLE_TAG_LOG(...)      FS_TAG_LOGI("[FS_HANDLE]", __VA_ARGS__)

#define _list_handle_take(x)     do{if ((x) != NULL) {xSemaphoreTakeRecursive((x), portMAX_DELAY);}}while(0)
#define _list_handle_give(x)     do{if ((x) != NULL) {xSemaphoreGiveRecursive((x));}}while(0)
#define _list_handle_delete(x)   do{if ((x) != NULL) {vSemaphoreDelete((x)); (x) = NULL;}}while(0)
#define _list_handle_init(x)     do{if ((x) == NULL) {(x) = xSemaphoreCreateRecursiveMutex();}}while(0)
#define _list_handle_isnull_asset(x)   (NULL == (x))

const char FS_HANDLE_TAG_MUTEX[] PROGMEM = "[FS_HANDLE]";

ESPFSHandle::ESPFSHandle(/* args */) {}

ESPFSHandle::~ESPFSHandle() {}

bool ESPFSHandle::copy(FS *fs_src, String path_src, String path_des, FS *fs_des) {
  /* H4: this was a stub that returned true while doing nothing.  No caller exists
   * (verified 2026-10-09), so report the truth until a real implementation lands. */
  (void)fs_src; (void)path_src; (void)path_des; (void)fs_des;
  wdt_reset();
  return false;
}

bool ESPFSHandle::isInhibited(FS *fs, String path) {
  if (((&SD_FS_SYSTEM == fs) && (path.length() <= 5)) ||   // SD card: /2024, /2025
      ((&NAND_FS_SYSTEM == fs) && (path.length() <= 2)) || // NAND memory
      (path.indexOf("System Volume Information") != -1) || // SD card info
      (path.indexOf("CONFIG") != -1)                       // Internal memory
  )
  {
    FS_HANDLE_TAG_CONSOLE("Inhibited path %s", path.c_str());
    return true;
  }
  return false;
}

bool ESPFSHandle::cleanUp(FS &fs, uint64_t sLimit) {
  wdt_reset();
  /* nothrow: the build has exceptions enabled, so a plain new would terminate the firmware
   * on a boot-time OOM instead of degrading (LOW-2). */
  ESPMessageList *dirHandler = new (std::nothrow) ESPMessageList(FS_HANDLE_TAG_MUTEX);
  if (dirHandler == nullptr) {
    FS_HANDLE_TAG_LOG("cleanUp: no memory for the directory list");
    return false;
  }
  listDir(fs, "/", 0, dirHandler, sLimit);
  bool allDeleted = true;
  while (dirHandler->available()) {
    wdt_reset();
    /* C7c: the removed esp_task_wdt_reset() here was a no-op - cleanUp() runs from
     * loopTask, which is not subscribed to the TWDT (IDF 4.4 returns ESP_ERR_NOT_FOUND
     * for an unsubscribed caller).  wdt_reset() above feeds ESPWdt (project watchdog). */

    size_t item_size;
    char *item = (char *)dirHandler->bufferReceive(&item_size);
    if (item) {
      if (!isInhibited(&fs, item)) {
        /* H4: surface per-entry failures instead of always reporting success. */
        if (!deleteRecursive(&fs, item)) {
          FS_HANDLE_TAG_LOG("cleanUp: could not remove %s", item);
          allDeleted = false;
        }
      }
      dirHandler->bufferReturnItem();
    }
  }
  delete dirHandler;
  return allDeleted;
}

boolean ESPFSHandle::backup2Folder(FS &fs, String path) {
  if (path == "/") return false;
  if (!path.startsWith("/")) return false;
  if (path.endsWith("/")) { path.remove(path.length() - 1); }

  if(!(fs.exists(path))) {
      return false;
  }

  File file = fs.open(path, "r");
  if (!file) {
    return false;
  }
  
  bool isDir = file.isDirectory();
  file.close();
  if (isDir) return false;

  int index = path.lastIndexOf('/');
  index++; /* pass '/' */

  String fileName = path.substring(index);    /* Without prefix '/' look likes log.csv*/

  if (fileName.startsWith("BK_")) {
    // reject backup with prefix BK_
    return false;
  }
  
  if (fileName.startsWith("DEL_")) {
    // reject backup with prefix DEL_
    return false;
  }

  String folderPath;
  String timeName;
  String fsName = (&SD_FS_SYSTEM == &fs) ? "SDFS" : "LITTLEFS";
  if (fsName == "SDFS") {
    int dot = path.lastIndexOf('.');
    folderPath = path.substring(0, dot); /* Without '.' */
    timeName = "/BK_";
    timeName += String(ESPTime.yymmddGet());
    timeName += "_";
    timeName += ESPTime.hhMMssToStr();
    timeName += '_';
    timeName += fileName;
  } else {
    folderPath = "/BACKUP";
    timeName = "/";
    timeName += ESPTime.hhMMssToStr();
    timeName += '_';
    timeName += fileName;
  }

  FS_HANDLE_TAG_CONSOLE("Folder path = %s", folderPath.c_str());
  if(!(fs.exists(folderPath))) {
      FS_HANDLE_TAG_CONSOLE("mkdir %s", folderPath.c_str());
      /* H4: the old code ignored mkdir() and later reported only a generic rename
       * failure.  The rename below is the real gate; report the cause when it fails. */
      if (!fs.mkdir(folderPath) && !fs.exists(folderPath)) {
        FS_HANDLE_TAG_LOG("mkdir %s FAILED - rename will likely fail", folderPath.c_str());
      }
  }
  
  if (!mutexFileList.take(path.c_str(), LINKED_LIST_FILE_MUTEX_TIMEOUT_1S, FS_HANDLE_TAG_MUTEX)) { // locked file
    return false;
  }

  String timePath = folderPath + timeName;
  bool status = fs.rename(path, timePath);
  // fs.remove(path); // Make sure current path is deleted

  mutexFileList.give(path.c_str(), FS_HANDLE_TAG_MUTEX);

  if (status) {
    FS_HANDLE_TAG_LOG("%s Backup succeed file %s to %s", fsName.c_str(), path.c_str(), timePath.c_str());
  } else {
    FS_HANDLE_TAG_LOG("%s Fail to backup file %s to %s", fsName.c_str(), path.c_str(), timePath.c_str());
  }
  /* H4: a fixed delay(100) "to give time to the filesystem" is replaced by a bounded
   * confirmation - runs in the caller's task (trace/console/coredump), never async_tcp. */
  if (status) {
    for (uint8_t retry = 0; (retry < 3U) && fs.exists(path); ++retry) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (fs.exists(path)) {
      FS_HANDLE_TAG_LOG("%s Backup rename not visible yet: %s", fsName.c_str(), path.c_str());
    }
  }
  return status;
}

void ESPFSHandle::listDir(FS &fs,
                          const char *dirname,
                          uint8_t levels,
                          ESPMessageList *mList,
                          uint64_t sLimit)
{
  FS_HANDLE_TAG_CONSOLE("Listing directory: %s", dirname);
  File root = fs.open(dirname, "r");

  if (!root) {
    FS_HANDLE_TAG_CONSOLE("Failed to open directory");
    return;
  }

  if (!root.isDirectory()) {
    FS_HANDLE_TAG_CONSOLE("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      FS_HANDLE_CONSOLE("  DIR : %s", file.name());
      if (mList) {
        /* W3x: rebuild the absolute child path from base name + walked directory
         * (this FS reports base names only); feeds the later remove()/rmdir(). */
        String child = file.name();
        {
          const char *childBase = strrchr(child.c_str(), '/');
          childBase = childBase ? (childBase + 1) : child.c_str();
          child = (String(dirname) == "/") ? (String("/") + childBase) : (String(dirname) + "/" + childBase);
        }
        /* ML5: a refused enqueue used to be invisible - the entry then never reached
         * cleanUp().  Report it instead of silently dropping the directory. */
        if (!mList->bufferSend(child.c_str())) {
          FS_HANDLE_TAG_LOG("List full or 0 length, skip %s", child.c_str());
        }
      }
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      FS_HANDLE_TAG_CONSOLE("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
      if (levels) {
        /* Keep mList and sLimit: the recursive call used to drop both, so a deeper walk
         * silently collected nothing. */
        listDir(fs, file.name(), levels - 1, mList, sLimit);
      }
    }
    else
    {
      FS_HANDLE_CONSOLE("  FILE: %s  SIZE: %u", file.name(), file.size());
      if (mList && file.size() >= sLimit) {
        /* W3x: same reconstruction as the directory branch above. */
        String child = file.name();
        {
          const char *childBase = strrchr(child.c_str(), '/');
          childBase = childBase ? (childBase + 1) : child.c_str();
          child = (String(dirname) == "/") ? (String("/") + childBase) : (String(dirname) + "/" + childBase);
        }
        if (!mList->bufferSend(child.c_str())) {
          FS_HANDLE_TAG_LOG("List full, skip %s", child.c_str());
        }
      }
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      FS_HANDLE_TAG_CONSOLE("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}


/*
   Delete the file or folder designed by the given path.
   If it's a file, delete it.
   If it's a folder, delete all nested contents first then the folder itself

   IMPORTANT NOTE: using recursion is generally not recommended on embedded devices and can lead to crashes (stack overflow errors).
   This use is just for demonstration purpose, and FSBrowser might crash in case of deeply nested filesystems.
   Please don't do this on a production system.
*/
bool ESPFSHandle::deleteRecursive(FS *fs, String path) {
  FS_HANDLE_TAG_CONSOLE("Delete path %s", path.c_str());
  if (path.isEmpty() || path == "/") return false;

  if(!(fs->exists(path))) {
    FS_HANDLE_TAG_CONSOLE("Path is not exist");
    return false;
  }

  File file = fs->open(path, "r");
  if (!file) {
    FS_HANDLE_TAG_CONSOLE("File %s can't open", path.c_str());
    return false;
  }
  bool isDir = file.isDirectory();
  file.close();

  // If it's a plain file, delete it
  if (!isDir) {
    if (!mutexFileList.take(path.c_str(), LINKED_LIST_FILE_MUTEX_TIMEOUT_1S, FS_HANDLE_TAG_MUTEX)) { // locked file
      return false;
    }
    /* H4: report the real remove() result - the old code returned true even on failure. */
    bool removed = fs->remove(path);
    mutexFileList.give(path.c_str(), FS_HANDLE_TAG_MUTEX);
    if (!removed) {
      FS_HANDLE_TAG_LOG("Delete file %s FAILED", path.c_str());
    }
    return removed;
  }

  // Otherwise delete its contents first
  bool allChildrenRemoved = true;
#ifdef ESP32
  File dir = fs->open(path);
  if (!dir) {
    FS_HANDLE_TAG_CONSOLE("Dir %s can't open", path.c_str());
    return false;
  }
  File entry = dir.openNextFile();
  while (entry) { 
    /* C7c: the removed esp_task_wdt_reset() here was a no-op (callers run in loopTask,
     * which is not TWDT-subscribed); wdt_reset() keeps the project watchdog fed. */
    wdt_reset();
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
      /* H4: a nested failure must reach the caller, not only the console. */
      if (!deleteRecursive(fs, name)) { allChildrenRemoved = false; }
    }
    else {
      /* ML5: one busy file must not abort the whole walk.  Report why it was skipped and
       * carry on with the remaining entries instead of leaving them undeleted. */
      mutex_status_t lockReason = MUTEX_STATUS_OK;
      bool isLock = mutexFileList.takeEx(name.c_str(), LINKED_LIST_FILE_MUTEX_TIMEOUT_1S,
                                         FS_HANDLE_TAG_MUTEX, &lockReason);
      if (!isLock) {
        FS_HANDLE_TAG_LOG("Skip locked %s (%s)", name.c_str(), mutexStatusToString(lockReason));
        allChildrenRemoved = false;
      } else {
        /* H4: check remove() instead of assuming success. */
        if (!fs->remove(name)) {
          FS_HANDLE_TAG_LOG("Remove %s FAILED", name.c_str());
          allChildrenRemoved = false;
        }
        mutexFileList.give(name.c_str(), FS_HANDLE_TAG_MUTEX);
      }
    }
    entry = dir.openNextFile();  
  }
  dir.close();
#else
  Dir dir = fs->openDir(path);
  while (dir.next()) { deleteRecursive(path + '/' + dir.fileName()); }
#endif

  // Then delete the folder itself
  bool status = fs->rmdir(path);
  if (!status) {
    /* H4: this line used to reference a non-existent member (`_fsName`) and compiled only
     * because FS_HANDLE_TAG_CONSOLE is an empty macro.  Derive the name like
     * backup2Folder() does and use an always-on log for a real failure. */
    const char *fsName = (&SD_FS_SYSTEM == fs) ? "SDFS" : "LITTLEFS";
    FS_HANDLE_TAG_LOG("Delete folder %s ERROR (%s)", path.c_str(), fsName);
  }
  return status && allChildrenRemoved;
}

ESPFSHandle FSHandle;
