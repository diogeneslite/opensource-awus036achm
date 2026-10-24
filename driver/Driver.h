/*
 * ALFA AWUS036ACHM Windows 11 Driver Entry & WDF Lifecycle
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"
#include "UsbEngine.h"
#include "PowerDelivery.h"
#include "Mcu.h"
#include "Phy.h"
#include "Eeprom.h"
#include "TxRxQueue.h"

#define AWUS_DEVICE_NAME        L"\\Device\\Awus036achm"
#define AWUS_DOS_DEVICE_NAME    L"\\DosDevices\\Awus036achm"

/* IOCTLs for user-mode power & performance tuning */
#define IOCTL_AWUS_GET_TELEMETRY    CTL_CODE(FILE_DEVICE_NETWORK, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AWUS_SET_POWER_MODE   CTL_CODE(FILE_DEVICE_NETWORK, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AWUS_SET_CHANNEL      CTL_CODE(FILE_DEVICE_NETWORK, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _AWUS_TELEMETRY_DATA {
    UCHAR   MacAddress[6];
    UINT32  PowerMode;
    INT32   CurrentTempC;
    UINT8   CurrentChannel;
    BOOLEAN Is5GHz;
    UINT64  TxPackets;
    UINT64  RxPackets;
    UINT32  LastRssi;
    UINT32  LastSnr;
} AWUS_TELEMETRY_DATA, *PAWUS_TELEMETRY_DATA;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AwusEvtDeviceAdd;
EVT_WDF_DRIVER_UNLOAD AwusEvtDriverUnload;
EVT_WDF_DEVICE_PREPARE_HARDWARE AwusEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE AwusEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY AwusEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT AwusEvtDeviceD0Exit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL AwusEvtIoDeviceControl;
NTSTATUS AwusLedInit(PVOID DeviceContext);

