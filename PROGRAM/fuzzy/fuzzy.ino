#include <Arduino.h>
#include <ESP32Servo.h>

// ============================================================
// SISTEM IRIGASI FUZZY MAMDANI
// VERSION:
// - Sensor robust reading
// - Median + average tengah
// - Tinggal isi nilai ADC kalibrasi sendiri
// ============================================================

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
#define SENSOR_1 4
#define SENSOR_2 6
#define SENSOR_3 5
#define SENSOR_4 3

#define SERVO_PIN 18

#define JUMLAH_BACA 100

#define BATAS_DARURAT 3.8

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
// ISI SENDIRI NILAI ADC HASIL KALIBRASI
// FORMAT:
// {ADC, cm}
// =============================================

float kalS1[][2] = {

  {0, 0.0},
  {648, 0.5},
  {812, 1.0},
  {1000, 1.5},
  {1147, 2.0},
  {1300, 2.5},
  {1500, 3.0},
  {1650, 3.5},
  {1710, 4.0}
};

float kalS2[][2] = {

  {0, 0.0},
  {648, 0.5},
  {720, 1.0},
  {900, 1.5},
  {1122, 2.0},
  {1270, 2.5},
  {1300, 3.0},
  {1455, 3.5},
  {1550, 4.0}
};

float kalS3[][2] = {

  {0, 0.0},
  {650, 0.5},
  {832, 1.0},
  {1192, 1.5},
  {1276, 2.0},
  {1477, 2.5},
  {1747, 3.0},
  {1800, 3.5},
  {1900, 4.0}
};

float kalS4[][2] = {

  {0, 0.0},
  {650, 0.5},
  {800, 1.0},
  {1200, 1.5},
  {1265, 2.0},
  {1410, 2.5},
  {1681, 3.0},
  {1720, 3.5},
  {1790, 4.0}
};

int kalSize = 9;

// =============================================
// INTERPOLASI ADC → CM
// =============================================
float interpolasi(float adc, float tabel[][2], int size) {

  if (adc <= tabel[0][0]) {
    return 0.0;
  }

  if (adc >= tabel[size - 1][0]) {
    return 4.0;
  }

  for (int i = 0; i < size - 1; i++) {

    if (adc >= tabel[i][0] &&
        adc <= tabel[i + 1][0]) {

      float ratio =
        (adc - tabel[i][0]) /
        (tabel[i + 1][0] - tabel[i][0]);

      return tabel[i][1] +
             ratio *
             (tabel[i + 1][1] - tabel[i][1]);
    }
  }

  return 0.0;
}

// =============================================
// BACA SENSOR ROBUST
// Median 50% tengah
// =============================================
int bacaSensor(int pin) {

  int samples[JUMLAH_BACA];

  // Ambil sample
  for (int i = 0; i < JUMLAH_BACA; i++) {

    samples[i] = analogRead(pin);

    delay(1);
  }

  // Sort
  for (int i = 0; i < JUMLAH_BACA - 1; i++) {

    for (int j = 0; j < JUMLAH_BACA - i - 1; j++) {

      if (samples[j] > samples[j + 1]) {

        int tmp = samples[j];

        samples[j] = samples[j + 1];
        samples[j + 1] = tmp;
      }
    }
  }

  // Ambil 50% tengah
  long total = 0;

  int dari   = JUMLAH_BACA / 4;
  int sampai = 3 * JUMLAH_BACA / 4;

  for (int i = dari; i < sampai; i++) {

    total += samples[i];
  }

  return total / (sampai - dari);
}

// =============================================
// CEK DARURAT
// =============================================
int sensorPemicu = 0;

bool cekDarurat(float cm1,
                float cm2,
                float cm3,
                float cm4) {

  if (cm1 >= BATAS_DARURAT) {
    sensorPemicu = 1;
    return true;
  }

  if (cm2 >= BATAS_DARURAT) {
    sensorPemicu = 2;
    return true;
  }

  if (cm3 >= BATAS_DARURAT) {
    sensorPemicu = 3;
    return true;
  }

  if (cm4 >= BATAS_DARURAT) {
    sensorPemicu = 4;
    return true;
  }

  sensorPemicu = 0;

  return false;
}

// =============================================
// MEMBERSHIP FUNCTION
// =============================================
float mf_rendah(float h) {

  if (h <= 0.8) return 1.0;
  if (h >= 2.0) return 0.0;

  return (2.0 - h) / (2.0 - 0.8);
}

float mf_sedang(float h) {

  if (h <= 1.5 || h >= 3.5) return 0.0;

  if (h <= 2.5) {

    return (h - 1.5) /
           (2.5 - 1.5);
  }

  return (3.5 - h) /
         (3.5 - 2.5);
}

float mf_tinggi(float h) {

  if (h <= 3.0) return 0.0;
  if (h >= 3.5) return 1.0;

  return (h - 3.0) /
         (3.5 - 3.0);
}

float mf_kecil(float d) {

  if (d <= 0.4) return 1.0;
  if (d >= 0.8) return 0.0;

  return (0.8 - d) /
         (0.8 - 0.4);
}

float mf_selisih_sedang(float d) {

  if (d <= 0.6 || d >= 1.4) return 0.0;

  if (d <= 1.0) {

    return (d - 0.6) /
           (1.0 - 0.6);
  }

  return (1.4 - d) /
         (1.4 - 1.0);
}

float mf_besar(float d) {

  if (d <= 1.2) return 0.0;
  if (d >= 1.6) return 1.0;

  return (d - 1.2) /
         (1.6 - 1.2);
}

// =============================================
// RULE EVALUATION
// =============================================
FuzzyOutput evaluasiRules(float h, float d) {

  float r  = mf_rendah(h);
  float sm = mf_sedang(h);
  float t  = mf_tinggi(h);

  float k  = mf_kecil(d);
  float sd = mf_selisih_sedang(d);
  float b  = mf_besar(d);

  float a1 = min(r,  k);
  float a2 = min(r,  sd);
  float a3 = min(r,  b);

  float a4 = min(sm, k);
  float a5 = min(sm, sd);
  float a6 = min(sm, b);

  float a7 = min(t,  k);
  float a8 = min(t,  sd);
  float a9 = min(t,  b);

  FuzzyOutput fo;

  fo.alpha_penuh =
    max(a1, a2);

  fo.alpha_cukup =
    max(a3, a4);

  fo.alpha_sedikit =
    max(a5, a6);

  fo.alpha_tertutup =
    max(a7, max(a8, a9));

  return fo;
}

// =============================================
// DEFUZZIFIKASI
// =============================================
const float CENTER_TERTUTUP = 27.5;
const float CENTER_SEDIKIT  = 70.0;
const float CENTER_CUKUP    = 110.0;
const float CENTER_PENUH    = 142.5;

float defuzzifikasi(FuzzyOutput fo) {

  float pembilang =

    (fo.alpha_tertutup * CENTER_TERTUTUP) +
    (fo.alpha_sedikit  * CENTER_SEDIKIT)  +
    (fo.alpha_cukup    * CENTER_CUKUP)    +
    (fo.alpha_penuh    * CENTER_PENUH);

  float penyebut =

    fo.alpha_tertutup +
    fo.alpha_sedikit  +
    fo.alpha_cukup    +
    fo.alpha_penuh;

  if (penyebut == 0.0) {

    return 20.0;
  }

  return constrain(
    pembilang / penyebut,
    20.0,
    160.0
  );
}

// =============================================
// SETUP
// =============================================
int counter = 0;

void setup() {

  Serial.begin(115200);

  analogReadResolution(12);

  analogSetAttenuation(ADC_11db);

  servoInit();

  Serial.println("");
  Serial.println("=================================");
  Serial.println(" SISTEM IRIGASI FUZZY MAMDANI ");
  Serial.println("=================================");

  Serial.println("Warmup sensor...");

  for (int i = 0; i < 5; i++) {

    bacaSensor(SENSOR_1);
    bacaSensor(SENSOR_2);
    bacaSensor(SENSOR_3);
    bacaSensor(SENSOR_4);

    delay(500);
  }

  Serial.println("Sistem siap.");
}

// =============================================
// LOOP
// =============================================
void loop() {

  counter++;

  // =========================================
  // BACA ADC
  // =========================================
  int adc1 = bacaSensor(SENSOR_1);
  int adc2 = bacaSensor(SENSOR_2);
  int adc3 = bacaSensor(SENSOR_3);
  int adc4 = bacaSensor(SENSOR_4);

  // =========================================
  // KONVERSI ADC → CM
  // =========================================
  float cm1 = interpolasi(adc1, kalS1, kalSize);
  float cm2 = interpolasi(adc2, kalS2, kalSize);
  float cm3 = interpolasi(adc3, kalS3, kalSize);
  float cm4 = interpolasi(adc4, kalS4, kalSize);

  // =========================================
  // TAMPILKAN SENSOR
  // =========================================
  Serial.println("");
  Serial.println("---------------------------------");

  Serial.print("[");
  Serial.print(counter);
  Serial.println("]");

  Serial.print("RAW ADC : ");

  Serial.print(adc1);
  Serial.print("\t");

  Serial.print(adc2);
  Serial.print("\t");

  Serial.print(adc3);
  Serial.print("\t");

  Serial.println(adc4);

  Serial.print("TINGGI  : ");

  Serial.print(cm1, 2);
  Serial.print("\t");

  Serial.print(cm2, 2);
  Serial.print("\t");

  Serial.print(cm3, 2);
  Serial.print("\t");

  Serial.println(cm4, 2);

  // =========================================
  // CEK DARURAT
  // =========================================
  if (cekDarurat(cm1, cm2, cm3, cm4)) {

    servoTulis(20.0);

    Serial.print("!! DARURAT SENSOR S");
    Serial.print(sensorPemicu);

    Serial.println(" >= 3.8 cm");

    Serial.println("SERVO TUTUP PAKSA");

    delay(2000);

    return;
  }

  // =========================================
  // FUZZY
  // =========================================
  float rata =

    (cm1 + cm2 + cm3 + cm4) / 4.0;

  float maks =

    max(cm1,
    max(cm2,
    max(cm3, cm4)));

  float minim =

    min(cm1,
    min(cm2,
    min(cm3, cm4)));

  float selisih = maks - minim;

  FuzzyOutput fo =

    evaluasiRules(rata, selisih);

  float sudut =

    defuzzifikasi(fo);

  servoTulis(sudut);

  // =========================================
  // OUTPUT
  // =========================================
  Serial.print("Rata    : ");
  Serial.print(rata, 2);
  Serial.println(" cm");

  Serial.print("Selisih : ");
  Serial.print(selisih, 2);
  Serial.println(" cm");

  Serial.print("Alpha   : ");

  Serial.print("tutup=");
  Serial.print(fo.alpha_tertutup, 3);

  Serial.print(" sedikit=");
  Serial.print(fo.alpha_sedikit, 3);

  Serial.print(" cukup=");
  Serial.print(fo.alpha_cukup, 3);

  Serial.print(" penuh=");
  Serial.println(fo.alpha_penuh, 3);

  Serial.print(">> SERVO : ");
  Serial.print(sudut, 1);
  Serial.println(" derajat");

  delay(2000);
}