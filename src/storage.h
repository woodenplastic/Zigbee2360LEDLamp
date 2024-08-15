#ifndef STORAGE_H
#define STORAGE_H

#include <FS.h>
#include "configData.h"


class StorageManager {
public:

    bool mountLittleFS();
    void listDir(fs::FS& fs, const char* dirname, uint8_t levels);
};

extern StorageManager storageManager;


#endif