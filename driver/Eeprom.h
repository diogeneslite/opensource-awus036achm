/*
 * ALFA AWUS036ACHM EEPROM / eFuse Subsystem
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"

#define MT_EEPROM_SIZE  512

typedef struct _AWUS_EEPROM_DATA {
    UCHAR   MacAddress[6];
    UINT8   TargetPower2G;
    UINT8   TargetPower5G;
    INT8    FreqOffset;
    BOOLEAN HasExtPa2G;
    BOOLEAN HasExtPa5G;
    BOOLEAN HasExtLna2G;
    BOOLEAN HasExtLna5G;
} AWUS_EEPROM_DATA, *PAWUS_EEPROM_DATA;

NTSTATUS AwusEepromRead(PVOID DeviceContext, PAWUS_EEPROM_DATA EepromData);
