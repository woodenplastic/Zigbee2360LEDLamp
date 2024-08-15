#include <Arduino.h>


#include <stdarg.h>

#include <initializer_list>

#include "DRFZigbee.h"
#include "M5Stack.h"
#include "byteArray.h"







#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
WiFiUDP Udp;
const IPAddress broadcastIp(255, 255, 255, 255);
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
IPAddress subnet(255, 255, 255, 0);
#include <ArduinoJson.h>


#include "storage.h"
StorageManager storageManager;

#include "configData.h"
ConfigData configData;

#include <PsychicHttp.h>

PsychicHttpServer server;
PsychicWebSocketHandler websocketHandler;
PsychicEventSource eventSource;

#include <DNSServer.h>
const uint16_t DNS_PORT = 53;

DNSServer dnsServer;

#include <ESPmDNS.h>

#define NET_TIMEOUT_MS 20000
#define WIFI_RECOVER_TIME_MS 30000

// HERE PASSWORD FOR AP // SSID = MAIKO
const char *softAP_password = "12345678";


const int maxScanAttempts = 1;
const int scanInterval = 3000; // Scan every 3 seconds

unsigned long lastInputTime = 0; // Initialize to 0
bool CONFIG_SAVED = true;        // Initialize as true to prevent immediate saving

const unsigned int localPort = 53000; // local port to listen for OSC packets (actually not used for sending)

#include <ELog.h>
#define MAIKO 0

SemaphoreHandle_t sema_Server = NULL;

TaskHandle_t AutoSaveHandle = NULL;
TaskHandle_t checkNetworksHandle = NULL;
TaskHandle_t syncClientsHandle = NULL;

EventGroupHandle_t SERVER_GROUP;
EventGroupHandle_t networkEventGroup;

const int MANUAL_TRIGGER_BIT = BIT0;
const int SYNC_CLIENTS_BIT = BIT1;


void setLedDutyCycle(size_t i) {
    ledData* led = configData.getLedData(i);
    // Constrain the values to be within the PWM range (0-255)
    int pwm1 = constrain(led->warmCycle * 0.25, 0, 255);
    int pwm2 = constrain(led->coldCycle * 0.25, 0, 255);

    // Write the PWM values to the specified channels
    ledcWrite(led->warmChannel, pwm1);
    ledcWrite(led->coldChannel, pwm2);
}


// NETWORK ///////////////////////////////////////////////////////////////////////

bool isSSIDAvailable(const char *ssidToFind)
{
  int scanAttempts = 0;
  while (scanAttempts < maxScanAttempts)
  {
    int networkCount = WiFi.scanNetworks();
    for (size_t i = 0; i < networkCount; ++i)
    {
      if (strcmp(WiFi.SSID(i).c_str(), ssidToFind) == 0)
      {
        return true;
      }
    }
    delay(scanInterval);
    scanAttempts++;
  }
  return false;
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_READY:
    logger.log(MAIKO, INFO, "WiFi Ready");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    logger.log(MAIKO, INFO, "WiFi Scan Done");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    logger.log(MAIKO, INFO, "WiFi STA Started");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    logger.log(MAIKO, INFO, "WiFi STA Stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    logger.log(MAIKO, INFO, "WiFi Connected");
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    logger.log(MAIKO, INFO, "WiFi Disconnected");
    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    logger.log(MAIKO, INFO, "WiFi Auth Mode Changed");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    logger.log(MAIKO, INFO, "WiFi Got IP: %s", WiFi.localIP().toString().c_str());
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    logger.log(MAIKO, INFO, "WiFi Lost IP");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    logger.log(MAIKO, INFO, "WiFi AP Started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    logger.log(MAIKO, INFO, "WiFi AP Stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    logger.log(MAIKO, INFO, "Station Connected to WiFi AP");
    configData.connectedToAP = true;
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    logger.log(MAIKO, INFO, "Station Disconnected from WiFi AP");
    configData.connectedToAP = false;
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    logger.log(MAIKO, INFO, "Station IP Assigned in WiFi AP Mode");
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    logger.log(MAIKO, INFO, "Probe Request Received in WiFi AP Mode");
    break;
  default:
    break;
  }
}

String scanWifiNetworks()
{

  int numNetworks = WiFi.scanNetworks();
  if (numNetworks == WIFI_SCAN_FAILED)
  {
    logger.log(MAIKO, INFO, "Wi-Fi scan failed!");
    return "{}";
  }
  else if (numNetworks == 0)
  {
    logger.log(MAIKO, INFO, "No networks found");
    return "[]";
  }

  JsonDocument jsonDoc;
  JsonArray networks = jsonDoc.to<JsonArray>();

  for (size_t i = 0; i < numNetworks; ++i)
  {
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryption"] = WiFi.encryptionType(i);
  }

  String jsonResult;
  serializeJson(jsonDoc, jsonResult);
  logger.log(MAIKO, INFO, "Scanned Wi-Fi Networks:");
  logger.log(MAIKO, INFO, "%s", jsonResult);

  return jsonResult;
}

void checkNetwork(void *parameter)
{
  for (;;)
  {
    // Wait for either the periodic delay or the manual trigger event
    EventBits_t bits = xEventGroupWaitBits(
        networkEventGroup,
        MANUAL_TRIGGER_BIT,
        pdTRUE,               // Clear the bit on exit
        pdFALSE,              // Wait for any bit
        pdMS_TO_TICKS(120000) // 60-second delay
    );
    if ((WiFi.softAPgetStationNum() == 0) && !WiFi.isConnected())
    {
      if (WiFi.getMode() == WIFI_STA)
      {
        WiFi.disconnect();
      }
      if (WiFi.getMode() == WIFI_AP)
      {
        WiFi.softAPdisconnect();
        dnsServer.stop();
        WiFi.mode(WIFI_STA);
      }

      if ((strlen(configData.ssid) > 0) && isSSIDAvailable(configData.ssid))
      {
        if (configData.useStaticWiFi)
        {
          WiFi.config(configData.Swifi_ip, configData.Swifi_gw, configData.Swifi_subnet, configData.Swifi_dns);
        }
        WiFi.begin(configData.ssid, configData.password);
        logger.log(MAIKO, INFO, "Connecting to %s", configData.ssid);
        return;
      }

      logger.log(MAIKO, INFO, "No Saved WiFi SSID, starting AP mode");
      if (WiFi.getMode() == WIFI_STA)
      {
        WiFi.mode(WIFI_AP);
      }
      if (configData.useStaticWiFi)
      {
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      }
      WiFi.softAP(/*configData.devicename*/ "MAIKO", softAP_password);
      dnsServer.start(DNS_PORT, "*", apIP);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void AutoSave(void *parameter)
{
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(1000); // Periodic timeout of 1000ms
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);

    if (!CONFIG_SAVED && (millis() - lastInputTime >= 30000))
    {
      CONFIG_SAVED = true;
      configData.save("/config.json");

      logger.log(MAIKO, INFO, "[SYSTEM] AUTOSAVING");
    }
  }
}

void syncClients(void *parameter)
{
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    String bdata;

    serializeJson(configData.get(), bdata);
    websocketHandler.sendAll(bdata.c_str());
  }
}

/////////////////////////////////////////////////////////////

void serverInit()
{

  server.config.max_uri_handlers = 20; // maximum number of uri handlers (.on() calls)
  server.listen(80);

  DefaultHeaders::Instance().addHeader("Server", "PsychicHttp");

  PsychicStaticFileHandler *handler = server.serveStatic("/", LittleFS, "/www/");
  // handler->setFilter(ON_STA_FILTER);
  handler->setCacheControl("max-age=60");

  websocketHandler.onOpen([](PsychicWebSocketClient *client) { 
    logger.log(MAIKO, DEBUG, "[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString()); 
    xTaskNotifyGive(syncClientsHandle);
  });

  websocketHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame)
                           {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, frame->payload, frame->len);
    if (error) {
      return ESP_FAIL;
    }
    else {
      logger.log(MAIKO, DEBUG, "Received JSON payload: %s", frame->payload);
      JsonObject obj = doc["led"];
      if(!obj.isNull()) {
        int index = obj["index"];
        int warmCycle = obj["warmCycle"];
        int coldCycle = obj["coldCycle"];
        ledData* led = configData.getLedData(0);
        led->warmCycle = warmCycle;
        led->coldCycle = coldCycle;
        lastInputTime = millis();
        CONFIG_SAVED = false;

      }




      return ESP_OK;
    } });

  websocketHandler.onClose([](PsychicWebSocketClient *client)
                           {
                             // logger.log(MAIKO,DEBUG, "[socket] connection #%u closed from %s", client->socket(), client->remoteIP());
                           });

  // attach the handler to /ws.  You can then connect to ws://ip.address/ws
  server.on("/ws", &websocketHandler);

  /*
      //EventSource server
      // curl -i -N http://psychic.local/events
      eventSource.onOpen([](PsychicEventSourceClient *client) {
        logger.log(MAIKO,INFO, "[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
        client->send("Hello user!", NULL, millis(), 1000);
      });
      eventSource.onClose([](PsychicEventSourceClient *client) {
        logger.log(MAIKO,INFO, "[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
      });
      server.on("/events", &eventSource);

  */

  server.onOpen([](PsychicClient *client)
                {
                  // logger.log(MAIKO,DEBUG, "[client connection #%u connected from %s", client->socket(), client->remoteIP());
                  // syncClients();
                });

  server.onClose([](PsychicClient *client)
                 {
                   // logger.log(MAIKO,DEBUG, "[client] connection #%u closed from %s", client->socket(), client->remoteIP());
                 });

  server.on("/credWifi", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
            {
              JsonObject doc = json.as<JsonObject>();

              strncpy(configData.ssid, doc["ssid"], sizeof(configData.ssid));
              strncpy(configData.password, doc["password"], sizeof(configData.password));

              configData.save("/config.json");

              request->reply("OK");
              configData.connectedToAP = false;
              xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
              return ESP_OK;
            });

  server.on("/credSWifi", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
            {
      JsonObject doc = json.as<JsonObject>();


      configData.useStaticWiFi = doc["Swifi"];

      configData.Swifi_ip.fromString(doc["Swifi_ip"].as<String>());
      configData.Swifi_subnet.fromString(doc["Swifi_sub"].as<String>());
      configData.Swifi_gw.fromString(doc["Swifi_gw"].as<String>());
      configData.Swifi_dns.fromString(doc["Swifi_dns"].as<String>());

      
      configData.save("/config.json");

      request->reply("OK");
      configData.connectedToAP = false;
      xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
      return ESP_OK; });

  server.on("/doScanWiFi", HTTP_GET, [](PsychicRequest *request)
            {
      request->reply(scanWifiNetworks().c_str()); 
      return ESP_OK; });
}

void setup()
{
  sema_Server = xSemaphoreCreateMutex();

  SERVER_GROUP = xEventGroupCreate();
  networkEventGroup = xEventGroupCreate();

#ifdef ST_DEBUG
  Serial.begin(115200);
  logger.registerSerial(MAIKO, DEBUG, "MAIKO"); // We want messages with DEBUG level and lower    delay(1000);
#endif

#ifdef WROVER
  Wire.begin(SDA_PIN, SCL_PIN);
#endif


  if (!storageManager.mountLittleFS()) {
    logger.log(MAIKO, ERROR, "File System Mount Failed");
    return;
  } else {
    configData.load("/config.json");

  }



  strncpy(configData.devicename, "MAIKO", sizeof(configData.devicename));

    ledData* led1 = configData.getLedData(0);
    ledData* led2 = configData.getLedData(1);

    led1->warmChannel = 0;
    led1->coldChannel = 1;
    led1->warmPin = 25;
    led1->coldPin = 26;

    led2->warmChannel = 2;
    led2->coldChannel = 3;
    led2->warmPin = 27;
    led2->coldPin = 14;   


  for(size_t i; i < LEDCOUNT; i++) {
    ledData* led = configData.getLedData(i);
    ledcSetup(led->warmChannel, pwmFreq,pwmResolution);
    ledcSetup(led->coldChannel, pwmFreq,pwmResolution);
    ledcAttachPin(led->warmPin, led->warmChannel);
    ledcAttachPin(led->coldPin, led->warmChannel);
  }


  WiFi.onEvent(WiFiEvent);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(configData.devicename);

  Udp.begin(localPort);

  if (MDNS.begin(configData.devicename))
  {
    MDNS.addService("http", "tcp", 80);
    logger.log(MAIKO, DEBUG, "MDNS: started");
  }
  else
  {
    logger.log(MAIKO, ERROR, "MDNS: failed to start");
  }

  serverInit();

  xTaskCreate(AutoSave, "AutoSave", 4096, NULL, 2, &AutoSaveHandle);
  xTaskCreate(checkNetwork, "checkNetwork", 2048, NULL, 1, &checkNetworksHandle);
  xTaskCreate(syncClients, "Sync CLients", 4096, NULL, 1, &syncClientsHandle);
  xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
}
void loop() {
  // DNS server processing for AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    dnsServer.processNextRequest();
  }

  for (size_t i; i < LEDCOUNT; i++) {
    setLedDutyCycle(i);
  }
}
