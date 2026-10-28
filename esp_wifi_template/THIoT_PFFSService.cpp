/** Common include */
#include <Arduino.h>
#include "FS.h"
#include "THIoT_ESPMessageList.h"
#include "THIoT_PFFSService.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_PFSerialTrace.h"

#define FS_UTILITY_TAG_CONSOLE(...) //CONSOLE_TAG_LOGI("[FS_SERVICE]", __VA_ARGS__)
#define FS_SERVICE_TAG_MUTEX        "[FS_SERVICE]"
/* Reserved for conditions that must stay visible in a release build. */
#define FS_SERVICE_TAG_LOGE(...)    CONSOLE_TAG_LOGE("[FS_SERVICE]", __VA_ARGS__)

#define FS_UTILITY_FILE_MUTEX_TIMEOUT LINKED_LIST_FILE_MUTEX_TIMEOUT_1S

const char * const fsu_error_code[] PROGMEM = {
/* 00 */   "FSU_ERROR_NONE",
/* 01 */   "FSU_ERROR_FILE_NOT_FOUND",
/* 02 */   "FSU_ERROR_FILE_TOO_LARGE",
/* 03 */   "FSU_ERROR_FILE_OPEN",
/* 04 */   "FSU_ERROR_FILE_READ",
/* 05 */   "FSU_ERROR_FILE_WRITE",
/* 06 */   "FSU_ERROR_FILE_SEEK",
/* 07 */   "FSU_ERROR_FILE_MEMORY",
/* 08 */   "FSU_ERROR_FILE_PARSE",
/* 09 */   "FSU_ERROR_FILE_LOOKUP",
/* 10 */   "FSU_ERROR_FILE_QUERY",
/* 11 */   "FSU_ERROR_FILE_LOCK"
};

FSULogService::FSULogService(FS &fs, int logLength, const char *path, size_t headerLength)
    : _fs(&fs),
      _path{0},
      _mutex(xSemaphoreCreateRecursiveMutex()),
      _window(logLength * 2) /*_window the minimum size of two line so multi 2 length input*/,
      _headerLength(headerLength)
{
    _posLineBegin = 0;
    _posLineEnd = 0;
    _lookupPos = 0;
    _errorCode = FSU_ERROR_NONE;
    _parseLogCallback = nullptr;
    _lookupLogCallback = nullptr;
    _queryLogCallback = nullptr;
    /* Nobody can reach this object before the constructor returns, so the path
     * may be stored without taking the lock. */
    pathAssign(path);
}

FSULogService::~FSULogService()
{
    if (_mutex)
    {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

/* Brief: Lock of this instance. Recursive; created by the constructor, the
 * lazy creation only covers a failed allocation there. */
bool FSULogService::lock()
{
    if (!_mutex)
    {
        _mutex = xSemaphoreCreateRecursiveMutex();
    }
    return _mutex && xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
}

void FSULogService::unlock()
{
    if (_mutex)
    {
        xSemaphoreGiveRecursive(_mutex);
    }
}

void FSULogService::enterCritical()
{
    lock();
}

void FSULogService::exitCritical()
{
    unlock();
}

/* Brief: Store [path] as the active file. Caller holds the lock.
 * An over-long path is refused instead of truncated: a truncated name would
 * silently read another file, so it is reported and left empty instead. */
void FSULogService::pathAssign(const char *path)
{
    if (!path)
    {
        _path[0] = '\0';
        return;
    }

    const size_t length = strlen(path);
    if (length >= sizeof(_path))
    {
        FS_SERVICE_TAG_LOGE("Path too long (%u >= %u), refused: %s",
                            (unsigned)length, (unsigned)sizeof(_path), path);
        _path[0] = '\0';
        _errorCode = FSU_ERROR_FILE_OPEN;
        return;
    }

    memcpy(_path, path, length + 1);
}

/* Brief: Copy the active path into a caller-owned buffer. Caller holds the
 * lock, so the copy cannot race with setPath() and the caller never uses the
 * member buffer afterwards. */
bool FSULogService::pathSnapshot(char *dst, size_t size) const
{
    if (!dst || size == 0)
    {
        return false;
    }

    if (_path[0] == '\0')
    {
        dst[0] = '\0';
        return false;
    }

    strncpy(dst, _path, size - 1);
    dst[size - 1] = '\0';
    return true;
}

/**
 * @brief Find the first param in the file
 * @return true if found, else vice versa
*/
bool FSULogService::withOffset(uint32_t offset)
{
    FS_UTILITY_TAG_CONSOLE("offset %lu", offset);
    Guard guard(this);
    if (offset < _headerLength)
    {
        offset = _headerLength;
    }
    return find(offset, FIND_FORWARD);
}

/**
 * @brief Find the first param in the file
 * @return true if found, else vice versa
*/
bool FSULogService::first()
{
    Guard guard(this);
    return find(_headerLength, FIND_FORWARD);
}

/**
 * @brief Find the last param in the file
 * @return true if found, else vice versa
*/
bool FSULogService::last()
{
    FS_UTILITY_TAG_CONSOLE("");
    Guard guard(this);
    return find(-_window, FIND_REVERSE);
}

/**
 * @brief Find the next param in the file after last found
 * @return true if found, else vice versa
*/
bool FSULogService::next()
{
    FS_UTILITY_TAG_CONSOLE("");
    Guard guard(this);
    return find(_posLineEnd, FIND_FORWARD);
}

/**
 * @brief Find the param in the file with a value preference.
 * @return true if found, else vice versa
*/
bool FSULogService::withValue(int value)
{
    FS_UTILITY_TAG_CONSOLE("value %d", value);
    Guard guard(this);
    /* Snapshot the path under the lock: the member buffer is never handed out,
     * so no reallocation of it can happen while the file mutex is held. */
    char path[FS_SERVICE_PATH_LENGTH_MAX];
    if (!pathSnapshot(path, sizeof(path)))
    {
        FS_UTILITY_TAG_CONSOLE("Path is empty");
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    bool isLock = mutexFileList.take(path, FS_UTILITY_FILE_MUTEX_TIMEOUT, FS_SERVICE_TAG_MUTEX); // locked file
    if (!isLock) {
        FS_UTILITY_TAG_CONSOLE("Mutex list take FAIL");
        _errorCode = FSU_ERROR_FILE_LOCK;
        return false;
    }

    bool status = false;
    if (lookUp(value))
    {
        if (query(_lookupPos, value))
        {
            status = true;
        }
    }
    mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
    _errorCode = status ? FSU_ERROR_NONE : FSU_ERROR_FILE_QUERY;
    return status;
}

/** 
 * @brief Find the index start of a file. Value to compare get 
 *        from _lookupLogCallback() function
 * @param [in] find_val the value needs to find in the file.
 * @return - True if _lookupLogCallback() always return true and
 *           (right_bound - left_bound) <= window size
 *           >> The variable _lookupPos shall content the value needs to seek
 *              to the position in the file for start query() function ).
 *         - False if _lookupLogCallback() return false or file format fail.
 */
bool FSULogService::lookUp(int find_val)
{
    File file;
    char path[FS_SERVICE_PATH_LENGTH_MAX];
    uint32_t mid_index, left_bound, right_bound;
    int parse_val;
    bool status_isError = false;

    Guard guard(this);
    if (!pathSnapshot(path, sizeof(path)))
    {
        FS_UTILITY_TAG_CONSOLE("Path is empty");
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    if (!_lookupLogCallback)
    {
        FS_UTILITY_TAG_CONSOLE("_lookupLogCallback() Null");
        _errorCode = FSU_ERROR_FILE_LOOKUP;
        return false;
    }

    if (!_fs->exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        _errorCode = FSU_ERROR_FILE_NOT_FOUND;
        return false;
    }

    FS_UTILITY_TAG_CONSOLE("Open Path: %s", path);
    file = _fs->open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    left_bound = _headerLength;
    right_bound = file.size();

    FS_UTILITY_TAG_CONSOLE("left_bound input: %lu", left_bound);
    FS_UTILITY_TAG_CONSOLE("right_bound input: %lu", right_bound);

    char* buff = new (std::nothrow) char[_window + 1];
    if (!buff)
    {
        FS_UTILITY_TAG_CONSOLE("Memory allocation for buffer failed");
        file.close();
        _errorCode = FSU_ERROR_FILE_MEMORY;
        return false;
    }

    do
    {
        if ((right_bound - left_bound) <= _window)
        {
            mid_index = left_bound;
        }
        else
        {
            mid_index = (left_bound + right_bound) / 2;
        }
        
        if (!file.seek(mid_index))
        {
            status_isError = true;
            FS_UTILITY_TAG_CONSOLE("Seek to mid_index %u failed!", mid_index);
            break;
        }

        int read_cnt = file.readBytes(buff, _window);
        if (read_cnt <= 0 || read_cnt > _window)
        {
            FS_UTILITY_TAG_CONSOLE("Read bytes failed or no data");
            status_isError = true;
            break;
        }
        buff[read_cnt] = '\0'; /* Add NuLL */
        char *line = lineIndex(buff, FIND_FORWARD);
        if (line != nullptr)
        {
            _posLineBegin = mid_index + (line - buff);
            // -1 is '\r'
            _posLineEnd = _posLineBegin + (strchr(line, '\n') - 1 - line);

            fsContent_t output;
            output.buff = line;
            output.len  = _posLineEnd - _posLineBegin;
            output.pos  = _posLineBegin;
            if (_lookupLogCallback(&output, &parse_val))
            {
                FS_UTILITY_TAG_CONSOLE("parse_val: %d", parse_val);

                if (find_val < parse_val)
                {
                    right_bound = mid_index;
                    FS_UTILITY_TAG_CONSOLE("Update right_bound %lu", right_bound);
                }
                else
                {
                    left_bound = mid_index;
                    FS_UTILITY_TAG_CONSOLE("Update left_bound %lu", left_bound);
                }
            }
            else
            {
                status_isError = true;
                FS_UTILITY_TAG_CONSOLE("_lookupLogCallback() Failure");
                break;
            }
        }
        else
        {
            FS_UTILITY_TAG_CONSOLE("Line Null");
            status_isError = true;
            break;
        }
    } while ((right_bound - left_bound) > _window);

    delete[] buff;
    file.close();
    _lookupPos = left_bound;
    FS_UTILITY_TAG_CONSOLE("left_bound output: %lu", left_bound);
    FS_UTILITY_TAG_CONSOLE("right_bound output: %lu", right_bound);
    FS_UTILITY_TAG_CONSOLE("%s", status_isError ? "Failure!" : "succeed!");
    _errorCode = status_isError ? FSU_ERROR_FILE_LOOKUP : FSU_ERROR_NONE;
    return !status_isError;
}

/** 
 * @brief Find the value in file with the position begin at offset 
 * @param [in] offset the index of file to starting process
 * @param [in] find_value the value needs to find.
 * @return true if the _queryLogCallback() return true
 * */
bool FSULogService::query(uint32_t offset, int find_val)
{
    File file;
    char path[FS_SERVICE_PATH_LENGTH_MAX];
    uint32_t fsize, window;
    bool status_isOK = false;

    Guard guard(this);
    if (!pathSnapshot(path, sizeof(path)))
    {
        FS_UTILITY_TAG_CONSOLE("Path is empty");
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    if (!_queryLogCallback)
    {
        FS_UTILITY_TAG_CONSOLE("_queryLogCallback() Null");
        _errorCode = FSU_ERROR_FILE_QUERY;
        return false;
    }

    if (!_fs->exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        _errorCode = FSU_ERROR_FILE_NOT_FOUND;
        return false;
    }

    file = _fs->open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    if (offset < _headerLength)
    {
        offset = _headerLength;
    }

    fsize = file.size();
    if(!file.seek(offset))
    {
        FS_UTILITY_TAG_CONSOLE("File size %lu", fsize);
        FS_UTILITY_TAG_CONSOLE("Seek to %lu failed!", offset);
        file.close();
        _errorCode = FSU_ERROR_FILE_SEEK;
        return false;
    }

    uint32_t fsizeRemain = fsize - offset;
    if (fsizeRemain < _window)
    {
        window = fsizeRemain;
    }
    else
    {
        window = _window * 2; // double window to minimum found 2 lines (2 params)
    }

    FS_UTILITY_TAG_CONSOLE("window size %u", window);

    char* buff = new (std::nothrow) char[window + 1];
    if (buff == nullptr)
    {
        FS_UTILITY_TAG_CONSOLE("Memory allocation failed for buffer");
        file.close();
        _errorCode = FSU_ERROR_FILE_MEMORY;
        return false;
    }
    
    int read_cnt = file.readBytes(buff, window);
    if (read_cnt <= 0 || read_cnt > window)
    {
        FS_UTILITY_TAG_CONSOLE("Read bytes failed or empty file");
        delete[] buff;
        file.close();
        _errorCode = FSU_ERROR_FILE_READ;
        return false;
    }
    buff[read_cnt] = '\0'; /* Add NuLL */
    char *line = lineIndex(buff, FIND_FORWARD);
    fsContent_t output;
    while (line != nullptr)
    {
        _posLineBegin = offset + (line - buff);
        // -1 is '\r'
        _posLineEnd = _posLineBegin + (strchr(line, '\n') - 1 - line);

        output.buff = line;
        output.len  = _posLineEnd - _posLineBegin;
        output.pos  = _posLineBegin;
        if (_queryLogCallback(&output, find_val))
        {
            status_isOK = true;
            FS_UTILITY_TAG_CONSOLE("_posLineBegin = %lu", _posLineBegin);
            FS_UTILITY_TAG_CONSOLE("_posLineEnd = %lu", _posLineEnd);
            break;
        }
        line = lineIndex(line, FIND_FORWARD);
    }

    delete[] buff;
    file.close();

    FS_UTILITY_TAG_CONSOLE("%s", status_isOK ? "succeed!" : "Failure!");
    _errorCode = status_isOK ? FSU_ERROR_NONE : FSU_ERROR_FILE_PARSE;
    return status_isOK;
}

/** 
 * @brief Find the first params in file with the position begin at offset 
 * @param [in] offset the index of file to starting process
 * @param [in] searchType the type search from direct_search_t.
 * @return true if the _parseLogCallback() return true, else vice versa
 * */
bool FSULogService::find(int offset, uint8_t searchType)
{
    File file;
    size_t fsize;
    bool status_isOK = false;
    char path[FS_SERVICE_PATH_LENGTH_MAX];

    Guard guard(this);
    if (!pathSnapshot(path, sizeof(path)))
    {
        FS_UTILITY_TAG_CONSOLE("Path is empty");
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    if (!_parseLogCallback)
    {
        _errorCode = FSU_ERROR_FILE_PARSE;
        return false;
    }

    bool isLock = mutexFileList.take(path, FS_UTILITY_FILE_MUTEX_TIMEOUT, FS_SERVICE_TAG_MUTEX); // locked file
    if (!isLock) {
        FS_UTILITY_TAG_CONSOLE("Mutex list take FAIL");
        _errorCode = FSU_ERROR_FILE_LOCK;
        return false;
    }

    if (!_fs->exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
        _errorCode = FSU_ERROR_FILE_NOT_FOUND;
        return false;
    }

    file = _fs->open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
        mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
        _errorCode = FSU_ERROR_FILE_OPEN;
        return false;
    }

    fsize = file.size();

    uint32_t seek;
    if (offset < 0)
    {
        seek = max(0, (int)(fsize + offset));
    }
    else
    {
        seek = offset;
    }

    FS_UTILITY_TAG_CONSOLE("offset %ld, seek %ld", offset, seek);

    if (!file.seek(seek))
    {
        FS_UTILITY_TAG_CONSOLE("Seek to %u failed!", seek);
        file.close();
        mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
        _errorCode = FSU_ERROR_FILE_SEEK;
        return false;
    }

    char* buff = new (std::nothrow) char[_window + 1];
    if (buff == nullptr)
    {
        FS_UTILITY_TAG_CONSOLE("Memory allocation failed for buffer");
        file.close();
        mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
        _errorCode = FSU_ERROR_FILE_MEMORY;
        return false;
    }
    int read_cnt = file.readBytes(buff, _window);
    if (read_cnt <= 0 || read_cnt > _window)
    {
        FS_UTILITY_TAG_CONSOLE("Read bytes failed or empty file");
        delete[] buff;
        file.close();
        mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
        _errorCode = FSU_ERROR_FILE_READ;
        return false;
    }
    buff[read_cnt] = '\0'; /* Add NuLL */

    char *line = lineIndex(buff, searchType);
    if (line != nullptr)
    {
        _posLineBegin = seek + (line - buff);
        // -1 is '\r'
        _posLineEnd = _posLineBegin + (strchr(line, '\n') - 1 - line);

        fsContent_t output;
        output.buff = line;
        output.len  = _posLineEnd - _posLineBegin;
        output.pos  = _posLineBegin;
        if (_parseLogCallback(&output))
        {
            status_isOK = true;
            FS_UTILITY_TAG_CONSOLE("_posLineBegin = %lu", _posLineBegin);
            FS_UTILITY_TAG_CONSOLE("_posLineEnd = %lu", _posLineEnd);
        }
    }

    delete[] buff;
    file.close();

    FS_UTILITY_TAG_CONSOLE("%s", status_isOK ? "succeed!" : "Failure!");

    mutexFileList.give(path, FS_SERVICE_TAG_MUTEX);
    _errorCode = status_isOK ? FSU_ERROR_NONE : FSU_ERROR_FILE_PARSE;
    return status_isOK;
}

/**
 * @brief Find the string start at \n and stop at \n
 *        "This is\r\nfile system\r\nservice"
 * 
 * @return char* = "file system\r\nservice"
*/
char* FSULogService::lineIndex(char* buff, uint8_t searchType)
{
    char *pStart = nullptr;
    char *pEnd;

    if (FIND_FORWARD == searchType)
    {
        pStart = strchr(buff, '\n');
        if (nullptr == pStart)
        {
            FS_UTILITY_TAG_CONSOLE("FIND_FORWARD not Find newline");
            return nullptr;
        }

        /* pass '\n' */
        pStart++;
        pEnd = strchr(pStart, '\n');
        if (nullptr == pEnd)
        {
            return nullptr;
        }
    }
    else if (FIND_REVERSE == searchType)
    {
        pEnd = strrchr(buff, '\n');
        if (nullptr == pEnd)
        {
            FS_UTILITY_TAG_CONSOLE("FIND_REVERSE not Find newline");
            return nullptr;
        }
        *pEnd = '\0';

        pStart = strrchr(buff, '\n');
        *pEnd = '\n';
        if (nullptr == pStart)
        {
            return nullptr;
        }
        /* pass '\n' */
        pStart++;
    }

    return pStart;
}

FSULogStream::FSULogStream(FS &fs, const char *logPath)
    : _fs(&fs), _logPath(logPath)
{
}

size_t FSULogStream::write(const uint8_t *buffer, size_t size) {
    /* ML7: lock and unlock with the SAME stable copy of the path.  Taking a raw
     * _logPath.c_str() pointer and using it again after a blocking take() meant that a
     * concurrent rotation (the param setting log) could reallocate _logPath and leave give()
     * comparing against freed memory. */
    const String lockPath = _logPath;
    mutex_status_t lockReason = MUTEX_STATUS_OK;
    bool isLock = mutexFileList.takeEx(lockPath.c_str(), FS_UTILITY_FILE_MUTEX_TIMEOUT,
                                       FS_SERVICE_TAG_MUTEX, &lockReason); // locked file
    if (!isLock) {
        FS_UTILITY_TAG_CONSOLE("Mutex list take FAIL %s (%s)", lockPath.c_str(), mutexStatusToString(lockReason));
        return 0;
    }
    size_t wSize = 0;
    File fs_handle = _fs->open(lockPath, "a");
    if (fs_handle) {
        wSize = fs_handle.write(buffer, size);
        fs_handle.close();
    }
    mutexFileList.give(lockPath.c_str(), FS_SERVICE_TAG_MUTEX);
    return wSize;
}

size_t FSULogStream::logSize() {
    const String lockPath = _logPath;   /* stable key, see write() */
    mutex_status_t lockReason = MUTEX_STATUS_OK;
    bool isLock = mutexFileList.takeEx(lockPath.c_str(), FS_UTILITY_FILE_MUTEX_TIMEOUT,
                                       FS_SERVICE_TAG_MUTEX, &lockReason); // locked file
    if (!isLock) {
        FS_UTILITY_TAG_CONSOLE("Mutex list take FAIL %s (%s)", lockPath.c_str(), mutexStatusToString(lockReason));
        return 0;
    }
    size_t fSize = 0;
    File fs_handle = _fs->open(lockPath, "r");
    if (fs_handle) {
        fSize = fs_handle.size();
        fs_handle.close();
    }
    mutexFileList.give(lockPath.c_str(), FS_SERVICE_TAG_MUTEX);
    return fSize;
}

void FSULogStream::clean() {
    const String lockPath = _logPath;   /* stable key, see write() */
    const String backupPath = backupNameOf(lockPath);
    mutex_status_t lockReason = MUTEX_STATUS_OK;
    bool isLock = mutexFileList.takeEx(lockPath.c_str(), FS_UTILITY_FILE_MUTEX_TIMEOUT,
                                       FS_SERVICE_TAG_MUTEX, &lockReason); // locked file
    if (!isLock) {
        FS_UTILITY_TAG_CONSOLE("Mutex list take FAIL %s (%s)", lockPath.c_str(), mutexStatusToString(lockReason));
        return;
    }
    _fs->remove(lockPath);
    _fs->remove(backupPath);
    mutexFileList.give(lockPath.c_str(), FS_SERVICE_TAG_MUTEX);
}
