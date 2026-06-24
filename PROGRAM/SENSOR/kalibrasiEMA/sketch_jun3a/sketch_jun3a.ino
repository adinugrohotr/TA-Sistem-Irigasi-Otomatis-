#include <Arduino.h>

#define SENSOR_AKTIF 3       // ganti sesuai sensor yang dikalibrasi
#define NAMA_SENSOR  "S1"    // ganti S1/S2/S3/S4
#define N_SAMPLE     100
#define ALPHA_EMA    0.2f    // smoothing EMA

float ema = 0;
bool  ema_init = false;

// =============================================
// BACA MEDIAN 50% TENGAH
// =============================================
int bacaMedian(int pin) {
  int samples[N_SAMPLE];
  for (int i = 0; i < N_SAMPLE; i++) {
    samples[i] = analogRead(pin);
    delay(2);
  }
  for (int i = 0; i < N_SAMPLE-1; i++)
    for (int j = 0; j < N_SAMPLE-i-1; j++)
      if (samples[j] > samples[j+1]) {
        int t=samples[j]; samples[j]=samples[j+1]; samples[j+1]=t;
      }
  long total = 0;
  int dari   = N_SAMPLE/4;
  int sampai = 3*N_SAMPLE/4;
  for (int i = dari; i < sampai; i++) total += samples[i];
  return total/(sampai-dari);
}

// =============================================
// BACA DENGAN EMA — panggil berulang sampai stabil
// =============================================
float bacaEMA(int pin) {
  int median = bacaMedian(pin);
  if (!ema_init) {
    ema = median;
    ema_init = true;
  } else {
    ema = ALPHA_EMA * median + (1 - ALPHA_EMA) * ema;
  }
  return ema;
}

// =============================================
// TUNGGU EMA STABIL
// Baca terus sampai perubahan EMA < toleransi
// selama N detik berturut-turut
// =============================================
int tungguStabilEMA(int pin) {
  const float TOLERANSI = 3.0;  // perubahan EMA max dianggap stabil
  const int   DETIK_OK  = 8;    // harus stabil N detik berturut-turut

  int count = 0;
  float emaSebel = bacaEMA(pin);

  Serial.println("  Menunggu EMA stabil...");

  while (true) {
    delay(1000);
    float emaBaru = bacaEMA(pin);
    float delta   = abs(emaBaru - emaSebel);

    Serial.print("  EMA="); Serial.print(emaBaru, 1);
    Serial.print("  delta="); Serial.print(delta, 1);

    if (delta <= TOLERANSI) {
      count++;
      Serial.print("  stabil ");
      for (int i = 0; i < count; i++) Serial.print("|");
      Serial.print(" ("); Serial.print(count);
      Serial.print("/"); Serial.print(DETIK_OK); Serial.println(")");
    } else {
      count = 0;
      Serial.println("  bergerak — reset");
    }

    emaSebel = emaBaru;

    if (count >= DETIK_OK) {
      Serial.println("  >> EMA stabil!");
      return (int)emaBaru;
    }
  }
}

// =============================================
// TITIK KALIBRASI
// =============================================
float titikKal[] = {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
int   jumlahTitik = 9;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Reset EMA tiap titik baru
  ema_init = false;

  Serial.println("========================================");
  Serial.print("  KALIBRASI "); Serial.println(NAMA_SENSOR);
  Serial.println("  Median + EMA — siap copy-paste");
  Serial.println("========================================");
  Serial.println("Pastikan servo TIDAK tersambung!");
  Serial.println("Tekan Enter untuk mulai...");
  while (!Serial.available()) delay(100);
  while (Serial.available())  Serial.read();

  // Warmup
  Serial.println("Warmup...");
  ema_init = false;
  for (int i = 0; i < 20; i++) {
    bacaEMA(SENSOR_AKTIF);
    delay(200);
  }

  Serial.println("\n--- MULAI ---\n");
  Serial.print("float kal"); Serial.print(NAMA_SENSOR);
  Serial.println("[][2] = {");

  for (int t = 0; t < jumlahTitik; t++) {
    float target = titikKal[t];

    // Reset EMA setiap titik supaya tidak carry-over dari titik sebelumnya
    ema_init = false;

    Serial.println("----------------------------------------");
    Serial.print("TITIK "); Serial.print(t+1);
    Serial.print("/"); Serial.print(jumlahTitik);
    Serial.print(": Atur air ke "); Serial.print(target, 1);
    Serial.println(" cm");

    if (target == 0.0) {
      Serial.println("  (Kondisi kering — langsung baca)");
      delay(3000);
      // Warmup EMA untuk kondisi kering
      for (int i = 0; i < 10; i++) { bacaEMA(SENSOR_AKTIF); delay(200); }
    } else {
      Serial.println("  Tuang air ke target, tekan Enter saat siap...");
      while (!Serial.available()) delay(100);
      while (Serial.available())  Serial.read();
      // Warmup EMA dulu setelah air dituang
      Serial.println("  Warmup EMA 5 detik...");
      for (int i = 0; i < 10; i++) { bacaEMA(SENSOR_AKTIF); delay(500); }
    }

    // Tunggu benar-benar stabil
    int adcFinal = tungguStabilEMA(SENSOR_AKTIF);

    // Verifikasi 3x baca
    Serial.println("  Verifikasi 3x:");
    int v[3];
    for (int k = 0; k < 3; k++) {
      v[k] = (int)bacaEMA(SENSOR_AKTIF);
      Serial.print("    "); Serial.print(k+1);
      Serial.print(": "); Serial.println(v[k]);
      delay(1000);
    }
    int rentang = max(v[0],max(v[1],v[2])) - min(v[0],min(v[1],v[2]));
    Serial.print("  Rentang: "); Serial.print(rentang);
    Serial.println(rentang <= 10 ? " → OK ✓" :
                   rentang <= 25 ? " → Cukup" : " → Kurang stabil ⚠");

    // Output siap copy-paste
    Serial.print("  {"); Serial.print(adcFinal);
    Serial.print(", "); Serial.print(target, 1); Serial.print("}");
    if (t < jumlahTitik-1) Serial.print(",");
    Serial.println("");

    if (t < jumlahTitik-1) {
      Serial.print("\nEnter untuk lanjut ke ");
      Serial.print(titikKal[t+1], 1); Serial.println(" cm...");
      while (!Serial.available()) delay(100);
      while (Serial.available())  Serial.read();
    }
  }

  Serial.println("};");
  Serial.println("\n========================================");
  Serial.println("  SELESAI — copy blok di atas ke kode utama");
  Serial.println("========================================");
}

void loop() {}