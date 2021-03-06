/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "imx175.h"
#define SENSOR_NAME "imx175"
#define PLATFORM_DRIVER_NAME "msm_camera_imx175"
#define imx175_obj imx175_##obj


DEFINE_MUTEX(imx175_mut);
struct class *camera_class;
static struct msm_sensor_ctrl_t imx175_s_ctrl;
int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);

static struct v4l2_subdev_info imx175_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx175_init_conf[] = {
	{&imx175_recommend_settings[0],
	ARRAY_SIZE(imx175_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx175_confs[] = {
	{&imx175_snap_settings[0],
	ARRAY_SIZE(imx175_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_prev_settings[0],
	ARRAY_SIZE(imx175_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx175_dimensions[] = {
/*Sony 3280x2464,30fps*/
	{
		.x_output = 0xCC4,	// 3268
		.y_output = 0x996,	// 2454
		.line_length_pclk = 0xD54,		// 3412
		.frame_length_lines = 0x9BE,	// 2494
		.vt_pixel_clk = 256200000,
		.op_pixel_clk = 256200000,
		.binning_factor = 1,
	},
/*Sony 3280x2464,30fps*/
	{
		.x_output = 0xCC4,	// 3268
		.y_output = 0x996,	// 2454
		.line_length_pclk = 0xD54,		// 3412
		.frame_length_lines = 0x9BE,	// 2494
		.vt_pixel_clk = 256200000,
		.op_pixel_clk = 256200000,
		.binning_factor = 1,
	},
};
/*
static struct msm_camera_csid_vc_cfg imx175_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx175_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = imx175_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 0x14,
	},
};


static struct msm_camera_csi2_params *imx175_csi_params_array[] = {
	&imx175_csi_params,
	&imx175_csi_params,
};*/

static struct msm_sensor_output_reg_addr_t imx175_reg_addr = {
	.x_output = 0x034c,
	.y_output = 0x034e,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx175_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0175,
};

static struct msm_sensor_exp_gain_info_t imx175_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0205,
	.vert_offset = 4,
};

static const struct i2c_device_id imx175_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx175_s_ctrl},
	{ }
};

static struct i2c_driver imx175_i2c_driver = {
	.id_table = imx175_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx175_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};


int32_t imx175_sensor_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

static ssize_t imx175_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "imx175");
}

static ssize_t imx175_camera_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s\n", "imx175", "imx175");
}
static ssize_t imx175_camera_check_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s %s\n", "imx175", "imx175", "imx175");
}

static DEVICE_ATTR(rear_camtype, S_IRUGO, imx175_camera_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO, imx175_camera_fw_show, NULL);
static DEVICE_ATTR(rear_checkfw, S_IRUGO, imx175_camera_check_fw_show, NULL);


static int __init imx175_sensor_init_module(void)
{
	struct device *cam_dev_rear = NULL;

	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class)) {
		CDBG("Failed to create class(camera)!\n");
		return 0;
	}

	cam_dev_rear =
	device_create(camera_class, NULL, 0, NULL, "rear");
	
	if (device_create_file
	(cam_dev_rear, &dev_attr_rear_camtype) < 0) {
		printk("failed to create device file, %s\n",
		dev_attr_rear_camtype.attr.name);
	}

	if (device_create_file
	(cam_dev_rear, &dev_attr_rear_camfw) < 0) {
		printk("failed to create device file, %s\n",
		dev_attr_rear_camfw.attr.name);
	}

	if (device_create_file
	(cam_dev_rear, &dev_attr_rear_checkfw) < 0) {
		printk("failed to create device file, %s\n",
		dev_attr_rear_camfw.attr.name);
	}

	return i2c_add_driver(&imx175_i2c_driver);
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t imx175_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	printk("%s: %d\n", __func__, __LINE__);

	printk("%s:sensor_power_on %d\n", __func__, __LINE__);



	printk("imx175_sensor_power_up(1) : i2c_scl: %d, i2c_sda: %d\n",
	 gpio_get_value(13), gpio_get_value(12));
	/* Power on */
	data->sensor_platform_info->sensor_power_on();
	mdelay(3);

	/* MCLK */
	printk("MCLK ON!!!");
	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);

	mdelay(5);
	

	/* Power on */
	data->sensor_platform_info->sensor_power_on_sub();
	mdelay(5);

    rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		pr_err("%s: request gpio failed\n", __func__);

	printk("imx175_sensor_power_up(2) : i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(13), gpio_get_value(12));
	
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	printk("imx175_sensor_power_up(3) : i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(13), gpio_get_value(12));

	
	/* MAIN_CAM_RESET */
	printk("MAIN_CAM_RESET!!!");
	data->sensor_platform_info->
		sensor_pmic_gpio_ctrl(data->sensor_platform_info->reset, 1);

	usleep(50);

	printk("imx175_sensor_power_up(4) : i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(13), gpio_get_value(12));	

	return 0;
}

int32_t imx175_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	printk("%s\n", __func__);


	data->sensor_platform_info->sensor_power_off_sub();

	/* MCLK */
	printk("MCLK OFF!!!");

	/* MAIN_CAM_RESET */
	printk("MAIN_CAM_RESET OFF!!!");
	data->sensor_platform_info->
		sensor_pmic_gpio_ctrl(data->sensor_platform_info->reset, 0);

	usleep(50);


	printk("imx175_sensor_power_down(1) : i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(13), gpio_get_value(12));
	

	rc = msm_camera_request_gpio_table(data, 0);
	if (rc < 0)
		pr_err("%s: request gpio failed\n", __func__);

	printk("imx175_sensor_power_down(2) : i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get_value(13), gpio_get_value(12));

	/* Power off */
	data->sensor_platform_info->sensor_power_off();


	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);


	return 0;
}


static struct v4l2_subdev_core_ops imx175_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx175_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx175_subdev_ops = {
	.core = &imx175_subdev_core_ops,
	.video  = &imx175_subdev_video_ops,
};

static struct msm_sensor_fn_t imx175_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx175_sensor_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx175_sensor_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = imx175_sensor_power_up,
	.sensor_power_down = imx175_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx175_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx175_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx175_start_settings),
	.stop_stream_conf = imx175_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx175_stop_settings),
	.group_hold_on_conf = imx175_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx175_groupon_settings),
	.group_hold_off_conf = imx175_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx175_groupoff_settings),
	.init_settings = &imx175_init_conf[0],
	.init_size = ARRAY_SIZE(imx175_init_conf),
	.mode_settings = &imx175_confs[0],
	.output_settings = &imx175_dimensions[0],
	.num_conf = ARRAY_SIZE(imx175_confs),
};

static struct msm_sensor_ctrl_t imx175_s_ctrl = {
	.msm_sensor_reg = &imx175_regs,
	.sensor_i2c_client = &imx175_sensor_i2c_client,
	.sensor_i2c_addr = 0x34,
	.sensor_output_reg_addr = &imx175_reg_addr,
	.sensor_id_info = &imx175_id_info,
	.sensor_exp_gain_info = &imx175_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	/*.csi_params = &imx175_csi_params_array[0],*/
	.msm_sensor_mutex = &imx175_mut,
	.sensor_i2c_driver = &imx175_i2c_driver,
	.sensor_v4l2_subdev_info = imx175_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx175_subdev_info),
	.sensor_v4l2_subdev_ops = &imx175_subdev_ops,
	.func_tbl = &imx175_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};
module_init(imx175_sensor_init_module);
MODULE_DESCRIPTION("Sony 8MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
