/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/pinctrl/consumer.h>

#define LED_GPIO_FLASH_DRIVER_NAME	"qcom,leds-gpio-flash"
#define LED_TRIGGER_DEFAULT		"none"

enum msm_flash_seq_type_t {
	FLASH_EN = 0,
	FLASH_NOW = 1,
	ENABLE_MULTI = 2,
	FLASH_SEQ_MAX = 3,
};

struct msm_flash_ctrl_seq {
	uint8_t flash_on_val;
	uint8_t torch_on_val;
};

struct led_gpio_flash_data {
	int flash_en;
	int flash_now;
	int front_en;
	int rear_en;
	bool has_front_en;
	bool has_rear_en;
	int brightness;
	struct led_classdev cdev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_default;
	struct msm_flash_ctrl_seq ctrl_seq[FLASH_SEQ_MAX];
};

static struct of_device_id led_gpio_flash_of_match[] = {
	{.compatible = LED_GPIO_FLASH_DRIVER_NAME,},
	{},
};

static void led_gpio_brightness_set(struct led_classdev *led_cdev,
				    enum led_brightness value)
{
	struct led_gpio_flash_data *flash_led =
	    container_of(led_cdev, struct led_gpio_flash_data, cdev);
	int brightness = value;
	int flash_en = 0, flash_now = 0, multi_en = 0;

	if (brightness > LED_HALF) {
		flash_en = flash_led->ctrl_seq[FLASH_EN].flash_on_val;
		flash_now = flash_led->ctrl_seq[FLASH_NOW].flash_on_val;
		multi_en = flash_led->ctrl_seq[ENABLE_MULTI].flash_on_val;
	} else if (brightness > LED_OFF) {
		flash_en = flash_led->ctrl_seq[FLASH_EN].torch_on_val;
		flash_now = flash_led->ctrl_seq[FLASH_NOW].torch_on_val;
		multi_en = flash_led->ctrl_seq[ENABLE_MULTI].torch_on_val;
	}

	if (flash_led->has_rear_en && gpio_is_valid(flash_led->rear_en))
		gpio_direction_output(flash_led->rear_en, multi_en ? 1 : 0);
	if (flash_led->has_front_en && gpio_is_valid(flash_led->front_en))
		gpio_direction_output(flash_led->front_en, 0);

	gpio_direction_output(flash_led->flash_en, flash_en ? 1 : 0);
	gpio_direction_output(flash_led->flash_now, flash_now ? 1 : 0);
	flash_led->brightness = brightness;
}

static enum led_brightness led_gpio_brightness_get(struct led_classdev *led_cdev)
{
	struct led_gpio_flash_data *flash_led =
	    container_of(led_cdev, struct led_gpio_flash_data, cdev);
	return flash_led->brightness;
}

int led_gpio_flash_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *temp_str;
	struct led_gpio_flash_data *flash_led = NULL;
	struct device_node *node = pdev->dev.of_node;
	const char *seq_name = NULL;
	uint32_t array_flash_seq[FLASH_SEQ_MAX] = {1, 0, 1};
	uint32_t array_torch_seq[FLASH_SEQ_MAX] = {0, 1, 1};
	int seq_cnt, i;

	flash_led = devm_kzalloc(&pdev->dev, sizeof(*flash_led), GFP_KERNEL);
	if (!flash_led)
		return -ENOMEM;

	flash_led->front_en = -EINVAL;
	flash_led->rear_en = -EINVAL;
	flash_led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	if (!of_property_read_string(node, "linux,default-trigger", &temp_str))
		flash_led->cdev.default_trigger = temp_str;

	flash_led->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR_OR_NULL(flash_led->pinctrl)) {
		flash_led->gpio_state_default = pinctrl_lookup_state(
			flash_led->pinctrl, "flash_default");
		if (!IS_ERR_OR_NULL(flash_led->gpio_state_default))
			pinctrl_select_state(flash_led->pinctrl,
					     flash_led->gpio_state_default);
	}

	flash_led->flash_en = of_get_named_gpio(node, "qcom,flash-en", 0);
	if (!gpio_is_valid(flash_led->flash_en))
		return -EINVAL;
	rc = gpio_request(flash_led->flash_en, "FLASH_EN");
	if (rc)
		return rc;

	flash_led->flash_now = of_get_named_gpio(node, "qcom,flash-now", 0);
	if (!gpio_is_valid(flash_led->flash_now)) {
		gpio_free(flash_led->flash_en);
		return -EINVAL;
	}
	rc = gpio_request(flash_led->flash_now, "FLASH_NOW");
	if (rc) {
		gpio_free(flash_led->flash_en);
		return rc;
	}

	if (of_property_read_bool(node, "qcom,flash-front-enable")) {
		flash_led->front_en = of_get_named_gpio(node,
			"qcom,flash-front-enable-gpio", 0);
		if (gpio_is_valid(flash_led->front_en) &&
		    !gpio_request(flash_led->front_en, "FRONT_ENABLE")) {
			gpio_direction_output(flash_led->front_en, 0);
			flash_led->has_front_en = true;
		}
	}

	if (of_property_read_bool(node, "qcom,flash-rear-enable")) {
		flash_led->rear_en = of_get_named_gpio(node,
			"qcom,flash-rear-enable-gpio", 0);
		if (gpio_is_valid(flash_led->rear_en) &&
		    !gpio_request(flash_led->rear_en, "REAR_ENABLE")) {
			gpio_direction_output(flash_led->rear_en, 0);
			flash_led->has_rear_en = true;
		}
	}

	rc = of_property_read_string(node, "linux,name", &flash_led->cdev.name);
	if (rc)
		flash_led->cdev.name = "flashlight";

	seq_cnt = of_property_count_strings(node, "qcom,op-seq");
	if (seq_cnt <= 0)
		seq_cnt = 2;
	if (seq_cnt > FLASH_SEQ_MAX)
		seq_cnt = FLASH_SEQ_MAX;

	of_property_read_u32_array(node, "qcom,flash-seq-val",
				   array_flash_seq, seq_cnt);
	of_property_read_u32_array(node, "qcom,torch-seq-val",
				   array_torch_seq, seq_cnt);

	for (i = 0; i < seq_cnt; i++) {
		if (of_property_read_string_index(node, "qcom,op-seq",
						  i, &seq_name) < 0)
			continue;
		if (!strcmp(seq_name, "flash_en")) {
			flash_led->ctrl_seq[FLASH_EN].flash_on_val = !!array_flash_seq[i];
			flash_led->ctrl_seq[FLASH_EN].torch_on_val = !!array_torch_seq[i];
		} else if (!strcmp(seq_name, "flash_now")) {
			flash_led->ctrl_seq[FLASH_NOW].flash_on_val = !!array_flash_seq[i];
			flash_led->ctrl_seq[FLASH_NOW].torch_on_val = !!array_torch_seq[i];
		} else if (!strcmp(seq_name, "enable_multi")) {
			flash_led->ctrl_seq[ENABLE_MULTI].flash_on_val = !!array_flash_seq[i];
			flash_led->ctrl_seq[ENABLE_MULTI].torch_on_val = !!array_torch_seq[i];
		}
	}

	platform_set_drvdata(pdev, flash_led);
	flash_led->cdev.max_brightness = LED_FULL;
	flash_led->cdev.brightness_set = led_gpio_brightness_set;
	flash_led->cdev.brightness_get = led_gpio_brightness_get;

	rc = led_classdev_register(&pdev->dev, &flash_led->cdev);
	if (rc)
		return rc;

	pr_info("%s: probe successfully (%s)\n", __func__, flash_led->cdev.name);
	return 0;
}

int led_gpio_flash_remove(struct platform_device *pdev)
{
	struct led_gpio_flash_data *flash_led = platform_get_drvdata(pdev);
	led_classdev_unregister(&flash_led->cdev);
	return 0;
}

static struct platform_driver led_gpio_flash_driver = {
	.probe = led_gpio_flash_probe,
	.remove = led_gpio_flash_remove,
	.driver = {
		.name = LED_GPIO_FLASH_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = led_gpio_flash_of_match,
	}
};

static int __init led_gpio_flash_init(void)
{
	return platform_driver_register(&led_gpio_flash_driver);
}

static void __exit led_gpio_flash_exit(void)
{
	platform_driver_unregister(&led_gpio_flash_driver);
}

late_initcall(led_gpio_flash_init);
module_exit(led_gpio_flash_exit);

MODULE_DESCRIPTION("QCOM GPIO LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-msm-gpio-flash");
