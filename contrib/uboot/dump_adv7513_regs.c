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

struct adv7513_regs {
	u_char addr;
	u_char value;
	char *description;
};

struct adv7513_regs interest_array[] = {
	{ADV7513_CHIP_ID_HI, ADV7513_CHIP_ID_HI_VAL, "chip ID hi value"},
	{ADV7513_CHIP_ID_LO, ADV7513_CHIP_ID_LO_VAL, "chip ID lo value"},
	{ADV7513_HPD_MNSNS, ADV7513_HPD_MNSNS_BITS, "HPD and monitor sense bits"},
	{ADV7513_PWR_DWN, ADV7513_PWR_DWN_BIT, "power down bit"},
	{ADV7513_EDID_RDY, ADV7513_EDID_RDY_BIT, "EDID ready bit"},
	{ADV7513_HPD_CNTL, ADV7513_HPD_CNTL_BITS, "force HPD bit"},
};

struct adv7513_regs write_array[] = {
	{0x98, 0x03, "must be set to this"},
	{0x9A, 0xE0, "must be set to this"},
	{0x9C, 0x30, "must be set to this"},
	{0x9D, 0x61, "must be set to this"},
	{0xA2, 0xA4, "must be set to this"},
	{0xA3, 0xA4, "must be set to this"},
	{0xE0, 0xD0, "must be set to this"},
	{0xF9, 0x00, "must be set to this"},
	{0x16, 0x30, "8-bit color depth"},
	{0x17, 0x02, "aspect ratio 16:9 or 4:3"},
	{0xAF, 0x06, "HDMI mode, no HDCP"},
	{0x0C, 0x00, "disable I2S inputs"},
	{0x96, 0xF6, "clear all interrupts"},
};


/* test program */
int dump_adv7513_regs(int argc, char * const argv[]) {

	int i;
	int result;
	char *print_str;
	uint8_t adv7513_read_buffer[256];

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
	result = hdmi_i2c_read(
			ADV7513_MAIN_ADDR,	// uint8_t chip
			0x00,			// unsigned int addr
			0,			// int alen
			adv7513_read_buffer,	// uint8_t *buffer
			256			// int len
		);

	if(result != 0) {
		print_str = "reading I2C";
		printf("%s%s\n", ERROR_STR, print_str);
		return(1);
	}

	/* print the ADV7513 register data */
	printf("\nRaw ADV7513 Register Data\n");
	for(i = 0 ; i < 256 ; i++) {
		printf("0x%02X ", adv7513_read_buffer[i]);
		if(((i + 1) % 16) == 0)
			printf("\n");
	}

	printf("\n\nRegisters of interest.\n\n");
	printf("%s\t%s\t%s\t%s\n",
		"ADDR",
		"VALUE",
		"BITS",
		"DESCRIPTION");
	for(i = 0 ; 
	i < (int)(sizeof(interest_array) / sizeof(struct adv7513_regs)) ;
	i++)
		printf("0x%02X\t0x%02X\t0x%02X\t%s\n",
			interest_array[i].addr,
			adv7513_read_buffer[interest_array[i].addr],
			interest_array[i].value,
			interest_array[i].description);

	printf("\n\nRegisters we write.\n\n");
	printf("%s\t%s\t%s\t%s\n",
		"ADDR",
		"VALUE",
		"BITS",
		"DESCRIPTION");
	for(i = 0 ; 
	i < (int)(sizeof(write_array) / sizeof(struct adv7513_regs)) ;
	i++)
		printf("0x%02X\t0x%02X\t0x%02X\t%s\n",
			write_array[i].addr,
			adv7513_read_buffer[write_array[i].addr],
			write_array[i].value,
			write_array[i].description);

	return (0);
}

#include "de10_nano_hdmi_i2c.c"
