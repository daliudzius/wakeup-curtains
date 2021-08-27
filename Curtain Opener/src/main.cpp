#include <Arduino.h>
#include <Unistep2.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <fauxmoESP.h>

// Wifi & MQTT credentials
#include "..\credentials.h"

// Initialize stepper object
// stepper(IN1,IN2,IN3,IN4,stepPerRev,stepDelay)
// stepDelay >= 900 for reliable performance
Unistep2 stepper(14, 12, 13, 16, 4096, 900);

// Initialize fauxmoESP - alexa integration library
fauxmoESP alexa;

// For Serial Monitor
#define SERIAL_BAUDRATE 115200

// This will be the name that Alexa discovers
#define ALEXA_NAME "Curtains"

// This is the esp8266 FLASH button to use as emergency stop
#define BUTTON_PIN 0

// Steps to travel half curtain rod (~10 rotations needed)
int travelSteps = 4096 * 10;

// Track state of curtains (make sure they are open when restarting)
bool curtainsOpen = true;

// Initialize MQTT Client Object
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Topic names
const char *controlTopic = "curtain/control";     // Topic to send controls to ESP
const char *setTravelTopic = "curtain/setTravel"; // Topic to set curtain rod travel steps
const char *logTopic = "curtain/log";             // Topic to log commands

// Used by MQTT to load and convert payload
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

/******************************** WiFi Setup ************************************/
void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/************************* When MQTT Message Received ***************************/
// Is called upon incoming MQTT msg
void mqttCallback(char *topic, byte *payload, unsigned int length)
{

  Serial.print("MQTT msg arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Toggle stepper only if correct topic, and stepper not already moving
  if (strcmp(topic, controlTopic) == 0 && stepper.stepsToGo() == 0 && payload)
  {
    if (curtainsOpen)
    {
      snprintf(msg, MSG_BUFFER_SIZE, "Close requested by MQTT");
      stepper.move(-1 * travelSteps);
      alexa.setState(ALEXA_NAME, false, 255); // update alexa state
      curtainsOpen = false;
    }
    else
    {
      snprintf(msg, MSG_BUFFER_SIZE, "Open requested by MQTT");
      stepper.move(travelSteps);
      alexa.setState(ALEXA_NAME, true, 0); // update alexa state
      curtainsOpen = true;
    }
    client.publish(logTopic, msg);
    Serial.println(msg);
  }
  else if (strcmp(topic, setTravelTopic) == 0 && payload)
  {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    int newSteps = atoi((char *)payload);
    travelSteps = newSteps;
    snprintf(msg, MSG_BUFFER_SIZE, "TravelSteps set to: %d\n", travelSteps);
    Serial.printf(msg);
    client.publish(logTopic, msg);
  }
}

/************************* When Alexa Command Received ***************************/
// Callback when a command from Alexa is received
// You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
// State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%")
void alexaCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
  Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

  // check if Alexa is commanding this device, and if stepper not already moving
  if (strcmp(device_name, ALEXA_NAME) == 0 && stepper.stepsToGo() == 0)
  {
    // If state received as "true" then we open curtains (CW)
    // If state received as "false" then we close curtains (CCW)
    if (state) // Open curtains
    {
      if (!curtainsOpen)
      {
        curtainsOpen = true;
        stepper.move(travelSteps);
        Serial.println("Open requested by Alexa");
        snprintf(msg, MSG_BUFFER_SIZE, "Open requested by Alexa");
        client.publish(logTopic, msg);
      }
      else
      {
        Serial.println("Open requested by Alexa, but already open.");
      }
    }
    else // Close curtains
    {
      if (curtainsOpen)
      {
        curtainsOpen = false;
        stepper.move(-1 * travelSteps);
        Serial.println("Close requested by Alexa");
        snprintf(msg, MSG_BUFFER_SIZE, "Close requested by Alexa");
        client.publish(logTopic, msg);
      }
      else
      {
        Serial.println("Close requested by Alexa, but already closed.");
      }
    }
  }
}

/******************************** Alexa Setup ************************************/
void setup_alexa()
{
  alexa.createServer(true);

  // The TCP port must be 80 for gen3 devices (default is 1901)
  alexa.setPort(80);

  // You can enable/disable alexa device programmatically
  alexa.enable(true);

  // Add virtual device with it's name
  alexa.addDevice(ALEXA_NAME);

  // Bind callback for when a command from Alexa is received
  alexa.onSetState(alexaCallback);
}

/***************************** Reconnect to MQTT *******************************/
void reconnect()
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
      Serial.printf(" connected to %s\n", MQTT_BROKER);
      // Once connected, publish an announcement...
      client.publish(logTopic, "ESP Connected to MQTT");
      // ... and resubscribe
      client.subscribe(setTravelTopic);
      client.subscribe(controlTopic);
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

/*********************************** Setup **************************************/
void setup()
{
  // Your setup code here
  // The library initializes the pins for you

  Serial.begin(SERIAL_BAUDRATE);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set FLASH button to pullup
  Serial.println(F(" *** Curtains Opener starting ***"));
  setup_wifi();
  setup_alexa();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

/******************************** Main Loop ************************************/
void loop()
{
  /************** MQTT Code **************/
  // Check frequently for MQTT connection, reconnect if necessary
  if (!client.connected())
  {
    reconnect();
  }
  // Called frequently to allow the client to process incoming messages
  // and maintain its connection to the server.
  client.loop();

  /************** Alexa Code **************/
  // fauxmoESP uses an async TCP server but a sync UDP server
  // Therefore, we have to manually poll for UDP packets
  alexa.handle();

  /************** Maintenance calls *******/
  // We need to call run() frequently for non-blocking stepper movement
  stepper.run();

  /************** Emergency Stop **********/
  if (stepper.stepsToGo() != 0 && digitalRead(BUTTON_PIN) == LOW)
  {
    stepper.stop();
    curtainsOpen = !curtainsOpen;
    snprintf(msg, MSG_BUFFER_SIZE, "Stepper reversed - Curtain are now %s\n", curtainsOpen ? "Opened" : "Closed");
    Serial.println(msg);
    client.publish(logTopic, msg);
  }
}
