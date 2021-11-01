/**
 * EventRegister
 */
class EventRegister {
  private:
    unsigned long timeMs;

  public:
    EventRegister() {
    }

    void registerEvent() {
      timeMs = millis();
    }

    bool hasEvent() {
      return timeMs == 0;
    }

    bool registeredLessThanSecAgo(int sekunden) {
      if (hasEvent()) {
        return false;
      }
      unsigned long curMs = millis();
      return (curMs - timeMs) <= (sekunden * 1000ul);
    }

    void clearEvent() {
      timeMs = 0;
    }
};
