#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

#include <string>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#endif

// OTA
#include <ElegantOTA.h>

#if defined(ESP8266)
ESP8266WebServer server(80);
#elif defined(ESP32)
WebServer server(80);
#endif

#define TIMER_INTERVAL_MS 500

unsigned long ota_progress_millis = 0;

// MQTT
#include <PubSubClient.h>
#define MSG_BUFFER_SIZE (50)
// Update these with values suitable for your network.

// version
#define MAYOR 1
#define MINOR 0
#define PATCH 0

// WIFI PASSWORD
#define WIFI_SSID "MIWIFI_zmiz"
#define WIFI_PASS "3wRdPJut"

String version = String(MAYOR) + "." + String(MINOR) + "." + String(PATCH);
bool state = 1;

// MQTT
std::string device = "car_mod";
std::string TOPIC_IP = "home/" + device;
const char *mqtt_server = "192.168.1.2";
const char *TOPIC = TOPIC_IP.c_str();
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
volatile uint32_t lastMillis = 0;
// OTA
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// MQTT
void callback_mqtt(char *topic, byte *payload, unsigned int length);
void reconnect_mqtt();

// APP
uint8_t cmd = 10;
uint8_t uhf_buf[12];
uint8_t buflen = sizeof(uhf_buf);
RH_ASK driver(9600, D1, D2, D5);
bool recv_flag = false;
void UHF_recv();

uint16_t w2 = 0b0111011101110111; // start=w2 original 0b1110111011101110;
bool w1 = 0;                      // toggle 1
uint16_t tmp_w2 = ~w2;
uint8_t w1_cnt = 0;
bool start_flag = 0; // when one send n*w1 with starting 2*w2
uint8_t w2_cnt = 0;
volatile uint32_t lastMicros = 0;
// LOOP counter
uint32_t time1 = 0;
bool run_once = 0;

void func_w2();
void func_w1(uint8 command);
void setup()
{
  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D6, OUTPUT);
  digitalWrite(D6, LOW);
  Serial.begin(115200);

  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.printf("RX_MODULE\n");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS); // change it to your ussid and password
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
  }
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  server.on("/", []()
            { server.send(200, "text/plain", "Car UHF version" + version); });

  ElegantOTA.begin(&server); // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");

  // SETUP APP

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback_mqtt);
  Serial.println("Delta ms = " + String(millis() - lastMillis) + " " + version);

  // APP
  if (!driver.init())
    Serial.println("init failed");
}

void loop()
{

  server.handleClient();
  ElegantOTA.loop();

  if (!client.connected())
  {
    reconnect_mqtt();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis >= 500)
    {
      snprintf(msg, MSG_BUFFER_SIZE, "UHF Msg: %s", uhf_buf);
      client.publish(TOPIC_IP.c_str(), msg);
      lastMillis = millis();
      if (time1 >= 20)
      {
        digitalWrite(LED_BUILTIN, state);
        state = !state;

        // Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        snprintf(msg, MSG_BUFFER_SIZE, "%s:%s", device.c_str(), WiFi.localIP().toString().c_str());
        client.publish(TOPIC_IP.c_str(), msg);
        time1 = 0;
      }
      time1++;
    }
  }
  volatile long long lastMicro = 0;

  if (micros() - lastMicro >= 50)
  {

    lastMicro = micros();
  }
  UHF_recv();
  func_w2();
  func_w1(cmd);
}
// APP
void UHF_recv()
{
  recv_flag = driver.recv(uhf_buf, &buflen);
  if (recv_flag) // Non-blocking
  {
    // Message with a good checksum received, dump it.
    Serial.print("Message: ");
    Serial.println((char *)uhf_buf);
  }
}

bool func_w2()
{
  bool finish = false;
  for (size_t i = 0; i < 4; i++)
  {
    digitalWrite(D6, 0);
    delayMicroseconds(440);
    digitalWrite(D6, 0);
    delayMicroseconds(500);
    digitalWrite(D6, 0);
    delayMicroseconds(500);
    digitalWrite(D6, 1);
    delayMicroseconds(680);
  }
  finish = true;
  return finish;
}
bool func_w1(uint8 command)
{
  bool finish = false;
  uint8_t code = ((command * 2));
  for (size_t i = 0; i < code; i++)
  {
    digitalWrite(D6, w1);
    if (i < (code - 1) && w1 == 1)
      delayMicroseconds(660);
    if (i == (code - 1) && w1 == 1)
      delayMicroseconds(460);
    if (w1 == 0)
      delayMicroseconds(320);
    w1 = !w1;
  }
  finish = true;
  return finish;
}
// OTA
void onOTAStart()
{
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final)
{
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000)
  {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Serial.println("OTA update finished successfully!");
  }
  else
  {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

// MQTT
void callback_mqtt(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] msg: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if (strcmp(topic, TOPIC) == 0)
    cmd = atoi((char *)payload[0]);
}

void reconnect_mqtt()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(TOPIC, "car ctrl uhf");
      // ... and resubscribe
      client.subscribe(TOPIC);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}