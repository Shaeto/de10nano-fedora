#ifdef CONFIG_DM_I2C
int hdmi_i2c_set_bus_num(unsigned int busnum);
int hdmi_i2c_get_cur_bus(struct udevice **busp);
int hdmi_i2c_get_cur_bus_chip(uint chip_addr, struct udevice **devp);
int hdmi_i2c_read(uint chip_addr, uint offset, int alen, uint8_t *buffer, int len);
int hdmi_i2c_write(uint chip_addr, uint offset, int alen, uint8_t *buffer, int len);
#endif
