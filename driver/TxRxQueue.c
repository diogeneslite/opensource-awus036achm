/*
 * ALFA AWUS036ACHM NetAdapter Datapath Queues (TX/RX) Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "TxRxQueue.h"
#include <ntddk.h>

static AWUS_DATAPATH_STATS g_DatapathStats = { 0 };

VOID
AwusProcessRxFrame(
    _In_ PAWUS_DEVICE_CONTEXT DevCtx,
    _In_ WDFMEMORY Memory,
    _In_ size_t NumBytesTransferred
)
{
    PUCHAR buffer;
    PMT7610_RXWI rxwi;
    PUCHAR payload;
    size_t payloadLen;

    UNREFERENCED_PARAMETER(DevCtx);

    buffer = (PUCHAR)WdfMemoryGetBuffer(Memory, NULL);
    if (!buffer || NumBytesTransferred < sizeof(MT7610_RXWI)) {
        return;
    }

    rxwi = (PMT7610_RXWI)buffer;

    /* Drop CRC errors */
    if (rxwi->CrcErr) {
        g_DatapathStats.RxCrcErrors++;
        return;
    }

    payload = buffer + sizeof(MT7610_RXWI);
    payloadLen = NumBytesTransferred - sizeof(MT7610_RXWI);

    /* Update real-time RF telemetry */
    g_DatapathStats.RxPackets++;
    g_DatapathStats.RxBytes += payloadLen;
    g_DatapathStats.LastRxRssi = rxwi->RSSI;
    g_DatapathStats.LastRxSnr = rxwi->SNR;

    /* In a full NetAdapterCx pipeline, the payload is inserted into NetRing RX queue */
    /* When monitor mode is active, the raw 802.11 frame + radiotap is delivered */
}

NTSTATUS
AwusTransmitNetPacket(
    _In_ PAWUS_DEVICE_CONTEXT DevCtx,
    _In_reads_bytes_(PacketLength) const PVOID PacketBuffer,
    _In_ UINT32 PacketLength,
    _In_ UINT8 Priority
)
{
    NTSTATUS status;
    UCHAR txBuffer[2304 + sizeof(MT7610_TXWI)];
    PMT7610_TXWI txwi;
    UINT32 totalLen;

    if (PacketLength > 2304) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlZeroMemory(txBuffer, sizeof(txBuffer));
    txwi = (PMT7610_TXWI)txBuffer;

    /* Format TXWI Descriptor for High Throughput (VHT80 / HT40) */
    txwi->FrgNum = 0;
    txwi->Mmps = 0;
    txwi->CFACK = 0;
    txwi->TS = 0;
    txwi->AMPDU = 1;          /* Hardware A-MPDU aggregation enabled */
    txwi->MpduDensity = 4;
    txwi->TxPwrAdj = 0;
    txwi->MCS = 7;            /* Default peak MCS */
    txwi->BW = 2;             /* 80 MHz VHT */
    txwi->SGI = 1;            /* Short Guard Interval (400ns) */
    txwi->Stbc = 0;
    txwi->eTxBF = 1;          /* Explicit Transmit Beamforming */
    txwi->PhyMode = 4;        /* VHT (802.11ac) */
    txwi->TxOp = 0;
    txwi->Ack = 1;
    txwi->Length = PacketLength;

    /* Copy Ethernet / 802.11 payload following TXWI */
    RtlCopyMemory(txBuffer + sizeof(MT7610_TXWI), PacketBuffer, PacketLength);
    totalLen = sizeof(MT7610_TXWI) + PacketLength;

    /* Transmit via USB Bulk OUT endpoint */
    status = AwusUsbTransmitFrame(DevCtx, Priority, txBuffer, totalLen);
    if (NT_SUCCESS(status)) {
        g_DatapathStats.TxPackets++;
        g_DatapathStats.TxBytes += PacketLength;
    } else {
        g_DatapathStats.TxErrors++;
    }

    return status;
}
