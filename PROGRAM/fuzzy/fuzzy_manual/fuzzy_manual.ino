#include <Arduino.h>
#include <ESP32Servo.h>

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
// SERVO
// =============================================
#define SERVO_PIN 18
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
// FUZZY ENGINE
// =============================================
float mf_rendah(float h) {
  if (h <= 0.8) return 1.0;
  if (h >= 2.0) return 0.0;
  return (2.0 - h) / (2.0 - 0.8);
}
float mf_sedang(float h) {
  if (h <= 1.5 || h >= 3.5) return 0.0;
  if (h <= 2.5) return (h - 1.5) / (2.5 - 1.5);
  return           (3.5 - h) / (3.5 - 2.5);
}
float mf_tinggi(float h) {
  if (h <= 3.0) return 0.0;
  if (h >= 3.5) return 1.0;
  return (h - 3.0) / (3.5 - 3.0);
}
float mf_kecil(float d) {
  if (d <= 0.4) return 1.0;
  if (d >= 0.8) return 0.0;
  return (0.8 - d) / (0.8 - 0.4);
}
float mf_selisih_sedang(float d) {
  if (d <= 0.6 || d >= 1.4) return 0.0;
  if (d <= 1.0) return (d - 0.6) / (1.0 - 0.6);
  return         (1.4 - d) / (1.4 - 1.0);
}
float mf_besar(float d) {
  if (d <= 1.2) return 0.0;
  if (d >= 1.6) return 1.0;
  return (d - 1.2) / (1.6 - 1.2);
}

FuzzyOutput evaluasiRules(float h, float d) {
  float r  = mf_rendah(h);
  float sm = mf_sedang(h);
  float t  = mf_tinggi(h);
  float k  = mf_kecil(d);
  float sd = mf_selisih_sedang(d);
  float b  = mf_besar(d);

  float a1=min(r,k);  float a2=min(r,sd);  float a3=min(r,b);
  float a4=min(sm,k); float a5=min(sm,sd); float a6=min(sm,b);
  float a7=min(t,k);  float a8=min(t,sd);  float a9=min(t,b);

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
  float p = (fo.alpha_tertutup * CENTER_TERTUTUP) +
            (fo.alpha_sedikit  * CENTER_SEDIKIT)  +
            (fo.alpha_cukup    * CENTER_CUKUP)    +
            (fo.alpha_penuh    * CENTER_PENUH);
  float q =  fo.alpha_tertutup + fo.alpha_sedikit +
             fo.alpha_cukup    + fo.alpha_penuh;
  if (q == 0.0) return 20.0;
  return constrain(p / q, 20.0, 160.0);
}

// =============================================
// PARSING INPUT SERIAL
// Format ketik di Serial Monitor:
// s1,s2,s3,s4
// Contoh: 2.0,2.5,2.2,2.6
// =============================================
float parseFloat(String s) {
  return s.toFloat();
}

void prosesInput(String input) {
  // Pisahkan 4 nilai berdasarkan koma
  float val[4] = {0,0,0,0};
  int idx = 0;
  int start = 0;

  for (int i = 0; i <= input.length(); i++) {
    if (i == input.length() || input[i] == ',') {
      if (idx < 4) {
        val[idx] = input.substring(start, i).toFloat();
        idx++;
      }
      start = i + 1;
    }
  }

  if (idx < 4) {
    Serial.println("Format salah! Ketik: s1,s2,s3,s4");
    Serial.println("Contoh: 2.0,2.5,2.2,2.6");
    return;
  }

  float cm1 = val[0], cm2 = val[1];
  float cm3 = val[2], cm4 = val[3];

  float rata    = (cm1+cm2+cm3+cm4)/4.0;
  float maks    = max(cm1, max(cm2, max(cm3, cm4)));
  float minim   = min(cm1, min(cm2, min(cm3, cm4)));
  float selisih = maks - minim;

  // Cek darurat
  bool darurat = (cm1>=3.8 || cm2>=3.8 || cm3>=3.8 || cm4>=3.8);

  // Tentukan kategori
  String kat_rata, kat_selisih, kat_output;
  if      (rata < 2.0)  kat_rata = "Rendah";
  else if (rata <= 3.0) kat_rata = "Sedang";
  else                  kat_rata = "Tinggi";

  if      (selisih < 0.6)  kat_selisih = "Kecil";
  else if (selisih <= 1.2) kat_selisih = "Sedang";
  else                     kat_selisih = "Besar";

  FuzzyOutput fo = evaluasiRules(rata, selisih);
  float sudut   = darurat ? 20.0 : defuzzifikasi(fo);

  if      (sudut <= 40)  kat_output = "Tertutup";
  else if (sudut <= 85)  kat_output = "Sedikit Terbuka";
  else if (sudut <= 125) kat_output = "Cukup Terbuka";
  else                   kat_output = "Terbuka Penuh";

  // Gerakkan servo sungguhan
  servoTulis(sudut);

  // Tampilkan hasil
  Serial.println("==========================================");
  Serial.print("INPUT   : S1="); Serial.print(cm1,1);
  Serial.print(" S2=");          Serial.print(cm2,1);
  Serial.print(" S3=");          Serial.print(cm3,1);
  Serial.print(" S4=");          Serial.println(cm4,1);
  Serial.print("Rata    : "); Serial.print(rata,2);
  Serial.print(" cm (");     Serial.print(kat_rata); Serial.println(")");
  Serial.print("Selisih : "); Serial.print(selisih,2);
  Serial.print(" cm (");     Serial.print(kat_selisih); Serial.println(")");
  Serial.println("------------------------------------------");

  if (darurat) {
    Serial.println("!! DARURAT >= 3.8 cm → SERVO TUTUP PAKSA");
  } else {
    Serial.print("Alpha   : tutup="); Serial.print(fo.alpha_tertutup,3);
    Serial.print(" sedikit=");        Serial.print(fo.alpha_sedikit,3);
    Serial.print(" cukup=");          Serial.print(fo.alpha_cukup,3);
    Serial.print(" penuh=");          Serial.println(fo.alpha_penuh,3);
  }

  Serial.print(">> SERVO FUZZY : "); Serial.print(sudut,1); Serial.println("°");
  Serial.print(">> KATEGORI    : "); Serial.println(kat_output);
  Serial.println(">> Ukur sudut aktual dengan busur sekarang!");
  Serial.println("==========================================");
  Serial.println("");
  Serial.println("Ketik nilai berikutnya (s1,s2,s3,s4):");
}

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(115200);
  servoInit();

  Serial.println("==========================================");
  Serial.println(" PENGUJIAN FUZZY - INPUT MANUAL");
  Serial.println(" Tabel 3.21 & 3.22");
  Serial.println("==========================================");
  Serial.println("Format input: s1,s2,s3,s4 lalu Enter");
  Serial.println("Contoh: 2.0,2.5,2.2,2.6");
  Serial.println("");
  Serial.println("10 skenario siap diuji:");
  Serial.println("1:  0.5,0.6,0.5,0.6   (Rendah+Kecil)");
  Serial.println("2:  0.8,1.5,1.0,1.7   (Rendah+Sedang)");
  Serial.println("3:  0.5,2.0,0.8,2.2   (Rendah+Besar)");
  Serial.println("4:  2.3,2.4,2.3,2.4   (Sedang+Kecil)");
  Serial.println("5:  2.0,2.8,2.1,2.9   (Sedang+Sedang)");
  Serial.println("6:  1.8,3.2,2.0,3.4   (Sedang+Besar)");
  Serial.println("7:  3.2,3.3,3.2,3.3   (Tinggi+Kecil)");
  Serial.println("8:  3.1,3.7,3.2,3.6   (Tinggi+Sedang)");
  Serial.println("9:  3.0,3.6,3.0,3.7   (Tinggi+Besar)");
  Serial.println("10: 2.0,3.9,2.1,2.0   (Cut-off)");
  Serial.println("==========================================");
  Serial.println("");
}

// =============================================
// LOOP
// =============================================
String inputBuffer = "";

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        prosesInput(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}