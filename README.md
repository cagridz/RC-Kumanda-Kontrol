# RC-Kumanda-Kontrol
//Lora, GPS, Lidar, RC Kumanda, Ultrasonik sensör destekleyen Arduino Kodu
#include <L298N.h>
#include "Wire.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <Servo.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "LoRa_E32.h"
#include <IBusBM.h>

// Pin Tanımlamaları - YENİDEN DÜZENLENDİ

// Motor Pinleri (Sabit - değiştirilmedi)
// Motor1: 9, 8, 7
// Motor2: 6, 5, 4

// LoRa Pinleri (Yeniden düzenlendi)
#define LORA_RX 10      // E32 RX -> Arduino Pin 10
#define LORA_TX 11      // E32 TX -> Arduino Pin 11
#define LORA_M0 2       // Mode Control Pin M0 (değiştirildi: 6 -> 2)
#define LORA_M1 3       // Mode Control Pin M1 (değiştirildi: 7 -> 3)
#define LORA_AUX 5      // AUX Pin (değiştirildi, motor2 ile çakışıyordu)

// GPS Pinleri (Yeniden düzenlendi)
#define GPS_RX 12       // (değiştirildi: 3 -> 12)
#define GPS_TX 13       // (değiştirildi: 4 -> 13)
#define GPS_BAUD 9600

// Ultrasonik Sensör Pinleri (Yeniden düzenlendi)
#define TRIG_PIN 22     // (değiştirildi: 7 -> 22)
#define ECHO_PIN 23     // (değiştirildi: 6 -> 23)
#define OBSTACLE_THRESHOLD 30 // cm

// Kumanda Alıcısı Pini (Sabit - değiştirilmedi)
#define IBUS_RX_PIN 19  // Arduino Mega RX1

// iBUS için doğrudan Serial1 kullanımı
IBusBM ibus;

// Motor Sürücüler (Sabit - değiştirilmedi)
L298N motor1(9, 8, 7);
L298N motor2(6, 5, 4);

// İletişim ve Sensörler için Nesneler
SoftwareSerial LoRaSerial(LORA_RX, LORA_TX);
SoftwareSerial portgps(GPS_RX, GPS_TX);
LoRa_E32 e32ttl(&LoRaSerial);
TinyGPSPlus gps;
Adafruit_HMC5883_Unified compass = Adafruit_HMC5883_Unified(12345);

// Kontrol ve GPS Değişkenleri
int desired_heading;
int compass_heading;
int compass_dev = 5;
bool goToTarget = false;
bool returning = false;
double targetLat = 0.0;
double targetLon = 0.0;
unsigned long startTime = 0;
const unsigned long waitTime = 30000; // 30 saniye

// LoRa İletişim Yapısı
typedef struct {
  byte pitch;
  byte roll;
  byte yaw;
} Signal;

Signal data;

// RC Kumanda Değerleri
int throttle = 0;
int steering = 0;
unsigned long lastControlTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastLoRaTime = 0;

void setup() {
  // Seri haberleşmeyi başlat
  Serial.begin(115200);
  
  // Hardware Serial için iBUS
  Serial1.begin(115200);  // FS-iA6B için
  ibus.begin(Serial1);
  
  // GPS başlat (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  portgps.begin(GPS_BAUD);
  
  // LoRa modülünü başlat (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  LoRaSerial.begin(9600);
  e32ttl.begin();
  
  // I2C ve pusula başlat (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  Wire.begin();
  /*
  if (!compass.begin()) {
    Serial.println("HMC5883L bulunamadı, bağlantıyı kontrol edin!");
    // Pusula takılı değilse programı durdurma
  }
  */
  
  // Ultrasonik sensör pinlerini ayarla (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // LoRa control pinlerini ayarla
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);
  pinMode(LORA_AUX, INPUT);
  
  // Normal çalışma modu için M0 ve M1'i LOW yapın
  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);
  
  Serial.println("Sistem başlatıldı!");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Kumanda verilerini oku (20ms'de bir - 50Hz)
  if (currentMillis - lastControlTime >= 20) {
    readFlySkyReceiver();
    controlMotors(); // Hemen kumanda değerlerini uygula
    lastControlTime = currentMillis;
  }
  
  // GPS verilerini oku (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  // Sadece her 100ms'de bir GPS verilerini oku
  if (currentMillis - lastControlTime >= 100) {
    while (portgps.available() > 0) {
      gps.encode(portgps.read());
    }
  }
  
  // LoRa ile veri gönderme (isteğe bağlı - takılı değilse devre dışı bırakılabilir)
  // Sadece her 500ms'de bir LoRa verileri gönder
  if (currentMillis - lastLoRaTime >= 500) {
    sendLoRaData();
    lastLoRaTime = currentMillis;
  }
  
  // Debug bilgilerini ekrana yazdır (sadece her 500ms'de bir)
  if (currentMillis - lastPrintTime >= 500) {
    Serial.print("Throttle: ");
    Serial.print(throttle);
    Serial.print(", Steering: ");
    Serial.println(steering);
    lastPrintTime = currentMillis;
  }
}

void readFlySkyReceiver() {
  ibus.loop();
  
  // Yeni değerleri oku
  int newThrottle = ibus.readChannel(2);  // Kanal 3: Gaz
  int newSteering = ibus.readChannel(0);  // Kanal 1: Yön
  
  // Gaz ve yön değerlerini -500 ile 500 arasına ölçekle
  throttle = map(newThrottle, 1000, 2000, -500, 500);
  steering = map(newSteering, 1000, 2000, 500, -500);  // Yön ters çevrildi!
}

void controlMotors() {
  // Ultrasonik sensör takılı değilse bu kısmı devre dışı bırakın
  /*
  int distance = measureDistance();
  if (distance < OBSTACLE_THRESHOLD && throttle > 50) {
    motor1.stop();
    motor2.stop();
    Serial.println("Engel tespit edildi! Duruyorum.");
    return;
  }
  */

  // Motor hızlarını hesapla
  int leftMotorSpeed = throttle + steering;
  int rightMotorSpeed = throttle - steering;

  leftMotorSpeed = constrain(leftMotorSpeed, -500, 500);
  rightMotorSpeed = constrain(rightMotorSpeed, -500, 500);

  // Motor sürücü için 0-255 aralığına dönüştür
  int leftPower = map(abs(leftMotorSpeed), 0, 500, 0, 255);
  int rightPower = map(abs(rightMotorSpeed), 0, 500, 0, 255);
  
  // Motorların yönünü ve hızını ayarla
  // Motor durak bölgesi için daha küçük eşik (10)
  if (leftMotorSpeed > 10) {  // İleri
    motor1.setSpeed(leftPower);
    motor1.forward();
  } else if (leftMotorSpeed < -10) {  // Geri
    motor1.setSpeed(leftPower);
    motor1.backward();
  } else {  // Dur
    motor1.stop();
  }

  if (rightMotorSpeed > 10) {  // İleri
    motor2.setSpeed(rightPower);
    motor2.forward();
  } else if (rightMotorSpeed < -10) {  // Geri
    motor2.setSpeed(rightPower);
    motor2.backward();
  } else {  // Dur
    motor2.stop();
  }
}

int measureDistance() {
  // Ultrasonik sensör ile mesafe ölçümü
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
  return distance;
}

void sendLoRaData() {
  // LoRa ile veri gönderme
  data.pitch = map(throttle, -500, 500, 0, 255);
  data.roll = 127; // Orta değer
  data.yaw = map(steering, -500, 500, 0, 255);
  
  ResponseStatus rs = e32ttl.sendFixedMessage(21, 179, 7, &data, sizeof(Signal));
}

void requestTargetLocation() {
  Serial.println("Hedef konum isteniyor...");
  ResponseStatus rs = e32ttl.sendFixedMessage(21, 179, 7, "REQUEST_LOCATION", sizeof("REQUEST_LOCATION"));
  Serial.println(rs.getResponseDescription());
}
