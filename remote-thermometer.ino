#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "Timer.h"


// Replace with your network details
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mdnsname = "home-termometr";
const char* deviceName = "HOME";
const char* extDeviceName = "OUTDOOR";
const char* extAddr = "http://192.168.1.46";
boolean readExternal = true;

IPAddress ip(192,168,1,45);   
IPAddress gateway(192,168,1,100);   
IPAddress subnet(255,255,255,0);  
 
float humidityCalibration = 0.0;
float temperatureCalibration = 0.0;

#define DHTPIN            D3
#define DHTTYPE           DHT11
#define LED_PIN 2


SSD1306Wire  display(0x3c, D2, D1);
DHT dht(DHTPIN, DHTTYPE);

Timer timer;
WiFiServer server(80);

float humidity;
float temperature;
float heatIndex;

float extHumidity;
float extTemperature;
float extHeatIndex;

boolean wifiStatus = false;
boolean readUpdated = false;
boolean extReadUpdated = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  Serial.println();

  pinMode(LED_PIN, OUTPUT);


  Wire.begin();
  dht.begin();
  
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);


  delay(10);
  
  timer.every(2000, readTempHum, (void*)0);
  timer.every(2000, printTemp, (void*)0);
  timer.every(200, printTempDisplay, (void*)0);
  timer.every(1000, checkWifiConnection, (void*)0);
  timer.every(50, handleHttpClient, (void*)0);

  if (readExternal) {
      timer.every(2800, readExtTempHum, (void*)0);
  }

  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);

  server.begin();
  
  if (!MDNS.begin(mdnsname)) { // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
}

void loop() {
  timer.update();
}

void checkWifiConnection(void *context) {
  Serial.print("Check wifi: ");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No connection");

    wifiStatus = false;
  } else {
    Serial.print("Ok ");
    Serial.println(WiFi.localIP());
    
    wifiStatus = true;
  }
}

void readTempHum(void *context) {
  Serial.println("Read sensor");

  float readedHum = dht.readHumidity();
  float readedTemp = dht.readTemperature();

  if (!isnan(readedHum) && !isnan(readedTemp)) {
    humidity    = readedHum + humidityCalibration;
    temperature = readedTemp + temperatureCalibration;
    heatIndex    = dht.computeHeatIndex(temperature, humidity, false);
  }


  readUpdated = true;
}

void readExtTempHum(void *context) {

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Read external data");
 
    HTTPClient http;  //Declare an object of class HTTPClient
    http.setTimeout(2000);

 
    http.begin(extAddr);
    int httpCode = http.GET(); 
 
    if (httpCode > 0) { //Check the returning code
 
      String payload = http.getString();   //Get the request response payload

      //parse payload
      String readingsLine = payload.substring(0, payload.indexOf("\n"));
      readingsLine = readingsLine.substring(4);

      extTemperature = readingsLine.substring(0, readingsLine.indexOf(";")).toFloat();
      readingsLine = readingsLine.substring(readingsLine.indexOf(";")+1);

      extHumidity = readingsLine.substring(0, readingsLine.indexOf(";")).toFloat();
      readingsLine = readingsLine.substring(readingsLine.indexOf(";")+1);
      
      extHeatIndex = readingsLine.substring(0, readingsLine.indexOf(";")).toFloat();

      extReadUpdated = true; 
    }
 
    http.end();   //Close connection
  }
}


void printTemp(void *context) {
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" *C\t");
  Serial.print("Heat index: ");
  Serial.print(heatIndex);
  Serial.print(" *C ");
  Serial.println();
  
  Serial.print("EXT\tHumidity: ");
  Serial.print(extHumidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(extTemperature);
  Serial.print(" *C\t");
  Serial.print("Heat index: ");
  Serial.print(extHeatIndex);
  Serial.print(" *C ");
  Serial.println();
}

void printTempDisplay(void *context) {
  char str_temp[6];
    
  display.clear();

  // divider
  display.drawLine(display.getWidth()/2, 0, display.getWidth()/2, display.getHeight()-12);
  display.drawLine(0, display.getHeight()-12, display.getWidth(), display.getHeight()-12);

  // labels
  display.setFont(ArialMT_Plain_10);
  
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth()/4 , 0, deviceName);
  display.drawString((display.getWidth()/4) * 3 , 0, extDeviceName);

  // update indicators
  if (readUpdated) {
    readUpdated = false;
    display.fillCircle(2, 5, 2);
    
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
  }
  
  if (extReadUpdated) {
    extReadUpdated = false;
    display.fillCircle(display.getWidth()-2, 5, 2);
  } 

  // temps
  display.setFont(ArialMT_Plain_24);

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf(temperature, 2, 1, str_temp);
  display.drawString(display.getWidth()/2 - 8, 10, str_temp);
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  dtostrf(extTemperature, 2, 1, str_temp);
  display.drawString(display.getWidth()/2 + 8, 10, str_temp);

  // hum
  display.setFont(ArialMT_Plain_16);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  dtostrf(humidity, 2, 1, str_temp);
  sprintf(str_temp,"%s%%", str_temp);
  display.drawString(display.getWidth()/4, 35, str_temp);
  
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  dtostrf(extHumidity, 2, 1, str_temp);
  sprintf(str_temp,"%s%%", str_temp);
  display.drawString((display.getWidth()/4) * 3, 35, str_temp);

  // ip
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth()/2, display.getHeight() - 10, WiFi.localIP().toString());

  // wifi status
  display.drawCircle(display.getWidth()-6, display.getHeight() - 4, 3);

  if (wifiStatus) {
      display.fillCircle(display.getWidth()-6, display.getHeight() - 4, 3);
  }

  display.display();
}

void handleHttpClient(void *context) {
   // Listenning for new clients
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client");
    // bolean to locate when the http request ends
    boolean blank_line = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (c == '\n' && blank_line) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            // your actual web page that displays temperature and humidity
            client.print("<!--");
            client.print(temperature);
            client.print(";");
            client.print(humidity);
            client.print(";");
            client.print(heatIndex);
            client.print(";");
            client.println("-->");
            client.println("<html>");
            client.println("<head></head><body><h1>");
            client.println(deviceName);
            client.println("</h1><h3>Temperature: ");
            client.println(temperature);
            client.println("*C</h3><h3>Humidity: ");
            client.println(humidity);
            client.println("%</h3><h3>Heat index: ");
            client.println(heatIndex);
            client.println("*C</h3>");
            client.println("</body></html>");   
            break;
        }
        if (c == '\n') {
          // when starts reading a new line
          blank_line = true;
        }
        else if (c != '\r') {
          // when finds a character on the current line
          blank_line = false;
        }
      }
    }  
    // closing the client connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
}
