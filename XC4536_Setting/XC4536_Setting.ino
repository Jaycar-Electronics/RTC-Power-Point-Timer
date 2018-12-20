#include <Wire.h>
#include "RTClib.h"
RTC_DS1307 rtc;

void setup () {
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop () {
}
