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
#include <glib.h>
#include <scpi.h>

#include "protocol.h"

static int rs_zvx_scpi_send_rwlocked(const struct sr_dev_inst *sdi,
				const char *format, ...)
{
	va_list args;
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	va_start(args, format);
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_send(scpi, format, args);
	g_mutex_unlock(&devc->rw_mutex);
	va_end(args);

	return ret;
}

static int rohde_schwarz_zvx_get_uint64(const struct sr_dev_inst *sdi,
										const char *command, uint64_t *response)
{
	int ret;
	char *buf;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	buf = NULL;
	ret = sr_scpi_get_string(scpi, command, &buf);
	if (ret != SR_OK)
		return ret;

	*response = atoll(buf);
	g_free(buf);
	return SR_OK;
}

SR_PRIV int rohde_schwarz_zvx_init(struct sr_scpi_dev_inst *scpi)
{
	int ret;

	ret = sr_scpi_send(scpi, "*CLS");
	if (ret != SR_OK)
		return ret;

	return ret;
}

SR_PRIV int rohde_schwarz_zvx_remote(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	/* enable display updates: */
	ret = sr_scpi_send(scpi, "SYST:DISP:UPD ON");
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

//	ret = sr_scpi_send(scpi, "SYST:USER:DISP:TITL 'sigrok controlled'");
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_local(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	/* enable display updates: */
	ret = sr_scpi_send(scpi, "SYST:DISP:UPD ON");
	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

SR_PRIV int rohde_schwarz_zvx_sync(const struct sr_dev_inst *sdi)
{
	int ret;

	if ((ret = rohde_schwarz_zvx_read_frequency(sdi)) != SR_OK)
		return ret;
	if ((ret = rohde_schwarz_zvx_read_span(sdi) != SR_OK))
		return ret;
//	if ((ret = rohde_schwarz_zvx_read_ref_level(sdi) != SR_OK))
//		return ret;
	if ((ret = rohde_schwarz_zvx_read_clk_src_idx(sdi)) != SR_OK)
		return ret;

	return SR_OK;
}

/// ************************** band center ********************************** */
SR_PRIV int rohde_schwarz_zvx_read_frequency(const struct sr_dev_inst *sdi)
{
	int ret;
	double frequency;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	frequency = 0.0;
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "FREQ:CENT?", &frequency);
	g_mutex_unlock(&devc->rw_mutex);
	if (ret != SR_OK)
		return ret;

	if (frequency == 0)
		return SR_ERR;

	devc->frequency = frequency;
	return SR_OK;
}

SR_PRIV int rohde_Schwarz_zvx_minmax_frequency(const struct sr_dev_inst *sdi,
        double *min_freq, double *max_freq)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;


	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "FREQ:CENT? MIN", min_freq);
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	ret = sr_scpi_get_double(scpi, "FREQ:CENT? MAX", max_freq);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_set_frequency(const struct sr_dev_inst *sdi,
        double frequency)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	if (devc->frequency == frequency)
		return SR_OK;

	g_mutex_lock(&devc->rw_mutex);
	if ((ret = sr_scpi_send(scpi, "FREQ:CENT %fHz", devc->frequency)) != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}
	devc->frequency = frequency;

	ret = sr_scpi_get_double(scpi, "FREQ:SPAN?", &devc->span);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

/// ************************** span ***************************************** */
SR_PRIV int rohde_schwarz_zvx_read_span(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "FREQ:SPAN?", &devc->span);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_Schwarz_zvx_minmax_span(const struct sr_dev_inst *sdi,
        double *min_span, double *max_span)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "FREQ:SPAN? MIN", min_span);
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	ret = sr_scpi_get_double(scpi, "FREQ:SPAN? MAX", max_span);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_set_span(const struct sr_dev_inst *sdi,
        double span)
{
	int ret;
	double new_frequency;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	if (devc->span == span)
		return SR_OK;

	devc->span = span;
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_send(scpi, "FREQUENCY:SPAN %fHz", devc->span);
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	new_frequency = 0.0;
	ret = sr_scpi_get_double(scpi, "FREQ:CENT?", &new_frequency);
	g_mutex_unlock(&devc->rw_mutex);
	if (ret != SR_OK)
		return ret;
	if (new_frequency == 0.0)
		return SR_ERR;

	return ret;
}

/// ************************** ref level ************************************ */
/*SR_PRIV int rohde_schwarz_zvx_read_ref_level(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "DISP:TRAC:Y:RLEV?", &devc->ref_level);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_Schwarz_zvx_minmax_ref_level(const struct sr_dev_inst *sdi,
        double *min_ref_level, double *max_ref_level)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_double(scpi, "DISP:TRAC:Y:RLEV? MIN", min_ref_level);
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	ret = sr_scpi_get_double(scpi, "DISP:TRAC:Y:RLEV? MAX", max_ref_level);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_set_ref_level(const struct sr_dev_inst *sdi,
        double ref_level)
{
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	if (devc->ref_level == ref_level)
		return SR_OK;

	devc->ref_level = ref_level;
	return rs_zvx_scpi_send_rwlocked(sdi, "DISP:TRAC:Y:RLEV %fdBm",
									devc->ref_level);
}*/
/// ************************* S-Parameter ************************************ */
SR_PRIV int rohde_schwarz_zvx_set_sparams(const struct sr_dev_inst *sdi,
 char *sparams[], size_t lenght)
{
    int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

    g_mutex_lock(&devc->rw_mutex);
    ret = sr_scpi_send(scpi, "DISP1:TRAC1:DEL");
    if (ret != SR_OK) {
        g_mutex_unlock(&devc->rw_mutex);
        return ret;
    }

    ret = sr_scpi_send(scpi, "CALC:PAR:DEL:ALL");
    if (ret != SR_OK) {
        g_mutex_unlock(&devc->rw_mutex);
        return ret;
    }

    int i = 1;
    char display[19] = "DISP:WINDX:STAT ON";
    char define[30] = "CALCX:PAR:SDEF 'trcXX', 'XXX'";  //S11 or Y11 or Z21...
    char calc[17] = "CALCX:FORM SMITH";
    char feed[30] = "DISP:WINDX:TRAC1:FEED 'trcXX'";
    for(size_t x = 0; x < lenght; ++x) {
        display[offset_window] = i + '0';
        ret = sr_scpi_send(scpi, display);
        if (ret != SR_OK) {
            g_mutex_unlock(&devc->rw_mutex);
            return ret;
        }
        define[offset_calcCH] = i + '1'; //+'2'; // Begin with defining channel 2 as channel 1 has S21 defined when reseted which led to errors
        define[offset_create_trcName1] = sparams[x][1];
        define[offset_create_trcName2] = sparams[x][2];
        for(int sparam_iter = 0; sparam_iter < 3; sparam_iter++)
            define[offset_define_parameter + sparam_iter] = sparams[x][sparam_iter];

        ret = sr_scpi_send(scpi, define);
        if (ret != SR_OK) {
            g_mutex_unlock(&devc->rw_mutex);
            return ret;
        }
        calc[offset_calcCH] = i + '1'; // Also Format channel 2 and counting
        ret = sr_scpi_send(scpi, calc);
        if (ret != SR_OK) {
            g_mutex_unlock(&devc->rw_mutex);
            return ret;
        }
        feed[offset_window] = i + '0';
        feed[offset_assign_trcName1] = sparams[x][1];
        feed[offset_assign_trcName2] = sparams[x][2];

        ret = sr_scpi_send(scpi, feed);
        if (ret != SR_OK) {
            g_mutex_unlock(&devc->rw_mutex);
            return ret;
        }
        i++;
    }
    g_mutex_unlock(&devc->rw_mutex);
	return ret;

}


SR_PRIV int rohde_schwarz_zvx_preset(const struct sr_dev_inst *sdi, gboolean bool)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_send(scpi, "*RST");
	g_mutex_unlock(&devc->rw_mutex);
	if (ret != SR_OK)
		return ret;
    devc->num_sparams = 1; // When resetet only S21 is active
	ret = rohde_schwarz_zvx_sync(sdi);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_get_active_traces(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

    if (devc->received_cmd_str)
		g_free(devc->received_cmd_str);
	devc->received_cmd_str = NULL;

	ret = SR_OK;
	g_mutex_lock(&devc->rw_mutex);

    char *buf;
	gchar **strings;
	devc->active_traces = NULL;
	if ((ret = sr_scpi_get_string(scpi, "CONF:TRAC:CAT?", &devc->active_traces)) != SR_OK) {
		sr_spew("get active Parameter failed!");
		return ret;
	}
	if ((ret = sr_scpi_get_string(scpi, "CONF:TRAC:CAT?", &buf)) != SR_OK) {
		sr_spew("get active Parameter failed!");
		return ret;
	}

	strings = g_strsplit(buf, ",", devc->possible_params*2);
	for (size_t i = 0 ; i < devc->possible_params*2 ; ++i) {
		/* Safe active Traces as Number  */
		gchar *str = strings[i];
		if (str == NULL)
		{
            if((i % 2)) {
                g_mutex_unlock(&devc->rw_mutex);
                g_strfreev(strings);
                g_free(buf);
                sr_spew("Couldn't detect number of active Traces!");
                return SR_ERR;
            }
            devc->num_sparams = i/2;
			break;
		}
	}
	g_strfreev(strings);
	g_free(buf);
	g_mutex_unlock(&devc->rw_mutex);
    return ret;


}

/// ************************* clk source ************************************ */
SR_PRIV int rohde_schwarz_zvx_read_clk_src_idx(const struct sr_dev_inst *sdi)
{
	int ret;
	size_t i;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	char *buf;
	const char *rohde_schwarz_zvx_ref_clk_sources[] = {
		"INT",
		"EXT"
	};

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	buf = NULL;
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_get_string(scpi, "ROSC:SOUR?", &buf);
	g_mutex_unlock(&devc->rw_mutex);
	if (ret != SR_OK)
		return ret;

	ret = SR_ERR;
	for (i = 0; i < ARRAY_SIZE(rohde_schwarz_zvx_ref_clk_sources); ++i) {
		if (g_strcmp0(rohde_schwarz_zvx_ref_clk_sources[i], buf) == 0) {
			devc->clk_source_idx = (i > 1) ? 1 : 0;
			ret = SR_OK;
			break;
		}
	}

	g_free(buf);
	return ret;
}

SR_PRIV int rohde_schwarz_zvx_set_clk_src(const struct sr_dev_inst *sdi,
        size_t idx)
{
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	if (devc->clk_source_idx == idx)
		return SR_OK;

	devc->clk_source_idx = idx;

	return rs_zvx_scpi_send_rwlocked(sdi, "ROSC:SOUR %s",
									 (idx == 0) ? "INT" : "EXT1");
}

static void rohde_schwarz_zvx_send_packet(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;

	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;

	if (!devc)
		return;

	std_session_send_df_frame_begin(sdi);

	sr_analog_init(&analog, &encoding, &meaning, &spec, 10);
	analog.meaning->mq = SR_MQ_N_PORT_PARAMETER;
	analog.meaning->unit = SR_UNIT_UNITLESS;
	analog.meaning->mqflags = SR_MQFLAG_N_PORT_S_PARAMETER;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = devc->data_points/devc->num_sparams;
	analog.data = devc->vals;
	analog.encoding->unitsize = sizeof(double);
	analog.encoding->is_float = TRUE;
	analog.encoding->digits = 10;

	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);


	for(size_t i = 1; i < devc->num_sparams; i++) {
	    analog.data = devc->vals + i*analog.num_samples;
	    sr_session_send(sdi, &packet);
	}

	sr_sw_limits_update_samples_read(&devc->limits, devc->data_points);
	sr_sw_limits_update_frames_read(&devc->limits, 1);

	std_session_send_df_frame_end(sdi);
}

static int rohde_schwarz_zvx_receive_trace(struct sr_scpi_dev_inst *scpi,
        const char *cmd, double *resp, size_t n)
{
	sr_info("rohde_schwarz_zvx_receive_trace. , rohde-schwarz-zvx");
	char *buf;
	int ret;
	gchar **strings;

	buf = NULL;
	if ((ret = sr_scpi_get_string(scpi, cmd, &buf)) != SR_OK) {
		sr_spew("%s failed!", cmd);
		return ret;
	}
	strings = g_strsplit(buf, ",", n);
	for (size_t i = 0 ; i < n ; ++i) {
		/* Work with strings, and, when done: */
		gchar *str = strings[i];
		if (str == NULL) {
			sr_spew("data from trace not enough data after %zu samples", i);
			break;
		}
		*resp++ = atof(str);
	}
	g_strfreev(strings);
	g_free(buf);

	return SR_OK;
}



SR_PRIV int rohde_schwarz_zvx_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	int sweep_points;
	int ret;

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv)|| !(scpi = sdi->conn))
		return TRUE;

	g_mutex_lock(&devc->rw_mutex);
	size_t old_data_points = devc->data_points;

	ret = sr_scpi_get_int(scpi, "SWEEP:POINTS?", &sweep_points);
	if (ret != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return TRUE;
	}
	devc->sweep_points = sweep_points;

	// data_points = defines number of real and imaginary values of all traces
	devc->data_points = sweep_points*2*devc->num_sparams;

	if (old_data_points < devc->data_points) {
		free(devc->vals);
		devc->vals = NULL;
	}

	if (!devc->vals ) {
		if (!devc->vals)
			devc->vals = g_malloc(devc->data_points * sizeof(double));

		if (!devc->vals ) {
			sr_spew("mem allocation for trace data failed!");
			if (devc->vals) {
				g_free(devc->vals);
				devc->vals = NULL;
			}
			g_mutex_unlock(&devc->rw_mutex);
			return TRUE;
		}
	}

	if ((ret = rohde_schwarz_zvx_receive_trace(scpi, "CALC:DATA:DALL? SDATA",
							devc->vals, devc->data_points)) != SR_OK) {
		g_mutex_unlock(&devc->rw_mutex);
		return TRUE;
	}
	g_mutex_unlock(&devc->rw_mutex);

	rohde_schwarz_zvx_send_packet(sdi);

	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}


SR_PRIV int rs_fsw_and_fsv_cmd_set(const struct sr_dev_inst *sdi, const char *cmd)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_scpi_send(scpi, cmd);
	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

SR_PRIV int rs_fsw_and_fsv_cmd_req(const struct sr_dev_inst *sdi, const char *cmd)
{
	int ret;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	if (!(scpi = sdi->conn) || !(devc = sdi->priv))
		return SR_ERR;

	if (devc->received_cmd_str)
		g_free(devc->received_cmd_str);
	devc->received_cmd_str = NULL;

	ret = SR_OK;
	g_mutex_lock(&devc->rw_mutex);
	if ((ret = sr_scpi_get_string(scpi, cmd, &devc->received_cmd_str)) != SR_OK)
		sr_spew("rs_fsw_and_fsv_cmd_req::sr_scpi_get_string() failed!");
	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}
