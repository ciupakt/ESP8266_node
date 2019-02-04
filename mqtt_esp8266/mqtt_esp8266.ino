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
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "socket2-out"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "socket2-in"

// Set MQTT client id
#define MY_MQTT_CLIENT_ID "socket2"

//  - Melink
//GPIO3 - RESET
//GPIO4 - RELAY
//GPIO5 - ONOFF
//GPIO12 - LEDLINK
//GPIO13 - LEDRELAY
//#define GPIO_RESET 3
#define GPIO_RELAY 4
#define GPIO_ONOFF 5
#define GPIO_LEDLINK 12
#define GPIO_LEDRELAY 13

static bool relay_on_off = false;

void set_relay(bool onoff)
{
  relay_on_off = onoff;

  Serial.print("onoff=");
  Serial.println(onoff);
    
  if(onoff){
    digitalWrite(GPIO_RELAY, HIGH);
    digitalWrite(GPIO_LEDRELAY, HIGH);
  }else{
    digitalWrite(GPIO_RELAY, LOW);
    digitalWrite(GPIO_LEDRELAY, LOW);
  }
}

bool get_relay(void)
{
  return relay_on_off;
}

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
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  //pinMode(4, INPUT_PULLUP);
  //pinMode(5, INPUT_PULLUP);
  //pinMode(12, INPUT_PULLUP);
  //pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);

  pinMode(GPIO_ONOFF, INPUT_PULLUP);
    
  pinMode(GPIO_RELAY, OUTPUT); 
  pinMode(GPIO_LEDRELAY, OUTPUT);
  set_relay(false);
  
  pinMode(GPIO_LEDLINK, OUTPUT);
  set_ledlink(false);

}


WiFiClient espClient;
PubSubClient client(espClient);


void setup_wifi() {

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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    set_relay(true);
  } else
  if ((char)payload[0] == '0') {
    set_relay(false);
  } else
  if ((char)payload[0] == '?') {
    char relay_status[2] = "0";
    relay_status[0] = get_relay()+'0';
    client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX, relay_status);
  } 
}

void reconnect() {
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
      relay_status[0] = get_relay()+'0';
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

void setup() {

  setup_hw();

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void read_buttons(void)
{
  #define DEBOUNCE_COUNTER 10
  
  static long timer_onoff_button = 0;
  
  // read the state of the switch into a local variable:
  int onoff_read = digitalRead(GPIO_ONOFF);
  if(onoff_read == LOW){
    if(timer_onoff_button <= DEBOUNCE_COUNTER){
      timer_onoff_button++;
    }
  }else{
    timer_onoff_button = 0;
  }
 
  if(timer_onoff_button == DEBOUNCE_COUNTER){
    set_relay(!get_relay());

    char relay_status[2] = "0";
    relay_status[0] = get_relay()+'0';
    client.publish(MY_MQTT_PUBLISH_TOPIC_PREFIX, relay_status);  
  }

}

void loop() {
  static long last_time = 0;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - last_time > 5) {
    last_time = now;
    //5ms tic
    read_buttons();
  }
}
