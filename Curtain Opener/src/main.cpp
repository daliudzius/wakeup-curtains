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

#define BUTTON_PIN 0 // This is the esp8266 FLASH button

// Steps to travel half curtain rod (~10 rotations needed)
int maxSteps = 4096 * 10;

const char *controlTopic = "curtain/control";
const char *setDistanceTopic = "curtain/setDistance";
const char *logTopic = "curtain/log";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

int lastSteps = 0;
int startTime = 0;
int endTime = 0;
bool done = false;

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

/************************* Where the magic happens *******************************/

// Is called upon incoming MQTT msg
void callback(char *topic, byte *payload, unsigned int length)
{

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, controlTopic) == 0) // Handle curtain/control requests
  {
    if ((char)payload[0] == '1') // '1' to CW move (OPEN CURTAINS)
    {
      snprintf(msg, MSG_BUFFER_SIZE, "Move requested: %d", maxSteps);
      client.publish(logTopic, msg);

      // activate if not moving
      if (stepper.stepsToGo() == 0)
      {
        stepper.move(maxSteps);
        Serial.printf("Stepper moving %d steps \n", maxSteps);
      }
    }
    else if ((char)payload[0] == '2') // '2' to CCW move (CLOSE CURTAINS)
    {
      snprintf(msg, MSG_BUFFER_SIZE, "Revers move requested: -%d", maxSteps);
      client.publish(logTopic, msg);

      // activate if not moving
      if (stepper.stepsToGo() == 0)
      {
        stepper.move(-1 * maxSteps);
        Serial.printf("Stepper moving -%d steps \n", maxSteps);
      }
    }
    else if ((char)payload[0] == '3') // '3' to move/reset to nearest rotation
    {
      // activate if not moving

      if (stepper.stepsToGo() == 0)
      {
        Serial.printf("Resetting stepper position.\n");
        stepper.moveTo(0);
      }
    }
  }
  else if (strcmp(topic, setDistanceTopic) == 0 && payload)
  {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    int newSteps = atoi((char *)payload);
    maxSteps = newSteps;
    Serial.printf("maxSteps set to %d\n", maxSteps);
  }
  else if (payload)
  {
    // If not activated, publish an ack
    client.publish(logTopic, "ack - but no move.");
  }
}

/*********************************** Setup **************************************/

void setup()
{
  // Your setup code here
  // The library initializes the pins for you

  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set FLASH button to pullup
  Serial.println(F(" *** Unistep2 test ***"));
  setup_wifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
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
      client.publish(logTopic, "ESP reconnected.");
      // ... and resubscribe
      client.subscribe(setDistanceTopic);
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

/******************************** Main Loop ************************************/

void loop()
{
  /******* MQTT Code *******/
  // Check frequently for MQTT connection, reconnect if necessary
  if (!client.connected())
  {
    reconnect();
  }

  /******* Maintenance calls *******/
  // We need to call run() frequently for non-blocking stepper movement
  stepper.run();
  // Called frequently to allow the client to process incoming messages
  // and maintain its connection to the server.
  client.loop();

  /******* Emergency Stop *******/
  if (stepper.stepsToGo() != 0 && digitalRead(BUTTON_PIN) == LOW)
  {
    stepper.stop();
    Serial.printf("Stopped at position: %d\n", stepper.currentPosition());
  }
}
