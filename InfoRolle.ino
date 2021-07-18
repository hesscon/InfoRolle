#include "protothreads.h"

#define MOTOR1_PIN1 16
#define MOTOR1_PIN2 17
#define MOTOR1_PWM  5

#define MOTOR2_PIN1 18
#define MOTOR2_PIN2 19
#define MOTOR2_PWM  4

#define PHOTO_LINKS   34
#define PHOTO_RECHTS  35

#define START_BUTTON  14
#define STATUS_LED    1

unsigned long activatedAt;
boolean motorActive;
int photoSchwelle = 2600;

typedef enum {
  LINKS_DREHEN, RECHTS_DREHEN, WARTEN, FEHLER
} RolleStatus;
RolleStatus status = WARTEN;
boolean linksDrehen = true;
pt ptStatus;

typedef enum {
  KEIN, EIN, ZWEI
} StreifenStatus;
unsigned long lastStripeMillis = 0L;

void setup() {
  Serial.begin(19200);
  Serial.println("");
  Serial.println("        ___  __      __   __             ___ ");
  Serial.println("| |\\ | |__  /  \\    |__) /  \\ |    |    |__  ");  
  Serial.println("| | \\| |    \\__/    |  \\ \\__/ |___ |___ |___ ");
  
  PT_INIT(&ptStatus);
  pinMode(STATUS_LED, OUTPUT); 

  pinMode(PHOTO_LINKS, INPUT);
  pinMode(PHOTO_RECHTS, INPUT);

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

  pinMode(START_BUTTON, INPUT_PULLUP);
}

// the loop function runs over and over again forever
void loop() {
  PT_SCHEDULE(pingStatus(status, &ptStatus));

  if (status == FEHLER) {
    return;
  }

  if (!isMotorActive()) {
    int buttonRead = digitalRead(START_BUTTON);
    if (buttonRead == HIGH) {
      // Nicht gedrückt!
      return;
    }
    do {
      delay(100);
      buttonRead = digitalRead(START_BUTTON);
    } while(buttonRead == LOW);
    Serial.println("Button pressed!");
    delay(500);
    activateMotor();
    delay(2000); // 2 Sek Band laufen lassen, damit wir keine Streifen lesen
    return;
  }

  //
  // => Ab hier wissen wir dass Motor aktiv ist
  //

  if (activeSinceMs() > 30000) {
    Serial.println("Deactivate after 10s");
    setErrorMode();
    return;
  }

  StreifenStatus streifen = readStripeState();
  if (streifen == KEIN) {
    return;

  } else if (streifen == EIN) {
    Serial.println("EIN Streifen erkannt!");
    deactivateMotor();

  } else if (streifen == ZWEI) {
    Serial.println("ZWEI Streifen erkannt!");
    deactivateMotor();
    Serial.println("Richtungswechsel!");
    linksDrehen = !linksDrehen;
  }
}

void setErrorMode() {
    Serial.println("Enter ERROR state!");
    deactivateMotor();
    lastStripeMillis = 0L;
    //status = FEHLER;
}

StreifenStatus readStripeState() {
  int photoLinks = analogRead(PHOTO_LINKS);
  int photoRechts = analogRead(PHOTO_RECHTS);
  boolean linksDunkel = photoLinks < photoSchwelle;
  boolean rechtsDunkel = photoRechts < photoSchwelle;
  String photoReadString = "Photo read: ";
  photoReadString += photoLinks;
  photoReadString += " - ";
  photoReadString += photoRechts;
  samplingLog(photoReadString);

  if (linksDunkel && rechtsDunkel) {
    lastStripeMillis = 0L;
    return ZWEI;
  }

  unsigned long curMillis = millis();
  if (lastStripeMillis && ((curMillis - lastStripeMillis) > 1000)) {
    setErrorMode();
    return KEIN;
  }

  if (status == LINKS_DREHEN) {
    if (rechtsDunkel) {
      lastStripeMillis = millis();
      return KEIN;
    }
    if (linksDunkel) {
      lastStripeMillis = 0L;
      return EIN;
    }
  } else if (status == RECHTS_DREHEN) {
    if (linksDunkel) {
      lastStripeMillis = millis();
      return KEIN;
    }
    if (rechtsDunkel) {
      lastStripeMillis = 0L;
      return EIN;
    }
  }

  return KEIN;
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
  if (motorActive || status == FEHLER) {
    return;
  }
  Serial.println("Activate Motor!");
  if (linksDrehen) {
    digitalWrite(MOTOR1_PIN1, HIGH);
    digitalWrite(MOTOR1_PIN2, LOW);
    ledcWrite(1, 255);
    digitalWrite(MOTOR2_PIN1, LOW);
    digitalWrite(MOTOR2_PIN2, HIGH);
    ledcWrite(0, 200);
    status = LINKS_DREHEN;
  } else {
    digitalWrite(MOTOR1_PIN1, LOW);
    digitalWrite(MOTOR1_PIN2, HIGH);
    ledcWrite(1, 200);
    digitalWrite(MOTOR2_PIN1, HIGH);
    digitalWrite(MOTOR2_PIN2, LOW);
    ledcWrite(0, 255);
    status = RECHTS_DREHEN;
  }
  motorActive = true;
  activatedAt = millis();
}

void deactivateMotor() {
  Serial.println("Deactivate Motor!");
  digitalWrite(MOTOR1_PIN1, LOW);
  digitalWrite(MOTOR1_PIN2, LOW);
  digitalWrite(MOTOR2_PIN1, LOW);
  digitalWrite(MOTOR2_PIN2, LOW);
  motorActive = false;
  status = WARTEN;
}