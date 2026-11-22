/* MStar MSG2xxx Qualcomm DT & I2C glue for LYF Flame 8 (LS-4505) */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "mstar_drv_platform_interface.h"

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif

#define MSG_TP_IC_NAME "msg2xxx"

struct i2c_client *g_I2cClient = NULL;
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
struct regulator *g_ReguVdd = NULL;
#endif

unsigned global_irq_gpio;
unsigned global_reset_gpio;
struct regulator *TPvdd;
struct regulator *TPvcc_i2c;

static struct kobject *msg_vkey_kobj;

static ssize_t msg_virtualkeys_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"0x01:158:80:904:160:80\n"
		"0x01:172:240:904:160:80\n"
		"0x01:139:400:904:160:80\n");
}

static struct kobj_attribute msg_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.msg2xxx",
		.mode = S_IRUGO,
	},
	.show = &msg_virtualkeys_show,
};

static struct attribute *msg_props_attrs[] = {
	&msg_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group msg_props_attr_group = {
	.attrs = msg_props_attrs,
};

static int tp_msg_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	u32 flags = 0;
	int gpio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;

	if (!np)
		return -EINVAL;

	(void)pinctrl; (void)active_state;

	gpio = of_get_named_gpio_flags(np, "mstar,reset-gpio", 0, &flags);
	if (!gpio_is_valid(gpio))
		gpio = of_get_named_gpio_flags(np, "mstar,reset-gpios", 0, &flags);
	global_reset_gpio = gpio;

	gpio = of_get_named_gpio_flags(np, "mstar,irq-gpio", 0, &flags);
	if (!gpio_is_valid(gpio))
		gpio = of_get_named_gpio_flags(np, "mstar,interrupt-gpios", 0, &flags);
	global_irq_gpio = gpio;

	pr_info("msg2xxx: parsed DT irq_gpio=%u reset_gpio=%u\n",
		global_irq_gpio, global_reset_gpio);

	if (of_property_read_bool(np, "mstar,create-vkeys") && !msg_vkey_kobj) {
		msg_vkey_kobj = kobject_create_and_add("board_properties", NULL);
		if (msg_vkey_kobj) {
			if (sysfs_create_group(msg_vkey_kobj, &msg_props_attr_group))
				pr_err("msg2xxx: failed to create virtualkeys.msg2xxx\n");
		}
	}

	return 0;
}

static int touch_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	const char *vdd_name = "vdd";
#endif
	if (client == NULL)
		return -EINVAL;

	g_I2cClient = client;
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	g_ReguVdd = regulator_get(&g_I2cClient->dev, vdd_name);
#endif
	tp_msg_parse_dt(client);
	return MsDrvInterfaceTouchDeviceProbe(g_I2cClient, id);
}

static int touch_driver_remove(struct i2c_client *client)
{
	return MsDrvInterfaceTouchDeviceRemove(client);
}

static const struct i2c_device_id touch_device_id[] = {
	{MSG_TP_IC_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct of_device_id touch_match_table[] = {
	{ .compatible = "mstar,msg2xxx", },
	{},
};

static struct i2c_driver touch_device_driver = {
	.driver = {
		.name = MSG_TP_IC_NAME,
		.owner = THIS_MODULE,
		.of_match_table = touch_match_table,
	},
	.probe = touch_driver_probe,
	.remove = touch_driver_remove,
	.id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
	int ret = i2c_add_driver(&touch_device_driver);
	if (ret < 0)
		return -ENODEV;
	return ret;
}

static void __exit touch_driver_exit(void)
{
	i2c_del_driver(&touch_device_driver);
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);
