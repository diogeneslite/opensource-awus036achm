# 🚀 OpenSource-AWUS036ACHM: Modern Windows 11 Driver

> **"I have zero coding knowledge, but since the last official driver for this card was released almost a decade ago, I vibe-coded this brand-new open-source Windows 11 driver to make all our lives easier!"**

The **ALFA Network AWUS036ACHM** is a legendary long-range USB Wi-Fi card (MediaTek MT7610U chip), but the old manufacturer drivers are stuck in the Windows 7/8 era. 

This project brings the adapter straight into modern **Windows 11** with modern network architecture, unlocked power, and peak Wi-Fi speeds!

---

## ⚡ Superpowers & Features

- 🔋 **Free +5 dBm Power Boost:** We unlocked the card's external amplifiers so your Wi-Fi signal punches through walls and reaches further.
- 🏎️ **Full Throttle Speed:** Unlocks 80 MHz bandwidth (802.11ac VHT80) on 5 GHz for the maximum 433.3 Mbps link rate.
- 🎛️ **Toggle Right from Windows:** No weird config files—you can switch power and speed settings directly inside Windows Device Manager.
- 🌡️ **Smart Heat Guard:** Automatically protects the adapter so it runs cool and stays stable even at max power.

---

## ⚠️ Important Note: Windows Test Mode

> [!WARNING]
> **This community driver is not yet digitally signed by Microsoft.**
> 
> Windows 11 blocks unsigned drivers by default. To use this driver today, your PC must be in **Windows Test Mode**. 
> *(A fully WHQL-signed release is in the works!)*

### How to turn on Test Mode (1 minute):
1. Press the Windows Start button, type `cmd`, right-click **Command Prompt**, and select **Run as administrator**.
2. Paste this command and hit Enter:
   ```cmd
   bcdedit /set testsigning on
   ```
3. Restart your computer. (You'll see a small "Test Mode" text in the bottom right of your screen—this is completely normal).

*(To turn it off later, just run `bcdedit /set testsigning off` and reboot.)*

---

## 📥 How to Install (The Easy Way)

No need to open PowerShell, compile code, or mess with manual commands:

1. **Plug in** your ALFA AWUS036ACHM adapter.
2. **Download or clone** this repository.
3. Locate **`install_opensource_driver.bat`**.
4. **Right-click it** and select **Run as administrator**.

🎉 **That's it!** The script automatically installs the security certificate, registers the driver with Windows, connects your adapter, and turns on Wi-Fi.

---

## 🛠️ For Developers & The Curious

Under the hood, this isn't a hacky wrapper—it's written in clean C using Microsoft's modern **KMDF (Kernel-Mode Driver Framework)** and **NetAdapterCx** architecture.

- **To build from source:** Open Developer Command Prompt for Visual Studio 2022 (with WDK installed) and run:
  ```cmd
  build.bat
  ```
- **Live Hardware Telemetry Tool:** Check signal, temperature, and live power draw with the included utility:
  ```cmd
  tools\awus_control\awus_control.exe --status
  ```

---

## 📜 License

Licensed under the **GPL-2.0** license. Feel free to use, modify, and improve it!
