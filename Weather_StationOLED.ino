//Uses a 0.96 Yellow-Blue OLED Display Driver IC: SSD1306
//Arduino Based Weather Station with Web UI - Aman Shaikh
#include "Arduino_LED_Matrix.h"
#include <stdint.h>
#include "DHT.h"
#include "RTC.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include "secret.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Crypto.h>
#include <SHA256.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <HttpClient.h>
#include <b64.h>
#include <BME280SpiSw.h>
#include "frames.h"

#define DHTTYPE DHT22
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 

//for bme280
#define CHIP_SELECT_PIN 6 //CSB
#define MOSI_PIN 5 //SDA
#define MISO_PIN 7 //SD0
#define SCK_PIN  8 //SCL
BME280SpiSw::Settings settings(CHIP_SELECT_PIN, MOSI_PIN, MISO_PIN, SCK_PIN);
BME280SpiSw bme(settings);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long delayTime = 60000;
auto timeZoneOffsetHours = 5.5;

int buzzerpin = 8;
int DHTPin = 2;
int sensorPinA = A0;
int sensorPinD = 3;
int sensorDataD;
int sensorDataA;
int rainpin = 9;

DHT dht(DHTPin, DHTTYPE);

int rain;
float Humidity;
float Temperature;
float Temp_Fahrenheit;
float hic;
int rainSensorValue;
float pres;
float alt;
float presi;

const char* ssid = SECRET_SSID;
const char* pass = SECRET_PASS;
const char* secret = SHARED_SECRET;
const char* host = IPadd; 
const int port = 3000;

File myFile;

ArduinoLEDMatrix matrix;

int wifiStatus = WL_IDLE_STATUS;
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp);
long rssi;

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  //signal strength: 
  rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void connectToWiFi(){
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Warning: Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("INFO: Firmware update is available, Please update!");
  }

  while (wifiStatus != WL_CONNECTED) {
    Serial.print("INFO: Attempting to connect to SSID: ");
    Serial.println(ssid);
    wifiStatus = WiFi.begin(ssid, pass);
    delay(10000);
  }

  Serial.println("INFO: Connected to WiFi");
  printWifiStatus();
}

bool checkForRain() {
  rainSensorValue = digitalRead(rainpin);
  if(rainSensorValue < 1)
  {
  return 1;
  }
  else 
  {
    return 0;
  }
   // Shows 1 if not raining, 0 if raining.
}

// Function to read air quality index
int readAirQualityIndex() {
  int sensorDataA = analogRead(sensorPinA);
  digitalWrite(13, sensorDataA > 400 ? HIGH : LOW);
  return sensorDataA;
}

// Function to display air quality information
void displayAirQuality(int airQualityIndex) {
  Serial.print(F("Air Quality Index: "));
  Serial.print(airQualityIndex, DEC);
  Serial.print(" PPM");
  if (airQualityIndex < 500) {
    Serial.println(", Fresh Air");
  } else if (airQualityIndex > 500 && airQualityIndex <= 1000) {
    Serial.println(", Poor Air");
  } else if (airQualityIndex > 1000) {
    Serial.println(", Very Poor Air");
  }
}

// Function to check for gas leak
bool checkForGasLeak() {
  return digitalRead(sensorPinD) == LOW;
}

void displaySensorData() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Display humidity
  display.setCursor(0, 0);
  display.print("Humidity:");
  display.print(Humidity);
  display.println("%");

  // Display temperature
  display.print("Temp:");
  display.print(Temperature);
  display.print((char)247); // Degree symbol
  display.print("C (");
  display.print(Temp_Fahrenheit);
  display.println("F)");

  // Display heat index
  display.print("Heat Index:");
  display.print(hic);
  display.print((char)247);
  display.println("C");


  // Air quality section
  display.print("Air Quality:");
  int airQuality = readAirQualityIndex();
  display.print(airQuality);

  if (airQuality <= 200) {
    display.println(" Good");
  } else if (airQuality <= 500) {
    display.println(" Mid");
  } else {
    display.println(" Poor");
  }

  // Display rain detection
  display.print("Raining:");
  display.println(checkForRain() ? "Yes" : "No");

  //Display Altitude
  display.print("Altitude: ");
  display.print(alt);
  display.println("m");

 //Display Atmospheric Pressure
  display.print("Pressure: ");
  display.print(presi);
  display.println("hPa");

  display.display();
}


void setup() {
  Serial.begin(9600);
  Serial.println(F("INFO: Booting and loading modules!"));
  //Initializing our OLED module
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Warning: OLED Driver SSD1306 Allocation Failed!"));
    for(;;); // Don't proceed, loop forever
  }

while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }

  //pinMode(buzzerpin, OUTPUT);
  pinMode(sensorPinD, INPUT);
  pinMode(DHTPin, INPUT);
  pinMode(sensorPinA, INPUT);
  pinMode(rainpin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  dht.begin();

  connectToWiFi();
  RTC.begin();
  Serial.println("\nINFO: Starting connection to server...");
  timeClient.begin();
  timeClient.update();

  // Get the current date and time from an NTP server and convert it to unix
  auto unixTime = timeClient.getEpochTime() + (timeZoneOffsetHours * 3600);
  Serial.print("Unix time = ");
  Serial.println(unixTime);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);

  RTCTime currentTime;
  RTC.getTime(currentTime); 
  Serial.println("The RTC was just set to: " + String(currentTime));

  display.display();
  delay(2000); 


//Initializing our SD Card
  Serial.print("INFO: Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("Warning: Initialization failed!");
    while (1);
  }
  Serial.println("INFO: SD Initialization done.");

  matrix.loadSequence(frames);
  matrix.begin();
  matrix.autoscroll(300);
  matrix.play(true);
  delay(100);

}

void loop() {

  
  if (checkForRain()) {
    Serial.println("Rain detected.");
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
  
   float temp(NAN), hum(NAN);

   BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
   BME280::PresUnit presUnit(BME280::PresUnit_Pa);
   bme.read(pres, temp, hum, tempUnit, presUnit);
  presi = pres/100; //Display pressure in hPa
  alt = 44330 * ( 1 - pow(presi/1013.25, 1/5.255) );

  Humidity = dht.readHumidity();
  Temperature = dht.readTemperature();
  Temp_Fahrenheit = dht.readTemperature(true);
  hic = dht.computeHeatIndex(Temperature, Humidity, false);

  if (isnan(Humidity) || isnan(Temperature) || isnan(Temp_Fahrenheit)) {
    Serial.println(F("Warning: Unable to find DHT Sensor. Check Connection!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(Humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(Temperature);
  Serial.print(F("°C | "));
  Serial.print(Temp_Fahrenheit);
  Serial.print(F("°F Heat index: "));
  Serial.print(hic);
  Serial.println(F("°C "));

  int airQualityIndex = readAirQualityIndex();
  displayAirQuality(airQualityIndex);

  if (checkForGasLeak()) {
    Serial.println("Gas Leak Found");
    Serial.print("AD: ");
    Serial.print(airQualityIndex * 3.3 / 1024);
    Serial.println(" V");
  } else {
    Serial.println("No Gas Leakage");
  }
  
  //Was for debugging the sensor
/*  Serial.print(rainSensorValue);
  Serial.println(F(" Rain ")); */

  displaySensorData();

  RTCTime currentTime;
  RTC.getTime(currentTime);
  myFile = SD.open("Logging.txt", FILE_WRITE);
   if (myFile) {
    // Write timestamp to the file on first line
    myFile.print(currentTime.getDayOfMonth(), DEC);
    myFile.print("-");
    myFile.print(Month2int(currentTime.getMonth()));
    myFile.print("-");
    myFile.print(currentTime.getYear(), DEC);
    myFile.print(" ");
    myFile.print(currentTime.getHour(), DEC);
    myFile.print(":");
    myFile.print(currentTime.getMinutes(), DEC);
    myFile.print(":");
    myFile.print(currentTime.getSeconds(), DEC);

    myFile.print(" | Temperature: ");
    myFile.print(Temperature);
    myFile.print(", Humidity: ");
    myFile.print(Humidity);
    myFile.print(", AQI: ");
    myFile.print(airQualityIndex);
    myFile.print(", HI: ");
    myFile.print(hic);
    myFile.print(", ALT: ");
    myFile.print(alt);
    myFile.print(", Pressure: ");
    myFile.print(pres);
    myFile.print(", Raining: ");
    myFile.print(checkForRain() ? "Yes" : "No");
    myFile.print(", WiFI Strength (dBm): ");
    myFile.println(rssi);
    myFile.close();
    Serial.println("Data saved to SD card");
  } else {
    Serial.println("Error opening file");
  }

RTC.getTime(currentTime);
 
WiFiClient client;


if (client.connect(host, port)) {
    Serial.println("Connected to server");

    // Create a JSON object
StaticJsonDocument<200> jsonDoc;
// Create a JSON object in the buffer
JsonObject jsonData = jsonDoc.to<JsonObject>();

// Add the data to the JSON object
jsonData["time"] = String(currentTime);
jsonData["temperature"] = Temperature;
jsonData["humidity"] = Humidity;
jsonData["aqi"] = airQualityIndex;
jsonData["hi"] = hic;
jsonData["alt"] = alt;
jsonData["pres"] = pres;
jsonData["raining"] = checkForRain() ? "Yes" : "No";
jsonData["wifi_strength"] = rssi;

    // Convert the JSON object to a string
String jsonString;
serializeJson(jsonDoc, jsonString);

Serial.println("JSON data:");
serializeJson(jsonData, Serial);
Serial.print("Content-Length: ");
Serial.println(measureJson(jsonData));

    // Send the JSON string to the server
    client.print("POST /data HTTP/1.1\r\n");
    client.print("Host: " + String(host) + "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(jsonString.length()) + "\r\n");
    client.print("\r\n");
    client.print(jsonString);

    // Read the response from the server (Debugging ; will remove l8r)
    Serial.println("Reading response");
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }

    // Close the connection
    client.stop();
  } else {
    Serial.println("Connection to server failed");
  }

  delay(delayTime);
}


