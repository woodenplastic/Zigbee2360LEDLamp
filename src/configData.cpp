#include "configData.h"
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include <StreamUtils.h>
#include <ArduinoJson.h>



int ConfigData::calculateState(bool in, bool out)
{
  return (out << 1) | in;
}

void ConfigData::make(JsonDocument &doc) {

    useStaticEth = doc["useStaticEth"];
    useStaticWiFi = doc["useStaticWiFi"];

    strncpy(devicename, doc["devicename"], sizeof(devicename));
    strncpy(ssid, doc["ssid"], sizeof(ssid));
    strncpy(password, doc["password"], sizeof(password));

    wifi_ip.fromString(doc["wifi_ip"].as<String>());
    wifi_subnet.fromString(doc["wifi_subnet"].as<String>());
    wifi_gw.fromString(doc["wifi_gw"].as<String>());
    wifi_dns.fromString(doc["wifi_dns"].as<String>());

    Swifi_ip.fromString(doc["Swifi_ip"].as<String>());
    Swifi_gw.fromString(doc["Swifi_gw"].as<String>());
    Swifi_dns.fromString(doc["Swifi_dns"].as<String>());
    Swifi_subnet.fromString(doc["Swifi_sub"].as<String>());

    for (JsonObject leds : doc["leds"].as<JsonArray>()) {
        
        size_t i = leds["index"].as<size_t>();
        ledData *led = getLedData(i);
        led->warmPin = leds["warmPin"].as<uint8_t>();
        led->coldPin = leds["coldPin"].as<uint8_t>();
        led->warmChannel = leds["warmChannel"].as<uint8_t>();
        led->coldChannel = leds["coldChannel"].as<uint8_t>();
        led->warmCycle = leds["warmCycle"].as<uint32_t>();
        led->coldCycle = leds["coldCycle"].as<uint32_t>();
        led->position = leds["position"].as<String>();


    }

}

JsonDocument ConfigData::get() {
    JsonDocument doc;

    doc["devicename"] = devicename;

    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["wifi_ip"] = wifi_ip.toString();
    doc["wifi_subnet"] = wifi_subnet.toString();
    doc["wifi_gw"] = wifi_gw.toString();
    doc["wifi_dns"] = wifi_dns.toString();

    doc["useStaticWiFi"] = useStaticWiFi;
    doc["Swifi_ip"] = Swifi_ip.toString();
    doc["Swifi_sub"] = Swifi_subnet.toString();
    doc["Swifi_gw"] = Swifi_gw.toString();
    doc["Swifi_dns"] = Swifi_dns.toString();

    JsonArray leds = doc["leds"].to<JsonArray>();

    for (size_t i = 0; i < LEDCOUNT; ++i) {   
        const ledData *led = getLedData(i);
        if (led != nullptr) {
            JsonObject obj = leds.add<JsonObject>();
            obj["index"] = i;
            obj["warmPin"] = led->warmPin;
            obj["coldPin"] = led->coldPin;
            obj["warmChannel"] = led->warmChannel;
            obj["coldChannel"] = led->coldChannel;
            obj["warmCycle"] = led->warmCycle;
            obj["coldCycle"] = led->coldCycle;
            obj["position"] = led->position;
        }
    }

    return doc;
}

void ConfigData::save(const char* filePath) {
    auto saveToFilesystem = [&](fs::FS& filesystem) {
        if (filesystem.exists(filePath)) {
            filesystem.remove(filePath);
        }

        File file = filesystem.open(filePath, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to create config file");
            return;
        }

        WriteBufferingStream bufferedFile(file, 64);
        if (serializeJson(get(), bufferedFile) == 0) {
            Serial.println("Failed to serialize config Settings");
        }

        bufferedFile.flush();
        file.close();
    };

    saveToFilesystem(LittleFS);
    Serial.printf("Saved %s\n", filePath);
}

void ConfigData::load(const char* filePath) {
    auto loadFromFilesystem = [&](fs::FS& filesystem, const char* fsName) {
        if (!filesystem.exists(filePath)) {
            Serial.printf("[STORAGE] Config file %s does not exist on %s\n", filePath, fsName);
            save(filePath);
            return;
        }

        File file = filesystem.open(filePath, FILE_READ);
        if (!file) {
            Serial.printf("Failed to open config file for reading from %s\n", fsName);
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error) {
            Serial.printf("Failed to deserialize config JSON from %s\n", fsName);
            save(filePath);
            return;
        }

        make(doc);
        Serial.printf("Loaded %s\n", filePath);
    };


    loadFromFilesystem(LittleFS, "LittleFS");

}


ledData* ConfigData::getLedData(size_t index) {
    if (index < 0) {
        return nullptr;
    }

    if (index >= lampdata.size()) {
        lampdata.resize(index + 1);
    }
    return &lampdata[index];
    Serial.println("made LED DATA");
}
