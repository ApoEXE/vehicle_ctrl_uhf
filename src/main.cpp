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

// LOOP counter
uint32_t time1 = 0;
bool run_once_w21 = 1; // not used
bool run_once_w22 = 1; // not used
bool run_once_w23 = 1; // not used
bool run_once_w24 = 1; // not used
bool run_once_w11 = 1; // not used
bool run_once_w12 = 1; // not used

// GEMINI
unsigned long lastMicro = 0; // main timer
unsigned long lastMicro_send = 0;
bool forward = true;
bool w1 = 0; // Initialize w1
enum State
{
  IDLE,
  W2_STEP1,
  W2_STEP2,
  W2_STEP3,
  W2_STEP4,
  W1_STEP
};
State currentState = IDLE;
size_t w2_counter = 0;
size_t w1_counter = 0;
uint8_t w1_code = 0; // cmd goes in here

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

  UHF_recv();
  //  GEMINI
  if (true)
  // if (micros() - lastMicro >= 50)
  {
    // digitalWrite(D6, state);
    digitalWrite(LED_BUILTIN, state);
    state = !state;
    lastMicro = micros();

    switch (currentState)
    {
    case IDLE:
      if (forward)
      {
        currentState = W2_STEP1;
        w2_counter = 0;
      }
      break;

    case W2_STEP1:
      if (run_once_w21)
      {
        Serial.println("STEP1");
        run_once_w21 = 0;
      }

      digitalWrite(D6, 0);
      if (micros() - lastMicro_send >= 440)
      {
        lastMicro_send = micros();
        currentState = W2_STEP2;
      }
      break;

    case W2_STEP2:
      if (run_once_w22)
      {
        Serial.println("STEP2");
        run_once_w22 = 0;
      }

      digitalWrite(D6, 0);
      if (micros() - lastMicro_send >= 500)
      {
        lastMicro_send = micros();
        currentState = W2_STEP3;
      }
      break;

    case W2_STEP3:
      if (run_once_w23)
      {
        Serial.println("STEP3");
        run_once_w23 = 0;
      }

      digitalWrite(D6, 0);
      if (micros() - lastMicro_send >= 500)
      {
        lastMicro_send = micros();
        currentState = W2_STEP4;
      }
      break;

    case W2_STEP4:
      if (run_once_w24)
      {
        Serial.println("STEP4");
        run_once_w24 = 0;
      }

      digitalWrite(D6, 1);
      if (micros() - lastMicro_send >= 680)
      {
        lastMicro_send = micros();
        w2_counter++;
        if (w2_counter < 4)
        {
          currentState = W2_STEP1;
        }
        else
        {
          currentState = W1_STEP;
          w1_code = cmd * 2; // Calculate w1 code
          w1_counter = 0;
          w1 = 0;
        }
      }
      break;

    case W1_STEP:
      if (run_once_w11)
      {
        Serial.println("ENTERED W1");
        run_once_w11 = 0;
      }

      digitalWrite(D6, w1);
      unsigned long w1_delay = 0;

      if (w1_counter < (w1_code - 1) && w1 == 1)
      {
        w1_delay = 660;
      }
      else if (w1_counter == (w1_code - 1) && w1 == 1)
      {
        w1_delay = 460;
      }
      else if (w1 == 0)
      {
        w1_delay = 320;
      }

      if (micros() - lastMicro_send >= w1_delay)
      {
        lastMicro_send = micros();
        w1 = !w1;
        w1_counter++;
        if (w1_counter >= w1_code)
        {
          currentState = IDLE;
        }
      }
      break;
    }
  }
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