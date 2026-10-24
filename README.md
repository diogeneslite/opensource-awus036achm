# OpenSource-AWUS036ACHM: Modern Windows 11 Driver

I have zero coding knowledge, but since last driver for this card on the windows platform was released around a decade ago, I vibe coded this open source driver for the card to make our lives easier.

I included the option to raise the chipset's power to the maximum, so that we can enjoy an extra 5db boost for free. there's also a new option to force the maximum throughput.

You can switch between, and toggle on or off these feature using Device Manager.

That was a lot of typing lol now I leave you with the boring AI project description:









High-performance, open-source Windows 11 driver for the **ALFA Network AWUS036ACHM** USB Wi-Fi adapter (MediaTek MT7610U chipset), engineered for **peak throughput (802.11ac VHT80 433.3 Mbps)** and **maximum Front-End Module (FEM) power delivery**.

---

## 1. Hardware Reverse Engineering & Findings

From deep static and cryptographic analysis of the vendor installer (`Setup.exe`) and comparison with the open-source Linux `mt76x0` subsystem, the following hardware details were uncovered:

- **Target Chipset:** MediaTek MT7610U (Single-stream 1x1:1 802.11a/b/g/n/ac)
- **USB IDs:** `USB\VID_0E8D&PID_7610` (Alfa standard) & `USB\VID_148F&PID_7610` (Ralink legacy)
- **Embedded MCU Firmware:** `FW_7610.bin` (80,524 bytes):
  - `ILM`: 69,016 bytes (Instruction Local Memory)
  - `DLM`: 11,476 bytes (Data Local Memory)
  - `IVB`: 64 bytes (Interrupt Vector Block)
  - Build Version: `201312200938____`
  - Integrated directly as C bytecode into [`driver/Firmware.h`](driver/Firmware.h) for instant boot.
- **External FEM / Power Delivery:**
  - The AWUS036ACHM incorporates dedicated external Skyworks / Richwave Power Amplifiers (PA) and Low Noise Amplifiers (LNA).
  - Configured via hardware pinmux, Automatic Level Control (`MT_TX_ALC_CFG_0`, `MT_TX_ALC_CFG_1`), and rate power mapping (`MT_TX_PWR_CFG_0` through `MT_TX_PWR_CFG_9`).

---

## 2. Driver Architecture (Modern Windows 11 Standards)

The driver is developed using Microsoft's modern **NetAdapterCx (NetworkAdapter WDF Class Extension)** and **KMDF (Kernel-Mode Driver Framework)** architecture, replacing legacy NDIS miniport models:

```
opensource-awus036achm/
├── driver/
│   ├── Driver.h / Driver.c            # WDF DriverEntry, PnP/Power lifecycle & IOCTL dispatcher
│   ├── Registers.h                    # Complete MT7610U hardware registers & TXWI/RXWI headers
│   ├── Firmware.h                     # Embedded MT7610U MCU firmware bytecode (FW_7610.bin)
│   ├── Mcu.h / Mcu.c                  # MCU firmware upload, IVB vector & command execution
│   ├── UsbEngine.h / UsbEngine.c      # Asynchronous USB control & multi-queue bulk datapath
│   ├── Phy.h / Phy.c                  # Dual-band synthesizer, 20/40/80 MHz bandwidth switching & AGC
│   ├── PowerDelivery.h / PowerDelivery.c # Maximum Power Delivery, external PA biasing & ALC
│   ├── Eeprom.h / Eeprom.c            # eFuse reader, MAC retrieval & factory power calibration
│   ├── TxRxQueue.h / TxRxQueue.c      # NetRing queues, A-MPDU aggregation & RXWI parsing
│   ├── awus036achm.inf                # Universal Driver Package for Windows 11 (64-bit)
│   └── awus036achm.vcxproj            # MSBuild kernel driver project
├── tools/
│   └── awus_control/
│       ├── main.c                     # User-mode telemetry & real-time RF power adjustment utility
│       ├── awus_control.exe           # Compiled 64-bit control tool
│       └── awus_control.vcxproj
├── build.bat                          # Automated build script using Visual Studio 2022
└── opensource-awus036achm.sln         # Complete Visual Studio Solution
```

---

## 3. Peak Performance & Maximum Power Features

1. **Maximum Hardware Power Delivery:**
   - **Boost Mode (+5 dBm Unlocked):** Unlocks full linear output of the external Front-End Module.
   - **ALC (Automatic Level Control):** Continuously calibrates PA biasing to prevent EVM compression.
   - **Dynamic Thermal Tracking:** Adjusts power dynamically based on on-die thermal sensors to prevent thermal runaway.
2. **Peak 802.11ac Throughput:**
   - **80 MHz VHT80 Bandwidth:** Supports 433.3 Mbps PHY link rate on 5 GHz UNII-1/2/3 bands.
   - **A-MPDU Hardware Aggregation:** Aggregates up to 64 KB subframes per burst.
   - **Reverse Direction Grant (RDG) & TX Burst:** Maximizes channel utilization during transmission opportunities.
   - **Zero-Latency USB:** Disables selective suspend sleep states for real-time packet delivery.

---

## 4. User-Mode Telemetry & Control Tool (`awus_control.exe`)

The included command-line tool provides real-time hardware telemetry and live RF power tuning:

```cmd
# Query live hardware stats, power mode, and on-die temperature:
tools\awus_control\awus_control.exe --status

# Switch to Maximum Hardware Boost (+5 dBm):
tools\awus_control\awus_control.exe --power 2
```

---

## 5. Building & Installation

### Building from Source

To compile the driver package and telemetry utility using Visual Studio 2022 & WDK:
```cmd
build.bat
```

---

### Installation

#### Option A: Automated Setup via PowerShell (Recommended)

Run PowerShell as **Administrator** in the repository folder and execute:

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force
.\install_opensource_driver.ps1
```

> **Tip:** You can also simply right-click **`install_opensource_driver.bat`** and choose **Run as administrator**.

**What this automated script does:**
1. **Certificate Import:** Automatically adds `driver\awus036achm.cer` to the Windows `Root` and `TrustedPublisher` certificate stores so Windows trusts the package.
2. **PnP Driver Staging:** Stages and installs `driver\awus036achm.inf` via `pnputil`.
3. **PnP Device Binding:** Cycles the device node (`USB\VID_0E8D&PID_7610`) to immediately bind to the new driver.
4. **Service Refresh:** Restarts Windows WLAN AutoConfig (`wlansvc`) to bring the wireless interface online immediately.

#### Option B: Manual Installation via Device Manager

1. Open **Device Manager** (`devmgmt.msc`).
2. Locate the **ALFA AWUS036ACHM** (`USB\VID_0E8D&PID_7610`) under **Network adapters** or **Other devices**.
3. Right-click and choose **Update Driver** -> **Browse my computer for drivers**.
4. Point to the [`driver/`](driver/) folder and click **Next**.

