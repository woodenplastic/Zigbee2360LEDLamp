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

// HERE PASSWORD FOR AP // SSID = ALMALOOX
const char *softAP_password = "12345678";


const int maxScanAttempts = 1;
const int scanInterval = 3000; // Scan every 3 seconds

unsigned long lastInputTime = 0; // Initialize to 0
bool CONFIG_SAVED = true;        // Initialize as true to prevent immediate saving

const unsigned int localPort = 53000; // local port to listen for OSC packets (actually not used for sending)

#include <ELog.h>
#define ALMALOOX 0

SemaphoreHandle_t sema_Server = NULL;

TaskHandle_t AutoSaveHandle = NULL;
TaskHandle_t checkNetworksHandle = NULL;
TaskHandle_t syncClientsHandle = NULL;

EventGroupHandle_t SERVER_GROUP;
EventGroupHandle_t networkEventGroup;

const int MANUAL_TRIGGER_BIT = BIT0;
const int SYNC_CLIENTS_BIT = BIT1;


void setLedDutyCycle() {
  for (int i; i < 2; i++) {
    ledData* led = configData.getLedData(0);
    // Constrain the values to be within the PWM range (0-255)
    int pwm1 = constrain(led->warmCycle * 0.25, 0, 255);
    int pwm2 = constrain(led->coldCycle * 0.25, 0, 255);

    // Write the PWM values to the specified channels
    ledcWrite(led->warmChannel, pwm1);
    ledcWrite(led->coldChannel, pwm2);
    
  }
}


// NETWORK ///////////////////////////////////////////////////////////////////////

bool isSSIDAvailable(const char* ssidToFind) {
  if (ssidToFind == nullptr || *ssidToFind == '\0') {
    return false;
  }

  int scanAttempts = 0;
  while (scanAttempts < maxScanAttempts)
  {
    int networkCount = WiFi.scanNetworks();
    for (size_t i = 0; i < networkCount; ++i)
    {
      if (strcmp(WiFi.SSID(i).c_str(), ssidToFind) == 0)
      {
        return true; // Found the saved SSID
      }
    }
    delay(scanInterval);
    scanAttempts++;
  }
  return false; // Saved SSID not found after scanning
}


void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_READY:
    logger.log(ALMALOOX, INFO, "WiFi Ready");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    logger.log(ALMALOOX, INFO, "WiFi Scan Done");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    logger.log(ALMALOOX, INFO, "WiFi STA Started");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    logger.log(ALMALOOX, INFO, "WiFi STA Stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    logger.log(ALMALOOX, INFO, "WiFi Connected");
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    logger.log(ALMALOOX, INFO, "WiFi Disconnected");
    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    logger.log(ALMALOOX, INFO, "WiFi Auth Mode Changed");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    logger.log(ALMALOOX, INFO, "WiFi Got IP: %s", WiFi.localIP().toString().c_str());
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    logger.log(ALMALOOX, INFO, "WiFi Lost IP");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    logger.log(ALMALOOX, INFO, "WiFi AP Started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    logger.log(ALMALOOX, INFO, "WiFi AP Stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    logger.log(ALMALOOX, INFO, "Station Connected to WiFi AP");
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    logger.log(ALMALOOX, INFO, "Station Disconnected from WiFi AP");
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    logger.log(ALMALOOX, INFO, "Station IP Assigned in WiFi AP Mode");
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    logger.log(ALMALOOX, INFO, "Probe Request Received in WiFi AP Mode");
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
    logger.log(ALMALOOX, INFO, "Wi-Fi scan failed!");
    return "{}";
  }
  else if (numNetworks == 0)
  {
    logger.log(ALMALOOX, INFO, "No networks found");
    return "[]";
  }

  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  for (size_t i = 0; i < numNetworks; ++i)
  {
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryption"] = WiFi.encryptionType(i);
  }

  String jsonResult;
  serializeJson(doc, jsonResult);
  logger.log(ALMALOOX, INFO, "Scanned Wi-Fi Networks:");
  logger.log(ALMALOOX, INFO, "%s", jsonResult);

  return jsonResult;
}

void checkNetwork(void* parameter) {
  for (;;) {
    // Wait for either the periodic delay or the manual trigger event
    EventBits_t bits = xEventGroupWaitBits(
      networkEventGroup,
      MANUAL_TRIGGER_BIT,
      pdTRUE, // Clear the bit on exit
      pdFALSE, // Wait for any bit
      pdMS_TO_TICKS(30000) // 30-second delay
    );
    
    // Check if we need to connect to a Wi-Fi network
    if ((WiFi.softAPgetStationNum() == 0) && !WiFi.isConnected()) {

      // Handle WiFi mode switching
      if (WiFi.getMode() == WIFI_STA) {
        WiFi.disconnect();
      }
      if (WiFi.getMode() == WIFI_AP) {
        WiFi.softAPdisconnect();
        dnsServer.stop();
        WiFi.mode(WIFI_STA);
      }

      // Try to connect to the SSID if it's available
      if (isSSIDAvailable(configData.ssid)) {
        if (configData.useStaticWiFi) {
          WiFi.config(configData.Swifi_ip, configData.Swifi_gw, configData.Swifi_subnet, configData.Swifi_dns);
        }
        WiFi.begin(configData.ssid, configData.password);
        logger.log(ALMALOOX, INFO, "Connecting to %s", configData.ssid);
        
        // Wait for up to 10 seconds to see if the connection is successful
        unsigned long startTime = millis();
        bool connected = false;
        while (millis() - startTime < 10000) { // 10 seconds timeout
          if (WiFi.isConnected()) {
            connected = true;
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(1000)); // Check every 500 milliseconds
        }
        
        if (connected) {
          // Successfully connected
          logger.log(ALMALOOX, INFO, "Successfully connected to %s", configData.ssid);
          continue; // Exit the loop and wait for the next event
        } else {
          // Failed to connect
          logger.log(ALMALOOX, INFO, "Failed to connect to %s, switching to AP mode", configData.ssid);
        }
      } else {
        logger.log(ALMALOOX, INFO, "No Saved WiFi SSID, starting AP mode");
      }

      // Switch to AP mode
      if (WiFi.getMode() == WIFI_STA) {
        WiFi.mode(WIFI_AP);
      }
      if (configData.useStaticWiFi) {
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      }
      WiFi.softAP(configData.devicename, softAP_password);
      dnsServer.start(DNS_PORT, "*", apIP);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Short delay to avoid tight loop
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

      logger.log(ALMALOOX, INFO, "[SYSTEM] AUTOSAVING");
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
    logger.log(ALMALOOX, DEBUG, "[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString()); 
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
      logger.log(ALMALOOX, DEBUG, "Received JSON payload: %s", frame->payload);
      JsonObject obj = doc["leds"];
      if(!obj.isNull()) {
        int index = obj["index"];
        int warmCycle = obj["warmCycle"];
        int coldCycle = obj["coldCycle"];
        ledData* led = configData.getLedData(index);
        led->warmCycle = warmCycle;
        led->coldCycle = coldCycle;
        lastInputTime = millis();
        CONFIG_SAVED = false;

      }




      return ESP_OK;
    } });

  websocketHandler.onClose([](PsychicWebSocketClient *client)
                           {
                             // logger.log(ALMALOOX,DEBUG, "[socket] connection #%u closed from %s", client->socket(), client->remoteIP());
                           });

  // attach the handler to /ws.  You can then connect to ws://ip.address/ws
  server.on("/ws", &websocketHandler);

  /*
      //EventSource server
      // curl -i -N http://psychic.local/events
      eventSource.onOpen([](PsychicEventSourceClient *client) {
        logger.log(ALMALOOX,INFO, "[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
        client->send("Hello user!", NULL, millis(), 1000);
      });
      eventSource.onClose([](PsychicEventSourceClient *client) {
        logger.log(ALMALOOX,INFO, "[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
      });
      server.on("/events", &eventSource);

  */

  server.onOpen([](PsychicClient *client)
                {
                  // logger.log(ALMALOOX,DEBUG, "[client connection #%u connected from %s", client->socket(), client->remoteIP());
                  // syncClients();
                });

  server.onClose([](PsychicClient *client)
                 {
                   // logger.log(ALMALOOX,DEBUG, "[client] connection #%u closed from %s", client->socket(), client->remoteIP());
                 });

  server.on("/redirect", HTTP_GET, [](PsychicRequest* request)
    { return request->redirect("http://almaloox.local"); });

  server.on("/connecttest.txt", HTTP_GET, [](PsychicRequest* request)
    { return request->redirect("http://almaloox.local"); });

  server.on("/hotspot-detect.html", HTTP_GET, [](PsychicRequest* request)
    { return request->redirect("http://almaloox.local"); });

  server.on("/generate_204", HTTP_GET, [](PsychicRequest* request) {
    return request->redirect("http://almaloox.local"); });

  server.on("/gen_204", HTTP_GET, [](PsychicRequest* request) { 
    return request->redirect("http://almaloox.local"); });


  server.on("/credWifi", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
            {
              serializeJsonPretty(json, Serial);
              JsonObject doc = json.as<JsonObject>();

              strncpy(configData.ssid, doc["ssid"], sizeof(configData.ssid));
              strncpy(configData.password, doc["password"], sizeof(configData.password));

              configData.save("/config.json");

              request->reply("OK");
              WiFi.softAPdisconnect();
              
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
      WiFi.softAPdisconnect();
      
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

#ifdef ALMALOOX_DEBUG
  Serial.begin(115200);
  logger.registerSerial(ALMALOOX, DEBUG, "ALMALOOX");
#endif

#ifdef WROVER
  Wire.begin(SDA_PIN, SCL_PIN);
#endif


  if (!storageManager.mountLittleFS()) {
    logger.log(ALMALOOX, ERROR, "File System Mount Failed");
    return;
  } else {
    configData.load("/config.json");

  }


  for(size_t i; i < LEDCOUNT; i++) {
    ledData* led = configData.getLedData(i);
    ledcSetup(led->warmChannel, pwmFreq,pwmResolution);
    ledcSetup(led->coldChannel, pwmFreq,pwmResolution);
    ledcAttachPin(led->warmPin, led->warmChannel);
    ledcAttachPin(led->coldPin, led->coldChannel);
  }


  WiFi.onEvent(WiFiEvent);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(configData.devicename);

  Udp.begin(localPort);

  if (MDNS.begin(configData.devicename))
  {
    MDNS.addService("http", "tcp", 80);
    logger.log(ALMALOOX, DEBUG, "MDNS: started");
  }
  else
  {
    logger.log(ALMALOOX, ERROR, "MDNS: failed to start");
  }

  serverInit();

  xTaskCreate(AutoSave, "AutoSave", 4096, NULL, 2, &AutoSaveHandle);
  xTaskCreate(checkNetwork, "checkNetwork", 2048, NULL, 1, &checkNetworksHandle);
  xTaskCreate(syncClients, "Sync CLients", 4096, NULL, 1, &syncClientsHandle);
  xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
}
void loop() {
  setLedDutyCycle();
  // DNS server processing for AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    dnsServer.processNextRequest();
  }



}
