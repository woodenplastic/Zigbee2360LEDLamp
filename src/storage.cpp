#include "storage.h"
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>






bool StorageManager::mountLittleFS() {
    if (LittleFS.begin()) {
        return true; // Successfully mounted
    }


    if (LittleFS.format()) {
        if (LittleFS.begin()) {
            return true; // Successfully mounted after formatting
        }
    }

   // config.ModulErrorLittleFS = true;
    return false;
}




void StorageManager::listDir(fs::FS& fs, const char* dirname, uint8_t levels) {

  File root = fs.open(dirname);
  if (!root)
  {
    return;
  }
  if (!root.isDirectory())
  {
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
    }
    file = root.openNextFile();
  }
}


