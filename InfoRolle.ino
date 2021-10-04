#include <Preferences.h>

//#include <U8g2lib.h>
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "protothreads.h"

// U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE); // WORKS!
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);

U8X8_SSD1306_128X64_NONAME_HW_I2C display(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);
//U8X8_SSD1306_96X16_ER_HW_I2C display(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);
//U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

typedef enum {
  LINKS_DREHEN, RECHTS_DREHEN, WARTEN, FEHLER
} RolleStatus;

#include "StatusLog.h"
#include "RollenSensor.h"

#define MOTOR1_PIN1 16
#define MOTOR1_PIN2 17
#define MOTOR1_PWM  5

#define MOTOR2_PIN1 18
#define MOTOR2_PIN2 19
#define MOTOR2_PWM  4

#define MOTOR_V_ZIEHEN 255
// #define MOTOR_V_ANLAUFEN 200
// #define MOTOR_V_SCHIEBEN 150
#define MOTOR_V_ANLAUFEN 0
#define MOTOR_V_SCHIEBEN 0

#define POTI_SCHWELLE_PIN  15
#define PHOTO_LINKS_PIN   34
#define PHOTO_RECHTS_PIN  35
#define START_BUTTON_PIN  14
#define STATUS_LED    1

#define CFG_NAECHSTE_RICHTUNG_LINKS "naechsteRichtungLinks"

RollenSensor rollenSensor(PHOTO_LINKS_PIN, PHOTO_RECHTS_PIN);
RolleStatus rollenStatus = WARTEN;
Preferences preferences;
pt ptLogStatus;
unsigned long activatedAt;
boolean motorActive;
boolean naechsteRichtungLinks;

void setup() {
  Serial.begin(19200);
  Serial.println("");
  Serial.println("        ___  __      __   __             ___ ");
  Serial.println("| |\\ | |__  /  \\    |__) /  \\ |    |    |__  ");  
  Serial.println("| | \\| |    \\__/    |  \\ \\__/ |___ |___ |___ ");
  
  display.begin();
  display.setFont(u8x8_font_chroma48medium8_r);
  display.drawString(0, 0, "INFO ROLLE");

  PT_INIT(&ptLogStatus);
  pinMode(STATUS_LED, OUTPUT); 

  pinMode(PHOTO_LINKS_PIN, INPUT);
  pinMode(PHOTO_RECHTS_PIN, INPUT);

  pinMode(MOTOR1_PIN1, OUTPUT);
  pinMode(MOTOR1_PIN2, OUTPUT);
  pinMode(MOTOR1_PWM, OUTPUT);
  ledcSetup(0, 30000, 8);
  ledcAttachPin(MOTOR1_PWM, 0);

  pinMode(MOTOR2_PIN1, OUTPUT);
  pinMode(MOTOR2_PIN2, OUTPUT);
  pinMode(MOTOR2_PWM, OUTPUT);
  ledcSetup(1, 30000, 8);
  ledcAttachPin(MOTOR2_PWM, 1);

  deactivateMotor();

  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(POTI_SCHWELLE_PIN, INPUT);

  preferences.begin("inforolle", false);
  naechsteRichtungLinks = preferences.getBool(CFG_NAECHSTE_RICHTUNG_LINKS, true);
  Serial.printf("Nächste Drehrichtung: %s\n", (naechsteRichtungLinks? "LINKS" : "RECHTS"));
}

// the loop function runs over and over again forever
void loop() {
  PT_SCHEDULE(printStatus(rollenStatus, &ptLogStatus));
  displayRolleStatus(rollenStatus);
  int streifenSchwelle = readDisplayStreifenSchwelle();

  if (rollenStatus == FEHLER) {
    return;
  }

  if (!isMotorActive()) {
    if (!buttonPressed()) {
      return;
    }
    delay(500);
    activateMotor();
    delay(2000); // 2 Sek Band laufen lassen, damit wir keine Streifen lesen
    return;
  }

  //
  // => Ab hier wissen wir dass Motor aktiv ist
  //

  if (buttonPressed()) {
    deactivateMotor();
  }

  if (activeSinceMs() > (10*1000)) {
    Serial.println("Kein Streifen nach 20s erkannt!");
    errorDetected();
    return;
  }

  if (!rollenSensor.istKalibriert()) {
    rollenSensor.kalibrierungsMessung();
    return;
  }

  StreifenStatus streifen = rollenSensor.leseStreifen(rollenStatus, streifenSchwelle);
  if (streifen == KEIN) {
    return;

  } else if (streifen == EIN) {
    Serial.println("EIN Streifen erkannt!");
    deactivateMotor();
    rollenSensor.resetKalibrierung();

  } else if (streifen == ZWEI) {
    Serial.println("ZWEI Streifen erkannt!");
    deactivateMotor();
    rollenSensor.resetKalibrierung();
    setRichtungswechsel();

  } else {
    // FEHLER
    Serial.println("Sensor-Fehler!");
    errorDetected();
  }
}

void setRichtungswechsel() {
    Serial.println("Richtungswechsel!");
    naechsteRichtungLinks = !naechsteRichtungLinks;
    preferences.putBool(CFG_NAECHSTE_RICHTUNG_LINKS, naechsteRichtungLinks);
    preferences.end();
    preferences.begin("inforolle", false);
}

boolean buttonPressed() {
    int buttonRead = digitalRead(START_BUTTON_PIN);
    if (buttonRead == HIGH) {
      return false;
    }
    do {
      delay(100);
      buttonRead = digitalRead(START_BUTTON_PIN);
    } while(buttonRead == LOW);
    Serial.println("Button pressed!");
    return true;
}

void errorDetected() {
    Serial.println("FEHLER!");
    deactivateMotor();
    rollenSensor.resetKalibrierung();
    setRichtungswechsel();
    //rollenStatus = FEHLER;
}

int readDisplayStreifenSchwelle() {
  int schwelleNormalisiert = (analogRead(POTI_SCHWELLE_PIN) / 4000.0) * 100.0;
  displayStatus(3, String(schwelleNormalisiert));
  return schwelleNormalisiert;
}

boolean isMotorActive() {
  return motorActive;
}

long activeSinceMs() {
  if (!isMotorActive()) {
    return 0L;
  }
  return millis() - activatedAt;
}

void activateMotor() {
  if (motorActive || rollenStatus == FEHLER) {
    return;
  }

  Serial.println("Activate Motor!");
  if (naechsteRichtungLinks) {
    // Motor 1 voll ziehen
    digitalWrite(MOTOR1_PIN1, HIGH);
    digitalWrite(MOTOR1_PIN2, LOW);
    ledcWrite(1, MOTOR_V_ZIEHEN);

    // Motor 2 anlaufen und dann schieben
    digitalWrite(MOTOR2_PIN1, LOW);
    digitalWrite(MOTOR2_PIN2, HIGH);
    ledcWrite(0, MOTOR_V_ANLAUFEN);
    delay(500);
    ledcWrite(0, MOTOR_V_SCHIEBEN);

    rollenStatus = LINKS_DREHEN;

  } else {
    // Motor 2 voll ziehen
    digitalWrite(MOTOR2_PIN1, HIGH);
    digitalWrite(MOTOR2_PIN2, LOW);
    ledcWrite(0, MOTOR_V_ZIEHEN);

    // Motor 1 anlaufen und dann schieben
    digitalWrite(MOTOR1_PIN1, LOW);
    digitalWrite(MOTOR1_PIN2, HIGH);
    ledcWrite(1, MOTOR_V_ANLAUFEN);
    delay(500);
    ledcWrite(1, MOTOR_V_SCHIEBEN);

    rollenStatus = RECHTS_DREHEN;
  }

  motorActive = true;
  activatedAt = millis();
}

void deactivateMotor() {
  Serial.println("Deactivate Motor!");
  if (rollenStatus == LINKS_DREHEN) {
    // digitalWrite(MOTOR2_PIN1, HIGH);
    // digitalWrite(MOTOR2_PIN2, LOW);
    ledcWrite(0, MOTOR_V_ZIEHEN);
    digitalWrite(MOTOR1_PIN1, LOW);
    digitalWrite(MOTOR1_PIN2, HIGH);
    delay(500);

    digitalWrite(MOTOR1_PIN1, LOW);
    digitalWrite(MOTOR1_PIN2, LOW);
    digitalWrite(MOTOR2_PIN1, LOW);
    digitalWrite(MOTOR2_PIN2, LOW);

  } else if (rollenStatus == RECHTS_DREHEN) {
    // digitalWrite(MOTOR1_PIN1, HIGH);
    // digitalWrite(MOTOR1_PIN2, LOW);
    ledcWrite(1, MOTOR_V_ZIEHEN);
    digitalWrite(MOTOR2_PIN1, LOW);
    digitalWrite(MOTOR2_PIN2, HIGH);
    delay(500);

    digitalWrite(MOTOR2_PIN1, LOW);
    digitalWrite(MOTOR2_PIN2, LOW);
    digitalWrite(MOTOR1_PIN1, LOW);
    digitalWrite(MOTOR1_PIN2, LOW);
  }
  motorActive = false;
  rollenStatus = WARTEN;
}