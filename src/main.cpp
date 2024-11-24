#include <Arduino.h>
#include <WiFiUdp.h>
WiFiUDP Udp;

#include <stdarg.h>

#include <initializer_list>

#include "DRFZigbee.h"
#include "M5Stack.h"
#include "byteArray.h"
#include "LittleFS.h"

#include <DFRobot_GP8XXX.h>

DFRobot_GP8XXX_IIC GP8413_0(RESOLUTION_15_BIT, 0x59, &Wire);
DFRobot_GP8XXX_IIC GP8413_1(RESOLUTION_15_BIT, 0x58, &Wire);

uint16_t mapTo15Bit(uint16_t value)
{
  // Ensure the input value is within the expected range
  if (value > 1024)
    value = 1024;

  // Perform the mapping calculation
  return (value * 10000) / 1024;
}

// range is 0~10000mv
void setDacVoltage0(uint16_t vol, uint8_t ch)
{
  uint16_t setting_vol = 0;
  if (vol > 10000)
  {
    vol = 10000;
  }
  if (ch > 1)
    ch = 1;
  setting_vol = (int16_t)((float)vol / 10000.0f * 32767.0f);
  if (setting_vol > 32767)
  {
    setting_vol = 32767;
  }
  GP8413_0.setDACOutVoltage(setting_vol, ch);
}

void setDacVoltage1(uint16_t vol, uint8_t ch)
{
  uint16_t setting_vol = 0;
  if (vol > 10000)
  {
    vol = 10000;
  }
  if (ch > 1)
    ch = 1;
  setting_vol = (int16_t)((float)vol / 10000.0f * 32767.0f);
  if (setting_vol > 32767)
  {
    setting_vol = 32767;
  }
  GP8413_1.setDACOutVoltage(setting_vol, ch);
}

#define ZIGBEE_PANID 0x22DB
// #define ZIGBEE_PANID    0x162A 0x22DB

// #define COORDINNATOR

char asciiHexList[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

DRFZigbee zigbee;
int lastInput = 0;
int lastCheckZigbee = 0;

uint16_t reviceCount = 0, timeoutCount = 0, errorCount = 0;
unsigned long reviceTime = 0;

bool flushFlag = true;

void configZigbee()
{

  DRFZigbee::zigbee_arg_t *arg = new DRFZigbee::zigbee_arg_t;
  zigbee.linkMoudle();
  zigbee.readModuleparm(arg);

  Serial.printf("main_pointType: %02X\r\n", arg->main_pointType);
  Serial.printf("main_PANID: %04X\r\n", arg->main_PANID);
  Serial.printf("main_channel: %02X\r\n", arg->main_channel);
  Serial.printf("main_transmissionMode: %02X\r\n", arg->main_transmissionMode);
  Serial.printf("main_customID: %04X\r\n", arg->main_customID);
  Serial.printf("main_res0: %04X\r\n", arg->main_res0);
  Serial.printf("main_uartBaud: %02X\r\n", arg->main_uartBaud);
  Serial.printf("main_uartBit: %02X\r\n", arg->main_uartBit);
  Serial.printf("main_uatrtStop: %02X\r\n", arg->main_uatrtStop);
  Serial.printf("main_uartCheck: %02X\r\n", arg->main_uartCheck);
  Serial.printf("main_res1: %04X\r\n", arg->main_res1);
  Serial.printf("main_ATN: %02X\r\n", arg->main_ATN);
  Serial.printf("main_mac: ");
  for (int i = 0; i < 8; i++)
  {
    Serial.printf("%02X", arg->main_mac[i]);
  }
  Serial.printf("\r\n");

  Serial.printf("preset_pointType: %02X\r\n", arg->preset_pointType);
  Serial.printf("preset_PANID: %04X\r\n", arg->preset_PANID);
  Serial.printf("preset_channel: %02X\r\n", arg->preset_channel);
  Serial.printf("preset_transmissionMode: %02X\r\n", arg->preset_transmissionMode);
  Serial.printf("preset_customID: %04X\r\n", arg->preset_customID);
  Serial.printf("preset_res0: %04X\r\n", arg->preset_res0);
  Serial.printf("preset_uartBaud: %02X\r\n", arg->preset_uartBaud);
  Serial.printf("preset_uartBit: %02X\r\n", arg->preset_uartBit);
  Serial.printf("preset_uatrtStop: %02X\r\n", arg->preset_uatrtStop);
  Serial.printf("preset_uartCheck: %02X\r\n", arg->preset_uartCheck);
  Serial.printf("preset_res1: %04X\r\n", arg->preset_res1);
  Serial.printf("preset_ATN: %02X\r\n", arg->preset_ATN);

  Serial.printf("shortAddr: %04X\r\n", arg->shortAddr);
  Serial.printf("res3: %02X\r\n", arg->res3);
  Serial.printf("encryption: %02X\r\n", arg->encryption);
  Serial.printf("password: ");
  for (int i = 0; i < 4; i++)
  {
    Serial.printf("%02X", arg->password[i]);
  }
  Serial.printf("\r\n");

  Serial.printf("Wave: ");
  for (int i = 0; i < 48; i++)
  {
    Serial.printf("%02X", arg->Wave[i]);
  }
  Serial.printf("\r\n");

#ifdef COORDINNATOR
  arg->main_pointType = DRFZigbee::kCoordinator;
#else
  arg->main_pointType = DRFZigbee::kEndDevice;
#endif
  arg->main_PANID = DRFZigbee::swap<uint16_t>(ZIGBEE_PANID);
  arg->main_channel = 20;
  arg->main_transmissionMode = DRFZigbee::kN2Ntransmission;
  arg->main_ATN = DRFZigbee::kANTEXP;

  Serial.printf("PAIN ID:%04X\r\n", arg->main_PANID);

  zigbee.setModuleparm(*arg);
  zigbee.rebootModule();
  delay(500);
}

#include "configData.h"
ConfigManager config;

#include "networkManager.h"
NetworkManager& network = NetworkManager::getInstance();

#include <ArduinoJson.h>

#include "storage.h"
StorageManager storageManager;

#include <PsychicHttp.h>

// credentials for the /auth-basic and /auth-digest examples
const char *app_user = "admin";
const char *app_pass = "admin";
const char *app_name = "Your App";

// #define PSY_ENABLE_SSL to enable ssl
#ifdef PSY_ENABLE_SSL
bool app_enable_ssl = true;
String server_cert;
String server_key;
#endif

// our main server object
#ifdef PSY_ENABLE_SSL
PsychicHttpsServer server;
#else
PsychicHttpServer server;
#endif
PsychicWebSocketHandler websocketHandler;
PsychicEventSource eventSource;

unsigned long lastInputTime = 0; // Initialize to 0
bool CONFIG_SAVED = true;        // Initialize as true to prevent immediate saving

TaskHandle_t AutoSaveHandle = NULL;
TaskHandle_t checkNetworksHandle = NULL;

EventGroupHandle_t networkEventGroup;

const int MANUAL_TRIGGER_BIT = BIT0;
const int SYNC_CLIENTS_BIT = BIT1;

void setLedDutyCycle(int index)
{

  ledData *led = config.getLedData(index);
  // Constrain the values to be within the PWM range (0-255)
  int pwm1 = constrain(led->warmCycle * 0.25, 0, 255);
  int pwm2 = constrain(led->coldCycle * 0.25, 0, 255);

  // Write the PWM values to the specified channels
  ledcWrite(led->warmChannel, pwm1);
  ledcWrite(led->coldChannel, pwm2);

  if (index == 0)
  {
    setDacVoltage0(mapTo15Bit(led->warmChannel), 0);
    setDacVoltage0(mapTo15Bit(led->coldChannel), 1);
  }
  else
  {
    setDacVoltage1(mapTo15Bit(led->warmChannel), 0);
    setDacVoltage1(mapTo15Bit(led->coldChannel), 1);
  }
}

// NETWORK ///////////////////////////////////////////////////////////////////////

void checkNetwork(void *parameter)
{
  for (;;)
  {
    EventBits_t bits = xEventGroupWaitBits(
        networkEventGroup,
        MANUAL_TRIGGER_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(30000));

      network.check();
      vTaskDelay(1);
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
      config.save();

      Serial.printf("[SYSTEM] AUTOSAVING");
    }
  }
}

void syncClients()
{

  String bdata;
  serializeJson(config.write(), bdata);
  websocketHandler.sendAll(bdata.c_str());
}

/////////////////////////////////////////////////////////////

void serverInit()
{


  server.config.max_uri_handlers = 20; // maximum number of uri handlers (.on() calls)

#ifdef PSY_ENABLE_SSL
  server.ssl_config.httpd.max_uri_handlers = 20; // maximum number of uri handlers (.on() calls)

  // do we want secure or not?
  if (app_enable_ssl)
  {
    server.listen(443, server_cert.c_str(), server_key.c_str());

    // this creates a 2nd server listening on port 80 and redirects all requests HTTPS
    PsychicHttpServer *redirectServer = new PsychicHttpServer();
    redirectServer->config.ctrl_port = 20424; // just a random port different from the default one
    redirectServer->listen(80);
    redirectServer->onNotFound([](PsychicRequest *request)
                               {
      String url = "https://" + request->host() + request->url();
      return request->redirect(url.c_str()); });
  }
  else
    server.listen(80);
#else
  server.listen(80);
#endif

  DefaultHeaders::Instance().addHeader("Server", "PsychicHttp");

  PsychicStaticFileHandler *handler = server.serveStatic("/", LittleFS, "/www/");
  // handler->setFilter(ON_STA_FILTER);
  handler->setCacheControl("max-age=60");

  websocketHandler.onOpen([](PsychicWebSocketClient *client)
                          {
    Serial.printf("[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString().c_str());
    syncClients(); });

  websocketHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame)
                           {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, frame->payload, frame->len);
    if (error) {
      return ESP_FAIL;
    }
    else {
      Serial.printf( "Received JSON payload: %s", frame->payload);
      JsonObject obj = doc["leds"];
      if(!obj.isNull()) {
        int index = obj["index"];
        int warmCycle = obj["warmCycle"];
        int coldCycle = obj["coldCycle"];
        ledData* led = config.getLedData(index);
        led->warmCycle = warmCycle;
        led->coldCycle = coldCycle;
        setLedDutyCycle(index);
        lastInputTime = millis();
        CONFIG_SAVED = false;

      }




      return ESP_OK;
    } });
  websocketHandler.onClose([](PsychicWebSocketClient *client)
                           { Serial.printf("[socket] connection #%u closed from %s", client->socket(), client->remoteIP()); });

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

  server.on("/redirect", HTTP_GET, [](PsychicRequest *request)
            { return request->redirect("http://almaloox.local"); });

  server.on("/connecttest.txt", HTTP_GET, [](PsychicRequest *request)
            { return request->redirect("http://almaloox.local"); });

  server.on("/hotspot-detect.html", HTTP_GET, [](PsychicRequest *request)
            { return request->redirect("http://almaloox.local"); });

  server.on("/generate_204", HTTP_GET, [](PsychicRequest *request)
            { return request->redirect("http://almaloox.local"); });

  server.on("/gen_204", HTTP_GET, [](PsychicRequest *request)
            { return request->redirect("http://almaloox.local"); });

  server.on("/credWifi", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
            {
              serializeJsonPretty(json, Serial);
              JsonObject doc = json.as<JsonObject>();

              strncpy(config.network.ssid, doc["ssid"], sizeof(config.network.ssid));
              strncpy(config.network.password, doc["password"], sizeof(config.network.password));

              config.save();

              request->reply("OK");
              WiFi.softAPdisconnect();
              
              xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
              return ESP_OK; });

  server.on("/credSWifi", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
            {
      JsonObject doc = json.as<JsonObject>();


      config.network.useStaticWiFi = doc["Swifi"];

      config.network.Swifi_ip.fromString(doc["Swifi_ip"].as<String>());
      config.network.Swifi_subnet.fromString(doc["Swifi_sub"].as<String>());
      config.network.Swifi_gw.fromString(doc["Swifi_gw"].as<String>());
      config.network.Swifi_dns.fromString(doc["Swifi_dns"].as<String>());

      
      config.save();

      request->reply("OK");
      WiFi.softAPdisconnect();
      
      xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
      return ESP_OK; });

  server.on("/doScanWiFi", HTTP_GET, [](PsychicRequest *request)
            {
      request->reply(network.scanWifi().c_str()); 
      return ESP_OK; });
}

void AppCoordinator()
{
  Serial.printf("AppCoordinator\r\n");

  DRFZigbee::zigbee_arg_t *arg = new DRFZigbee::zigbee_arg_t;
  zigbee.readModuleparm(arg);
  arg->main_pointType = DRFZigbee::kCoordinator;

  zigbee.setModuleparm(*arg);
  zigbee.rebootModule();

  delete arg;
}

void AppRouter()
{
  Serial.printf("AppRouter\r\n");

  DRFZigbee::zigbee_arg_t *arg = new DRFZigbee::zigbee_arg_t;
  zigbee.readModuleparm(arg);
  arg->main_pointType = DRFZigbee::kRouter;
  arg->main_PANID = DRFZigbee::swap<uint16_t>(ZIGBEE_PANID);
  arg->main_transmissionMode = DRFZigbee::kN2Ntransmission;
  arg->main_channel = 20;
  arg->main_ATN = DRFZigbee::kANTEXP;
  zigbee.setModuleparm(*arg);
  zigbee.rebootModule();

  zigbee.readModuleparm(arg);
  Serial.printf("main_pointType: %02X\r\n", arg->main_pointType);
  Serial.printf("main_PANID: %04X\r\n", arg->main_PANID);
  Serial.printf("main_channel: %02X\r\n", arg->main_channel);
  Serial.printf("main_transmissionMode: %02X\r\n", arg->main_transmissionMode);
  Serial.printf("main_customID: %04X\r\n", arg->main_customID);
  Serial.printf("main_res0: %04X\r\n", arg->main_res0);
  Serial.printf("main_res1: %04X\r\n", arg->main_res1);
  Serial.printf("main_ATN: %02X\r\n", arg->main_ATN);
  Serial.printf("main_mac: ");
  for (int i = 0; i < 8; i++)
  {
    Serial.printf("%02X", arg->main_mac[i]);
  }
  Serial.printf("\r\n");

  while (1)
  {
    DRFZigbee::reviceData_t revice;
    if (zigbee.reviceData(&revice) == DRFZigbee::kReviceOK)
    {
      revice.array->printfArray<HardwareSerial>(&Serial);
    }
  }
}

void AppEndDevice()
{
  Serial.printf("AppEndDevice\r\n");

  DRFZigbee::zigbee_arg_t *arg = new DRFZigbee::zigbee_arg_t;
  zigbee.readModuleparm(arg);
  arg->main_pointType = DRFZigbee::kEndDevice;
  arg->main_PANID = DRFZigbee::swap<uint16_t>(ZIGBEE_PANID);
  arg->main_transmissionMode = DRFZigbee::kN2Ntransmission;
  zigbee.setModuleparm(*arg);
  zigbee.rebootModule();

  zigbee.readModuleparm(arg);

  Serial.printf("main_pointType: %02X\r\n", arg->main_pointType);
  Serial.printf("main_PANID: %04X\r\n", arg->main_PANID);
  Serial.printf("main_channel: %02X\r\n", arg->main_channel);
  Serial.printf("main_transmissionMode: %02X\r\n", arg->main_transmissionMode);
  Serial.printf("main_customID: %04X\r\n", arg->main_customID);
  Serial.printf("main_res0: %04X\r\n", arg->main_res0);
  Serial.printf("main_res1: %04X\r\n", arg->main_res1);
  Serial.printf("main_ATN: %02X\r\n", arg->main_ATN);
  Serial.printf("main_mac: ");

  Serial.printf("\r\n");

  uint8_t senduff[256];
  char revicechar[256];
  uint16_t charPos = 0;

  memset(senduff, 0, 256);

  while (1)
  {
    DRFZigbee::reviceData_t revice;
    if (zigbee.reviceData(&revice, 10) == DRFZigbee::kReviceOK)
    {
      revice.array->printfArray<HardwareSerial>(&Serial);
      memset(revicechar, 0, 256);
      memcpy(revicechar, revice.array->dataptr(), revice.length);
    }

    if (digitalRead(5) == LOW)
    {
      zigbee.sendDataP2P(DRFZigbee::kP2PShortAddrMode, 0xffff,
                         senduff, charPos);
      memset(senduff, 0, 256);
      charPos = 0;
    }
  }
}

void setup()
{

#ifdef ALMALOOX_DEBUG
  Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, 16, 17);
#endif

  Wire.begin(SDA, SCL);

  if (!storageManager.mountLittleFS())
  {
    Serial.printf("File System Mount Failed");
    return;
  }
  else
  {
    config.load();
  }

  for (size_t i; i < LEDCOUNT; i++)
  {
    ledData *led = config.getLedData(i);
    ledcSetup(led->warmChannel, pwmFreq, pwmResolution);
    ledcSetup(led->coldChannel, pwmFreq, pwmResolution);
    ledcAttachPin(led->warmPin, led->warmChannel);
    ledcAttachPin(led->coldPin, led->coldChannel);
  }

  networkEventGroup = xEventGroupCreate();

  network.init();
  serverInit();

  xTaskCreate(checkNetwork, "checkNetwork", 4096, NULL, 1, &checkNetworksHandle);
  xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);

  setLedDutyCycle(0);
  setLedDutyCycle(1);


  for (uint8_t address = 0x58; address <= 0x59; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found I2C at: 0x%02X\n", address);
    }
    else {
    }
  }


  if (GP8413_0.begin() != 0)
  {
    Serial.println("Init Fail!");
  }
  else
  {
    Serial.println("INIT I2C 1 good");

    ledData *led = config.getLedData(0);
    // set channel0
    setDacVoltage0(led->warmChannel, 0);
    // set channel1
    setDacVoltage0(led->coldChannel, 1);
  }

  if (GP8413_1.begin() != 0)
  {
    Serial.println("Init Fail!");
  }
  else
  {
    Serial.println("INIT I2C 2 good");
    ledData *led = config.getLedData(1);
    // set channel0
    setDacVoltage1(led->warmChannel, 0);
    // set channel1
    setDacVoltage1(led->coldChannel, 1);
  }



  xTaskCreate(AutoSave, "AutoSave", 4096, NULL, 2, &AutoSaveHandle);

  /*


    zigbee.begin(Serial2);

    zigbee.linkMoudle();

    pinMode(5, INPUT_PULLUP);

    xTaskCreate([](void *arg)
                {
          uint8_t readbuff[128];
          while(1){
              int length = Serial2.available();
              if( length > 0) {
      Serial2.readBytes(readbuff,length);Serial.write(readbuff,length);}
              delay(10);
          } }, "reviceTask", 2048, nullptr, 2, nullptr);



    DRFZigbee::zigbee_arg_t *arg1 = new DRFZigbee::zigbee_arg_t;
    zigbee.readModuleparm(arg1);
    arg1->main_pointType = DRFZigbee::kRouter;
    arg1->main_PANID = DRFZigbee::swap<uint16_t>(ZIGBEE_PANID);
    arg1->main_transmissionMode = DRFZigbee::kN2Ntransmission;
    arg1->main_channel = 20;
    arg1->main_ATN = DRFZigbee::kANTEXP;
    zigbee.setModuleparm(*arg1);
    zigbee.rebootModule();


      //AppRouter();
      AppEndDevice();

    if (zigbee.sendCMDAndWaitRevice(0xfc, ZIGBEE_CMD_LINKMODULE) !=
        DRFZigbee::kReviceOK)
    {
          Serial.printf("Link Zigbee module faild!code:%d\r\n",zigbee.lastErrorcode); while(1) delay(100);
    }

    byteArray array;
    if (zigbee.sendCMDAndWaitRevice(0xfc, ZIGBEE_CMD_READPARM, &array) !=
        DRFZigbee::kReviceOK)
    {
          Serial.printf("Read module pram faild!code:%d\r\n",zigbee.lastErrorcode); while(1) delay(100);
    }

    if ((array.at(0) != 0x0A) || (array.at(1) != 0x0E))
    {
      Serial.println("Read module pram faild!");
      while (1)
        delay(100);
    }
    array = array.mid(2);

    array.printfArray<HardwareSerial>(&Serial);

    DRFZigbee::zigbee_arg arg;
    memcpy(arg.Wave, array.dataptr(), sizeof(DRFZigbee::zigbee_arg));

    arg.main_pointType = DRFZigbee::kCoordinator;
    arg.main_PANID = DRFZigbee::swap<uint16_t>(0x6889);
    arg.main_channel = 20;
    arg.main_ATN = DRFZigbee::kANTEXP;
    arg.main_transmissionMode = DRFZigbee::kN2Ntransmission;
    arg.main_customID = DRFZigbee::swap<uint16_t>(0x1213);
    arg.res3 = 0x01;

    byteArray sendArray;
    sendArray += 0x07;
    sendArray += byteArray(&arg.Wave[0], 16);
    sendArray += byteArray(&arg.Wave[24], 16);
    sendArray += byteArray(&arg.Wave[42], 6);

    sendArray.printfArray<HardwareSerial>(&Serial);

    if (zigbee.sendCMDAndWaitRevice(0xfc, sendArray, &array) !=
        DRFZigbee::kReviceOK)
    {
          Serial.printf("Read module pram faild!code:%d\r\n",zigbee.lastErrorcode); while(1) delay(100);
    }

    if (zigbee.sendCMDAndWaitRevice(0xfc, {0x06, 0x44, 0x54, 0x4b, 0xaa, 0xbb}) !=
        DRFZigbee::kReviceOK)
    {
          Serial.printf("reboot Zigbee module faild!code:%d\r\n",zigbee.lastErrorcode); while(1) delay(100);
    }

    delay(1000);

     zigbee.sendDataP2P(DRFZigbee::kP2PShortAddrMode,0x0000,{0x01,0x02,0x03,0x04});

    delay(1000);

    zigbee.nodeList.clear();
    for (int i = 0; i < 10; i++)
    {
        zigbee.getNetworksTopology();
        delay(100);
    }


    if( !zigbee.nodeList.empty() )
    {
        int count = 0;
        std::map<int,DRFZigbee::node>::iterator iter;
        for( iter = zigbee.nodeList.begin(); iter != zigbee.nodeList.end();iter
    ++ )
        {


            iter->first;
            iter->second;
            count++;
        }
    }


    // configZigbee();

    // AppRouter();
    // AppEndDevice();
    // AppCoordinator();

    reviceTime = millis();


    */
}

void loop()
{

  /*

    // DNS server processing for AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
  {
    dnsServer.processNextRequest();
  }


    if (digitalRead(5) == LOW)
    {
      Serial.printf("press Button\r\n");
    }
  */

  /*
  #ifdef COORDINNATOR
    zigbee.sendDataP2P(DRFZigbee::kP2PShortAddrMode, 0xffff,
                       {0xaa, 0x55, 0x01, 0x12});
    delay(50);
  #else
    DRFZigbee::reviceData_t revice;
    if (zigbee.reviceData(&revice, DRFZigbee::kP2PShortAddrMode, 1000) ==
        DRFZigbee::kReviceOK)
    {
      revice.array->printfArray<HardwareSerial>(&Serial);
      if ((revice.array->at(0) == 0xaa) && (revice.array->at(1) == 0x55) &&
          (revice.array->at(3) == 0x12))
      {
        reviceCount++;
      }
      else
      {
        errorCount++;
        Serial.printf("Error\r\n");
      }
    }

    if (reviceCount + errorCount == 10)
    {
      char strbuff[256];
      sprintf(strbuff, "%d %d %d %ld\r\n", reviceCount + errorCount,
              reviceCount, errorCount, (millis() - reviceTime));


      int8_t rssi = -1;
      do
      {
        rssi = zigbee.getModuleRSSI();
        delay(100);
      } while (rssi == -1);

      sprintf(strbuff, "RSSI: -%d\r\n", rssi);

      while (1)
      {
        if (millis() - lastCheckZigbee > 1000)
        {
          configZigbee();
          break;
        }
      }

      reviceTime = millis();
      errorCount = 0;
      reviceCount = 0;
      delay(10);
    }




    int8_t rssi = zigbee.getModuleRSSI();
    Serial.printf("rssi:%d",rssi);
    if( rssi != -1 )
    {
        char strbuff[256];
        sprintf(strbuff,"RSSI:%d\r\n",rssi);

        rssiPrintSprite.setTextColor(M5.Lcd.color565(0xab,0xff,0x58));
        rssiPrintSprite.fillRect(0, 0, 140, 40, M5.Lcd.color565(56, 56, 56));
        rssiPrintSprite.drawString(strbuff,0,0,4);
        rssiPrintSprite.pushSprite(180,60);
    }
    delay(200);


  #endif
    */
}
