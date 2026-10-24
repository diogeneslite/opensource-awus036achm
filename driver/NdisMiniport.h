/*
 * ALFA AWUS036ACHM NDIS 6.x Native 802.11 Miniport Definitions
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#ifndef NDIS_MINIPORT_DRIVER
#define NDIS_MINIPORT_DRIVER 1
#endif

#ifndef NDIS630_MINIPORT
#define NDIS630_MINIPORT 1
#endif

#ifndef NDIS_WDM
#define NDIS_WDM 1
#endif

#include <ntddk.h>
#include <wdf.h>
#include <wdfminiport.h>
#include <ndis.h>
#include <windot11.h>
#include <ndis/wifiattributes.h>

#include "Registers.h"
#include "UsbEngine.h"
#include "PowerDelivery.h"
#include "Eeprom.h"

#define AWUS_NDIS_MAJOR_VERSION     6
#define AWUS_NDIS_MINOR_VERSION     30
#define AWUS_DRIVER_MAJOR_VERSION   23
#define AWUS_DRIVER_MINOR_VERSION   1

/* Forward declarations */
typedef struct _AWUS_NDIS_ADAPTER AWUS_NDIS_ADAPTER, *PAWUS_NDIS_ADAPTER;

struct _AWUS_NDIS_ADAPTER {
    NDIS_HANDLE                 NdisAdapterHandle;
    WDFDEVICE                   WdfDevice;
    PAWUS_DEVICE_CONTEXT        DevCtx;
    UCHAR                       PermanentMac[6];
    UCHAR                       CurrentMac[6];
    BOOLEAN                     RadioOn;
    ULONG                       CurrentOpMode;
    ULONG                       PacketFilter;
    NDIS_MEDIA_CONNECT_STATE    ConnectState;
    ULONG64                     LinkSpeed;
    ULONG                       CurrentChannel;
    BOOLEAN                     Is5GHz;
    ULONG                       AutoConfigEnabled;
    NDIS_SPIN_LOCK              Lock;

    /* Native 802.11 Persistent Attributes */
    DOT11_PHY_ATTRIBUTES        PhyAttributes[2];
    DOT11_EXTSTA_ATTRIBUTES     ExtStaAttributes;
};

/* NDIS Miniport Driver Characteristics Callbacks */
MINIPORT_INITIALIZE             AwusMiniportInitializeEx;
MINIPORT_HALT                   AwusMiniportHaltEx;
MINIPORT_UNLOAD                 AwusMiniportDriverUnload;
MINIPORT_PAUSE                  AwusMiniportPause;
MINIPORT_RESTART                AwusMiniportRestart;
MINIPORT_OID_REQUEST            AwusMiniportOidRequest;
MINIPORT_CANCEL_OID_REQUEST     AwusMiniportCancelOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS  AwusMiniportSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS AwusMiniportReturnNetBufferLists;
MINIPORT_CANCEL_SEND            AwusMiniportCancelSend;
MINIPORT_CHECK_FOR_HANG         AwusMiniportCheckForHangEx;
MINIPORT_RESET                  AwusMiniportResetEx;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY AwusMiniportDevicePnPEventNotify;
MINIPORT_SHUTDOWN               AwusMiniportShutdownEx;

/* Global Handles */
extern WDFDRIVER g_WdfDriver;
extern NDIS_HANDLE g_NdisMiniportDriverHandle;
