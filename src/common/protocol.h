#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace Protocol {

enum PacketType : uint8_t {
  PACKET_UNKNOWN = 0,
  PACKET_DATA,
  PACKET_ACK,
  PACKET_CMD,
};

enum ErrorFlag : uint16_t {
  ERR_NONE = 0,
  ERR_SOIL = 1 << 0,
  ERR_MPU = 1 << 1,
  ERR_LORA = 1 << 2,
  ERR_PACKET = 1 << 3,
  ERR_DATA_RANGE = 1 << 4,
  ERR_CONFIG = 1 << 5,
};

struct SensorPacket {
  uint8_t gatewayId = 0;
  uint8_t nodeId = 0;
  uint32_t packetId = 0;
  uint32_t timestampMs = 0;
  int soilAdc = 0;
  float soilPercent = 0.0f;
  float betaDeg = 0.0f;
  float betaDotDegPerHour = 0.0f;
  float vibrationRmsG = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  uint16_t errorFlags = ERR_NONE;
  int rssi = 0;
};

struct AckPacket {
  uint8_t gatewayId = 0;
  uint8_t nodeId = 0;
  uint32_t packetId = 0;
  String status = "OK";
  uint32_t sleepSeconds = 0;
  String alertLevel = "NORMAL";
};

struct CommandPacket {
  uint8_t gatewayId = 0;
  uint8_t nodeId = 0;
  uint32_t packetId = 0;
  String command;
  String parameter;
};

PacketType packetType(const String &packet);

String buildDataPacket(const SensorPacket &data);
String buildAckPacket(const AckPacket &ack);
String buildCommandPacket(const CommandPacket &cmd);

bool parseDataPacket(const String &packet, SensorPacket &data);
bool parseAckPacket(const String &packet, AckPacket &ack);
bool parseCommandPacket(const String &packet, CommandPacket &cmd);
bool validatePacket(const String &packet);

uint16_t crc16Ccitt(const String &data);
String appendCrc(const String &withoutCrc);

String getField(const String &packet, const char *key);

} // namespace Protocol
