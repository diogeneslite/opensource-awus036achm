/*
 * ALFA AWUS036ACHM Maximum Power Delivery Subsystem
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 *
 * Implements high-power Front-End Module (FEM) biasing, per-rate ALC,
 * external PA/LNA drive, and thermal-compensated peak RF output.
 */

#pragma once

#include "Registers.h"

/* Power Delivery Configurations */
#define AWUS_POWER_LEVEL_DEFAULT        0       /* Standard Regulatory */
#define AWUS_POWER_LEVEL_HIGH           1       /* High Performance (+3 dBm) */
#define AWUS_POWER_LEVEL_MAX_BOOST      2       /* Maximum Hardware Power (+6 dBm, Unlocked) */

/* Target Power Limits (0.5 dBm units) */
#define AWUS_MAX_ALLOWED_TX_POWER_2G    60      /* 30 dBm (1000 mW Peak) */
#define AWUS_MAX_ALLOWED_TX_POWER_5G    56      /* 28 dBm (630 mW Peak) */

typedef struct _AWUS_POWER_CONTEXT {
    UINT32 PowerLevelMode;          /* DEFAULT, HIGH, MAX_BOOST */
    BOOLEAN ExtPaEnabled;           /* External Skyworks/Richwave PA enabled */
    BOOLEAN ExtLnaEnabled;          /* External LNA enabled */
    INT32   CurrentTemperature;     /* On-die sensor reading (Celsius) */
    INT32   TempOffset;             /* Calibration thermal offset */
    UINT8   TargetPower2G;          /* EEPROM calibrated target power */
    UINT8   TargetPower5G;          /* EEPROM calibrated target power */
    UINT8   CurChannel;             /* Active channel (1..165) */
    BOOLEAN Is5GHz;                 /* True if 5 GHz band active */
    INT8    PowerDeltaPerRate[16];  /* Modulation rate power adjustments */
} AWUS_POWER_CONTEXT, *PAWUS_POWER_CONTEXT;

/* Power Management Functions */
NTSTATUS AwusPowerInit(PAWUS_POWER_CONTEXT PowerCtx, UINT8 Eeprom2GTarget, UINT8 Eeprom5GTarget);
NTSTATUS AwusPowerSetMode(PAWUS_POWER_CONTEXT PowerCtx, UINT32 PowerMode);
NTSTATUS AwusPowerConfigureChannel(PVOID DeviceContext, PAWUS_POWER_CONTEXT PowerCtx, UINT8 Channel, BOOLEAN Is5GHz);
NTSTATUS AwusPowerApplyRateTable(PVOID DeviceContext, PAWUS_POWER_CONTEXT PowerCtx);
NTSTATUS AwusPowerUpdateThermal(PVOID DeviceContext, PAWUS_POWER_CONTEXT PowerCtx, INT32 RawTempSensor);
