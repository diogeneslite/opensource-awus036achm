/*
 * ALFA AWUS036ACHM NDIS 6.x OID Query/Set Definitions
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#pragma once

#include "NdisMiniport.h"

/* Supported OIDs table */
extern const NDIS_OID g_AwusSupportedOids[];
extern const ULONG g_AwusSupportedOidsCount;

/* Handlers */
NDIS_STATUS
AwusOidQueryInformation(
    _In_ PAWUS_NDIS_ADAPTER Adapter,
    _Inout_ PNDIS_OID_REQUEST OidRequest
);

NDIS_STATUS
AwusOidSetInformation(
    _In_ PAWUS_NDIS_ADAPTER Adapter,
    _Inout_ PNDIS_OID_REQUEST OidRequest
);

NDIS_STATUS
AwusOidMethod(
    _In_ PAWUS_NDIS_ADAPTER Adapter,
    _Inout_ PNDIS_OID_REQUEST OidRequest
);

VOID
AwusIndicateScanConfirm(
    _In_ PAWUS_NDIS_ADAPTER Adapter,
    _In_ NDIS_STATUS Status
);
