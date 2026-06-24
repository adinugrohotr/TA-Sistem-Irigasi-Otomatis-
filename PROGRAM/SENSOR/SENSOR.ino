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
#define VP_MODE         V7
#define VP_BTN_TUTUP    V8
#define VP_BTN_SEDIKIT  V9
#define VP_BTN_CUKUP    V10
#define VP_BTN_PENUH    V11
#define VP_SLIDER       V12

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
// KONSTANTA MODE KALIBRASI
// =============================================
#define DURASI_STABILISASI  20000   // 90 detik dalam ms
#define DURASI_REKAM        5000   // 30 detik dalam ms

// =============================================
// VARIABEL GLOBAL
// =============================================
bool  modeManual    = false;
float sudutManual   = 20.0;
int   sensorPemicu  = 0;
int   counter       = 0;
bool  statusDarurat = false;

const float ALPHA_EMA = 0.2;
float ema1 = -1, ema2 = -1, ema3 = -1, ema4 = -1;

BlynkTimer timer;

// Variabel mode kalibrasi
unsigned long waktuMulai       = 0;
bool          kalibrasSelesai  = false;
bool          faseRekam        = false;
long          sumAdc1          = 0;
long          sumAdc2          = 0;
long          sumAdc3          = 0;
long          sumAdc4          = 0;
int           jumlahSampelKal  = 0;
unsigned long waktuMulaiRekam  = 0;

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
  {23,   0.0}, {418,  0.5}, {624,  1.0},
  {975,  1.5}, {1280, 2.0}, {1394, 2.5},
  {1443, 3.0}, {1491, 3.5}, {1509, 4.0}
};
float kalS2[][2] = {
  {41,   0.0}, {337,  0.5}, {938,  1.0},
  {940,  1.5}, {1236, 2.0}, {1291, 2.5},
  {1432, 3.0}, {1493, 3.5}, {1543, 4.0}
};
float kalS3[][2] = {
  {96,   0.0}, {337,  0.5}, {733,  1.0},
  {887,  1.5}, {1156, 2.0}, {1396, 2.5},
  {1603, 3.0}, {1727, 3.5}, {1872, 4.0}
};
float kalS4[][2] = {
  {70,   0.0}, {521,  0.5}, {869,  1.0},
  {1156, 1.5}, {1209, 2.0}, {1398, 2.5},
  {1425, 3.0}, {1532, 3.5}, {1712, 4.0}
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

int bacaMedian(int pin) {
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

int bacaSensorEMA(int pin, float &ema_val) {
  int median = bacaMedian(pin);
  if (ema_val < 0) {
    ema_val = median;
  } else {
    ema_val = ALPHA_EMA * median + (1 - ALPHA_EMA) * ema_val;
  }
  return (int)ema_val;
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
// BLYNK HANDLERS
// =============================================
BLYNK_WRITE(VP_MODE) {
  if (statusDarurat) {
    Blynk.virtualWrite(VP_MODE, 0);
    Serial.println("[BLYNK] Mode switch diabaikan — DARURAT aktif");
    return;
  }
  modeManual = param.asInt();
  Serial.print("[BLYNK] Mode: ");
  Serial.println(modeManual ? "MANUAL" : "OTOMATIS");
  if (modeManual) Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
}

BLYNK_WRITE(VP_SLIDER) {
  if (statusDarurat || !modeManual) return;
  sudutManual = param.asInt();
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO, (int)sudutManual);
  Serial.print("[BLYNK] Slider: ");
  Serial.print((int)sudutManual); Serial.println("°");
}

BLYNK_WRITE(VP_BTN_TUTUP) {
  if (statusDarurat || !modeManual || !param.asInt()) return;
  sudutManual = 20.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: TUTUP (20°)");
}

BLYNK_WRITE(VP_BTN_SEDIKIT) {
  if (statusDarurat || !modeManual || !param.asInt()) return;
  sudutManual = 70.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: SEDIKIT TERBUKA (70°)");
}

BLYNK_WRITE(VP_BTN_CUKUP) {
  if (statusDarurat || !modeManual || !param.asInt()) return;
  sudutManual = 110.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: CUKUP TERBUKA (110°)");
}

BLYNK_WRITE(VP_BTN_PENUH) {
  if (statusDarurat || !modeManual || !param.asInt()) return;
  sudutManual = 142.0;
  servoTulis(sudutManual);
  Blynk.virtualWrite(VP_SERVO,  (int)sudutManual);
  Blynk.virtualWrite(VP_SLIDER, (int)sudutManual);
  Serial.println("[BLYNK] Preset: TERBUKA PENUH (142°)");
}

// =============================================
// KIRIM DATA KE BLYNK
// =============================================
void kirimKeBlynk() {
  counter++;

  // Baca median (raw sebelum EMA)
  int raw1 = bacaMedian(SENSOR_1);
  int raw2 = bacaMedian(SENSOR_2);
  int raw3 = bacaMedian(SENSOR_3);
  int raw4 = bacaMedian(SENSOR_4);

  // Terapkan EMA
  int adc1, adc2, adc3, adc4;
  if (ema1<0){ema1=raw1;}else{ema1=ALPHA_EMA*raw1+(1-ALPHA_EMA)*ema1;}
  if (ema2<0){ema2=raw2;}else{ema2=ALPHA_EMA*raw2+(1-ALPHA_EMA)*ema2;}
  if (ema3<0){ema3=raw3;}else{ema3=ALPHA_EMA*raw3+(1-ALPHA_EMA)*ema3;}
  if (ema4<0){ema4=raw4;}else{ema4=ALPHA_EMA*raw4+(1-ALPHA_EMA)*ema4;}
  adc1=(int)ema1; adc2=(int)ema2;
  adc3=(int)ema3; adc4=(int)ema4;

  float cm1 = interpolasi(adc1, kalS1, kalSize);
  float cm2 = interpolasi(adc2, kalS2, kalSize);
  float cm3 = interpolasi(adc3, kalS3, kalSize);
  float cm4 = interpolasi(adc4, kalS4, kalSize);

  Blynk.virtualWrite(VP_S1, cm1);
  Blynk.virtualWrite(VP_S2, cm2);
  Blynk.virtualWrite(VP_S3, cm3);
  Blynk.virtualWrite(VP_S4, cm4);

  // =============================================
  // LOGIKA TAMPILAN SERIAL — MODE KALIBRASI
  // Hanya mengatur apa yang ditampilkan di Serial.
  // Tidak mengubah apapun dari logika sistem.
  // =============================================
  unsigned long sekarang = millis();
  unsigned long selang   = sekarang - waktuMulai;

  if (!kalibrasSelesai) {

    if (selang < DURASI_STABILISASI) {
      // Fase 1: stabilisasi 90 detik
      // Hanya tampilkan countdown tiap 10 detik
      int sisaDetik = (DURASI_STABILISASI - selang) / 1000;
      if (sisaDetik % 10 == 0 || sisaDetik <= 5) {
        Serial.print("Stabilisasi sensor... sisa ");
        Serial.print(sisaDetik);
        Serial.println(" detik");
      }

    } else if (!faseRekam) {
      // Transisi ke fase rekam
      faseRekam       = true;
      waktuMulaiRekam = sekarang;
      sumAdc1 = sumAdc2 = sumAdc3 = sumAdc4 = 0;
      jumlahSampelKal = 0;
      Serial.println("");
      Serial.println(">> Stabilisasi selesai. Mulai rekam ADC 30 detik...");

    } else {
      // Fase 2: rekam ADC median selama 30 detik
      unsigned long selangRekam = sekarang - waktuMulaiRekam;

      if (selangRekam < DURASI_REKAM) {
        // Akumulasi ADC median (raw sebelum EMA)
        sumAdc1 += raw1;
        sumAdc2 += raw2;
        sumAdc3 += raw3;
        sumAdc4 += raw4;
        jumlahSampelKal++;

        // Tampilkan progress tiap 5 detik
        int sisaRekam = (DURASI_REKAM - selangRekam) / 1000;
        if (sisaRekam % 5 == 0 && sisaRekam > 0) {
          Serial.print("Merekam... sisa ");
          Serial.print(sisaRekam);
          Serial.print(" detik  (sampel: ");
          Serial.print(jumlahSampelKal);
          Serial.println(")");
        }

      } else {
        // Fase 3: hitung dan tampilkan hasil
        kalibrasSelesai = true;

        int rataAdc1 = (jumlahSampelKal > 0) ? (sumAdc1 / jumlahSampelKal) : 0;
        int rataAdc2 = (jumlahSampelKal > 0) ? (sumAdc2 / jumlahSampelKal) : 0;
        int rataAdc3 = (jumlahSampelKal > 0) ? (sumAdc3 / jumlahSampelKal) : 0;
        int rataAdc4 = (jumlahSampelKal > 0) ? (sumAdc4 / jumlahSampelKal) : 0;

        Serial.println("");
        Serial.println("==========================================");
        Serial.println("=== HASIL KALIBRASI ===");
        Serial.print("S1 = "); Serial.println(rataAdc1);
        Serial.print("S2 = "); Serial.println(rataAdc2);
        Serial.print("S3 = "); Serial.println(rataAdc3);
        Serial.print("S4 = "); Serial.println(rataAdc4);
        Serial.print("(dari "); Serial.print(jumlahSampelKal);
        Serial.println(" sampel)");
        Serial.println("==========================================");
        Serial.println("Sistem berjalan normal.");
        Serial.println("");
      }
    }

  } else {
    // Setelah kalibrasi selesai — tampilan normal
    Serial.println("---------------------------------");
    Serial.print("["); Serial.print(counter); Serial.println("]");
    Serial.print("ADC median : ");
    Serial.print(raw1); Serial.print("\t");
    Serial.print(raw2); Serial.print("\t");
    Serial.print(raw3); Serial.print("\t");
    Serial.println(raw4);
    Serial.print("ADC EMA    : ");
    Serial.print(adc1); Serial.print("\t");
    Serial.print(adc2); Serial.print("\t");
    Serial.print(adc3); Serial.print("\t");
    Serial.println(adc4);
    Serial.print("TINGGI cm  : ");
    Serial.print(cm1,2); Serial.print("\t");
    Serial.print(cm2,2); Serial.print("\t");
    Serial.print(cm3,2); Serial.print("\t");
    Serial.println(cm4,2);
  }

  // ── CEK DARURAT — selalu jalan di semua fase ──
  bool darurat = cekDarurat(cm1, cm2, cm3, cm4);

  if (darurat) {
    if (!statusDarurat) {
      statusDarurat = true;
      modeManual    = false;
      Blynk.virtualWrite(VP_MODE,   0);
      Blynk.virtualWrite(VP_SLIDER, 20);
      Serial.println("!! MASUK DARURAT — kontrol manual diblokir");
    }
    servoTulis(20.0);
    Blynk.virtualWrite(VP_SERVO,   20);
    Blynk.virtualWrite(VP_RATA,    0);
    Blynk.virtualWrite(VP_SELISIH, 0);
    Serial.print("!! DARURAT S"); Serial.print(sensorPemicu);
    Serial.println(" >= 3.8 cm → TUTUP PAKSA");
    return;

  } else {
    if (statusDarurat) {
      statusDarurat = false;
      Serial.println(">> Darurat selesai — sistem normal");
    }
  }

  // ── MODE MANUAL ──
  if (modeManual) {
    if (kalibrasSelesai) Serial.println("MODE: MANUAL");
    Blynk.virtualWrite(VP_RATA,    0);
    Blynk.virtualWrite(VP_SELISIH, 0);
    Blynk.virtualWrite(VP_SERVO,   (int)sudutManual);
    return;
  }

  // ── MODE OTOMATIS — fuzzy ──
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

  if (kalibrasSelesai) {
    Serial.print("Rata    : "); Serial.print(rata,2);    Serial.println(" cm");
    Serial.print("Selisih : "); Serial.print(selisih,2); Serial.println(" cm");
    Serial.print("Alpha   : tutup="); Serial.print(fo.alpha_tertutup,3);
    Serial.print(" sedikit=");        Serial.print(fo.alpha_sedikit,3);
    Serial.print(" cukup=");          Serial.print(fo.alpha_cukup,3);
    Serial.print(" penuh=");          Serial.println(fo.alpha_penuh,3);
    Serial.print(">> SERVO : "); Serial.print(sudut,1); Serial.println("°");
  }
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
  for (int i = 0; i < 10; i++) {
    int r1=bacaMedian(SENSOR_1), r2=bacaMedian(SENSOR_2);
    int r3=bacaMedian(SENSOR_3), r4=bacaMedian(SENSOR_4);
    if(ema1<0){ema1=r1;}else{ema1=ALPHA_EMA*r1+(1-ALPHA_EMA)*ema1;}
    if(ema2<0){ema2=r2;}else{ema2=ALPHA_EMA*r2+(1-ALPHA_EMA)*ema2;}
    if(ema3<0){ema3=r3;}else{ema3=ALPHA_EMA*r3+(1-ALPHA_EMA)*ema3;}
    if(ema4<0){ema4=r4;}else{ema4=ALPHA_EMA*r4+(1-ALPHA_EMA)*ema4;}
    delay(500);
  }

  // Catat waktu mulai setelah warmup selesai
  waktuMulai = millis();

  timer.setInterval(2000L, kirimKeBlynk);
  Blynk.virtualWrite(VP_MODE,   0);
  Blynk.virtualWrite(VP_SLIDER, 20);

  Serial.println("=================================");
  Serial.println(" SISTEM IRIGASI FUZZY + BLYNK  ");
  Serial.println("=================================");
  Serial.println("Mode kalibrasi aktif:");
  Serial.println("  30 detik stabilisasi sensor");
  Serial.println("  5 detik rekam ADC median");
  Serial.println("  Hasil ditampilkan otomatis");
  Serial.println("=================================");
}

// =============================================
// LOOP
// =============================================
void loop() {
  Blynk.run();
  timer.run();
}