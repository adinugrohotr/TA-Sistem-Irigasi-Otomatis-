#include <Arduino.h>

// ============================================================
// KALIBRASI SENSOR AIR — VERSI ANTI DRIFT
//
// Konsep:
// 1. ADC harus naik dulu
// 2. Setelah naik → cek apakah:
//      - perubahan per detik kecil
//      - drift total beberapa detik kecil
// 3. Kalau lolos → countdown settling
// 4. Kalau drift lagi → reset
// ============================================================

// =============================================
// KONFIGURASI
// =============================================
#define SENSOR_AKTIF  3
#define NAMA_SENSOR   "S4"

#define N_SAMPLE 200

// ---------- Deteksi naik ----------
#define TOLERANSI_NAIK 5

// ---------- Stabilitas ----------
#define WINDOW_STABIL         5

// Perubahan MAKS tiap detik
#define TOLERANSI_PER_DETIK   3

// Drift total selama WINDOW_STABIL detik
#define TOLERANSI_TOTAL       10

// ---------- Settling ----------
#define SETTLING_SETELAH      15

#define TOLERANSI_COUNTDOWN 20
// =============================================
// TITIK KALIBRASI
// 0.0 cm dilewati
// =============================================
float titikKal[] = {
  1.0,
  1.5,
  2.0,
  2.5,
  3.0,
  3.5,
  4.0
};

int jumlahTitik = 7;

// =============================================
// BACA SENSOR ROBUST
// =============================================
int bacaSensorRobust(int pin) {

  int samples[N_SAMPLE];

  for (int i = 0; i < N_SAMPLE; i++) {

    samples[i] = analogRead(pin);
    delay(2);
  }

  // Sort
  for (int i = 0; i < N_SAMPLE - 1; i++) {

    for (int j = 0; j < N_SAMPLE - i - 1; j++) {

      if (samples[j] > samples[j + 1]) {

        int tmp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = tmp;
      }
    }
  }

  long total = 0;

  int dari   = N_SAMPLE / 4;
  int sampai = 3 * N_SAMPLE / 4;

  for (int i = dari; i < sampai; i++) {
    total += samples[i];
  }

  return total / (sampai - dari);
}

// =============================================
// TUNGGU STABIL
// =============================================
void tungguStabil(int pin) {

  Serial.println("");
  Serial.println("  Pantau ADC...");
  Serial.println("  Tahap:");
  Serial.println("  1. ADC harus naik");
  Serial.println("  2. Drift harus kecil");
  Serial.println("");

  int bacaSebelumnya = analogRead(pin);

  bool sudahNaik = false;

  // Untuk window stabil
  int nilaiAwalWindow = 0;
  int counterWindow   = 0;

  while (true) {

    delay(1000);

    int bacaSekarang = analogRead(pin);

    int delta = bacaSekarang - bacaSebelumnya;
    int deltaAbs = abs(delta);

    Serial.print("  [");
    Serial.print(bacaSekarang);
    Serial.print("] ");

    // =====================================================
    // FASE 1 — tunggu naik
    // =====================================================
    if (!sudahNaik) {

      if (delta >= TOLERANSI_NAIK) {

        sudahNaik = true;

        nilaiAwalWindow = bacaSekarang;
        counterWindow = 0;

        Serial.print("NAIK +");
        Serial.print(delta);
        Serial.println(" → mulai cek drift");

      } else {

        if (delta < 0) {

          Serial.print("turun ");
          Serial.println(delta);

        } else {

          Serial.print("belum naik (");
          Serial.print(delta);
          Serial.println(")");
        }
      }
    }

    // =====================================================
    // FASE 2 — cek drift
    // =====================================================
    else {

      // Cek perubahan lokal
      if (deltaAbs <= TOLERANSI_PER_DETIK) {

        counterWindow++;

        int driftTotal =
          abs(bacaSekarang - nilaiAwalWindow);

        Serial.print("delta ok ");
        Serial.print(deltaAbs);

        Serial.print(" | drift ");
        Serial.print(driftTotal);

        Serial.print(" | ");
        Serial.print(counterWindow);
        Serial.print("/");
        Serial.println(WINDOW_STABIL);

        // Drift total terlalu besar
        if (driftTotal > TOLERANSI_TOTAL) {

          Serial.println("    drift total terlalu besar → reset");

          counterWindow = 0;
          nilaiAwalWindow = bacaSekarang;
        }

      } else {

        Serial.print("masih berubah (");
        Serial.print(deltaAbs);
        Serial.println(") → reset");

        counterWindow = 0;

        nilaiAwalWindow = bacaSekarang;
      }

      // =================================================
      // Stabil cukup lama
      // =================================================
      if (counterWindow >= WINDOW_STABIL) {

        Serial.println("");
        Serial.print("  Stabil terdeteksi!");
        Serial.print(" Countdown ");
        Serial.print(SETTLING_SETELAH);
        Serial.println(" detik...");

        bool gagal = false;

        int lastCountdown = bacaSekarang;

        for (int s = SETTLING_SETELAH; s > 0; s--) {

          delay(1000);

          int cek = bacaSensorRobust(pin);

          int diff = abs(cek - lastCountdown);

          Serial.print("  ");
          Serial.print(s);
          Serial.print(".. [");
          Serial.print(cek);
          Serial.print("]");

          // Kalau tiba-tiba drift/goyang lagi
          if (diff > TOLERANSI_COUNTDOWN) {

            Serial.println(" GOYANG → ulang");

            gagal = true;

            sudahNaik = false;
            counterWindow = 0;

            nilaiAwalWindow = cek;
            bacaSebelumnya = cek;

            break;
          }

          Serial.println(" ok");

          lastCountdown = cek;
        }

        // Kalau sukses
        if (!gagal) {

          Serial.println("");
          Serial.println("  >> SIAP AMBIL DATA!");
          Serial.println("");

          return;
        }
      }
    }

    bacaSebelumnya = bacaSekarang;
  }
}

// =============================================
// VERIFIKASI 3X
// =============================================
void bacaTigaKali(int pin) {

  Serial.println("  Verifikasi 3x:");

  int hasil[3];

  for (int i = 0; i < 3; i++) {

    hasil[i] = bacaSensorRobust(pin);

    Serial.print("    ke-");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(hasil[i]);

    delay(300);
  }

  int rentang =
    max(hasil[0], max(hasil[1], hasil[2])) -
    min(hasil[0], min(hasil[1], hasil[2]));

  Serial.print("  Rentang: ");
  Serial.print(rentang);

  if (rentang <= 20) {

    Serial.println(" → STABIL ✓");

  } else if (rentang <= 50) {

    Serial.println(" → CUKUP STABIL");

  } else {

    Serial.println(" → TIDAK STABIL ⚠");
  }
}

// =============================================
// SETUP
// =============================================
void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("========================================");
  Serial.print("  KALIBRASI SENSOR ");
  Serial.println(NAMA_SENSOR);
  Serial.println("  (ANTI DRIFT VERSION)");
  Serial.println("========================================");

  Serial.println("PASTIKAN servo TIDAK tersambung!");
  Serial.println("");

  Serial.println("Tekan Enter untuk mulai...");

  while (!Serial.available()) delay(100);

  while (Serial.available()) {
    Serial.read();
  }

  // Warmup ADC
  Serial.println("");
  Serial.println("Warmup ADC...");

  for (int i = 0; i < 15; i++) {

    analogRead(SENSOR_AKTIF);
    delay(200);
  }

  Serial.println("");
  Serial.println("--- MULAI KALIBRASI ---");
  Serial.println("");

  Serial.print("float kal");
  Serial.print(NAMA_SENSOR);
  Serial.println("[][2] = {");

  // =============================================
  // LOOP KALIBRASI
  // =============================================
  for (int t = 0; t < jumlahTitik; t++) {

    float targetCm = titikKal[t];

    Serial.println("----------------------------------------");

    Serial.print("TITIK ");
    Serial.print(t + 1);
    Serial.print("/");
    Serial.print(jumlahTitik);

    Serial.print(" → ");
    Serial.print(targetCm, 1);
    Serial.println(" cm");

    Serial.println("");
    Serial.println("  Tuang air mendekati target");
    Serial.println("  lalu tekan Enter");
    Serial.println("");

    while (!Serial.available()) delay(100);

    while (Serial.available()) {
      Serial.read();
    }

    // =========================================
    // Tunggu stabil
    // =========================================
    tungguStabil(SENSOR_AKTIF);

    // =========================================
    // Ambil data final
    // =========================================
    int adcFinal = bacaSensorRobust(SENSOR_AKTIF);

    bacaTigaKali(SENSOR_AKTIF);

    Serial.println("");

    Serial.print("  >> ADC final: ");
    Serial.println(adcFinal);

    Serial.print("  {");
    Serial.print(adcFinal);
    Serial.print(", ");
    Serial.print(targetCm, 1);
    Serial.print("}");

    if (t < jumlahTitik - 1) {
      Serial.print(",");
    }

    Serial.println("");

    // =========================================
    // Lanjut
    // =========================================
    if (t < jumlahTitik - 1) {

      Serial.println("");

      Serial.print("Tekan Enter untuk lanjut ke ");
      Serial.print(titikKal[t + 1], 1);
      Serial.println(" cm...");

      while (!Serial.available()) delay(100);

      while (Serial.available()) {
        Serial.read();
      }
    }
  }

  Serial.println("};");

  Serial.println("");
  Serial.println("========================================");
  Serial.println("  KALIBRASI SELESAI");
  Serial.println("========================================");
}

// =============================================
// LOOP
// =============================================
void loop() {

}