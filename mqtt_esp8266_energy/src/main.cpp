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
#define MY_MQTT_PUBLISH_TOPIC_PREFIX_WATT "power1-out-watt"
#define MY_MQTT_PUBLISH_TOPIC_PREFIX_KWH "power1-out-kWh"
#define MY_MQTT_PUBLISH_TOPIC_PREFIX_PULSE "power1-out-pulse"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX_WATT "power1-in-watt"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX_KWH "power1-in-kWh"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX_PULSE "power1-in-pulse"

// Set MQTT client id
#define MY_MQTT_CLIENT_ID "power1"

//********************************************************
//  - GPIO
#define GPIO_INPUT D1
#define GPIO_LEDLINK D0

//********************************************************
// Configure
#define PULSE_FACTOR 1000       // Number of blinks per of your meter
#define SLEEP_MODE false        // Watt value can only be reported when sleep mode is false.
#define MAX_WATT 10000          // Max watt value to report. This filters outliers.
#define MAX_DIFF_WATT 10

uint32_t SEND_FREQUENCY = 20000; // Minimum time between send (in milliseconds). We don't want to spam the gateway.
double ppwh = ((double)PULSE_FACTOR)/1000; // Pulses per watt hour
bool pcReceived = false;

volatile uint32_t last_blink = 0;
volatile uint32_t watt = 0;
uint32_t old_watt = 0;
volatile uint32_t pulse_count = 0;
uint32_t old_pulse_count = 0;
double old_kwh = 0;
uint32_t last_send_watt = 0;
uint32_t last_send_kwh = 0;
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

  String topic_str = String(topic);
  // Switch on the LED if an 1 was received as first character
  if (topic_str == MY_MQTT_SUBSCRIBE_TOPIC_PREFIX_PULSE) {
    String pulse_str = String((char*)payload);
    pulse_count = pulse_str.toInt();

  }
}

void onPulse()
{
  uint32_t new_blink = millis();
  uint32_t interval = 0;

  pulse_count++;

  if (new_blink < last_blink) { //counter overflow
    last_blink = new_blink;
    return;
  }else{
    last_blink = new_blink;
  }
  
  interval = new_blink-last_blink;

  Serial.print(">>newBlink:");
  Serial.println(new_blink);

  Serial.print(">>interval:");
  Serial.println(interval);

  watt = (3600000.0 /interval) / ppwh;

  Serial.print(">>watt:");
  Serial.println(watt);

}

void calcutate_power(void)
{
  uint32_t now = millis();
  // Only send values at a maximum frequency or woken up from sleep
  bool send_time_kwh = (now - last_send_kwh) > SEND_FREQUENCY;

  if (send_time_kwh) {
    last_send_kwh = now;
    if (pulse_count != old_pulse_count) {
      old_pulse_count = pulse_count;
      double kwh = ((double)pulse_count/((double)PULSE_FACTOR));
      
      if (kwh != old_kwh) {
        old_kwh = kwh;
        String kwh_str = String(kwh, 3);
        client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX_KWH, kwh_str.c_str());
      }
    }
  }

  bool send_time_watt = (now - last_send_watt) > SEND_FREQUENCY;

  uint32_t diff = 0;
  if(watt > old_watt){
    diff = watt - old_watt;
  }else{
    diff = old_watt - watt;
  }

  if ((diff > MAX_DIFF_WATT) || (send_time_watt)) {
    if (send_time_watt) {
      last_send_watt = now;
    }

    old_watt = watt;

    Serial.print("Watt:");
    Serial.println(watt);

    if (watt<((uint32_t)MAX_WATT)) {
      String watt_str = String(watt, 3);
      client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX_WATT, watt_str.c_str());
    }
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
      
      // ... and resubscribe
      client.subscribe(MY_MQTT_SUBSCRIBE_TOPIC_PREFIX_PULSE);
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

  long now = millis();
  if (now - last_time > 5) {
    last_time = now;
    //5ms tic
    calcutate_power();
  }
}
