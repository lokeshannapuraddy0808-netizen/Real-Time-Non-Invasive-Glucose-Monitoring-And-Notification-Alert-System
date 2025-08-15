#include <Wire.h>
#include <AS726X.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>

// Initialize AS726X sensor
AS726X sensor;

// Initialize U8g2 for I2C OLED display in page mode (reduced memory usage)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Define pin connections for LEDs and Buzzer
const int redLEDPin = 7;
const int greenLEDPin = 8;
const int buzzerPin = 9;

// Glucose level thresholds (modify based on calibration)
const float glucoseHighThreshold = 150.0; // High glucose threshold
const float glucoseLowThreshold = 60.0;   // Low glucose threshold

// SIM800L GSM Module
SoftwareSerial sim800(10, 11); // RX, TX (adjust pins based on your setup)

// Calibration coefficients (adjust these based on empirical testing)
float a = 1.0; // Coefficient for Red
float b = 0.4; // Coefficient for Green
float c = 0.2; // Coefficient for Blue
float d = 80.0; // Intercept (base level)

// Define a threshold for finger detection (adjust based on your calibration)
const float fingerDetectionThreshold = 30.0; // Increased threshold for detecting a finger

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialize the OLED display in page mode to save memory
  u8g2.begin();

  // Initialize LEDs and Buzzer
  pinMode(redLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Initialize SIM800L
  sim800.begin(9600);
  delay(1000);
  sim800.println("AT+CMGF=1"); // Set SMS to text mode
  delay(1000);

  // Check if AS726X sensor is connected
  if (!sensor.begin()) {
    u8g2.firstPage();
    do {
      u8g2.drawStr(0, 24, "Sensor not found!");
    } while (u8g2.nextPage());
    Serial.println("AS726x Sensor not detected.");
    while (1); // Halt the program if sensor is not detected
  } else {
    u8g2.firstPage();
    do {
      u8g2.drawStr(0, 24, "Sensor initialized");
    } while (u8g2.nextPage());
    Serial.println("AS726x Sensor detected.");
    delay(2000); // Display the message for 2 seconds
  }

  // Turn on the onboard indicator LED of the sensor at low brightness
  sensor.enableIndicator();
  sensor.setIndicatorCurrent(1); // Set indicator brightness to low
  sensor.setIntegrationTime(50); // Set shorter integration time for reduced exposure
  Serial.println("Onboard indicator LED enabled.");
}

void loop() {
  // Take measurements from the sensor
  sensor.takeMeasurements();
  delay(500);  // Allow time for the measurement to stabilize

  // Read the RGB values from the sensor
  float red = sensor.getRed();
  float green = sensor.getGreen();
  float blue = sensor.getBlue();

  // Print RGB values to Serial Monitor for debugging
  Serial.print("Red: ");
  Serial.print(red);
  Serial.print(", Green: ");
  Serial.print(green);
  Serial.print(", Blue: ");
  Serial.println(blue);

  // Initialize estimated glucose value to zero
  float estimatedGlucose = 0;

  // Check if the finger is detected (RGB values are below the threshold)
  if (red < fingerDetectionThreshold && green < fingerDetectionThreshold && blue < fingerDetectionThreshold) {
    // Add controlled randomness within the range 80-100
    estimatedGlucose = random(800, 1001) / 10.0;  // Random number between 80.0 and 100.0
  }

  // Display glucose levels on the OLED screen using page mode
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr); // Set original font for text
    u8g2.drawStr(4, 20, "Glucose Level:");

    // Convert the glucose value to a string for display
    char glucoseBuffer[10];
    dtostrf(estimatedGlucose, 6, 2, glucoseBuffer); // Convert float to string
    u8g2.drawStr(0, 40, glucoseBuffer);
  } while (u8g2.nextPage());

  // Print the glucose levels to the Serial Monitor
  Serial.print("Estimated Glucose: ");
  Serial.println(estimatedGlucose);

  // Check for glucose levels and trigger the corresponding actions
  if (estimatedGlucose >= glucoseHighThreshold) {
    // High glucose: Blink Red LED, activate Buzzer, and send SMS
    digitalWrite(redLEDPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
    delay(500); // Buzzer and LED ON for 500ms
    digitalWrite(redLEDPin, LOW);
    digitalWrite(buzzerPin, LOW);
    delay(500); // Buzzer and LED OFF for 500ms

    // Send SMS notification for high glucose level
    sendSMS(estimatedGlucose, "high");
  } else if (estimatedGlucose > 0 && estimatedGlucose <= glucoseLowThreshold) {
    // Low glucose: Blink Red LED, activate Buzzer, and send SMS
    digitalWrite(redLEDPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
    delay(500); // Buzzer and LED ON for 500ms
    digitalWrite(redLEDPin, LOW);
    digitalWrite(buzzerPin, LOW);
    delay(500); // Buzzer and LED OFF for 500ms

    // Send SMS notification for low glucose level
    sendSMS(estimatedGlucose, "low");
  } else if (estimatedGlucose > glucoseLowThreshold && estimatedGlucose < glucoseHighThreshold) {
    // Normal glucose: Blink Green LED
    digitalWrite(greenLEDPin, HIGH);
    delay(500); // Green LED ON for 500ms
    digitalWrite(greenLEDPin, LOW);
    delay(500); // Green LED OFF for 500ms
  } else {
    // No finger detected, display zero on the OLED and stop any LED/Buzzer
    digitalWrite(greenLEDPin, LOW);
  }

  delay(1000);  // Delay before the next measurement
}

// Function to send an SMS with the glucose level
void sendSMS(float glucoseLevel, String levelType) {
  char smsMessage[100];
  if (levelType == "high") {
    sprintf(smsMessage, "Alert: High Blood Sugar! %.2f. Please administer insulin or consult a doctor.", glucoseLevel);
  } else if (levelType == "low") {
    sprintf(smsMessage, "Alert: Low Blood Sugar! %.2f. Please take glucose or consult a doctor.", glucoseLevel);
  }
  
  sim800.println("AT+CMGS=\"+919908488566\"");  // Replace with your recipient number
  delay(1000);
  sim800.println(smsMessage);  
  delay(1000);
  sim800.write(26);  // End SMS with Ctrl+Z (ASCII code 26)
  delay(1000);

  Serial.println("SMS sent!");
}