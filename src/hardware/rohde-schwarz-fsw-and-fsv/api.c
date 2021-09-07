/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 Daniel Anselmi <danselmi@gmx.ch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <scpi.h>
#include <string.h>

#include "protocol.h"

static struct sr_dev_driver rohde_schwarz_fsw_and_fsv_driver_info;

static const char *manufacturer = "Rohde&Schwarz";

static const char *device_models[] = {
	"FSV-3",
	"FSV-4",
	"FSV-7",
	"FSV-13",
	"FSV-30",
	"FSV-40",
	"FSW-8",
	"FSW-13",
	"FSW-26",
	"FSW-43",
	"FSW-50",
	"FSW-67",
	"FSW-85",
};

static const uint64_t rbws_fsv[] = {
	1, 2, 3, 5,
	10, 20, 30, 50,
	100, 200, 300, 500,
	1000, 2000, 3000, 5000, 6250,
	10000, 20000, 30000, 50000,
	100000, 200000, 300000, 500000,
	1000000, 2000000, 3000000, 5000000,
	10000000
};

static const uint64_t vbws_fsv[] = {
	1, 2, 3, 5,
	10, 20, 30, 50,
	100, 200, 300, 500,
	1000, 2000, 3000, 5000,
	10000, 20000, 30000, 50000,
	100000, 200000, 300000, 500000,
	1000000, 2000000, 3000000, 5000000,
	10000000, 20000000, 28000000
};

static const uint64_t rbws_fsw[] = {
	1, 2, 3, 5,
	10, 20, 30, 50,
	100, 200, 300, 500,
	1000, 2000, 3000, 5000,
	10000, 20000, 30000, 50000,
	100000, 200000, 300000, 500000,
	1000000, 2000000, 3000000, 5000000,
	10000000
};

static const uint64_t vbws_fsw[] = {
	1, 2, 3, 5,
	10, 20, 30, 50,
	100, 200, 300, 500,
	1000, 2000, 3000, 5000,
	10000, 20000, 30000, 50000,
	100000, 200000, 300000, 500000,
	1000000, 2000000, 3000000, 5000000,
	10000000, 20000000, 28000000,
	40000000, 50000000, 80000000
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_SPECTRUM_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,

	SR_CONF_PRESET                | SR_CONF_SET,

	SR_CONF_LIMIT_MSEC            | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_FRAMES          | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC            | SR_CONF_GET | SR_CONF_SET,

	SR_CONF_BAND_CENTER_FREQUENCY | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_SPAN                  | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_RESOLUTION_BANDWIDTH  | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_VIDEO_BANDWIDTH       | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,

	SR_CONF_REF_LEVEL             | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,

	SR_CONF_EXTERNAL_CLOCK_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,

	SR_CONF_COMMAND_SET           | SR_CONF_SET,
	SR_CONF_COMMAND_REQ           | SR_CONF_GET | SR_CONF_SET
};

static const char *clock_sources[] = {
	"Internal",
	"External",
};

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;
	uint8_t model_found;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (rs_fsw_and_fsv_init(scpi) != SR_OK)
		goto fail;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK)
		goto fail;

	if (strcmp(hw_info->manufacturer, manufacturer) != 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &rohde_schwarz_fsw_and_fsv_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	g_mutex_init(&devc->rw_mutex);
	sdi->priv = devc;

	model_found = 0;
	for (size_t i = 0; i < ARRAY_SIZE(device_models); i++) {
		if (!strcmp(device_models[i], sdi->model)) {
			model_found = 1;
			break;
		}
	}
	if (!model_found) {
		sr_dbg("Device %s %s is not supported by this driver.",
			manufacturer, sdi->model);
		goto fail;
	}

	if (sdi->model[2] == 'V') {
		devc->rbws = rbws_fsv;
		devc->num_rbws = ARRAY_SIZE(rbws_fsv);
		devc->vbws = vbws_fsv;
		devc->num_vbws = ARRAY_SIZE(vbws_fsv);
	}
	else if (sdi->model[2] == 'W') {
		devc->rbws = rbws_fsw;
		devc->num_rbws = ARRAY_SIZE(rbws_fsw);
		devc->vbws = vbws_fsw;
		devc->num_vbws = ARRAY_SIZE(vbws_fsw);
	}
	else {
		devc->rbws = NULL;
		devc->num_rbws = 0;
		devc->vbws = NULL;
		devc->num_vbws = 0;
	}

	if (rs_fsw_and_fsv_minmax_frequency(sdi, &devc->freq_min,
											&devc->freq_max) != SR_OK)
		goto fail;

	if (rs_fsw_and_fsv_minmax_span(sdi, &devc->span_min,
									&devc->span_max) != SR_OK)
		goto fail;

	if (rs_fsw_and_fsv_minmax_ref_level(sdi, &devc->ref_level_min,
											&devc->ref_level_max) != SR_OK)
		goto fail;

	if (rs_fsw_and_fsv_sync(sdi) != SR_OK)
		goto fail;

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "CH1");

	return sdi;

fail:
	sr_scpi_hw_info_free(hw_info);
	sr_dev_inst_free(sdi);
	g_mutex_clear(&devc->rw_mutex);
	g_free(devc);
	return NULL;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	return sr_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	int ret;

	scpi = sdi->conn;
	ret = sr_scpi_open(scpi);
	if (ret < 0) {
		sr_err("Failed to open SCPI device: %s.", sr_strerror(ret));
		return SR_ERR;
	}

	if ((ret = rs_fsw_and_fsv_remote(sdi)) != SR_OK)
		return ret;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	scpi = sdi->conn;
	devc = sdi->priv;
	if (!sdi || !scpi || !devc)
		return SR_ERR_BUG;

	sr_dbg("DIAG: sdi->status %d.", sdi->status - SR_ST_NOT_FOUND);
	if (sdi->status <= SR_ST_INACTIVE)
		return SR_OK;

	rs_fsw_and_fsv_local(sdi);

	if (devc->received_cmd_str)
		g_free(devc->received_cmd_str);

	if (devc) {
		if(devc->vals)
			g_free(devc->vals);
		devc->vals = NULL;

		g_mutex_clear(&devc->rw_mutex);
	}

	return sr_scpi_close(scpi);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR;
	devc = sdi->priv;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_CONN:
		*data = g_variant_new_string(sdi->connection_id);
		break;
	case SR_CONF_BAND_CENTER_FREQUENCY:
		*data = g_variant_new_double(devc->frequency);
		break;
	case SR_CONF_SPAN:
		*data = g_variant_new_double(devc->span);
		break;
	case SR_CONF_REF_LEVEL:
		*data = g_variant_new_double(devc->ref_level);
		break;
	case SR_CONF_RESOLUTION_BANDWIDTH:
		*data = g_variant_new_uint64(devc->rbw);
		break;
	case SR_CONF_VIDEO_BANDWIDTH:
		*data = g_variant_new_uint64(devc->vbw);
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		*data = g_variant_new_string(clock_sources[devc->clk_source_idx]);
		break;
	case SR_CONF_COMMAND_REQ:
		if (devc->received_cmd_str) {
			*data = g_variant_new_string(devc->received_cmd_str);
			break;
		}
		ret = SR_ERR_NA;
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	size_t i;
	struct dev_context *devc;
	const char *str_param;
	uint64_t uival;
	double dval;
	(void)cg;

	devc = sdi->priv;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_PRESET:
		return rs_fsw_and_fsv_preset(sdi);
	case SR_CONF_BAND_CENTER_FREQUENCY:
		dval = g_variant_get_double(data);
		ret = rs_fsw_and_fsv_set_frequency(sdi, dval);
		break;
	case SR_CONF_SPAN:
		dval = g_variant_get_double(data);
		ret = rs_fsw_and_fsv_set_span(sdi, dval);
		break;
	case SR_CONF_REF_LEVEL:
		dval = g_variant_get_double(data);
		ret = rs_fsw_and_fsv_set_ref_level(sdi, dval);
		break;
	case SR_CONF_RESOLUTION_BANDWIDTH:
		uival = g_variant_get_uint64(data);
		ret = rs_fsw_and_fsv_set_rbw(sdi, uival);
		break;
	case SR_CONF_VIDEO_BANDWIDTH:
		uival = g_variant_get_uint64(data);
		ret = rs_fsw_and_fsv_set_vbw(sdi, uival);
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		str_param = g_variant_get_string(data, NULL);
		for (i = 0; i < ARRAY_SIZE(clock_sources); i++)
			if (g_strcmp0(clock_sources[i], str_param) == 0) {
				ret = rs_fsw_and_fsv_set_clk_src(sdi, i);
				break;
			}
		break;
	case SR_CONF_COMMAND_SET:
		str_param = g_variant_get_string(data, NULL);
		ret = rs_fsw_and_fsv_cmd_set(sdi, str_param);
		break;
	case SR_CONF_COMMAND_REQ:
		str_param = g_variant_get_string(data, NULL);
		ret = rs_fsw_and_fsv_cmd_req(sdi, str_param);
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		if (!cg)
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts,
									drvopts, devopts);
		if (!devc)
			return SR_ERR_ARG;
		break;
	case SR_CONF_REF_LEVEL:
		*data = std_gvar_min_max_step(devc->ref_level_min,
									  devc->ref_level_max, 0.01);
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		*data = std_gvar_array_str(ARRAY_AND_SIZE(clock_sources));
		break;
	case SR_CONF_RESOLUTION_BANDWIDTH:
		*data = std_gvar_array_u64(devc->rbws, devc->num_rbws);
		break;
	case SR_CONF_VIDEO_BANDWIDTH:
		*data = std_gvar_array_u64(devc->vbws, devc->num_vbws);
		break;
	case SR_CONF_BAND_CENTER_FREQUENCY:
		*data = std_gvar_min_max_step(devc->freq_min,
									  devc->freq_max, 0.01);
		break;
	case SR_CONF_SPAN:
		*data = std_gvar_min_max_step(devc->span_min,
									  devc->span_max, 0.01);
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	int ret;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;

	sr_sw_limits_acquisition_start(&devc->limits);

	ret = rs_fsw_and_fsv_sync(sdi);
	if (ret != SR_OK)
		return ret;

	ret = std_session_send_df_header(sdi);
	if (ret != SR_OK)
		return ret;

	ret = sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 10,
		rs_fsw_and_fsv_receive_data, (void *)sdi);

	return ret;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	sr_scpi_source_remove(sdi->session, sdi->conn);

	std_session_send_df_end(sdi);

	return SR_OK;
}

static struct sr_dev_driver rohde_schwarz_fsw_and_fsv_driver_info = {
	.name = "rohde-schwarz-fsw-and-fsv",
	.longname = "Rohde&Schwarz FSW and FSV",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(rohde_schwarz_fsw_and_fsv_driver_info);

