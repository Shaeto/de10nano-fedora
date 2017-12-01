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

#include <common.h>
#include <exports.h>
#include "de10_nano_hdmi_config.h"

/*
This program is built to run as a u-boot standalone application and leverage the
u-boot provided runtime environment.  Prior to running this program you will
need to configure a few things in the u-boot environment.

run fpga_cfg
i2c dev 2
load mmc 0:1 0x0c300000 STARTUP.BMP
dcache flush
dcache off

load this program into memory at 0x0C100000 and then run it at 0x0C100001, yes
you set the lsb of the address to indicate a thumb mode branch or something like
that.

load mmc 0:1 0x0c100000 de10_nano_hdmi_config.bin
go 0x0C100001

*/

/* ADV7513 register configurations */
init_config init_config_array[] = {
	{0x98, 0x03},	// must be set
	{0x9A, 0xE0},	// must be set
	{0x9C, 0x30},	// must be set
	{0x9D, 0x61},	// must be set
	{0xA2, 0xA4},	// must be set
	{0xA3, 0xA4},	// must be set
	{0xE0, 0xD0},	// must be set
	{0xF9, 0x00},	// must be set
	{0x16, 0x30},	// 8-bit color depth
	{0x17, 0x02},	// aspect ratio 16:9, modified below if needed
	{0xAF, 0x06},	// HDMI mode, no HDCP
	{0x0C, 0x00},	// disable I2S inputs
	{0x96, 0xF6},	// clear all interrupts
};

/* prototypes */
void pll_calc_fixed(struct pll_calc_struct *the_pll_calc_struct);
void uitoa(uint32_t uint32_input, char **output_str);

/* main configuration function */
int de10_nano_hdmi_config(int argc, char * const argv[]) {

	int i;
	int j;
	int result;
	char *print_str;
	uint8_t adv7513_read_buffer[256];
	uint8_t adv7513_edid_buffer[256];
	uint8_t adv7513_write_val;
	uint8_t checksum;
	uint8_t edid_header_pattern_array[8] = {
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
	uint8_t *descriptor_base;
	uint8_t dtd_offset;
	uint8_t dtd_count;

	uint16_t pixel_clock;
	uint16_t horizontal_active_pixels;
	uint16_t horizontal_blanking_pixels;
	uint16_t vertical_active_lines;
	uint16_t vertical_blanking_lines;
	uint16_t horizontal_sync_offset;
	uint16_t horizontal_sync_width;
	uint8_t vertical_sync_offset;
	uint8_t vertical_sync_width;
	uint8_t interlaced;

	uint32_t pixel_clock_MHz;
	uint8_t bad_pixel_clock_MHz;
	uint8_t bad_horizontal_active_pixels_value;
	uint8_t bad_vertical_active_lines_value;
	uint8_t bad_interlaced_value;
	uint8_t valid_timing_configuration_located;
	uint8_t monitor_connected;

	int32_t aspect_ratio;

	uint32_t N_reg;
	uint32_t M_reg;
	uint32_t C_reg;
	uint32_t K_reg;
	uint32_t BW_reg;
	uint32_t CP_reg;
	uint32_t VCODIV_reg;

	volatile uint32_t *pll_ptr;
	volatile uint32_t *fbr_ptr;
	volatile uint32_t *cvo_ptr;
	volatile uint32_t *cvo_reset_pio_ptr;
	volatile uint32_t *pll_reset_pio_ptr;
	volatile uint32_t *pll_locked_pio_ptr;
	volatile uint32_t *video_ptr;
	volatile struct bmp_image_header *bmp_header_ptr;
	volatile struct bmp_24_bit_pixel *bmp_pixel_ptr;

	struct pll_calc_struct shared_struct;

	char snprintf_buffer[256];
	char *snprintf_buffer_ptr;
	uint32_t milestones;

	/* initialize u-boot application environment */
	app_startup(argv);

	/* initialize pointers and status */
	setenv(HDMI_STATUS_ENV, "startup");
	setenv(HDMI_INFO_ENV, "none");
	setenv(HDMI_ERROR_ENV, "none");

        pll_ptr = (uint32_t*)(LWH2F_BASE + PLL_RECNFG_BASE);
	fbr_ptr = (uint32_t*)(LWH2F_BASE + FBR_BASE);
	cvo_ptr = (uint32_t*)(LWH2F_BASE + CVO_BASE);
	cvo_reset_pio_ptr = (uint32_t*)(LWH2F_BASE + CVO_RESET_PIO_BASE);
	pll_reset_pio_ptr = (uint32_t*)(LWH2F_BASE + PLL_RESET_PIO_BASE);
	pll_locked_pio_ptr = (uint32_t*)(LWH2F_BASE + PLL_LOCKED_PIO_BASE);
	video_ptr = (uint32_t*)VIDEO_BUFFER;

	valid_timing_configuration_located = 0;
	milestones = 0;

	/* since we cannot read a single register from the ADV7513 with the
	 * default I2C driver, we just read the first N registers starting
	 * from ZERO and reaching up to the register we want
	 */
	setenv(HDMI_STATUS_ENV, "read ADV7513 chip ID");
	milestones |= 0x01 << 0;
	result = i2c_read(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			0x00,			// unsigned int addr
			0,			// int alen
			adv7513_read_buffer,	// uint8_t *buffer
			ADV7513_CHIP_ID_LO + 1	// int len
		);

	if(result != 0) {
		print_str = "reading I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(1);
	}

	/* verify the chip id for the ADV7513 */
	setenv(HDMI_STATUS_ENV, "verify ADV7513 chip ID");
	milestones |= 0x01 << 1;
	if(
		(adv7513_read_buffer[ADV7513_CHIP_ID_HI] !=
			ADV7513_CHIP_ID_HI_VAL) ||
		(adv7513_read_buffer[ADV7513_CHIP_ID_LO] !=
			ADV7513_CHIP_ID_LO_VAL) ) {

		print_str = "Bad Chip ID";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(2);
	}

	/* verify that we see a monitor attached to the HDMI connector */
	setenv(HDMI_STATUS_ENV, "verify monitor attached");
	milestones |= 0x01 << 2;
	monitor_connected = 1;
	if((adv7513_read_buffer[ADV7513_HPD_MNSNS] & ADV7513_HPD_MNSNS_BITS) !=
		ADV7513_HPD_MNSNS_BITS) {

		print_str = "No HDMI display detected";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		monitor_connected = 0;
	}

	/* force HPD true */
	setenv(HDMI_STATUS_ENV, "force HPD true");
	milestones |= 0x01 << 3;
	adv7513_write_val = ADV7513_HPD_CNTL_BITS;
	result = i2c_write(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			ADV7513_HPD_CNTL,	// unsigned int addr
			1,			// int alen
			&adv7513_write_val,	// uint8_t *buffer
			1			// int len
		);

	if(result != 0) {
		print_str = "writing I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(4);
	}

	/* power down the ADV7513 */
	setenv(HDMI_STATUS_ENV, "power down ADV7513");
	milestones |= 0x01 << 4;
	adv7513_write_val = adv7513_read_buffer[ADV7513_PWR_DWN];
	adv7513_write_val |= ADV7513_PWR_DWN_BIT;
	result = i2c_write(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			ADV7513_PWR_DWN,	// unsigned int addr
			1,			// int alen
			&adv7513_write_val,	// uint8_t *buffer
			1			// int len
		);

	if(result != 0) {
		print_str = "writing I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(4);
	}

	/* power up the ADV7513 */
	setenv(HDMI_STATUS_ENV, "power up ADV7513");
	milestones |= 0x01 << 5;
	adv7513_write_val &= ~ADV7513_PWR_DWN_BIT;
	result = i2c_write(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			ADV7513_PWR_DWN,	// unsigned int addr
			1,			// int alen
			&adv7513_write_val,	// uint8_t *buffer
			1			// int len
		);

	if(result != 0) {
		print_str = "writing I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(5);
	}

	/*
	if we sense a monitor is attached then we attempt to read the EDID data
	and configure ourselves according to that.  Otherwise we apply a default
	1024*728 configuration
	*/
	setenv(HDMI_STATUS_ENV, "monitor present fork");
	milestones |= 0x01 << 6;
	if(monitor_connected == 0)
		goto post_EDID_evaluation;

	/* wait for the EDID data to become ready */
	setenv(HDMI_STATUS_ENV, "wait EDID ready");
	milestones |= 0x01 << 7;
	for(i = 0 ; i < 1000 ; i++) {
		result = i2c_read(
				ADV7513_MAIN_ADDR,	// uint8_t chip
				0x00,			// unsigned int addr
				0,			// int alen
				adv7513_read_buffer,	// uint8_t *buffer
				ADV7513_EDID_RDY + 1	// int len
			);

		if(result != 0) {
			print_str = "reading I2C";
			printf("%s%s\n", ERROR_STR, print_str);
			setenv(HDMI_ERROR_ENV, print_str);
			print_str = "HDMI_milestones";
			snprintf_buffer_ptr = &snprintf_buffer[0];
			uitoa(milestones, &snprintf_buffer_ptr);
			setenv(print_str, snprintf_buffer);
			printf("%s = %s\n", print_str, snprintf_buffer);
			return(6);
		}

		if((adv7513_read_buffer[ADV7513_EDID_RDY] &
			ADV7513_EDID_RDY_BIT) == ADV7513_EDID_RDY_BIT)
			break;
	}

	if(i >= 1000) {
		print_str = "EDID timeout";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* read the EDID data */
	setenv(HDMI_STATUS_ENV, "read EDID data");
	milestones |= 0x01 << 8;
	result = i2c_read(
			ADV7513_EDID_ADDR,	// uint8_t chip
			0x00,			// unsigned int addr
			0,			// int alen
			adv7513_edid_buffer,	// uint8_t *buffer
			256			// int len
		);

	if(result != 0) {
		print_str = "reading I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(8);
	}

	/* verify EDID block 0 checksum */
	setenv(HDMI_STATUS_ENV, "verify EDID block 0 checksum");
	milestones |= 0x01 << 9;
	checksum = 0;
	for(i = 0 ; i < 128 ; i++)
		checksum += adv7513_edid_buffer[i];

	if(checksum != 0) {
		print_str = "EDID block 0 checksum";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* verify block 0 header pattern */
	setenv(HDMI_STATUS_ENV, "verify block 0 header pattern");
	milestones |= 0x01 << 10;
	for(i = 0 ; i < 8 ; i++) {
		if(edid_header_pattern_array[i] != adv7513_edid_buffer[i]) {
			print_str = "EDID header pattern";
			printf("%s%s\n", WARN_STR, print_str);
			setenv(HDMI_ERROR_ENV, print_str);
			goto post_EDID_evaluation;
		}
	}

	/* decode descriptor blocks */
	setenv(HDMI_STATUS_ENV, "decode EDID block 0");
	milestones |= 0x01 << 11;
	bad_horizontal_active_pixels_value = 0;
	bad_vertical_active_lines_value = 0;
	bad_interlaced_value = 0;
	for(i = 0 ; i < 4 ; i++) {
		descriptor_base = &adv7513_edid_buffer[54];
		descriptor_base += i * 18;
		if(
			(descriptor_base[0] == 0) &&
			(descriptor_base[1] == 0) &&
			(descriptor_base[2] == 0) &&
			(descriptor_base[4] == 0)
		  ) {
			/* not a Detailed Timing Descriptor */
			continue;
		}
		/* Detailed Timing Descriptor
		 * extract the relevant fields of the descriptor
		 */
		pixel_clock =
			(descriptor_base[1] << 8) |
			descriptor_base[0];

		horizontal_active_pixels =
			(((descriptor_base[4] >> 4) & 0x0F) << 8) |
			descriptor_base[2];

		horizontal_blanking_pixels =
			((descriptor_base[4] & 0x0F) << 8) |
			descriptor_base[3];

		vertical_active_lines =
			(((descriptor_base[7] >> 4) & 0x0F) << 8) |
			descriptor_base[5];

		vertical_blanking_lines =
			((descriptor_base[7] & 0x0F) << 8) |
			descriptor_base[6];

		horizontal_sync_offset =
			(((descriptor_base[11] >> 6) & 0x03) << 8) |
			descriptor_base[8];

		horizontal_sync_width =
			(((descriptor_base[11] >> 4) & 0x03) << 8) |
			descriptor_base[9];

		vertical_sync_offset =
			(((descriptor_base[11] >> 2) & 0x03) << 4) |
			((descriptor_base[10] >> 4) & 0x0F);

		vertical_sync_width =
			((descriptor_base[11] & 0x03) << 4) |
			(descriptor_base[10] & 0x0F);

		interlaced = (descriptor_base[17] & 0x80) ? 1 : 0;

		/* adjust pixel clock up to MHz */
		pixel_clock_MHz = pixel_clock * 10000;

		/* check for valid ranges of key parameters */
		if((pixel_clock_MHz > 150000000) ||
			(pixel_clock_MHz < 60000000)) {
			bad_pixel_clock_MHz++;
			continue;
		}
		if((horizontal_active_pixels > 1920) ||
			(horizontal_active_pixels < 1280)) {
			bad_horizontal_active_pixels_value++;
			continue;
		}
		if((vertical_active_lines > 1080) ||
			(vertical_active_lines < 720)) {
			bad_vertical_active_lines_value++;
			continue;
		}
		if(interlaced != 0) {
			bad_interlaced_value++;
			continue;
		}
		valid_timing_configuration_located = 1;
		break;
	}

	if(valid_timing_configuration_located != 0)
		goto post_EDID_evaluation;

	/* check for extension blocks */
	setenv(HDMI_STATUS_ENV, "check for extension blocks");
	milestones |= 0x01 << 31;
	if(adv7513_edid_buffer[126] == 0)
		goto post_EDID_evaluation;

	/* verify extension block checksum */
	setenv(HDMI_STATUS_ENV, "verify extension block checksum");
	milestones |= 0x01 << 30;
	checksum = 0;
	for(i = 0 ; i < 128 ; i++)
		checksum += adv7513_edid_buffer[128 + i];

	if(checksum != 0) {
		print_str = "extension block 1 checksum";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* verify extension tag */
	setenv(HDMI_STATUS_ENV, "verify extension tag");
	milestones |= 0x01 << 29;
	if(adv7513_edid_buffer[128] != 0x02) {
		print_str = "extension tag";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* verify revision number */
	setenv(HDMI_STATUS_ENV, "verify revision number");
	milestones |= 0x01 << 28;
	if(adv7513_edid_buffer[129] != 0x03) {
		print_str = "extension revision number";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* check for DTDs in extension block */
	setenv(HDMI_STATUS_ENV, "check for DTDs in extension block");
	milestones |= 0x01 << 27;
	dtd_offset = adv7513_edid_buffer[130];
	dtd_count = adv7513_edid_buffer[131] & 0x0F;
	if(
		(dtd_offset == 0x00) ||
		(dtd_count == 0x00)
	  ) {
		print_str = "No DTDs present in extension block";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		goto post_EDID_evaluation;
	}

	/* decode descriptor blocks */
	setenv(HDMI_STATUS_ENV, "decode EDID block 1");
	milestones |= 0x01 << 26;
	for(i = 0 ; i < dtd_count ; i++) {
		descriptor_base = &adv7513_edid_buffer[128];
		descriptor_base += dtd_offset;
		descriptor_base += i * 18;
		if(
			(descriptor_base[0] == 0) &&
			(descriptor_base[1] == 0) &&
			(descriptor_base[2] == 0) &&
			(descriptor_base[4] == 0)
		  ) {
			/* not a Detailed Timing Descriptor */
			continue;
		}
		/* Detailed Timing Descriptor
		 * extract the relevant fields of the descriptor
		 */
		pixel_clock =
			(descriptor_base[1] << 8) |
			descriptor_base[0];

		horizontal_active_pixels =
			(((descriptor_base[4] >> 4) & 0x0F) << 8) |
			descriptor_base[2];

		horizontal_blanking_pixels =
			((descriptor_base[4] & 0x0F) << 8) |
			descriptor_base[3];

		vertical_active_lines =
			(((descriptor_base[7] >> 4) & 0x0F) << 8) |
			descriptor_base[5];

		vertical_blanking_lines =
			((descriptor_base[7] & 0x0F) << 8) |
			descriptor_base[6];

		horizontal_sync_offset =
			(((descriptor_base[11] >> 6) & 0x03) << 8) |
			descriptor_base[8];

		horizontal_sync_width =
			(((descriptor_base[11] >> 4) & 0x03) << 8) |
			descriptor_base[9];

		vertical_sync_offset =
			(((descriptor_base[11] >> 2) & 0x03) << 4) |
			((descriptor_base[10] >> 4) & 0x0F);

		vertical_sync_width =
			((descriptor_base[11] & 0x03) << 4) |
			(descriptor_base[10] & 0x0F);

		interlaced = (descriptor_base[17] & 0x80) ? 1 : 0;

		/* adjust pixel clock up to MHz */
		pixel_clock_MHz = pixel_clock * 10000;

		/* check for valid ranges of key parameters */
		if((pixel_clock_MHz > 150000000) ||
			(pixel_clock_MHz < 60000000)) {
			bad_pixel_clock_MHz++;
			continue;
		}
		if((horizontal_active_pixels > 1920) ||
			(horizontal_active_pixels < 1280)) {
			bad_horizontal_active_pixels_value++;
			continue;
		}
		if((vertical_active_lines > 1080) ||
			(vertical_active_lines < 720)) {
			bad_vertical_active_lines_value++;
			continue;
		}
		if(interlaced != 0) {
			bad_interlaced_value++;
			continue;
		}
		valid_timing_configuration_located = 1;
		break;
	}

post_EDID_evaluation:

	/* if no valid timing is found, then set 1024x768 default */
	setenv(HDMI_STATUS_ENV, "evaluate timing configuration");
	milestones |= 0x01 << 12;
	if(valid_timing_configuration_located == 0) {
		print_str = "no valid timing found, setting 1024x768 default";
		printf("%s%s\n", WARN_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);

		pixel_clock_MHz = 65000000;
		horizontal_active_pixels = 1024;
		horizontal_blanking_pixels = 320;
		vertical_active_lines = 768;
		vertical_blanking_lines = 38;
		horizontal_sync_offset = 24;
		horizontal_sync_width = 136;
		vertical_sync_offset = 3;
		vertical_sync_width = 6;
		interlaced = 0;
	}

	/* determine the aspect ratio of the timing parameters */
	aspect_ratio = (horizontal_active_pixels * 9) -
			(vertical_active_lines * 16);

	if(abs(aspect_ratio) > (horizontal_active_pixels * 2))
		for(i = 0 ;
			i < (int)(sizeof(init_config_array) /
				sizeof(init_config)) ;
			i++)
			if(init_config_array[i].addr == 0x17)
				/* set to 4:3 aspect */
				init_config_array[i].value = 0x00;

	/* calculate the PLL reconfiguration register values */
	shared_struct.desired_frequency = pixel_clock_MHz;
	shared_struct.m_value = 0;
	shared_struct.c_value = 0;
	shared_struct.k_value = 0;
	pll_calc_fixed(&shared_struct);

	if(shared_struct.desired_frequency == 0) {
		print_str = "PLL calculation failure";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(11);
	}

	/* stop the FBR and wait for it to idle */
	setenv(HDMI_STATUS_ENV, "stop FBR");
	milestones |= 0x01 << 13;
	fbr_ptr[FBR_CNTL_REG] = 0x00000000;

	for(i = 0 ; i < 100000 ; i++) {
		if((fbr_ptr[FBR_STAT_REG] & 0x01) == 0x00)
			break;
	}
	if(i >= 100000) {
		print_str = "FBR stop timeout";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(12);
	}

	/* stop the CVO */
	setenv(HDMI_STATUS_ENV, "stop CVO");
	milestones |= 0x01 << 14;
	cvo_ptr[CVO_CNTL_REG] = 0x00000000;

	for(i = 0 ; i < 100000 ; i++) {
		if((cvo_ptr[CVO_STAT_REG] & 0x01) == 0x00)
			break;
	}
	if(i >= 100000) {
		print_str = "CVO stop timeout";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(13);
	}

	/* assert CVO reset */
	setenv(HDMI_STATUS_ENV, "assert CVO reset");
	milestones |= 0x01 << 15;
	cvo_reset_pio_ptr[0] = 0x00000001;

	/* apply PLL reconfiguration */
	N_reg = 0x00010000;

	if((shared_struct.m_value & 0x01) != 0) {
		M_reg = (1 << 17) |
			(((shared_struct.m_value >> 1) + 1) << 8) |
			(shared_struct.m_value >> 1);
	} else {
		M_reg = ((shared_struct.m_value >> 1) << 8) |
			(shared_struct.m_value >> 1);
	}

	if((shared_struct.c_value & 0x01) != 0) {
		C_reg = (1 << 17) |
			(((shared_struct.c_value >> 1) + 1) << 8) |
			(shared_struct.c_value >> 1);
	} else {
		C_reg = ((shared_struct.c_value >> 1) << 8) |
			(shared_struct.c_value >> 1);
	}

	K_reg = shared_struct.k_value;

	if((shared_struct.m_value == 12) || (shared_struct.m_value == 13)) {
		BW_reg = 8;
		CP_reg = 3;
	} else {
		BW_reg = 7;
		CP_reg = 2;
	}

	VCODIV_reg = 0x00000000;

        pll_ptr[PLL_MODE_REG]	= 0x00;		/* waitrequest mode */
        pll_ptr[PLL_N_CNTR_REG] = N_reg;
        pll_ptr[PLL_M_CNTR_REG] = M_reg;
        pll_ptr[PLL_C_CNTR_REG] = C_reg;
        pll_ptr[PLL_K_REG]      = K_reg;
        pll_ptr[PLL_BW_REG]     = BW_reg;
        pll_ptr[PLL_CP_REG]     = CP_reg;
        pll_ptr[PLL_VCODIV_REG] = VCODIV_reg;

        pll_ptr[PLL_START_REG] = 0x01;

	/* assert and release PLL reset, then wait for lock */
	setenv(HDMI_STATUS_ENV, "reset PLL");
	milestones |= 0x01 << 16;
	pll_reset_pio_ptr[0] = 0x00000001;
	pll_reset_pio_ptr[0] = 0x00000000;

	for(i = 0 ; i < 100000 ; i++) {
		if((pll_locked_pio_ptr[0] & 0x01) == 0x01)
			break;
	}
	if(i >= 100000) {
		print_str = "PLL lock timeout";
		printf("%s%s\n", ERROR_STR, print_str);
		setenv(HDMI_ERROR_ENV, print_str);
		print_str = "HDMI_milestones";
		snprintf_buffer_ptr = &snprintf_buffer[0];
		uitoa(milestones, &snprintf_buffer_ptr);
		setenv(print_str, snprintf_buffer);
		printf("%s = %s\n", print_str, snprintf_buffer);
		return(14);
	}

	/* release CVO reset */
	setenv(HDMI_STATUS_ENV, "release CVO reset");
	milestones |= 0x01 << 17;
	cvo_reset_pio_ptr[0] = 0x00000000;

	/* configure the FBR */
	fbr_ptr[FBR_FRM_INFO_REG] = (horizontal_active_pixels << 13) |
					vertical_active_lines;
	fbr_ptr[FBR_MISC_REG] = 0x00000000;
	fbr_ptr[FBR_FRM_STRT_ADDR_REG] = VIDEO_BUFFER;

	/* start the FBR */
	fbr_ptr[FBR_CNTL_REG] = 0x00000001;

	/* start the CVO */
	cvo_ptr[CVO_CNTL_REG] = 0x00000007;

	/* configure the CVO */
	cvo_ptr[CVO_BANK_SELECT_REG]		= 0;
	cvo_ptr[CVO_M_VALID_REG]		= 0;
	cvo_ptr[CVO_M_CNTL_REG]			= 0;
	cvo_ptr[CVO_M_SMPL_CNT_REG]		= horizontal_active_pixels;
	cvo_ptr[CVO_M_F0_LN_CNT_REG]		= vertical_active_lines;
	cvo_ptr[CVO_M_F1_LN_CNT_REG]		= 0;
	cvo_ptr[CVO_M_HOR_FRNT_PRCH_REG]	= horizontal_sync_offset;
	cvo_ptr[CVO_M_HOR_SYNC_LEN_REG]		= horizontal_sync_width;
	cvo_ptr[CVO_M_HOR_BLNK_REG]		= horizontal_blanking_pixels;
	cvo_ptr[CVO_M_VER_FRNT_PRCH_REG]	= vertical_sync_offset;
	cvo_ptr[CVO_M_VER_SYNC_LEN_REG]		= vertical_sync_width;
	cvo_ptr[CVO_M_VER_BLNK_REG]		= vertical_blanking_lines;
	cvo_ptr[CVO_M_F0_VER_F_PRCH_REG]	= 0;
	cvo_ptr[CVO_M_F0_VER_SYNC_REG]		= 0;
	cvo_ptr[CVO_M_F0_VER_BLNK_REG]		= 0;
	cvo_ptr[CVO_M_ACT_PIC_LINE_REG]		= 0;
	cvo_ptr[CVO_M_F0_VER_RIS_REG]		= 0;
	cvo_ptr[CVO_M_FLD_RIS_REG]		= 0;
	cvo_ptr[CVO_M_FLD_FLL_REG]		= 0;
	cvo_ptr[CVO_M_STNDRD_REG]		= 0;
	cvo_ptr[CVO_M_SOF_SMPL_REG]		= 0;
	cvo_ptr[CVO_M_SOF_LINE_REG]		= 0;
	cvo_ptr[CVO_M_VCOCLK_DIV_REG]		= 0;
	cvo_ptr[CVO_M_ANC_LINE_REG]		= 0;
	cvo_ptr[CVO_M_F0_ANC_LINE_REG]		= 0;
	cvo_ptr[CVO_M_H_SYNC_POL_REG]		= 1;
	cvo_ptr[CVO_M_V_SYNC_POL_REG]		= 1;
	cvo_ptr[CVO_M_VALID_REG]		= 1;

	/* configure the ADV7513 */
	setenv(HDMI_STATUS_ENV, "configure ADV7513");
	milestones |= 0x01 << 18;
	for(i = 0 ; i < (int)(sizeof(init_config_array) / sizeof(init_config))
			; i++) {

		result = i2c_write(
			ADV7513_MAIN_ADDR,		// uint8_t chip
			init_config_array[i].addr,	// unsigned int addr
			1,				// int alen
			&init_config_array[i].value,	// uint8_t *buffer
			1				// int len
		);

		if(result != 0) {
			print_str = "writing I2C";
			printf("%s%s\n", ERROR_STR, print_str);
			setenv(HDMI_ERROR_ENV, print_str);
			print_str = "HDMI_milestones";
			snprintf_buffer_ptr = &snprintf_buffer[0];
			uitoa(milestones, &snprintf_buffer_ptr);
			setenv(print_str, snprintf_buffer);
			printf("%s = %s\n", print_str, snprintf_buffer);
			return(15);
		}
	}

	/* report configuration details to u-boot environment */
	setenv(HDMI_STATUS_ENV, "report configuration");
	milestones |= 0x01 << 19;

	print_str = "HDMI_vld_tmng_fnd";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(valid_timing_configuration_located, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_h_active_pix";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(horizontal_active_pixels, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_h_blank_pix";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(horizontal_blanking_pixels, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_h_sync_off";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(horizontal_sync_offset, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_h_sync_width";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(horizontal_sync_width, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_v_active_lin";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(vertical_active_lines, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_v_blank_lin";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(vertical_blanking_lines, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_v_sync_off";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(vertical_sync_offset, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_v_sync_width";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(vertical_sync_width, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_pll_freq";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(shared_struct.desired_frequency, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_pll_m";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(shared_struct.m_value, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_pll_c";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(shared_struct.c_value, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_pll_k";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(shared_struct.k_value, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_stride";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(horizontal_active_pixels * 4, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	print_str = "HDMI_milestones";
	snprintf_buffer_ptr = &snprintf_buffer[0];
	uitoa(milestones, &snprintf_buffer_ptr);
	setenv(print_str, snprintf_buffer);
	printf("%s = %s\n", print_str, snprintf_buffer);

	setenv(HDMI_STATUS_ENV, "complete");

	/* paint the frame buffer */
/*
	for(j = 0 ; j < vertical_active_lines ; j++) {
		for(i = 0 ; i < horizontal_active_pixels ; i++) {
			if(i < (horizontal_active_pixels / 3))
				video_ptr[i + (j * horizontal_active_pixels)] =
								0x00FF0000;
			else if(i < (2 * (horizontal_active_pixels / 3)))
				video_ptr[i + (j * horizontal_active_pixels)] =
								0x0000FF00;
			else
				video_ptr[i + (j * horizontal_active_pixels)] =
								0x000000FF;
		}
	}

	for(i = 0 ; i < horizontal_active_pixels ; i++) {
		video_ptr[i] = 0x00FFFFFF;
		video_ptr[i + (horizontal_active_pixels *
				(vertical_active_lines - 1))] = 0x00FFFFFF;
	}

	for(i = 0 ; i < vertical_active_lines ; i++) {
		video_ptr[i * horizontal_active_pixels] = 0x00FFFFFF;
		video_ptr[(horizontal_active_pixels - 1) +
				(i * horizontal_active_pixels)] =
							0x00FFFFFF;
	}
*/
	for(j = 0 ; j < vertical_active_lines ; j++) {
		for(i = 0 ; i < horizontal_active_pixels ; i++) {
			video_ptr[i + (j * horizontal_active_pixels)] =
								0x000071C5;
		}
	}

	print_str = "background painted";
	setenv(HDMI_INFO_ENV, print_str);

	bmp_header_ptr = (struct bmp_image_header *)BMP_IMAGE_BASE;

	if(bmp_header_ptr->bmp_signature != 0x4D42) {
		print_str = "bad BMP signature";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(16);
	}

	if(bmp_header_ptr->bmp_header_size != 124) {
		print_str = "bad BMP header size";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(17);
	}

	if(bmp_header_ptr->bmp_color_planes != 1) {
		print_str = "bad BMP color planes";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(18);
	}

	if(bmp_header_ptr->bmp_bits_per_pixel != 24) {
		print_str = "bad BMP bits per pixel";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(19);
	}

	if(bmp_header_ptr->bmp_compression_method != 0) {
		print_str = "bad BMP compression method";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(20);
	}

	if(bmp_header_ptr->bmp_width != 640) {
		print_str = "bad BMP image width";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(21);
	}

	if(bmp_header_ptr->bmp_height != 480) {
		print_str = "bad BMP image height";
		printf("%s%s\n", INFO_STR, print_str);
		setenv(HDMI_INFO_ENV, print_str);
		return(22);
	}

	bmp_pixel_ptr = (struct bmp_24_bit_pixel *)(BMP_IMAGE_BASE +
					bmp_header_ptr->bmp_bitmap_offset);

	for(j = bmp_header_ptr->bmp_height - 1 ; j >= 0 ; j--) {
		for(i = 0 ; i < bmp_header_ptr->bmp_width ; i++) {
			video_ptr[i + ((((bmp_header_ptr->bmp_height - 1) - j +
					((vertical_active_lines / 2) -
					(bmp_header_ptr->bmp_height / 2))) *
						horizontal_active_pixels)) +
					((horizontal_active_pixels / 2) -
					(bmp_header_ptr->bmp_width / 2)) ] =
				(bmp_pixel_ptr[i +
					(j * bmp_header_ptr->bmp_width)].red
									<< 0) |
				(bmp_pixel_ptr[i +
					(j * bmp_header_ptr->bmp_width)].green
									<< 8) |
				(bmp_pixel_ptr[i +
					(j * bmp_header_ptr->bmp_width)].blue
									<< 16);
		}
	}

	print_str = "startup BMP painted";
	setenv(HDMI_INFO_ENV, print_str);
	return (0);
}

/*
Original comment from pll_calc_float and pll_calc_fixed model test program. We
are only implementing the fixed  point version here.

Our goal here is to calculate the M, C and K values for the Altera fractional
PLL implemented very simply with one output.  The PLL input reference frequency
is multiplied by M.K to create an internal VCO frequency.  The M counter
represents the integer divide value and the K counter represents the fractional
divide value.  That VCO frequency is then divided by the C counter to produce
the desired output frequency.  The VCO frequency must be set to something
between 400MHz and 1300MHz. and we must choose a K value that is between 0.05
and 0.95.  A K value of 0.00 is also acceptable.

Our algorithm will begin by locating the minimum C divider that allows us to
set a VCO frequency above 400MHz.  From that C we derive the M.K value from the
product of C and the input and output clock ratio.  The M.K value must not
cause a VCO frequency above 1300MHz otherwise it is rejected.  The K value is
checked to be either 0.0 or fall between 0.05 and 0.95.  If all of these
conditions are not met, then we increment our C value by one, and we evaluate
the new M.K value until we find parameters that are all legal.  If we cannot
locate a set of valid parameters we increase the desired clock frequency by
10KHz and we re-run the algorithm.

We expect the desired output frequency of the PLL to be limited to a
maximum of MAX_INPUT_FREQ and a minimum of MIN_INPUT_FREQ.

The PLL reference input clock frequency is PLL_REF_FREQ.

The PLL VCO frequency must remain between maximum and minimum limits.
MAX_VCO = (MAX_MULTIPLE * PLL_REF_FREQ)
MIN_VCO = (MIN_MULTIPLE * PLL_REF_FREQ)

Here is some pseudo code of how the algorithm is implemented:

We would like to locate a starting C value so we start by computing the ratio
of the desired output clock frequency and the PLL reference input clock
frequency:

	clock_ratio = desired_frequency / PLL_REF_FREQ

Next we multiply the minimum multiplier by the clock ratio to get the minimum C
value that will not exceed a 400MHz VCO frequency:

	min_div_ratio = MIN_MULTIPLE / clock_ratio

Now this C value is likely a real number with some fractional quantity, but the
C counter must be an integer so we must ceil() this number to get the next
higher integer value:

	ceil_min_div_ratio = ceil(min_div_ratio)

Now we can calculate the M.K value that we would get with this C value:

	product_ratio_ceil = clock_ratio * ceil_min_div_ratio

Now we start to evaluate the K value by taking the floor of the M.K value which
is M:

	floor_product = (unsigned long)(product_ratio_ceil)

And we can extract the K value by computing the difference of M.K - M:

	fraction = product_ratio_ceil - floor_product

Now we can evaluate the value of K:
	if ((fraction >= 0.05) && (fraction <= 0.95))
		break;
	if (fraction == 0.0)
		break;

If K is not valid, then we increment the value of C and compute a new M.K value:

	ceil_min_div_ratio++
	product_ratio_ceil = clock_ratio * ceil_min_div_ratio

If we were unable to locate a valid set of M, C, and K, then we loop back
through the algorithm with a new desired frequency that is 10KHz greater than
the one we just failed to evaluate:

	if (product_ratio_ceil >= MAX_MULTIPLE)
		continue;
	else
		break;

NOTE: in this example below, we create a floating point implementation where the
values that we work with are real double precision floating point values.  We
also create a fixed point implementation below where the values that we work
with are scaled to a fixed point number where 1.0 is represented by
0x0000_0001_0000_0000 or 2**32, such that there is a 32-bit integer value and
a 32-bit fractional value.

One additional requirement beyond the values for M, C, and K, are the settings
bandwidth register and the CP register.  These appear to be set to 7 and 2
respectively, except for values of M that set the VCO into the range of 600MHz
to 700MHz.  In our case here, that would mean that M values of 12 ant 13 for
VCO frequencies of 600MHz and 650MHz would be affected.

        if((m_value == 12) || (m_value == 13)) {
                BW_reg = 8;
                CP_reg = 3;
        } else {
                BW_reg = 7;
                CP_reg = 2;
        }
*/

void pll_calc_fixed(struct pll_calc_struct *the_pll_calc_struct) {

	int i;
	unsigned long long input_freq;
	unsigned long long clock_ratio;
	unsigned long long min_div_ratio;
	unsigned long long ceil_min_div_ratio;
	unsigned long long product_ratio_ceil;
	unsigned long long floor_product;
	unsigned long long fraction;
	unsigned long long k_value;

	/*
	calculate the PLL reconfiguration register values

	There are some frequencies that we cannot fit into valid M, C and K
	values, so we setup a loop that will sweep through 1MHz in 10KHz
	increments.  Most dead zones are no greater than 100KHz, so we should be
	able to find a frequency reasonably close to the desired frequency.
	*/
	for (i = 0; i < 100; i++) {
		/*
		increment the desired frequency by 10KHz each pass through
		the loop since there are some frequencies that we cannot fit
		into the valid ranges for all parameters
		*/
		input_freq = the_pll_calc_struct->desired_frequency +
								(i * 10000);

		/* calculate a candidate C value */
		clock_ratio = (input_freq << 32) / PLL_REF_FREQ;
		if (clock_ratio == 0) {
			the_pll_calc_struct->desired_frequency = 0;
			the_pll_calc_struct->m_value = 1;
			return;
		}
		min_div_ratio = ((unsigned long long)MIN_MULTIPLE << 32) /
								clock_ratio;

		/* calculate ceil for the integer value of C */
		ceil_min_div_ratio = min_div_ratio;
		/*
		use rounding fuzz of 3 to align better with float point model
		*/
		if((((unsigned long long)MIN_MULTIPLE << 32) % clock_ratio) > 3)
			ceil_min_div_ratio += 1;

		/* calculate the M.K value based on our proposed C value */
		product_ratio_ceil = clock_ratio * ceil_min_div_ratio;

		/*
		evaluate K, if invalid, increment C and re-evaluate, continue
		incrementing C until M.K exceeds MAX_MULTIPLE
		*/
		fraction = 0;
		floor_product = 0;
		while(product_ratio_ceil <
			((unsigned long long)MAX_MULTIPLE << 32)) {
			/* extract M */
			floor_product = product_ratio_ceil &
				0xFFFFFFFF00000000ULL;

			/* extract K */
			fraction = product_ratio_ceil - floor_product;

			/* evaluate K */
			/*
			use rounding fuzz of 3 to align better with float point
			model
			*/
			if ((fraction >= (FXD_PNT_0P05 - 3)) &&
				(fraction <= (FXD_PNT_0P95 + 0)))
				break;
			/*
			use rounding fuzz of 4 to align better with float point
			model
			*/
			if (fraction <= 4) {
				fraction = 0;
				break;
			}
			/*
			use rounding fuzz of -4 to align better with float point
			model
			*/
			if (fraction >= 0xFFFFFFFC) {
				fraction = 0;
				floor_product += FXD_PNT_1P0;
				break;
			}

			/* increment C */
			ceil_min_div_ratio++;

			/* new M.K value */
			product_ratio_ceil = clock_ratio * ceil_min_div_ratio;
		}

		/* if we did not locate a valid M.K value, the try again */
		if (product_ratio_ceil >=
			((unsigned long long)MAX_MULTIPLE << 32))
			continue;
		else
			break;
	}

	/*
	this should never occur but if we were unable to locate a valid
	configuration, we return with an desired frequency of ZERO
	*/
	if (i >= 100) {
		the_pll_calc_struct->desired_frequency = 0;
		return;
	}

	/*
	if the K value is ZERO then we return 1
	otherwise we return K * 2^32
	*/
	if (fraction == 0)
		k_value = 1;
	else
		k_value = fraction;

	/* return all computed values */
	the_pll_calc_struct->desired_frequency = input_freq;
	the_pll_calc_struct->m_value = (unsigned long)(floor_product >> 32);
	the_pll_calc_struct->c_value = (unsigned long)ceil_min_div_ratio;
	the_pll_calc_struct->k_value = (unsigned long)k_value;

	return;
}

/* unsigned integer to ascii */
const char digits[] = "0123456789";
void uitoa(uint32_t uint32_input, char **output_str) {

	char *str_ptr;

	if(uint32_input > 9) {
		uitoa(uint32_input / 10, output_str);
		uint32_input -= (uint32_input / 10) * 10;
	}

	str_ptr = *output_str;
	*str_ptr++ = digits[uint32_input];
	*str_ptr = '\0';
	*output_str = str_ptr;
}

/*
create a hang symbol because the _div0 library will want to call it, our code
above should not execute division by zero
*/
void hang(void) {
	printf("HANG!!!\n");
	while(1);
}

