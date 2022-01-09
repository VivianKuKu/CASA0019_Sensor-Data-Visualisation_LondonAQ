  #include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include "secret.h"
#include <PubSubClient.h>

#include "Wire.h"
#include "Adafruit_PWMServoDriver.h"
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#define MIN_PULSE_WIDTH 600
#define MAX_PULSE_WIDTH 2600
#define FREQUENCY 50

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
String apikey = SECRET_API;
const char* mqtt_server = SECRET_MQTTURL;
const char* mqttuser = SECRET_MQTTUSER;
const char* mqttpass = SECRET_MQTTPASS;

String Londonlat = "51.531307";
String Londonlon = "-0.117306";
//String HKlat = "22.302711";
//String HKlon = "114.177216";
String Jinanlat = "36.6667";
String Jinanlon = "116.9833";
String lat = "51.531307";
String lon = "-0.117306";

int AQIVal;
double NO2Val;
double O3Val;
double pm10Val;
double pm25Val;

// THE DEFAULT TIMER IS SET TO 10 SECONDS FOR TESTING PURPOSES
// For a final application, check the API call limits per hour/minute to avoid getting blocked/banned
unsigned long lastTime = 0;
// Timer set to 10 minutes
unsigned long timerDelay = 600000;
int Firsttime = 0;

String jsonBuffer;

#include <Servo.h>
Servo AQIservo;
Servo NO2servo;
Servo O3servo;
Servo pm10servo;
Servo pm25servo;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  Serial.begin(9600);

  //Set the motors
  pwm.begin();
  pwm.setPWMFreq(FREQUENCY);

  pwm.setPWM(0, 0, pulseWidth(0)); //AQI
  pwm.setPWM(1, 0, pulseWidth(0)); //NO2
  pwm.setPWM(2, 0, pulseWidth(0)); //O3
  pwm.setPWM(3, 0, pulseWidth(0)); //PM10
  pwm.setPWM(4, 0, pulseWidth(0)); //PM2.5

  //connect to wifi
  startWifi();

  //Connect to MQTT
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
  Serial.println("Timer set to 10 seconds (timerDelay variable), it will take 10 seconds before publishing the first reading.");
}

int pulseWidth(int angle) {
  int pulse_wide, analog_value;
  pulse_wide = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);
  return analog_value;
}

void loop() {
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
    if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Send an HTTP GET request
  if ((millis() - lastTime) > timerDelay || Firsttime == 0) {
    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      Firsttime == 1;
      //      String serverPath = "http://api.openweathermap.org/data/2.5/air_pollution?lat=51.531307&lon=-0.117306&appid=61b50eaef776915a03310a03116c54df";
      String serverPath = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + lat + "&lon=" + lon + "&appid=" + apikey;
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      JSONVar myObject = JSON.parse(jsonBuffer);

      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }

      //extract data from JSON object
      AQIVal = myObject["list"][0]["main"]["aqi"];
      NO2Val = myObject["list"][0]["components"]["no2"];
      O3Val = myObject["list"][0]["components"]["o3"];
      pm10Val = myObject["list"][0]["components"]["pm10"];
      pm25Val = myObject["list"][0]["components"]["pm2_5"];

      Serial.print("JSON object = ");
      Serial.println(myObject);
      Serial.print("AQI: ");
      Serial.println(AQIVal);
      Serial.print("NO2: ");
      Serial.println(NO2Val);
      Serial.print("O3: ");
      Serial.println(O3Val);
      Serial.print("PM10: ");
      Serial.println(pm10Val);
      Serial.print("PM2.5: ");
      Serial.println(pm25Val);

//      sendMQTT();

      //convert values into motor angles
      int AQIangle = map(round(AQIVal), 1, 5, 165, 15);
      int NO2angle = map(constrain(round(NO2Val), 0, 400), 0, 400, 180, 0);
      int O3angle = map(constrain(round(O3Val), 0, 240), 0, 240, 180, 0);
      int pm10angle = map(constrain(round(pm10Val), 0, 180), 0, 180, 180, 0);
      int pm25angle = map(constrain(round(pm25Val), 0, 110), 0, 110, 180, 0);
      

      //turn the motor
      //AQI
      pwm.setPWM(0, 0, pulseWidth(AQIangle));
      Serial.print("AQI Angle: ");
      Serial.println(AQIangle);
      //NO2
      pwm.setPWM(1, 0, pulseWidth(NO2angle));
      Serial.print("NO2 Angle: ");
      Serial.println(NO2angle);
      //O3
      pwm.setPWM(2, 0, pulseWidth(O3angle));
      Serial.print("O3 Angle: ");
      Serial.println(O3angle);
      //pm10
      pwm.setPWM(3, 0, pulseWidth(pm10angle));
      Serial.print("pm10 Angle: ");
      Serial.println(pm10angle);
      //pm25
      pwm.setPWM(4, 0, pulseWidth(pm25angle));
      Serial.print("pm25 Angle: ");
      Serial.println(pm25angle);
     

    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}

void startWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payloadhttp = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payloadhttp = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payloadhttp;
}

//void sendMQTT() {
//
//  if (!client.connected()) {
//    reconnect();
//  }
//  client.loop();
//  snprintf (msg, 50, "%i", AQIVal);
//  Serial.print("Publish message: ");
//  Serial.println(msg);
//  client.publish("student/CASA0019/group1/API/AQI", msg);
//  
//  snprintf (msg, 50, "%.2f", NO2Val);
//  Serial.print("Publish message: ");
//  Serial.println(msg);
//  client.publish("student/CASA0019/group1/API/NO2", msg);
//  
//  snprintf (msg, 50, "%.2f", O3Val);
//  Serial.print("Publish message: ");
//  Serial.println(msg);
//  client.publish("student/CASA0019/group1/API/O3", msg);
//  
//  snprintf (msg, 50, "%.2f", pm10Val);
//  Serial.print("Publish message: ");
//  Serial.println(msg);
//  client.publish("student/CASA0019/group1/API/pm10", msg);
//  
//  snprintf (msg, 50, "%.2f", pm25Val);
//  Serial.print("Publish message: ");
//  Serial.println(msg);
//  client.publish("student/CASA0019/group1/API/pm25", msg);
//}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '1') {
    Serial.println("Payload: TowerBridge_Clicked");
    lat = Londonlat;
    lon = Londonlon;
  } else if ((char)payload[0] == '2') {
    Serial.println("Payload: LondonEye_Clicked");
    lat = Jinanlat;
    lon = Jinanlon;
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("student/CASA0019/group1/App");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(100);
    }
  }
}
