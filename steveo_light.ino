// Steve-o's RBG, HomeAssistant/MQTT controlled, nightlight.   
//  
// Thanks to: 
//      @bruhautomation for giving me a jumping off point from his ESP-MQTT-JSON-Multisensor code/video https://github.com/bruhautomation/ESP-MQTT-JSON-Multisensor
//      @corbanmailloux for providing a great framework for implementing flash/fade with HomeAssistant https://github.com/corbanmailloux/esp-mqtt-rgb-led
//

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

const char* wifi_ssid = CONFIG_WIFI_SSID;
const char* wifi_password = CONFIG_WIFI_PASS;
const char* mqtt_server = CONFIG_MQTT_HOST;
const int   mqtt_port = CONFIG_MQTT_PORT;

const char* light_state_topic = CONFIG_MQTT_TOPIC_STATE;
const char* light_set_topic = CONFIG_MQTT_TOPIC_SET;

const char* on_cmd = CONFIG_MQTT_PAYLOAD_ON;
const char* off_cmd = CONFIG_MQTT_PAYLOAD_OFF;

const char* SENSORNAME = CONFIG_MQTT_CLIENT_ID;
const char* OTApassword = CONFIG_OTA_PASS;
const int   OTAport = CONFIG_OTA_PORT;

// Connect NeoPixel to D4 on ESP
#define NEOPIN    D4

Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, NEOPIN, NEO_GRB + NEO_KHZ800);

const int BUFFER_SIZE = 300;

/******************************** GLOBALS for fade/flash *******************************/
byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 255;

byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

bool stateOn = false;

int transitionTime = 0;

WiFiClient espClient;
PubSubClient client(espClient);



/********************************** START SETUP*****************************************/
void setup() {

  Serial.begin(115200);

  Serial.begin(115200);
  delay(10);

  ArduinoOTA.setPort(OTAport);

  ArduinoOTA.setHostname(SENSORNAME);

  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.println("Starting Node named " + String(SENSORNAME));


  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  strip.begin();
  strip.show();

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IPess: ");
  Serial.println(WiFi.localIP());

}




/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {

  Serial.print("I'm setting up wifi!\n");
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("I'm done setting up wifi!\n");
}



/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("I'm calling back!\n");
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }

  if (stateOn) {
    // Update lights
    realRed = map(red, 0, 255, 0, brightness);
    realGreen = map(green, 0, 255, 0, brightness);
    realBlue = map(blue, 0, 255, 0, brightness);
  }
  else {
    realRed = 0;
    realGreen = 0;
    realBlue = 0;
  }

  sendState();
  Serial.print("I'm done calling back!\n");
}



/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
  Serial.print("I'm processing JSON\n");
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], on_cmd) == 0) {
      stateOn = true;
    }
    else if (strcmp(root["state"], off_cmd) == 0) {
      stateOn = false;
    }
  }

  if (root.containsKey("color")) {
    red = root["color"]["r"];
    green = root["color"]["g"];
    blue = root["color"]["b"];
  }

  if (root.containsKey("brightness")) {
    brightness = root["brightness"];
  }

  if (root.containsKey("transition")) {
    transitionTime = root["transition"];
  }
  else {
    transitionTime = 0;
  }

  Serial.print("I'm done processing JSON!\n");
  return true;
}



/********************************** START SEND STATE*****************************************/
void sendState() {
  Serial.print("I'm sending state!\n");
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["state"] = (stateOn) ? on_cmd : off_cmd;
  JsonObject& color = root.createNestedObject("color");
  color["r"] = red;
  color["g"] = green;
  color["b"] = blue;


  root["brightness"] = brightness;


  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  if (!client.connected()) {
    Serial.print("I can't publish if I'm not connected to MQTT\n");
    reconnect();
  } else {
  Serial.print("I'm publishing to MQTT...\n");
  Serial.println(buffer);
  client.publish(light_state_topic, buffer, true);
  }
  Serial.print("I'm done sending state!\n");
}


/********************************** START SET COLOR *****************************************/
void setColor(int inR, int inG, int inB) {
  Serial.print("I'm setting color!\n");
  colorWipe(strip.Color(inR, inG, inB), 50);

  Serial.println("Setting LEDs:");
  Serial.print("r: ");
  Serial.print(inR);
  Serial.print(", g: ");
  Serial.print(inG);
  Serial.print(", b: ");
  Serial.println(inB);

  Serial.print("I'm done setting color!\n");
}

void setBrightness(int inBrightness) {
  Serial.print("I'm setting brightness!\n");
  strip.setBrightness(inBrightness);
  Serial.print("I'm done setting brightness!\n");
}



/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Just checking, I have the wifi.\n");
  } else {
    Serial.print("For whatever reason, I don't have the wifi.\n");
    setup_wifi();
    return;
  }

  Serial.print("I'm reconnecting to MQTT!\n");
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME)) {
      Serial.println("connected");
      client.subscribe(light_set_topic);
      setColor(0, 0, 0);
      sendState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 10 seconds");
      // Wait 10 seconds before retrying
      delay(10000);
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Just checking again, I have the wifi.\n");
      } else {
        Serial.print("You can't connect to MQTT if you don't have the wifi.\n");
        setup_wifi();
        return;
      }
    }
  }
  Serial.print("I'm done reconnecting to MQTT!\n");
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}



/********************************** START MAIN LOOP***************************************/
void loop() {

  Serial.print("Loop-da-loop!\n");
  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print("OMG: I lost my weefees");
    setup_wifi();
    return;
  }

  ArduinoOTA.handle();

  if (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(1);
      Serial.print("I need wifi to connect to MQTT");
      setup_wifi();
      return;
    }
    reconnect();
  }
  client.loop();

  setColor(realRed, realGreen, realBlue);
  setBrightness(brightness);
  Serial.print("fin.\n\n");
      
}
