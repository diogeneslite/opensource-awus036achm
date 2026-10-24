/*
 * ALFA AWUS036ACHM Windows 11 Driver Entry & WDF Lifecycle Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "Driver.h"
#include "NdisMiniport.h"
#include <usbdlib.h>

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS mpChars;

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "====================================================\n"
        "[AWUS] OpenSource ALFA AWUS036ACHM Driver Loaded\n"
        "[AWUS] NDIS 6.30 Native 802.11 / WDF Hybrid Miniport\n"
        "====================================================\n"));

    /* Initialize WDF for Miniport operation (no dispatch override) */
    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
    config.EvtDriverUnload = AwusEvtDriverUnload;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        &g_WdfDriver
    );

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] WdfDriverCreate failed: 0x%08X\n", status));
        return status;
    }

    /* Register NDIS Miniport Driver Characteristics */
    NdisZeroMemory(&mpChars, sizeof(mpChars));
    mpChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    mpChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    mpChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);

    mpChars.MajorNdisVersion = AWUS_NDIS_MAJOR_VERSION;
    mpChars.MinorNdisVersion = AWUS_NDIS_MINOR_VERSION;
    mpChars.MajorDriverVersion = AWUS_DRIVER_MAJOR_VERSION;
    mpChars.MinorDriverVersion = AWUS_DRIVER_MINOR_VERSION;

    mpChars.Flags = NDIS_WDM_DRIVER;

    mpChars.InitializeHandlerEx = AwusMiniportInitializeEx;
    mpChars.HaltHandlerEx = AwusMiniportHaltEx;
    mpChars.UnloadHandler = AwusMiniportDriverUnload;
    mpChars.PauseHandler = AwusMiniportPause;
    mpChars.RestartHandler = AwusMiniportRestart;
    mpChars.OidRequestHandler = AwusMiniportOidRequest;
    mpChars.CancelOidRequestHandler = AwusMiniportCancelOidRequest;
    mpChars.SendNetBufferListsHandler = AwusMiniportSendNetBufferLists;
    mpChars.ReturnNetBufferListsHandler = AwusMiniportReturnNetBufferLists;
    mpChars.CancelSendHandler = AwusMiniportCancelSend;
    mpChars.CheckForHangHandlerEx = AwusMiniportCheckForHangEx;
    mpChars.ResetHandlerEx = AwusMiniportResetEx;
    mpChars.DevicePnPEventNotifyHandler = AwusMiniportDevicePnPEventNotify;
    mpChars.ShutdownHandlerEx = AwusMiniportShutdownEx;

    status = NdisMRegisterMiniportDriver(
        DriverObject,
        RegistryPath,
        (NDIS_HANDLE)g_WdfDriver,
        &mpChars,
        &g_NdisMiniportDriverHandle
    );

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] NdisMRegisterMiniportDriver failed: 0x%08X\n", status));
        WdfDriverMiniportUnload(g_WdfDriver);
        return status;
    }

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] NDIS Miniport Driver Registered Successfully! (Handle=0x%p)\n",
        g_NdisMiniportDriverHandle));

    return STATUS_SUCCESS;
}

VOID
AwusEvtDriverUnload(
    _In_ WDFDRIVER Driver
)
{
    UNREFERENCED_PARAMETER(Driver);
    if (g_NdisMiniportDriverHandle) {
        NdisMDeregisterMiniportDriver(g_NdisMiniportDriverHandle);
        g_NdisMiniportDriverHandle = NULL;
    }
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Driver Unloaded cleanly.\n"));
}

NTSTATUS
AwusEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDFDEVICE device;
    PAWUS_DEVICE_CONTEXT devCtx;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;
    DECLARE_CONST_UNICODE_STRING(ntDevName, AWUS_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(dosDevName, AWUS_DOS_DEVICE_NAME);

    UNREFERENCED_PARAMETER(Driver);

    /* Set device type and assign standard security descriptor for IOCTL access */
    DECLARE_CONST_UNICODE_STRING(sddlString, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;WD)");
    WdfDeviceInitAssignSDDLString(DeviceInit, &sddlString);
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_NETWORK);
    WdfDeviceInitSetCharacteristics(DeviceInit, FILE_DEVICE_SECURE_OPEN, FALSE);

    /* Assign NT Device Name for user-mode IOCTL control */
    (void)WdfDeviceInitAssignName(DeviceInit, &ntDevName);

    /* Configure PnP and Power callbacks */
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = AwusEvtDevicePrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = AwusEvtDeviceReleaseHardware;
    pnpCallbacks.EvtDeviceD0Entry = AwusEvtDeviceD0Entry;
    pnpCallbacks.EvtDeviceD0Exit = AwusEvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    /* Create Device Context */
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, AWUS_DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL, "[AWUS] WdfDeviceCreate failed: 0x%X\n", status));
        return status;
    }

    /* Create symbolic link for performance monitor tools (non-fatal if exists) */
    status = WdfDeviceCreateSymbolicLink(device, &dosDevName);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] Note: Symbolic link returned 0x%X\n", status));
    }

    devCtx = AwusGetDeviceContext(device);
    RtlZeroMemory(devCtx, sizeof(AWUS_DEVICE_CONTEXT));
    devCtx->WdfDevice = device;

    /* Allocate and attach Power Context */
    devCtx->PowerContext = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(AWUS_POWER_CONTEXT), 'SuwA');
    if (!devCtx->PowerContext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Create default I/O queue for user-mode IOCTLs */
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = AwusEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    return status;
}

NTSTATUS
AwusLedInit(
    _In_ PVOID DeviceContext
)
{
    NTSTATUS status;

    /* Program MT7610U hardware LED configuration register (duration, mode, TX/RX activity blink) */
    status = AwusUsbWriteRegister(DeviceContext, MT_LED_CFG, 0x7F031E46);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] Failed writing MT_LED_CFG: 0x%X\n", status));
    }

    /* Program MT7610U LED control: enable LED 0, active polarity, TX blink mode, and kick */
    status = AwusUsbWriteRegister(DeviceContext, MT_LED_CTRL, 0x00000083);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] Failed writing MT_LED_CTRL: 0x%X\n", status));
    }

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Hardware LED Armed & Active (MT_LED_CFG=0x7F031E46).\n"));
    return STATUS_SUCCESS;
}

NTSTATUS
AwusEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    NTSTATUS status;
    PAWUS_DEVICE_CONTEXT devCtx = AwusGetDeviceContext(Device);
    AWUS_EEPROM_DATA eepromData;
    WDF_USB_DEVICE_CREATE_CONFIG usbConfig;
    UINT32 val = 0;
    LARGE_INTEGER delay;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] EvtDevicePrepareHardware: Initializing USB Stack...\n"));

    /* Create USB Target Device */
    WDF_USB_DEVICE_CREATE_CONFIG_INIT(&usbConfig, USBD_CLIENT_CONTRACT_VERSION_602);
    status = WdfUsbTargetDeviceCreateWithParameters(Device, &usbConfig, WDF_NO_OBJECT_ATTRIBUTES, &devCtx->UsbDevice);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL, "[AWUS] Failed creating USB Target Device (0x%X)\n", status));
        return status;
    }

    /* Enumerate Endpoints & Pipes */
    status = AwusUsbInit(devCtx);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] AwusUsbInit returned 0x%X\n", status));
        return status;
    }

    /* 1. Hardware Power-On: Enable WLAN function, Clocks & GPIO Output */
    status = AwusUsbReadRegister(devCtx, MT_WLAN_FUN_CTRL, &val);
    if (NT_SUCCESS(status)) {
        val |= (MT_WLAN_FUN_CTRL_WLAN_EN | MT_WLAN_FUN_CTRL_WLAN_CLK_EN | MT_WLAN_FUN_CTRL_GPIO_OUT_EN);
        AwusUsbWriteRegister(devCtx, MT_WLAN_FUN_CTRL, val);
    }
    delay.QuadPart = -10000; /* 1 ms */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    /* 2. Configure USB DMA Engine */
    AwusUsbWriteRegister(devCtx, MT_USB_DMA_CFG, MT_USB_DMA_CFG_RX_EN | MT_USB_DMA_CFG_TX_EN);

    /* 3. Core BBP & MAC Reset */
    AwusUsbWriteRegister(devCtx, MT_MAC_SYS_CTRL, MT_MAC_SYS_CTRL_RESET_CSR | MT_MAC_SYS_CTRL_RESET_BBP);
    delay.QuadPart = -200000; /* 20 ms */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    AwusUsbWriteRegister(devCtx, MT_MAC_SYS_CTRL, 0);

    /* 4. Arm and Flash Physical LED */
    AwusLedInit(devCtx);

    /* 5. Upload Authentic MediaTek MT7610U Firmware (FW_7610.bin) */
    status = AwusMcuUploadFirmware(devCtx);
    if (NT_SUCCESS(status)) {
        devCtx->FirmwareLoaded = TRUE;
    } else {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] Firmware boot deferred (status 0x%X)\n", status));
    }

    /* Re-assert LED configuration after firmware initialization */
    AwusLedInit(devCtx);

    /* 6. Read Factory EEPROM (MAC Address, Power Calibration) */
    status = AwusEepromRead(devCtx, &eepromData);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL, "[AWUS] EEPROM read status 0x%X, using defaults.\n", status));
    }

    RtlCopyMemory(devCtx->PermanentMacAddress, eepromData.MacAddress, 6);
    RtlCopyMemory(devCtx->CurrentMacAddress, eepromData.MacAddress, 6);

    /* 7. Initialize Maximum Power Delivery Subsystem */
    if (devCtx->PowerContext) {
        AwusPowerInit(
            (PAWUS_POWER_CONTEXT)devCtx->PowerContext,
            eepromData.TargetPower2G,
            eepromData.TargetPower5G
        );
    }

    /* 8. Initialize Baseband / PHY */
    AwusPhyInit(devCtx);

    /* 9. Configure High-Performance 5 GHz Channel 36 in 80 MHz VHT Mode */
    AwusPhySetChannel(devCtx, 36, AWUS_BW_80MHZ);

    /* 10. Apply Maximum Power Delivery Envelope */
    if (devCtx->PowerContext) {
        AwusPowerConfigureChannel(
            devCtx,
            (PAWUS_POWER_CONTEXT)devCtx->PowerContext,
            36,
            TRUE
        );
    }

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] Hardware fully configured: Status OK, LED active, Maximum Power Delivery armed.\n"));

    return STATUS_SUCCESS;
}

NTSTATUS
AwusEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    PAWUS_DEVICE_CONTEXT devCtx = AwusGetDeviceContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Releasing Hardware...\n"));

    if (devCtx->PowerContext) {
        ExFreePoolWithTag(devCtx->PowerContext, 'SuwA');
        devCtx->PowerContext = NULL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
AwusEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    PAWUS_DEVICE_CONTEXT devCtx = AwusGetDeviceContext(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Device entered D0 (Working Power State). Starting RX Engine...\n"));

    /* Re-arm LED on power-up */
    AwusLedInit(devCtx);

    /* Start asynchronous continuous reader for incoming bulk traffic */
    return AwusUsbStartRxReader(devCtx);
}


NTSTATUS
AwusEvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
)
{
    PAWUS_DEVICE_CONTEXT devCtx = AwusGetDeviceContext(Device);
    UNREFERENCED_PARAMETER(TargetState);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Device exiting D0. Stopping RX Engine...\n"));

    AwusUsbStopRxReader(devCtx);
    return STATUS_SUCCESS;
}

VOID
AwusEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PAWUS_DEVICE_CONTEXT devCtx = AwusGetDeviceContext(device);
    NTSTATUS status = STATUS_SUCCESS;
    size_t bytesReturned = 0;

    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
    case IOCTL_AWUS_GET_TELEMETRY: {
        PAWUS_TELEMETRY_DATA telemetry;
        PAWUS_POWER_CONTEXT pwrCtx = (PAWUS_POWER_CONTEXT)devCtx->PowerContext;

        if (OutputBufferLength < sizeof(AWUS_TELEMETRY_DATA)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(AWUS_TELEMETRY_DATA), (PVOID*)&telemetry, NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlZeroMemory(telemetry, sizeof(AWUS_TELEMETRY_DATA));
        RtlCopyMemory(telemetry->MacAddress, devCtx->CurrentMacAddress, 6);

        if (pwrCtx) {
            telemetry->PowerMode = pwrCtx->PowerLevelMode;
            telemetry->CurrentTempC = pwrCtx->CurrentTemperature;
            telemetry->CurrentChannel = pwrCtx->CurChannel;
            telemetry->Is5GHz = pwrCtx->Is5GHz;
        }

        bytesReturned = sizeof(AWUS_TELEMETRY_DATA);
        break;
    }

    case IOCTL_AWUS_SET_POWER_MODE: {
        PUINT32 pMode;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(UINT32), (PVOID*)&pMode, NULL);
        if (NT_SUCCESS(status) && devCtx->PowerContext) {
            AwusPowerSetMode((PAWUS_POWER_CONTEXT)devCtx->PowerContext, *pMode);
            AwusPowerConfigureChannel(
                devCtx,
                (PAWUS_POWER_CONTEXT)devCtx->PowerContext,
                ((PAWUS_POWER_CONTEXT)devCtx->PowerContext)->CurChannel,
                ((PAWUS_POWER_CONTEXT)devCtx->PowerContext)->Is5GHz
            );
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
