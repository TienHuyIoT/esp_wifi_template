/** Common include */
#include <Arduino.h>
#include "FS.h"
#include "THIoT_PFFSService.h"
#include "THIoT_PFcUtil.h"
#include "THIoT_PFSerialTrace.h"

#define FS_UTILITY_TAG_CONSOLE(...) //SERIAL_FUNCTION_TAG_LOGI("[FS_UTILITY]", __VA_ARGS__)

FSULogService::FSULogService(const fs::FS &fs, int _window, const char *path, size_t headerLength)
    : _fs(fs),
      _path(path),
      _headerLength(headerLength),
      _window(_window)
{
    _posLineBegin = 0;
    _posLineEnd = 0;
    _lookupPos = 0;
    _parseLogCallback = nullptr;
    _lookupLogCallback = nullptr;
    _queryLogCallback = nullptr;
}

FSULogService::~FSULogService()
{
}

/**
 * @brief Find the first param in the file
 * @return true if found, else vice versa
*/
bool FSULogService::withOffset(uint32_t offset)
{
    FS_UTILITY_TAG_CONSOLE("offset %lu", offset);
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
    return find(_headerLength, FIND_FORWARD);
}

/**
 * @brief Find the last param in the file
 * @return true if found, else vice versa
*/
bool FSULogService::last()
{
    FS_UTILITY_TAG_CONSOLE("");
    return find(-_window, FIND_REVERSE);
}

/**
 * @brief Find the next param in the file after last found
 * @return true if found, else vice versa
*/
bool FSULogService::next()
{
    FS_UTILITY_TAG_CONSOLE("");
    return find(_posLineEnd, FIND_FORWARD);
}

/**
 * @brief Find the param in the file with a value preference.
 * @return true if found, else vice versa
*/
bool FSULogService::withValue(int value)
{
    FS_UTILITY_TAG_CONSOLE("value %d", value);
    if (lookUp(value))
    {
        if (query(_lookupPos, value))
        {
            return true;
        }
    }
    return false;
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
    const char* path = _path.c_str();
    uint32_t mid_index, left_bound, right_bound;
    int parse_val;
    bool status_isError = false;

    if (!_lookupLogCallback)
    {
        FS_UTILITY_TAG_CONSOLE("_lookupLogCallback() Null");
        return false;
    }

    if (!_fs.exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        return false;
    }

    FS_UTILITY_TAG_CONSOLE("Open Path: %s", path);
    file = _fs.open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
        return false;
    }

    left_bound = _headerLength;
    right_bound = file.size();

    FS_UTILITY_TAG_CONSOLE("left_bound input: %lu", left_bound);
    FS_UTILITY_TAG_CONSOLE("right_bound input: %lu", right_bound);

    char* buff = new(std::nothrow) char[_window + 1];

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

        size_t read_cnt = file.readBytes(buff, _window);
        buff[read_cnt] = '\0'; /* Add NuLL */
        char *line = lineIndex(buff, FIND_FORWARD);
        if (line != nullptr)
        {
            _posLineBegin = mid_index + (line - buff);
            // +1 is '\n'
            _posLineEnd = _posLineBegin + (strchr(line, '\n') + 1 - line);

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
    FS_UTILITY_TAG_CONSOLE("right_boundoutput: %lu", right_bound);
    FS_UTILITY_TAG_CONSOLE("%s", status_isError ? "Failure!" : "succeed!");
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
    const char* path = _path.c_str();
    uint32_t fsize, window;
    bool status_isOK = false;

    if (!_queryLogCallback)
    {
        FS_UTILITY_TAG_CONSOLE("_queryLogCallback() Null");
        return false;
    }

    if (!_fs.exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        return false;
    }

    file = _fs.open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
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

    char* buff = new(std::nothrow) char[window + 1];
    size_t read_cnt = file.readBytes(buff, window);
    buff[read_cnt] = '\0'; /* Add NuLL */
    char *line = lineIndex(buff, FIND_FORWARD);
    fsContent_t output;
    while (line != nullptr)
    {
        _posLineBegin = offset + (line - buff);
        // +1 is '\n'
        _posLineEnd = _posLineBegin + (strchr(line, '\n') + 1 - line);

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
    uint32_t read_cnt;
    bool status_isOK = false;
    const char* path = _path.c_str();

    if (!_parseLogCallback)
    {
        return false;
    }

    if (!_fs.exists(path))
    {
        FS_UTILITY_TAG_CONSOLE("File '%s' is not exist", path);
        return false;
    }

    file = _fs.open(path, "r");
    if (!file)
    {
        FS_UTILITY_TAG_CONSOLE("Failed to open file %s for reading", path);
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
        return false;
    }

    char* buff = new(std::nothrow) char[_window + 1];
    read_cnt = file.readBytes(buff, _window);
    buff[read_cnt] = '\0'; /* Add NuLL */

    char *line = lineIndex(buff, searchType);
    if (line != nullptr)
    {
        _posLineBegin = seek + (line - buff);
        // +1 is '\n'
        _posLineEnd = _posLineBegin + (strchr(line, '\n') + 1 - line);

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
