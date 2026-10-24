/*
 * MediaTek MT7610U / ALFA AWUS036ACHM Register Definitions
 * Copyright (C) 2026 OpenSource-AWUS036ACHM Project
 *
 * Designed for High-Performance & Maximum Power Delivery on Windows 11.
 */

#pragma once

#include <ntddk.h>

/* USB Hardware IDs */
#define AWUS036ACHM_VID_MEDIATEK    0x0E8D
#define AWUS036ACHM_PID_MT7610U     0x7610
#define AWUS036ACHM_VID_RALINK      0x148F
#define AWUS036ACHM_PID_RT2870      0x7610

/* USB Control Endpoint Vendor Requests */
#define RT_USB_REQ_DEVICE_MODE      0x12
#define RT_USB_REQ_MULTI_READ       0x63
#define RT_USB_REQ_MULTI_WRITE      0x66
#define RT_USB_REQ_EEPROM_READ      0x09
#define RT_USB_REQ_EP0_READ         0x07
#define RT_USB_REQ_EP0_WRITE        0x06

/* USB Bulk Endpoints */
#define AWUS_USB_EP_BULK_IN         0x81    /* RX Data Queue */
#define AWUS_USB_EP_BULK_OUT_MGMT   0x01    /* Management / High Priority */
#define AWUS_USB_EP_BULK_OUT_HCCA   0x02    /* HCCA / AC_VO */
#define AWUS_USB_EP_BULK_OUT_ACVI   0x03    /* AC_VI Video */
#define AWUS_USB_EP_BULK_OUT_ACBE   0x04    /* AC_BE Best Effort */
#define AWUS_USB_EP_BULK_OUT_ACBK   0x05    /* AC_BK Background */

/* Chip Identification & Clock Control */
#define MT_ASIC_VER_ID              0x0000
#define MT_CMB_CTRL                 0x0020
#define MT_CMB_CTRL_XTAL_RDY        (1 << 22)
#define MT_CMB_CTRL_PLL_LD          (1 << 23)

#define MT_WLAN_FUN_CTRL            0x0080
#define MT_WLAN_FUN_CTRL_WLAN_EN    (1 << 0)
#define MT_WLAN_FUN_CTRL_WLAN_CLK_EN (1 << 1)
#define MT_WLAN_FUN_CTRL_WLAN_RESET_RF (1 << 2)
#define MT_WLAN_FUN_CTRL_WLAN_RESET (1 << 3)
#define MT_WLAN_FUN_CTRL_GPIO_OUT_EN (0xFF << 24)

#define MT_LED_CTRL                 0x0770
#define MT_LED_CTRL_REPLAY(n)       (1 << (0 + (8 * (n))))
#define MT_LED_CTRL_POLARITY(n)     (1 << (1 + (8 * (n))))
#define MT_LED_CTRL_TX_BLINK_MODE(n) (1 << (2 + (8 * (n))))
#define MT_LED_CTRL_KICK(n)         (1 << (7 + (8 * (n))))

#define MT_LED_TX_BLINK_0           0x0774
#define MT_LED_TX_BLINK_1           0x0778


/* USB DMA Engine Registers */
#define MT_USB_DMA_CFG              0x0238
#define MT_USB_DMA_CFG_RX_EN        (1 << 0)
#define MT_USB_DMA_CFG_TX_EN        (1 << 1)
#define MT_USB_DMA_CFG_TX_CLR       (1 << 19)
#define MT_USB_DMA_CFG_RX_CLR       (1 << 20)
#define MT_USB_DMA_CFG_RX_AGG_EN    (1 << 22)
#define MT_USB_DMA_CFG_RX_AGG_TO    (0xFF << 24)
#define MT_USB_DMA_CFG_RX_AGG_LMT   (0xFF << 8)
#define MT_USB_CYC_CFG              0x02a4

#define MT_WPDMA_GLO_CFG            0x0208
#define MT_WPDMA_RST_IDX            0x020C

/* MAC Core Registers */
#define MT_MAC_CSR0                 0x1000
#define MT_MAC_SYS_CTRL             0x1004
#define MT_MAC_SYS_CTRL_RESET_CSR   (1 << 0)
#define MT_MAC_SYS_CTRL_RESET_BBP   (1 << 1)
#define MT_MAC_SYS_CTRL_ENABLE_TX   (1 << 2)
#define MT_MAC_SYS_CTRL_ENABLE_RX   (1 << 3)

#define MT_MAC_ADDR_DW0             0x1008
#define MT_MAC_ADDR_DW1             0x100C
#define MT_MAC_BSSID_DW0            0x1010
#define MT_MAC_BSSID_DW1            0x1014
#define MT_MAX_LEN_CFG              0x1024
#define MT_LED_CFG                  0x102C
#define MT_AMPDU_MAX_LEN_20M1S      0x1030

/* Transmit Control Registers */
#define MT_TX_SW_CFG0               0x1300
#define MT_TX_SW_CFG1               0x1304
#define MT_TX_SW_CFG2               0x1308
#define MT_TXOP_CTRL_CFG            0x1340
#define MT_TX_TIMEOUT_CFG           0x1344
#define MT_TX_RTS_CFG               0x1348
#define MT_TX_PROT_CFG6             0x13E0
#define MT_TX_PROT_CFG7             0x13E4
#define MT_TX_PROT_CFG8             0x13E8

/* Maximum Power Delivery & Transmit Power Control (ALC) Registers */
#define MT_TX_ALC_CFG_0             0x13B0
#define MT_TX_ALC_CFG_1             0x13B4
#define MT_TX_ALC_CFG_2             0x13C0
#define MT_TX_ALC_CFG_3             0x13C4
#define MT_TX_ALC_CFG_4             0x13C8

#define MT_TX_PWR_CFG_0             0x1314  /* CCK 1, 2, 5.5, 11 Mbps */
#define MT_TX_PWR_CFG_1             0x1318  /* OFDM 6, 9, 12, 18 Mbps */
#define MT_TX_PWR_CFG_2             0x131C  /* OFDM 24, 36, 48, 54 Mbps */
#define MT_TX_PWR_CFG_3             0x1320  /* HT20/40 MCS 0, 1, 2, 3 */
#define MT_TX_PWR_CFG_4             0x1324  /* HT20/40 MCS 4, 5, 6, 7 */
#define MT_TX_PWR_CFG_7             0x13D4  /* VHT20/40 MCS 0..3 */
#define MT_TX_PWR_CFG_8             0x13D8  /* VHT20/40 MCS 4..7 */
#define MT_TX_PWR_CFG_9             0x13DC  /* VHT80 MCS 0..9 (Peak 433Mbps) */

/* BBP / RF Subsystem */
#define MT_BBP_REG_BASE             0x0500
#define MT_RF_CSR_CFG               0x0500
#define MT_RF_BYPASS_0              0x0504
#define MT_RF_SETTING_0             0x0508

/* Microcontroller (MCU) Registers */
#define MT_MCU_RESET_CTL            0x070C
#define MT_MCU_INT_LEVEL            0x0718
#define MT_MCU_COM_REG0             0x0730
#define MT_MCU_COM_REG1             0x0734
#define MT_MCU_COM_REG2             0x0738
#define MT_MCU_COM_REG3             0x073C
#define MT_MCU_SEMAPHORE_00         0x07B0
#define MT_MCU_ILM_ADDR             0x00080000
#define MT_MCU_DLM_OFFSET           0x00041000
#define MT_MCU_IVB_SIZE             64

/* MCU Command Codes */
enum MCU_COMMAND {
    MCU_CMD_FUN_SET_OP          = 1,
    MCU_CMD_LOAD_CR             = 2,
    MCU_CMD_INIT_GAIN_OP        = 3,
    MCU_CMD_DYNC_VGA_OP         = 6,
    MCU_CMD_TDLS_CH_SW          = 7,
    MCU_CMD_BURST_WRITE         = 8,
    MCU_CMD_READ_MODIFY_WRITE   = 9,
    MCU_CMD_RANDOM_READ         = 10,
    MCU_CMD_BURST_READ          = 11,
    MCU_CMD_RANDOM_WRITE        = 12,
    MCU_CMD_LED_MODE_OP         = 16,
    MCU_CMD_POWER_SAVING_OP     = 20,
    MCU_CMD_WOW_CONFIG          = 21,
    MCU_CMD_CARRIER_DETECT_OP   = 28,
    MCU_CMD_RADAR_DETECT_OP     = 29,
    MCU_CMD_SWITCH_CHANNEL_OP   = 30,
    MCU_CMD_CALIBRATION_OP      = 31,
    MCU_CMD_BEACON_OP           = 32,
    MCU_CMD_ANTENNA_OP          = 33,
};

/* EEPROM / eFuse Offsets */
#define MT_EE_NIC_CONF_0            0x34
#define MT_EE_NIC_CONF_1            0x36
#define MT_EE_FREQ_OFFSET           0x3A
#define MT_EE_MAC_ADDR_0            0x04
#define MT_EE_MAC_ADDR_1            0x06
#define MT_EE_MAC_ADDR_2            0x08
#define MT_EE_2G_TARGET_POWER       0xD0
#define MT_EE_5G_TARGET_POWER       0xD2
#define MT_EE_EXT_PA_2G             0x36
#define MT_EE_EXT_PA_5G             0x38

/* TXWI (Transmit Wireless Information) Header Descriptor */
#pragma pack(push, 1)
typedef struct _MT7610_TXWI {
    /* DWORD 0 */
    UINT32 FrgNum : 3;
    UINT32 Mmps : 1;
    UINT32 CFACK : 1;
    UINT32 TS : 1;
    UINT32 AMPDU : 1;
    UINT32 MpduDensity : 3;
    UINT32 TxPwrAdj : 4;
    UINT32 MCS : 6;
    UINT32 BW : 2;          /* 0:20M, 1:40M, 2:80M */
    UINT32 SGI : 1;         /* Short GI */
    UINT32 Stbc : 1;
    UINT32 eTxBF : 1;
    UINT32 Sounding : 1;
    UINT32 PhyMode : 3;     /* 0:Legacy CCK, 1:OFDM, 2:HT Mix, 3:HT Green, 4:VHT */
    UINT32 TxOp : 2;

    /* DWORD 1 */
    UINT32 Ack : 1;
    UINT32 NSEQ : 1;
    UINT32 BAWinSize : 6;
    UINT32 WCID : 8;
    UINT32 Length : 12;
    UINT32 TxStream : 4;

    /* DWORD 2 */
    UINT32 IV : 32;

    /* DWORD 3 */
    UINT32 EIV : 32;
} MT7610_TXWI, *PMT7610_TXWI;

/* RXWI (Receive Wireless Information) Header Descriptor */
typedef struct _MT7610_RXWI {
    /* DWORD 0 */
    UINT32 WCID : 8;
    UINT32 KeyIdx : 2;
    UINT32 BSSIdx : 3;
    UINT32 U2M : 1;
    UINT32 Mcast : 1;
    UINT32 Bcast : 1;
    UINT32 MyBss : 1;
    UINT32 CrcErr : 1;
    UINT32 CipherErr : 2;
    UINT32 AMSDU : 1;
    UINT32 HTC : 1;
    UINT32 RSSI : 8;        /* Raw RSSI */

    /* DWORD 1 */
    UINT32 FrgNum : 4;
    UINT32 Seq : 12;
    UINT32 MPDULen : 12;
    UINT32 TID : 4;

    /* DWORD 2 */
    UINT32 MCS : 6;
    UINT32 BW : 2;
    UINT32 SGI : 1;
    UINT32 Stbc : 1;
    UINT32 PhyMode : 3;
    UINT32 SNR : 8;
    UINT32 Reserved : 11;

    /* DWORD 3 */
    UINT32 Timestamp : 32;
} MT7610_RXWI, *PMT7610_RXWI;
#pragma pack(pop)
