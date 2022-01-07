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

#ifndef LIBSIGROK_HARDWARE_ROHDE_SCHWARZ_ZVX_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ROHDE_SCHWARZ_ZVX_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "rohde-schwarz-zvx"

// Offsets in Arrays used to create Traces in the rohde_schwarz_zvx_set_sparams() function

#define offset_window           9 // Which window/Diagram Area to create

#define offset_calcCH           4 // Channel Number
#define offset_create_trcName1 19 // First Number of name for Trace to be created
#define offset_create_trcName2 20 // Second Number of name for Trace to be created

#define offset_assign_trcName1 26 // First Number of Trace to be assigned to Window
#define offset_assign_trcName2 27 // Second Number of Trace to be assigend

#define offset_define_parameter 25 // Which type of Parameter? e.g S11, Y22 ->Defined by Rohde&Schwarz

struct dev_context {
	GMutex rw_mutex;
	double frequency;          /* Hz */
	double span;               /* Hz */
	//double   ref_level;        /* dBm */
	size_t   sweep_points;
	size_t   data_points;
	size_t   clk_source_idx;
	double   *vals;
	struct sr_sw_limits limits;
	double freq_min;
	double freq_max;
	double span_min;
	double span_max;
	size_t num_sparams;
	size_t possible_params;
	char *received_cmd_str;
	char *active_traces;
	//double ref_level_min;
	//double ref_level_max;
};

SR_PRIV int rohde_schwarz_zvx_receive_data(int fd, int revents, void *cb_data);

SR_PRIV int rohde_schwarz_zvx_init(struct sr_scpi_dev_inst *scpi);
SR_PRIV int rohde_schwarz_zvx_local(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_schwarz_zvx_remote(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_schwarz_zvx_sync(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_Schwarz_zvx_minmax_frequency(const struct sr_dev_inst *sdi,
                                            double *min_freq, double *max_freq);
SR_PRIV int rohde_schwarz_zvx_read_frequency(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_Schwarz_zvx_minmax_span(const struct sr_dev_inst *sdi,
                                          double *min_span, double *max_span);
SR_PRIV int rohde_schwarz_zvx_read_span(const struct sr_dev_inst *sdi);
/*SR_PRIV int rohde_Schwarz_zvx_minmax_ref_level(const struct sr_dev_inst *sdi,
                                double *min_ref_level, double *max_ref_level);
SR_PRIV int rohde_schwarz_zvx_read_ref_level(const struct sr_dev_inst *sdi);*/
SR_PRIV int rohde_schwarz_zvx_preset(const struct sr_dev_inst *sdi, gboolean bool);
SR_PRIV int rohde_schwarz_zvx_get_active_traces(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_schwarz_zvx_read_clk_src_idx(const struct sr_dev_inst *sdi);
SR_PRIV int rohde_schwarz_zvx_set_frequency(const struct sr_dev_inst *sdi,
                                            double freq);
SR_PRIV int rohde_schwarz_zvx_set_span(const struct sr_dev_inst *sdi,
                                       double span);
/*SR_PRIV int rohde_schwarz_zvx_set_ref_level(const struct sr_dev_inst *sdi,
                                            double ref_level);*/
SR_PRIV int rohde_schwarz_zvx_set_clk_src(const struct sr_dev_inst *sdi,
                                             size_t idx);

SR_PRIV int rs_fsw_and_fsv_cmd_set(const struct sr_dev_inst *sdi,
				const char *cmd);
SR_PRIV int rs_fsw_and_fsv_cmd_req(const struct sr_dev_inst *sdi,
				const char *cmd);

SR_PRIV int rohde_schwarz_zvx_set_sparams(const struct sr_dev_inst *sdi,
					char *sparams[], size_t lenght);
#endif
