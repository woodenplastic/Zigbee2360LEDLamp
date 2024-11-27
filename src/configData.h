#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <ArduinoJson.h>
#include <IPAddress.h>

struct ledData {
    uint8_t warmPin = 0;
    uint8_t coldPin = 0;
    uint32_t mainBrightness = 0;
    uint32_t colorBalance = 0;
    uint8_t coldChannel = 0;
    uint8_t warmChannel = 0;
    String position = "FRONT";
};

class Network {
public:
    char ssid[30] = "";
    char password[30] = "";
    char URL[50];

    IPAddress wifi_ip;
    IPAddress wifi_gw;
    IPAddress wifi_dns;
    IPAddress wifi_subnet;

    IPAddress Swifi_ip;
    IPAddress Swifi_gw;
    IPAddress Swifi_dns;
    IPAddress Swifi_subnet;

    bool useStaticWiFi = false;


Network() = default;
};

// Base struct for configuration data
struct Hardware {
    char devicename[20] = "";
    bool ModulErrorLittleFS = false;
    Hardware() = default;

};



class ConfigManager {
public:
    ConfigManager() = default;
    Network network;
    Hardware hardware;

    ledData* getLedData(size_t index);
    void save();
    void load();
    void read(JsonDocument& doc);
    JsonDocument write();
    void checkDeviceName();

private:
    uint32_t hashAttributes(); 
    static constexpr size_t MAX_LAMPS = 2; // Define the maximum number of lamps
    ledData lampdata[MAX_LAMPS];          // Fixed-size array on the stack
};



#endif // CONFIGDATA_H
