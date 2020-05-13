/*
 *  max17050_fuelgauge.c
 *  Samsung MAX17050 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/battery/sec_fuelgauge.h>

#ifdef CONFIG_FUELGAUGE_MAX17050_COULOMB_COUNTING
static int fg_i2c_read(struct i2c_client *client,
				u8 reg, u8 *data, u8 length)
{
	s32 value;

	value = i2c_smbus_read_i2c_block_data(client, reg, length, data);
	if (value < 0 || value != length) {
		dev_err(&client->dev, "%s: Error(%d)\n",
			__func__, value);
		return -1;
	}

	return 0;
}

static int fg_i2c_write(struct i2c_client *client,
				u8 reg, u8 *data, u8 length)
{
	s32 value;

	value = i2c_smbus_write_i2c_block_data(client, reg, length, data);
	if (value < 0) {
		dev_err(&client->dev, "%s: Error(%d)\n",
			__func__, value);
		return -1;
	}

	return 0;
}

static int fg_read_register(struct i2c_client *client,
				u8 addr)
{
	u8 data[2];

	if (fg_i2c_read(client, addr, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read addr(0x%x)\n",
			__func__, addr);
		return -1;
	}

	return (data[1] << 8) | data[0];
}

static int fg_write_register(struct i2c_client *client,
				u8 addr, u16 w_data)
{
	u8 data[2];

	data[0] = w_data & 0xFF;
	data[1] = w_data >> 8;

	if (fg_i2c_write(client, addr, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to write addr(0x%x)\n",
			__func__, addr);
		return -1;
	}

	return 0;
}

#if 0
static int fg_read_16register(struct i2c_client *client,
				u8 addr, u16 *r_data)
{
	u8 data[32];
	int i = 0;

	if (fg_i2c_read(client, addr, data, 32) < 0) {
		dev_err(&client->dev, "%s: Failed to read addr(0x%x)\n",
			__func__, addr);
		return -1;
	}

	for (i = 0; i < 16; i++)
		r_data[i] = (data[2 * i + 1] << 8) | data[2 * i];

	return 0;
}
#endif

static void fg_write_and_verify_register(struct i2c_client *client,
				u8 addr, u16 w_data)
{
	u16 r_data;
	u8 retry_cnt = 2;

	while (retry_cnt) {
		fg_write_register(client, addr, w_data);
		r_data = fg_read_register(client, addr);

		if (r_data != w_data) {
			dev_err(&client->dev,
				"%s: verification failed (addr: 0x%x, w_data: 0x%x, r_data: 0x%x)\n",
				__func__, addr, w_data, r_data);
			retry_cnt--;
		} else
			break;
	}
}

static void fg_test_print(struct i2c_client *client)
{
	u8 data[2];
	u32 average_vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;
	u16 reg_data;

	if (fg_i2c_read(client, AVR_VCELL_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read VCELL\n", __func__);
		return;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	average_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	average_vcell += (temp2 << 4);

	dev_info(&client->dev, "%s: AVG_VCELL(%d), data(0x%04x)\n", __func__,
		average_vcell, (data[1]<<8) | data[0]);

	reg_data = fg_read_register(client, FULLCAP_REG);
	dev_info(&client->dev, "%s: FULLCAP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = fg_read_register(client, REMCAP_REP_REG);
	dev_info(&client->dev, "%s: REMCAP_REP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = fg_read_register(client, REMCAP_MIX_REG);
	dev_info(&client->dev, "%s: REMCAP_MIX(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = fg_read_register(client, REMCAP_AV_REG);
	dev_info(&client->dev, "%s: REMCAP_AV(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);
}

static void fg_periodic_read(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 reg;
	int i;
	int data[0x10];
	char *str = NULL;

	str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
	if (!str)
		return;
#if defined(CONFIG_KLIMT) || defined(CONFIG_CHAGALL) || defined(CONFIG_N1A)
	if((fg_read_register(client, 0x12) != fuelgauge->pdata->QRTable00)) {
		if (fg_write_register(client, 0x12, (u16)fuelgauge->pdata->QRTable00) < 0) {
			dev_err(&client->dev, "%s: Failed to write QRtable0 \n",
				__func__);
				}
		}
	if((fg_read_register(client, 0x22) != fuelgauge->pdata->QRTable10)) {
		if (fg_write_register(client, 0x22, (u16)fuelgauge->pdata->QRTable10) < 0) {
			dev_err(&client->dev, "%s: Failed to write QRtable10 \n",
				__func__);
				}
		}
	if((fg_read_register(client, 0x32) != fuelgauge->pdata->QRTable20)) {
		if (fg_write_register(client, 0x32, (u16)fuelgauge->pdata->QRTable20) < 0) {
			dev_err(&client->dev, "%s: Failed to write QRtable20 \n",
				__func__);
				}
		}
	if((fg_read_register(client, 0x42) != fuelgauge->pdata->QRTable30)) {
		if (fg_write_register(client, 0x42, (u16)fuelgauge->pdata->QRTable30) < 0) {
			dev_err(&client->dev, "%s: Failed to write QRtable30 \n",
				__func__);
				}
		}
#endif
	for (i = 0; i < 16; i++) {
		for (reg = 0; reg < 0x10; reg++)
			data[reg] = fg_read_register(client, reg + i * 0x10);

		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (i == 4)
			i = 13;
	}

	dev_info(&client->dev, "%s", str);

	kfree(str);
}

static void fg_read_regs(struct i2c_client *client, char *str)
{
	int data = 0;
	u32 addr = 0;

	for (addr = 0; addr <= 0x4f; addr++) {
		data = fg_read_register(client, addr);
		sprintf(str+strlen(str), "0x%04x, ", data);
	}

	/* "#" considered as new line in application */
	sprintf(str+strlen(str), "#");

	for (addr = 0xe0; addr <= 0xff; addr++) {
		data = fg_read_register(client, addr);
		sprintf(str+strlen(str), "0x%04x, ", data);
	}
}

static int fg_read_vcell(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];
	u32 vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;
	static int count = 0;

	if (fg_i2c_read(client, VCELL_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
#if !defined(CONFIG_N1A)
	vcell += (temp2 << 4);
#else
    vcell += (temp2 << 4);
/*N1A read V over for ~ 330 mV*/
    if (vcell > 4500)
        vcell -= 300;
    else if (vcell > 4200)
        vcell -= 192;
    else if (vcell > 4005)
        vcell -= 150;
    else if (vcell > 3855)
        vcell -= 120;
    else if (vcell >3735)
        vcell -= 80;
    else if (vcell > 3655)
        vcell -= 50;

#endif
	if (!(count++ % PRINT_COUNT)) {
		dev_dbg(&client->dev, "%s: VCELL(%d), data(0x%04x)\n",
			__func__, vcell, (data[1]<<8) | data[0]);
		count = 1;
	}

	return vcell;
}

static int fg_read_vfocv(struct i2c_client *client)
{
	u8 data[2];
	u32 vfocv = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (fg_i2c_read(client, VFOCV_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read VFOCV\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;

	vfocv += (temp2 << 4);

	return vfocv;
}

static int fg_read_avg_vcell(struct i2c_client *client)
{
	u8 data[2];
	u32 avg_vcell = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (fg_i2c_read(client, AVR_VCELL_REG, data, 2) < 0) {
		dev_err(&client->dev,
			"%s: Failed to read AVG_VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	avg_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
#if !defined(CONFIG_N1A)
	avg_vcell += (temp2 << 4);
#else
    avg_vcell += (temp2 << 4);
    if (avg_vcell > 4500)
        avg_vcell -= 300;
    else if (avg_vcell > 4200)
        avg_vcell -= 192;
    else if (avg_vcell > 4005)
        avg_vcell -= 150;
    else if (avg_vcell > 3855)
        avg_vcell -= 120;
    else if (avg_vcell >3735)
        avg_vcell -= 80;
    else if (avg_vcell > 3655)
        avg_vcell -= 50;
#endif

	return avg_vcell;
}

/***** No write when  (fuelgauge->pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_FG)*/
static int fg_write_temp(struct i2c_client *client, int temperature)
{
	u8 data[2];

	data[0] = (temperature%10) * 1000 / 39;
	data[1] = temperature / 10;
	fg_i2c_write(client, TEMPERATURE_REG, data, 2);

	dev_dbg(&client->dev, "%s: temperature to (%d, 0x%02x%02x)\n",
		__func__, temperature, data[1], data[0]);

	return temperature;
}

static int fg_adjust_temp (struct i2c_client *client, enum power_supply_property psp, int value)
{
	int temp = 0;
	int temp_adc;
	int low = 0;
	int high = 0;
	int mid = 0;
	static int count = 0;
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	const sec_bat_adc_table_data_t *temp_adc_table;
	unsigned int temp_adc_table_size;

	temp_adc = value;

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		if (fuelgauge->pdata->temp_adc_table) {
			temp_adc_table = fuelgauge->pdata->temp_adc_table;
			temp_adc_table_size = fuelgauge->pdata->temp_adc_table_size;
		} else {
			return temp_adc;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		if (fuelgauge->pdata->temp_amb_adc_table) {
			temp_adc_table = fuelgauge->pdata->temp_amb_adc_table;
			temp_adc_table_size =
				fuelgauge->pdata->temp_amb_adc_table_size;
		} else {
			return temp_adc;
		}
		break;
	default:
		return temp_adc;
	}

	if (temp_adc_table[0].adc <= temp_adc) {
		temp = temp_adc_table[0].temperature;
		goto finish;
	} else if (temp_adc_table[temp_adc_table_size-1].adc >= temp_adc) {
		temp = temp_adc_table[temp_adc_table_size-1].temperature;
		goto finish;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].adc > temp_adc)
			low = mid + 1;
		else if (temp_adc_table[mid].adc < temp_adc)
			high = mid - 1;
		else {
			temp = temp_adc_table[mid].temperature;
			goto finish;
		}
	}

	temp = temp_adc_table[high].temperature;
	temp +=
		((temp_adc_table[low].temperature -
		temp_adc_table[high].temperature) *
		(temp_adc - temp_adc_table[high].adc)) /
		(temp_adc_table[low].adc - temp_adc_table[high].adc);

finish:
	if (!(count++ % PRINT_COUNT)) {
		dev_dbg(&client->dev,
			"%s: Temp_org(%d) -> Temp_adj(%d)\n",
			__func__, temp_adc, temp);
		count = 1;
	}
	return temp;
}

static int fg_read_temp(struct i2c_client *client)
{
#if 0
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int i;
#endif
	u8 data[2] = {0, 0};
    u8 data1[2] = {0, 0};
	int temper = 0;
	static int count = 0;

	if (fg_i2c_write(client, STATUS_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to write addr(0x%x)\n",
			__func__, STATUS_REG);
		return -1;
	}
	msleep(250);
	if (fg_i2c_read(client, STATUS_REG, data, 2) < 0) {
		dev_err(&client->dev,"%s: Failed to read STATUS_REG\n", __func__);
	}
/**************************************************/		
    dev_dbg(&client->dev, "%s: STATUS_REG(%d), data(0x%04x)\n",
        __func__, STATUS_REG, (data1[1]<<8) | data1[0]);
    fg_i2c_read(client, DESIGNCAP_REG, data1, 2);
    dev_dbg(&client->dev, "%s: DESIGNCAP_REG(%d), data(0x%04x)\n",
        __func__, DESIGNCAP_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, FULLCAP_REG, data1, 2);
    dev_dbg(&client->dev, "%s: FULLCAP_REG(%d), data(0x%04x)\n",
        __func__, FULLCAP_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, FULLCAP_NOM_REG, data1, 2);
    dev_dbg(&client->dev, "%s: FULLCAP_NOM_REG(%d), data(0x%04x)\n",
        __func__, FULLCAP_NOM_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, REMCAP_REP_REG, data1, 2);
    dev_dbg(&client->dev, "%s: REMCAP_REP_REG(%d), data(0x%04x)\n",
        __func__, REMCAP_REP_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, REMCAP_MIX_REG, data1, 2);
    dev_dbg(&client->dev, "%s: REMCAP_MIX_REG(%d), data(0x%04x)\n",
        __func__, REMCAP_MIX_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, REMCAP_AV_REG, data1, 2);
    dev_dbg(&client->dev, "%s: REMCAP_AV_REG(%d), data(0x%04x)\n",
        __func__, REMCAP_AV_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, VCELL_REG, data1, 2);
    dev_dbg(&client->dev, "%s: VCELL_REG(%d), data(0x%04x)\n",
        __func__, VCELL_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, AVR_VCELL_REG, data1, 2);
    dev_dbg(&client->dev, "%s: AVR_VCELL_REG(%d), data(0x%04x)\n",
        __func__, AVR_VCELL_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, VFOCV_REG, data1, 2);
    dev_dbg(&client->dev, "%s: VFOCV_REG(%d), data(0x%04x)\n",
        __func__, VFOCV_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, SOCREP_REG, data1, 2);
    dev_dbg(&client->dev, "%s: SOCREP_REG(%d), data(0x%04x)\n",
        __func__, SOCREP_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, SOCMIX_REG, data1, 2);
    dev_dbg(&client->dev, "%s: SOCMIX_REG(%d), data(0x%04x)\n",
        __func__, SOCMIX_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, SOCAV_REG, data1, 2);
    dev_dbg(&client->dev, "%s: SOCAV_REG(%d), data(0x%04x)\n",
        __func__, SOCAV_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, VFSOC_REG, data1, 2);
    dev_dbg(&client->dev, "%s: VFSOC_REG(%d), data(0x%04x)\n",
        __func__, VFSOC_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, AVR_TEMPERATURE_REG, data1, 2);
    dev_dbg(&client->dev, "%s: AVR_TEMPERATURE_REG(%d), data(0x%04x)\n",
        __func__, AVR_TEMPERATURE_REG, (data1[1]<<8) | data1[0]);

    fg_i2c_read(client, OCV_REG, data1, 2);
    dev_dbg(&client->dev, "%s: OCV_REG(%d), data(0x%04x)\n",
        __func__, OCV_REG, (data1[1]<<8) | data1[0]);
/*************************************************************/

    if (fg_i2c_read(client, TEMPERATURE_REG, data, 2) < 0) {
        dev_err(&client->dev,
            "%s: Failed to read TEMPERATURE_REG\n",
            __func__);
        return -1;
    }

    if (data[1]&(0x1 << 7)) {
        temper = ((~(data[1]))&0xFF)+1;
#if 0
#if defined(CONFIG_N1A)
        /* compensate for read lower then real temp*/
		if (temper < -5)
            temper = -5;
        if (temper>41)
            temper +=2;
        else if (temper>37)
            temper +=4;
        else if (temper>31)
            temper +=6;
        else if (temper>22)
            temper +=9;
        else if (temper>12)
            temper +=10;
        else if (temper>0)
            temper +=12;
#endif
#endif
        temper *= (1000);
        temper -= ((~((int)data[0]))+1) * 39 / 10;
    } else {
        temper = data[1] & 0x7f;
        temper *= 1000;
        temper += data[0] * 39 / 10;
    }
#if 0
    /* Adjust temperature */
    for (i = 0; i < TEMP_RANGE_MAX_NUM-1; i++) {
        if ((temper >= get_battery_data(fuelgauge).
            temp_adjust_table[i][RANGE]) &&
            (temper < get_battery_data(fuelgauge).
            temp_adjust_table[i+1][RANGE])) {
            temper = (temper *
                get_battery_data(fuelgauge).
                temp_adjust_table[i][SLOPE] /
                100) -
                get_battery_data(fuelgauge).
                temp_adjust_table[i][OFFSET];
            break;
        }
    }
    if (i == TEMP_RANGE_MAX_NUM-1)
        dev_dbg(&client->dev,
            "%s : No adjustment for temperature\n",
            __func__);
#endif
    dev_dbg(&client->dev, "%s: TEMPERATURE(%d), data(0x%04x)\n",
        __func__, temper, (data[1]<<8) | data[0]);

	if (!(count++ % PRINT_COUNT)) {
		dev_dbg(&client->dev, "%s: TEMPERATURE(%d), data(0x%04x)\n",
			__func__, temper, (data[1]<<8) | data[0]);
		count = 1;
	}
	return temper/100;
}

/* soc should be 0.1% unit data stored in percent xxx.xxxx%*/
static int fg_read_vfsoc(struct i2c_client *client)
{
	u8 data[2];
	int soc;

	if (fg_i2c_read(client, VFSOC_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read VFSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
static int fg_read_avsoc(struct i2c_client *client)
{
	u8 data[2];
	int soc;

	if (fg_i2c_read(client, SOCAV_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read AVSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit data stored in percent xxx.xxxx%*/
static int fg_read_soc(struct i2c_client *client)
{
	u8 data[2];
	int soc;
	int rep_soc;
	int vf_soc;
	static int count = 0;

	if (fg_i2c_read(client, SOCREP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;
	rep_soc = min(soc, 1000);
	vf_soc = fg_read_vfsoc(client);

	if (!(count++ % PRINT_COUNT)) {
		dev_dbg(&client->dev, "%s: raw capacity (%d), data(0x%04x)\n",
			__func__, soc, (data[1]<<8) | data[0]);
		dev_dbg(&client->dev, "%s: RepSOC (%d), VFSOC (%d)\n",
			__func__, rep_soc/10, vf_soc/10);
		count = 1;
	}

	return rep_soc;
}

/* soc should be 0.01% unit */
static int fg_read_rawsoc(struct i2c_client *client)
{
	u8 data[2];
	int soc;
	static int count = 0;

	if (fg_i2c_read(client, SOCREP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = (data[1] * 100) + (data[0] * 100 / 256);

	if (!(count++ % PRINT_COUNT)) {
		dev_dbg(&client->dev, "%s: raw capacity (0x01%%) (%d), data(0x%04x)\n",
			__func__, soc, (data[1]<<8) | data[0]);
		count = 1;
	}

	return min(soc, 10000);
}

/*FULLCAP_REG as DESIGNCAP_REG*/
static int fg_read_fullcap(struct i2c_client *client)
{
	u8 data[2];
	int ret;
    /*FULLCAP_REG Vs DESIGNCAP_REG better use is FULLCAP*/
	if (fg_i2c_read(client, FULLCAP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read FULLCAP\n", __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}
/*FULLCAP_REG as FULLCAP_NOM_REG*/
static int fg_read_mixcap(struct i2c_client *client)
{
	u8 data[2];
	int ret;

	if (fg_i2c_read(client, FULLCAP_NOM_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read REMCAP_MIX_REG\n",
			__func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_avcap(struct i2c_client *client)
{
	u8 data[2];
	int ret;

	if (fg_i2c_read(client, REMCAP_AV_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read REMCAP_AV_REG\n",
			__func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

/* Capacity of Cell should monitor 2 values SOC now and Charge_Capacity remain*/
static int fg_read_remain_cap_percent(struct i2c_client *client)
{
	u8 remain_data[2], fullcap_data[2], data[2];
	int ret;
	int soc;
    static int prev_rep1, prev_rep2;

	if (fg_i2c_read(client, REMCAP_REP_REG, remain_data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read REMCAP_REP_REG\n",
			__func__);
		return -1;
	}
	
	ret = ((remain_data[1] << 8) + remain_data[0]);
    
	if (ret < prev_rep1) {
        /* let it drop about 0.4% each read ( = 64) */
        if ((prev_rep1 - ret) > 64) {
            if ( (prev_rep2 + prev_rep1 - 2 * ret) > 130) {
                if(prev_rep2 > prev_rep1) {
                    ret = prev_rep1 - 64;
                    prev_rep2 = prev_rep1;
                } else {
                    ret = prev_rep1 - 64;
                    prev_rep1 -= 64;
                }
                /* correction to prevent soc jump when discharge */
                /**** correct value  ***/
                fg_write_register(client, REMCAP_REP_REG, (u16)(ret));
            }
        } else {
            if (prev_rep2 == prev_rep1 ) {
                prev_rep1 = ret;
            } else {
                prev_rep2 = prev_rep1;
                prev_rep1 = ret;
            }
        }
    } else {
        prev_rep1 = ret;
        prev_rep2 = prev_rep1;
    }
	if (fg_i2c_read(client, FULLCAP_REG, fullcap_data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read FULLCAP_REG\n",
			__func__);
		return -1;
	}

	ret = 100 * (((remain_data[1] << 8) + remain_data[0]) / ((fullcap_data[1] << 8) + fullcap_data[0]));
    /* data stored in % */
	if (fg_i2c_read(client, SOCREP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = data[1] + (data[0]/ 256);

    /*** check if SOC or remain Charge_Capacity is greater ********/
    if (ret > soc) {
        return ret;
    } else {
        return soc;
    }
}

static int fg_read_repcap(struct i2c_client *client)
{
	u8 data[2];
	int ret;

	if (fg_i2c_read(client, REMCAP_REP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read REMCAP_REP_REG\n",
			__func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int fg_read_current(struct i2c_client *client, int unit)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 data1[2], data2[2];
	u32 temp, sign;
	s32 i_current;
	s32 avg_current;

	if (fg_i2c_read(client, CURRENT_REG, data1, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read CURRENT\n",
			__func__);
		return -1;
	}

	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read AVERAGE CURRENT\n",
			__func__);
		return -1;
	}

	temp = ((data1[1]<<8) | data1[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		i_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		i_current = temp * 15625 / 100000;
	}

	if (sign)
		i_current *= -1;

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	avg_current = temp * 15625 / 100000;
	if (sign)
		avg_current *= -1;

	if (!(fuelgauge->info.pr_cnt++ % PRINT_COUNT)) {
		fg_test_print(client);
		dev_info(&client->dev, "%s: CURRENT(%dmA), AVG_CURRENT(%dmA)\n",
			__func__, i_current, avg_current);
		fuelgauge->info.pr_cnt = 1;
		/* Read max17050's all registers every 5 minute. */
		fg_periodic_read(client);
	}

	return i_current;
}

static int fg_read_avg_current(struct i2c_client *client, int unit)
{
	u8  data2[2];
	u32 temp, sign;
	s32 avg_current;

	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read AVERAGE CURRENT\n",
			__func__);
		return -1;
	}

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		avg_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		avg_current = temp * 15625 / 100000;
	}

	if (sign)
		avg_current *= -1;

	return avg_current;
}

int fg_reset_soc(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	/* delay for current stablization */
	msleep(500);

	dev_info(&client->dev,
		"%s: Before quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, fg_read_vcell(client), fg_read_vfocv(client),
		fg_read_vfsoc(client), fg_read_soc(client));
	dev_info(&client->dev,
		"%s: Before quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(client, SEC_BATTEY_CURRENT_MA),
		fg_read_avg_current(client, SEC_BATTEY_CURRENT_MA));

	if (!fuelgauge->pdata->check_jig_status()) {
		dev_info(&client->dev,
			"%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	fg_write_register(client, CYCLES_REG, 0);

	if (fg_i2c_read(client, MISCCFG_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read MiscCFG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);
	if (fg_i2c_write(client, MISCCFG_REG, data, 2) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write MiscCFG\n", __func__);
		return -1;
	}

/*********************************SET UP******************************************/
/*
1) Read Status register. If POR = 0, exit.
2) Wait 600ms for POR operation to fully complete.
3) Restore all application register values.
4) Restore fuel gauge learned-value information (see the
Save and Restore Registers section).
5) Clear POR bit.
*/    

	if (fg_i2c_read(client, STATUS_REG, data, 2) < 0) {
		dev_err(&client->dev,"%s: Failed to read STATUS_REG\n", __func__);
	}
    if(!(data[0] & (0x1 << 1)))
        /* wait for  600ms to complete power up POR 0->1*/
        msleep(600);
    
    /* Set Design Charge Capacity of Battery in mAh 
     * converted to uV with sense Resistor of 10mOhm 
     * with 5uV step
     * */
	fg_write_register(client, DESIGNCAP_REG, get_battery_data(fuelgauge).Charge_Capacity);
    
    data[0] = 0x00;
    data[1] = 0x00;
    fg_i2c_write(client, ATRATE_REG, data, 2);
    /* 95% FULLSOCTHR*/
    data[0] = 0x00;
    data[1] = 0x5F;
    fg_i2c_write(client, FULLSOCTHR_REG, data, 2);
    
    /*End of charge current detection*/
    //200mA = 500h 250mA = 640h with 10mOhm UNITS: 1.5625μV/RSENSE
    data[0] = 0xC0;
    data[1] = 0x03;
    fg_i2c_write(client, ICHGTERM_REG, data, 2);
    /**** Reset Status bits*/
    data[0] = 0x00;
    data[1] = 0x00;
    fg_i2c_write(client, STATUS_REG, data, 2);
/******************************************************************************************/
	/* NOT using FG for temperature */
	if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		data[0] = 0x00;
		data[1] = 0x21;
		fg_i2c_write(client, CONFIG_REG, data, 2);
	} else {
    /***** CONFIG_REG **********************************************************************/
    /*SS TS VS  ALRTp   AINSH   Ten Tex SHDN    I2CSH   ALSH    ETHRM   FTHRM   Aen Bei Ber*/
    /*0  0  0   0       0       1   0   0       0       1       1       0       0   0   0  */

		data[0] = 0x30;
		data[1] = 0x02;
        fg_i2c_write(client, CONFIG_REG, data, 2);
    }
/******************************************************************************************/
	msleep(500);

	dev_info(&client->dev,
		"%s: After quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, fg_read_vcell(client), fg_read_vfocv(client),
		fg_read_vfsoc(client), fg_read_soc(client));
	dev_info(&client->dev,
		"%s: After quick-start - current(%d), avg current(%d)\n",
		__func__, fg_read_current(client, SEC_BATTEY_CURRENT_MA),
		fg_read_avg_current(client, SEC_BATTEY_CURRENT_MA));
	fg_write_register(client, CYCLES_REG, 0x00a0);

	return 0;
}


int fg_reset_capacity_by_jig_connection(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);

	dev_info(&client->dev,
		"%s: DesignCap = Charge_Capacity (Jig Connection)\n", __func__);

	return fg_write_register(client, DESIGNCAP_REG,
		get_battery_data(fuelgauge).Charge_Capacity);
}

#if 0
int fg_adjust_capacity(struct i2c_client *client)
{
	u8 data[2];

	data[0] = 0;
	data[1] = 0;

	/* 1. Write RemCapREP(05h)=0; */
	if (fg_i2c_write(client, REMCAP_REP_REG, data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to write RemCap_REP\n",
			__func__);
		return -1;
	}
	msleep(200);

	dev_info(&client->dev, "%s: After adjust - RepSOC(%d)\n", __func__,
		fg_read_soc(client));

	return 0;
}
#endif
#if 0
void fg_low_batt_compensation(struct i2c_client *client, u32 level)
{
	int read_val;
	u32 temp;

	dev_info(&client->dev, "%s: Adjust SOCrep to %d!!\n",
		__func__, level);

	read_val = fg_read_register(client, FULLCAP_REG);
	/* RemCapREP (05h) = FullCap(10h) x 0.0090 */
	temp = read_val * (level*90) / 10000;
	fg_write_register(client, REMCAP_REP_REG, (u16)temp);
}
#endif
#if 0
static void fg_read_model_data(struct i2c_client *client)
{
	u16 data0[16], data1[16], data2[16];
	int i;
	int relock_check;

	dev_info(&client->dev, "[FG_Model] ");

	/* Unlock model access */
	fg_write_register(client, 0x62, 0x0059);
	fg_write_register(client, 0x63, 0x00C4);

	/* Read model data */
	fg_read_16register(client, 0x80, data0);
	fg_read_16register(client, 0x90, data1);
	fg_read_16register(client, 0xa0, data2);

	/* Print model data */
	for (i = 0; i < 16; i++)
		dev_info(&client->dev, "0x%04x, ", data0[i]);

	for (i = 0; i < 16; i++)
		dev_info(&client->dev, "0x%04x, ", data1[i]);

	for (i = 0; i < 16; i++) {
		if (i == 15)
			dev_info(&client->dev, "0x%04x", data2[i]);
		else
			dev_info(&client->dev, "0x%04x, ", data2[i]);
	}

	do {
		relock_check = 0;
		/* Lock model access */
		fg_write_register(client, 0x62, 0x0000);
		fg_write_register(client, 0x63, 0x0000);

		/* Read model data again */
		fg_read_16register(client, 0x80, data0);
		fg_read_16register(client, 0x90, data1);
		fg_read_16register(client, 0xa0, data2);

		for (i = 0; i < 16; i++) {
			if (data0[i] || data1[i] || data2[i]) {
				dev_dbg(&client->dev,
					"%s: data is non-zero, lock again!!\n",
					__func__);
				relock_check = 1;
			}
		}
	} while (relock_check);
}
#endif

static int fg_check_status_reg(struct i2c_client *client)
{
	u8 status_data[2];
	int ret = 0;

	/* 1. Check Smn was generated read */
	if (fg_i2c_read(client, STATUS_REG, status_data, 2) < 0) {
		dev_err(&client->dev, "%s: Failed to read STATUS_REG\n",
			__func__);
		return -1;
	}
	dev_info(&client->dev, "%s:STATUS_REG addr(0x00), data[0](0x%02x)\n", __func__,status_data[0]);
	dev_info(&client->dev, "%s:STATUS_REG addr(0x00), data[1](0x%02x)\n", __func__,status_data[1]);

	if (status_data[1] & (0x1 << 2))
		ret = 1;

	/* 2. clear Status reg */
	status_data[1] = 0;
	if (fg_i2c_write(client, STATUS_REG, status_data, 2) < 0) {
		dev_info(&client->dev, "%s: Failed to write STATUS_REG\n",
			__func__);
		return -1;
	}
		dev_info(&client->dev,
			"%s: addr(0x%02x), data(0x%04x)\n", __func__, STATUS_REG,
			(status_data[1]<<8) | status_data[0]);

	return ret;
}

int get_fuelgauge_value(struct i2c_client *client, int data)
{
	int ret;

	switch (data) {
	case FG_LEVEL:
		ret = fg_read_soc(client);
		break;

	case FG_TEMPERATURE:
		ret = fg_read_temp(client);
		break;

	case FG_VOLTAGE:
		ret = fg_read_vcell(client);
		break;

	case FG_CURRENT:
		ret = fg_read_current(client, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CURRENT_AVG:
		ret = fg_read_avg_current(client, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CHECK_STATUS:
		ret = fg_check_status_reg(client);
		break;

	case FG_RAW_SOC:
        ret = fg_read_remain_cap_percent(client);
		break;

	case FG_VF_SOC:
		ret = fg_read_vfsoc(client);
		break;

	case FG_AV_SOC:
		ret = fg_read_avsoc(client);
		break;

	case FG_FULLCAP://FULLCAP_REG
		ret = fg_read_fullcap(client);
		break;

	case FG_MIXCAP:
		ret = fg_read_mixcap(client);
		break;

	case FG_AVCAP:
		ret = fg_read_avcap(client);
		break;

	case FG_REPCAP:
		ret = fg_read_repcap(client);
		break;
        
	default:
		ret = -1;
		break;
	}

	return ret;
}

int fg_alert_init(struct i2c_client *client, int soc)
{
	u8 misccgf_data[2];
	u8 salrt_data[2];
	u8 config_data[2];
	u8 valrt_data[2];
	u8 talrt_data[2];
	u16 read_data = 0;

	/* Using RepSOC */
	if (fg_i2c_read(client, MISCCFG_REG, misccgf_data, 2) < 0) {
		dev_err(&client->dev,
			"%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);

	if (fg_i2c_write(client, MISCCFG_REG, misccgf_data, 2) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data[1] = 0xff;
	salrt_data[0] = soc;
	if (fg_i2c_write(client, SALRT_THRESHOLD_REG, salrt_data, 2) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	/* Reset VALRT Threshold setting (disable) */
	valrt_data[1] = 0xFF;
	valrt_data[0] = 0x00;
	if (fg_i2c_write(client, VALRT_THRESHOLD_REG, valrt_data, 2) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register(client, (u8)VALRT_THRESHOLD_REG);
	if (read_data != 0xff00)
		dev_err(&client->dev,
			"%s: VALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/* Reset TALRT Threshold setting (disable) */
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if (fg_i2c_write(client, TALRT_THRESHOLD_REG, talrt_data, 2) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register(client, (u8)TALRT_THRESHOLD_REG);
	if (read_data != 0x7f80)
		dev_err(&client->dev,
			"%s: TALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/*mdelay(100);*/

	/* Enable SOC alerts */
	if (fg_i2c_read(client, CONFIG_REG, config_data, 2) < 0) {
		dev_err(&client->dev,
			"%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);

	if (fg_i2c_write(client, CONFIG_REG, config_data, 2) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	return 1;
}

void fg_fullcharged_compensation(struct i2c_client *client,
		u32 is_recharging, bool pre_update)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	static int new_fullcap_data;

	dev_info(&client->dev, "%s: is_recharging(%d), pre_update(%d)\n",
		__func__, is_recharging, pre_update);

	new_fullcap_data =
		fg_read_register(client, FULLCAP_REG);
/*****************************????????????????????????????????????????*/
	if (new_fullcap_data < 0)
		new_fullcap_data = get_battery_data(fuelgauge).Charge_Capacity;
/*****************************????????????????????????????????????????*/
	/* compare with initial capacity */
	if (new_fullcap_data >
		(get_battery_data(fuelgauge).Charge_Capacity * 110 / 100)) {
        /****  Full Cap > 110% Design Cap ****/
		dev_info(&client->dev,
			"%s: [Case 1] capacity = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, get_battery_data(fuelgauge).Charge_Capacity,
			new_fullcap_data);

		new_fullcap_data =
			(get_battery_data(fuelgauge).Charge_Capacity * 110) / 100;

		fg_write_register(client, FULLCAP_REG,
			(u16)(new_fullcap_data));
	} else if (new_fullcap_data <
		(get_battery_data(fuelgauge).Charge_Capacity * 90 / 100)) {
        /****  Full Cap < 90% Design Cap ****/
		dev_info(&client->dev,
			"%s: [Case 5] capacity = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, get_battery_data(fuelgauge).Charge_Capacity,
			new_fullcap_data);

		new_fullcap_data =
			(get_battery_data(fuelgauge).Charge_Capacity * 90) / 100;

		fg_write_register(client, FULLCAP_REG,
			(u16)(new_fullcap_data));
	} else {
         /****  90% Design Cap <= Full Cap <= 110% Design Cap ****/
	/* compare with previous capacity */
		if (new_fullcap_data >
			(fuelgauge->info.previous_fuelcap * 110 / 100)) {
			dev_info(&client->dev,
				"%s: [Case 2] previous_fuelcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fuelcap,
				new_fullcap_data);

			new_fullcap_data =
				(fuelgauge->info.previous_fuelcap * 110) / 100;

//			fg_write_register(client, REMCAP_REP_REG, (u16)(new_fullcap_data));
			fg_write_register(client, FULLCAP_REG,
				(u16)(new_fullcap_data));
		} else if (new_fullcap_data <
			(fuelgauge->info.previous_fuelcap * 90 / 100)) {
			dev_info(&client->dev,
				"%s: [Case 3] previous_fuelcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fuelcap,
				new_fullcap_data);

			new_fullcap_data =
				(fuelgauge->info.previous_fuelcap * 90) / 100;

//			fg_write_register(client, REMCAP_REP_REG, (u16)(new_fullcap_data));
			fg_write_register(client, FULLCAP_REG,
				(u16)(new_fullcap_data));
		} else {
			dev_info(&client->dev,
				"%s: [Case 4] previous_fuelcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_fuelcap,
				new_fullcap_data);
		}
	}

	/* 4. Write RepSOC(06h)=100%; */
//	fg_write_register(client, SOCREP_REG, (u16)(0x64 << 8));

	/* 5. Write MixSOC(0Dh)=100%; */
//	fg_write_register(client, SOCMIX_REG, (u16)(0x64 << 8));

	/* 6. Write AVSOC(0Eh)=100%; */
//	fg_write_register(client, SOCAV_REG, (u16)(0x64 << 8));

	/* if pre_update case, skip updating PrevFullCAP value. */
	if (!pre_update)
		fuelgauge->info.previous_fuelcap =
			fg_read_register(client, FULLCAP_REG);

	dev_info(&client->dev,
		"%s: (A) FullCap = 0x%04x, RemCap = 0x%04x\n", __func__,
		fg_read_register(client, FULLCAP_REG),
		fg_read_register(client, REMCAP_REP_REG));

	fg_periodic_read(client);
}

/*no calling signal to this function*/
void fg_check_vf_fuelcap_range(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	static int new_vffuelcap;
	bool is_vffuelcap_changed = true;

	if (fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(client);

	new_vffuelcap = fg_read_register(client, FULLCAP_NOM_REG);
	if (new_vffuelcap < 0)
		new_vffuelcap = get_battery_data(fuelgauge).Charge_Capacity;

	/* compare with initial capacity */
	if (new_vffuelcap >
		(get_battery_data(fuelgauge).Charge_Capacity * 110 / 100)) {
		dev_info(&client->dev,
			"%s: [Case 1] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, get_battery_data(fuelgauge).Charge_Capacity,
			new_vffuelcap);

		new_vffuelcap =
			(get_battery_data(fuelgauge).Charge_Capacity * 110) / 100;

		fg_write_register(client, DQACC_REG,
			(u16)(new_vffuelcap / 4));
		fg_write_register(client, DPACC_REG, (u16)0x3200);
	} else if (new_vffuelcap <
		(get_battery_data(fuelgauge).Charge_Capacity * 50 / 100)) {
		dev_info(&client->dev,
			"%s: [Case 5] capacity = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, get_battery_data(fuelgauge).Charge_Capacity,
			new_vffuelcap);

		new_vffuelcap =
			(get_battery_data(fuelgauge).Charge_Capacity * 50) / 100;

		fg_write_register(client, DQACC_REG,
			(u16)(new_vffuelcap / 4));
		fg_write_register(client, DPACC_REG, (u16)0x3200);
	} else {
	/* compare with previous capacity */
		if (new_vffuelcap >
			(fuelgauge->info.previous_vffuelcap * 110 / 100)) {
			dev_info(&client->dev,
				"%s: [Case 2] previous_vffuelcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffuelcap,
				new_vffuelcap);

			new_vffuelcap =
				(fuelgauge->info.previous_vffuelcap * 110) /
				100;

			fg_write_register(client, DQACC_REG,
				(u16)(new_vffuelcap / 4));
			fg_write_register(client, DPACC_REG, (u16)0x3200);
		} else if (new_vffuelcap <
			(fuelgauge->info.previous_vffuelcap * 90 / 100)) {
			dev_info(&client->dev,
				"%s: [Case 3] previous_vffuelcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffuelcap,
				new_vffuelcap);

			new_vffuelcap =
				(fuelgauge->info.previous_vffuelcap * 90) / 100;

			fg_write_register(client, DQACC_REG,
				(u16)(new_vffuelcap / 4));
			fg_write_register(client, DPACC_REG, (u16)0x3200);
		} else {
			dev_info(&client->dev,
				"%s: [Case 4] previous_vffuelcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, fuelgauge->info.previous_vffuelcap,
				new_vffuelcap);
			is_vffuelcap_changed = false;
		}
	}

	/* delay for register setting (dQacc, dPacc) */
	if (is_vffuelcap_changed)
		msleep(300);

	fuelgauge->info.previous_vffuelcap =
		fg_read_register(client, FULLCAP_NOM_REG);

	if (is_vffuelcap_changed)
		dev_info(&client->dev,
			"%s : VfFullCap(0x%04x), dQacc(0x%04x), dPacc(0x%04x)\n",
			__func__,
			fg_read_register(client, FULLCAP_NOM_REG),
			fg_read_register(client, DQACC_REG),
			fg_read_register(client, DPACC_REG));

}

/* no need to set full charged*/
#if 0
void fg_set_full_charged(struct i2c_client *client)
{
	dev_info(&client->dev, "[FG_Set_Full] (B) FullCAP(%d), RemCAP(%d)\n",
		(fg_read_register(client, FULLCAP_REG)/2),
		(fg_read_register(client, REMCAP_REP_REG)/2));

	fg_write_register(client, FULLCAP_REG,
		(u16)fg_read_register(client, REMCAP_REP_REG));

	dev_info(&client->dev, "[FG_Set_Full] (A) FullCAP(%d), RemCAP(%d)\n",
		(fg_read_register(client, FULLCAP_REG)/2),
		(fg_read_register(client, REMCAP_REP_REG)/2));
}
#endif

static void display_low_batt_comp_cnt(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);

	pr_info("Check Array(%s): [%d, %d], [%d, %d], ",
			get_battery_data(fuelgauge).type_str,
			fuelgauge->info.low_batt_comp_cnt[0][0],
			fuelgauge->info.low_batt_comp_cnt[0][1],
			fuelgauge->info.low_batt_comp_cnt[1][0],
			fuelgauge->info.low_batt_comp_cnt[1][1]);
	pr_info("[%d, %d], [%d, %d], [%d, %d]\n",
			fuelgauge->info.low_batt_comp_cnt[2][0],
			fuelgauge->info.low_batt_comp_cnt[2][1],
			fuelgauge->info.low_batt_comp_cnt[3][0],
			fuelgauge->info.low_batt_comp_cnt[3][1],
			fuelgauge->info.low_batt_comp_cnt[4][0],
			fuelgauge->info.low_batt_comp_cnt[4][1]);
}

static void add_low_batt_comp_cnt(struct i2c_client *client,
				int range, int level)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int i;
	int j;

	/* Increase the requested count value, and reset others. */
	fuelgauge->info.low_batt_comp_cnt[range-1][level/2]++;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (i == range-1 && j == level/2)
				continue;
			else
				fuelgauge->info.low_batt_comp_cnt[i][j] = 0;
		}
	}
}


void prevent_early_poweroff(struct i2c_client *client,
	int vcell, int *fg_soc)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int soc = 0;
	int read_val;

	soc = fg_read_soc(client);

	/* No need to write REMCAP_REP in below normal cases */
	if (soc > POWER_OFF_SOC_HIGH_MARGIN || vcell > get_battery_data(fuelgauge).low_battery_comp_voltage)
		return;

	dev_info(&client->dev, "%s: soc=%d, vcell=%d\n", __func__,
		soc, vcell);

	if (vcell > POWER_OFF_VOLTAGE_HIGH_MARGIN) {
        
		//read_val = fg_read_register(client, FULLCAP_REG);
		/* FullCAP * 0.013 */
        /*
		fg_write_register(client, REMCAP_REP_REG,
		(u16)(read_val * 13 / 1000));
		msleep(200);
		*fg_soc = fg_read_soc(client);
		dev_info(&client->dev, "%s: new soc=%d, vcell=%d\n",
			__func__, *fg_soc, vcell);
        */
	}
}


void reset_low_batt_comp_cnt(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);

	memset(fuelgauge->info.low_batt_comp_cnt, 0,
		sizeof(fuelgauge->info.low_batt_comp_cnt));
}

static int check_low_batt_comp_condition(
				struct i2c_client *client, int *nLevel)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int i;
	int j;
	int ret = 0;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (fuelgauge->info.low_batt_comp_cnt[i][j] >=
				MAX_LOW_BATT_CHECK_CNT) {
				display_low_batt_comp_cnt(client);
				ret = 1;
				*nLevel = j*2 + 1;
				break;
			}
		}
	}

	return ret;
}

static int get_low_batt_threshold(struct i2c_client *client,
				int range, int nCurrent, int level)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int ret = 0;

	ret = get_battery_data(fuelgauge).low_battery_table[range][OFFSET] +
		((nCurrent *
		get_battery_data(fuelgauge).low_battery_table[range][SLOPE]) /
		1000);

	return ret;
}

int low_batt_compensation(struct i2c_client *client,
		int fg_soc, int fg_vcell, int fg_current)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int fg_avg_current = 0;
	int fg_min_current = 0;
	int new_level = 0;
	int i, table_size;

	/* Not charging, Under low battery comp voltage */
	if (fg_vcell <= get_battery_data(fuelgauge).low_battery_comp_voltage) {
		fg_avg_current = fg_read_avg_current(client,
			SEC_BATTEY_CURRENT_MA);
		fg_min_current = min(fg_avg_current, fg_current);

		table_size =
			sizeof(get_battery_data(fuelgauge).low_battery_table) /
			(sizeof(s16)*TABLE_MAX);

		for (i = 1; i < CURRENT_RANGE_MAX_NUM; i++) {
			if ((fg_min_current >= get_battery_data(fuelgauge).
				low_battery_table[i-1][RANGE]) &&
				(fg_min_current < get_battery_data(fuelgauge).
				low_battery_table[i][RANGE])) {
				if (fg_soc >= 10 && fg_vcell <
					get_low_batt_threshold(client,
					i, fg_min_current, 1)) {
					add_low_batt_comp_cnt(
						client, i, 1);
				} else {
					reset_low_batt_comp_cnt(client);
				}
			}
		}

		if (check_low_batt_comp_condition(client, &new_level)) {
			//fg_low_batt_compensation(client, new_level);
			reset_low_batt_comp_cnt(client);

			/* Do not update soc right after
			 * low battery compensation
			 * to prevent from powering-off suddenly
			 */
			dev_info(&client->dev,
				"%s: SOC is set to %d by low compensation!!\n",
				__func__, fg_read_soc(client));
		}
	}

	/* Prevent power off over 3500mV */
	prevent_early_poweroff(client, fg_vcell, &fg_soc);

	return fg_soc;
}

static bool is_booted_in_low_battery(struct i2c_client *client)
{
	int fg_vcell = get_fuelgauge_value(client, FG_VOLTAGE);
	int fg_current = get_fuelgauge_value(client, FG_CURRENT);
	int threshold = 0;

	threshold = 3300 + ((fg_current * 17) / 100);

	if (fg_vcell <= threshold)
		return true;
	else
		return false;
}
/* This function no other calling*/
static bool fuelgauge_recovery_handler(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
#if 0
	int current_soc;
	int avsoc;
	int temperature;
#endif

	if (fuelgauge->info.soc < LOW_BATTERY_SOC_REDUCE_UNIT) {
		fuelgauge->info.is_low_batt_alarm = false;
	} else {
		dev_err(&client->dev,
			"%s: Reduce the Reported SOC by 1%%\n",
			__func__);
#if 0
		current_soc =
			get_fuelgauge_value(client, FG_LEVEL) / 10;

		if (current_soc) {
			dev_info(&client->dev,
				"%s: Returning to Normal discharge path\n",
				__func__);
			dev_info(&client->dev,
				"%s: Actual SOC(%d) non-zero\n",
				__func__, current_soc);
			fuelgauge->info.is_low_batt_alarm = false;
		} else {
			temperature =
				get_fuelgauge_value(client, FG_TEMPERATURE);
			avsoc =
				get_fuelgauge_value(client, FG_AV_SOC);

			if ((fuelgauge->info.soc > avsoc) ||
				(temperature < 0)) {
				fuelgauge->info.soc -=
					LOW_BATTERY_SOC_REDUCE_UNIT;
				dev_err(&client->dev,
					"%s: New Reduced RepSOC (%d)\n",
					__func__, fuelgauge->info.soc);
			} else
				dev_info(&client->dev,
					"%s: Waiting for recovery (AvSOC:%d)\n",
					__func__, avsoc);
		}
#endif
		fuelgauge->info.soc -=
			LOW_BATTERY_SOC_REDUCE_UNIT;
		dev_err(&client->dev,
			"%s: New Reduced RepSOC (%d)\n",
			__func__, fuelgauge->info.soc);
	}

	return fuelgauge->info.is_low_batt_alarm;
}

static void full_comp_work_handler(struct work_struct *work)
{
	struct sec_fg_info *fg_info =
		container_of(work, struct sec_fg_info, full_comp_work.work);
	struct sec_fuelgauge_info *fuelgauge =
		container_of(fg_info, struct sec_fuelgauge_info, info);
	int avg_current;
	union power_supply_propval value;

	avg_current = get_fuelgauge_value(fuelgauge->client, FG_CURRENT_AVG);
	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

	if (avg_current >= 25) {
		cancel_delayed_work(&fuelgauge->info.full_comp_work);
		schedule_delayed_work(&fuelgauge->info.full_comp_work, 100);
	} else {
		dev_info(&fuelgauge->client->dev,
			"%s: full charge compensation start (avg_current %d)\n",
			__func__, avg_current);
		fg_fullcharged_compensation(fuelgauge->client,
			(int)(value.intval ==
			POWER_SUPPLY_STATUS_FULL), false);
	}
}

static irqreturn_t sec_jig_irq_thread(int irq, void *irq_data)
{
	struct sec_fuelgauge_info *fuelgauge = irq_data;

	if (fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(fuelgauge->client);
	else
		dev_info(&fuelgauge->client->dev,
				"%s: jig removed\n", __func__);
	return IRQ_HANDLED;
}

bool sec_hal_fg_init(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge =
				i2c_get_clientdata(client);
	ktime_t	current_time;
	struct timespec ts;
	u8 data[2] = {0, 0};

#if defined(ANDROID_ALARM_ACTIVATED)
	current_time = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(current_time);
#else
	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);
#endif

	fuelgauge->info.fuelcap_check_interval = ts.tv_sec;

	fuelgauge->info.is_low_batt_alarm = false;
	fuelgauge->info.is_first_check = true;

	/* Init parameters to prevent wrong compensation. */
	fuelgauge->info.previous_fuelcap =
		fg_read_register(client, FULLCAP_REG);
	fuelgauge->info.previous_vffuelcap =
		fg_read_register(client, FULLCAP_NOM_REG);

	/* To reduce booting time, skip reading regs
	* fg_read_model_data(client);
	* fg_periodic_read(client);
	*/
	if (fuelgauge->pdata->check_cable_callback() !=
		POWER_SUPPLY_TYPE_BATTERY &&
		is_booted_in_low_battery(client))
		fuelgauge->info.low_batt_boot_flag = 1;///?/

	if (fuelgauge->pdata->check_jig_status())
		fg_reset_capacity_by_jig_connection(client);
	else {
		if (fuelgauge->pdata->jig_irq) {
			int ret;
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
					NULL, sec_jig_irq_thread,
					fuelgauge->pdata->jig_irq_attr,
					"jig-irq", fuelgauge);
			if (ret) {
				dev_info(&fuelgauge->client->dev,
					"%s: Failed to Reqeust IRQ\n",
					__func__);
			}
		}
	}

	INIT_DELAYED_WORK(&fuelgauge->info.full_comp_work,
		full_comp_work_handler);

/*******************************************SET UP*********************************/
    /* SET STATUS at boot*/
/*
1) Read Status register. If POR = 0, exit.
2) Wait 600ms for POR operation to fully complete.
3) Restore all application register values.
4) Restore fuel gauge learned-value information (see the
Save and Restore Registers section).
5) Clear POR bit.
*/    
	if (fg_i2c_read(client, STATUS_REG, data, 2) < 0) {
		dev_err(&client->dev,"%s: Failed to read STATUS_REG\n", __func__);
	}
    if(!(data[0] & (0x1 << 1)))
        /* wait for  600ms to complete power up*/
        msleep(600);

    
    /* Set Design Charge Capacity of Battery in mAh 
     * converted to uV with sense Resistor of 10mOhm 
     * with 5uV step
     *  0x4038;// = 8220mAh with 10m ohm with 5uV step*/
	fg_write_register(client, DESIGNCAP_REG, get_battery_data(fuelgauge).Charge_Capacity);

    data[0] = 0x00;
    data[1] = 0x00;
    fg_i2c_write(client, ATRATE_REG, data, 2);
    
    data[0] = 0x00;
    data[1] = 0x5F;
    fg_i2c_write(client, FULLSOCTHR_REG, data, 2);
    
    /*End of charge current detection*/
    //150mA with 10mOhm UNITS: 1.5625μV/RSENSE
    data[0] = 0xC0;
    data[1] = 0x03;
    fg_i2c_write(client, ICHGTERM_REG, data, 2);

    /*clear status setting bits*/
    data[0] = 0x00;
    data[1] = 0x00;
    fg_i2c_write(client, STATUS_REG, data, 2);
    msleep(250);
	if (fg_i2c_read(client, STATUS_REG, data, 2) < 0) {
		dev_err(&client->dev,
			"%s: Failed to read STATUS_REG\n", __func__);
		return 0;
	}
	
	dev_info(&client->dev,"%s:STATUS_REG addr(0x%02x), data(0x%04x) after init.\n", __func__, STATUS_REG,
			(data[1]<<8) | data[0]);

	/* NOT using FG for temperature */
	if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		data[0] = 0x00;
		data[1] = 0x21;
		fg_i2c_write(client, CONFIG_REG, data, 2);
	} else {
    /***** CONFIG_REG **********************************************************************/
    /*SS TS VS  ALRTp   AINSH   Ten Tex SHDN    I2CSH   ALSH    ETHRM   FTHRM   Aen Bei Ber*/
    /*0  0  0   0       0       1   0   0       0       1       1       0       0   0   0  */
		data[0] = 0x30;
		data[1] = 0x02;
        fg_i2c_write(client, CONFIG_REG, data, 2);
    }

/****************************************************************************/

	return true;
}

bool sec_hal_fg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	if (fg_alert_init(client, soc) > 0)
		return true;
	else
		return false;
}

bool sec_hal_fg_is_fuelalerted(struct i2c_client *client)
{
	if (get_fuelgauge_value(client, FG_CHECK_STATUS) > 0)
		return true;
	else
		return false;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct sec_fuelgauge_info *fuelgauge =
		(struct sec_fuelgauge_info *)irq_data;
	union power_supply_propval value;
	int overcurrent_limit_in_soc;
	int current_soc =
		get_fuelgauge_value(fuelgauge->client, FG_LEVEL);
#if defined(FUELALERT_CHECK_VOLTAGE_FEATURE)
	int fg_vcell = get_fuelgauge_value(fuelgauge->client, FG_VOLTAGE);
#endif

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);
	if (value.intval == POWER_SUPPLY_STATUS_CHARGING)
		return true;

	if ((int)fuelgauge->info.soc - current_soc <= STABLE_LOW_BATTERY_DIFF)
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF_LOWBATT;
	else
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF;

	if (((int)fuelgauge->info.soc - current_soc) >
		overcurrent_limit_in_soc) {
		dev_info(&fuelgauge->client->dev,
			"%s: Abnormal Current Consumption jump by %d units\n",
			__func__, (((int)fuelgauge->info.soc - current_soc)));
		dev_info(&fuelgauge->client->dev,
			"%s: Last Reported SOC (%d).\n",
			__func__, fuelgauge->info.soc);

		fuelgauge->info.is_low_batt_alarm = true;

		if (fuelgauge->info.soc >=
			LOW_BATTERY_SOC_REDUCE_UNIT)
			return true;
	}

	if (value.intval ==
			POWER_SUPPLY_STATUS_DISCHARGING) {
#if defined(FUELALERT_CHECK_VOLTAGE_FEATURE)
		if (fg_vcell >= POWER_OFF_VOLTAGE_HIGH_MARGIN) {
			dev_info(&fuelgauge->client->dev,
				"%s: skip setting battery level as 0 (voltage: %d)\n",
				__func__, fg_vcell);
			return true;
		}
#endif
		dev_err(&fuelgauge->client->dev,
			"Set battery level as 0, power off.\n");
		fuelgauge->info.soc = 0;
		value.intval = 0;
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_CAPACITY, value);
	}

	return true;
}

bool sec_hal_fg_full_charged(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge =
				i2c_get_clientdata(client);
	union power_supply_propval value;

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

	/* full charge compensation algorithm by MAXIM */
	fg_fullcharged_compensation(client,
		(int)(value.intval == POWER_SUPPLY_STATUS_FULL), true);

	cancel_delayed_work(&fuelgauge->info.full_comp_work);
	schedule_delayed_work(&fuelgauge->info.full_comp_work, 100);

	return false;
}

bool sec_hal_fg_reset(struct i2c_client *client)
{
	if (!fg_reset_soc(client))
		return true;
	else
		return false;
}

bool sec_hal_fg_get_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_fuelgauge_value(client, FG_VOLTAGE);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = fg_read_vfocv(client);
			break;
		case SEC_BATTEY_VOLTAGE_AVERAGE:
		default:
			val->intval = fg_read_avg_vcell(client);
			break;
		}
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				fg_read_current(client, SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval = get_fuelgauge_value(client, FG_CURRENT);
			break;
		}
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				fg_read_avg_current(client,
				SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval =
				get_fuelgauge_value(client, FG_CURRENT_AVG);
			break;
		}
		break;
		/* Fuel Charge_Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CAPACITY_DESIGNED:
			val->intval = get_fuelgauge_value(client, FG_FULLCAP);//DESIGNCAP_REG vs FULLCAP_REG
			break;
		case SEC_BATTEY_CAPACITY_ABSOLUTE:
			val->intval = get_fuelgauge_value(client, FG_MIXCAP);//FULLCAP_NOM_REG
			break;
		case SEC_BATTEY_CAPACITY_TEMPERARY:
			val->intval = get_fuelgauge_value(client, FG_AVCAP);//REMCAP_AV_REG
			break;
		case SEC_BATTEY_CAPACITY_CURRENT:
			val->intval = get_fuelgauge_value(client, FG_REPCAP);//REMCAP_REP_REG
			break;
		}
		break;
		/* % Charge_Capacity  */
	case POWER_SUPPLY_PROP_CAPACITY:
        /* Measurement from FG only*/
			val->intval = get_fuelgauge_value(client, FG_RAW_SOC);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = get_fuelgauge_value(client, FG_TEMPERATURE);
		val->intval = fg_adjust_temp(client, psp, val->intval);
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct sec_fuelgauge_info *fuelgauge =
				i2c_get_clientdata(client);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval != POWER_SUPPLY_TYPE_BATTERY) {
			if (fuelgauge->info.is_low_batt_alarm) {
				dev_info(&client->dev,
					"%s: Reset low_batt_alarm\n",
					__func__);
				fuelgauge->info.is_low_batt_alarm = false;
			}

			reset_low_batt_comp_cnt(client);
		}
		break;
    if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		fg_write_temp(client, val->intval);
		break;
    }    
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		fg_reset_capacity_by_jig_connection(client);
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
/*	case FG_REG: */
/*		break; */
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			fg->reg_data[1], fg->reg_data[0]);
		break;
	case FG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		fg_read_regs(fg->client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int ret = 0;
	int x = 0;

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			fg->reg_addr = x;
			if (fg_i2c_read(fg->client,
				fg->reg_addr, fg->reg_data, 2) < 0) {
				dev_err(dev, "%s: Error in read\n", __func__);
				break;
			}
			dev_dbg(dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				__func__, fg->reg_addr,
				fg->reg_data[1], fg->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%04x\n",
				__func__, fg->reg_addr, x);
			fg_write_and_verify_register(fg->client,
				fg->reg_addr, (u16)x);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

