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




struct Sliders {
  int cycle;
  int value;
};

 std::vector<Sliders> slidersList;

// Function to get a pointer to a Sliders struct from the vector
Sliders* getSlider(std::vector<Sliders>& sliders, size_t index) {
    if (index >= sliders.size()) {
        // If the index is out of range, resize the vector to accommodate the index
        sliders.resize(index + 1);
    }
    // Return a pointer to the Sliders struct at the given index
    return &sliders[index];
}



#endif