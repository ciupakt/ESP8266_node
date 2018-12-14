/*
 Basic ESP8266 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.

 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.
const char* ssid = "wireless";
const char* password = "12345678";
const char* mqtt_server = "192.168.1.10";

// Set this node's subscribe and publish topic prefix
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "power1-out"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "power1-in"

// Set MQTT client id
#define MY_MQTT_CLIENT_ID "power1"

//********************************************************
//  - GPIO
#define GPIO_INPUT D0
#define GPIO_LEDLINK D1

//********************************************************
// Configure
#define PULSE_FACTOR 1000       // Number of blinks per of your meter
#define SLEEP_MODE false        // Watt value can only be reported when sleep mode is false.
#define MAX_WATT 10000          // Max watt value to report. This filters outliers.

uint32_t SEND_FREQUENCY = 20000; // Minimum time between send (in milliseconds). We don't want to spam the gateway.
double ppwh = ((double)PULSE_FACTOR)/1000; // Pulses per watt hour
bool pcReceived = false;
volatile uint32_t pulseCount = 0;
volatile uint32_t lastBlink = 0;
volatile uint32_t watt = 0;
uint32_t oldPulseCount = 0;
uint32_t oldWatt = 0;
double oldkWh;
uint32_t lastSend;
//********************************************************

WiFiClient espClient;
PubSubClient client(espClient);

void onPulse(void);

void set_ledlink(bool onoff)
{
  if(onoff){
    digitalWrite(GPIO_LEDLINK, HIGH);
  }else{
    digitalWrite(GPIO_LEDLINK, LOW);
  }
}

void setup_hw(void)
{
  pinMode(D0, INPUT_PULLUP);
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);
  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);
  pinMode(D5, INPUT_PULLUP);
  pinMode(D6, INPUT_PULLUP);
  pinMode(D7, INPUT_PULLUP);

  pinMode(GPIO_INPUT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GPIO_INPUT), onPulse, FALLING);

  pinMode(GPIO_LEDLINK, OUTPUT);
  set_ledlink(false);
}


void callback(char* topic, byte* payload, u32_t length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (u32_t i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    
  } else
  if ((char)payload[0] == '0') {
    
  } else
  if ((char)payload[0] == '?') {
    char relay_status[2] = "0";

    client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX, relay_status);
  } 
}


void onPulse()
{
  uint32_t newBlink = millis();
  uint32_t interval = 0;

  pulseCount++;

  if (newBlink < lastBlink) { //counter overflow
    lastBlink = newBlink;
    return;
  }else{
    lastBlink = newBlink;
  }
  
  interval = newBlink-lastBlink;

  //Serial.print(">>newBlink:");
  //Serial.println(newBlink);

  //Serial.print(">>interval:");
  //Serial.println(interval);
    
  if (interval<100L) { // Sometimes we get interrupt on RISING
    return;
  }

  watt = (3600000.0 /interval) / ppwh;

  //Serial.print(">>watt:");
  //Serial.println(watt);

}

void calcutate_power(void)
{
  uint32_t now = millis();
  // Only send values at a maximum frequency or woken up from sleep
  bool sendTime = now - lastSend > SEND_FREQUENCY;
  
  if (pcReceived && (SLEEP_MODE || sendTime)) {
    // New watt value has been calculated
    if (!SLEEP_MODE && watt != oldWatt) {
      // Check that we don't get unreasonable large watt value.
      // could happen when long wraps or false interrupt triggered
      if (watt<((uint32_t)MAX_WATT)) {
        //send(wattMsg.set(watt));  // Send watt value to gw
      }
      Serial.print("Watt:");
      Serial.println(watt);
      oldWatt = watt;
    }

    // Pulse count value has changed
    if (pulseCount != oldPulseCount) {
      //send(pcMsg.set(pulseCount));  // Send pulse count value to gw
      double kWh = ((double)pulseCount/((double)PULSE_FACTOR));
      oldPulseCount = pulseCount;
      if (kWh != oldkWh) {
        //send(kWhMsg.set(kWh, 4));  // Send kWh value to gw
        oldkWh = kWh;
      }
    }
    lastSend = now;
  } else if (sendTime && !pcReceived) {
    // No pulse count value received. Try requesting it again
    //request(CHILD_ID, V_VAR1);
    lastSend=now;
  }

  if (SLEEP_MODE) {
    //sleep(SEND_FREQUENCY);
  }
}


void setup_wifi()
{

  bool blink_led = true;
  
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  set_ledlink(blink_led);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    blink_led = !blink_led;
    set_ledlink(blink_led);
  }
  set_ledlink(true);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  bool blink_led = false;
  set_ledlink(blink_led);
  
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (client.connect(MY_MQTT_CLIENT_ID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      String msg = String("Present ") + String(MY_MQTT_CLIENT_ID);
      client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX, msg.c_str());

      char relay_status[2] = "0";

      client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX, relay_status);
      
      // ... and resubscribe
      client.subscribe(MY_MQTT_SUBSCRIBE_TOPIC_PREFIX);
      set_ledlink(true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for(int i=0; i<25; i++){
        blink_led = !blink_led;
        set_ledlink(blink_led);
        delay(200);  
      }
    }
  }
}


void setup()
{

  setup_hw();

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  static long last_time = 0;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  lastSend=millis();

  long now = millis();
  if (now - last_time > 5) {
    last_time = now;
    //5ms tic
    calcutate_power();
  }

}
