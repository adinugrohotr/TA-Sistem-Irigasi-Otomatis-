#define BLYNK_TEMPLATE_ID "TMPL6zilRVja-"
#define BLYNK_TEMPLATE_NAME "Servo Test"
#define BLYNK_AUTH_TOKEN "pDImzMwqWlzdvZTuHzm-mMFSrRga2BAh"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

char ssid[] = "Kos madani 2";
char pass[] = "Iqbal2003";

Servo myServo;

#define SERVO_PIN 18

BLYNK_WRITE(V0) {
  int angle = param.asInt();

  angle = constrain(angle, 20, 160);

  myServo.write(angle);

  Serial.print("Sudut: ");
  Serial.println(angle);
}

void setup() {
  Serial.begin(115200);

  Serial.println("Boot");

  myServo.attach(SERVO_PIN);

  myServo.write(20);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");

  Blynk.config(BLYNK_AUTH_TOKEN);

  Serial.println("Connect Blynk");

  if (Blynk.connect()) {
    Serial.println("Blynk Connected");
  } else {
    Serial.println("Blynk Failed");
  }
}

void loop() {
  Blynk.run();
}