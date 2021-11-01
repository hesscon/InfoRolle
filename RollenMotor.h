
typedef enum {
  AUS, AUFROLLEN, ABROLLEN
} RollenMotorStatus;

/**
 * RollenMotor
 */
class RollenMotor {
  private:
    int pinStrom1;
    int pinStrom2;
    int pinGeschwindigkeit;
    RollenMotorStatus status;

  public:
    RollenMotor(int pinMotorStrom1, int pinMotorStrom2, int pinMotorGeschwindigkeit) {
      pinStrom1 = pinMotorStrom1;
      pinStrom2 = pinMotorStrom2;
      pinGeschwindigkeit = pinMotorGeschwindigkeit;
      status = AUS;
    }

    void aus() {
      ledcWrite(pinGeschwindigkeit, 0);
      digitalWrite(pinStrom1, LOW);
      digitalWrite(pinStrom2, LOW);
    }

    void aufrollen(int geschwindigkeit) {
      if (status != AUFROLLEN) {
        aus();
      }
      digitalWrite(pinStrom1, HIGH);
      digitalWrite(pinStrom2, LOW);
      ledcWrite(pinGeschwindigkeit, geschwindigkeit);
      status = AUFROLLEN;
    }

    void abrollen(int geschwindigkeit) {
      if (status != ABROLLEN) {
        aus();
      }
      digitalWrite(pinStrom1, LOW);
      digitalWrite(pinStrom2, HIGH);
      ledcWrite(pinGeschwindigkeit, geschwindigkeit);
      status = ABROLLEN;
    }
};
