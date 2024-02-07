#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

const char* ssid = "UW MPSK";
const char* password = "~iKj4Ydp(a"; // Replace with your network password
#define DATABASE_URL "https://esp32-s3-test-dd4a4-default-rtdb.firebaseio.com" // Replace with your database URL
#define API_KEY "AIzaSyDsiUgppnJYo0ZmbuDvORBMpHOQUSbIkUQ" // Replace with your API key
#define STAGE_INTERVAL 6000 // 12 seconds each stage
#define MAX_WIFI_RETRIES 5 // Maximum number of WiFi connection retries

int uploadInterval = 4000; //  seconds each upload

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

// HC-SR04 Pins
const int trigPin = D0;
const int echoPin = D1;

// Define sound speed in cm/usec
const float soundSpeed = 0.034;

// Function prototypes
float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  unsigned long startTime = millis();

  // Now, turn on WiFi and keep measuring
  Serial.println("Turning on WiFi and measuring");
  connectToWiFi();
}

void loop() {
  unsigned long startTime = millis();
  bool objectDetected = false;

  // 检测30秒内是否有物体在50cm范围内
  while (millis() - startTime < 10000) { // 30秒的检测窗口
    float distance = measureDistance();
    if (distance <= 50.0) {
      objectDetected = true;
      break; // 跳出循环
    }
    delay(100); // 间隔100毫秒进行下一次测量
  }

  if (objectDetected) {
    Serial.println("Object detected, initializing Firebase...");
    initFirebase(); // 初始化Firebase

    Serial.println("Sending data to Firebase...");
    startTime = millis();
    while (millis() - startTime < 10000) { // 持续30秒发送数据
      float currentDistance = measureDistance();
      sendDataToFirebase(currentDistance);
      delay(100); // 测量间隔
    }
  } else {
    Serial.println("No object detected, going to deep sleep for 90 seconds...");

    WiFi.disconnect();
    esp_sleep_enable_timer_wakeup(STAGE_INTERVAL * 12000); // in microseconds
    esp_deep_sleep_start();
    }
}


float measureDistance()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi()
{
  // Print the device's MAC address.
  Serial.println(WiFi.macAddress());
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES){
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance){
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > uploadInterval || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // Write an Float number on the database path test/float
    if (Firebase.RTDB.pushFloat(&fbdo, "test/distance", distance)){
      Serial.println("PASSED");
      Serial.print("PATH: ");
      Serial.println(fbdo.dataPath());
      Serial.print("TYPE: " );
      Serial.println(fbdo.dataType());
    } else {
      Serial.println("FAILED");
      Serial.print("REASON: ");
      Serial.println(fbdo.errorReason());
    }
    count++;
  }
}

