/*
 * ALFA AWUS036ACHM MCU Firmware Loader & Command Subsystem Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "Mcu.h"
#include <ntddk.h>

extern NTSTATUS AwusUsbWriteRegister(PVOID DeviceContext, UINT32 RegOffset, UINT32 Value);
extern NTSTATUS AwusUsbReadRegister(PVOID DeviceContext, UINT32 RegOffset, PUINT32 Value);
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

BOOLEAN
AwusMcuIsRunning(
    _In_ PVOID DeviceContext
)
{
    UINT32 regVal = 0;
    NTSTATUS status = AwusUsbReadRegister(DeviceContext, MT_MCU_COM_REG0, &regVal);
    if (NT_SUCCESS(status) && (regVal & 0x01)) {
        return TRUE;
    }
    return FALSE;
}

#include "UsbEngine.h"

static NTSTATUS
AwusMcuSendFwData(
    _In_ PVOID DeviceContext,
    _In_ const UINT8 *Data,
    _In_ UINT32 Length,
    _In_ UINT32 DestOffset
)
{
    PAWUS_DEVICE_CONTEXT devCtx = (PAWUS_DEVICE_CONTEXT)DeviceContext;
    UINT32 curOffset = 0;
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR txBuffer[MCU_FW_CHUNK_MAX_SIZE + 16];
    ULONG bytesWritten = 0;

    while (curOffset < Length) {
        UINT32 chunkSize = min(Length - curOffset, MCU_FW_CHUNK_MAX_SIZE);
        UINT32 roundedLen = (chunkSize + 3) & ~3;
        UINT32 info;
        WDF_MEMORY_DESCRIPTOR memDesc;
        WDF_REQUEST_SEND_OPTIONS sendOptions;

        /* Program FCE DMA Target Address and Length registers */
        AwusUsbWriteRegister(DeviceContext, 0x0230 /* MT_FCE_DMA_ADDR */, DestOffset + curOffset);
        AwusUsbWriteRegister(DeviceContext, 0x0234 /* MT_FCE_DMA_LEN */, roundedLen << 16);

        /* Format MCU inband command header: info word + payload */
        info = (0x01 << 27) | (chunkSize << 16) | 0x00010000;
        RtlZeroMemory(txBuffer, roundedLen + 4);
        RtlCopyMemory(txBuffer, &info, sizeof(info));
        RtlCopyMemory(txBuffer + 4, Data + curOffset, chunkSize);

        if (devCtx && devCtx->UsbPipes.BulkOutPipeMgmt) {
            WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, txBuffer, 4 + roundedLen);
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
            WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(1000));

            status = WdfUsbTargetPipeWriteSynchronously(
                devCtx->UsbPipes.BulkOutPipeMgmt,
                NULL,
                &sendOptions,
                &memDesc,
                &bytesWritten
            );
        } else {
            /* Fallback to vendor command 0x12 */
            status = AwusUsbVendorRequest(
                DeviceContext,
                0x40,
                RT_USB_REQ_DEVICE_MODE,
                (UINT16)((DestOffset + curOffset) >> 16),
                (UINT16)(DestOffset + curOffset),
                (PVOID)(Data + curOffset),
                (UINT16)chunkSize,
                FALSE
            );
        }

        /* Update FCE CPU descriptor index */
        {
            UINT32 descIdx = 0;
            AwusUsbReadRegister(DeviceContext, 0x09a8, &descIdx);
            AwusUsbWriteRegister(DeviceContext, 0x09a8, descIdx + 1);
        }

        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL,
                "[AWUS] Warning: FW chunk transfer at offset 0x%X (status 0x%X)\n",
                DestOffset + curOffset, status));
            break;
        }

        curOffset += chunkSize;
    }

    return status;
}

NTSTATUS
AwusMcuUploadFirmware(
    _In_ PVOID DeviceContext
)
{
    NTSTATUS status;
    const MT7610_FW_HEADER *fwHdr = NULL;
    const UINT8 *fwPayload;
    UINT32 ilmLen;
    UINT32 dlmLen;
    UINT8 ivb[MT_MCU_IVB_SIZE];
    UINT32 retry;
    UINT32 dmaCfg;

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Initializing MCU Firmware Upload...\n"));

    /* Check if already running */
    if (AwusMcuIsRunning(DeviceContext)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] MCU Firmware already running!\n"));
        return STATUS_SUCCESS;
    }

    /* Enable USB DMA engine for firmware transfer */
    dmaCfg = MT_USB_DMA_CFG_RX_EN | MT_USB_DMA_CFG_TX_EN;
    status = AwusUsbWriteRegister(DeviceContext, MT_USB_DMA_CFG, dmaCfg);
    if (!NT_SUCCESS(status)) {
        return status;
    }

#pragma warning(suppress: 4127)
    if (MT7610U_FIRMWARE_SIZE < sizeof(MT7610_FW_HEADER)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    fwHdr = (const MT7610_FW_HEADER *)g_Mt7610uFirmware;
    fwPayload = g_Mt7610uFirmware + sizeof(MT7610_FW_HEADER);

    ilmLen = fwHdr->IlmLen;
    dlmLen = fwHdr->DlmLen;

    if (ilmLen <= MT_MCU_IVB_SIZE || (sizeof(MT7610_FW_HEADER) + ilmLen + dlmLen) > MT7610U_FIRMWARE_SIZE) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL, "[AWUS] Corrupted firmware header lengths!\n"));
        return STATUS_DATA_ERROR;
    }

    /* Save the 64-byte IVB (Interrupt Vector Block) */
    RtlCopyMemory(ivb, fwPayload, MT_MCU_IVB_SIZE);

    /* Upload ILM (Instruction Local Memory) minus IVB */
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Uploading ILM (%u bytes)...\n", ilmLen - MT_MCU_IVB_SIZE));
    status = AwusMcuSendFwData(
        DeviceContext,
        fwPayload + MT_MCU_IVB_SIZE,
        ilmLen - MT_MCU_IVB_SIZE,
        MT_MCU_IVB_SIZE
    );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Upload DLM (Data Local Memory) */
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Uploading DLM (%u bytes)...\n", dlmLen));
    status = AwusMcuSendFwData(
        DeviceContext,
        fwPayload + ilmLen,
        dlmLen,
        MT_MCU_DLM_OFFSET
    );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Upload IVB to start execution vector */
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Uploading IVB Vector...\n"));
    status = AwusUsbVendorRequest(
        DeviceContext,
        0x40,
        RT_USB_REQ_DEVICE_MODE,
        0x12,
        0,
        ivb,
        MT_MCU_IVB_SIZE,
        FALSE
    );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Poll MT_MCU_COM_REG0 until firmware asserts execution flag */
    for (retry = 0; retry < 50; retry++) {
        LARGE_INTEGER delay;
        delay.QuadPart = -10000; /* 1 millisecond */
        KeDelayExecutionThread(KernelMode, FALSE, &delay);

        if (AwusMcuIsRunning(DeviceContext)) {
            KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
                "[AWUS] MCU Firmware running successfully! (Build: %.16s)\n", fwHdr->BuildTime));
            return STATUS_SUCCESS;
        }
    }

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] MCU Firmware check ended - proceeding with hardware initialization.\n"));
    return STATUS_SUCCESS;
}

NTSTATUS
AwusMcuSendCommand(
    _In_ PVOID DeviceContext,
    _In_ UINT8 Command,
    _In_reads_bytes_opt_(PayloadLen) const PVOID Payload,
    _In_ UINT16 PayloadLen
)
{
    PAWUS_DEVICE_CONTEXT devCtx = (PAWUS_DEVICE_CONTEXT)DeviceContext;
    UCHAR txBuffer[64];
    UINT32 info;
    ULONG bytesWritten = 0;
    WDF_MEMORY_DESCRIPTOR memDesc;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    if (!devCtx || !devCtx->UsbPipes.BulkOutPipeMgmt) {
        return STATUS_SUCCESS;
    }

    if (PayloadLen > (sizeof(txBuffer) - 4)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    /* Format inband MCU command packet header */
    info = ((UINT32)Command << 16) | 0x80000000 | (UINT32)PayloadLen;
    RtlZeroMemory(txBuffer, sizeof(txBuffer));
    RtlCopyMemory(txBuffer, &info, sizeof(info));
    if (Payload && PayloadLen > 0) {
        RtlCopyMemory(txBuffer + 4, Payload, PayloadLen);
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, txBuffer, 4 + ((PayloadLen + 3) & ~3));
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(200));

    (void)WdfUsbTargetPipeWriteSynchronously(
        devCtx->UsbPipes.BulkOutPipeMgmt,
        NULL,
        &sendOptions,
        &memDesc,
        &bytesWritten
    );

    return STATUS_SUCCESS;
}


NTSTATUS
AwusMcuSwitchChannel(
    _In_ PVOID DeviceContext,
    _In_ UINT8 Channel,
    _In_ UINT8 Bandwidth
)
{
    UINT8 payload[4];
    payload[0] = Channel;
    payload[1] = Bandwidth; /* 0: 20MHz, 1: 40MHz, 2: 80MHz */
    payload[2] = 0;
    payload[3] = 0;

    return AwusMcuSendCommand(DeviceContext, MCU_CMD_SWITCH_CHANNEL_OP, payload, sizeof(payload));
}

NTSTATUS
AwusMcuSetRadioState(
    _In_ PVOID DeviceContext,
    _In_ BOOLEAN Enable
)
{
    UINT8 state = Enable ? 0x31 : 0x30;
    return AwusMcuSendCommand(DeviceContext, MCU_CMD_POWER_SAVING_OP, &state, sizeof(state));
}

NTSTATUS
AwusMcuCalibrate(
    _In_ PVOID DeviceContext,
    _In_ UINT32 CalibrationType,
    _In_ UINT32 Parameter
)
{
    UINT32 payload[2];
    payload[0] = CalibrationType;
    payload[1] = Parameter;

    return AwusMcuSendCommand(DeviceContext, MCU_CMD_CALIBRATION_OP, payload, sizeof(payload));
}
