#include <Arduino.h>

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

const char *softAP_password = "12345678";
const int maxScanAttempts = 1;
const int scanInterval = 3000; // Scan every 3 seconds

unsigned long lastInputTime = 0; // Initialize to 0
bool CONFIG_SAVED = true;        // Initialize as true to prevent immediate saving

const unsigned int localPort = 53000; // local port to listen for OSC packets (actually not used for sending)

#include <ELog.h>
#define STOMPCS 0

SemaphoreHandle_t sema_Server = NULL;

TaskHandle_t AutoSaveHandle = NULL;
TaskHandle_t checkNetworksHandle = NULL;
TaskHandle_t syncClientsHandle = NULL;

EventGroupHandle_t SERVER_GROUP;
EventGroupHandle_t networkEventGroup;

const int MANUAL_TRIGGER_BIT = BIT0;
const int SYNC_CLIENTS_BIT = BIT1;


#define LED_PIN_1 14
#define LED_PIN_2 15
#define LED_PIN_3 16
#define LED_PIN_4 0







void setLedDutyCycle(int ledPin1, int ledPin2, int dutyCycle1, int dutyCycle2) {
  float flag1 = 0.25 * dutyCycle1;
  float flag2 = 0.25 * dutyCycle2;
  int pwm1 = flag1 * 0.001 * flag2;
  int pwm2 = flag1 * 0.001 * (250 - flag2);

  analogWrite(ledPin1, pwm1);
  analogWrite(ledPin2, pwm2);
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
    logger.log(STOMPCS, INFO, "WiFi Ready");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    logger.log(STOMPCS, INFO, "WiFi Scan Done");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    logger.log(STOMPCS, INFO, "WiFi STA Started");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    logger.log(STOMPCS, INFO, "WiFi STA Stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    logger.log(STOMPCS, INFO, "WiFi Connected");
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    logger.log(STOMPCS, INFO, "WiFi Disconnected");
    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    logger.log(STOMPCS, INFO, "WiFi Auth Mode Changed");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    logger.log(STOMPCS, INFO, "WiFi Got IP: %s", WiFi.localIP().toString().c_str());
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    logger.log(STOMPCS, INFO, "WiFi Lost IP");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    logger.log(STOMPCS, INFO, "WiFi AP Started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    logger.log(STOMPCS, INFO, "WiFi AP Stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    logger.log(STOMPCS, INFO, "Station Connected to WiFi AP");
    configData.connectedToAP = true;
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    logger.log(STOMPCS, INFO, "Station Disconnected from WiFi AP");
    configData.connectedToAP = false;
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    logger.log(STOMPCS, INFO, "Station IP Assigned in WiFi AP Mode");
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    logger.log(STOMPCS, INFO, "Probe Request Received in WiFi AP Mode");
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
    logger.log(STOMPCS, INFO, "Wi-Fi scan failed!");
    return "{}";
  }
  else if (numNetworks == 0)
  {
    logger.log(STOMPCS, INFO, "No networks found");
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
  logger.log(STOMPCS, INFO, "Scanned Wi-Fi Networks:");
  logger.log(STOMPCS, INFO, "%s", jsonResult);

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
        logger.log(STOMPCS, INFO, "Connecting to %s", configData.ssid);
        return;
      }

      logger.log(STOMPCS, INFO, "No Saved WiFi SSID, starting AP mode");
      if (WiFi.getMode() == WIFI_STA)
      {
        WiFi.mode(WIFI_AP);
      }
      if (configData.useStaticWiFi)
      {
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      }
      WiFi.softAP(configData.devicename, softAP_password);
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

      logger.log(STOMPCS, INFO, "[SYSTEM] AUTOSAVING");
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

  websocketHandler.onOpen([](PsychicWebSocketClient *client)
                          { logger.log(STOMPCS, DEBUG, "[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString()); });

  websocketHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame)
                           {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, frame->payload, frame->len);
    if (error) {
      return ESP_FAIL;
    }
    else {
      logger.log(STOMPCS, DEBUG, "Received JSON payload: %s", frame->payload);
      JsonObject obj = doc["slider"];
      if(!obj.isNull()) {
        int index = obj["index"];
        int value = obj["value"];
        Sliders* slider = getSlider(slidersList, index);
        slider->value = value;
        slider->cycle = map(value, 0, 100, 0, 1023);
      }




      return ESP_OK;
    } });

  websocketHandler.onClose([](PsychicWebSocketClient *client)
                           {
                             // logger.log(STOMPCS,DEBUG, "[socket] connection #%u closed from %s", client->socket(), client->remoteIP());
                           });

  // attach the handler to /ws.  You can then connect to ws://ip.address/ws
  server.on("/ws", &websocketHandler);

  /*
      //EventSource server
      // curl -i -N http://psychic.local/events
      eventSource.onOpen([](PsychicEventSourceClient *client) {
        logger.log(STOMPCS,INFO, "[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
        client->send("Hello user!", NULL, millis(), 1000);
      });
      eventSource.onClose([](PsychicEventSourceClient *client) {
        logger.log(STOMPCS,INFO, "[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
      });
      server.on("/events", &eventSource);

  */

  server.onOpen([](PsychicClient *client)
                {
                  // logger.log(STOMPCS,DEBUG, "[client connection #%u connected from %s", client->socket(), client->remoteIP());
                  // syncClients();
                });

  server.onClose([](PsychicClient *client)
                 {
                   // logger.log(STOMPCS,DEBUG, "[client] connection #%u closed from %s", client->socket(), client->remoteIP());
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
  logger.registerSerial(STOMPCS, DEBUG, "tst"); // We want messages with DEBUG level and lower    delay(1000);
#endif

#ifdef WROVER
  Wire.begin(SDA_PIN, SCL_PIN);
#endif


  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);
  pinMode(LED_PIN_4, OUTPUT);


  if (!storageManager.mountLittleFS())
  {
    logger.log(STOMPCS, ERROR, "File System Mount Failed");
    return;
  }

  configData.load("/config.json");

  WiFi.onEvent(WiFiEvent);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(configData.devicename);

  Udp.begin(localPort);

  if (MDNS.begin(configData.devicename))
  {
    MDNS.addService("http", "tcp", 80);
    logger.log(STOMPCS, DEBUG, "MDNS: started");
  }
  else
  {
    logger.log(STOMPCS, ERROR, "MDNS: failed to start");
  }

  serverInit();

  xTaskCreate(AutoSave, "AutoSave", 4096, NULL, 2, &AutoSaveHandle);
  xTaskCreate(checkNetwork, "checkNetwork", 2048, NULL, 1, &checkNetworksHandle);
  xTaskCreate(syncClients, "Sync CLients", 4096, NULL, 1, &syncClientsHandle);
  xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
}
void loop()
{
  // DNS server processing for AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
  {
    dnsServer.processNextRequest();
  }
  Sliders* sliderValues = slidersList.data();

  setLedDutyCycle(LED_PIN_1, LED_PIN_2, slidersList[0].cycle, slidersList[1].cycle);
  setLedDutyCycle(LED_PIN_3, LED_PIN_4, slidersList[2].cycle, slidersList[3].cycle);

}
