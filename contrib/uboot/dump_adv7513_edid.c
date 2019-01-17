/*                                                                               
 * The MIT License (MIT)                                                        
 * Copyright (c) 2016 Intel Corporation                                         
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
#include "de10_nano_hdmi_i2c.h"

struct legacy_timing_mode {
	u_int columns;
	u_int rows;
	u_int rate;
	const char *description;
};

struct legacy_timing_mode legacy_support_array[24] = {
/* EDID byte 35 */
/* bit 7 */ { 720,  400, 70, "720x400 @ 70 Hz"},
/* bit 6 */ { 720,  400, 88, "720x400 @ 88 Hz"},
/* bit 5 */ { 640,  480, 60, "640x480 @ 60 Hz"},
/* bit 4 */ { 640,  480, 67, "640x480 @ 67 Hz"},
/* bit 3 */ { 640,  480, 72, "640x480 @ 72 Hz"},
/* bit 2 */ { 640,  480, 75, "640x480 @ 75 Hz"},
/* bit 1 */ { 800,  600, 56, "800x600 @ 56 Hz"},
/* bit 0 */ { 800,  600, 60, "800x600 @ 60 Hz"},
/* EDID byte 36 */
/* bit 7 */ { 800,  600, 72, "800x600 @ 72 Hz"},
/* bit 6 */ { 800,  600, 75, "800x600 @ 75 Hz"},
/* bit 5 */ { 832,  624, 75, "832x624 @ 75 Hz"},
/* bit 4 */ {1024,  768, 87, "1024x768 @ 87 Hz, interlaced (1024x768i)"},
/* bit 3 */ {1024,  768, 60, "1024x768 @ 60 Hz"},
/* bit 2 */ {1024,  768, 72, "1024x768 @ 72 Hz"},
/* bit 1 */ {1024,  768, 75, "1024x768 @ 75 Hz"},
/* bit 0 */ {1280, 1024, 75, "1280x1024 @ 75 Hz"},
/* EDID byte 38 */
/* bit 7 */ {1152,  870, 75, "1152x870 @ 75 Hz (Apple Macintosh II)"},
/* bit 6 */ {   0,    0,  0, "manufacturer specific 6"},
/* bit 5 */ {   0,    0,  0, "manufacturer specific 5"},
/* bit 4 */ {   0,    0,  0, "manufacturer specific 4"},
/* bit 3 */ {   0,    0,  0, "manufacturer specific 3"},
/* bit 2 */ {   0,    0,  0, "manufacturer specific 2"},
/* bit 1 */ {   0,    0,  0, "manufacturer specific 1"},
/* bit 0 */ {   0,    0,  0, "manufacturer specific 0"},
};

/* test program */
int dump_adv7513_edid(int argc, char * const argv[]) {

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

	char descriptor_text[14];
	uint8_t dtd_offset;
	uint8_t dtd_count;

	char manufacturer_letter[3];
	uint16_t manufacturer_product_code;
	uint32_t serial_number;
	uint8_t manufacture_week;
	uint8_t manufacture_year;
	uint8_t manufacture_version;
	uint8_t manufacture_version_dot;
	uint32_t legacy_bitmap;
	const char *aspect_str;

	uint16_t pixel_clock;
	uint16_t horizontal_active_pixels;
	uint16_t horizontal_blanking_pixels;
	uint16_t vertical_active_lines;
	uint16_t vertical_blanking_lines;
	uint16_t horizontal_sync_offset;
	uint16_t horizontal_sync_width;
	uint8_t vertical_sync_offset;
	uint8_t vertical_sync_width;
	uint8_t horizontal_border_pixels;
	uint8_t vertical_border_pixels;
	uint8_t interlaced;

	/* initialize u-boot application environment */
	app_startup(argv);

	if (argc >= 2 && argv[1] && argv[1][0]) {
	    char *endp;
	    unsigned int mybus = ustrtoul(argv[1], &endp, 10);
	    hdmi_i2c_set_bus_num(mybus);
	}

	/* since we cannot read a single register from the ADV7513 with the
	 * default I2C driver, we just read the first N registers starting
	 * from ZERO and reaching up to the register we want
	 */
	print_str = "read ADV7513 chip ID";
	printf("%s%s\n", INFO_STR, print_str);
	result = hdmi_i2c_read(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			0x00,			// unsigned int addr
			0,			// int alen
			adv7513_read_buffer,	// uint8_t *buffer
			ADV7513_CHIP_ID_LO + 1	// int len
		);

	if(result != 0) {
		print_str = "reading I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		return(1);
	}

	/* verify the chip id for the ADV7513 */
	if(
		(adv7513_read_buffer[ADV7513_CHIP_ID_HI] !=
			ADV7513_CHIP_ID_HI_VAL) ||
		(adv7513_read_buffer[ADV7513_CHIP_ID_LO] !=
			ADV7513_CHIP_ID_LO_VAL) ) {

		print_str = "Bad Chip ID";
		printf("%s%s\n", ERROR_STR, print_str);
		return(2);
	}

	/* verify that we see a monitor attached to the HDMI connector */
	if((adv7513_read_buffer[ADV7513_HPD_MNSNS] & ADV7513_HPD_MNSNS_BITS) !=
		ADV7513_HPD_MNSNS_BITS) {
		
		print_str = "No HDMI display detected";
		printf("%s%s\n", ERROR_STR, print_str);
		return(3);
	}

	/* power down the ADV7513 */
	print_str = "power down ADV7513";
	printf("%s%s\n", INFO_STR, print_str);
	adv7513_write_val = adv7513_read_buffer[ADV7513_PWR_DWN];
	adv7513_write_val |= ADV7513_PWR_DWN_BIT;
	result = hdmi_i2c_write(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			ADV7513_PWR_DWN,	// unsigned int addr
			1,			// int alen
			&adv7513_write_val,	// uint8_t *buffer
			1			// int len
		);

	if(result != 0) {
		print_str = "writing I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		return(4);
	}

	/* power up the ADV7513 */
	print_str = "power up ADV7513";
	printf("%s%s\n", INFO_STR, print_str);
	adv7513_write_val &= ~ADV7513_PWR_DWN_BIT;
	result = hdmi_i2c_write(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			ADV7513_PWR_DWN,	// unsigned int addr
			1,			// int alen
			&adv7513_write_val,	// uint8_t *buffer
			1			// int len
		);

	if(result != 0) {
		print_str = "writing I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		return(5);
	}

	/* wait for the EDID data to become ready */
	print_str = "wait EDID ready";
	printf("%s%s\n", INFO_STR, print_str);
	for(i = 0 ; i < 1000 ; i++) {
		result = hdmi_i2c_read(
				ADV7513_MAIN_ADDR,	// uint8_t chip
				0x00,			// unsigned int addr
				0,			// int alen
				adv7513_read_buffer,	// uint8_t *buffer
				ADV7513_EDID_RDY + 1	// int len
			);

		if(result != 0) {
			print_str = "reading I2C";
			printf("%s%s\n", ERROR_STR, print_str);
			return(6);
		}
		
		if((adv7513_read_buffer[ADV7513_EDID_RDY] &
			ADV7513_EDID_RDY_BIT) == ADV7513_EDID_RDY_BIT)
			break;
	}
	
	if(i >= 1000) {
		print_str = "EDID timeout";
		printf("%s%s\n", ERROR_STR, print_str);
		return(7);
	}

	/* read the EDID data */
	print_str = "read EDID data";
	printf("%s%s\n", INFO_STR, print_str);
	result = hdmi_i2c_read(
			ADV7513_EDID_ADDR,	// uint8_t chip
			0x00,			// unsigned int addr
			0,			// int alen
			adv7513_edid_buffer,	// uint8_t *buffer
			256			// int len
		);

	if(result != 0) {
		print_str = "reading I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		return(8);
	}

	/* print the EDID data */
	printf("\nRaw EDID Data\n");
	for(i = 0 ; i < 256 ; i++) {
		printf("0x%02X ", adv7513_edid_buffer[i]);
		if(((i + 1) % 16) == 0)
			printf("\n");
	}

	/* verify EDID block 0 checksum */
	checksum = 0;
	for(i = 0 ; i < 128 ; i++)
		checksum += adv7513_edid_buffer[i];

	if(checksum != 0) {
		print_str = "EDID block 0 checksum";
		printf("%s%s\n", ERROR_STR, print_str);
		return(9);
	}

	/* verify block 0 header pattern */
	for(i = 0 ; i < 8 ; i++) {
		if(edid_header_pattern_array[i] != adv7513_edid_buffer[i]) {
			print_str = "EDID header pattern";
			printf("%s%s\n", ERROR_STR, print_str);
			return(10);
		}
	}

	/* extract three letter manufacturer ID */
	if((adv7513_edid_buffer[8] & 0x80) != 0x00) {
		print_str = "manufacturer letter format";
		printf("%s%s\n", ERROR_STR, print_str);
		return(1);
	}

	manufacturer_letter[0] = adv7513_edid_buffer[8];
	manufacturer_letter[0] >>= 2;
	manufacturer_letter[0] -= 1;
	manufacturer_letter[0] += 'A';

	manufacturer_letter[1] = adv7513_edid_buffer[8];
	manufacturer_letter[1] &= 0x03;
	manufacturer_letter[1] <<= 3;
	manufacturer_letter[1] |= (adv7513_edid_buffer[9] >> 5) & 0x07;
	manufacturer_letter[1] -= 1;
	manufacturer_letter[1] += 'A';

	manufacturer_letter[2] = adv7513_edid_buffer[9];
	manufacturer_letter[2] &= 0x1F;
	manufacturer_letter[2] -= 1;
	manufacturer_letter[2] += 'A';

	printf("\nManufacturer ID: %c%c%c\n",
			manufacturer_letter[0],
			manufacturer_letter[1],
			manufacturer_letter[2]);

	/* extract manufacturer product code */
	manufacturer_product_code = adv7513_edid_buffer[10] |
					(adv7513_edid_buffer[11] << 8);

	printf("Manufacturer Product Code: 0x%04X\n",
			manufacturer_product_code);

	/* extract serial number */
	serial_number = adv7513_edid_buffer[12] |
		(adv7513_edid_buffer[13] << 8) |
		(adv7513_edid_buffer[14] << 16) |
		(adv7513_edid_buffer[15] << 24);

	printf("Serial Number: 0x%08X\n", serial_number);

	/* extract manufacture week */
	manufacture_week = adv7513_edid_buffer[16];

	printf("Manufacture Week: %u\n", manufacture_week);

	/* extract manufacture year */
	manufacture_year = adv7513_edid_buffer[17];

	printf("Manufacture Year: %u\n", manufacture_year + 1990);

	/* extract EDID version */
	manufacture_version = adv7513_edid_buffer[18];
	manufacture_version_dot = adv7513_edid_buffer[19];

	printf("Manufacture Version: %c.%c\n",
			manufacture_version + '0',
			manufacture_version_dot + '0');

	/* decode supported legacy timings */
	legacy_bitmap = (adv7513_edid_buffer[35] << 16) |
			(adv7513_edid_buffer[36] <<  8) |
			(adv7513_edid_buffer[37] <<  0);
	
	printf("\nSupported legacy timing modes:\n");
	for(i = 0 ; i < 24 ; i++) {
		if(legacy_bitmap & (1 << (23 - i)))
			printf("%s\n", legacy_support_array[i].description);
	}

	/* decode standard display modes */
	printf("\nStandard Display Modes\n");

	for(i = 0 ; i < 8 ; i++) {
		if(
			(adv7513_edid_buffer[38 + (i * 2)] == 0x01) &&
			(adv7513_edid_buffer[39 + (i * 2)] == 0x01)
		  ) {
			printf("Entry %d: unused\n", i);
		} else {
			switch ((adv7513_edid_buffer[39 + (i * 2)] >> 6) &
									0x3 ) {
				case 0x0:
					aspect_str = "16:10";
					break;
				case 0x1:
					aspect_str = "4:3";
					break;
				case 0x2:
					aspect_str = "5:4";
					break;
				case 0x3:
					aspect_str = "16:9";
					break;
				default:
					aspect_str = "unknown";
					break;
			}

			printf("Entry %d: %d pix | %s | %d Hz\n",
				i,
				(adv7513_edid_buffer[38 + (i * 2)] + 31) * 8,
				aspect_str,
				(adv7513_edid_buffer[39 + (i * 2)] & 0x3F) +
									60);
		}
	}

	/* decode descriptor blocks */
	for(i = 0 ; i < 4 ; i++) {
		printf("\nDESCRIPTOR %d\n", i + 1);

		descriptor_base = &adv7513_edid_buffer[54];
		descriptor_base += i * 18;
		if(
			(descriptor_base[0] == 0) &&
			(descriptor_base[1] == 0) &&
			(descriptor_base[2] == 0) &&
			(descriptor_base[4] == 0)
		  ) {
			if(descriptor_base[3] == 0xFF) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Monitor Serial Number: %s\n",
						descriptor_text);
			}
			
			if(descriptor_base[3] == 0xFE) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Unspecified Text: %s\n",
						descriptor_text);
			}
			
			if(descriptor_base[3] == 0xFD)
				printf("Monitor Range Limits\n");
			
			if(descriptor_base[3] == 0xFC) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Monitor Name: %s\n", descriptor_text);
			}
			
			if(descriptor_base[3] == 0xFB)
				printf("Additional White Point Data\n");
			
			if(descriptor_base[3] == 0xFA)
				printf("Additional Standard Timing IDs\n");
		} else {
			printf("Detailed Timing Descriptor\n");
		
			pixel_clock =
				(descriptor_base[1] << 8) |
				descriptor_base[0];
			printf("Pixel Clock: %u * 10KHz\n", pixel_clock);
		
			horizontal_active_pixels =
				(((descriptor_base[4] >> 4) & 0x0F) << 8) |
				descriptor_base[2];
			printf("Horzontal Active Pixels: %u\n",
					horizontal_active_pixels);

			horizontal_blanking_pixels =
				((descriptor_base[4] & 0x0F) << 8) |
				descriptor_base[3];
			printf("Horizontal Blanking Pixels: %u\n",
					horizontal_blanking_pixels);

			vertical_active_lines =
				(((descriptor_base[7] >> 4) & 0x0F) << 8) |
				descriptor_base[5];
			printf("Vertical Active Lines: %u\n",
					vertical_active_lines);

			vertical_blanking_lines =
				((descriptor_base[7] & 0x0F) << 8) |
				descriptor_base[6];
			printf("Vertical Blanking Lines: %u\n",
					vertical_blanking_lines);
	
			horizontal_sync_offset =
				(((descriptor_base[11] >> 6) & 0x03) << 8) |
				descriptor_base[8];
			printf("Horizontal Sync Offset: %u\n",
					horizontal_sync_offset);

			horizontal_sync_width =
				(((descriptor_base[11] >> 4) & 0x03) << 8) |
				descriptor_base[9];
			printf("Horizontal Sync Width: %u\n",
					horizontal_sync_width);
	
			vertical_sync_offset =
				(((descriptor_base[11] >> 2) & 0x03) << 4) |
				((descriptor_base[10] >> 4) & 0x0F);
			printf("Vertical Sync Offset: %u\n",
					vertical_sync_offset);

			vertical_sync_width = 
				((descriptor_base[11] & 0x03) << 4) |
				(descriptor_base[10] & 0x0F);
			printf("Vertical Sync Width: %u\n",
					vertical_sync_width);

			horizontal_border_pixels = descriptor_base[15];
			printf("Horizontal Border Pixels: %u\n",
					horizontal_border_pixels);

			vertical_border_pixels = descriptor_base[16];
			printf("Vertical Border Pixels: %u\n",
					vertical_border_pixels);

			interlaced = (descriptor_base[17] & 0x80) ? 1 : 0;
			printf("Interlaced: %u\n", interlaced);
		}
	}
	
	/* check for extension blocks */
	printf("\nNumber of extension blocks: %u\n", adv7513_edid_buffer[126]);
	if(adv7513_edid_buffer[126] == 0)
		goto end_program;

	/* verify extension block checksum */
	checksum = 0;
	for(i = 0 ; i < 128 ; i++)
		checksum += adv7513_edid_buffer[128 + i];

	if(checksum != 0) {
		print_str = "extension block 1 checksum";
		printf("%s%s\n", ERROR_STR, print_str);
		return(2);
	}

	printf("\nEDID checksum block 1 is valid.\n");
			
	/* verify extension tag */
	if(adv7513_edid_buffer[128] != 0x02) {
		print_str = "extension tag";
		printf("%s%s\n", ERROR_STR, print_str);
		return(3);
	}

	printf("Extension Tag is valid.\n");

	/* verify revision number */
	if(adv7513_edid_buffer[129] != 0x03) {
		print_str = "extension revision number";
		printf("%s%s\n", ERROR_STR, print_str);
		return(4);
	}

	printf("Extension revision is 3.\n");

	/* checck for DTDs in extension block */
	dtd_offset = adv7513_edid_buffer[130];
	dtd_count = adv7513_edid_buffer[131] & 0x0F;
	if(
		(dtd_offset == 0x00) ||
		(dtd_count == 0x00)
	  ) {
		printf("No DTDs present in extension block.\n");
		goto end_program;
	}

	/* process descriptors in extension block */
	descriptor_base = &adv7513_edid_buffer[128];
	descriptor_base += dtd_offset;
	for(i = 0 ; i < dtd_count ; i++) {
		printf("\nDESCRIPTOR %d\n", i + 1);

		if(
			(descriptor_base[0] == 0) &&
			(descriptor_base[1] == 0) &&
			(descriptor_base[2] == 0) &&
			(descriptor_base[4] == 0)
		  ) {
			if(descriptor_base[3] == 0xFF) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Monitor Serial Number: %s\n",
						descriptor_text);
			} else if(descriptor_base[3] == 0xFE) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Unspecified Text: %s\n",
						descriptor_text);
			} else if(descriptor_base[3] == 0xFD) {
				printf("Monitor Range Limits\n");
			} else if(descriptor_base[3] == 0xFC) {
				memset(descriptor_text, '\0', 14);
				for(j = 0; j < 13 ; j++) {
					if(
					(descriptor_base[5 + j] >= 32) &&
					(descriptor_base[5 + j] <= 126)
					) {
						descriptor_text[j] =
							descriptor_base[5 + j];
					} else {
						break;
					}
				}
				printf("Monitor Name: %s\n", descriptor_text);
			} else if(descriptor_base[3] == 0xFB) {
				printf("Additional White Point Data\n");
			} else if(descriptor_base[3] == 0xFA) {
				printf("Additional Standard Timing IDs\n");
			} else {
				printf("Unknown Format\n");
			}
		} else {
			printf("Detailed Timing Descriptor\n");
		
			pixel_clock =
				(descriptor_base[1] << 8) |
				descriptor_base[0];
			printf("Pixel Clock: %u * 10KHz\n", pixel_clock);
		
			horizontal_active_pixels =
				(((descriptor_base[4] >> 4) & 0x0F) << 8) |
				descriptor_base[2];
			printf("Horzontal Active Pixels: %u\n",
					horizontal_active_pixels);

			horizontal_blanking_pixels =
				((descriptor_base[4] & 0x0F) << 8) |
				descriptor_base[3];
			printf("Horizontal Blanking Pixels: %u\n",
					horizontal_blanking_pixels);

			vertical_active_lines =
				(((descriptor_base[7] >> 4) & 0x0F) << 8) |
				descriptor_base[5];
			printf("Vertical Active Lines: %u\n",
					vertical_active_lines);

			vertical_blanking_lines =
				((descriptor_base[7] & 0x0F) << 8) |
				descriptor_base[6];
			printf("Vertical Blanking Lines: %u\n",
					vertical_blanking_lines);
	
			horizontal_sync_offset =
				(((descriptor_base[11] >> 6) & 0x03) << 8) |
				descriptor_base[8];
			printf("Horizontal Sync Offset: %u\n",
					horizontal_sync_offset);

			horizontal_sync_width =
				(((descriptor_base[11] >> 4) & 0x03) << 8) |
				descriptor_base[9];
			printf("Horizontal Sync Width: %u\n",
					horizontal_sync_width);
	
			vertical_sync_offset =
				(((descriptor_base[11] >> 2) & 0x03) << 4) |
				((descriptor_base[10] >> 4) & 0x0F);
			printf("Vertical Sync Offset: %u\n",
					vertical_sync_offset);

			vertical_sync_width = 
				((descriptor_base[11] & 0x03) << 4) |
				(descriptor_base[10] & 0x0F);
			printf("Vertical Sync Width: %u\n",
					vertical_sync_width);

			horizontal_border_pixels = descriptor_base[15];
			printf("Horizontal Border Pixels: %u\n",
					horizontal_border_pixels);

			vertical_border_pixels = descriptor_base[16];
			printf("Vertical Border Pixels: %u\n",
					vertical_border_pixels);

			interlaced = (descriptor_base[17] & 0x80) ? 1 : 0;
			printf("Interlaced: %u\n", interlaced);
		}
		descriptor_base += 18;
	}

end_program:
	return(0);
}

#include "de10_nano_hdmi_i2c.c"
