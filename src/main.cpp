// ALL CODE IN THIS FILE WAS WRITTEN BY ALEXANDRE DUFOREST
#include <Arduino.h>
#include <TFT_eSPI.h> // Include the TFT library
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi Credentials
const char* ssid = "AlexHotspot";     // Network SSID (name)
const char* password = "password";    // Network password

// AWS API details
const char* serverName = "http://3.93.191.123:5000/light-sensor";

// Pin Definitions
const int ledPin = 26;
const int lightSensorPin = 33;
const int touchPin = 2;

// LED State Variables
bool ledState = true;
volatile bool buttonPressed = false;

// Variables for light sensor
int minLight = 4095;                 // Minimum light value for calibration
int maxLight = 0;                    // Maximum light value for calibration
const unsigned long calibrationTime = 10000; // Calibration duration
unsigned long startTime;

// TFT_eSPI Initialization
TFT_eSPI tft = TFT_eSPI();

// Graph Variables
const int graphWidth = 240;  
const int graphHeight = 130;
const int graphX = 0;   
const int graphY = 0;

int brightnessBuffer[240];
int bufferIndex = 0;

unsigned long lastGraphUpdate = 0;
const unsigned long graphUpdateInterval = 100; // Update graph every 100ms

// Data Sending Interval Variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // Send data every 1000ms

// Function Prototypes
void IRAM_ATTR onTouchDetected();
void drawGraph();
void drawGraphAxes();
void sendBrightnessData(int ledBrightness);

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Connect to WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  // Initialize pins
  pinMode(ledPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);

  // Initialize LED PWM channel
  ledcSetup(0, 5000, 8);
  ledcAttachPin(ledPin, 0);

  // Initialize the touch button
  touchAttachInterrupt(touchPin, onTouchDetected, 40);

  // Initialize TFT display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);

  // Initialize brightness buffer
  for (int i = 0; i < graphWidth; i++) {
    brightnessBuffer[i] = 0;
  }

  drawGraphAxes();

  // Start calibration
  startTime = millis();
}

void loop() {
  unsigned long currentTime = millis();

  // Handle button press
  if (buttonPressed) {
    ledState = !ledState;
    Serial.println(ledState ? "LED Turned ON" : "LED Turned OFF");
    buttonPressed = false;
  }

  // Calibration phase
  if ((currentTime - startTime) <= calibrationTime) {
    int lightValue = analogRead(lightSensorPin);
    if (lightValue < minLight) {
      minLight = lightValue;
      Serial.print("New minLight: ");
      Serial.println(minLight);
    }
    if (lightValue > maxLight) {
      maxLight = lightValue;
      Serial.print("New maxLight: ");
      Serial.println(maxLight);
    }
    if ((currentTime / 500) % 2 == 0) {
      Serial.println("Calibrating...");
    }
  } else {
    // Operational phase
    if (ledState) {
      int lightValue = analogRead(lightSensorPin);

      // Map the light sensor value to LED brightness
      int ledBrightness = map(lightValue, minLight, maxLight, 255, 0);
      ledBrightness = constrain(ledBrightness, 0, 255);

      // Write PWM value to LED
      ledcWrite(0, ledBrightness);

      // Print ambient light and LED brightness
      Serial.print("Ambient Light: ");
      Serial.print(lightValue);
      Serial.print(" | LED Brightness: ");
      Serial.println(ledBrightness);

      // Update brightness buffer
      brightnessBuffer[bufferIndex] = ledBrightness;
      bufferIndex = (bufferIndex + 1) % graphWidth;

      // Update the graph at set intervals
      if (currentTime - lastGraphUpdate >= graphUpdateInterval) {
        drawGraph();
        lastGraphUpdate = currentTime;
      }

      // Send data to AWS at defined intervals
      if (currentTime - lastSendTime >= sendInterval) {
        sendBrightnessData(ledBrightness);
        lastSendTime = currentTime;
      }
    } else {
      // Turn off the LED
      ledcWrite(0, 0);
      Serial.println("LED is OFF");
    }
  }

  delay(10); // delay to stabilize readings
}

void IRAM_ATTR onTouchDetected() {
  buttonPressed = true;
}

// draw the graph axes and labels
void drawGraphAxes() {
  tft.drawRect(graphX, graphY, graphWidth, graphHeight, TFT_WHITE);

  // Y-Axis Labels (0 and 255)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(graphX + 5, graphY - 15);
  tft.print("255");
  tft.setCursor(graphX + 5, graphY + graphHeight - 10);
  tft.print("0");

  // X-Axis Label
  tft.setCursor(graphX + graphWidth / 2 - 20, graphY + graphHeight + 5);
  tft.print("Time");

  // Title for the graph
  tft.setTextSize(1);
  tft.setCursor(graphX + graphWidth / 2 - 30, graphY - 25);
  tft.print("Brightness");
}

// draw the brightness graph with fixed scale (0-255)
void drawGraph() {
  // Clear the graph area
  tft.fillRect(graphX + 1, graphY + 1, graphWidth - 2, graphHeight - 2, TFT_BLACK);

  // Draw graph borders
  tft.drawRect(graphX, graphY, graphWidth, graphHeight, TFT_WHITE);

  // Plot all brightness values
  for (int i = 0; i < graphWidth - 1; i++) {
    int currentValue = brightnessBuffer[(bufferIndex + i) % graphWidth];
    int nextValue = brightnessBuffer[(bufferIndex + i + 1) % graphWidth];

    int y1 = graphY + graphHeight - map(currentValue, 0, 255, 0, graphHeight);
    int y2 = graphY + graphHeight - map(nextValue, 0, 255, 0, graphHeight);

    tft.drawLine(graphX + i, y1, graphX + i + 1, y2, TFT_GREEN);
  }
}

// Function to send light sensor data to AWS
void sendBrightnessData(int ledBrightness) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Construct the URL with query parameters
    String url = String(serverName) + "?light_value=" + String(ledBrightness);
    
    http.begin(url); // Specify the URL
    http.addHeader("Content-Type", "application/json");
    
    // Prepare JSON payload
    String payload = "{\"light_value\": " + String(ledBrightness) + "}";
    
    // Send the POST request
    int httpResponseCode = http.POST(payload);
    
    // Handle the response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST Request: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}
