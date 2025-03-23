#include <L298N.h>
#include "Wire.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <Servo.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "LoRa_E32.h"
#include <IBusBM.h>

#define LORA_RX 10
#define LORA_TX 11
#define LORA_M0 2
#define LORA_M1 3
#define LORA_AUX 5

#define GPS_RX 12
#define GPS_TX 13
#define GPS_BAUD 9600

#define TRIG_PIN 22
#define ECHO_PIN 23
#define TRIG_PIN_2 24
#define ECHO_PIN_2 25
#define OBSTACLE_THRESHOLD 10

#define IBUS_RX_PIN 19

IBusBM ibus;
L298N motor1(9, 8, 7);
L298N motor2(6, 5, 4);

SoftwareSerial LoRaSerial(LORA_RX, LORA_TX);
SoftwareSerial portgps(GPS_RX, GPS_TX);
LoRa_E32 e32ttl(&LoRaSerial);
TinyGPSPlus gps;
Adafruit_HMC5883_Unified compass = Adafruit_HMC5883_Unified(12345);

int desired_heading;
int compass_heading;
int compass_dev = 5;
bool goToTarget = false;
bool returning = false;
double targetLat = 0.0;
double targetLon = 0.0;
unsigned long startTime = 0;
const unsigned long waitTime = 30000;

typedef struct {
  byte pitch;
  byte roll;
  byte yaw;
} Signal;

Signal data;

int throttle = 0;
int steering = 0;
int manualControlSwitch = 0;  // CH6: SWD - kumanda erişim tuşu
unsigned long lastControlTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastLoRaTime = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  ibus.begin(Serial1);
  portgps.begin(GPS_BAUD);
  LoRaSerial.begin(9600);
  e32ttl.begin();
  Wire.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);
  pinMode(LORA_AUX, INPUT);
  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);
  Serial.println("Sistem başlatıldı!");
}

void loop() {
  unsigned long currentMillis = millis();
  readFlySkyReceiver();

  if (manualControlSwitch > 1500) {
    controlMotors();
  } else {
    motor1.stop();
    motor2.stop();
  }

  if (currentMillis - lastLoRaTime >= 500) {
    sendLoRaData();
    lastLoRaTime = currentMillis;
  }

  if (currentMillis - lastPrintTime >= 500) {
    Serial.print("Throttle: "); Serial.print(throttle);
    Serial.print(", Steering: "); Serial.print(steering);
    Serial.print(", Kumanda Erişim: "); Serial.println(manualControlSwitch > 1500 ? "AKTİF" : "KAPALI");
    lastPrintTime = currentMillis;
  }
}

void readFlySkyReceiver() {
  ibus.loop();
  throttle = map(ibus.readChannel(2), 1000, 2000, -500, 500);
  steering = map(ibus.readChannel(0), 1000, 2000, 500, -500);
  manualControlSwitch = ibus.readChannel(5);   // CH6 (SWD) - kumanda erişimi
}

void controlMotors() {
  int distance1 = measureDistance(TRIG_PIN, ECHO_PIN);
  delay(50); // yankı karışmasını önlemek için gecikme
  int distance2 = measureDistance(TRIG_PIN_2, ECHO_PIN_2);

  if ((distance1 > 0 && distance1 < OBSTACLE_THRESHOLD && throttle > 50) ||
      (distance2 > 0 && distance2 < OBSTACLE_THRESHOLD && throttle > 50)) {
    motor1.stop();
    motor2.stop();
    Serial.println("ENGEL ALGILANDI! Duruyorum...");
    return;
  }

  if (throttle < -450 && steering > 450) {
    motor1.stop();
    motor2.stop();
    Serial.println("FREN MODU AKTİF");
    return;
  }

  int leftMotorSpeed = throttle + steering;
  int rightMotorSpeed = throttle - steering;

  leftMotorSpeed = constrain(leftMotorSpeed, -500, 500);
  rightMotorSpeed = constrain(rightMotorSpeed, -500, 500);

  int leftPower = map(abs(leftMotorSpeed), 0, 500, 0, 255);
  int rightPower = map(abs(rightMotorSpeed), 0, 500, 0, 255);

  if (leftMotorSpeed > 10) {
    motor1.setSpeed(leftPower);
    motor1.forward();
  } else if (leftMotorSpeed < -10) {
    motor1.setSpeed(leftPower);
    motor1.backward();
  } else {
    motor1.stop();
  }

  if (rightMotorSpeed > 10) {
    motor2.setSpeed(rightPower);
    motor2.forward();
  } else if (rightMotorSpeed < -10) {
    motor2.setSpeed(rightPower);
    motor2.backward();
  } else {
    motor2.stop();
  }
}

void sendLoRaData() {
  data.pitch = map(throttle, -500, 500, 0, 255);
  data.roll = 127;
  data.yaw = map(steering, -500, 500, 0, 255);
  e32ttl.sendFixedMessage(21, 179, 7, &data, sizeof(Signal));
}

void requestTargetLocation() {
  Serial.println("Hedef konum isteniyor...");
  ResponseStatus rs = e32ttl.sendFixedMessage(21, 179, 7, "REQUEST_LOCATION", sizeof("REQUEST_LOCATION"));
  Serial.println(rs.getResponseDescription());
}

int measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30 ms timeout
  if (duration == 0) return 999; // zaman aşımı olursa geçersiz mesafe dön
  return duration * 0.034 / 2;
}
