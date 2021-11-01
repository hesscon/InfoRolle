#include "SamplingLog.h"

typedef enum {
  KEIN, EIN, ZWEI, SENSOR_FEHLER
} StreifenStatus;

/**
 * RollenSensor
 */
class RollenSensor {
  private:
    int pinLinks;
    int pinRechts;
    int kalibrierungsMessungen = 0;
    int maxLinks = 2000;
    int maxRechts = 2000;
    float kalibrierungLinks = 0;
    float kalibrierungRechts = 0;
    unsigned long letzterStreifenMillis = 0L;  

    void logMessung(int messungLinks, int messungRechts) {
      String messungLog = String(messungLinks);
      messungLog += " - ";
      messungLog += messungRechts;
      displayStatus(2, messungLog);
    }

  public:
    RollenSensor(int pinDiodeLinks, int pinDiodeRechts) {
      pinLinks = pinDiodeLinks;
      pinRechts = pinDiodeRechts;
    }

    boolean istKalibriert() {
      return kalibrierungLinks != 0 && kalibrierungRechts != 0;
    }

    void kalibrierungsMessung() {
      int messungLinks = analogRead(pinLinks);
      int messungRechts = analogRead(pinRechts);
      maxLinks = max(maxLinks, messungLinks);
      maxRechts = max(maxRechts, messungRechts);
      kalibrierungsMessungen++;

      if (kalibrierungsMessungen >= 20) {
        kalibrierungLinks = 100.0 / ((float)maxLinks);
        kalibrierungRechts = 100.0 / ((float)maxRechts);
      }
    }

    void resetKalibrierung() {
      kalibrierungLinks = 0;
      kalibrierungRechts = 0;
      kalibrierungsMessungen = 0;
    }

    int leseLinks() {
      if (istKalibriert()) {
        return analogRead(pinLinks) * kalibrierungLinks;
      }
      return 100;
    }

    int leseRechts() {
      if (istKalibriert()) {
        return analogRead(pinRechts) * kalibrierungRechts;
      }
      return 100;
    }

    StreifenStatus leseStreifen(RolleStatus status, int streifenSchwelleProzent) {
      int photoLinks = leseLinks();
      int photoRechts = leseRechts();
      logMessung(photoLinks, photoRechts);
      boolean linksDunkel = photoLinks < streifenSchwelleProzent;
      boolean rechtsDunkel = photoRechts < streifenSchwelleProzent;

      if (linksDunkel && rechtsDunkel) {
        letzterStreifenMillis = 0L;
        return ZWEI;
      }

      unsigned long curMillis = millis();
      if (letzterStreifenMillis && ((curMillis - letzterStreifenMillis) > 1000)) {
        letzterStreifenMillis = 0L;
        return SENSOR_FEHLER;
      }

      if (status == LINKS_ZIEHEN) {
        if (rechtsDunkel) {
          letzterStreifenMillis = millis();
          return KEIN;
        }
        if (linksDunkel) {
          letzterStreifenMillis = 0L;
          return EIN;
        }
      } else if (status == RECHTS_ZIEHEN) {
        if (linksDunkel) {
          letzterStreifenMillis = millis();
          return KEIN;
        }
        if (rechtsDunkel) {
          letzterStreifenMillis = 0L;
          return EIN;
        }
      }

      // Wenn wir hier sind, dann dreht sich aktuell die Rolle nicht
      return (linksDunkel || rechtsDunkel)? EIN : KEIN;
    }

};
