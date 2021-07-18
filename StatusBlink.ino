#include "protothreads.h"

int blinkThread(struct pt* pt) {
  PT_BEGIN(pt);

  for(;;) {
    digitalWrite(STATUS_LED, HIGH);
    PT_SLEEP(pt, 1000);
    Serial.println("Blink...");
    digitalWrite(STATUS_LED, LOW);
    PT_SLEEP(pt, 1000);
  }

  PT_END(pt);
}

int pingStatus(RolleStatus status, struct pt* pt) {
 PT_BEGIN(pt);

  while(true) {
    PT_SLEEP(pt, 1000);

    switch (status) {
      case WARTEN:
        Serial.println("STATUS: warten...");
        break;

      case FEHLER:
        Serial.println("STATUS: fehler...");
        break;

      case LINKS_DREHEN:
        Serial.println("STATUS: nach links drehen...");
        break;

      case RECHTS_DREHEN:
        Serial.println("STATUS: nach rechts drehen...");
        break;
    }

    PT_SLEEP(pt, 1000);
  }

  PT_END(pt);
}
