#include "configData.h"
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include <StreamUtils.h>

uint32_t ConfigManager::hashAttributes()
{
    // Example attributes
    uint32_t cpuId = ESP.getEfuseMac(); // Or any other available unique identifier
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t combined = cpuId ^ flashSize; // Simple combination, consider a better hash function

    // Simple hash to mix bits (consider using a proper hash function)
    uint32_t hash = (combined >> 16) ^ (combined & 0xFFFF);
    return hash % 10000; // Reduce to 4-digit number
}

void ConfigManager::checkDeviceName()
{
    if (strlen(hardware.devicename) == 0)
    {
        char newDeviceName[sizeof(hardware.devicename)];
        snprintf(newDeviceName, sizeof(hardware.devicename), "%s%04d", "ALMALOOX_", hashAttributes()); // Use %04d to ensure the number is always 4 digits
        snprintf(network.URL, sizeof(network.URL), "http://%s.local", newDeviceName);
        strncpy(hardware.devicename, newDeviceName, sizeof(hardware.devicename));
    }
    else
    {
        snprintf(network.URL, sizeof(network.URL), "http://%s.local", hardware.devicename);
    }
}

// Updates the configuration data from a JSON document
void ConfigManager::read(JsonDocument &doc)
{
    JsonObject obj = doc["config"].as<JsonObject>();

    if (obj["ssid"].is<const char *>())
    {
        strcpy(network.ssid, obj["ssid"].as<const char *>());
    }
    if (obj["password"].is<const char *>())
    {
        strcpy(network.password, obj["password"].as<const char *>());
    }
    if (obj["wifi_ip"].is<const char *>())
    {
        network.wifi_ip.fromString(obj["wifi_ip"].as<const char *>());
    }
    if (obj["wifi_subnet"].is<const char *>())
    {
        network.wifi_subnet.fromString(obj["wifi_subnet"].as<const char *>());
    }
    if (obj["wifi_gw"].is<const char *>())
    {
        network.wifi_gw.fromString(obj["wifi_gw"].as<const char *>());
    }
    if (obj["wifi_dns"].is<const char *>())
    {
        network.wifi_dns.fromString(obj["wifi_dns"].as<const char *>());
    }
    if (obj["useStaticWiFi"].is<bool>())
    {
        network.useStaticWiFi = obj["useStaticWiFi"].as<bool>();
    }
    if (obj["Swifi_ip"].is<const char *>())
    {
        network.Swifi_ip.fromString(obj["Swifi_ip"].as<const char *>());
    }
    if (obj["Swifi_sub"].is<const char *>())
    {
        network.Swifi_subnet.fromString(obj["Swifi_sub"].as<const char *>());
    }
    if (obj["Swifi_gw"].is<const char *>())
    {
        network.Swifi_gw.fromString(obj["Swifi_gw"].as<const char *>());
    }

    if(obj["devidename"].is<const char*>())
    {
        strcpy(hardware.devicename, obj["devicename"].as<const char *>());
    }

    for (JsonObject leds : doc["leds"].as<JsonArray>())
    {
        size_t i = leds["index"].as<size_t>();
        if (i < MAX_LAMPS)
        {
            ledData *led = getLedData(i);
            led->warmPin = leds["warmPin"].as<uint8_t>();
            led->coldPin = leds["coldPin"].as<uint8_t>();
            led->warmChannel = leds["warmChannel"].as<uint8_t>();
            led->coldChannel = leds["coldChannel"].as<uint8_t>();
            led->mainBrightness = leds["mainBrightness"].as<uint32_t>();
            led->colorBalance = leds["colorBalance"].as<uint32_t>();
            led->position = leds["position"].as<String>();
        }
    }
}

// Converts configuration data to a JSON document
JsonDocument ConfigManager::write()
{

    JsonDocument doc;

    JsonObject obj = doc["config"].to<JsonObject>();

    obj["ssid"] = network.ssid;
    obj["password"] = network.password;
    obj["wifi_ip"] = network.wifi_ip.toString();
    obj["wifi_subnet"] = network.wifi_subnet.toString();
    obj["wifi_gw"] = network.wifi_gw.toString();
    obj["wifi_dns"] = network.wifi_dns.toString();

    obj["useStaticWiFi"] = network.useStaticWiFi;
    obj["Swifi_ip"] = network.Swifi_ip.toString();
    obj["Swifi_sub"] = network.Swifi_subnet.toString();
    obj["Swifi_gw"] = network.Swifi_gw.toString();
    obj["Swifi_dns"] = network.Swifi_dns.toString();

    obj["devicename"] = hardware.devicename;

    JsonArray leds = doc["leds"].to<JsonArray>();

    for (size_t i = 0; i < MAX_LAMPS; ++i)
    {
        const ledData *led = getLedData(i);
        if (led != nullptr)
        {
            JsonObject obj = leds.add<JsonObject>();
            obj["index"] = i;
            obj["warmPin"] = lampdata[i].warmPin;
            obj["coldPin"] = lampdata[i].coldPin;
            obj["warmChannel"] = lampdata[i].warmChannel;
            obj["coldChannel"] = lampdata[i].coldChannel;
            obj["mainBrightness"] = lampdata[i].mainBrightness;
            obj["colorBalance"] = lampdata[i].colorBalance;
            obj["position"] = lampdata[i].position;
        }
    }

    // serializeJsonPretty(doc, Serial);
    return doc;
}

// Save button data to filesystem
void ConfigManager::save()
{
    const char *filePath = "/config.json";

    auto saveToFilesystem = [&](fs::FS &filesystem)
    {
        if (filesystem.exists(filePath))
        {
            filesystem.remove(filePath);
        }

        File file = filesystem.open(filePath, FILE_WRITE);
        if (!file)
        {
            Serial.println("Failed to create config file");
            return;
        }

        WriteBufferingStream bufferedFile(file, 64);
        if (serializeJson(write(), bufferedFile) == 0)
        {
            Serial.println("Failed to serialize config Settings");
        }

        bufferedFile.flush();
        file.close();
    };

    saveToFilesystem(LittleFS);
    Serial.printf("Saved %s\n", filePath);
}

// Load button data from filesystem
void ConfigManager::load()
{

    const char *filePath = "/config.json";

    auto loadFromFilesystem = [&](fs::FS &filesystem, const char *fsName)
    {
        if (!filesystem.exists(filePath))
        {
            Serial.printf("[STORAGE] Config file %s does not exist on %s\n", filePath, fsName);
            save();
            return;
        }

        File file = filesystem.open(filePath, FILE_READ);
        if (!file)
        {
            Serial.printf("Failed to open config file for reading from %s\n", fsName);
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error)
        {
            Serial.printf("Failed to deserialize config JSON from %s\n", fsName);
            save();
            return;
        }

        read(doc);
        Serial.printf("Loaded %s\n", filePath);
    };

    loadFromFilesystem(LittleFS, "LittleFS");
}

ledData *ConfigManager::getLedData(size_t index)
{
    if (index >= MAX_LAMPS)
    {
        return nullptr; // Out-of-bounds access
    }
    return &lampdata[index];
}
