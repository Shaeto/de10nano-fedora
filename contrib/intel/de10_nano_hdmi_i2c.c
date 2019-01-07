#ifdef CONFIG_DM_I2C
struct udevice *i2c_cur_bus = NULL;

int hdmi_i2c_set_bus_num(unsigned int busnum)
{
	struct udevice *bus;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus);
	if (ret) {
		printf("%s/%s: No bus %d\n", ERROR_STR, __func__, busnum);
		return ret;
	}
	i2c_cur_bus = bus;

	return 0;
}

int hdmi_i2c_get_cur_bus(struct udevice **busp)
{
#ifdef ADV7513_I2C_DEFAULT_BUS_NUMBER
	if (!i2c_cur_bus) {
		if (hdmi_i2c_set_bus_num(ADV7513_I2C_DEFAULT_BUS_NUMBER)) {
			printf("Default I2C bus %d not found\n",
			       ADV7513_I2C_DEFAULT_BUS_NUMBER);
			return -ENODEV;
		}
	}
#endif

	if (!i2c_cur_bus) {
		puts("No I2C bus selected\n");
		return -ENODEV;
	}
	*busp = i2c_cur_bus;

	return 0;
}

int hdmi_i2c_get_cur_bus_chip(uint chip_addr, struct udevice **devp)
{
	struct udevice *bus;
	int ret;

	ret = hdmi_i2c_get_cur_bus(&bus);
	if (ret)
		return ret;

	return i2c_get_chip(bus, chip_addr, 1, devp);
}

int hdmi_i2c_read(uint chip_addr, uint offset, int alen, uint8_t *buffer, int len)
{
    struct udevice *dev;

    int ret = hdmi_i2c_get_cur_bus_chip(chip_addr, &dev);
    if (ret)
	return ret;

    ret = i2c_set_chip_offset_len(dev, alen);
    if (ret)
	return ret;

    return dm_i2c_read(dev, offset, buffer, len);
}

int hdmi_i2c_write(uint chip_addr, uint offset, int alen, uint8_t *buffer, int len)
{
    struct udevice *dev;

    int ret = hdmi_i2c_get_cur_bus_chip(chip_addr, &dev);
    if (ret)
	return ret;

    ret = i2c_set_chip_offset_len(dev, alen);
    if (ret)
	return ret;

    return dm_i2c_write(dev, offset, buffer, len);
}
#endif
