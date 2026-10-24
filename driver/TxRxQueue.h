/*
 * ALFA AWUS036ACHM NetAdapter Datapath Queues (TX/RX)
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "Registers.h"
#include "UsbEngine.h"

typedef struct _AWUS_DATAPATH_STATS {
    UINT64  TxPackets;
    UINT64  TxBytes;
    UINT64  TxErrors;
    UINT64  RxPackets;
    UINT64  RxBytes;
    UINT64  RxCrcErrors;
    UINT32  LastRxRssi;
    UINT32  LastRxSnr;
    UINT32  LastRxRateMbps;
} AWUS_DATAPATH_STATS, *PAWUS_DATAPATH_STATS;

VOID AwusProcessRxFrame(PAWUS_DEVICE_CONTEXT DevCtx, WDFMEMORY Memory, size_t NumBytesTransferred);
NTSTATUS AwusTransmitNetPacket(PAWUS_DEVICE_CONTEXT DevCtx, const PVOID PacketBuffer, UINT32 PacketLength, UINT8 Priority);
