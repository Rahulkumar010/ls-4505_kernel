// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct ili9806e_txd_ips {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline
struct ili9806e_txd_ips *to_ili9806e_txd_ips(struct drm_panel *panel)
{
	return container_of_const(panel, struct ili9806e_txd_ips, panel);
}

static void ili9806e_txd_ips_reset(struct ili9806e_txd_ips *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(120);
}

static int ili9806e_txd_ips_on(struct ili9806e_txd_ips *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff,
				     0xff, 0x98, 0x06, 0x04, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x08, 0x18);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x21, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_PARTIAL_ROWS, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_PARTIAL_COLUMNS,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_VSYNC_TIMING, 0x15);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x41, 0x54);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x42, 0x03);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x43, 0x0b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x44, 0x06);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x46, 0x55);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x50, 0x88);
	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x0088);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x52, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x1f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x60, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x61, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x62, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x63, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa0, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_DDB_START, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_PPS_START, 0x15);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa3, 0x0e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa4, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa5, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa6, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0x04);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_DDB_CONTINUE,
				     0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_PPS_CONTINUE,
				     0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xaa, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xab, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xac, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xad, 0x17);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xae, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xaf, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2, 0x15);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x0e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc4, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc6, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc7, 0x04);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc8, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc9, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xca, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc, 0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd, 0x17);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff,
				     0xff, 0x98, 0x06, 0x04, 0x06);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x01, 0x06);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x02, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x03, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x04, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x05, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x06, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x07, 0x03);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x08, 0x06);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x09, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0a, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0b, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0c, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0d, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0e, 0x09);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x0f, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x10, 0xff);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x11, 0xe0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x12, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x13, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x14, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x15, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x16, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x17, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x18, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x19, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1a, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1b, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1c, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1d, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x20, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x21, 0x23);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x22, 0x45);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x23, 0x67);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x24, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x25, 0x23);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_GAMMA_CURVE, 0x45);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x27, 0x67);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_PARTIAL_ROWS, 0x12);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_PARTIAL_COLUMNS,
				     0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x32, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x33, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x34, 0x87);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x35, 0x96);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_ADDRESS_MODE, 0xaa);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x37, 0xdb);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x38, 0xcc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x39, 0xbd);
	mipi_dsi_dcs_set_pixel_format_multi(&dsi_ctx, 0x78);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x3b, 0x69);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_MEMORY_CONTINUE,
				     0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_3D_CONTROL, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_MEMORY_CONTINUE,
				     0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x3f, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_VSYNC_TIMING, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x52, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff,
				     0xff, 0x98, 0x06, 0x04, 0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x02, 0x77);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1, 0x79);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x18, 0x1d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_GAMMA_CURVE, 0xb2);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff,
				     0xff, 0x98, 0x06, 0x04, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_ADDRESS_MODE, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x11, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x29, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int ili9806e_txd_ips_off(struct ili9806e_txd_ips *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int ili9806e_txd_ips_prepare(struct drm_panel *panel)
{
	struct ili9806e_txd_ips *ctx = to_ili9806e_txd_ips(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ili9806e_txd_ips_reset(ctx);

	ret = ili9806e_txd_ips_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int ili9806e_txd_ips_unprepare(struct drm_panel *panel)
{
	struct ili9806e_txd_ips *ctx = to_ili9806e_txd_ips(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = ili9806e_txd_ips_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode ili9806e_txd_ips_mode = {
	.clock = (480 + 40 + 8 + 36) * (854 + 8 + 2 + 11) * 60 / 1000,
	.hdisplay = 480,
	.hsync_start = 480 + 40,
	.hsync_end = 480 + 40 + 8,
	.htotal = 480 + 40 + 8 + 36,
	.vdisplay = 854,
	.vsync_start = 854 + 8,
	.vsync_end = 854 + 8 + 2,
	.vtotal = 854 + 8 + 2 + 11,
	.width_mm = 0,
	.height_mm = 0,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int ili9806e_txd_ips_get_modes(struct drm_panel *panel,
				      struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &ili9806e_txd_ips_mode);
}

static const struct drm_panel_funcs ili9806e_txd_ips_panel_funcs = {
	.prepare = ili9806e_txd_ips_prepare,
	.unprepare = ili9806e_txd_ips_unprepare,
	.get_modes = ili9806e_txd_ips_get_modes,
};

static int ili9806e_txd_ips_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ili9806e_txd_ips *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct ili9806e_txd_ips, panel,
				   &ili9806e_txd_ips_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void ili9806e_txd_ips_remove(struct mipi_dsi_device *dsi)
{
	struct ili9806e_txd_ips *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ili9806e_txd_ips_of_match[] = {
	{ .compatible = "mdss,ili9806e-txd-ips" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ili9806e_txd_ips_of_match);

static struct mipi_dsi_driver ili9806e_txd_ips_driver = {
	.probe = ili9806e_txd_ips_probe,
	.remove = ili9806e_txd_ips_remove,
	.driver = {
		.name = "panel-ili9806e-txd-ips",
		.of_match_table = ili9806e_txd_ips_of_match,
	},
};
module_mipi_dsi_driver(ili9806e_txd_ips_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ili9806e txd fwvga ips video mode dsi panel");
MODULE_LICENSE("GPL");
