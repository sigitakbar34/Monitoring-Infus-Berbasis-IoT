#include "esp32-hal-cpu.h"
#include <Arduino.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "sesuaikan nama ssid"
#define WIFI_PASSWORD "sesuaikan password wifi"

#define API_KEY "sesuaikan API key-nya"

#define DATABASE_URL "sesuaikan url database-nya"

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

int LEDred = 32;
int LEDyellow = 33;
int LEDgreen = 25;
int buzzer = 26;
int button = 27;
int pinIR = 34;

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 18;

HX711 scale;

int tetesSekarang = 0;
int oldJumlah = 0;
int jumlahTetes = 0;
int berat;
int selisih = 1;
unsigned long waktuSebelumnya = 0;
unsigned long warning = 60000;
unsigned long sumbat = 0;
int tetes;

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);

  pinMode(pinIR, INPUT_PULLUP);
  pinMode(button, INPUT_PULLUP);
  pinMode(LEDred, OUTPUT);
  pinMode(LEDyellow, OUTPUT);
  pinMode(LEDgreen, OUTPUT);
  pinMode(buzzer, OUTPUT);
  // ledcAttachPin(buzzer, 0);
  lcd.init();
  lcd.backlight();
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // scale.set_scale(1083.74166);
  scale.set_scale(1089.720);

  //scale.set_scale(-471.497);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();  // reset the scale to 0

  lcd.setCursor(0, 0);
  lcd.print("   Monitoring");
  lcd.setCursor(0, 1);
  lcd.print("      Infus");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   KELOMPOK 1");
  delay(1000);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Connecting");
  lcd.setCursor(0, 1);
  lcd.print("    to Wi-Fi");
  delay(2000);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    delay(300);
  }
  Serial.println();
  Serial.print("  Connected");
  Serial.println(WiFi.localIP());
  Serial.println();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Connected");

  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }

  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  attachInterrupt(digitalPinToInterrupt(button), reset, RISING);

  float berat = scale.get_units(10);
  int tetes = digitalRead(pinIR);
  unsigned long waktuSekarang = millis();


  if (waktuSekarang - waktuSebelumnya >= 1000) {
    if (berat < 0) {
      berat = 0;
    }

    if (tetes == 0) {
      tetesSekarang++;
      jumlahTetes += tetesSekarang;
      tetesSekarang = 0;
    }

    selisih = jumlahTetes - oldJumlah;
    oldJumlah = jumlahTetes;

    // pasang infus
    if (berat < 50) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  Pasang Infus");
      digitalWrite(LEDyellow, HIGH);
      digitalWrite(LEDred, LOW);
      digitalWrite(buzzer, LOW);
      digitalWrite(LEDgreen, LOW);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat = 0);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes = 0);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "-");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "-");
      jumlahTetes = 0;
    }

    // infus habis
    else if (berat > 50 && berat <= 80) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Infus Habis");
      lcd.setCursor(0, 1);
      lcd.print("Jml Tetes = ");
      lcd.print(jumlahTetes);
      digitalWrite(LEDgreen, LOW);
      digitalWrite(LEDyellow, LOW);
      digitalWrite(LEDred, HIGH);
      digitalWrite(buzzer, HIGH);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "Habis");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "-");
      jumlahTetes = 0;
    }

    unsigned long now = millis();
    // hampir habis & tetes aman
    if (now - sumbat <= warning && selisih != 0 && berat > 80 && berat <= 100) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hampir Habis");
      lcd.setCursor(0, 1);
      lcd.print("Jml Tetes = ");
      lcd.print(jumlahTetes);
      digitalWrite(LEDgreen, LOW);
      digitalWrite(LEDred, LOW);
      digitalWrite(LEDyellow, HIGH);
      digitalWrite(buzzer, HIGH);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "Hampir-Habis");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "OK");
      sumbat = millis();
    }

    // hampir habis & tetes tersumbat
    else if (now - sumbat > warning && selisih == 0 && berat > 80 && berat <= 100) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  TERSUMBAT!!!");
      digitalWrite(LEDred, HIGH);
      digitalWrite(buzzer, HIGH);
      digitalWrite(LEDgreen, LOW);
      digitalWrite(LEDyellow, HIGH);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "Hampir-Habis");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "TERSUMBAT!!!");
      sumbat = millis();
    }

    // sisa aman & tetes aman
    else if (now - sumbat <= warning && selisih != 0 && berat > 100) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sisa = ");
      lcd.print(berat, 2);
      lcd.setCursor(14, 0);
      lcd.print("g");
      digitalWrite(LEDgreen, HIGH);
      digitalWrite(LEDred, LOW);
      digitalWrite(buzzer, LOW);
      digitalWrite(LEDyellow, LOW);
      lcd.setCursor(0, 1);
      lcd.print("Jml Tetes = ");
      lcd.print(jumlahTetes);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "OK");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "OK");
      sumbat = millis();
    }

    // sisa aman & tersumbat
    else if (now - sumbat > warning && selisih == 0 && berat > 100) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  TERSUMBAT!!!");
      digitalWrite(LEDred, HIGH);
      digitalWrite(buzzer, HIGH);
      digitalWrite(LEDgreen, LOW);
      digitalWrite(LEDyellow, LOW);
      Firebase.RTDB.setFloat(&fbdo, "/Hasil_Pembacaan/Sisa", berat);
      Firebase.RTDB.setInt(&fbdo, "/Hasil_Pembacaan/jmlTetes", jumlahTetes);
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusSisa", "OK");
      Firebase.RTDB.setString(&fbdo, "/Hasil_Pembacaan/statusTetes", "TERSUMBAT!!!");
      sumbat = millis();
    }
  }

  waktuSebelumnya = millis();
  scale.power_down();
  delay(1000);
  scale.power_up();
}


void reset() {
  jumlahTetes = 0;
}
