#include <Arduino.h>
#include <max6675.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#define EXTERNAL_SWITCH_PIN 4
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <AutoConnect.h>
#include <EEPROM.h>
#include <PubSubClient.h>

#define TRIGGER_PIN 4 // Trigger switch should be LOW active.
#define HOLD_TIMER 3000
#define SAMPLES 12

WebServer Server;
AutoConnect portal(Server);
AutoConnectConfig config;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

int thermoDO = 19;
int thermoCS = 23;
int thermoCLK = 5;

const char usertopic[20] = "/9nuN1njcjg";
const char sensorusertopic[20] = "/9nuN1njcjg/sensor/";
char mqtt_server[20] = "m2mlight.com";
const char sens_apikey[15] = "gRpj1njcse";
long lastReconnectAttempt = 0;
float sum = 0;
int count_samples = 0;

MAX6675 KTh(thermoCLK, thermoCS, thermoDO);
struct t
{
  uint32_t tStart;
  uint32_t tTimeout;
};
// Tasks and their Schedules.
t t_sampling = {0,  5 * 1000}; // Run every x miliseconds

boolean reconnect()
{
  Serial.print(F("Attempting MQTT connection..."));
  if (client.connect("termocupla2", "mqtt", "m2mlight12"))
  {
    Serial.println(F("connected"));
    client.subscribe(usertopic);
    return true;
  }
  else
  {
    Serial.print(F("failed, rc="));
    Serial.print(client.state());
    return false;
  }
}

bool tCheck(struct t *t)
{
  if (millis() >= t->tStart + t->tTimeout)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void tRun(struct t *t)
{
  t->tStart = millis();
}

void callback(char *topico, byte *payload, unsigned int length)
{
}

void deleteAllCredentials()
{
  AutoConnectCredential credential;
  station_config_t config;
  uint8_t ent = credential.entries();
  while (ent--)
  {
    credential.load((int8_t)ent, &config);
    credential.del((const char *)&config.ssid[ent]);
  }
  WiFi.disconnect(false, true);
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32-Thermocouple");
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  delay(500);

  config.apid = "MINKA-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  config.psk = "minkafab2022";
  config.menuItems = (AC_MENUITEM_CONFIGNEW | AC_MENUITEM_DISCONNECT | AC_MENUITEM_RESET | AC_MENUITEM_HOME);
  config.title = "MinkaFab";
  config.autoReconnect = true;

  config.preserveAPMode = true;
  config.retainPortal = true;
  config.autoRise = false;
  portal.config(config);
  portal.begin();
  config.autoRise = true;
  portal.config(config);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
}

void loop()
{
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    unsigned long tm = millis();
    while (digitalRead(TRIGGER_PIN) == LOW)
    {
      yield();
    }
    // Hold the switch while HOLD_TIMER time to start connect.
    if (millis() - tm > HOLD_TIMER)
      deleteAllCredentials();
  }
  if (tCheck(&t_sampling))
  {
    float temp = KTh.readCelsius();
    sum += temp;
    count_samples++;
    Serial.printf("temp: %.1f sum: %.2f - count: %d \n",temp, sum, count_samples);
    if(count_samples>=SAMPLES)
    {
      sum = sum / count_samples;
      Serial.print("C = ");
      Serial.println(sum);
      if (WiFi.status() == WL_CONNECTED)
      {
        char mess[20];
        mess[0] = '\0';
        char number[10];
        dtostrf((double)sum, 4, 2, number);
        strcat(mess, sens_apikey);
        strcat(mess, "&");
        strcat(mess, number);
        mess[16] = '\0';
        Serial.println(mess);
        client.publish(sensorusertopic, mess);
      }
      else
      {
      }
      count_samples = 0;
      sum = 0;
    }
    tRun(&t_sampling);
  }
  if (!client.connected())
  {
    long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      if (reconnect())
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    client.loop();
  }

  portal.handleClient();
}