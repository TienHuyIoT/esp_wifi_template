/* Includes ------------------------------------------------------------------*/

#include <FS.h>
#include "app_config.h"
#include "hth_console_dbg.h"
#include "fs_handle.h"
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define FS_HANDLE_PORT CONSOLE_PORT
#define FS_HANDLE_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define FS_HANDLE_PRINTFLN(f_, ...) FS_HANDLE_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    FS_HANDLE_PORT.printf("Listing directory: %s\r\n", dirname);
    File root = fs.open(dirname, "r");

    if (!root)
    {
        FS_HANDLE_PORT.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        FS_HANDLE_PORT.println("Not a directory");
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
            FS_HANDLE_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\r\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
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
            FS_HANDLE_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\r\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

/* sd_format(SD_FS_SYSTEM, "/"); */
void sd_format(fs::FS &fs, const char *dirname)
{
  FS_HANDLE_PRINTF("Listing directory: %s\n", dirname);

  File root = fs.open(dirname, "r");
  if (!root)
  {
    FS_HANDLE_PRINTFLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    FS_HANDLE_PRINTFLN("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      FS_HANDLE_PRINTF("  DIR : %s\r\n", file.name());
      removeDir(fs, file.name());
    }
    else
    {
      FS_HANDLE_PRINTF("  FILE: %s\r\n", file.name());
      FS_HANDLE_PRINTF("  SIZE: %u\r\n", file.size());
      deleteFile(fs, file.name());
    }
    file = root.openNextFile();
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  FS_HANDLE_PRINTF("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    FS_HANDLE_PRINTFLN("File deleted");
  }
  else
  {
    FS_HANDLE_PRINTFLN("Delete failed");
  }
}

void removeDir(fs::FS &fs, const char *path)
{
  FS_HANDLE_PRINTF("Removing Dir: %s\n", path);
  if (fs.rmdir(path))
  {
    FS_HANDLE_PRINTFLN("Dir removed");
  }
  else
  {
    FS_HANDLE_PRINTFLN("rmdir failed");
  }
}

#if (0)
void createDir(fs::FS &fs, const char *path)
{
  FS_HANDLE_PRINTF("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    FS_HANDLE_PRINTFLN("Dir created");
  }
  else
  {
    FS_HANDLE_PRINTFLN("mkdir failed");
  }
}

void readFile(fs::FS &fs, const char *path)
{
  FS_HANDLE_PRINTF("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    FS_HANDLE_PRINTFLN("Failed to open file for reading");
    return;
  }

  FS_HANDLE_PRINTF("Read from file: ");
  while (file.available())
  {
    FS_HANDLE_PRINTF("%c", file.read());
  }
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  FS_HANDLE_PRINTF("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    FS_HANDLE_PRINTFLN("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    FS_HANDLE_PRINTFLN("File written");
  }
  else
  {
    FS_HANDLE_PRINTFLN("Write failed");
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  FS_HANDLE_PRINTF("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    FS_HANDLE_PRINTFLN("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    FS_HANDLE_PRINTFLN("Message appended");
  }
  else
  {
    FS_HANDLE_PRINTFLN("Append failed");
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  FS_HANDLE_PRINTF("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    FS_HANDLE_PRINTFLN("File renamed");
  }
  else
  {
    FS_HANDLE_PRINTFLN("Rename failed");
  }
}

void testFileIO(fs::FS &fs, const char *path)
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
    FS_HANDLE_PRINTF("%u bytes read for %u ms\n", flen, end);
    file.close();
  }
  else
  {
    FS_HANDLE_PRINTFLN("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    FS_HANDLE_PRINTFLN("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  FS_HANDLE_PRINTF("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

#endif // (0)
