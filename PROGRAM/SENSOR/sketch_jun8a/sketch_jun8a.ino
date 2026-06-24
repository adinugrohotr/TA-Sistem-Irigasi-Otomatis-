// =============================================
// KALIBRASI 4 SENSOR - bacaMedian
// Ketinggian: 0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0 cm
// =============================================

#define SENSOR_1    4
#define SENSOR_2    6
#define SENSOR_3    5
#define SENSOR_4    3
#define JUMLAH_BACA 100

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

float tingkat[] = {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
int jumlahTitik = 9;
int titikSekarang = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("=================================");
  Serial.println("KALIBRASI 4 SENSOR SEKALIGUS");
  Serial.println("Pastikan semua sensor sudah");
  Serial.println("terpasang resistor 10k ke GND");
  Serial.println("=================================");
  Serial.println();
  Serial.println("S1\tS2\tS3\tS4\tTINGGI");
  tampilkanInstruksi();
}

void tampilkanInstruksi() {
  if (titikSekarang >= jumlahTitik) {
    Serial.println();
    Serial.println("=================================");
    Serial.println("KALIBRASI SELESAI!");
    Serial.println("Copy tabel di atas ke program utama");
    Serial.println("=================================");
    return;
  }
  Serial.println();
  Serial.print(">>> Atur air ke ");
  Serial.print(tingkat[titikSekarang], 1);
  Serial.println(" cm, tunggu stabil, tekan ENTER");
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read(); // buang semua karakter

    int adc1 = bacaMedian(SENSOR_1);
    int adc2 = bacaMedian(SENSOR_2);
    int adc3 = bacaMedian(SENSOR_3);
    int adc4 = bacaMedian(SENSOR_4);

    Serial.print(adc1); Serial.print("\t");
    Serial.print(adc2); Serial.print("\t");
    Serial.print(adc3); Serial.print("\t");
    Serial.print(adc4); Serial.print("\t");
    Serial.println(tingkat[titikSekarang], 1);

    titikSekarang++;
    tampilkanInstruksi();
  }
}