#include "analysis.h"

#include <math.h>

#include "project_config.h"

namespace Analysis {

namespace {

float degToRad(float deg) {
  return deg * PI / 180.0f;
}

float calcPorePressure(float soilPercent) {
  const float normalized = (soilPercent - Config::MOISTURE_DANGER_START_PERCENT) /
                           (Config::MOISTURE_SATURATION_PERCENT - Config::MOISTURE_DANGER_START_PERCENT);
  return Config::PORE_PRESSURE_MAX_KPA * fmaxf(0.0f, normalized);
}

float calcDynamicIndex(const Protocol::SensorPacket &data) {
  const float betaTerm = fabsf(data.betaDotDegPerHour) / Config::BETA_DOT_CRIT_DEG_PER_HOUR;
  const float vibrationTerm = data.vibrationRmsG / Config::A_RMS_CRIT_G;
  return Config::DI_WEIGHT_BETA_DOT * betaTerm + Config::DI_WEIGHT_VIBRATION * vibrationTerm;
}

void applyAdaptiveDutyCycle(Result &result, const Protocol::SensorPacket &data) {
  result.batteryStatus = "NORMAL";
  result.otaLocked = false;

  if ((data.errorFlags & (Protocol::ERR_SOIL | Protocol::ERR_MPU)) != 0) {
    result.dutyCycleMode = "SENSOR_ERROR";
    result.nextSleepSeconds = Config::SLEEP_SENSOR_ERROR_SEC;
    return;
  }

  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_CRITICAL_V) {
    result.batteryStatus = "CRITICAL";
    result.otaLocked = true;
    result.dutyCycleMode = "BATTERY_CRITICAL";
    result.nextSleepSeconds = Config::SLEEP_SENSOR_ERROR_SEC;
    return;
  }

  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_LOW_V) {
    result.batteryStatus = "LOW";
    result.otaLocked = true;
    result.dutyCycleMode = "BATTERY_LOW";
    result.nextSleepSeconds = Config::SLEEP_WARNING_SEC;
    return;
  }

  if (result.alertLevel == "DANGER") {
    result.dutyCycleMode = "DANGER";
    result.nextSleepSeconds = Config::SLEEP_DANGER_SEC;
    return;
  }
  if (result.alertLevel == "WARNING") {
    result.dutyCycleMode = "WARNING";
    result.nextSleepSeconds = Config::SLEEP_WARNING_SEC;
    return;
  }
  result.dutyCycleMode = "NORMAL";
  result.nextSleepSeconds = Config::SLEEP_NORMAL_SEC;
}

} // namespace

Result evaluate(const Protocol::SensorPacket &data) {
  Result result;

  const float betaRad = degToRad(data.betaDeg);
  const float phiRad = degToRad(Config::SOIL_FRICTION_ANGLE_DEG);
  const float gammaZ = Config::SOIL_GAMMA_KN_M3 * Config::SLIP_LAYER_DEPTH_M;

  result.porePressureKpa = calcPorePressure(data.soilPercent);
  result.tauDriveKpa = gammaZ * sinf(betaRad) * cosf(betaRad);
  result.sigmaNormalKpa = gammaZ * cosf(betaRad) * cosf(betaRad);
  result.sigmaEffectiveKpa = result.sigmaNormalKpa - result.porePressureKpa;
  result.shearStrengthKpa = Config::SOIL_COHESION_KPA +
                            result.sigmaEffectiveKpa * tanf(phiRad);

  result.valid = isfinite(result.tauDriveKpa) && fabsf(result.tauDriveKpa) > 0.001f;
  if (result.valid) {
    result.factorOfSafety = result.shearStrengthKpa / result.tauDriveKpa;
    result.strainIndex = 1.0f / result.factorOfSafety;
    result.valid = isfinite(result.factorOfSafety) && isfinite(result.strainIndex);
  }

  result.dynamicIndex = calcDynamicIndex(data);

  if ((data.errorFlags & (Protocol::ERR_SOIL | Protocol::ERR_MPU)) != 0) {
    result.alertLevel = "WARNING";
    result.riskStatus = "SENSOR_ERROR";
    result.warningMessage = "Loi cam bien, dung gia tri gan nhat can kiem tra";
  } else if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_CRITICAL_V) {
    result.alertLevel = "DANGER";
    result.riskStatus = "BATTERY_CRITICAL";
    result.warningMessage = "Pin rat yeu, can thay/sac pin";
  } else if (!result.valid) {
    result.alertLevel = "WARNING";
    result.riskStatus = "ANALYSIS_INVALID";
    result.warningMessage = "Khong du dieu kien tinh FS va epsilon";
  } else if (result.factorOfSafety <= Config::FS_DANGER ||
             result.dynamicIndex >= Config::DI_DANGER ||
             result.strainIndex >= Config::STRAIN_DANGER) {
    result.alertLevel = "DANGER";
    result.riskStatus = "LANDSLIDE_RISK";
    result.warningMessage = "Nguy co bien dang/sat lo cao";
  } else if (result.factorOfSafety <= Config::FS_WARNING ||
             result.dynamicIndex >= Config::DI_WARNING ||
             result.strainIndex >= Config::STRAIN_WARNING ||
             (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_LOW_V)) {
    result.alertLevel = "WARNING";
    result.riskStatus = "UNSTABLE_TREND";
    result.warningMessage = "Dat co xu huong mat on dinh";
  } else {
    result.alertLevel = "NORMAL";
    result.riskStatus = "SAFE";
    result.warningMessage = "Dat on dinh";
  }

  applyAdaptiveDutyCycle(result, data);
  result.nextSleepSeconds = constrain(result.nextSleepSeconds,
                                      Config::SLEEP_DANGER_SEC,
                                      Config::SLEEP_NORMAL_SEC);
  return result;
}

const char *errorFlagsToText(uint16_t flags) {
  if (flags == Protocol::ERR_NONE) {
    return "NONE";
  }
  if ((flags & Protocol::ERR_PACKET) != 0) {
    return "PACKET";
  }
  if ((flags & Protocol::ERR_LORA) != 0) {
    return "LORA";
  }
  if ((flags & Protocol::ERR_SOIL) != 0) {
    return "SOIL";
  }
  if ((flags & Protocol::ERR_MPU) != 0) {
    return "MPU";
  }
  if ((flags & Protocol::ERR_BATTERY) != 0) {
    return "BATTERY";
  }
  return "MIXED";
}

} // namespace Analysis
