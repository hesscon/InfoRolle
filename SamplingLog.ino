String lastLog;
unsigned long lastLogTime;
unsigned long relogTime = 2000L;

void samplingLog(String log) {
  unsigned long curTime = millis();
  //if (lastLog!=NULL && lastLog.equals(log)) {
    if (lastLogTime + relogTime <= curTime) {
      doLog(log, curTime);
    }
  // } else {
  //     doLog(log, curTime);
  // }
}

void doLog(String log, unsigned long logTime) {
  lastLogTime = logTime;
  lastLog = log;
  Serial.println(log);
}