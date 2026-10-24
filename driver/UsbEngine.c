/*
 * ALFA AWUS036ACHM USB Engine Subsystem Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "UsbEngine.h"

/* Forward declaration for RX packet processing */
extern VOID AwusProcessRxFrame(PAWUS_DEVICE_CONTEXT DevCtx, WDFMEMORY Memory, size_t NumBytesTransferred);

NTSTATUS
AwusUsbVendorRequest(
    _In_ PVOID DeviceContext,
    _In_ UINT8 RequestType,
    _In_ UINT8 Request,
    _In_ UINT16 Value,
    _In_ UINT16 Index,
    _Inout_updates_bytes_opt_(BufferLength) PVOID Buffer,
    _In_ UINT16 BufferLength,
    _In_ BOOLEAN IsRead
)
{
    PAWUS_DEVICE_CONTEXT devCtx = (PAWUS_DEVICE_CONTEXT)DeviceContext;
    WDF_USB_CONTROL_SETUP_PACKET setupPacket;
    WDF_MEMORY_DESCRIPTOR memDesc;
    PWDF_MEMORY_DESCRIPTOR pMemDesc = NULL;
    ULONG bytesTransferred = 0;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    UNREFERENCED_PARAMETER(RequestType);

    if (!devCtx || !devCtx->UsbDevice) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (Buffer && BufferLength > 0) {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, Buffer, BufferLength);
        pMemDesc = &memDesc;
    }

    if (IsRead) {
        WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&setupPacket,
            BmRequestDeviceToHost,
            BmRequestToDevice,
            Request,
            Value,
            Index);
    } else {
        WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&setupPacket,
            BmRequestHostToDevice,
            BmRequestToDevice,
            Request,
            Value,
            Index);
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(1000));

    return WdfUsbTargetDeviceSendControlTransferSynchronously(
        devCtx->UsbDevice,
        NULL,
        &sendOptions,
        &setupPacket,
        pMemDesc,
        &bytesTransferred
    );
}

NTSTATUS
AwusUsbWriteRegister(
    _In_ PVOID DeviceContext,
    _In_ UINT32 RegOffset,
    _In_ UINT32 Value
)
{
    UINT32 leVal = Value;
    return AwusUsbVendorRequest(
        DeviceContext,
        0x40,
        RT_USB_REQ_EP0_WRITE,
        (UINT16)(leVal & 0xFFFF),
        (UINT16)RegOffset,
        &leVal,
        sizeof(UINT32),
        FALSE
    );
}

NTSTATUS
AwusUsbReadRegister(
    _In_ PVOID DeviceContext,
    _In_ UINT32 RegOffset,
    _Out_ PUINT32 Value
)
{
    NTSTATUS status;
    UINT32 leVal = 0;

    if (!Value) {
        return STATUS_INVALID_PARAMETER;
    }

    status = AwusUsbVendorRequest(
        DeviceContext,
        0xC0,
        RT_USB_REQ_EP0_READ,
        0,
        (UINT16)RegOffset,
        &leVal,
        sizeof(UINT32),
        TRUE
    );

    if (NT_SUCCESS(status)) {
        *Value = leVal;
    }
    return status;
}

static EVT_WDF_USB_READER_COMPLETION_ROUTINE AwusEvtUsbRxCompletionRoutine;
static EVT_WDF_USB_READERS_FAILED AwusEvtUsbRxReadersPipelineFailed;

VOID
AwusEvtUsbRxCompletionRoutine(
    _In_ WDFUSBPIPE Pipe,
    _In_ WDFMEMORY Memory,
    _In_ size_t NumBytesTransferred,
    _In_ WDFCONTEXT Context
)
{
    PAWUS_DEVICE_CONTEXT devCtx = (PAWUS_DEVICE_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Pipe);

    if (NumBytesTransferred >= sizeof(MT7610_RXWI)) {
        AwusProcessRxFrame(devCtx, Memory, NumBytesTransferred);
    }
}

BOOLEAN
AwusEvtUsbRxReadersPipelineFailed(
    _In_ WDFUSBPIPE Pipe,
    _In_ NTSTATUS Status,
    _In_ USBD_STATUS UsbdStatus
)
{
    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(UsbdStatus);

    /* Keep continuous reader active despite transient transport glitches */
    return TRUE;
}

NTSTATUS
AwusUsbInit(
    _Inout_ PAWUS_DEVICE_CONTEXT DevCtx
)
{
    NTSTATUS status;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    WDFUSBINTERFACE usbInterface;
    UCHAR numPipes;
    UCHAR i;

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);
    status = WdfUsbTargetDeviceSelectConfig(DevCtx->UsbDevice, WDF_NO_OBJECT_ATTRIBUTES, &configParams);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    usbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;
    numPipes = WdfUsbInterfaceGetNumConfiguredPipes(usbInterface);

    for (i = 0; i < numPipes; i++) {
        WDFUSBPIPE pipe;
        WDF_USB_PIPE_INFORMATION pipeInfo;

        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
        pipe = WdfUsbInterfaceGetConfiguredPipe(usbInterface, i, &pipeInfo);

        if (pipeInfo.PipeType == WdfUsbPipeTypeBulk) {
            if (WdfUsbTargetPipeIsInEndpoint(pipe)) {
                DevCtx->UsbPipes.BulkInPipe = pipe;
            } else {
                /* Map hardware TX queue endpoints */
                if (!DevCtx->UsbPipes.BulkOutPipeMgmt) {
                    DevCtx->UsbPipes.BulkOutPipeMgmt = pipe;
                } else if (!DevCtx->UsbPipes.BulkOutPipeVoice) {
                    DevCtx->UsbPipes.BulkOutPipeVoice = pipe;
                } else if (!DevCtx->UsbPipes.BulkOutPipeVideo) {
                    DevCtx->UsbPipes.BulkOutPipeVideo = pipe;
                } else if (!DevCtx->UsbPipes.BulkOutPipeBestEffort) {
                    DevCtx->UsbPipes.BulkOutPipeBestEffort = pipe;
                } else {
                    DevCtx->UsbPipes.BulkOutPipeBackground = pipe;
                }
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
AwusUsbStartRxReader(
    _In_ PAWUS_DEVICE_CONTEXT DevCtx
)
{
    NTSTATUS status;
    WDF_USB_CONTINUOUS_READER_CONFIG readerConfig;

    if (!DevCtx->UsbPipes.BulkInPipe) {
        return STATUS_DEVICE_NOT_READY;
    }

    /* Configure continuous asynchronous RX reader */
    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(
        &readerConfig,
        AwusEvtUsbRxCompletionRoutine,
        DevCtx,
        8192 /* Accommodate aggregated frames and RXWI descriptors */
    );


    readerConfig.NumPendingReads = 8; /* 8 concurrent outstanding URBs for peak throughput */
    readerConfig.EvtUsbTargetPipeReadersFailed = AwusEvtUsbRxReadersPipelineFailed;

    status = WdfUsbTargetPipeConfigContinuousReader(DevCtx->UsbPipes.BulkInPipe, &readerConfig);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DevCtx->UsbPipes.BulkInPipe));
}

VOID
AwusUsbStopRxReader(
    _In_ PAWUS_DEVICE_CONTEXT DevCtx
)
{
    if (DevCtx->UsbPipes.BulkInPipe) {
        WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DevCtx->UsbPipes.BulkInPipe), WdfIoTargetCancelSentIo);
    }
}

NTSTATUS
AwusUsbTransmitFrame(
    _In_ PAWUS_DEVICE_CONTEXT DevCtx,
    _In_ UINT8 QueuePriority,
    _In_reads_bytes_(FrameLen) const PVOID FrameData,
    _In_ UINT32 FrameLen
)
{
    WDFUSBPIPE targetPipe;
    WDF_MEMORY_DESCRIPTOR memDesc;
    ULONG bytesWritten = 0;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    /* Select destination endpoint by QoS queue priority */
    switch (QueuePriority) {
    case 3: /* Voice */
        targetPipe = DevCtx->UsbPipes.BulkOutPipeVoice ? DevCtx->UsbPipes.BulkOutPipeVoice : DevCtx->UsbPipes.BulkOutPipeBestEffort;
        break;
    case 2: /* Video */
        targetPipe = DevCtx->UsbPipes.BulkOutPipeVideo ? DevCtx->UsbPipes.BulkOutPipeVideo : DevCtx->UsbPipes.BulkOutPipeBestEffort;
        break;
    case 1: /* Background */
        targetPipe = DevCtx->UsbPipes.BulkOutPipeBackground ? DevCtx->UsbPipes.BulkOutPipeBackground : DevCtx->UsbPipes.BulkOutPipeBestEffort;
        break;
    case 0: /* Best Effort */
    default:
        targetPipe = DevCtx->UsbPipes.BulkOutPipeBestEffort ? DevCtx->UsbPipes.BulkOutPipeBestEffort : DevCtx->UsbPipes.BulkOutPipeMgmt;
        break;
    }

    if (!targetPipe) {
        return STATUS_DEVICE_NOT_READY;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, (PVOID)FrameData, FrameLen);
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(500));

    return WdfUsbTargetPipeWriteSynchronously(
        targetPipe,
        NULL,
        &sendOptions,
        &memDesc,
        &bytesWritten
    );
}
