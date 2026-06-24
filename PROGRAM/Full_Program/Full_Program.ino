#define BLYNK_TEMPLATE_ID   "TMPL6UU8a3mKv"
#define BLYNK_TEMPLATE_NAME "Sistem Irigasi Otomatis"
#define BLYNK_AUTH_TOKEN    "Hqw8VlBiPjx6HVdlJnQGiCXmD-38AS2h"

#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define WIFI_SSID "Kos madani 2"
#define WIFI_PASS "Iqbal2003"

// =============================================
// VIRTUAL PIN BLYNK
// =============================================
#define VP_S1           V0
#define VP_S2           V1
#define VP_S3           V2
#define VP_S4           V3
#define VP_RATA         V4
#define VP_SELISIH      V5
#define VP_SERVO        V6
#define VP_MODE         V7   // Switch: 0=otomatis, 1=manual
#define VP_BTN_TUTUP    V8   // Button preset tutup
#define VP_BTN_SEDIKIT  V9   // Button preset sedikit
#define VP_BTN_CUKUP    V10  // Button preset cukup
#define VP_BTN_PENUH    V11  // Button preset penuh
#define VP_SLIDER       V12  // Slider manual 20-160°

// =============================================
// STRUCT
// =============================================
struct FuzzyOutput {
  float alpha_tertutup;
  float alpha_sedikit;
  float alpha_cukup;
  float alpha_penuh;
};

// =============================================
// PIN & KONSTANTA
// =============================================
#define SENSOR_1      4
#define SENSOR_2      6
#define SENSOR_3      5
#define SENSOR_4      3
#define SERVO_PIN     18
#define JUMLAH_BACA   100
#define BATAS_DARURAT 3.8

// =============================================
// VARIABEL GLOBAL
// =============================================
bool  modeManual   = false;
float sudutManual  = 20.0;
int   sensorPemicu = 0;
int   counter      = 0;

BlynkTimer timer;

// =============================================
// SERVO
// =============================================
Servo myServo;

void servoInit() {
  myServo.attach(SERVO_PIN);
  myServo.write(20);
  delay(500);
}

void servoTulis(float sudut) {
  sudut = constrain(sudut, 20.0, 160.0);
  myServo.write((int)sudut);
}

// =============================================
// TABEL KALIBRASI
// =============================================
float kalS1[][2] = {
  {0,0.0},{648,0.5},{812,1.0},{1000,1.5},
  {1147,2.0},{1300,2.5},{1500,3.0},{1650,3.5},{1710,4.0}
};
float kalS2[][2] = {
  {0,0.0},{648,0.5},{720,1.0},{900,1.5},
  {1122,2.0},{1270,2.5},{1300,3.0},{1455,3.5},{1550,4.0}
};
float kalS3[][2] = {
  {0,0.0},{650,0.5},{832,1.0},{1192,1.5},
  {1276,2.0},{1477,2.5},{1747,3.0},{1800,3.5},{1900,4.0}
};
float kalS4[][2] = {
  {0,0.0},{650,0.5},{800,1.0},{1200,1.5},
  {1265,2.0},{1410,2.5},{1681,3.0},{1720,3.5},{1790,4.0}
};
int kalSize = 9;

// =============================================
// FUNGSI SENSOR
// =============================================
float interpolasi(float adc, float tabel[][2], int size) {
  if (adc <= tabel[0][0])      return 0.0;
  if (adc >= tabel[size-1][0]) return 4.0;
  for (int i = 0; i < size-1; i++) {
    if (adc >= tabel[i][0] && adc <= tabel[i+1][0]) {
      float ratio = (adc-tabel[i][0])/(tabel[i+1][0]-tabel[i][0]);
      return tabel[i][1] + ratio*(tabel[i+1][1]-tabel[i][1]);
    }
  }
  return 0.0;
}

int bacaSensor(int pin) {
  int samples[JUMLAH_BACA];
  for (int i = 0; i < JUMLAH_BACA; i++) {
    samples[i] = analogRead(pin);
    delay(1);
  }
  for (int i = 0; i < JUMLAH_BACA-1; i++)
    for (int j = 0; j < JUMLAH_BACA-i-1; j++)
      if (samples[j] > samples[j+1]) {
        int t=samples[j]; samples[j]=samples[j+1]; samples[j+1]=t;
      }
  long total = 0;
  int dari   = JUMLAH_BACA/4;
  int sampai = 3*JUMLAH_BACA/4;
  for (int i = dari; i < sampai; i++) total += samples[i];
  return total/(sampai-dari);
}

// =============================================
// CEK DARURAT
// =============================================
bool cekDarurat(float cm1, float cm2, float cm3, float cm4) {
  if (cm1 >= BATAS_DARURAT) { sensorPemicu=1; return true; }
  if (cm2 >= BATAS_DARURAT) { sensorPemicu=2; return true; }
  if (cm3 >= BATAS_DARURAT) { sensorPemicu=3; return true; }
  if (cm4 >= BATAS_DARURAT) { sensorPemicu=4; return true; }
  sensorPemicu = 0;
  return false;
}

// =============================================
// FUZZY ENGINE
// =============================================
float mf_rendah(float h) {
  if (h <= 0.8) return 1.0;
  if (h >= 2.0) return 0.0;
  return (2.0-h)/(2.0-0.8);
}
float mf_sedang(float h) {
  if (h <= 1.5 || h >= 3.5) return 0.0;
  if (h <= 2.5) return (h-1.5)/(2.5-1.5);
  return (3.5-h)/(3.5-2.5);
}
float mf_tinggi(float h) {
  if (h <= 3.0) return 0.0;
  if (h >= 3.5) return 1.0;
  return (h-3.0)/(3.5-3.0);
}
float mf_kecil(float d) {
  if (d <= 0.4) return 1.0;
  if (d >= 0.8) return 0.0;
  return (0.8-d)/(0.8-0.4);
}
float mf_selisih_sedang(float d) {
  if (d <= 0.6 || d >= 1.4) return 0.0;
  if (d <= 1.0) return (d-0.6)/(1.0-0.6);
  return (1.4-d)/(1.4-1.0);
}
float mf_besar(float d) {
  if (d <= 1.2) return 0.0;
  if (d >= 1.6) return 1.0;
  return (d-1.2)/(1.6-1.2);
}

FuzzyOutput evaluasiRules(float h, float d) {
  float r=mf_rendah(h), sm=mf_sedang(h), t=mf_tinggi(h);
  float k=mf_kecil(d),  sd=mf_selisih_sedang(d), b=mf_besar(d);
  float a1=min(r,k),  a2=min(r,sd),  a3=min(r,b);
  float a4=min(sm,k), a5=min(sm,sd), a6=min(sm,b);
  float a7=min(t,k),  a8=min(t,sd),  a9=min(t,b);
  FuzzyOutput fo;
  fo.alpha_penuh    = max(a1, a2);
  fo.alpha_cukup    = max(a3, a4);
  fo.alpha_sedikit  = max(a5, a6);
  fo.alpha_tertutup = max(a7, max(a8, a9));
  return fo;
}

const float CENTER_TERTUTUP = 27.5;
const float CENTER_SEDIKIT  = 70.0;
const float CENTER_CUKUP    = 110.0;
const float CENTER_PENUH    = 142.5;

float defuzzifikasi(FuzzyOutput fo) {
  float p = (fo.alpha_tertutup*CENTER_TERTUTUP) +
            (fo.alpha_sedikit *CENTER_SEDIKIT)  +
            (fo.alpha_cukup   *CENTER_CUKUP)    +
            (fo.alpha_penuh   *CENTER_PENUH);
  float q =  fo.alpha_tertutup + fo.alpha_sedikit +
             fo.alpha_cukup    + fo.alpha_penuh;
  if (q == 0.0) return 20.0;
  return constrain(p/q, 20.0, 160.0);
}

// =============================================
// BLYNK — TERIMA PERINTAH DARI APP
// =============================================

BLYNK_WRITE(VP_MODE) {
  modeManual = param.asInt();
  Serial.print("[BLYNK] Mode: ");
  Serial.println(modeManual ? "MANUAL" : "OTOMATIS");

  // Saat switch ke manual, sinkronkan posisi slider
  // ke sudut servo yang sedang aktif
  if (modeManual) {
    Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  }
}

// Slider — geser bebas 20-160°
// Hanya aktif saat mode manual
BLYNK_WRITE(VP_SLIDER) {
  if (!modeManual) return;  // abaikan kalau mode otomatis
  sudutManual = param.asInt();
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO, (int)sudutManual);
  Serial.print("[BLYNK] Slider: ");
  Serial.print((int)sudutManual);
  Serial.println("°");
}

// Button preset — shortcut sudut tertentu
// Saat ditekan, juga update posisi slider di app
BLYNK_WRITE(VP_BTN_TUTUP) {
  if (!modeManual || !param.asInt()) return;
  sudutManual = 20.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);  // sync slider
  Serial.println("[BLYNK] Preset: TUTUP (20°)");
}

BLYNK_WRITE(VP_BTN_SEDIKIT) {
  if (!modeManual || !param.asInt()) return;
  sudutManual = 70.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: SEDIKIT TERBUKA (70°)");
}

BLYNK_WRITE(VP_BTN_CUKUP) {
  if (!modeManual || !param.asInt()) return;
  sudutManual = 110.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: CUKUP TERBUKA (110°)");
}

BLYNK_WRITE(VP_BTN_PENUH) {
  if (!modeManual || !param.asInt()) return;
  sudutManual = 142.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: TERBUKA PENUH (142°)");
}

// =============================================
// KIRIM DATA KE BLYNK (dipanggil timer)
// =============================================
void kirimKeBlynk() {
  counter++;

  int adc1 = bacaSensor(SENSOR_1);
  int adc2 = bacaSensor(SENSOR_2);
  int adc3 = bacaSensor(SENSOR_3);
  int adc4 = bacaSensor(SENSOR_4);

  float cm1 = interpolasi(adc1, kalS1, kalSize);
  float cm2 = interpolasi(adc2, kalS2, kalSize);
  float cm3 = interpolasi(adc3, kalS3, kalSize);
  float cm4 = interpolasi(adc4, kalS4, kalSize);

  Blynk.virtualWrite(VP_S1, cm1);
  Blynk.virtualWrite(VP_S2, cm2);
  Blynk.virtualWrite(VP_S3, cm3);
  Blynk.virtualWrite(VP_S4, cm4);

  Serial.println("---------------------------------");
  Serial.print("["); Serial.print(counter); Serial.println("]");
  Serial.print("TINGGI  : ");
  Serial.print(cm1,2); Serial.print("\t");
  Serial.print(cm2,2); Serial.print("\t");
  Serial.print(cm3,2); Serial.print("\t");
  Serial.println(cm4,2);

  if (modeManual) {
    Serial.println("MODE: MANUAL");
    Blynk.virtualWrite(VP_RATA,    0);
    Blynk.virtualWrite(VP_SELISIH, 0);
    Blynk.virtualWrite(VP_SERVO,   (int)sudutManual);
    return;
  }

  if (cekDarurat(cm1, cm2, cm3, cm4)) {
    servoTulis(20.0);
    Blynk.virtualWrite(VP_SERVO,   20);
    Blynk.virtualWrite(VP_RATA,    0);
    Blynk.virtualWrite(VP_SELISIH, 0);
    Serial.print("!! DARURAT S"); Serial.print(sensorPemicu);
    Serial.println(" >= 3.8 → TUTUP PAKSA");
    return;
  }

  float rata    = (cm1+cm2+cm3+cm4)/4.0;
  float maks    = max(cm1, max(cm2, max(cm3, cm4)));
  float minim   = min(cm1, min(cm2, min(cm3, cm4)));
  float selisih = maks - minim;

  FuzzyOutput fo = evaluasiRules(rata, selisih);
  float sudut   = defuzzifikasi(fo);
  servoTulis(sudut);

  Blynk.virtualWrite(VP_RATA,    rata);
  Blynk.virtualWrite(VP_SELISIH, selisih);
  Blynk.virtualWrite(VP_SERVO,   (int)sudut);

  Serial.print("Rata    : "); Serial.print(rata,2);    Serial.println(" cm");
  Serial.print("Selisih : "); Serial.print(selisih,2); Serial.println(" cm");
  Serial.print("Alpha   : tutup="); Serial.print(fo.alpha_tertutup,3);
  Serial.print(" sedikit=");        Serial.print(fo.alpha_sedikit,3);
  Serial.print(" cukup=");          Serial.print(fo.alpha_cukup,3);
  Serial.print(" penuh=");          Serial.println(fo.alpha_penuh,3);
  Serial.print(">> SERVO : "); Serial.print(sudut,1); Serial.println("°");
}

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  servoInit();

  Serial.println("Menghubungkan ke WiFi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  Serial.println("Terhubung!");

  Serial.println("Warmup sensor...");
  for (int i = 0; i < 5; i++) {
    bacaSensor(SENSOR_1); bacaSensor(SENSOR_2);
    bacaSensor(SENSOR_3); bacaSensor(SENSOR_4);
    delay(500);
  }

  timer.setInterval(2000L, kirimKeBlynk);

  Blynk.virtualWrite(VP_MODE,   0);   // mulai otomatis
  Blynk.virtualWrite(VP_SLIDER, 20);  // slider di posisi tutup

  Serial.println("=================================");
  Serial.println(" SISTEM IRIGASI FUZZY + BLYNK  ");
  Serial.println("=================================");
}

// =============================================
// LOOP
// =============================================
void loop() {
  Blynk.run();
  timer.run();
}