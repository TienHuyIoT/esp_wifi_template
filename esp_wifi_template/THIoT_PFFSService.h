#ifndef __FS_UTILITY_H
#define __FS_UTILITY_H

/** Common include */
#include <Arduino.h>
#include <functional>
#include <stdint.h>
#include <stdbool.h>
#include <FS.h>

#define WINDOW_LOOKUP_SIZE 512
#define LOG_SIZE_MAX 1048576U

typedef std::function<bool(char*)> parseLogHandler;
typedef std::function<bool(void*, char*)> parseLogWithArgHandler;
typedef std::function<bool(char*, int*)> lookupLogHandler;
typedef std::function<bool(void*, char*, int*)> lookupLogWithArgHandler;
typedef std::function<bool(char*, int)> queryLogHandler;
typedef std::function<bool(void*, char*, int)> queryLogWithArgHandler;

/**
 * @brief file system utility
 * @param [in] _window the minimum size of two line
 * @param [in] _path the path file handle
 * @param [in] _parseLogCallback the callback handler parser params
 * @param [in] _position the seek index to find next log
*/
class FSULogService
{
private:
  typedef enum {
    FIND_NONE = 0,
    FIND_FORWARD,
    FIND_REVERSE
  } direct_search_t;
  
  fs::FS _fs;
  parseLogHandler _parseLogCallback;
  lookupLogHandler _lookupLogCallback;
  queryLogHandler _queryLogCallback;
  String _path;
  int _window;
  size_t _headerLength;
  size_t _posLineBegin;
  size_t _posLineEnd;
  uint32_t _lookupPos;
  bool find(int offset, uint8_t searchType);
  bool query(uint32_t offset, int find_val);
  bool lookUp(int find_val);
  char* lineIndex(char* buff, uint8_t searchType);
public:
  FSULogService(const fs::FS &fs, int window, const char *path, size_t headerLength);
  ~FSULogService();

  bool last();
  bool next();
  bool withValue(int value);
  bool withOffset(uint32_t offset);
  bool first(); // header size of file

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
    _path = path;
    return *this;
  }
};

class FSULogStream : public Stream
{
private:
    fs::FS* _fs;
    String _fileName;
public:
    FSULogStream(fs::FS &fs, const char* fileName)
    :_fs(&fs),_fileName(fileName) {

    }
    int available() override { return 0;}
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush(void) override {}
    size_t write(uint8_t c) override { write(&c, 1); return 1; }
    size_t write(const uint8_t *buffer, size_t size) override {
        File fs_handle = _fs->open(_fileName, "a");
        fs_handle.write(buffer, size);
        fs_handle.close();
        return size;
    }
    void clean() {
        _fs->remove(_fileName);
    }
};

#endif // __FS_UTILITY_H
