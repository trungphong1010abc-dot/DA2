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
  result.otaSupported = Config::OTA_SUPPORTED;
  result.otaLocked = false;

  const bool batteryReadingInvalid = !isfinite(data.batteryV) ||
                                     data.batteryV > Config::BATTERY_SANITY_MAX_V ||
                                     data.batteryV <= 0.1f;

  if ((data.errorFlags & (Protocol::ERR_SOIL | Protocol::ERR_MPU |
                          Protocol::ERR_DATA_RANGE | Protocol::ERR_CONFIG)) != 0) {
    result.dutyCycleMode = "SENSOR_ERROR";
    result.nextSleepSeconds = Config::SLEEP_SENSOR_ERROR_SEC;
    return;
  }

  if (batteryReadingInvalid) {
    result.batteryStatus = "INVALID";
    result.dutyCycleMode = "BATTERY_DATA_INVALID";
    result.nextSleepSeconds = Config::SLEEP_SENSOR_ERROR_SEC;
    return;
  }

  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_CRITICAL_V) {
    result.batteryStatus = "CRITICAL";
    result.otaLocked = Config::OTA_SUPPORTED;
    result.dutyCycleMode = "BATTERY_CRITICAL_PROVISIONAL";
    result.nextSleepSeconds = Config::SLEEP_SENSOR_ERROR_SEC;
    return;
  }

  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_LOW_V) {
    result.batteryStatus = "LOW";
    result.otaLocked = Config::OTA_SUPPORTED;
    result.dutyCycleMode = "BATTERY_LOW_PROVISIONAL";
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

  const bool sensorDataValid =
      (data.errorFlags & (Protocol::ERR_SOIL | Protocol::ERR_MPU |
                          Protocol::ERR_DATA_RANGE | Protocol::ERR_CONFIG)) == 0;

  const float betaRad = degToRad(data.betaDeg);
  const float phiRad = degToRad(Config::SOIL_FRICTION_ANGLE_DEG);
  const float gammaZ = Config::SOIL_GAMMA_KN_M3 * Config::SLIP_LAYER_DEPTH_M;

  result.porePressureKpa = calcPorePressure(data.soilPercent);
  result.tauDriveKpa = gammaZ * sinf(betaRad) * cosf(betaRad);
  result.sigmaNormalKpa = gammaZ * cosf(betaRad) * cosf(betaRad);
  result.sigmaEffectiveKpa = result.sigmaNormalKpa - result.porePressureKpa;
  result.shearStrengthKpa = Config::SOIL_COHESION_KPA +
                            result.sigmaEffectiveKpa * tanf(phiRad);

  result.valid = sensorDataValid && isfinite(result.tauDriveKpa) &&
                 isfinite(result.sigmaNormalKpa) && isfinite(result.sigmaEffectiveKpa) &&
                 isfinite(result.shearStrengthKpa) && fabsf(result.tauDriveKpa) > 0.001f;
  if (result.valid) {
    result.factorOfSafety = result.shearStrengthKpa / result.tauDriveKpa;
    if (isfinite(result.factorOfSafety) && fabsf(result.factorOfSafety) > 0.0001f) {
      result.strainIndex = 1.0f / result.factorOfSafety;
    }
    result.valid = isfinite(result.factorOfSafety) && isfinite(result.strainIndex);
  }

  result.dynamicIndex = sensorDataValid ? calcDynamicIndex(data) : NAN;

  if (!sensorDataValid) {
    result.alertLevel = "WARNING";
    result.riskStatus = "SENSOR_ERROR";
    result.warningMessage = "Du lieu cam bien khong hop le, can kiem tra";
  } else if (!isfinite(data.batteryV) || data.batteryV > Config::BATTERY_SANITY_MAX_V ||
             data.batteryV <= 0.1f) {
    result.alertLevel = "WARNING";
    result.riskStatus = "BATTERY_DATA_INVALID";
    result.warningMessage = "Dien ap pin ngoai mien hop le";
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
  if ((flags & Protocol::ERR_CONFIG) != 0) {
    return "CONFIG";
  }
  if ((flags & Protocol::ERR_DATA_RANGE) != 0) {
    return "DATA_RANGE";
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
