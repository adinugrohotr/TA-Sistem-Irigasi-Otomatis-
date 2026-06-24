#include <ESP32Servo.h>

Servo myServo;
int pin = 18;
int sudutSekarang = 20; // posisi awal

void setup() {
  Serial.begin(115200);
  myServo.attach(pin);
  
  // Langsung ke posisi 20 saat upload
  myServo.write(sudutSekarang);
  Serial.println("Servo ready di posisi 20°");
  Serial.println("Ketik sudut (20-160) lalu Enter:");
}

void loop() {
  if (Serial.available()) {
    int sudut = Serial.parseInt();
    
    if (sudut >= 20 && sudut <= 160) {
      sudutSekarang = sudut;
      myServo.write(sudutSekarang);
      Serial.print("Servo bergerak ke: ");
      Serial.print(sudutSekarang);
      Serial.println("°");
      Serial.println("Ketik sudut berikutnya (20-160):");
    } else {
      Serial.println("❌ Diluar range! Masukkan nilai 20-160");
      Serial.print("Servo tetap di posisi: ");
      Serial.print(sudutSekarang);
      Serial.println("°");
    }
  }
}