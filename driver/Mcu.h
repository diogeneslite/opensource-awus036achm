/*
 * ALFA AWUS036ACHM MCU Firmware Loader & Command Subsystem
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"
#include "Firmware.h"

#define MCU_FW_CHUNK_MAX_SIZE   0x38F8

#pragma pack(push, 1)
typedef struct _MT7610_FW_HEADER {
    UINT32 IlmLen;
    UINT32 DlmLen;
    UINT16 BuildVer;
    UINT16 FwVer;
    UINT8  Pad[4];
    CHAR   BuildTime[16];
} MT7610_FW_HEADER, *PMT7610_FW_HEADER;
#pragma pack(pop)

NTSTATUS AwusMcuUploadFirmware(PVOID DeviceContext);
BOOLEAN  AwusMcuIsRunning(PVOID DeviceContext);
NTSTATUS AwusMcuSwitchChannel(PVOID DeviceContext, UINT8 Channel, UINT8 Bandwidth);
NTSTATUS AwusMcuSetRadioState(PVOID DeviceContext, BOOLEAN Enable);
NTSTATUS AwusMcuCalibrate(PVOID DeviceContext, UINT32 CalibrationType, UINT32 Parameter);
NTSTATUS AwusMcuSendCommand(PVOID DeviceContext, UINT8 Command, const PVOID Payload, UINT16 PayloadLen);
