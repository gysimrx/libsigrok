/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Aurelien Jacobs <aurel@gnuage.org>
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
#include <string.h>
#include <math.h>
#include "protocol.h"

#define USB_TIMEOUT 500

static const char char_map[128] = {
	[0x20] = '-',
	[0x5F] = '0',
	[0x50] = '1',
	[0x6D] = '2',
	[0x7C] = '3',
	[0x72] = '4',
	[0x3E] = '5',
	[0x3F] = '6',
	[0x54] = '7',
	[0x7F] = '8',
	[0x7E] = '9',
	[0x0F] = 'C',
	[0x27] = 'F',
	[0x0B] = 'L',
	[0x79] = 'd',
	[0x10] = 'i',
	[0x39] = 'o',
};

static int brymen_bm86x_parse_digits(const unsigned char *buf, int length,
                                     char *str, float *floatval,
                                     char *temp_unit, int *digits, int flag)
{
	char c, *p = str;
	int i, ret;

	*digits = INT_MIN;

	if (buf[0] & flag)
		*p++ = '-';
	for (i = 0; i < length; i++) {
		if (i && i < 5 && buf[i+1] & 0x01) {
			*p++ = '.';
			*digits = 0;
		}
		c = char_map[buf[i+1] >> 1];
		if (i == 5 && (c == 'C' || c == 'F'))
			*temp_unit = c;
		else if (c) {
			*p++ = c;
			(*digits)++;
		}
	}
	*p = 0;

	if (*digits < 0)
		*digits = 0;

	if ((ret = sr_atof_ascii(str, floatval))) {
		sr_dbg("invalid float string: '%s'", str);
		return ret;
	}

	return SR_OK;
}

static void brymen_bm86x_parse(unsigned char *buf, float *floatval,
                               struct sr_datafeed_analog *analog)
{
	char str[16], temp_unit;
	int ret1, ret2, digits[2], over_limit;

	ret1 = brymen_bm86x_parse_digits(buf+2, 6, str, &floatval[0],
	                                 &temp_unit, &digits[0], 0x80);
	over_limit = strstr(str, "0L") || strstr(str, "0.L");
	ret2 = brymen_bm86x_parse_digits(buf+9, 4, str, &floatval[1],
	                                 &temp_unit, &digits[1], 0x10);

	/* main display */
	if (ret1 == SR_OK || over_limit) {
		/* SI unit */
		if (buf[8] & 0x01) {
			analog[0].meaning->mq = SR_MQ_VOLTAGE;
			analog[0].meaning->unit = SR_UNIT_VOLT;
			if (!strcmp(str, "diod"))
				analog[0].meaning->mqflags |= SR_MQFLAG_DIODE;
		} else if (buf[14] & 0x80) {
			analog[0].meaning->mq = SR_MQ_CURRENT;
			analog[0].meaning->unit = SR_UNIT_AMPERE;
		} else if (buf[14] & 0x20) {
			analog[0].meaning->mq = SR_MQ_CAPACITANCE;
			analog[0].meaning->unit = SR_UNIT_FARAD;
		} else if (buf[14] & 0x10) {
			analog[0].meaning->mq = SR_MQ_CONDUCTANCE;
			analog[0].meaning->unit = SR_UNIT_SIEMENS;
		} else if (buf[15] & 0x01) {
			analog[0].meaning->mq = SR_MQ_FREQUENCY;
			analog[0].meaning->unit = SR_UNIT_HERTZ;
		} else if (buf[10] & 0x01) {
			analog[0].meaning->mq = SR_MQ_CONTINUITY;
			analog[0].meaning->unit = SR_UNIT_OHM;
		} else if (buf[15] & 0x10) {
			analog[0].meaning->mq = SR_MQ_RESISTANCE;
			analog[0].meaning->unit = SR_UNIT_OHM;
		} else if (buf[15] & 0x02) {
			analog[0].meaning->mq = SR_MQ_POWER;
			analog[0].meaning->unit = SR_UNIT_DECIBEL_MW;
		} else if (buf[15] & 0x80) {
			analog[0].meaning->mq = SR_MQ_DUTY_CYCLE;
			analog[0].meaning->unit = SR_UNIT_PERCENTAGE;
		} else if (buf[ 2] & 0x0A) {
			analog[0].meaning->mq = SR_MQ_TEMPERATURE;
			if (temp_unit == 'F')
				analog[0].meaning->unit = SR_UNIT_FAHRENHEIT;
			else
				analog[0].meaning->unit = SR_UNIT_CELSIUS;
		}

		/* when MIN MAX and AVG are displayed at the same time, remove them */
		if ((buf[1] & 0xE0) == 0xE0)
			buf[1] &= ~0xE0;

		/* AC/DC/Auto flags */
		if (buf[1] & 0x10)  analog[0].meaning->mqflags |= SR_MQFLAG_DC;
		if (buf[2] & 0x01)  analog[0].meaning->mqflags |= SR_MQFLAG_AC;
		if (buf[1] & 0x01)  analog[0].meaning->mqflags |= SR_MQFLAG_AUTORANGE;
		if (buf[1] & 0x08)  analog[0].meaning->mqflags |= SR_MQFLAG_HOLD;
		if (buf[1] & 0x20)  analog[0].meaning->mqflags |= SR_MQFLAG_MAX;
		if (buf[1] & 0x40)  analog[0].meaning->mqflags |= SR_MQFLAG_MIN;
		if (buf[1] & 0x80)  analog[0].meaning->mqflags |= SR_MQFLAG_AVG;
		if (buf[3] & 0x01)  analog[0].meaning->mqflags |= SR_MQFLAG_RELATIVE;

		/* when dBm is displayed, remove the m suffix so that it is
		   not considered as the 10e-3 SI prefix */
		if (buf[15] & 0x02)
			buf[15] &= ~0x04;

		/* SI prefix */
		if (buf[14] & 0x40)  { floatval[0] *= 1e-9; digits[0] += 9; }  /* n */
		if (buf[15] & 0x08)  { floatval[0] *= 1e-6; digits[0] += 6; }  /* µ */
		if (buf[15] & 0x04)  { floatval[0] *= 1e-3; digits[0] += 3; }  /* m */
		if (buf[15] & 0x40)  { floatval[0] *= 1e3;  digits[0] -= 3; }  /* k */
		if (buf[15] & 0x20)  { floatval[0] *= 1e6;  digits[0] -= 6; }  /* M */

		if (over_limit)      floatval[0] = INFINITY;

		analog[0].encoding->digits  = digits[0];
		analog[0].spec->spec_digits = digits[0];
	}

	/* secondary display */
	if (ret2 == SR_OK) {
		/* SI unit */
		if (buf[14] & 0x08) {
			analog[1].meaning->mq = SR_MQ_VOLTAGE;
			analog[1].meaning->unit = SR_UNIT_VOLT;
		} else if (buf[9] & 0x04) {
			analog[1].meaning->mq = SR_MQ_CURRENT;
			analog[1].meaning->unit = SR_UNIT_AMPERE;
		} else if (buf[9] & 0x08) {
			analog[1].meaning->mq = SR_MQ_CURRENT;
			analog[1].meaning->unit = SR_UNIT_PERCENTAGE;
		} else if (buf[14] & 0x04) {
			analog[1].meaning->mq = SR_MQ_FREQUENCY;
			analog[1].meaning->unit = SR_UNIT_HERTZ;
		} else if (buf[9] & 0x40) {
			analog[1].meaning->mq = SR_MQ_TEMPERATURE;
			if (temp_unit == 'F')
				analog[1].meaning->unit = SR_UNIT_FAHRENHEIT;
			else
				analog[1].meaning->unit = SR_UNIT_CELSIUS;
		}

		/* AC flag */
		if (buf[9] & 0x20)  analog[1].meaning->mqflags |= SR_MQFLAG_AC;

		/* SI prefix */
		if (buf[ 9] & 0x01)  { floatval[1] *= 1e-6; digits[1] += 6; }  /* µ */
		if (buf[ 9] & 0x02)  { floatval[1] *= 1e-3; digits[1] += 3; }  /* m */
		if (buf[14] & 0x02)  { floatval[1] *= 1e3;  digits[1] -= 3; }  /* k */
		if (buf[14] & 0x01)  { floatval[1] *= 1e6;  digits[1] -= 6; }  /* M */

		analog[1].encoding->digits  = digits[1];
		analog[1].spec->spec_digits = digits[1];
	}

	if (buf[9] & 0x80)
		sr_spew("Battery is low.");
}

static void brymen_bm86x_handle_packet(const struct sr_dev_inst *sdi,
                                       unsigned char *buf)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog[2];
	struct sr_analog_encoding encoding[2];
	struct sr_analog_meaning meaning[2];
	struct sr_analog_spec spec[2];
	float floatval[2];

	devc = sdi->priv;

	sr_analog_init(&analog[0], &encoding[0], &meaning[0], &spec[0], 0);
	sr_analog_init(&analog[1], &encoding[1], &meaning[1], &spec[1], 0);

	brymen_bm86x_parse(buf, floatval, analog);

	if (analog[0].meaning->mq != 0) {
		/* Got a measurement. */
		analog[0].num_samples = 1;
		analog[0].data = &floatval[0];
		analog[0].meaning->channels = g_slist_append(NULL, sdi->channels->data);
		packet.type = SR_DF_ANALOG;
		packet.payload = &analog[0];
		sr_session_send(sdi, &packet);
		g_slist_free(analog[0].meaning->channels);
	}

	if (analog[1].meaning->mq != 0) {
		/* Got a measurement. */
		analog[1].num_samples = 1;
		analog[1].data = &floatval[1];
		analog[1].meaning->channels = g_slist_append(NULL, sdi->channels->next->data);
		packet.type = SR_DF_ANALOG;
		packet.payload = &analog[1];
		sr_session_send(sdi, &packet);
		g_slist_free(analog[1].meaning->channels);
	}

	if (analog[0].meaning->mq != 0 || analog[1].meaning->mq != 0)
		sr_sw_limits_update_samples_read(&devc->sw_limits, 1);
}

static int brymen_bm86x_send_command(const struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;
	unsigned char buf[] = { 0x00, 0x86, 0x66 };
	int ret;

	usb = sdi->conn;

	sr_dbg("Sending HID set report.");
	ret = libusb_control_transfer(usb->devhdl,
	                              LIBUSB_REQUEST_TYPE_CLASS  |
	                              LIBUSB_RECIPIENT_INTERFACE |
	                              LIBUSB_ENDPOINT_OUT,
	                              9,     /* bRequest: HID set_report */
	                              0x300, /* wValue: HID feature, report num 0 */
	                              0,     /* wIndex: interface 0 */
	                              buf, sizeof(buf), USB_TIMEOUT);

	if (ret < 0) {
		sr_err("HID feature report error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (ret != sizeof(buf)) {
		sr_err("Short packet: sent %d/%zu bytes.", ret, sizeof(buf));
		return SR_ERR;
	}

	return SR_OK;
}

static int brymen_bm86x_read_interrupt(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	unsigned char buf[24];
	int ret, transferred;

	devc = sdi->priv;
	usb = sdi->conn;

	sr_dbg("Reading HID interrupt report.");
	/* Get data from EP1 using an interrupt transfer. */
	ret = libusb_interrupt_transfer(usb->devhdl,
	                                LIBUSB_ENDPOINT_IN | 1, /* EP1, IN */
	                                buf, sizeof(buf),
	                                &transferred, USB_TIMEOUT);

	if (ret == LIBUSB_ERROR_TIMEOUT) {
		if (++devc->interrupt_pending > 3)
			devc->interrupt_pending = 0;
		return SR_OK;
	}

	if (ret < 0) {
		sr_err("USB receive error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (transferred != sizeof(buf)) {
		sr_err("Short packet: received %d/%zu bytes.", transferred, sizeof(buf));
		return SR_ERR;
	}

	devc->interrupt_pending = 0;
	brymen_bm86x_handle_packet(sdi, buf);

	return SR_OK;
}

SR_PRIV int brymen_bm86x_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (!devc->interrupt_pending) {
		if (brymen_bm86x_send_command(sdi))
			return FALSE;
		devc->interrupt_pending = 1;
	}

	if (brymen_bm86x_read_interrupt(sdi))
		return FALSE;

	if (sr_sw_limits_check(&devc->sw_limits))
		sdi->driver->dev_acquisition_stop(sdi);

	return TRUE;
}
