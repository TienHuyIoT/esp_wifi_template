/* Includes ------------------------------------------------------------------*/

#include <FS.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_SerialTrace.h"
#include "THIoT_ESPFSHandle.h"
#if (defined SD_SPI_INTERFACE) && (SD_SPI_INTERFACE == 1)
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define FS_HANDLE_PORT SERIAL_PORT
#define FS_HANDLE_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define FS_HANDLE_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[FS]", __VA_ARGS__)

ESPFSHandle::ESPFSHandle(/* args */)
{
}

ESPFSHandle::~ESPFSHandle()
{
}

void ESPFSHandle::listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    FS_HANDLE_TAG_CONSOLE("Listing directory: %s", dirname);
    File root = fs.open(dirname, "r");

    if (!root)
    {
        FS_HANDLE_TAG_CONSOLE("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        FS_HANDLE_TAG_CONSOLE("Not a directory");
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            FS_HANDLE_PORT.print("  DIR : ");
            FS_HANDLE_PORT.print(file.name());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            FS_HANDLE_TAG_CONSOLE("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            FS_HANDLE_PORT.print("  FILE: ");
            FS_HANDLE_PORT.print(file.name());
            FS_HANDLE_PORT.print("  SIZE: ");
            FS_HANDLE_PORT.print(file.size());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            FS_HANDLE_TAG_CONSOLE("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

/* format(SD_FS_SYSTEM, "/"); */
void ESPFSHandle::format(fs::FS &fs, const char *dirname)
{
  FS_HANDLE_TAG_CONSOLE("Listing directory: %s", dirname);

  File root = fs.open(dirname, "r");
  if (!root)
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    FS_HANDLE_TAG_CONSOLE("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      FS_HANDLE_TAG_CONSOLE("  DIR : %s", file.name());
      removeDir(fs, file.name());
    }
    else
    {
      FS_HANDLE_TAG_CONSOLE("  FILE: %s", file.name());
      FS_HANDLE_TAG_CONSOLE("  SIZE: %u", file.size());
      deleteFile(fs, file.name());
    }
    file = root.openNextFile();
  }
}

void ESPFSHandle::deleteFile(fs::FS &fs, const char *path)
{
  FS_HANDLE_TAG_CONSOLE("Deleting file: %s", path);
  if (fs.remove(path))
  {
    FS_HANDLE_TAG_CONSOLE("File deleted");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("Delete failed");
  }
}

void ESPFSHandle::removeDir(fs::FS &fs, const char *path)
{
  FS_HANDLE_TAG_CONSOLE("Removing Dir: %s", path);
  if (fs.rmdir(path))
  {
    FS_HANDLE_TAG_CONSOLE("Dir removed");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("rmdir failed");
  }
}

#if (0)
void ESPFSHandle::createDir(fs::FS &fs, const char *path)
{
  FS_HANDLE_TAG_CONSOLE("Creating Dir: %s", path);
  if (fs.mkdir(path))
  {
    FS_HANDLE_TAG_CONSOLE("Dir created");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("mkdir failed");
  }
}

void ESPFSHandle::readFile(fs::FS &fs, const char *path)
{
  FS_HANDLE_TAG_CONSOLE("Reading file: %s", path);

  File file = fs.open(path);
  if (!file)
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open file for reading");
    return;
  }

  FS_HANDLE_TAG_CONSOLE("Read from file: ");
  while (file.available())
  {
    FS_HANDLE_TAG_CONSOLE("%c", file.read());
  }
}

void ESPFSHandle::writeFile(fs::FS &fs, const char *path, const char *message)
{
  FS_HANDLE_TAG_CONSOLE("Writing file: %s", path);

  File file = fs.open(path, "w");
  if (!file)
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    FS_HANDLE_TAG_CONSOLE("File written");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("Write failed");
  }
}

void ESPFSHandle::appendFile(fs::FS &fs, const char *path, const char *message)
{
  FS_HANDLE_TAG_CONSOLE("Appending to file: %s", path);

  File file = fs.open(path, "a");
  if (!file)
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    FS_HANDLE_TAG_CONSOLE("Message appended");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("Append failed");
  }
}

void ESPFSHandle::renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  FS_HANDLE_TAG_CONSOLE("Renaming file %s to %s", path1, path2);
  if (fs.rename(path1, path2))
  {
    FS_HANDLE_TAG_CONSOLE("File renamed");
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("Rename failed");
  }
}

void ESPFSHandle::testFileIO(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    FS_HANDLE_TAG_CONSOLE("%u bytes read for %u ms", flen, end);
    file.close();
  }
  else
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open file for reading");
  }

  file = fs.open(path, "w");
  if (!file)
  {
    FS_HANDLE_TAG_CONSOLE("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  FS_HANDLE_TAG_CONSOLE("%u bytes written for %u ms", 2048 * 512, end);
  file.close();
}

#endif // (0)

ESPFSHandle FSHandle;
