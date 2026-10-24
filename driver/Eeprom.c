/*
 * ALFA AWUS036ACHM EEPROM / eFuse Subsystem Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "Eeprom.h"
#include <ntddk.h>

extern NTSTATUS AwusUsbVendorRequest(
    PVOID DeviceContext,
    UINT8 RequestType,
    UINT8 Request,
    UINT16 Value,
    UINT16 Index,
    PVOID Buffer,
    UINT16 BufferLength,
    BOOLEAN IsRead
);

NTSTATUS
AwusEepromRead(
    _In_ PVOID DeviceContext,
    _Out_ PAWUS_EEPROM_DATA EepromData
)
{
    NTSTATUS status;
    UINT16 eepromRaw[MT_EEPROM_SIZE / 2];
    UINT16 offset;

    if (!EepromData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(EepromData, sizeof(AWUS_EEPROM_DATA));

    /* Read EEPROM in 16-word (32-byte) blocks */
    for (offset = 0; offset < (MT_EEPROM_SIZE / 2); offset += 16) {
        status = AwusUsbVendorRequest(
            DeviceContext,
            0xC0,
            RT_USB_REQ_EEPROM_READ,
            0,
            offset * 2,
            &eepromRaw[offset],
            16 * sizeof(UINT16),
            TRUE
        );

        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
                "[AWUS] Failed reading EEPROM at offset 0x%X\n", offset * 2));
            break;
        }
    }

    /* Extract MAC Address from offset 0x04..0x09 */
    EepromData->MacAddress[0] = (UCHAR)(eepromRaw[MT_EE_MAC_ADDR_0 / 2] & 0xFF);
    EepromData->MacAddress[1] = (UCHAR)((eepromRaw[MT_EE_MAC_ADDR_0 / 2] >> 8) & 0xFF);
    EepromData->MacAddress[2] = (UCHAR)(eepromRaw[MT_EE_MAC_ADDR_1 / 2] & 0xFF);
    EepromData->MacAddress[3] = (UCHAR)((eepromRaw[MT_EE_MAC_ADDR_1 / 2] >> 8) & 0xFF);
    EepromData->MacAddress[4] = (UCHAR)(eepromRaw[MT_EE_MAC_ADDR_2 / 2] & 0xFF);
    EepromData->MacAddress[5] = (UCHAR)((eepromRaw[MT_EE_MAC_ADDR_2 / 2] >> 8) & 0xFF);

    /* If unprogrammed/all FF, supply default Alfa OUI 00:C0:CA */
    if ((EepromData->MacAddress[0] == 0xFF && EepromData->MacAddress[1] == 0xFF) ||
        (EepromData->MacAddress[0] == 0x00 && EepromData->MacAddress[1] == 0x00)) {
        EepromData->MacAddress[0] = 0x00;
        EepromData->MacAddress[1] = 0xC0;
        EepromData->MacAddress[2] = 0xCA;
        EepromData->MacAddress[3] = 0x76;
        EepromData->MacAddress[4] = 0x10;
        EepromData->MacAddress[5] = 0x01;
    }

    /* Extract Calibrated Target Power Levels */
    EepromData->TargetPower2G = (UINT8)(eepromRaw[MT_EE_2G_TARGET_POWER / 2] & 0xFF);
    EepromData->TargetPower5G = (UINT8)(eepromRaw[MT_EE_5G_TARGET_POWER / 2] & 0xFF);

    /* Frequency Offset (AFC) */
    EepromData->FreqOffset = (INT8)(eepromRaw[MT_EE_FREQ_OFFSET / 2] & 0xFF);

    /* External Front-End Module (PA & LNA) presence */
    EepromData->HasExtPa2G = TRUE;
    EepromData->HasExtPa5G = TRUE;
    EepromData->HasExtLna2G = TRUE;
    EepromData->HasExtLna5G = TRUE;

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] Hardware MAC: %02X:%02X:%02X:%02X:%02X:%02X (2G Target: %u, 5G Target: %u)\n",
        EepromData->MacAddress[0], EepromData->MacAddress[1], EepromData->MacAddress[2],
        EepromData->MacAddress[3], EepromData->MacAddress[4], EepromData->MacAddress[5],
        EepromData->TargetPower2G, EepromData->TargetPower5G));

    return STATUS_SUCCESS;
}
