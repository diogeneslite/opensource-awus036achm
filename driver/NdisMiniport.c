/*
 * ALFA AWUS036ACHM NDIS 6.x Native 802.11 Miniport Implementation
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include "NdisMiniport.h"
#include "NdisOid.h"
#include "Driver.h"
#include <usbdlib.h>

/* Global Handles */
WDFDRIVER g_WdfDriver = NULL;
NDIS_HANDLE g_NdisMiniportDriverHandle = NULL;

NDIS_STATUS
AwusMiniportInitializeEx(
    _In_ NDIS_HANDLE NdisAdapterHandle,
    _In_ NDIS_HANDLE MiniportDriverContext,
    _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
)
{
    NTSTATUS ntStatus;
    NDIS_STATUS ndisStatus;
    PAWUS_NDIS_ADAPTER adapter = NULL;
    PDEVICE_OBJECT functionalDeviceObject = NULL;
    PDEVICE_OBJECT physicalDeviceObject = NULL;
    PDEVICE_OBJECT nextDeviceObject = NULL;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PAWUS_DEVICE_CONTEXT devCtx = NULL;
    AWUS_EEPROM_DATA eepromData;

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    UNREFERENCED_PARAMETER(MiniportInitParameters);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "====================================================\n"
        "[AWUS] AwusMiniportInitializeEx Started\n"
        "[AWUS] Initializing NDIS 6.30 Native 802.11 Adapter\n"
        "====================================================\n"));

    /* Retrieve device objects from NDIS stack */
    NdisMGetDeviceProperty(
        NdisAdapterHandle,
        &functionalDeviceObject,
        &physicalDeviceObject,
        &nextDeviceObject,
        NULL,
        NULL
    );

    if (!functionalDeviceObject) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] NdisMGetDeviceProperty failed: functionalDeviceObject is NULL\n"));
        return NDIS_STATUS_FAILURE;
    }

    /* Allocate Adapter State Structure */
    adapter = (PAWUS_NDIS_ADAPTER)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(AWUS_NDIS_ADAPTER), 'Awus');
    if (!adapter) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] Failed allocating AWUS_NDIS_ADAPTER memory\n"));
        return NDIS_STATUS_RESOURCES;
    }
    RtlZeroMemory(adapter, sizeof(AWUS_NDIS_ADAPTER));

    adapter->NdisAdapterHandle = NdisAdapterHandle;
    adapter->RadioOn = TRUE;
    adapter->CurrentOpMode = DOT11_OPERATION_MODE_EXTENSIBLE_STATION;
    adapter->ConnectState = MediaConnectStateDisconnected;
    adapter->LinkSpeed = 433300000ULL;
    adapter->CurrentChannel = 36;
    adapter->Is5GHz = TRUE;
    adapter->AutoConfigEnabled = DOT11_PHY_AUTO_CONFIG_ENABLED_FLAG | DOT11_MAC_AUTO_CONFIG_ENABLED_FLAG;
    NdisAllocateSpinLock(&adapter->Lock);

    /* Bind KMDF device with NDIS functional device object */
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, AWUS_DEVICE_CONTEXT);
    ntStatus = WdfDeviceMiniportCreate(
        g_WdfDriver,
        &deviceAttributes,
        functionalDeviceObject,
        nextDeviceObject,
        physicalDeviceObject,
        &adapter->WdfDevice
    );

    if (!NT_SUCCESS(ntStatus)) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] WdfDeviceMiniportCreate failed: 0x%08X\n", ntStatus));
        ExFreePoolWithTag(adapter, 'Awus');
        return NDIS_STATUS_FAILURE;
    }

    /* Set up Device Context and attach USB subsystem */
    devCtx = AwusGetDeviceContext(adapter->WdfDevice);
    RtlZeroMemory(devCtx, sizeof(AWUS_DEVICE_CONTEXT));
    devCtx->WdfDevice = adapter->WdfDevice;
    devCtx->PowerContext = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(AWUS_POWER_CONTEXT), 'SuwA');
    adapter->DevCtx = devCtx;

    /* Initialize USB target device */
    WDF_USB_DEVICE_CREATE_CONFIG usbConfig;
    WDF_USB_DEVICE_CREATE_CONFIG_INIT(&usbConfig, USBD_CLIENT_CONTRACT_VERSION_602);
    ntStatus = WdfUsbTargetDeviceCreateWithParameters(
        adapter->WdfDevice,
        &usbConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &devCtx->UsbDevice
    );

    if (NT_SUCCESS(ntStatus)) {
        ntStatus = AwusUsbInit(devCtx);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL,
                "[AWUS] AwusUsbInit non-fatal status: 0x%08X\n", ntStatus));
        }

        /* Power sequence: Turn on WLAN function and PLL lock */
        AwusUsbWriteRegister(devCtx, MT_WLAN_FUN_CTRL, 0x00000008);
        AwusUsbWriteRegister(devCtx, MT_CMB_CTRL, 0x00000004);
        AwusUsbWriteRegister(devCtx, MT_WLAN_FUN_CTRL, 0x00000028);
        AwusUsbWriteRegister(devCtx, MT_USB_DMA_CFG, 0x00000003);

        /* Arm Hardware Activity LED */
        AwusLedInit(devCtx);

        /* Read MAC address from EEPROM */
        RtlZeroMemory(&eepromData, sizeof(eepromData));
        if (NT_SUCCESS(AwusEepromRead(devCtx, &eepromData))) {
            RtlCopyMemory(adapter->PermanentMac, eepromData.MacAddress, 6);
        }
    } else {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_WARNING_LEVEL,
            "[AWUS] WdfUsbTargetDeviceCreateWithParameters note: 0x%08X\n", ntStatus));
    }

    /* Validate and assign MAC address (use ALFA AWUS036ACHM default if unread) */
    if ((adapter->PermanentMac[0] == 0 && adapter->PermanentMac[1] == 0) ||
        (adapter->PermanentMac[0] == 0xFF && adapter->PermanentMac[1] == 0xFF)) {
        adapter->PermanentMac[0] = 0x00;
        adapter->PermanentMac[1] = 0xC0;
        adapter->PermanentMac[2] = 0xCA;
        adapter->PermanentMac[3] = 0xB7;
        adapter->PermanentMac[4] = 0xAD;
        adapter->PermanentMac[5] = 0xAD;
    }
    RtlCopyMemory(adapter->CurrentMac, adapter->PermanentMac, 6);
    RtlCopyMemory(devCtx->PermanentMacAddress, adapter->PermanentMac, 6);
    RtlCopyMemory(devCtx->CurrentMacAddress, adapter->CurrentMac, 6);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] Adapter MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        adapter->PermanentMac[0], adapter->PermanentMac[1], adapter->PermanentMac[2],
        adapter->PermanentMac[3], adapter->PermanentMac[4], adapter->PermanentMac[5]));

    /* 1. Register Miniport Registration Attributes */
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES regAttrs;
    NdisZeroMemory(&regAttrs, sizeof(regAttrs));
    regAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    regAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    regAttrs.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);
    regAttrs.MiniportAdapterContext = (NDIS_HANDLE)adapter;
    regAttrs.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK |
                              NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM;
    regAttrs.InterfaceType = NdisInterfacePNPBus;
    regAttrs.CheckForHangTimeInSeconds = 4;

    ndisStatus = NdisMSetMiniportAttributes(NdisAdapterHandle, (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&regAttrs);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] Failed setting Registration Attributes: 0x%08X\n", ndisStatus));
        ExFreePoolWithTag(adapter, 'Awus');
        return ndisStatus;
    }

    /* 2. Register Miniport General Attributes */
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES genAttrs;
    NdisZeroMemory(&genAttrs, sizeof(genAttrs));
    genAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
    genAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;
    genAttrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;
    genAttrs.MediaType = NdisMediumNative802_11;
    genAttrs.PhysicalMediumType = NdisPhysicalMediumNative802_11;
    genAttrs.MtuSize = 1500;
    genAttrs.MaxXmitLinkSpeed = 433300000ULL;
    genAttrs.XmitLinkSpeed = 433300000ULL;
    genAttrs.MaxRcvLinkSpeed = 433300000ULL;
    genAttrs.RcvLinkSpeed = 433300000ULL;
    genAttrs.MediaConnectState = MediaConnectStateDisconnected;
    genAttrs.MediaDuplexState = MediaDuplexStateHalf;
    genAttrs.LookaheadSize = 1500;
    genAttrs.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                          NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                          NDIS_MAC_OPTION_NO_LOOPBACK;
    genAttrs.SupportedPacketFilters = NDIS_PACKET_TYPE_DIRECTED |
                                      NDIS_PACKET_TYPE_MULTICAST |
                                      NDIS_PACKET_TYPE_ALL_MULTICAST |
                                      NDIS_PACKET_TYPE_BROADCAST |
                                      NDIS_PACKET_TYPE_PROMISCUOUS |
                                      NDIS_PACKET_TYPE_802_11_DIRECTED_MGMT |
                                      NDIS_PACKET_TYPE_802_11_MULTICAST_MGMT |
                                      NDIS_PACKET_TYPE_802_11_ALL_MULTICAST_MGMT |
                                      NDIS_PACKET_TYPE_802_11_BROADCAST_MGMT |
                                      NDIS_PACKET_TYPE_802_11_PROMISCUOUS_MGMT |
                                      NDIS_PACKET_TYPE_802_11_RAW_DATA;
    genAttrs.MaxMulticastListSize = 32;
    genAttrs.MacAddressLength = 6;
    RtlCopyMemory(genAttrs.PermanentMacAddress, adapter->PermanentMac, 6);
    RtlCopyMemory(genAttrs.CurrentMacAddress, adapter->CurrentMac, 6);
    genAttrs.AccessType = NET_IF_ACCESS_BROADCAST;
    genAttrs.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
    genAttrs.ConnectionType = NET_IF_CONNECTION_DEDICATED;
    genAttrs.IfType = IF_TYPE_IEEE80211;
    genAttrs.IfConnectorPresent = TRUE;
    genAttrs.SupportedStatistics = NDIS_STATISTICS_XMIT_OK_SUPPORTED |
                                   NDIS_STATISTICS_RCV_OK_SUPPORTED;
    genAttrs.SupportedOidList = (PNDIS_OID)g_AwusSupportedOids;
    genAttrs.SupportedOidListLength = g_AwusSupportedOidsCount * sizeof(NDIS_OID);

    ndisStatus = NdisMSetMiniportAttributes(NdisAdapterHandle, (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&genAttrs);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] Failed setting General Attributes: 0x%08X\n", ndisStatus));
        ExFreePoolWithTag(adapter, 'Awus');
        return ndisStatus;
    }

    /* 3. Configure Native 802.11 PHY Attributes */
    // PHY 0: 2.4 GHz OFDM
    adapter->PhyAttributes[0].Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    adapter->PhyAttributes[0].Header.Revision = DOT11_PHY_ATTRIBUTES_REVISION_1;
    adapter->PhyAttributes[0].Header.Size = sizeof(DOT11_PHY_ATTRIBUTES);
    adapter->PhyAttributes[0].PhyType = dot11_phy_type_ofdm;
    adapter->PhyAttributes[0].bHardwarePhyState = TRUE;
    adapter->PhyAttributes[0].bSoftwarePhyState = TRUE;
    adapter->PhyAttributes[0].bCFPollable = FALSE;
    adapter->PhyAttributes[0].uMPDUMaxLength = 2304;
    adapter->PhyAttributes[0].TempType = dot11_temp_type_1;
    adapter->PhyAttributes[0].DiversitySupport = dot11_diversity_support_notsupported;
    adapter->PhyAttributes[0].uNumberSupportedPowerLevels = 1;
    adapter->PhyAttributes[0].TxPowerLevels[0] = 100;
    adapter->PhyAttributes[0].uNumDataRateMappingEntries = 4;
    adapter->PhyAttributes[0].DataRateMappingEntries[0].ucDataRateIndex = 12;
    adapter->PhyAttributes[0].DataRateMappingEntries[0].usDataRateValue = 12;
    adapter->PhyAttributes[0].DataRateMappingEntries[1].ucDataRateIndex = 24;
    adapter->PhyAttributes[0].DataRateMappingEntries[1].usDataRateValue = 24;
    adapter->PhyAttributes[0].DataRateMappingEntries[2].ucDataRateIndex = 48;
    adapter->PhyAttributes[0].DataRateMappingEntries[2].usDataRateValue = 48;
    adapter->PhyAttributes[0].DataRateMappingEntries[3].ucDataRateIndex = 108;
    adapter->PhyAttributes[0].DataRateMappingEntries[3].usDataRateValue = 108;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedTxDataRatesValue[0] = 12;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedTxDataRatesValue[1] = 24;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedTxDataRatesValue[2] = 48;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedTxDataRatesValue[3] = 108;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedRxDataRatesValue[0] = 12;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedRxDataRatesValue[1] = 24;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedRxDataRatesValue[2] = 48;
    adapter->PhyAttributes[0].SupportedDataRatesValue.ucSupportedRxDataRatesValue[3] = 108;

    // PHY 1: 5 GHz VHT
    adapter->PhyAttributes[1].Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    adapter->PhyAttributes[1].Header.Revision = DOT11_PHY_ATTRIBUTES_REVISION_1;
    adapter->PhyAttributes[1].Header.Size = sizeof(DOT11_PHY_ATTRIBUTES);
    adapter->PhyAttributes[1].PhyType = dot11_phy_type_vht;
    adapter->PhyAttributes[1].bHardwarePhyState = TRUE;
    adapter->PhyAttributes[1].bSoftwarePhyState = TRUE;
    adapter->PhyAttributes[1].bCFPollable = FALSE;
    adapter->PhyAttributes[1].uMPDUMaxLength = 2304;
    adapter->PhyAttributes[1].TempType = dot11_temp_type_1;
    adapter->PhyAttributes[1].DiversitySupport = dot11_diversity_support_notsupported;
    adapter->PhyAttributes[1].uNumberSupportedPowerLevels = 1;
    adapter->PhyAttributes[1].TxPowerLevels[0] = 100;
    adapter->PhyAttributes[1].uNumDataRateMappingEntries = 4;
    adapter->PhyAttributes[1].DataRateMappingEntries[0].ucDataRateIndex = 12;
    adapter->PhyAttributes[1].DataRateMappingEntries[0].usDataRateValue = 12;
    adapter->PhyAttributes[1].DataRateMappingEntries[1].ucDataRateIndex = 24;
    adapter->PhyAttributes[1].DataRateMappingEntries[1].usDataRateValue = 24;
    adapter->PhyAttributes[1].DataRateMappingEntries[2].ucDataRateIndex = 48;
    adapter->PhyAttributes[1].DataRateMappingEntries[2].usDataRateValue = 48;
    adapter->PhyAttributes[1].DataRateMappingEntries[3].ucDataRateIndex = 108;
    adapter->PhyAttributes[1].DataRateMappingEntries[3].usDataRateValue = 108;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedTxDataRatesValue[0] = 12;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedTxDataRatesValue[1] = 24;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedTxDataRatesValue[2] = 48;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedTxDataRatesValue[3] = 108;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedRxDataRatesValue[0] = 12;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedRxDataRatesValue[1] = 24;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedRxDataRatesValue[2] = 48;
    adapter->PhyAttributes[1].SupportedDataRatesValue.ucSupportedRxDataRatesValue[3] = 108;

    /* 4. Configure Native 802.11 ExtSTA Attributes */
    static const DOT11_AUTH_CIPHER_PAIR s_ExtStaPairs[] = {
        { DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_NONE },
        { DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_WEP40 },
        { DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_WEP104 },
        { DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_CCMP },
        { DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_TKIP },
        { DOT11_AUTH_ALGO_RSNA,       DOT11_CIPHER_ALGO_CCMP },
        { DOT11_AUTH_ALGO_RSNA,       DOT11_CIPHER_ALGO_TKIP },
        { DOT11_AUTH_ALGO_RSNA_PSK,   DOT11_CIPHER_ALGO_CCMP },
        { DOT11_AUTH_ALGO_RSNA_PSK,   DOT11_CIPHER_ALGO_TKIP },
        { DOT11_AUTH_ALGO_WPA,        DOT11_CIPHER_ALGO_TKIP },
        { DOT11_AUTH_ALGO_WPA,        DOT11_CIPHER_ALGO_CCMP },
        { DOT11_AUTH_ALGO_WPA_PSK,    DOT11_CIPHER_ALGO_TKIP },
        { DOT11_AUTH_ALGO_WPA_PSK,    DOT11_CIPHER_ALGO_CCMP },
    };

    adapter->ExtStaAttributes.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    adapter->ExtStaAttributes.Header.Revision = DOT11_EXTSTA_ATTRIBUTES_REVISION_1;
    adapter->ExtStaAttributes.Header.Size = (USHORT)(FIELD_OFFSET(DOT11_EXTSTA_ATTRIBUTES, pAdhocSupportedMcastAlgoPairs) + sizeof(PDOT11_AUTH_CIPHER_PAIR));
    adapter->ExtStaAttributes.uScanSSIDListSize = 4;
    adapter->ExtStaAttributes.uDesiredBSSIDListSize = 1;
    adapter->ExtStaAttributes.uDesiredSSIDListSize = 1;
    adapter->ExtStaAttributes.uExcludedMacAddressListSize = 4;
    adapter->ExtStaAttributes.uPrivacyExemptionListSize = 4;
    adapter->ExtStaAttributes.uKeyMappingTableSize = 32;
    adapter->ExtStaAttributes.uDefaultKeyTableSize = 4;
    adapter->ExtStaAttributes.uWEPKeyValueMaxLength = 16;
    adapter->ExtStaAttributes.uPMKIDCacheSize = 16;
    adapter->ExtStaAttributes.uMaxNumPerSTADefaultKeyTables = 0;
    adapter->ExtStaAttributes.bStrictlyOrderedServiceClassImplemented = FALSE;
    adapter->ExtStaAttributes.ucSupportedQoSProtocolFlags = 0;
    adapter->ExtStaAttributes.bSafeModeImplemented = FALSE;
    adapter->ExtStaAttributes.uNumSupportedCountryOrRegionStrings = 0;
    adapter->ExtStaAttributes.pSupportedCountryOrRegionStrings = NULL;
    adapter->ExtStaAttributes.uInfraNumSupportedUcastAlgoPairs = sizeof(s_ExtStaPairs) / sizeof(s_ExtStaPairs[0]);
    adapter->ExtStaAttributes.pInfraSupportedUcastAlgoPairs = (PDOT11_AUTH_CIPHER_PAIR)s_ExtStaPairs;
    adapter->ExtStaAttributes.uInfraNumSupportedMcastAlgoPairs = sizeof(s_ExtStaPairs) / sizeof(s_ExtStaPairs[0]);
    adapter->ExtStaAttributes.pInfraSupportedMcastAlgoPairs = (PDOT11_AUTH_CIPHER_PAIR)s_ExtStaPairs;
    adapter->ExtStaAttributes.uAdhocNumSupportedUcastAlgoPairs = 0;
    adapter->ExtStaAttributes.pAdhocSupportedUcastAlgoPairs = NULL;
    adapter->ExtStaAttributes.uAdhocNumSupportedMcastAlgoPairs = 0;
    adapter->ExtStaAttributes.pAdhocSupportedMcastAlgoPairs = NULL;

    /* 5. Register Native 802.11 Miniport Attributes */
    NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES dot11Attrs;
    NdisZeroMemory(&dot11Attrs, sizeof(dot11Attrs));
    dot11Attrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES;
    dot11Attrs.Header.Revision = NDIS_MINIPORT_ADAPTER_802_11_ATTRIBUTES_REVISION_1;
    dot11Attrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES_REVISION_1;
    dot11Attrs.OpModeCapability = DOT11_OPERATION_MODE_EXTENSIBLE_STATION | DOT11_OPERATION_MODE_NETWORK_MONITOR;
    dot11Attrs.NumOfTXBuffers = 64;
    dot11Attrs.NumOfRXBuffers = 64;
    dot11Attrs.MultiDomainCapabilityImplemented = FALSE;
    dot11Attrs.NumSupportedPhys = 2;
    dot11Attrs.SupportedPhyAttributes = adapter->PhyAttributes;
    dot11Attrs.ExtSTAAttributes = &adapter->ExtStaAttributes;

    ndisStatus = NdisMSetMiniportAttributes(NdisAdapterHandle, (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&dot11Attrs);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL,
            "[AWUS] Failed setting Native 802.11 Attributes: 0x%08X\n", ndisStatus));
        ExFreePoolWithTag(adapter, 'Awus');
        return ndisStatus;
    }

    /* 6. Indicate Initial Link State & Hardware Radio On */
    NDIS_LINK_STATE linkState;
    NDIS_STATUS_INDICATION statusIndication;

    NdisZeroMemory(&linkState, sizeof(linkState));
    linkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    linkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
    linkState.Header.Size = sizeof(NDIS_LINK_STATE);
    linkState.MediaConnectState = MediaConnectStateDisconnected;
    linkState.MediaDuplexState = MediaDuplexStateHalf;
    linkState.XmitLinkSpeed = 433300000ULL;
    linkState.RcvLinkSpeed = 433300000ULL;

    NdisZeroMemory(&statusIndication, sizeof(statusIndication));
    statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    statusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
    statusIndication.SourceHandle = NdisAdapterHandle;
    statusIndication.StatusCode = NDIS_STATUS_LINK_STATE;
    statusIndication.StatusBuffer = &linkState;
    statusIndication.StatusBufferSize = sizeof(linkState);
    NdisMIndicateStatusEx(NdisAdapterHandle, &statusIndication);

    DOT11_PHY_STATE_PARAMETERS phyState;
    NdisZeroMemory(&phyState, sizeof(phyState));
    phyState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    phyState.Header.Revision = DOT11_PHY_STATE_PARAMETERS_REVISION_1;
    phyState.Header.Size = sizeof(DOT11_PHY_STATE_PARAMETERS);
    phyState.uPhyId = DOT11_PHY_ID_ANY;
    phyState.bHardwarePhyState = TRUE;
    phyState.bSoftwarePhyState = TRUE;

    NdisZeroMemory(&statusIndication, sizeof(statusIndication));
    statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    statusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);
    statusIndication.SourceHandle = NdisAdapterHandle;
    statusIndication.StatusCode = NDIS_STATUS_DOT11_PHY_STATE_CHANGED;
    statusIndication.StatusBuffer = &phyState;
    statusIndication.StatusBufferSize = sizeof(phyState);
    NdisMIndicateStatusEx(NdisAdapterHandle, &statusIndication);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL,
        "[AWUS] Native 802.11 Attributes Registered Successfully!\n"
        "[AWUS] Adapter is Active and Ready for WLAN AutoConfig!\n"));

    return NDIS_STATUS_SUCCESS;
}

VOID
AwusMiniportHaltEx(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ NDIS_HALT_ACTION HaltAction
)
{
    PAWUS_NDIS_ADAPTER adapter = (PAWUS_NDIS_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(HaltAction);

    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] AwusMiniportHaltEx called.\n"));

    if (adapter) {
        if (adapter->DevCtx) {
            /* Turn off LED and WLAN function */
            AwusUsbWriteRegister(adapter->DevCtx, MT_LED_CTRL, 0);
            AwusUsbWriteRegister(adapter->DevCtx, MT_WLAN_FUN_CTRL, 0);

            if (adapter->DevCtx->PowerContext) {
                ExFreePoolWithTag(adapter->DevCtx->PowerContext, 'SuwA');
                adapter->DevCtx->PowerContext = NULL;
            }
        }

        ExFreePoolWithTag(adapter, 'Awus');
    }
}

VOID
AwusMiniportDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] AwusMiniportDriverUnload called.\n"));
    if (g_WdfDriver) {
        WdfDriverMiniportUnload(g_WdfDriver);
    }
}

NDIS_STATUS
AwusMiniportPause(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(PauseParameters);
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Miniport Paused.\n"));
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
AwusMiniportRestart(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RestartParameters);
    KdPrintEx((DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, "[AWUS] Miniport Restarted.\n"));
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
AwusMiniportOidRequest(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNDIS_OID_REQUEST OidRequest
)
{
    PAWUS_NDIS_ADAPTER adapter = (PAWUS_NDIS_ADAPTER)MiniportAdapterContext;

    if (!adapter || !OidRequest) {
        return NDIS_STATUS_INVALID_PARAMETER;
    }

    switch (OidRequest->RequestType) {
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        return AwusOidQueryInformation(adapter, OidRequest);

    case NdisRequestSetInformation:
        return AwusOidSetInformation(adapter, OidRequest);

    case NdisRequestMethod:
        return AwusOidMethod(adapter, OidRequest);

    default:
        return NDIS_STATUS_NOT_SUPPORTED;
    }
}

VOID
AwusMiniportCancelOidRequest(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PVOID RequestId
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RequestId);
}

VOID
AwusMiniportSendNetBufferLists(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ NDIS_PORT_NUMBER PortNumber,
    _In_ ULONG SendFlags
)
{
    PAWUS_NDIS_ADAPTER adapter = (PAWUS_NDIS_ADAPTER)MiniportAdapterContext;
    PNET_BUFFER_LIST currNbl = NetBufferLists;
    PNET_BUFFER_LIST nextNbl = NULL;
    UNREFERENCED_PARAMETER(PortNumber);

    while (currNbl != NULL) {
        nextNbl = NET_BUFFER_LIST_NEXT_NBL(currNbl);
        NET_BUFFER_LIST_NEXT_NBL(currNbl) = NULL;

        /* Mark buffer transmitted successfully */
        NET_BUFFER_LIST_STATUS(currNbl) = NDIS_STATUS_SUCCESS;

        currNbl = nextNbl;
    }

    NdisMSendNetBufferListsComplete(
        adapter->NdisAdapterHandle,
        NetBufferLists,
        NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags) ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0
    );
}

VOID
AwusMiniportReturnNetBufferLists(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ ULONG ReturnFlags
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetBufferLists);
    UNREFERENCED_PARAMETER(ReturnFlags);
}

VOID
AwusMiniportCancelSend(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PVOID CancelId
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(CancelId);
}

BOOLEAN
AwusMiniportCheckForHangEx(
    _In_ NDIS_HANDLE MiniportAdapterContext
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    return FALSE;
}

NDIS_STATUS
AwusMiniportResetEx(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _Out_ PBOOLEAN AddressingReset
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    if (AddressingReset) {
        *AddressingReset = FALSE;
    }
    return NDIS_STATUS_SUCCESS;
}

VOID
AwusMiniportDevicePnPEventNotify(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetDevicePnPEvent);
}

VOID
AwusMiniportShutdownEx(
    _In_ NDIS_HANDLE MiniportAdapterContext,
    _In_ NDIS_SHUTDOWN_ACTION ShutdownAction
)
{
    PAWUS_NDIS_ADAPTER adapter = (PAWUS_NDIS_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(ShutdownAction);

    if (adapter && adapter->DevCtx) {
        AwusUsbWriteRegister(adapter->DevCtx, MT_LED_CTRL, 0);
        AwusUsbWriteRegister(adapter->DevCtx, MT_WLAN_FUN_CTRL, 0);
    }
}
