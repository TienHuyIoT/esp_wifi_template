#define SPIFFS_PORT Serial
#define SPIFFS_PRINTF(f_, ...) SPIFFS_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    SPIFFS_PORT.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        SPIFFS_PORT.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        SPIFFS_PORT.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            SPIFFS_PORT.print("  DIR : ");
            SPIFFS_PORT.print(file.name());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            SPIFFS_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            SPIFFS_PORT.print("  FILE: ");
            SPIFFS_PORT.print(file.name());
            SPIFFS_PORT.print("  SIZE: ");
            SPIFFS_PORT.print(file.size());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            SPIFFS_PORT.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

//http://192.168.4.1:25123/foo.txt
//http://192.168.4.1:25123/foo.txt?delect=true  //delect
//http://192.168.4.1:25123/foo.txt?download=1
bool handleFileRead(String path)
{
    SPIFFS_PRINTF("\r\nhandleFileRead: %s", path.c_str());
    if (path.endsWith("/"))
        path += "home.htm";
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";

    fs::FS *fs = &FILE_SYSTEM;

    if (server.hasArg("delect") && server.arg("delect") == "true")
    {
        if (fs->exists(path))
        {
            fs->remove(path);
            server.send(200, "text/json", "{\"St\":\"OK\",\"Mgs\":\"Delect File OK\"}");
        }
        else
        {
            server.send(200, "text/json", "{\"St\":\"Error\",\"Mgs\":\"File not exist\"}");
        }

        return true;
    }

    if (fs->exists(pathWithGz) || fs->exists(path))
    {
        if (fs->exists(pathWithGz))
            path += ".gz";
        File file = fs->open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

//http://192.168.4.1:25123/upload
void handleFileUpload()
{
    if (server.uri() != "/edit" && server.uri() != "/upload")
        return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        String filename = upload.filename;
        if (!filename.startsWith("/"))
            filename = "/" + filename;
        SPIFFS_PRINTF("\r\nhandleFileUpload Name: %s", filename.c_str());
        fs::FS *fs = &FILE_SYSTEM;
        fsUploadFile = fs->open(filename, "w");
        filename = String();
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        //SPIFFS_PRINTF("handleFileUpload Data: %lu",upload.currentSize);
        if (fsUploadFile)
            fsUploadFile.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (fsUploadFile)
            fsUploadFile.close();
        SPIFFS_PRINTF("\r\nhandleFileUpload Size: %lu", upload.totalSize);
    }
}

void handleFileDelete()
{
    if (server.args() == 0)
        return server.send(500, "text/plain", "BAD ARGS");
    String path = server.arg(0);
    SPIFFS_PRINTF("handleFileDelete: %s", path.c_str());
    if (path == "/")
    {
        return server.send(500, "text/plain", "BAD PATH");
    }

    fs::FS *fs = &FILE_SYSTEM;

    if (!fs->exists(path))
    {
        return server.send(404, "text/plain", "FileNotFound");
    }

    fs->remove(path);
    server.send(200, "text/plain", "");
    path = String();
}

void handleFileCreate()
{
    if (server.args() == 0)
        return server.send(500, "text/plain", "BAD ARGS");
    String path = server.arg(0);
    SPIFFS_PRINTF("handleFileCreate: %s", path.c_str());
    if (path == "/")
        return server.send(500, "text/plain", "BAD PATH");
    if (FILE_SYSTEM.exists(path))
        return server.send(500, "text/plain", "FILE EXISTS");
    File file = FILE_SYSTEM.open(path, "w");
    if (file)
        file.close();
    else
        return server.send(500, "text/plain", "CREATE FAILED");
    server.send(200, "text/plain", "");
    path = String();
}

void handleFileList()
{
    if (!server.hasArg("dir"))
    {
        server.send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = server.arg("dir");
    if (path != "/" && !SPIFFS.exists((char *)path.c_str()))
    {
        returnFail("BAD PATH");
        return;
    }
    File dir = SPIFFS.open((char *)path.c_str());
    path = String();
    if (!dir.isDirectory())
    {
        dir.close();
        returnFail("NOT DIR");
        return;
    }
    dir.rewindDirectory();

    String output = "[";
    for (int cnt = 0; true; ++cnt)
    {
        File entry = dir.openNextFile();
        if (!entry)
            break;

        if (cnt > 0)
            output += ',';

        output += "{\"type\":\"";
        if (entry.isDirectory())
        {
            output += "dir";
        }
        else
        {
            output += F("file\",\"size\":\"");
            output += entry.size();
        }
        output += "\",\"name\":\"";
        if (entry.name()[0] == '/')
        {
            output += strrchr(entry.name(), '/') + 1;
        }
        else
        {
            output += entry.name();
        }
        output += "\"";
        output += "}";
        entry.close();
    }
    output += "]";
    server.send(200, "text/json", output);
    dir.close();
}

/*
   Return the FS type, status and size info
   SPIFFS_PORT.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    SPIFFS_PORT.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
*/
static String spifs_unsupportedFiles = String();
void handleStatus()
{
    SPIFFS_PORT.println("handleStatus");
    char buf_ttb[64];
    char buf_udb[64];
    SPIFFS_PORT.printf("\nTotal space: %lu\n", SPIFFS.totalBytes());
    SPIFFS_PORT.printf("Used space: %lu\n", SPIFFS.usedBytes());
    sprintf(buf_ttb, "%lu", SPIFFS.totalBytes());
    sprintf(buf_udb, "%lu", SPIFFS.usedBytes());

    String json;
    json.reserve(128);

    json = "{\"type\":\"";
    json += "SPIFFS";
    json += "\", \"isOk\":";
    if (1)
    {
        json += F("\"true\", \"totalBytes\":\"");
        json += buf_ttb;
        json += F("\", \"usedBytes\":\"");
        json += buf_udb;
        json += "\"";
    }
    else
    {
        json += "\"false\"";
    }
    json += F(",\"unsupportedFiles\":\"");
    json += spifs_unsupportedFiles;
    json += "\"}";

    server.send(200, "application/json", json);
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}