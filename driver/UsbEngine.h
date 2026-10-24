/*
 * ALFA AWUS036ACHM USB Engine Subsystem
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"
#include <wdf.h>
#include <usb.h>
#include <usbspec.h>
#include <wdfusb.h>

typedef struct _AWUS_USB_PIPE_INFO {
    WDFUSBPIPE BulkInPipe;
    WDFUSBPIPE BulkOutPipeMgmt;
    WDFUSBPIPE BulkOutPipeVoice;
    WDFUSBPIPE BulkOutPipeVideo;
    WDFUSBPIPE BulkOutPipeBestEffort;
    WDFUSBPIPE BulkOutPipeBackground;
} AWUS_USB_PIPE_INFO, *PAWUS_USB_PIPE_INFO;

typedef struct _AWUS_DEVICE_CONTEXT {
    WDFDEVICE           WdfDevice;
    WDFUSBDEVICE        UsbDevice;
    AWUS_USB_PIPE_INFO  UsbPipes;
    UCHAR               PermanentMacAddress[6];
    UCHAR               CurrentMacAddress[6];
    BOOLEAN             FirmwareLoaded;
    PVOID               NetAdapter;
    PVOID               PowerContext;
} AWUS_DEVICE_CONTEXT, *PAWUS_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(AWUS_DEVICE_CONTEXT, AwusGetDeviceContext)

/* USB Control Engine APIs */
NTSTATUS AwusUsbInit(PAWUS_DEVICE_CONTEXT DevCtx);
NTSTATUS AwusUsbWriteRegister(PVOID DeviceContext, UINT32 RegOffset, UINT32 Value);
NTSTATUS AwusUsbReadRegister(PVOID DeviceContext, UINT32 RegOffset, PUINT32 Value);
NTSTATUS AwusUsbVendorRequest(
    PVOID DeviceContext,
    UINT8 RequestType,
    UINT8 Request,
    UINT16 Value,
    UINT16 Index,
    PVOID Buffer,
    UINT16 BufferLength,
    BOOLEAN IsRead
);

/* Bulk Data Plane APIs */
NTSTATUS AwusUsbStartRxReader(PAWUS_DEVICE_CONTEXT DevCtx);
VOID     AwusUsbStopRxReader(PAWUS_DEVICE_CONTEXT DevCtx);
NTSTATUS AwusUsbTransmitFrame(PAWUS_DEVICE_CONTEXT DevCtx, UINT8 QueuePriority, const PVOID FrameData, UINT32 FrameLen);
