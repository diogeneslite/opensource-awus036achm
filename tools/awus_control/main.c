/*
 * ALFA AWUS036ACHM Performance & Power Delivery Control Utility
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winioctl.h>

#define AWUS_USER_DEVICE_PATH   L"\\\\.\\Awus036achm"

#define IOCTL_AWUS_GET_TELEMETRY    CTL_CODE(FILE_DEVICE_NETWORK, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AWUS_SET_POWER_MODE   CTL_CODE(FILE_DEVICE_NETWORK, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AWUS_SET_CHANNEL      CTL_CODE(FILE_DEVICE_NETWORK, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _AWUS_TELEMETRY_DATA {
    UCHAR   MacAddress[6];
    UINT32  PowerMode;
    INT32   CurrentTempC;
    UINT8   CurrentChannel;
    BOOLEAN Is5GHz;
    UINT64  TxPackets;
    UINT64  RxPackets;
    UINT32  LastRssi;
    UINT32  LastSnr;
} AWUS_TELEMETRY_DATA, *PAWUS_TELEMETRY_DATA;

static const char* GetPowerModeName(UINT32 mode) {
    switch (mode) {
    case 0: return "Standard Regulatory (Default)";
    case 1: return "High Performance (+3 dBm)";
    case 2: return "MAXIMUM HARDWARE BOOST (+5 dBm, Full FEM Drive)";
    default: return "Unknown";
    }
}

void PrintUsage(const char* exeName) {
    printf("=================================================================\n");
    printf("  ALFA AWUS036ACHM Windows 11 Power & Performance Control v1.0   \n");
    printf("=================================================================\n\n");
    printf("Usage:\n");
    printf("  %s --status                 Display adapter telemetry & power status\n", exeName);
    printf("  %s --power [0|1|2]          Set RF power level:\n", exeName);
    printf("                                0 = Standard Regulatory\n");
    printf("                                1 = High Performance (+3 dBm)\n");
    printf("                                2 = Maximum Hardware Boost (+5 dBm)\n");
    printf("  %s --help                   Display this help screen\n\n", exeName);
}

int main(int argc, char* argv[]) {
    HANDLE hDevice;
    DWORD bytesReturned;
    BOOL success;

    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        PrintUsage(argv[0]);
        return 0;
    }

    hDevice = CreateFileW(
        AWUS_USER_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("[-] Failed opening ALFA AWUS036ACHM device (Error: %lu)\n", err);
        if (err == ERROR_FILE_NOT_FOUND) {
            printf("[*] Device driver is not currently running or adapter is unplugged.\n");
        } else if (err == ERROR_ACCESS_DENIED) {
            printf("[*] Please run this utility with Administrator privileges.\n");
        }
        return 1;
    }

    if (strcmp(argv[1], "--status") == 0) {
        AWUS_TELEMETRY_DATA telem;
        ZeroMemory(&telem, sizeof(telem));

        success = DeviceIoControl(
            hDevice,
            IOCTL_AWUS_GET_TELEMETRY,
            NULL, 0,
            &telem, sizeof(telem),
            &bytesReturned,
            NULL
        );

        if (success) {
            printf("=================================================================\n");
            printf("  ALFA AWUS036ACHM Live Hardware Telemetry                       \n");
            printf("=================================================================\n");
            printf("  Hardware MAC Address : %02X:%02X:%02X:%02X:%02X:%02X\n",
                telem.MacAddress[0], telem.MacAddress[1], telem.MacAddress[2],
                telem.MacAddress[3], telem.MacAddress[4], telem.MacAddress[5]);
            printf("  RF Power Mode        : %s\n", GetPowerModeName(telem.PowerMode));
            printf("  Current RF Channel   : %u (%s Band)\n", telem.CurrentChannel, telem.Is5GHz ? "5 GHz" : "2.4 GHz");
            printf("  On-Die Temperature   : %d C\n", telem.CurrentTempC);
            printf("  Datapath Statistics  : %llu TX Packets | %llu RX Packets\n", (unsigned long long)telem.TxPackets, (unsigned long long)telem.RxPackets);
            printf("  Signal Quality       : RSSI: %u | SNR: %u dB\n", telem.LastRssi, telem.LastSnr);
            printf("=================================================================\n");
        } else {
            printf("[-] DeviceIoControl failed: %lu\n", GetLastError());
        }
    } else if (strcmp(argv[1], "--power") == 0) {
        if (argc < 3) {
            printf("[-] Missing power mode argument (0, 1, or 2)\n");
            CloseHandle(hDevice);
            return 1;
        }

        UINT32 mode = (UINT32)atoi(argv[2]);
        if (mode > 2) {
            printf("[-] Invalid power mode %u. Valid options: 0, 1, 2\n", mode);
            CloseHandle(hDevice);
            return 1;
        }

        success = DeviceIoControl(
            hDevice,
            IOCTL_AWUS_SET_POWER_MODE,
            &mode, sizeof(mode),
            NULL, 0,
            &bytesReturned,
            NULL
        );

        if (success) {
            printf("[+] Power delivery mode updated successfully to: %s\n", GetPowerModeName(mode));
        } else {
            printf("[-] Failed updating power mode (Error: %lu)\n", GetLastError());
        }
    } else {
        printf("[-] Unrecognized command: %s\n", argv[1]);
        PrintUsage(argv[0]);
    }

    CloseHandle(hDevice);
    return 0;
}
