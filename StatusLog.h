#include "protothreads.h"

char *rolleStatusToString(RolleStatus status) {
    switch (status) {
      case WARTEN:
        return "WARTEN...";

      case FEHLER:
        return "FEHLER...";

      case LINKS_AUFROLLEN:
        return "nach LINKS...";

      case RECHTS_AUFROLLEN:
        return "nach RECHTS...";
    }
}

char *rolleModusToString(RolleModus modus) {
    switch (modus) {
      case ZEIT_GESTEUERT:
        return "Modus: Zeit";

      case EREIGNIS_GESTEUERT:
        return "Modus: Ereignis";

      case SAUBER_AUFROLLEN:
        return "Modus: Aufrollen";
    }
}

int printStatus(RolleStatus status, struct pt* pt) {
 PT_BEGIN(pt);

  while(true) {
    PT_SLEEP(pt, 1000);
    Serial.println(rolleStatusToString(status));
    PT_SLEEP(pt, 1000);
  }

  PT_END(pt);
}

RolleStatus lastDisplayStatus;

int displayStatus(RolleStatus status, struct pt* pt) {
 PT_BEGIN(pt);

  while(true) {
    PT_SLEEP(pt, 100);
    if (status != lastDisplayStatus) {
      display.clearLine(1);
      display.drawString(0, 1, rolleStatusToString(status));
      lastDisplayStatus = status;
    }
  }

  PT_END(pt);
}

String displayStatusStrings[20];

void displayStatus(int row, String status) {
  if (status != displayStatusStrings[row]) {
    displayStatusStrings[row] = status;
    display.clearLine(row);
    display.drawString(0, row, status.c_str());
  } 
}

void displayRolleStatus(RolleStatus status) {
  displayStatus(1, rolleStatusToString(status));
}

void displayRolleModus(RolleModus modus) {
  displayStatus(5, rolleModusToString(modus));
}

void displayLog(String log) {
  display.clearLine(8);
  display.drawString(0, 8, log.c_str());
}
