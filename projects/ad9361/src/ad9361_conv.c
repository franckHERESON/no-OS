
/***************************************************************************//**
 *   @file   ad9361_conv.c
 *   @brief  Implementation of AD9361 Conv Driver.
********************************************************************************
 * Copyright 2014-2015(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include <inttypes.h>
#include <string.h>
#include "ad9361.h"
#include "delay.h"
#include "config.h"
#include "ad9361_util.h"
#include "util.h"
#include "axi_adc_core.h"

#ifndef AXI_ADC_NOT_PRESENT

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
#define ADI_REG_VERSION			0x0000

#define ADI_REG_ID			0x0004

#define ADI_REG_RSTN			0x0040
#define ADI_RSTN			(1 << 0)
#define ADI_MMCM_RSTN			(1 << 1)

#define ADI_REG_CNTRL			0x0044
#define ADI_R1_MODE			(1 << 2)
#define ADI_DDR_EDGESEL			(1 << 1)
#define ADI_PIN_MODE			(1 << 0)

#define ADI_REG_STATUS			0x005C
#define ADI_MUX_PN_ERR			(1 << 3)
#define ADI_MUX_PN_OOS			(1 << 2)
#define ADI_MUX_OVER_RANGE		(1 << 1)
#define ADI_STATUS			(1 << 0)

#define ADI_REG_DELAY_CNTRL		0x0060	/* <= v8.0 */
#define ADI_DELAY_SEL			(1 << 17)
#define ADI_DELAY_RWN			(1 << 16)
#define ADI_DELAY_ADDRESS(x)		(((x) & 0xFF) << 8)
#define ADI_TO_DELAY_ADDRESS(x)		(((x) >> 8) & 0xFF)
#define ADI_DELAY_WDATA(x)		(((x) & 0x1F) << 0)
#define ADI_TO_DELAY_WDATA(x)		(((x) >> 0) & 0x1F)

#define ADI_REG_CHAN_CNTRL(c)		(0x0400 + (c) * 0x40)
#define ADI_PN_SEL			(1 << 10) /* !v8.0 */
#define ADI_IQCOR_ENB			(1 << 9)
#define ADI_DCFILT_ENB			(1 << 8)
#define ADI_FORMAT_SIGNEXT		(1 << 6)
#define ADI_FORMAT_TYPE			(1 << 5)
#define ADI_FORMAT_ENABLE		(1 << 4)
#define ADI_PN23_TYPE			(1 << 1) /* !v8.0 */
#define ADI_ENABLE			(1 << 0)

#define ADI_REG_CHAN_STATUS(c)		(0x0404 + (c) * 0x40)
#define ADI_PN_ERR			(1 << 2)
#define ADI_PN_OOS			(1 << 1)
#define ADI_OVER_RANGE			(1 << 0)

#define ADI_REG_CHAN_CNTRL_1(c)		(0x0410 + (c) * 0x40)
#define ADI_DCFILT_OFFSET(x)		(((x) & 0xFFFF) << 16)
#define ADI_TO_DCFILT_OFFSET(x)		(((x) >> 16) & 0xFFFF)
#define ADI_DCFILT_COEFF(x)		(((x) & 0xFFFF) << 0)
#define ADI_TO_DCFILT_COEFF(x)		(((x) >> 0) & 0xFFFF)

#define ADI_REG_CHAN_CNTRL_2(c)		(0x0414 + (c) * 0x40)
#define ADI_IQCOR_COEFF_1(x)		(((x) & 0xFFFF) << 16)
#define ADI_TO_IQCOR_COEFF_1(x)		(((x) >> 16) & 0xFFFF)
#define ADI_IQCOR_COEFF_2(x)		(((x) & 0xFFFF) << 0)
#define ADI_TO_IQCOR_COEFF_2(x)		(((x) >> 0) & 0xFFFF)

#define PCORE_VERSION(major, minor, letter) ((major << 16) | (minor << 8) | letter)
#define PCORE_VERSION_MAJOR(version)	(version >> 16)
#define PCORE_VERSION_MINOR(version)	((version >> 8) & 0xff)
#define PCORE_VERSION_LETTER(version)	(version & 0xff)

#define ADI_REG_CHAN_CNTRL_3(c)		(0x0418 + (c) * 0x40) /* v8.0 */
#define ADI_ADC_PN_SEL(x)		(((x) & 0xF) << 16)
#define ADI_TO_ADC_PN_SEL(x)		(((x) >> 16) & 0xF)
#define ADI_ADC_DATA_SEL(x)		(((x) & 0xF) << 0)
#define ADI_TO_ADC_DATA_SEL(x)		(((x) >> 0) & 0xF)

/* PCORE Version > 8.00 */
#define ADI_REG_DELAY(l)		(0x0800 + (l) * 0x4)

#define SUCCESS		0
#define FAILURE		-1

#define	SPI_CPHA	0x01
#define	SPI_CPOL	0x02

#define	SPI_CS_DECODE	0x01

#define GPIO_OUT	0x01
#define GPIO_IN		0x00

#define GPIO_HIGH	0x01
#define GPIO_LOW	0x00

enum adc_pn_sel {
	ADC_PN9 = 0,
	ADC_PN23A = 1,
	ADC_PN7 = 4,
	ADC_PN15 = 5,
	ADC_PN23 = 6,
	ADC_PN31 = 7,
	ADC_PN_CUSTOM = 9,
	ADC_PN_END = 10,
};

/**
 * Get the number of PHY channels.
 * @return The number of PHY channels.
 */
static uint32_t ad9361_num_phy_chan(struct axiadc_converter *conv)
{
	if (conv->chip_info->num_channels > 4)
		return 4;
	return conv->chip_info->num_channels;
}

/**
 * Check PN checker status.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_check_pn(struct ad9361_rf_phy *phy, bool tx,
			       uint32_t delay)
{
	struct axiadc_converter *conv = phy->adc_conv;
	struct axi_adc *axi_adc = phy->rx_adc;
	uint32_t num_chan = ad9361_num_phy_chan(conv);
	uint32_t chan;

	for (chan = 0; chan < num_chan; chan++)
		axi_adc_write(axi_adc, ADI_REG_CHAN_STATUS(chan),
			      ADI_PN_ERR | ADI_PN_OOS);
	mdelay(delay);
	uint32_t adi_reg_status;
	axi_adc_read(axi_adc, ADI_REG_STATUS, &adi_reg_status);
	if (!tx && !(adi_reg_status & ADI_STATUS))
		return 1;

	for (chan = 0; chan < num_chan; chan++) {
		uint32_t adi_reg_chan_status;
		axi_adc_read(axi_adc, ADI_REG_CHAN_STATUS(chan), &adi_reg_chan_status);
		if (adi_reg_chan_status)
			return 1;
	}

	return 0;
}

/**
 * HDL loopback enable/disable.
 * @param phy The AD9361 state structure.
 * @param enable Enable/disable option.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad9361_hdl_loopback(struct ad9361_rf_phy *phy, bool enable)
{
	struct axiadc_converter *conv = phy->adc_conv;
	struct axi_adc *rx_adc = phy->rx_adc;
	uint32_t reg, addr, chan;

	uint32_t version;
	axi_adc_read(rx_adc, 0x4000, &version);

	/* Still there but implemented a bit different */
	if (PCORE_VERSION_MAJOR(version) > 7)
		addr = 0x4418;
	else
		addr = 0x4414;

	for (chan = 0; chan < conv->chip_info->num_channels; chan++) {
		axi_adc_read(rx_adc, addr + (chan) * 0x40, &reg);

		if (PCORE_VERSION_MAJOR(version) > 7) {
			if (enable && reg != 0x8) {
				conv->scratch_reg[chan] = reg;
				reg = 0x8;
			} else if (reg == 0x8) {
				reg = conv->scratch_reg[chan];
			}
		} else {
			/* DAC_LB_ENB If set enables loopback of receive data */
			if (enable)
				reg |= BIT(1);
			else
				reg &= ~BIT(1);
		}
		axi_adc_write(rx_adc, addr + (chan) * 0x40, reg);
	}

	return 0;
}

/**
 * Set IO delay.
 * @param st The AXI ADC state structure.
 * @param lane Lane number.
 * @param val Value.
 * @param tx The Synthesizer TX = 1, RX = 0.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_iodelay_set(struct axiadc_state *st, unsigned lane,
				  unsigned val, bool tx)
{
	if (tx) {
		if (PCORE_VERSION_MAJOR(st->pcore_version) > 8)
			axi_adc_write(st->phy->rx_adc, 0x4000 + ADI_REG_DELAY(lane), val);
		else
			return -ENODEV;
	} else {
		axi_adc_idelay_set(st->phy->rx_adc, lane, val);
	}

	return 0;
}

/**
 * Set midscale IO delay.
 * @param phy The AD9361 state structure.
 * @param tx The Synthesizer TX = 1, RX = 0.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_midscale_iodelay(struct ad9361_rf_phy *phy, bool tx)
{
	struct axiadc_state *st = phy->adc_state;
	int32_t ret = 0, i;

	for (i = 0; i < 7; i++)
		ret |= ad9361_iodelay_set(st, i, 15, tx);

	return 0;
}

/**
 * Digital tune IO delay.
 * @param phy The AD9361 state structure.
 * @param tx The Synthesizer TX = 1, RX = 0.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_dig_tune_iodelay(struct ad9361_rf_phy *phy, bool tx)
{
	struct axiadc_state *st = phy->adc_state;
	int32_t i, j;
	uint32_t s0, c0;
	uint8_t field[32];

	for (i = 0; i < 7; i++) {
		for (j = 0; j < 32; j++) {
			ad9361_iodelay_set(st, i, j, tx);
			mdelay(1);
			field[j] = ad9361_check_pn(phy, tx, 10);
		}

		c0 = ad9361_find_opt(&field[0], 32, &s0);
		ad9361_iodelay_set(st, i, s0 + c0 / 2, tx);

		dev_dbg(&phy->spi->dev,
			"%s Lane %"PRId32", window cnt %"PRIu32" , start %"PRIu32", IODELAY set to %"PRIu32"\n",
			tx ? "TX" :"RX",  i, c0, s0, s0 + c0 / 2);
	}

	return 0;
}

/**
 * Digital tune verbose print.
 * @param phy The AD9361 state structure.
 * @param field Field.
 * @param tx The Synthesizer TX = 1, RX = 0.
 * @return 0 in case of success, negative error code otherwise.
 */
static void ad9361_dig_tune_verbose_print(struct ad9361_rf_phy *phy,
		uint8_t field[][16], bool tx,
		int32_t sel_clk, int32_t sel_data)
{
	int32_t i, j;
	char c;

	printk("SAMPL CLK: %"PRIu32" tuning: %s\n",
	       clk_get_rate(phy, phy->ref_clk_scale[RX_SAMPL_CLK]), tx ? "TX" : "RX");
	printk("  ");
	for (i = 0; i < 16; i++)
		printk("%"PRIx32":", i);
	printk("\n");

	for (i = 0; i < 2; i++) {
		printk("%"PRIx32":", i);
		for (j = 0; j < 16; j++) {
			if (field[i][j])
				c = '#';
			else if ((i == 0 && j == sel_data) ||
				 (i == 1 && j == sel_clk))
				c = 'O';
			else
				c = 'o';
			printk("%c ", c);
		}
		printk("\n");
	}
}

/**
 * Set intf delay.
 * @param phy The AD9361 state structure.
 * @return None.
 */
static void ad9361_set_intf_delay(struct ad9361_rf_phy *phy, bool tx,
				  uint32_t clock_delay,
				  uint32_t data_delay, bool clock_changed)
{
	if (clock_changed)
		ad9361_ensm_force_state(phy, ENSM_STATE_ALERT);
	ad9361_spi_write(phy->spi,
			 REG_RX_CLOCK_DATA_DELAY + (tx ? 1 : 0),
			 RX_DATA_DELAY(data_delay) |
			 DATA_CLK_DELAY(clock_delay));
	if (clock_changed)
		ad9361_ensm_force_state(phy, ENSM_STATE_FDD);
}

/**
 * Digital interface timing analysis.
 * @param phy The AD9361 state structure.
 * @param buf The buffer.
 * @param buflen The buffer length.
 * @return The size in case of success, negative error code otherwise.
 */
int32_t ad9361_dig_interface_timing_analysis(struct ad9361_rf_phy *phy,
		char *buf, int32_t buflen)
{
	uint32_t loopback, bist, ensm_state;
	int32_t i, j, len = 0;
	uint8_t field[16][16];
	uint8_t rx;

	dev_dbg(&phy->spi->dev, "%s:\n", __func__);

	loopback = phy->bist_loopback_mode;
	bist = phy->bist_config;
	ensm_state = ad9361_ensm_get_state(phy);
	rx = ad9361_spi_read(phy->spi, REG_RX_CLOCK_DATA_DELAY);

	/* Mute TX, we don't want to transmit the PRBS */
	ad9361_tx_mute(phy, 1);

	if (!phy->pdata->fdd)
		ad9361_set_ensm_mode(phy, true, false);

	ad9361_bist_loopback(phy, 0);
	ad9361_bist_prbs(phy, BIST_INJ_RX);

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			ad9361_set_intf_delay(phy, false, i, j, j == 0);
			field[j][i] = ad9361_check_pn(phy, false, 1);
		}
	}

	ad9361_ensm_force_state(phy, ENSM_STATE_ALERT);
	ad9361_spi_write(phy->spi, REG_RX_CLOCK_DATA_DELAY, rx);
	ad9361_bist_loopback(phy, loopback);
	ad9361_spi_write(phy->spi, REG_BIST_CONFIG, bist);

	if (!phy->pdata->fdd)
		ad9361_set_ensm_mode(phy, phy->pdata->fdd, phy->pdata->ensm_pin_ctrl);
	ad9361_ensm_restore_state(phy, ensm_state);

	ad9361_tx_mute(phy, 0);

	len += snprintf(buf + len, buflen, "CLK: %"PRIu32" Hz 'o' = PASS\n",
			clk_get_rate(phy, phy->ref_clk_scale[RX_SAMPL_CLK]));
	len += snprintf(buf + len, buflen, "DC");
	for (i = 0; i < 16; i++)
		len += snprintf(buf + len, buflen, "%"PRIx32":", i);
	len += snprintf(buf + len, buflen, "\n");

	for (i = 0; i < 16; i++) {
		len += snprintf(buf + len, buflen, "%"PRIx32":", i);
		for (j = 0; j < 16; j++) {
			len += snprintf(buf + len, buflen, "%c ",
					(field[i][j] ? '.' : 'o'));
		}
		len += snprintf(buf + len, buflen, "\n");
	}
	len += snprintf(buf + len, buflen, "\n");

	return len;
}

/**
 * Digital tune delay.
 * @param phy The AD9361 state structure.
 * @param max_freq Maximum frequency.
 * @param flags Flags: BE_VERBOSE, BE_MOREVERBOSE, DO_IDELAY, DO_ODELAY.
 * @param tx Set if TX.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_dig_tune_delay(struct ad9361_rf_phy *phy,
				     uint32_t max_freq, enum dig_tune_flags flags, bool tx)
{
	static const uint32_t rates[3] = {25000000U, 40000000U, 61440000U};
	uint32_t s0, s1, c0, c1;
	uint32_t i, j, r;
	bool half_data_rate;
	uint8_t field[2][16];

	if (((phy->pdata->port_ctrl.pp_conf[2] & LVDS_MODE) ||
	     !phy->pdata->rx2tx2))
		half_data_rate = false;
	else
		half_data_rate = true;

	memset(field, 0, 32);
	for (r = 0; r < (max_freq ? ARRAY_SIZE(rates) : 1); r++) {
		if (max_freq)
			ad9361_set_trx_clock_chain_freq(phy,
							half_data_rate ? rates[r] / 2 : rates[r]);

		for (i = 0; i < 2; i++) {
			for (j = 0; j < 16; j++) {
				/*
				 * i == 0: clock delay = 0, data delay from 0 to 15
				 * i == 1: clock delay = 15, data delay from 15 to 0
				 */
				ad9361_set_intf_delay(phy, tx, i ? 15 : 0,
						      i ? 15 - j : j, j == 0);
				field[i][j] |= ad9361_check_pn(phy, tx, 4);
			}
		}

		if ((flags & BE_MOREVERBOSE) && max_freq) {
			ad9361_dig_tune_verbose_print(phy, field, tx, -1, -1);
		}
	}

	c0 = ad9361_find_opt(&field[0][0], 16, &s0);
	c1 = ad9361_find_opt(&field[1][0], 16, &s1);

	if (!c0 && !c1) {
		ad9361_dig_tune_verbose_print(phy, field, tx, -1, -1);
		dev_err(&phy->spi->dev, "%s: Tuning %s FAILED!", __func__,
			tx ? "TX" : "RX");
		return -EIO;
	} else if (flags & BE_VERBOSE) {
		if (c1 > c0)
			ad9361_dig_tune_verbose_print(phy, field, tx, (s1 + c1 / 2), -1);
		else
			ad9361_dig_tune_verbose_print(phy, field, tx, -1, (s0 + c0 / 2));
	}

	if (c1 > c0)
		ad9361_set_intf_delay(phy, tx, s1 + c1 / 2, 0, true);
	else
		ad9361_set_intf_delay(phy, tx, 0, s0 + c0 / 2, true);

	return 0;
}

/**
 * Digital tune RX.
 * @param phy The AD9361 state structure.
 * @param max_freq Maximum frequency.
 * @param flags Flags: BE_VERBOSE, BE_MOREVERBOSE, DO_IDELAY, DO_ODELAY.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_dig_tune_rx(struct ad9361_rf_phy *phy, uint32_t max_freq,
				  enum dig_tune_flags flags)
{
	struct axi_adc *rx_adc = phy->rx_adc;
	int32_t ret;

	ad9361_bist_loopback(phy, 0);
	ad9361_bist_prbs(phy, BIST_INJ_RX);

	ret = ad9361_dig_tune_delay(phy, max_freq, flags, false);
	if (flags & DO_IDELAY)
		ad9361_dig_tune_iodelay(phy, false);

	axi_adc_write(rx_adc, ADI_REG_RSTN, ADI_MMCM_RSTN);
	axi_adc_write(rx_adc, ADI_REG_RSTN, ADI_RSTN | ADI_MMCM_RSTN);

	return ret;
}

/**
 * Digital tune TX.
 * @param phy The AD9361 state structure.
 * @param max_freq Maximum frequency.
 * @param flags Flags: BE_VERBOSE, BE_MOREVERBOSE, DO_IDELAY, DO_ODELAY.
 * @return 0 in case of success, negative error code otherwise.
 */
static int32_t ad9361_dig_tune_tx(struct ad9361_rf_phy *phy, uint32_t max_freq,
				  enum dig_tune_flags flags)
{
	struct axiadc_converter *conv = phy->adc_conv;
	struct axi_adc *rx_adc = phy->rx_adc;
	uint32_t saved_dsel[4], saved_chan_ctrl6[4], saved_chan_ctrl0[4];
	uint32_t chan, num_chan;
	uint32_t hdl_dac_version;
	uint32_t tmp, saved = 0;
	int32_t ret;

	num_chan = ad9361_num_phy_chan(conv);
	axi_adc_read(rx_adc, 0x4000, &hdl_dac_version);

	ad9361_bist_prbs(phy, BIST_DISABLE);
	ad9361_bist_loopback(phy, 1);
	axi_adc_write(rx_adc, 0x4000 + ADI_REG_RSTN, ADI_RSTN | ADI_MMCM_RSTN);

	for (chan = 0; chan < num_chan; chan++) {
		axi_adc_read(rx_adc, ADI_REG_CHAN_CNTRL(chan), &saved_chan_ctrl0[chan]);
		axi_adc_write(rx_adc, ADI_REG_CHAN_CNTRL(chan),
			      ADI_FORMAT_SIGNEXT | ADI_FORMAT_ENABLE |
			      ADI_ENABLE | ADI_IQCOR_ENB);
		axi_adc_set_pnsel(phy->rx_adc, chan, ADC_PN_CUSTOM);
		axi_adc_read(rx_adc, 0x4414 + (chan) * 0x40, &saved_chan_ctrl6[chan]);
		if (PCORE_VERSION_MAJOR(hdl_dac_version) > 7) {
			axi_adc_read(rx_adc, 0x4418 + (chan) * 0x40, &saved_dsel[chan]);
			axi_adc_write(rx_adc, 0x4418 + (chan) * 0x40, 9);
			axi_adc_write(rx_adc, 0x4414 + (chan) * 0x40, 0); /* !IQCOR_ENB */
			axi_adc_write(rx_adc, 0x4044, 1);
		} else {
			axi_adc_write(rx_adc, 0x4414 + (chan) * 0x40, 1); /* DAC_PN_ENB */
		}
	}
	if (PCORE_VERSION_MAJOR(hdl_dac_version) < 8) {
		axi_adc_read(rx_adc, 0x4048, &tmp);
		saved = tmp;
		tmp &= ~0xF;
		tmp |= 1;
		axi_adc_write(rx_adc, 0x4048, tmp);
	}

	ret = ad9361_dig_tune_delay(phy, max_freq, flags, true);
	if (flags & DO_ODELAY)
		ad9361_dig_tune_iodelay(phy, true);

	if (PCORE_VERSION_MAJOR(hdl_dac_version) < 8)
		axi_adc_write(rx_adc, 0x4048, saved);

	for (chan = 0; chan < num_chan; chan++) {
		axi_adc_write(rx_adc, ADI_REG_CHAN_CNTRL(chan),
			      saved_chan_ctrl0[chan]);
		axi_adc_set_pnsel(phy->rx_adc, chan, ADC_PN9);
		if (PCORE_VERSION_MAJOR(hdl_dac_version) > 7) {
			axi_adc_write(rx_adc, 0x4418 + chan * 0x40,
				      saved_dsel[chan]);
			axi_adc_write(rx_adc, 0x4044, 1);
		}

		axi_adc_write(rx_adc, 0x4414 + chan * 0x40, saved_chan_ctrl6[chan]);
	}

	return ret;
}

/**
 * Digital tune.
 * @param phy The AD9361 state structure.
 * @param max_freq Maximum frequency.
 * @param flags Flags: BE_VERBOSE, BE_MOREVERBOSE, DO_IDELAY, DO_ODELAY.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad9361_dig_tune(struct ad9361_rf_phy *phy, uint32_t max_freq,
			enum dig_tune_flags flags)
{
	struct axiadc_converter *conv = phy->adc_conv;
	struct axi_adc *rx_adc = phy->rx_adc;
	uint32_t loopback, bist, ensm_state;
	bool restore = false;
	int32_t ret = 0;

	if (!conv)
		return -ENODEV;

	dev_dbg(&phy->spi->dev, "%s: freq %"PRIu32" flags 0x%X\n", __func__,
		max_freq, flags);

	ensm_state = ad9361_ensm_get_state(phy);

	if (phy->pdata->dig_interface_tune_skipmode == 2 ||
	    (flags & RESTORE_DEFAULT)) {
		/* skip completely and use defaults */
		restore = true;
	} else {
		loopback = phy->bist_loopback_mode;
		bist = phy->bist_config;

		/* Mute TX, we don't want to transmit the PRBS */
		ad9361_tx_mute(phy, 1);

		if (!phy->pdata->fdd)
			ad9361_set_ensm_mode(phy, true, false);

		if (flags & DO_IDELAY)
			ad9361_midscale_iodelay(phy, false);

		if (flags & DO_ODELAY)
			ad9361_midscale_iodelay(phy, true);

		ret = ad9361_dig_tune_rx(phy, max_freq, flags);
		if (ret == 0 && !phy->pdata->dig_interface_tune_skipmode)
			ret = ad9361_dig_tune_tx(phy, max_freq, flags);

		ad9361_bist_loopback(phy, loopback);
		ad9361_spi_write(phy->spi, REG_BIST_CONFIG, bist);

		if (ret == -EIO)
			restore = true;
		if (!max_freq)
			ret = 0;
	}

	if (restore) {
		ad9361_ensm_force_state(phy, ENSM_STATE_ALERT);
		ad9361_spi_write(phy->spi, REG_RX_CLOCK_DATA_DELAY,
				 phy->pdata->port_ctrl.rx_clk_data_delay);
		ad9361_spi_write(phy->spi, REG_TX_CLOCK_DATA_DELAY,
				 phy->pdata->port_ctrl.tx_clk_data_delay);
	} else if (!(flags & SKIP_STORE_RESULT)) {
		phy->pdata->port_ctrl.rx_clk_data_delay =
			ad9361_spi_read(phy->spi, REG_RX_CLOCK_DATA_DELAY);
		phy->pdata->port_ctrl.tx_clk_data_delay =
			ad9361_spi_read(phy->spi, REG_TX_CLOCK_DATA_DELAY);
	}

	if (!phy->pdata->fdd)
		ad9361_set_ensm_mode(phy, phy->pdata->fdd, phy->pdata->ensm_pin_ctrl);
	ad9361_ensm_restore_state(phy, ensm_state);

	axi_adc_write(rx_adc, ADI_REG_RSTN, ADI_MMCM_RSTN);
	axi_adc_write(rx_adc, ADI_REG_RSTN, ADI_RSTN | ADI_MMCM_RSTN);

	ad9361_tx_mute(phy, 0);

	return ret;
}

/**
* Setup the AD9361 device.
* @param phy The AD9361 state structure.
* @return 0 in case of success, negative error code otherwise.
*/
int32_t ad9361_post_setup(struct ad9361_rf_phy *phy)
{
	struct axiadc_converter *conv = phy->adc_conv;
	struct axi_adc *rx_adc = phy->rx_adc;
	int32_t rx2tx2 = phy->pdata->rx2tx2;
	uint32_t tmp, num_chan, flags;
	int32_t i, ret;

	num_chan = ad9361_num_phy_chan(conv);

	axi_adc_write(rx_adc, ADI_REG_CNTRL, rx2tx2 ? 0 : ADI_R1_MODE);
	axi_adc_read(rx_adc, 0x4048, &tmp);

	if (!rx2tx2) {
		axi_adc_write(rx_adc, 0x4048, tmp | BIT(5)); /* R1_MODE */
		axi_adc_write(rx_adc, 0x404c,
			      (phy->pdata->port_ctrl.pp_conf[2] & LVDS_MODE) ? 1 : 0); /* RATE */
	} else {
		tmp &= ~BIT(5);
		axi_adc_write(rx_adc, 0x4048, tmp);
		axi_adc_write(rx_adc, 0x404c,
			      (phy->pdata->port_ctrl.pp_conf[2] & LVDS_MODE) ? 3 : 1); /* RATE */
	}

#ifdef ALTERA_PLATFORM
	axiadc_write(st, 0x404c, 1);
#endif

	for (i = 0; i < num_chan; i++) {
		axi_adc_write(rx_adc, ADI_REG_CHAN_CNTRL_1(i),
			      ADI_DCFILT_OFFSET(0));
		axi_adc_write(rx_adc, ADI_REG_CHAN_CNTRL_2(i),
			      (i & 1) ? 0x00004000 : 0x40000000);
		axi_adc_write(rx_adc, ADI_REG_CHAN_CNTRL(i),
			      ADI_FORMAT_SIGNEXT | ADI_FORMAT_ENABLE |
			      ADI_ENABLE | ADI_IQCOR_ENB);
	}

	flags = 0x0;

	axi_adc_read(rx_adc, ADI_REG_ID, &tmp);
	ret = ad9361_dig_tune(phy, (tmp) ?
			      0 : 61440000, flags);
	if (ret < 0)
		return ret;

	if (flags & (DO_IDELAY | DO_ODELAY)) {
		axi_adc_read(rx_adc, ADI_REG_ID, &tmp);
		ret = ad9361_dig_tune(phy, (tmp) ?
				      0 : 61440000, flags & BE_VERBOSE);
		if (ret < 0)
			return ret;
	}

	ret = ad9361_set_trx_clock_chain(phy,
					 phy->pdata->rx_path_clks,
					 phy->pdata->tx_path_clks);

	ad9361_ensm_force_state(phy, ENSM_STATE_ALERT);
	ad9361_ensm_restore_prev_state(phy);

	return ret;
}
#else
/**
 * HDL loopback enable/disable.
 * @param phy The AD9361 state structure.
 * @param enable Enable/disable option.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad9361_hdl_loopback(struct ad9361_rf_phy *phy, bool enable)
{
	return -ENODEV;
}

/**
 * Digital tune.
 * @param phy The AD9361 state structure.
 * @param max_freq Maximum frequency.
 * @param flags Flags: BE_VERBOSE, BE_MOREVERBOSE, DO_IDELAY, DO_ODELAY.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad9361_dig_tune(struct ad9361_rf_phy *phy, uint32_t max_freq,
			enum dig_tune_flags flags)
{
	return 0;
}

/**
* Setup the AD9361 device.
* @param phy The AD9361 state structure.
* @return 0 in case of success, negative error code otherwise.
*/
int32_t ad9361_post_setup(struct ad9361_rf_phy *phy)
{
	return 0;
}
#endif
