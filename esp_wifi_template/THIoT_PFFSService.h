#ifndef __FS_SERVICE_H
#define __FS_SERVICE_H

/** Common include */
#include <Arduino.h>
#include <functional>
#include <stdint.h>
#include <stdbool.h>
#include <FS.h>

#define WINDOW_LOOKUP_SIZE 512
#define LOG_SIZE_MAX 1048576U

/* Fixed storage for the active file path of an FSULogService. Length checked
 * against every live log path of the project; a longer path is refused and
 * logged instead of being truncated (a truncated path would read the wrong
 * file), see FSULogService::pathAssign(). */
#define FS_SERVICE_PATH_LENGTH_MAX 64

extern const char * const fsu_error_code[];

typedef struct {
  char *buff;
  size_t len;
  size_t pos;
} fsContent_t;

typedef struct {
    size_t start;
    size_t end;
} position_log_t;

typedef std::function<bool(fsContent_t*)> parseLogHandler;
typedef std::function<bool(void*, fsContent_t*)> parseLogWithArgHandler;
typedef std::function<bool(fsContent_t*, int*)> lookupLogHandler;
typedef std::function<bool(void*, fsContent_t*, int*)> lookupLogWithArgHandler;
typedef std::function<bool(fsContent_t*, int)> queryLogHandler;
typedef std::function<bool(void*, fsContent_t*, int)> queryLogWithArgHandler;

/**
 * @brief file system utility
 * @param [in] _window the minimum size of two line
 * @param [in] _path the path file handle
 * @param [in] _parseLogCallback the callback handler parser params
 * @param [in] _position the seek index to find next log
*/
class FSULogService
{
  using File = fs::File;
  using FS = fs::FS;
private:
  typedef enum {
    FIND_NONE = 0,
    FIND_FORWARD,
    FIND_REVERSE
  } direct_search_t;
  
  FS *_fs;
  parseLogHandler _parseLogCallback;
  lookupLogHandler _lookupLogCallback;
  queryLogHandler _queryLogCallback;
  /* Active file path. A fixed buffer, never reallocated, so no task can hold a
   * pointer into memory that another task frees: this is the T14 fix. Keeping a
   * String here was unsafe because setPath() could reallocate it while another
   * task was using the pointer returned by c_str() (see the archived core dump:
   * StoreProhibited inside find()). */
  char _path[FS_SERVICE_PATH_LENGTH_MAX];
  /* Per-instance recursive mutex. It protects _path and the cursor state
   * (_posLineBegin/_posLineEnd/_lookupPos/_errorCode) that every operation
   * mutates. Recursive because the public operations nest (withValue() ->
   * lookUp()/query(), first()/last()/next()/withOffset() -> find()) and because
   * a caller may keep the section open across setPath() + operation, see
   * enterCritical().
   *
   * Lock order: this mutex is always the OUTER one; the file mutex
   * (mutexFileList) is only ever taken inside it. Never call an operation of
   * this instance while holding mutexFileList for the same path, and note that
   * every CsvLogStore-owned instance is additionally serialised by the store
   * mutex, which is taken before this one. */
  SemaphoreHandle_t _mutex;
  int _window;
  size_t _headerLength; /* Exclude \r\n at the end of string */
  size_t _posLineBegin; /* Exclude \r\n at the begin */
  size_t _posLineEnd;   /* Include \r\n at the end */
  uint32_t _lookupPos;
  int _errorCode;
  bool lock();
  void unlock();
  void pathAssign(const char *path);               /* caller holds the lock */
  bool pathSnapshot(char *dst, size_t size) const; /* caller holds the lock */

  /* RAII holder of the instance lock: nested Guard instances are safe because
   * the mutex is recursive, and the lock is released on every return path. */
  struct Guard
  {
    FSULogService *_owner;
    bool _locked;
    explicit Guard(FSULogService *owner) : _owner(owner), _locked(owner->lock()) {}
    ~Guard()
    {
      if (_locked)
      {
        _owner->unlock();
      }
    }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
  };

  bool find(int offset, uint8_t searchType);
  bool query(uint32_t offset, int find_val);
  bool lookUp(int find_val);
  char* lineIndex(char* buff, uint8_t searchType);
public:
  typedef enum {
  /* 00 */  FSU_ERROR_NONE = 0,
  /* 01 */  FSU_ERROR_FILE_NOT_FOUND,
  /* 02 */  FSU_ERROR_FILE_TOO_LARGE,
  /* 03 */  FSU_ERROR_FILE_OPEN,
  /* 04 */  FSU_ERROR_FILE_READ,
  /* 05 */  FSU_ERROR_FILE_WRITE,
  /* 06 */  FSU_ERROR_FILE_SEEK,
  /* 07 */  FSU_ERROR_FILE_MEMORY,
  /* 08 */  FSU_ERROR_FILE_PARSE,
  /* 09 */  FSU_ERROR_FILE_LOOKUP,
  /* 10 */  FSU_ERROR_FILE_QUERY,
  /* 11 */  FSU_ERROR_FILE_LOCK
  } error_t;
  /* window: Equal or exceed length of one log */
  FSULogService(FS &fs, int logLength, const char *path, size_t headerLength);
  ~FSULogService();

  bool last();
  bool next();
  bool withValue(int value);
  bool withOffset(uint32_t offset);
  bool first(); // header size of file

  /* Critical section of this instance. Available to callers that must keep the
   * path and the operation together: the caller sets the path and runs the
   * operation inside one section, or another task could re-point the service in
   * between and the operation would read the other file. Recursive, so setPath()
   * and the operation may also take it on their own. Every CsvLogStore-owned
   * instance is already serialised by the store mutex that wraps this one. */
  void enterCritical();
  void exitCritical();

  int errorCode() const { return _errorCode; }

  void fsHandler(FS &fs) {
    Guard guard(this);
    _fs = &fs;
  }

  FSULogService &onLookupLog(lookupLogHandler fn)
  {
    _lookupLogCallback = fn;
    return *this;
  }

  FSULogService &onLookupLogWithArg(lookupLogWithArgHandler fn, void* arg)
  {
    using namespace std::placeholders;
    onLookupLog(std::bind(fn, arg, _1, _2));
    return *this;
  }

  FSULogService &onQueryLog(queryLogHandler fn)
  {
    _queryLogCallback = fn;
    return *this;
  }

  FSULogService &onQueryLogWithArg(queryLogWithArgHandler fn, void* arg)
  {
    using namespace std::placeholders;
    onQueryLog(std::bind(fn, arg, _1, _2));
    return *this;
  }

  FSULogService &onParseLog(parseLogHandler fn)
  {
    _parseLogCallback = fn;
    return *this;
  }

  FSULogService &onParseLogWithArg(parseLogWithArgHandler fn, void* arg)
  {
    onParseLog(std::bind(fn, arg, std::placeholders::_1));
    return *this;
  }

  FSULogService &setWindow(int window)
  {
    _window = window;
    return *this;
  }

  FSULogService &setPath(const char *path)
  {
    Guard guard(this);
    pathAssign(path);
    return *this;
  }
};

class FSULogStream : public Stream
{
protected:
    using File = fs::File;
    using FS = fs::FS;
    FS *_fs;
    String _logPath;
    /* ML7: build the backup name from a caller-supplied path, so an operation can work on
     * one stable snapshot instead of re-reading _logPath while another task rotates it. */
    static String backupNameOf(const String &path) {
        String ext = path.substring(1); /* pass '/' */
        String name = "/bk";
        name += ext;
        return name;
    }
    String backupName() {
        return backupNameOf(_logPath);
    }
public:
    FSULogStream(FS &fs, const char* logPath);
    void pathSet(const char* logPath) {_logPath = logPath;}
    int available() override { return 0;}
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush(void) override {}
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t logSize();
    void clean();
    void fsHandler(FS &fs) {
      _fs = &fs;
    }  
};

#endif // __FS_SERVICE_H
