#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <ArduinoJson.h>


class ConfigData {
public:
    char ssid[30];
    char password[30];
    char devicename[20];

    IPAddress wifi_ip;
    IPAddress wifi_gw;
    IPAddress wifi_dns;
    IPAddress wifi_subnet;

    IPAddress Swifi_ip;
    IPAddress Swifi_gw;
    IPAddress Swifi_dns;
    IPAddress Swifi_subnet;


    bool useStaticWiFi;
    bool useStaticEth;

    bool ModulErrorLittleFS;

    bool connectedToAP = false;


    ConfigData() = default;

    void make(JsonDocument& doc);
    JsonDocument get();
    void save(const char* filePath);
    void load(const char* filePath);


    private:
    int calculateState(bool in, bool out);

};

extern ConfigData configData;


#endif