/*
 * Copyright (C) 2008 Robotics at Maryland
 * Copyright (C) 2008 Joseph Lisee <jlisee@umd.edu>
 * All rights reserved.
 *
 * Author: Joseph Lisee <jlisee@umd.edu>
 * File:  packages/vision/src/device/ISonar.cpp
 */

// Project Includes
#include "vehicle/include/device/ISonar.h"

RAM_CORE_EVENT_TYPE(ram::vehicle::device::ISonar, UPDATE);

namespace ram {
namespace vehicle {
namespace device {

ISonar::ISonar(core::EventHubPtr eventHub, std::string name) :
    IDevice(eventHub, name)
{
}

ISonar::~ISonar()
{
}
    
} // namespace device
} // namespace vehicle
} // namespace ram
