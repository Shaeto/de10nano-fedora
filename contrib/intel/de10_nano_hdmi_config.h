/*
 * The MIT License (MIT)
 * Copyright (c) 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _DE10_NANO_HDMI_CONFIG_H_
#define _DE10_NANO_HDMI_CONFIG_H_

/* PLL calculation parameters */
#define PLL_REF_FREQ	(50000000)
#define MAX_INPUT_FREQ	(165000000)
#define MIN_INPUT_FREQ	(65000000)
#define MIN_MULTIPLE	(8)
#define MAX_MULTIPLE	(26)

/* macros for fixed point algorithm */
#define FXD_PNT_1P0	(1ULL << 32)
#define FXD_PNT_0P95	((unsigned long long)(FXD_PNT_1P0 * 0.95))
#define FXD_PNT_0P05	((unsigned long long)(FXD_PNT_1P0 * 0.05))

/* pll calc function data structure */
struct pll_calc_struct {
	unsigned long desired_frequency;
	unsigned long m_value;
	unsigned long c_value;
	unsigned long k_value;
};

/* LWH2F bridge */
#define LWH2F_BASE	(0xFF200000)

/* SYSID component */
#define SYSID_BASE	(0x1000)
#define SYSID_ID	2899645442
#define SYSID_TS	1485984203

/* CVO RESET PIO component */
#define CVO_RESET_PIO_BASE	(0xC000)

/* PLL LOCKED PIO component */
#define PLL_LOCKED_PIO_BASE	(0xD000)

/* PLL RESET PIO component */
#define PLL_RESET_PIO_BASE	(0xB000)

/* CVO component */
#define CVO_BASE		(0x9000)
#define CVO_CNTL_REG		(0)
#define CVO_STAT_REG		(1)
#define CVO_INTR_REG		(2)
#define CVO_VID_MODE_MATCH_REG	(3)
#define CVO_BANK_SELECT_REG	(4)
#define CVO_M_CNTL_REG		(5)
#define CVO_M_SMPL_CNT_REG	(6)
#define CVO_M_F0_LN_CNT_REG	(7)
#define CVO_M_F1_LN_CNT_REG	(8)
#define CVO_M_HOR_FRNT_PRCH_REG	(9)
#define CVO_M_HOR_SYNC_LEN_REG	(10)
#define CVO_M_HOR_BLNK_REG	(11)
#define CVO_M_VER_FRNT_PRCH_REG	(12)
#define CVO_M_VER_SYNC_LEN_REG	(13)
#define CVO_M_VER_BLNK_REG	(14)
#define CVO_M_F0_VER_F_PRCH_REG	(15)
#define CVO_M_F0_VER_SYNC_REG	(16)
#define CVO_M_F0_VER_BLNK_REG	(17)
#define CVO_M_ACT_PIC_LINE_REG	(18)
#define CVO_M_F0_VER_RIS_REG	(19)
#define CVO_M_FLD_RIS_REG	(20)
#define CVO_M_FLD_FLL_REG	(21)
#define CVO_M_STNDRD_REG	(22)
#define CVO_M_SOF_SMPL_REG	(23)
#define CVO_M_SOF_LINE_REG	(24)
#define CVO_M_VCOCLK_DIV_REG	(25)
#define CVO_M_ANC_LINE_REG	(26)
#define CVO_M_F0_ANC_LINE_REG	(27)
#define CVO_M_H_SYNC_POL_REG	(28)
#define CVO_M_V_SYNC_POL_REG	(29)
#define CVO_M_VALID_REG		(30)

/* FBR component */
#define FBR_BASE		(0x8000)
#define FBR_CNTL_REG		(0)
#define FBR_STAT_REG		(1)
#define FBR_INTR_REG		(2)
#define FBR_FRM_CNTR_REG	(3)
#define FBR_REP_CNTR_REG	(4)
#define FBR_FRM_INFO_REG	(5)
#define FBR_FRM_STRT_ADDR_REG	(6)
#define FBR_FRM_RDR_REG		(7)
#define FBR_MISC_REG		(8)

/* video buffer in system DRAM */
#define VIDEO_BUFFER	(0x3F000000)

/* PLL calculation parameters */
#define PLL_REF_FREQ	(50000000)
#define MAX_INPUT_FREQ	(165000000)
#define MIN_INPUT_FREQ	(65000000)
#define MIN_MULTIPLE	(8)
#define MAX_MULTIPLE	(26)
#define X_WIDTH		(32)

/* PLL RECONFIG component */
#define PLL_RECNFG_BASE	(0xA000)
#define PLL_MODE_REG	(0)
#define PLL_STATUS_REG	(1)
#define PLL_START_REG	(2)
#define PLL_N_CNTR_REG	(3)
#define PLL_M_CNTR_REG	(4)
#define PLL_C_CNTR_REG	(5)
#define PLL_DPS_REG	(6)
#define PLL_K_REG	(7)
#define PLL_BW_REG	(8)
#define PLL_CP_REG	(9)
#define PLL_C0_RD_REG	(10)
#define PLL_VCODIV_REG	(0x1C)

/* ADV7513 component */
#define I2C_BUS "/dev/i2c-1"
#define ADV7513_I2C_DEFAULT_BUS_NUMBER (2)
#define ADV7513_MAIN_ADDR       (0x39)
#define ADV7513_EDID_ADDR       (0x3F)
#define ADV7513_CHIP_ID_HI      (0xF5)
#define ADV7513_CHIP_ID_HI_VAL  (0x75)
#define ADV7513_CHIP_ID_LO      (0xF6)
#define ADV7513_CHIP_ID_LO_VAL  (0x11)
#define ADV7513_PWR_DWN         (0x41)
#define ADV7513_PWR_DWN_BIT     (0x40)
#define ADV7513_HPD_MNSNS       (0x42)
#define ADV7513_HPD_MNSNS_BITS  (0x60)
#define ADV7513_EDID_RDY        (0x96)
#define ADV7513_EDID_RDY_BIT    (0x04)
#define ADV7513_HPD_CNTL        (0xD6)
#define ADV7513_HPD_CNTL_BITS   (0xC0)

typedef struct {
	u_char addr;
	u_char value;
} init_config;

/* common use strings */
#define ERROR_STR	"HDMI ERROR: "
#define WARN_STR	"HDMI WARNING: "
#define INFO_STR	"HDMI INFO: "
#define HDMI_STATUS_ENV	"HDMI_status"
#define HDMI_ERROR_ENV	"HDMI_error"
#define HDMI_INFO_ENV	"HDMI_info"

/* BMP image support */
#define BMP_IMAGE_BASE	(0x0C300000)
struct bmp_image_header {
	uint16_t bmp_signature;
	uint32_t bmp_file_size;
	uint16_t bmp_reserved_1;
	uint16_t bmp_reserved_2;
	uint32_t bmp_bitmap_offset;

	uint32_t bmp_header_size;
	uint32_t bmp_width;
	uint32_t bmp_height;
	uint16_t bmp_color_planes;
	uint16_t bmp_bits_per_pixel;
	uint32_t bmp_compression_method;
	uint32_t bmp_image_size;
	uint32_t bmp_h_resolution;
	uint32_t bmp_v_resolution;
	uint32_t bmp_number_of_colors;
	uint32_t bmp_important_colors;
} __attribute__((packed));

struct bmp_24_bit_pixel {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

#endif /* _DE10_NANO_HDMI_CONFIG_H_ */

