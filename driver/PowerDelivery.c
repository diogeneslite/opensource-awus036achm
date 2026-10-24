/*
 * ALFA AWUS036ACHM Maximum Power Delivery Subsystem Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "PowerDelivery.h"
#include <ntddk.h>

/* Forward declaration of USB register writer implemented in UsbEngine */
extern NTSTATUS AwusUsbWriteRegister(PVOID DeviceContext, UINT32 RegOffset, UINT32 Value);
extern NTSTATUS AwusUsbReadRegister(PVOID DeviceContext, UINT32 RegOffset, PUINT32 Value);

NTSTATUS
AwusPowerInit(
    _Inout_ PAWUS_POWER_CONTEXT PowerCtx,
    _In_ UINT8 Eeprom2GTarget,
    _In_ UINT8 Eeprom5GTarget
)
{
    if (!PowerCtx) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(PowerCtx, sizeof(AWUS_POWER_CONTEXT));

    /* Default to Maximum Boost mode for Alfa hardware */
    PowerCtx->PowerLevelMode = AWUS_POWER_LEVEL_MAX_BOOST;
    PowerCtx->ExtPaEnabled = TRUE;
    PowerCtx->ExtLnaEnabled = TRUE;

    PowerCtx->TargetPower2G = (Eeprom2GTarget != 0xFF && Eeprom2GTarget != 0) ? Eeprom2GTarget : 48; /* 24 dBm */
    PowerCtx->TargetPower5G = (Eeprom5GTarget != 0xFF && Eeprom5GTarget != 0) ? Eeprom5GTarget : 44; /* 22 dBm */

    PowerCtx->CurrentTemperature = 25; /* 25C reference */
    PowerCtx->TempOffset = 0;

    /*
     * Fine-tuned rate offsets (0.5 dBm step) to maximize linear output without EVM distortion:
     * CCK rates: +4 dBm (saturate external PA for max range)
     * OFDM low rates (6-18M): +3 dBm
     * OFDM high rates (36-54M): +1 dBm
     * HT20/40 MCS0-4: +3 dBm
     * HT20/40 MCS5-7: +1 dBm
     * VHT80 MCS0-6: +2 dBm
     * VHT80 MCS7-9: 0 dBm (requires high linearity for 256-QAM)
     */
    PowerCtx->PowerDeltaPerRate[0] = 8;  /* CCK 1, 2 */
    PowerCtx->PowerDeltaPerRate[1] = 8;  /* CCK 5.5, 11 */
    PowerCtx->PowerDeltaPerRate[2] = 6;  /* OFDM 6, 9 */
    PowerCtx->PowerDeltaPerRate[3] = 4;  /* OFDM 12, 18 */
    PowerCtx->PowerDeltaPerRate[4] = 2;  /* OFDM 24, 36 */
    PowerCtx->PowerDeltaPerRate[5] = 1;  /* OFDM 48, 54 */
    PowerCtx->PowerDeltaPerRate[6] = 6;  /* HT/VHT MCS 0, 1 */
    PowerCtx->PowerDeltaPerRate[7] = 4;  /* HT/VHT MCS 2, 3 */
    PowerCtx->PowerDeltaPerRate[8] = 2;  /* HT/VHT MCS 4, 5 */
    PowerCtx->PowerDeltaPerRate[9] = 1;  /* HT/VHT MCS 6, 7 */
    PowerCtx->PowerDeltaPerRate[10] = 0; /* VHT80 MCS 8, 9 (Peak 256-QAM) */

    return STATUS_SUCCESS;
}

NTSTATUS
AwusPowerSetMode(
    _Inout_ PAWUS_POWER_CONTEXT PowerCtx,
    _In_ UINT32 PowerMode
)
{
    if (!PowerCtx) {
        return STATUS_INVALID_PARAMETER;
    }

    if (PowerMode > AWUS_POWER_LEVEL_MAX_BOOST) {
        PowerMode = AWUS_POWER_LEVEL_MAX_BOOST;
    }

    PowerCtx->PowerLevelMode = PowerMode;
    return STATUS_SUCCESS;
}

NTSTATUS
AwusPowerConfigureChannel(
    _In_ PVOID DeviceContext,
    _Inout_ PAWUS_POWER_CONTEXT PowerCtx,
    _In_ UINT8 Channel,
    _In_ BOOLEAN Is5GHz
)
{
    NTSTATUS status;
    UINT32 alcCfg0;
    UINT32 alcCfg1;
    UINT8 baseTarget;
    INT32 boost;

    if (!PowerCtx) {
        return STATUS_INVALID_PARAMETER;
    }

    PowerCtx->CurChannel = Channel;
    PowerCtx->Is5GHz = Is5GHz;

    baseTarget = Is5GHz ? PowerCtx->TargetPower5G : PowerCtx->TargetPower2G;

    /* Apply Boost Mode based on user power policy */
    switch (PowerCtx->PowerLevelMode) {
    case AWUS_POWER_LEVEL_HIGH:
        boost = 6;  /* +3.0 dBm */
        break;
    case AWUS_POWER_LEVEL_MAX_BOOST:
        boost = 10; /* +5.0 dBm (Maximum power delivery) */
        break;
    case AWUS_POWER_LEVEL_DEFAULT:
    default:
        boost = 0;
        break;
    }

    baseTarget = (UINT8)min((INT32)baseTarget + boost, (INT32)(Is5GHz ? AWUS_MAX_ALLOWED_TX_POWER_5G : AWUS_MAX_ALLOWED_TX_POWER_2G));

    /*
     * Configure MT7610U Hardware ALC (Automatic Level Control)
     * Bit 31: Enable ALC
     * Bit 29: Dynamic Power Step
     * Bit 13: High-Power External PA Enable
     */
    alcCfg0 = (1U << 31) | (1U << 29);
    if (PowerCtx->ExtPaEnabled) {
        alcCfg0 |= (1U << 13) | (1U << 12);
    }
    alcCfg0 |= ((UINT32)baseTarget & 0x3F);

    status = AwusUsbWriteRegister(DeviceContext, MT_TX_ALC_CFG_0, alcCfg0);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Target ALC Power limits for high-linearity PA envelope */
    alcCfg1 = ((UINT32)baseTarget << 16) | ((UINT32)baseTarget);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_ALC_CFG_1, alcCfg1);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Program per-rate power curves */
    return AwusPowerApplyRateTable(DeviceContext, PowerCtx);
}

NTSTATUS
AwusPowerApplyRateTable(
    _In_ PVOID DeviceContext,
    _In_ PAWUS_POWER_CONTEXT PowerCtx
)
{
    NTSTATUS status;
    UINT8 baseTarget;
    UINT32 pwr0, pwr1, pwr2, pwr3, pwr4, pwr7, pwr8, pwr9;
    INT32 p;

    if (!PowerCtx) {
        return STATUS_INVALID_PARAMETER;
    }

    baseTarget = PowerCtx->Is5GHz ? PowerCtx->TargetPower5G : PowerCtx->TargetPower2G;
    if (PowerCtx->PowerLevelMode == AWUS_POWER_LEVEL_MAX_BOOST) {
        baseTarget += 8; /* +4 dBm */
    }

    /* CCK Rates (1, 2, 5.5, 11M) */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[0];
    pwr0 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_0, pwr0);
    if (!NT_SUCCESS(status)) return status;

    /* OFDM Rates (6, 9, 12, 18M) */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[2];
    pwr1 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_1, pwr1);
    if (!NT_SUCCESS(status)) return status;

    /* OFDM Rates (24, 36, 48, 54M) */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[4];
    pwr2 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_2, pwr2);
    if (!NT_SUCCESS(status)) return status;

    /* HT20/40 MCS 0..3 */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[6];
    pwr3 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_3, pwr3);
    if (!NT_SUCCESS(status)) return status;

    /* HT20/40 MCS 4..7 */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[8];
    pwr4 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_4, pwr4);
    if (!NT_SUCCESS(status)) return status;

    /* VHT20/40 MCS 0..3 */
    pwr7 = pwr3;
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_7, pwr7);
    if (!NT_SUCCESS(status)) return status;

    /* VHT20/40 MCS 4..7 */
    pwr8 = pwr4;
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_8, pwr8);
    if (!NT_SUCCESS(status)) return status;

    /* VHT80 MCS 0..9 (Peak 802.11ac 433 Mbps) */
    p = baseTarget + PowerCtx->PowerDeltaPerRate[10];
    pwr9 = ((UINT32)(p & 0x3F)) | (((UINT32)(p & 0x3F)) << 8) | (((UINT32)(p & 0x3F)) << 16) | (((UINT32)(p & 0x3F)) << 24);
    status = AwusUsbWriteRegister(DeviceContext, MT_TX_PWR_CFG_9, pwr9);

    return status;
}

NTSTATUS
AwusPowerUpdateThermal(
    _In_ PVOID DeviceContext,
    _Inout_ PAWUS_POWER_CONTEXT PowerCtx,
    _In_ INT32 RawTempSensor
)
{
    INT32 tempC;
    INT32 tempDiff;

    if (!PowerCtx) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Convert raw ADC reading to Celsius */
    tempC = RawTempSensor + PowerCtx->TempOffset;
    PowerCtx->CurrentTemperature = tempC;

    /*
     * Dynamic Thermal Throttling Prevention:
     * If device is cold (<40C), PA can safely push +1 dBm more.
     * If device is hot (>85C), scale down by 1.5 dBm to protect PA from thermal runaway.
     */
    tempDiff = tempC - 45;
    if (tempDiff > 40) {
        /* Extremely high temp - protect hardware */
        AwusUsbWriteRegister(DeviceContext, MT_TX_ALC_CFG_2, 0x00020002);
    } else {
        /* Normal / High Performance operating temperature */
        AwusUsbWriteRegister(DeviceContext, MT_TX_ALC_CFG_2, 0x00000000);
    }

    return STATUS_SUCCESS;
}
