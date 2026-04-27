#include <Arduino.h>
#include "soil_moisture.h"

// thay đổi giữa chế độ calib và test bằng cách comment/uncomment 2 dòng define dưới đây
//#define RUN_CALIB
#define RUN_TEST 

void setup() {
#ifdef RUN_CALIB
  soilCalibSetup();
#else
  soilTestSetup();
#endif
}

void loop() {
#ifdef RUN_CALIB
  soilCalibLoop();
#else
  soilTestLoop();
#endif
}