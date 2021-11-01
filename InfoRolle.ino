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
  LINKS_AUFROLLEN = 0, RECHTS_AUFROLLEN, WARTEN, FEHLER
} RolleStatus;

typedef enum {
  ZEIT_GESTEUERT = 0, EREIGNIS_GESTEUERT, SAUBER_AUFROLLEN
} RolleModus;

#include "StatusLog.h"
#include "RollenSensor.h"
#include "RollenMotor.h"
#include "EventRegister.h"

#define MOTOR_LINKS_PIN1 16
#define MOTOR_LINKS_PIN2 17
#define MOTOR_LINKS_PWM  5
#define MOTOR_LINKS_GESCHW 0

#define MOTOR_RECHTS_PIN1 18
#define MOTOR_RECHTS_PIN2 19
#define MOTOR_RECHTS_PWM  4
#define MOTOR_RECHTS_GESCHW 1

#define MOTOR_V_AUFROLLEN 200
#define MOTOR_V_ANLAUFEN 200
#define MOTOR_V_ABROLLEN 170
//#define MOTOR_V_ANLAUFEN 0
//#define MOTOR_V_ABROLLEN 0

#define POTI_SCHWELLE_PIN  15
#define PHOTO_LINKS_PIN   34
#define PHOTO_RECHTS_PIN  35
#define EVENT_BUTTON_PIN  14
#define FUNC_BUTTON_PIN  27
#define STATUS_LED    1

#define CFG_INFOROLLE "inforolle"
#define CFG_NAECHSTE_RICHTUNG_LINKS "richtungL"

#define AUTO_AKTIVIERUNG_INTERVALL 20000
#define MAX_BAND_ABSCHNITT_LAUFZEIT 12000

RollenSensor rollenSensor(PHOTO_LINKS_PIN, PHOTO_RECHTS_PIN);
RolleStatus rollenStatus = WARTEN;
RolleModus rollenModus = ZEIT_GESTEUERT;
RollenMotor motorLinks(MOTOR_LINKS_PIN2, MOTOR_LINKS_PIN1, MOTOR_LINKS_GESCHW);
RollenMotor motorRechts(MOTOR_RECHTS_PIN1, MOTOR_RECHTS_PIN2, MOTOR_RECHTS_GESCHW);
Preferences preferences;
pt ptLogStatus;
unsigned long activatedAt = 0;
unsigned long lastRunEventAt = 0;
int numberErrors = 0;
boolean motorActive;
bool naechsteRichtungLinks;
int streifenSchwelle;
EventRegister funcBtnReg = EventRegister();

void setup() {
  Serial.begin(115200);
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

  pinMode(MOTOR_LINKS_PIN1, OUTPUT);
  pinMode(MOTOR_LINKS_PIN2, OUTPUT);
  pinMode(MOTOR_LINKS_PWM, OUTPUT);
  ledcSetup(0, 30000, 8);
  ledcAttachPin(MOTOR_LINKS_PWM, MOTOR_LINKS_GESCHW);

  pinMode(MOTOR_RECHTS_PIN1, OUTPUT);
  pinMode(MOTOR_RECHTS_PIN2, OUTPUT);
  pinMode(MOTOR_RECHTS_PWM, OUTPUT);
  ledcSetup(1, 30000, 8);
  ledcAttachPin(MOTOR_RECHTS_PWM, MOTOR_RECHTS_GESCHW);

  deactivateMotor();

  pinMode(EVENT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FUNC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(POTI_SCHWELLE_PIN, INPUT);

  preferences.begin(CFG_INFOROLLE, false);
  naechsteRichtungLinks = preferences.getBool(CFG_NAECHSTE_RICHTUNG_LINKS, true);
  preferences.end();
  Serial.printf("Nächste Drehrichtung: %s\n", (naechsteRichtungLinks? "LINKS" : "RECHTS"));
}

// the loop function runs over and over again forever
void loop() {
  PT_SCHEDULE(printStatus(rollenStatus, &ptLogStatus));
  displayRolleStatus(rollenStatus);
  displayRolleModus(rollenModus);
  streifenSchwelle = readDisplayStreifenSchwelle();

  if (rollenStatus == FEHLER) {
    return;
  }

  if (buttonPressed(FUNC_BUTTON_PIN)) {
    funcBtnReg.registerEvent();
    deactivateMotor();
    rollenModus = static_cast<RolleModus>((rollenModus+1) % 3);
    return;
  }
  if (funcBtnReg.registeredLessThanSecAgo(3)) {
    return;
  }
  funcBtnReg.clearEvent();

  switch(rollenModus) {
    case ZEIT_GESTEUERT:
    case EREIGNIS_GESTEUERT:
      loopNormalModus();
      break;

    case SAUBER_AUFROLLEN:
      loopSauberAufrollenModus();
      break;
  }

}

void loopSauberAufrollenModus() {
  if (!isMotorActive()) {
    activateMotor();
    delay(1000); // 1 Sek Band laufen lassen, damit wir keine Streifen lesen
  }
  if (!rollenSensor.istKalibriert()) {
    rollenSensor.kalibrierungsMessung();
    return;
  }

  StreifenStatus streifen = rollenSensor.leseStreifen(rollenStatus, streifenSchwelle);
  if (streifen == ZWEI) {
    Serial.println("ZWEI Streifen erkannt!");
    deactivateMotor();
    rollenSensor.resetKalibrierung();
    setRichtungswechsel();
    delay(500);
  }
}

void loopNormalModus() {
  if (!isMotorActive()) {
    if (runEventTriggered()) {
      delay(500);
      activateMotor();
      delay(1000); // 1 Sek Band laufen lassen, damit wir keine Streifen lesen
    }
    return;
  }

  //
  // => Ab hier wissen wir dass Motor aktiv ist
  //

  if (buttonPressed(EVENT_BUTTON_PIN)) {
    deactivateMotor();
  }

  if (activeSinceMs() > MAX_BAND_ABSCHNITT_LAUFZEIT) {
    Serial.println("Kein Streifen nach max Abschnittlaufzeit erkannt!");
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
    preferences.begin(CFG_INFOROLLE, false);
    preferences.putBool(CFG_NAECHSTE_RICHTUNG_LINKS, naechsteRichtungLinks);
    preferences.end();
}

boolean runEventTriggered() {
  unsigned long curMs = millis();

  if (buttonPressed(EVENT_BUTTON_PIN)) {
    lastRunEventAt = curMs;
    return true;
  }

  // Autoaktivierung nach Zeitintervall
  if(curMs - lastRunEventAt > AUTO_AKTIVIERUNG_INTERVALL) {
    lastRunEventAt = curMs;
    return true;
  } 

  return false;
}

boolean buttonPressed(int buttonPin) {
  int buttonRead = digitalRead(buttonPin);
  if (buttonRead == HIGH) {
    return false;
  }
  do {
    delay(100);
    buttonRead = digitalRead(buttonPin);
  } while(buttonRead == LOW);

  Serial.printf("Button %d pressed!\n", buttonPin);

  return true;
}

void errorDetected() {
    Serial.println("FEHLER!");
    deactivateMotor();
    rollenSensor.resetKalibrierung();
    setRichtungswechsel();
    if (numberErrors > 1) {
      rollenStatus = FEHLER;
    }
    numberErrors++;
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
    // Motor Rechts etwas aufrollen
    motorRechts.aufrollen(MOTOR_V_AUFROLLEN);
    delay(500);

    // Motor Links voll ziehen
    motorLinks.aufrollen(MOTOR_V_AUFROLLEN);

    // Motor Rechts anlaufen und dann langsam schieben
    motorRechts.abrollen(MOTOR_V_ANLAUFEN);
    delay(500);
    motorRechts.abrollen(MOTOR_V_ABROLLEN);
 
    rollenStatus = LINKS_AUFROLLEN;

  } else {
    // Motor Links etwas aufrollen
    motorLinks.aufrollen(MOTOR_V_AUFROLLEN);
    delay(500);

    // Motor Rechts voll ziehen
    motorRechts.aufrollen(MOTOR_V_AUFROLLEN);

    // Motor Links anlaufen und dann langsam schieben
    motorLinks.abrollen(MOTOR_V_ANLAUFEN);
    delay(500);
    motorLinks.abrollen(MOTOR_V_ABROLLEN);

    rollenStatus = RECHTS_AUFROLLEN;
  }

  displayRolleStatus(rollenStatus);

  motorActive = true;
  activatedAt = millis();
}

void deactivateMotor() {
  Serial.println("Deactivate Motor!");
  // Papierband straffen!
  if (rollenStatus == LINKS_AUFROLLEN) {
    motorRechts.aufrollen(MOTOR_V_AUFROLLEN);
    delay(500);

  } else if (rollenStatus == RECHTS_AUFROLLEN) {
    motorLinks.aufrollen(MOTOR_V_AUFROLLEN);
    delay(500);
  }

  motorLinks.aus();
  motorRechts.aus();

  motorActive = false;
  rollenStatus = WARTEN;
}