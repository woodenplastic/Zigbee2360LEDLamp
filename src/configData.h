#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <ArduinoJson.h>
#include <IPAddress.h>

struct ledData {
    uint8_t warmPin = 0;
    uint8_t coldPin = 0;
    uint32_t warmCycle = 1024;
    uint32_t coldCycle = 1024;
    uint8_t coldChannel = 0;
    uint8_t warmChannel = 0;
    String position = "FRONT";
};

class Network {
public:
    char ssid[30] = "";
    char password[30] = "";
    char devicename[20] = "";

    IPAddress wifi_ip;
    IPAddress wifi_gw;
    IPAddress wifi_dns;
    IPAddress wifi_subnet;

    IPAddress Swifi_ip;
    IPAddress Swifi_gw;
    IPAddress Swifi_dns;
    IPAddress Swifi_subnet;

    bool useStaticWiFi = false;
    bool useStaticEth = false;
    bool ModulErrorLittleFS = false;

Network() = default;
};


class ConfigManager {
    public:
    ConfigManager() = default;
    Network network;

    ledData* getLedData(size_t index);
    void save();
    void load();
    void read(JsonDocument& doc);
    JsonDocument write();

    private: 
    std::vector<ledData> lampdata;
};



#endif // CONFIGDATA_H
