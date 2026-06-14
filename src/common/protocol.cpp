#include "protocol.h"

#include <stdlib.h>
#include <string.h>

namespace Protocol {

namespace {

String floatField(float value, uint8_t decimals) {
  return String(value, static_cast<unsigned int>(decimals));
}

bool parseUint8(const String &value, uint8_t &out) {
  if (value.length() == 0) {
    return false;
  }
  const long parsed = value.toInt();
  if (parsed < 0 || parsed > 255) {
    return false;
  }
  out = static_cast<uint8_t>(parsed);
  return true;
}

bool parseUint16(const String &value, uint16_t &out) {
  if (value.length() == 0) {
    return false;
  }
  const long parsed = value.toInt();
  if (parsed < 0 || parsed > 65535) {
    return false;
  }
  out = static_cast<uint16_t>(parsed);
  return true;
}

bool parseUint32(const String &value, uint32_t &out) {
  if (value.length() == 0) {
    return false;
  }
  out = static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 10));
  return true;
}

String stripCrcField(const String &packet) {
  const int crcIndex = packet.lastIndexOf("|crc=");
  if (crcIndex < 0) {
    return packet;
  }
  return packet.substring(0, crcIndex);
}

} // namespace

uint16_t crc16Ccitt(const String &data) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < data.length(); ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000) != 0) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

String appendCrc(const String &withoutCrc) {
  char crcText[5];
  snprintf(crcText, sizeof(crcText), "%04X", crc16Ccitt(withoutCrc));
  return withoutCrc + "|crc=" + crcText;
}

String getField(const String &packet, const char *key) {
  const String search = String("|") + key + "=";
  int start = packet.indexOf(search);
  if (start < 0) {
    const String searchAtStart = String(key) + "=";
    if (!packet.startsWith(searchAtStart)) {
      return "";
    }
    start = 0;
  } else {
    start += 1;
  }

  start += strlen(key) + 1;
  int end = packet.indexOf('|', start);
  if (end < 0) {
    end = packet.length();
  }
  return packet.substring(start, end);
}

bool validatePacket(const String &packet) {
  const int crcIndex = packet.lastIndexOf("|crc=");
  if (crcIndex < 0 || crcIndex + 9 != packet.length()) {
    return false;
  }

  const String crcText = packet.substring(crcIndex + 5);
  const uint16_t expected = static_cast<uint16_t>(strtoul(crcText.c_str(), nullptr, 16));
  const uint16_t actual = crc16Ccitt(packet.substring(0, crcIndex));
  return expected == actual;
}

PacketType packetType(const String &packet) {
  if (!packet.startsWith("DA2|")) {
    return PACKET_UNKNOWN;
  }
  if (packet.startsWith("DA2|DATA|")) {
    return PACKET_DATA;
  }
  if (packet.startsWith("DA2|ACK|")) {
    return PACKET_ACK;
  }
  if (packet.startsWith("DA2|CMD|")) {
    return PACKET_CMD;
  }
  return PACKET_UNKNOWN;
}

String buildDataPacket(const SensorPacket &data) {
  String packet = "DA2|DATA";
  packet += "|gw=" + String(data.gatewayId);
  packet += "|node=" + String(data.nodeId);
  packet += "|pid=" + String(data.packetId);
  packet += "|ts=" + String(data.timestampMs);
  packet += "|adc=" + String(data.soilAdc);
  packet += "|hs=" + floatField(data.soilPercent, 2);
  packet += "|beta=" + floatField(data.betaDeg, 3);
  packet += "|bdot=" + floatField(data.betaDotDegPerHour, 4);
  packet += "|arms=" + floatField(data.vibrationRmsG, 5);
  packet += "|pitch=" + floatField(data.pitchDeg, 3);
  packet += "|roll=" + floatField(data.rollDeg, 3);
  packet += "|vbat=" + floatField(data.batteryV, 3);
  packet += "|err=" + String(data.errorFlags);
  return appendCrc(packet);
}

String buildAckPacket(const AckPacket &ack) {
  String packet = "DA2|ACK";
  packet += "|gw=" + String(ack.gatewayId);
  packet += "|node=" + String(ack.nodeId);
  packet += "|pid=" + String(ack.packetId);
  packet += "|status=" + ack.status;
  packet += "|sleep=" + String(ack.sleepSeconds);
  packet += "|alert=" + ack.alertLevel;
  return appendCrc(packet);
}

String buildCommandPacket(const CommandPacket &cmd) {
  String packet = "DA2|CMD";
  packet += "|gw=" + String(cmd.gatewayId);
  packet += "|node=" + String(cmd.nodeId);
  packet += "|pid=" + String(cmd.packetId);
  packet += "|cmd=" + cmd.command;
  packet += "|param=" + cmd.parameter;
  return appendCrc(packet);
}

bool parseDataPacket(const String &packet, SensorPacket &data) {
  if (packetType(packet) != PACKET_DATA || !validatePacket(packet)) {
    return false;
  }

  uint16_t err = 0;
  bool ok = true;
  ok &= parseUint8(getField(packet, "gw"), data.gatewayId);
  ok &= parseUint8(getField(packet, "node"), data.nodeId);
  ok &= parseUint32(getField(packet, "pid"), data.packetId);
  ok &= parseUint32(getField(packet, "ts"), data.timestampMs);
  data.soilAdc = getField(packet, "adc").toInt();
  data.soilPercent = getField(packet, "hs").toFloat();
  data.betaDeg = getField(packet, "beta").toFloat();
  data.betaDotDegPerHour = getField(packet, "bdot").toFloat();
  data.vibrationRmsG = getField(packet, "arms").toFloat();
  data.pitchDeg = getField(packet, "pitch").toFloat();
  data.rollDeg = getField(packet, "roll").toFloat();
  data.batteryV = getField(packet, "vbat").toFloat();
  ok &= parseUint16(getField(packet, "err"), err);
  data.errorFlags = err;
  return ok;
}

bool parseAckPacket(const String &packet, AckPacket &ack) {
  if (packetType(packet) != PACKET_ACK || !validatePacket(packet)) {
    return false;
  }

  bool ok = true;
  ok &= parseUint8(getField(packet, "gw"), ack.gatewayId);
  ok &= parseUint8(getField(packet, "node"), ack.nodeId);
  ok &= parseUint32(getField(packet, "pid"), ack.packetId);
  ack.status = getField(packet, "status");
  ok &= parseUint32(getField(packet, "sleep"), ack.sleepSeconds);
  ack.alertLevel = getField(packet, "alert");
  return ok;
}

bool parseCommandPacket(const String &packet, CommandPacket &cmd) {
  if (packetType(packet) != PACKET_CMD || !validatePacket(packet)) {
    return false;
  }

  bool ok = true;
  ok &= parseUint8(getField(packet, "gw"), cmd.gatewayId);
  ok &= parseUint8(getField(packet, "node"), cmd.nodeId);
  ok &= parseUint32(getField(packet, "pid"), cmd.packetId);
  cmd.command = getField(packet, "cmd");
  cmd.parameter = getField(packet, "param");
  return ok;
}

} // namespace Protocol
