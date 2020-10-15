#include "app_config.h"
#include "sd_card.h"

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)

/* Includes ------------------------------------------------------------------*/
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 1)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SD_FS_PORT Serial
#define SD_FS_PRINTF(f_, ...) SD_FS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)
#define SD_FS_PRINTFLN(f_, ...) SD_FS_PORT.printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)


/* Private variables ---------------------------------------------------------*/
static bool sd_card_status;

void sd_card_init(void)
{
#if (defined SD_POWER_ENABLE) && (SD_POWER_ENABLE == 1)
  SD_POWER_PINMODE_INIT();
  SD_POWER_ON();
  delay(5); // add timeout to supply power to sd card
#endif  
#if (defined SD_CARD_SYSTEM) && (SD_CARD_SYSTEM == 0)
    if(!SD_FS_SYSTEM.begin()){
        SD_FS_PRINTFLN("\r\nCard Mount Failed");
        log_report(LOG_REPORT_SD, (char*)"Card Mount Failed");
        sd_card_status = false;
        return;
    }
#else
    // Init SPI driver.
    // We must be init SPI PIN the first than Init SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_NSS_PIN);
    // Init SD card with SPI clock = 20Mhz
    if(!SD_FS_SYSTEM.begin(SD_NSS_PIN, SPI, 20E6)){
        SD_FS_PRINTFLN("\r\nCard Mount Failed");
        log_report(LOG_REPORT_SD, (char*)"Card Mount Failed");
        sd_card_status = false;
        return;
    }
#endif    
    sd_card_status = true;
    uint8_t cardType = SD_FS_SYSTEM.cardType();

    if(cardType == CARD_NONE){
        SD_FS_PRINTFLN("\r\nNo SD_FS_SYSTEM card attached");
        return;
    }

    SD_FS_PRINTF("\r\nSD_FS_SYSTEM Card Type: ");
    if(cardType == CARD_MMC){
        SD_FS_PRINTFLN("MMC");
    } else if(cardType == CARD_SD){
        SD_FS_PRINTFLN("SDSC");
    } else if(cardType == CARD_SDHC){
        SD_FS_PRINTFLN("SDHC");
    } else {
        SD_FS_PRINTFLN("UNKNOWN");
    }

    uint64_t cardSize = SD_FS_SYSTEM.cardSize() / (1024 * 1024);
    SD_FS_PRINTF("SD_FS_SYSTEM Card Size: %lluMB\r\n", cardSize);

    log_report(LOG_REPORT_SD, (char*)"SD Init OK");

#if (0)
    createDir(SD_FS_SYSTEM, "/mydir");
    listDir(SD_FS_SYSTEM, "/", 0);
    removeDir(SD_FS_SYSTEM, "/mydir");
    listDir(SD_FS_SYSTEM, "/", 2);
    writeFile(SD_FS_SYSTEM, "/hello.txt", "Hello ");
    appendFile(SD_FS_SYSTEM, "/hello.txt", "World!\n");
    readFile(SD_FS_SYSTEM, "/hello.txt");
    deleteFile(SD_FS_SYSTEM, "/foo.txt");
    renameFile(SD_FS_SYSTEM, "/hello.txt", "/foo.txt");
    readFile(SD_FS_SYSTEM, "/foo.txt");
    testFileIO(SD_FS_SYSTEM, "/test.txt");
    SD_FS_PRINTF("Total space: %lluMB\n", SD_FS_SYSTEM.totalBytes() / (1024 * 1024));
    SD_FS_PRINTF("Used space: %lluMB\n", SD_FS_SYSTEM.usedBytes() / (1024 * 1024));
#endif
}

#if (0)
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    SD_FS_PRINTF("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        SD_FS_PRINTFLN("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        SD_FS_PRINTFLN("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            SD_FS_PRINTF("  DIR : ");
            SD_FS_PRINTFLN(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            SD_FS_PRINTF("  FILE: ");
            SD_FS_PRINTF(file.name());
            SD_FS_PRINTF("  SIZE: ");
            SD_FS_PRINTFLN(file.size());
        }
        file = root.openNextFile();
    }
}
#endif

void createDir(fs::FS &fs, const char * path){
    SD_FS_PRINTF("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        SD_FS_PRINTFLN("Dir created");
    } else {
        SD_FS_PRINTFLN("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    SD_FS_PRINTF("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        SD_FS_PRINTFLN("Dir removed");
    } else {
        SD_FS_PRINTFLN("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    SD_FS_PRINTF("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        SD_FS_PRINTFLN("Failed to open file for reading");
        return;
    }

    SD_FS_PRINTF("Read from file: ");
    while(file.available()){
        SD_FS_PRINTF("%c", file.read());
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    SD_FS_PRINTF("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        SD_FS_PRINTFLN("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        SD_FS_PRINTFLN("File written");
    } else {
        SD_FS_PRINTFLN("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    SD_FS_PRINTF("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        SD_FS_PRINTFLN("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        SD_FS_PRINTFLN("Message appended");
    } else {
        SD_FS_PRINTFLN("Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    SD_FS_PRINTF("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        SD_FS_PRINTFLN("File renamed");
    } else {
        SD_FS_PRINTFLN("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    SD_FS_PRINTF("Deleting file: %s\n", path);
    if(fs.remove(path)){
        SD_FS_PRINTFLN("File deleted");
    } else {
        SD_FS_PRINTFLN("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        SD_FS_PRINTF("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        SD_FS_PRINTFLN("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        SD_FS_PRINTFLN("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    SD_FS_PRINTF("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

void sd_returnOK() {
  server.send(200, "text/plain", "");
}

void replyOKWithMsg(String msg) {
  server.send(200, "text/plain", msg);
}

void sd_returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

void sd_handleFileUpload() {
  print_handlerequest();
  if (server.uri() != "/edit_sd") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (SD_FS_SYSTEM.exists((char *)upload.filename.c_str())) {
      SD_FS_SYSTEM.remove((char *)upload.filename.c_str());
    }
    sd_uploadfile = SD_FS_SYSTEM.open(upload.filename.c_str(), FILE_WRITE);
    SD_FS_PRINTF("Upload: START, filename: "); SD_FS_PRINTFLN("%s", upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (sd_uploadfile) {
      sd_uploadfile.write(upload.buf, upload.currentSize);
    }
    SD_FS_PRINTF("Upload: WRITE, Bytes: "); SD_FS_PRINTFLN("%u", upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (sd_uploadfile) {
      sd_uploadfile.close();
    }
    SD_FS_PRINTF("Upload: END, Size: "); SD_FS_PRINTFLN("%u", upload.totalSize);
  }
}

void sd_deleteRecursive(String path) {
  File file = SD_FS_SYSTEM.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD_FS_SYSTEM.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      sd_deleteRecursive(entryPath);
    } else {
      entry.close();
      SD_FS_SYSTEM.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD_FS_SYSTEM.rmdir((char *)path.c_str());
  file.close();
}

void sd_handleDelete() {
  print_handlerequest();
  if (server.args() == 0) {
    return sd_returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD_FS_SYSTEM.exists((char *)path.c_str())) {
    sd_returnFail("BAD PATH");
    return;
  }
  sd_deleteRecursive(path);
  sd_returnOK();
}

void sd_handleCreate() {

  String path = server.arg("path");
  if (path.isEmpty()) {
    return sd_returnFail(F("PATH ARG MISSING"));
  }

  if (path == "/") {
    return sd_returnFail("BAD PATH");
  }
  if (SD_FS_SYSTEM.exists(path)) {
    return sd_returnFail(F("PATH FILE EXISTS"));
  }

  String src = server.arg("src");
  if (src.isEmpty()) {
    // No source specified: creation
    SD_FS_PRINTFLN("handleFileCreate: %s", path.c_str());
    if (path.endsWith("/")) {
      // Create a folder
      path.remove(path.length() - 1);
      if (!SD_FS_SYSTEM.mkdir(path)) {
        return sd_returnFail(F("MKDIR FAILED"));
      }
    } else {
      // Create a file
      File file = SD_FS_SYSTEM.open(path, "w");
      if (file) {
        file.write(0);
        file.close();
      } else {
        return sd_returnFail(F("CREATE FAILED"));
      }
    }
    if (path.lastIndexOf('/') > -1) {
      path = path.substring(0, path.lastIndexOf('/'));
    }
    replyOKWithMsg(path);
  } else {
    // Source specified: rename
    if (src == "/") {
      return sd_returnFail("BAD SRC");
    }
    if (!SD_FS_SYSTEM.exists(src)) {
      return sd_returnFail(F("SRC FILE NOT FOUND"));
    }

    SD_FS_PRINTFLN("handleFileCreate: %s from %s", path.c_str(), src.c_str());

    if (path.endsWith("/")) {
      path.remove(path.length() - 1);
    }
    if (src.endsWith("/")) {
      src.remove(src.length() - 1);
    }
    if (!SD_FS_SYSTEM.rename(src, path)) {
      return sd_returnFail(F("RENAME FAILED"));
    }
    replyOKWithMsg(lastExistingParent(src));
  }
}

/*
   As some FS (e.g. LittleFS) delete the parent folder when the last child has been removed,
   return the path of the closest parent still existing
*/
String lastExistingParent(String path) {
  while (!path.isEmpty() && !SD_FS_SYSTEM.exists(path)) {
    if (path.lastIndexOf('/') > 0) {
      path = path.substring(0, path.lastIndexOf('/'));
    } else {
      path = String();  // No slash => the top folder does not exist
    }
  }
  SD_FS_PRINTFLN("Last existing parent: %s", path.c_str());
  return path;
}

void sd_printDirectory() {
  print_handlerequest();
  if (!server.hasArg("dir")) {
    return sd_returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD_FS_SYSTEM.exists((char *)path.c_str())) {
    return sd_returnFail("BAD PATH");
  }
  File dir = SD_FS_SYSTEM.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return sd_returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String output;
    if (cnt > 0) {
      output = ',';
    }

    output += "{\"type\":\"";
    if (entry.isDirectory()) {
      output += "dir";
    } else {
      output += F("file\",\"size\":\"");
      output += entry.size();
    }
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += strrchr(entry.name(), '/') + 1;
    } else {
      output += entry.name();
    }
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

/*
   Return the FS type, status and size info
   SD_FS_PRINTF("Total space: %lluMB\n", SD_FS_SYSTEM.totalBytes() / (1024 * 1024));
    SD_FS_PRINTF("Used space: %lluMB\n", SD_FS_SYSTEM.usedBytes() / (1024 * 1024));
*/
String unsupportedFiles = String();
void sd_handleStatus() {
  SD_FS_PRINTFLN("handleStatus");
  char buf_ttb[64];
  char buf_udb[64];
  sprintf(buf_ttb,"%llu", SD_FS_SYSTEM.totalBytes());
  sprintf(buf_udb,"%llu", SD_FS_SYSTEM.usedBytes());
  String json;
  json.reserve(128);

  json = "{\"type\":\"";
  json += "SDFS";
  json += "\", \"isOk\":";
  if (sd_card_status) {
    json += F("\"true\", \"totalBytes\":\"");
    json += buf_ttb;
    json += F("\", \"usedBytes\":\"");
    json += buf_udb;
    json += "\"";
  } else {
    json += "\"false\"";
  }
  json += F(",\"unsupportedFiles\":\"");
  json += unsupportedFiles;
  json += "\"}";

  server.send(200, "application/json", json);
}

#endif
