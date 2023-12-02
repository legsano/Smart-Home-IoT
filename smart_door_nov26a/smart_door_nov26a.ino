#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "arduino_secrets.h"
#include "thingProperties.h"
#include <DHT.h>

#define DHTPIN 13  // Pin connected to the DHT22 sensor
#define DHTTYPE DHT22  // DHT 22 (AM2302) sensor type
#define SS_PIN    5   // SDA pin for MFRC522
#define RST_PIN   4   // Reset pin for MFRC522
#define SERVO_PIN 25  // Servo motor control pin
#define MOTOR_PIN1 33 // Motor control pin 1
#define MOTOR_PIN2 32 // Motor control pin 2
#define MAG_SWITCH 14 // GPIO pin for magnetic switch
#define SCREEN_RESET 16 // Reset pin for OLED display
#define KY037_ANALOG_PIN 34   // analog pin for KY-037 sensor
#define KY037_DIGITAL_PIN 35   // Digital pin for output KY-037 sensor
#define LED_PIN 2             // Digital pin for LED 

double frequency = 200;
uint8_t resolution_bits=10;
DHT dht(DHTPIN, DHTTYPE);
// Define the correct RFID card NUID
byte correctNUID[] = {0x92, 0xA6, 0xB8, 0x89};

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myservo;
ESP32PWM PWMSET;

// OLED display setup
Adafruit_SSD1306 display(128, 64, &Wire, SCREEN_RESET);

bool doorLocked = true; // Variable to track the door lock state
int knockCount = 0;           // Number of knockCount

void setup() {
  Serial.begin(115200);
  SPI.begin();
  dht.begin();
  delay(1500);
  initProperties();
  mfrc522.PCD_Init();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  pinMode(SERVO_PIN, OUTPUT);
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MAG_SWITCH, INPUT_PULLUP);
  pinMode(KY037_ANALOG_PIN, INPUT);
  pinMode(KY037_DIGITAL_PIN, INPUT);  
  pinMode(LED_PIN, OUTPUT);

  PWMSET.attachPin(SERVO_PIN, frequency);

  myservo.attach(SERVO_PIN);
  
  // OLED display initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // Print initial status
  updateDisplay();
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();
  int humidity = dht.readHumidity();          // Analog DHT11 readHumidity
  float temperature = dht.readTemperature();  // Analog DHT11 readTemperature
  humid = humidity;
  temp = temperature;

  ArduinoCloud.update();

  // Deteksi suara tepukan tangan dengan menggunakan output digital KY-037
  if (digitalRead(KY037_DIGITAL_PIN) == HIGH) {
    knock();
  }

  if (digitalRead(MAG_SWITCH) == HIGH) {
    if (!doorLocked) {
      unlockDoor(); // Unlock the door if it's not locked
    }
    displayStatus("Door Opened");
    myservo.write(90); // Unlock position
    delay(1000);
  } else {
    ArduinoCloud.update();
    displayStatus("Door Locked");
    doorlocked == true;
    ArduinoCloud.update();
    myservo.write(0);
    // Check RFID card only if the door is closed
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      if (compareNUID(mfrc522.uid.uidByte, correctNUID, mfrc522.uid.size)) {
        displayStatus("Card Verified");
        unlockDoor();
        delay(5000);
        lockDoor();
      } else {
        displayStatus("Access Denied");
        delay(2000);
      }
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }
}

bool compareNUID(byte *nuid1, byte *nuid2, byte size) {
  for (byte i = 0; i < size; i++) {
    if (nuid1[i] != nuid2[i]) {
      return false;
    }
  }
  return true;
}

void knock(){
    knockCount++;
    Serial.print("Tepukan Tangan ke-");
    Serial.println(knockCount);

   if (knockCount == 1) {
      // Tepuk tangan 1x, hidupkan LED pada ESP32
      digitalWrite(LED_PIN, HIGH);
      led = true;
    } else if (knockCount == 2) {
      // Tepuk tangan 2x, matikan LED pada ESP32
      digitalWrite(LED_PIN, LOW);
      knockCount = 0; // Atur ulang knockCount setelah 2 tepukan
      led = false;
    }
    // Tunggu sebentar untuk menghindari deteksi ganda
    delay(1000);
}

void unlockDoor() {
  doorLocked = false;
  updateDisplay();
  displayStatus("Door Unlocked");
  myservo.write(90); // Unlock position
  delay(1000);
  digitalWrite(MOTOR_PIN1, HIGH); // Motor rotates to open the door
  digitalWrite(MOTOR_PIN2, LOW);
  delay(3000); // Adjust this delay according to the door opening time
  digitalWrite(MOTOR_PIN1, LOW);
  doorlog = "Door Unlocked";
  ArduinoCloud.update();
}

void unlockDoor2() {
  doorLocked = false;
  updateDisplay();
  displayStatus("Door Unlocked");
  myservo.write(90); // Unlock position
  digitalWrite(MOTOR_PIN1, HIGH); // Motor rotates to open the door
  digitalWrite(MOTOR_PIN2, LOW);
  digitalWrite(MOTOR_PIN1, LOW);
  doorlog = "Door Unlocked";
  ArduinoCloud.update();
}


void lockDoor() {
  if (!doorLocked) {
    doorLocked = true;
    updateDisplay();
    displayStatus("Door Locked");
    myservo.write(0); // Lock position
    delay(1000);
    digitalWrite(MOTOR_PIN1, LOW); // Motor stops
    digitalWrite(MOTOR_PIN2, LOW);

    // Send "door locked" to IoT Cloud
    doorlog = "Door Locked";
    ArduinoCloud.update();
  }
}

void onDoorlockedChange() {
  if (doorlocked==1) {
    doorLocked = false;
    lockDoor();
  } if(doorlocked==0) {
    doorLocked = true;
    unlockDoor2();
  }
}

void displayStatus(const char *status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.display();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if (doorLocked) {
    display.println("Door Locked");
    display.setCursor(0, 20);     
    display.println("Scan your RFID");
  } else {
    display.println("Door Unlocked");
    display.setCursor(0, 20);
    display.println("Door is open");
  }
  display.display();
}

void onDoorlogChange()  {
  // Add your code here to act upon Doorlog change
}

void onLedChange()  {
  // Add your code here to act upon Led change
  if (led) {
    // Turn on LED
    digitalWrite(LED_PIN, HIGH);
    knockCount = 1;
  } else {
    // Turn off LED
    digitalWrite(LED_PIN, LOW);
    knockCount = 0;
  }
}

void onTempChange()  {
  // Add your code here to act upon Temp change
  Serial.print("New Temperature value: ");
  Serial.println(temp);
}

void onHumidChange()  {
  // Add your code here to act upon Humid change
  Serial.print("New Humidity value: ");
  Serial.println(humid);
}

