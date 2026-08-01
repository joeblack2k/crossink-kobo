#include "HalTiltSensor.h"

HalTiltSensor halTiltSensor;

void HalTiltSensor::update(std::uint8_t /*mode*/, std::uint8_t /*orientation*/, bool /*inReader*/) {}

void HalTiltSensor::update(std::uint8_t /*mode*/, std::uint8_t /*direction*/, std::uint8_t /*orientation*/,
                           bool /*inReader*/) {}
