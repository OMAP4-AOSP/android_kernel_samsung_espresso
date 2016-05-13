/* Power support for Samsung Gerry Board.
 *
 * Copyright (C) 2011 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/twl.h>
#include <linux/power/max17042_battery.h>
#include <linux/battery.h>
#include <linux/irq.h>

#include <asm/system_info.h>

#include "board-espresso.h"

#define GPIO_TA_NCONNECTED	32
#define GPIO_TA_NCHG		142
#define GPIO_TA_EN		13
#define GPIO_FUEL_ALERT	44

#define GPIO_CHG_SDA		98
#define GPIO_CHG_SCL		99
#define GPIO_FUEL_SDA		62
#define GPIO_FUEL_SCL		61

#define CHARGER_STATUS_FULL	0x1

#define HIGH_BLOCK_TEMP	500
#define HIGH_RECOVER_TEMP	420
#define LOW_BLOCK_TEMP		(-50)
#define LOW_RECOVER_TEMP	0

static irqreturn_t charger_state_isr(int irq, void *_data)
{
	int val;

	val = gpio_get_value(GPIO_TA_NCHG);

	irq_set_irq_type(irq, val ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	return IRQ_HANDLED;
}

static irqreturn_t fuel_alert_isr(int irq, void *_data)
{
	int val;

	val = gpio_get_value(GPIO_FUEL_ALERT);
	pr_info("%s: fuel alert interrupt occured : %d\n", __func__, val);

	return IRQ_HANDLED;
}

static void charger_gpio_init(void)
{
	int irq, fuel_irq;
	int ret;
	struct gpio charger_gpios[] = {
		{
			.flags = GPIOF_IN,
			.gpio  = GPIO_TA_NCHG,
			.label = "TA_nCHG"
		},
		{
			.flags = GPIOF_OUT_INIT_LOW,
			.gpio  = GPIO_TA_EN,
			.label = "TA_EN"
		},
		{
			.flags = GPIOF_IN,
			.gpio  = GPIO_FUEL_ALERT,
			.label = "FUEL_ALERT"
		},
	};

	gpio_request_array(charger_gpios, ARRAY_SIZE(charger_gpios));

	irq = gpio_to_irq(GPIO_TA_NCHG);
	ret = request_threaded_irq(irq, NULL, charger_state_isr,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"Charge_Status", NULL);
	if (unlikely(ret < 0))
		pr_err("%s: request irq %d failed for gpio %d\n",
			__func__, irq, GPIO_TA_NCHG);

	fuel_irq = gpio_to_irq(GPIO_FUEL_ALERT);
	ret = request_threaded_irq(fuel_irq, NULL, fuel_alert_isr,
			IRQF_TRIGGER_FALLING,
			"Fuel Alert irq", NULL);
	if (unlikely(ret < 0))
		pr_err("%s: request fuel alert irq %d failed for gpio %d\n",
			__func__, fuel_irq, GPIO_FUEL_ALERT);
}

static void charger_enble_set(int state)
{
	gpio_set_value(GPIO_TA_EN, !state);
	pr_debug("%s: Set charge status: %d, current status: %d\n",
		__func__, state, !state);
}

static struct i2c_gpio_platform_data espresso_gpio_i2c5_pdata = {
	.udelay = 10,
	.timeout = 0,
	.sda_pin = GPIO_CHG_SDA,
	.scl_pin = GPIO_CHG_SCL,
};

static struct platform_device espresso_gpio_i2c5_device = {
	.name = "i2c-gpio",
	.id = 5,
	.dev = {
		.platform_data = &espresso_gpio_i2c5_pdata,
	}
};

static struct i2c_gpio_platform_data espresso_gpio_i2c7_pdata = {
	.udelay = 3,
	.timeout = 0,
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
};

static struct platform_device espresso_gpio_i2c7_device = {
	.name = "i2c-gpio",
	.id = 7,
	.dev = {
		.platform_data = &espresso_gpio_i2c7_pdata,
	},
};

static void set_chg_state(int cable_type)
{
	omap4_espresso_usb_detected(cable_type);
	omap4_espresso_tsp_ta_detect(cable_type);
}

static const __initdata struct i2c_board_info smb136_i2c[] = {
	{
		I2C_BOARD_INFO("smb136-charger", 0x4D), /* 9A >> 1 */
	},
};

static const __initdata struct i2c_board_info smb347_i2c[] = {
	{
		I2C_BOARD_INFO("smb347-charger", 0x0C >> 1),
	},
};

static struct max17042_platform_data max17042_pdata = {
	.enable_current_sense = true,
	.r_sns = 10000,
	.enable_por_init = false,
};

static const __initdata struct i2c_board_info max17042_i2c[] = {
	{
		I2C_BOARD_INFO("max17042", 0x36),
		.platform_data = &max17042_pdata,
	},
};

int check_charger_type(void)
{
	int cable_type;
	short adc;

	adc = omap4_espresso_get_adc(ADC_CHECK_1);
	cable_type = adc > CABLE_DETECT_VALUE ?
			CABLE_TYPE_AC :
			CABLE_TYPE_USB;

	pr_info("%s: Charger type is [%s], adc = %d\n",
		__func__,
		cable_type == CABLE_TYPE_AC ? "AC" : "USB",
		adc);

	return cable_type;
}

void __init omap4_espresso_charger_init(void)
{
	int ret;

	charger_gpio_init();

	if (!gpio_is_valid(GPIO_TA_NCONNECTED))
		gpio_request(GPIO_TA_NCONNECTED, "TA_nCONNECTED");

	ret = platform_device_register(&espresso_gpio_i2c5_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c5 device register fail\n", __func__);

	ret = platform_device_register(&espresso_gpio_i2c7_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c7 device register fail\n", __func__);

#ifdef CONFIG_BATTERY_MANAGER
	if (board_is_espresso10())
		i2c_register_board_info(5, smb347_i2c, ARRAY_SIZE(smb347_i2c));
	else
		i2c_register_board_info(5, smb136_i2c, ARRAY_SIZE(smb136_i2c));
#endif

	i2c_register_board_info(7, max17042_i2c, ARRAY_SIZE(max17042_i2c));

#ifdef CONFIG_BATTERY_MANAGER
	ret = platform_device_register(&battery_manager_device);
	if (ret < 0)
		pr_err("%s: battery monitor device register fail\n", __func__);
#endif
}
