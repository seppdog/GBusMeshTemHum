//SDA GPIO21
//SCL GPIO22

#include "Arduino.h"
#include "GBusHelpers.h"
#include "GBusWifiMesh.h"
#include <EEPROM.h>
#include <WiFi.h>
#include "Tasker.h"
#include "Adafruit_HTU21DF.h"
#include <ArduinoJson.h>

#define FWVERSION "1.1"
#define MODULNAME "GBusTempHum"
#define LogLevel ESP_LOG_NONE

#define GetTempHumIntervall (5*60*1000)

MeshApp mesh;
Tasker tasker;

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

// Prototypes
void meshMessage(String msg, uint32_t from, int flag);
void SentNodeInfo();
void RootNotActiveWatchdog();
void meshConnected();
void ReadTempHum();


uint8_t ModulType = 255;

void setup()
{
  Serial.begin(115200);
  /**
   * @brief Set the log level for serial port printing.
   */
  esp_log_level_set("*", LogLevel);
  esp_log_level_set(TAG, LogLevel);

  MDF_LOGI("ModuleType: %u\n", ModulType);

  pinMode(2,OUTPUT);
  digitalWrite(2,HIGH);
  mesh.onMessage(meshMessage);
  mesh.onConnected(meshConnected);
  mesh.start(false);

  if (!htu.begin())
  {
    MDF_LOGE("Couldn't find sensor!");
  }

  tasker.setTimeout(RootNotActiveWatchdog, CheckForRootNodeIntervall);
  tasker.setTimeout(ReadTempHum, GetTempHumIntervall);
}

void loop()
{
  tasker.loop();
}

void ReadTempHum()
{
  MDF_LOGI("ReadTempHum");
  float temp = htu.readTemperature();
  float rel_hum = htu.readHumidity();

  StaticJsonDocument<500> TempHumJson;

  TempHumJson["Temp"] = String(temp, 1);
  TempHumJson["Hum"] = String(rel_hum, 1);

  String TempHumJsonString;
  serializeJson(TempHumJson, TempHumJsonString);
  String Msg = "MQTT values " + TempHumJsonString;
  mesh.SendMessage(Msg);
  tasker.setTimeout(ReadTempHum, GetTempHumIntervall);
}

void RootNotActiveWatchdog()
{
  ESP.restart();
}

void SentNodeInfo()
{
  mesh_addr_t bssid;
  esp_err_t err = esp_mesh_get_parent_bssid(&bssid);

  char MsgBuffer[300];
  sprintf(MsgBuffer, "MQTT Info ModulName:%s,NodeID:%u,SubType:%u,MAC:%s,WifiStrength:%d,Parent:%s,FW:%s", MODULNAME, mesh.GetNodeId(), ModulType, WiFi.macAddress().c_str(), getWifiStrength(3), hextab_to_string(bssid.addr).c_str(), FWVERSION);
  String Msg = String(MsgBuffer);
  mesh.SendMessage(Msg);
}

void meshConnected()
{
  digitalWrite(2, LOW);
  SentNodeInfo();
}

void meshMessage(String msg, uint32_t from, int flag)
{
  MDF_LOGD("meshMessage msg %u: %s", msg.length(), msg.c_str());

  String Type = getValue(msg, ' ', 0);
  String Number = getValue(msg, ' ', 1);
  String Command = getValue(msg, ' ', 2);
  uint8_t NumberInt = Number.toInt();

  if (Type == "Output")
  {
    
  }
  else if (msg.startsWith("I'm Root!"))
  {
    MDF_LOGI("Gateway hold alive received");
    tasker.cancel(RootNotActiveWatchdog);
    tasker.setTimeout(RootNotActiveWatchdog, CheckForRootNodeIntervall);
  }
  else if (Type == "Config")
  {
    Serial.printf("Config\n");
    String ConfigType = getValue(msg, ' ', 1);
    // Config ModulType 2|4|6
    if (ConfigType == "ModulType")
    {
      String Type = getValue(msg, ' ', 2);
      Serial.printf("New ModulType: %s\n", Type.c_str());
      EEPROM.write(0, (uint8_t)Type.toInt());
      EEPROM.commit();
      ESP.restart();
    }
    else if (ConfigType == "WifiPower")
    {
      String Type = getValue(msg, ' ', 2);
      Serial.printf("WifiPower: %s\n", Type.c_str());
      EEPROM.write(2, (uint8_t)Type.toInt());
      EEPROM.commit();
      ESP.restart();
    }
  }
  else if (Type == "GetNodeInfo")
  {
    SentNodeInfo();
  }
  else if (Type == "Reboot")
  {
    ESP.restart();
  }
}
