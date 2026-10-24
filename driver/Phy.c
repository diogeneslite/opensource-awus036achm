/*
 * ALFA AWUS036ACHM Baseband & RF Subsystem Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "Phy.h"
#include "PowerDelivery.h"
#include "Mcu.h"
#include <ntddk.h>

extern NTSTATUS AwusUsbWriteRegister(PVOID DeviceContext, UINT32 RegOffset, UINT32 Value);
extern NTSTATUS AwusUsbReadRegister(PVOID DeviceContext, UINT32 RegOffset, PUINT32 Value);

/* Channel Frequency Mapping Table (Dual-Band 2.4G & 5G) */
static const AWUS_RF_CHANNEL_ENTRY g_RfChannels[] = {
    /* 2.4 GHz Channels */
    { 1,  2412000, FALSE, 0x01, 0x00, 0x5B },
    { 2,  2417000, FALSE, 0x01, 0x00, 0x5B },
    { 3,  2422000, FALSE, 0x01, 0x00, 0x5B },
    { 4,  2427000, FALSE, 0x01, 0x00, 0x5B },
    { 5,  2432000, FALSE, 0x01, 0x00, 0x5B },
    { 6,  2437000, FALSE, 0x01, 0x00, 0x5B },
    { 7,  2442000, FALSE, 0x01, 0x00, 0x5B },
    { 8,  2447000, FALSE, 0x01, 0x00, 0x5B },
    { 9,  2452000, FALSE, 0x01, 0x00, 0x5B },
    { 10, 2457000, FALSE, 0x01, 0x00, 0x5B },
    { 11, 2462000, FALSE, 0x01, 0x00, 0x5B },
    { 12, 2467000, FALSE, 0x01, 0x00, 0x5B },
    { 13, 2472000, FALSE, 0x01, 0x00, 0x5B },
    { 14, 2484000, FALSE, 0x01, 0x00, 0x5B },

    /* 5 GHz UNII-1 & UNII-2 (VHT80 Capable) */
    { 36, 5180000, TRUE,  0x02, 0x40, 0x1B },
    { 40, 5200000, TRUE,  0x02, 0x40, 0x1B },
    { 44, 5220000, TRUE,  0x02, 0x40, 0x1B },
    { 48, 5240000, TRUE,  0x02, 0x40, 0x1B },
    { 52, 5260000, TRUE,  0x02, 0x40, 0x1B },
    { 56, 5280000, TRUE,  0x02, 0x40, 0x1B },
    { 60, 5300000, TRUE,  0x02, 0x40, 0x1B },
    { 64, 5320000, TRUE,  0x02, 0x40, 0x1B },

    /* 5 GHz UNII-2 Extended */
    { 100, 5500000, TRUE, 0x02, 0x40, 0x1B },
    { 104, 5520000, TRUE, 0x02, 0x40, 0x1B },
    { 108, 5540000, TRUE, 0x02, 0x40, 0x1B },
    { 112, 5560000, TRUE, 0x02, 0x40, 0x1B },
    { 116, 5580000, TRUE, 0x02, 0x40, 0x1B },
    { 120, 5600000, TRUE, 0x02, 0x40, 0x1B },
    { 124, 5620000, TRUE, 0x02, 0x40, 0x1B },
    { 128, 5640000, TRUE, 0x02, 0x40, 0x1B },
    { 132, 5660000, TRUE, 0x02, 0x40, 0x1B },
    { 136, 5680000, TRUE, 0x02, 0x40, 0x1B },
    { 140, 5700000, TRUE, 0x02, 0x40, 0x1B },

    /* 5 GHz UNII-3 (High Band, High Power) */
    { 149, 5745000, TRUE, 0x02, 0x40, 0x1B },
    { 153, 5765000, TRUE, 0x02, 0x40, 0x1B },
    { 157, 5785000, TRUE, 0x02, 0x40, 0x1B },
    { 161, 5805000, TRUE, 0x02, 0x40, 0x1B },
    { 165, 5825000, TRUE, 0x02, 0x40, 0x1B },
};

NTSTATUS
AwusPhyInit(
    _In_ PVOID DeviceContext
)
{
    NTSTATUS status;

    /* Baseband core reset and initialization */
    status = AwusUsbWriteRegister(DeviceContext, MT_MAC_SYS_CTRL, MT_MAC_SYS_CTRL_RESET_BBP);
    if (!NT_SUCCESS(status)) return status;

    /* Configure MAC burst timing and Short Inter-Frame Space (SIFS) */
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_SW_CFG0, 0x00000404);
    if (!NT_SUCCESS(status)) return status;

    /* Transmit opportunity (TXOP) burst control for peak throughput */
    status = AwusUsbWriteRegister(DeviceContext, MT_TXOP_CTRL_CFG, 0x0000583F);
    if (!NT_SUCCESS(status)) return status;

    /* Enable High-Throughput Aggregation (A-MPDU max length 64KB) */
    status = AwusUsbWriteRegister(DeviceContext, MT_AMPDU_MAX_LEN_20M1S, 0x000FFFFF);
    if (!NT_SUCCESS(status)) return status;

    /* Enable RX & TX engines */
    status = AwusUsbWriteRegister(DeviceContext, MT_MAC_SYS_CTRL,
        MT_MAC_SYS_CTRL_ENABLE_TX | MT_MAC_SYS_CTRL_ENABLE_RX);

    return status;
}

NTSTATUS
AwusPhySetBandwidth(
    _In_ PVOID DeviceContext,
    _In_ UINT8 Bandwidth
)
{
    NTSTATUS status;
    UINT32 bbpVal = 0;

    /* Read BBP register 4 (Bandwidth control) */
    status = AwusUsbReadRegister(DeviceContext, MT_BBP_REG_BASE + 0x04, &bbpVal);
    if (!NT_SUCCESS(status)) return status;

    bbpVal &= ~0x18; /* Clear bandwidth mask */

    switch (Bandwidth) {
    case AWUS_BW_80MHZ:
        bbpVal |= (2 << 3); /* 80 MHz VHT */
        break;
    case AWUS_BW_40MHZ:
        bbpVal |= (1 << 3); /* 40 MHz HT/VHT */
        break;
    case AWUS_BW_20MHZ:
    default:
        bbpVal |= (0 << 3); /* 20 MHz Standard */
        break;
    }

    return AwusUsbWriteRegister(DeviceContext, MT_BBP_REG_BASE + 0x04, bbpVal);
}

NTSTATUS
AwusPhySetChannel(
    _In_ PVOID DeviceContext,
    _In_ UINT8 Channel,
    _In_ UINT8 Bandwidth
)
{
    size_t i;
    BOOLEAN is5G = FALSE;


    /* Locate channel properties */
    for (i = 0; i < sizeof(g_RfChannels) / sizeof(g_RfChannels[0]); i++) {
        if (g_RfChannels[i].Channel == Channel) {
            is5G = g_RfChannels[i].Is5GHz;
            break;
        }
    }

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] Tuning RF Channel %u (%s, BW %s)...\n",
        Channel,
        is5G ? "5 GHz" : "2.4 GHz",
        Bandwidth == AWUS_BW_80MHZ ? "80 MHz" : (Bandwidth == AWUS_BW_40MHZ ? "40 MHz" : "20 MHz")));

    /* Notify MCU firmware of channel and bandwidth switch (best-effort) */
    AwusMcuSwitchChannel(DeviceContext, Channel, Bandwidth);

    /* Program Baseband filter */
    AwusPhySetBandwidth(DeviceContext, Bandwidth);

    /* Re-calibrate Automatic Gain Control (AGC) for new frequency */
    AwusPhyCalibrateAgc(DeviceContext);

    return STATUS_SUCCESS;
}

NTSTATUS
AwusPhyCalibrateAgc(
    _In_ PVOID DeviceContext
)
{
    /* Request MCU full hardware calibration offload */
    return AwusMcuCalibrate(DeviceContext, 1 /* AGC/VGA Calibration */, 0);
}
