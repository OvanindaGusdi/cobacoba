#define BLYNK_TEMPLATE_ID "TMPL65SRoXUg5"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "w5YqL4s68FKI083Hj38VK-AcmqTCpzX3"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Ovaninda";
char pass[] = "samsung55";

BlynkTimer timer;

// ===== PZEM004T =====
#define PZEM_RX 16
#define PZEM_TX 17
PZEM004Tv30 pzem(Serial2, PZEM_RX, PZEM_TX);

// ===== DHT22 =====
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ===== Relay & Button =====
const int relayPins[4]  = {18, 19, 23, 5};
const int buttonPins[4] = {26, 25, 33, 32};
int relayState[4]       = {LOW, LOW, LOW, LOW};
int lastButtonState[4]  = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
const unsigned long debounceDelay = 200;

const int vPins[4] = {V1, V2, V3, V4};
const char* lampuNames[4] = {"Lampu 1", "Lampu 2", "Lampu 3", "Lampu 4"};

// ===== LCD Mode =====
enum DisplayMode {LAMPU_STATUS, PZEM_DISPLAY, TEMPHUMID};
DisplayMode currentMode = PZEM_DISPLAY;  // mulai dari PZEM
unsigned long lastActivityTime = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long idleTimeout = 5000; // kembali ke auto mode setelah 5 detik
const unsigned long displaySwitchTime = 5000; // tiap 5 detik ganti tampilan

// ===== Variabel data sensor =====
float voltage = 0, currentVal = 0, power = 0, energy = 0;
float freq = 0, pf = 0;

// ====== BLYNK CONNECT ======
BLYNK_CONNECTED() {
  for (int i = 0; i < 4; i++) Blynk.syncVirtual(vPins[i]);
}

// ====== BLYNK CONTROL ======
BLYNK_WRITE_DEFAULT() {
  int pinIndex = request.pin - V1;
  if (pinIndex >= 0 && pinIndex < 4) {
    relayState[pinIndex] = param.asInt();
    digitalWrite(relayPins[pinIndex], relayState[pinIndex]);
    Blynk.virtualWrite(vPins[pinIndex], relayState[pinIndex]);
    Serial.printf("%s %s (Blynk)\n", lampuNames[pinIndex], relayState[pinIndex] ? "nyala" : "mati");
    showLampStatus();
    currentMode = LAMPU_STATUS;
    lastActivityTime = millis();
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
    digitalWrite(relayPins[i], LOW);
  }

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(3,0); lcd.print("MONITORING AND");
  lcd.setCursor(5,1); lcd.print("CONTROLLING");
  lcd.setCursor(5,2); lcd.print("OVAN'S ROOM");
  delay(2000);

  dht.begin();
  Blynk.begin(auth, ssid, pass);

  timer.setInterval(2000L, updateSensors);  // update sensor rutin
  timer.setInterval(5000L, sendPZEMData);   // kirim ke Blynk
}

// ====== LOOP ======
void loop() {
  Blynk.run();
  timer.run();
  checkButtons();

  // Jika mode lampu, tunggu idle 5 detik baru kembali rotasi otomatis
  if (currentMode == LAMPU_STATUS && millis() - lastActivityTime > idleTimeout) {
    currentMode = PZEM_DISPLAY;
    lastDisplaySwitch = millis();
    showPZEMDataLCD();
  }

  // Jika tidak sedang kontrol lampu → ganti tampilan otomatis
  if (currentMode != LAMPU_STATUS && millis() - lastDisplaySwitch > displaySwitchTime) {
    if (currentMode == PZEM_DISPLAY) {
      currentMode = TEMPHUMID;
      showTemperatureHumidity();
    } else {
      currentMode = PZEM_DISPLAY;
      showPZEMDataLCD();
    }
    lastDisplaySwitch = millis();
  }
}

// ====== BUTTON HANDLER ======
void checkButtons() {
  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading == LOW && lastButtonState[i] == HIGH && millis() - lastDebounceTime[i] > debounceDelay) {
      lastDebounceTime[i] = millis();
      relayState[i] = !relayState[i];
      digitalWrite(relayPins[i], relayState[i]);
      Blynk.virtualWrite(vPins[i], relayState[i]);
      Serial.printf("%s %s (Manual)\n", lampuNames[i], relayState[i] ? "nyala" : "mati");

      showLampStatus();
      currentMode = LAMPU_STATUS;
      lastActivityTime = millis();
    }
    lastButtonState[i] = reading;
  }
}

// ====== LCD FUNCTIONS ======
void showTemperatureHumidity() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Temperature :");
  lcd.setCursor(10,1); lcd.print(t,1); lcd.print(" C");
  lcd.setCursor(0,2); lcd.print("Humidity :");
  lcd.setCursor(10,3); lcd.print(h,1); lcd.print(" %");

  if (!isnan(t)) Blynk.virtualWrite(V11, t);
  if (!isnan(h)) Blynk.virtualWrite(V12, h);
}

void showPZEMDataLCD() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Voltage: "); lcd.print(voltage,1); lcd.print(" Volt");
  lcd.setCursor(0,1); lcd.print("Current: "); lcd.print(currentVal,2); lcd.print(" Amp");
  lcd.setCursor(0,2); lcd.print("Power  : "); lcd.print(power,1); lcd.print(" Watt");
  lcd.setCursor(0,3); lcd.print("Energy : "); lcd.print(energy,3); lcd.print(" kWh");
}

void showLampStatus() {
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0, i);
    lcd.print(lampuNames[i]);
    lcd.print(": ");
    lcd.print(relayState[i] ? "ON " : "OFF");
  }
}

// ====== UPDATE SENSOR ======
void updateSensors() {
  // baca data DHT
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) Blynk.virtualWrite(V11, t);
  if (!isnan(h)) Blynk.virtualWrite(V12, h);

  // baca data PZEM
  voltage = pzem.voltage();
  currentVal = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  freq = pzem.frequency();
  pf = pzem.pf();
}

// ====== PZEM DATA KE BLYNK ======
void sendPZEMData() {
  if (!isnan(voltage)) Blynk.virtualWrite(V5, voltage);
  if (!isnan(currentVal)) Blynk.virtualWrite(V6, currentVal);
  if (!isnan(power))   Blynk.virtualWrite(V7, power);
  if (!isnan(energy))  Blynk.virtualWrite(V8, energy);
  if (!isnan(freq))    Blynk.virtualWrite(V9, freq);
  if (!isnan(pf))      Blynk.virtualWrite(V10, pf);

  Serial.printf("V=%.1fV I=%.2fA P=%.1fW E=%.3fkWh F=%.1fHz PF=%.2f\n",
                voltage, currentVal, power, energy, freq, pf);
}
