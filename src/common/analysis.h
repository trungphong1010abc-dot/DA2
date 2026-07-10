#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "protocol.h"

namespace Analysis {

struct Result {
  bool valid = false;
  float porePressureKpa = 0.0f;
  float sigmaNormalKpa = 0.0f;
  float tauDriveKpa = 0.0f;
  float sigmaEffectiveKpa = 0.0f;
  float shearStrengthKpa = 0.0f;
  float factorOfSafety = NAN;
  float dynamicIndex = 0.0f;
  float strainIndex = NAN;
  String alertLevel = "NORMAL";
  String riskStatus = "SAFE";
  String dutyCycleMode = "NORMAL";
  uint32_t nextSleepSeconds = 0;
};

Result evaluate(const Protocol::SensorPacket &data);

const char *errorFlagsToText(uint16_t flags);

} // namespace Analysis
