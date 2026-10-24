/*
 * ALFA AWUS036ACHM Baseband & RF Subsystem
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"

#define AWUS_BW_20MHZ   0
#define AWUS_BW_40MHZ   1
#define AWUS_BW_80MHZ   2

typedef struct _AWUS_RF_CHANNEL_ENTRY {
    UINT8  Channel;
    UINT32 RfFreqKHz;
    BOOLEAN Is5GHz;
    UINT8  RfR01;
    UINT8  RfR02;
    UINT8  RfR42;
} AWUS_RF_CHANNEL_ENTRY, *PAWUS_RF_CHANNEL_ENTRY;

NTSTATUS AwusPhyInit(PVOID DeviceContext);
NTSTATUS AwusPhySetChannel(PVOID DeviceContext, UINT8 Channel, UINT8 Bandwidth);
NTSTATUS AwusPhySetBandwidth(PVOID DeviceContext, UINT8 Bandwidth);
NTSTATUS AwusPhyCalibrateAgc(PVOID DeviceContext);
