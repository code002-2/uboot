// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for Qualcomm Kaanapali (SM8850)
 *
 * (C) Copyright 2026 EYC <e@eyc.one>
 *
 * Register offsets, frequency tables and mux source values are taken from the
 * Linux driver drivers/clk/qcom/gcc-kaanapali.c (GPL-2.0-only, (c) Qualcomm,
 * introduced in Linux commit d1919c375f21). Only the USB and UFS clocks
 * needed to bring up storage and the USB gadget console are described here.
 */

#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dt-bindings/clock/qcom,kaanapali-gcc.h>
#include <dt-bindings/clock/qcom,rpmh.h>
#include <dt-bindings/clock/qcom,sm8750-tcsr.h>

#include "clock-qcom.h"

/* On-board TCXO (xo_board in kaanapali.dtsi), TOFIX get from DT */
#define TCXO_RATE	76800000

/* RPMh divides the board XO by two on its way out: clk_rpmh_bi_tcxo_div2. */
#define RPMH_CXO_RATE	(TCXO_RATE / 2)

/*
 * kaanapali.dtsi halves it once more in the bi_tcxo_div2 fixed-factor clock
 * before it reaches GCC, so the frequency tables below, which are copied from
 * Linux, are written against a 19.2MHz P_BI_TCXO.
 */
#define GCC_TCXO_RATE	(RPMH_CXO_RATE / 2)

static const struct freq_tbl ftbl_gcc_usb30_prim_master_clk_src[] = {
	F(66666667, CFG_CLK_SRC_GPLL0_EVEN, 4.5, 0, 0),
	F(133333333, CFG_CLK_SRC_GPLL0, 4.5, 0, 0),
	F(200000000, CFG_CLK_SRC_GPLL0, 3, 0, 0),
	F(240000000, CFG_CLK_SRC_GPLL0, 2.5, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_ufs_phy_axi_clk_src[] = {
	F(25000000, CFG_CLK_SRC_GPLL0_EVEN, 12, 0, 0),
	F(75000000, CFG_CLK_SRC_GPLL0_EVEN, 4, 0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0_EVEN, 3, 0, 0),
	F(201500000, CFG_CLK_SRC_GPLL4, 4, 0, 0),
	F(403000000, CFG_CLK_SRC_GPLL4, 2, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_ufs_phy_ice_core_clk_src[] = {
	F(75000000, CFG_CLK_SRC_GPLL0_EVEN, 4, 0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0_EVEN, 3, 0, 0),
	F(201500000, CFG_CLK_SRC_GPLL4, 4, 0, 0),
	F(403000000, CFG_CLK_SRC_GPLL4, 2, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_ufs_phy_phy_aux_clk_src[] = {
	F(9600000, CFG_CLK_SRC_CXO, 2, 0, 0),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	{ }
};

static ulong kaanapali_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	const struct freq_tbl *freq;

	switch (clk->id) {
	case GCC_USB30_PRIM_MASTER_CLK:
		freq = qcom_find_freq(ftbl_gcc_usb30_prim_master_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, 0x39034,
				     freq->pre_div, freq->m, freq->n, freq->src, 8);
		return freq->freq;
	case GCC_USB30_PRIM_MOCK_UTMI_CLK:
		/*
		 * Linux programs this RCG to 19.2MHz: CXO with divider 1,
		 * which F() encodes as (2 * 1 - 1) = 1. Go through _mnd for
		 * the same reason as the UFS clocks below: it is the only
		 * variant that clears the divider field, so whatever divider
		 * ABL left behind cannot survive under the new source.
		 */
		clk_rcg_set_rate_mnd(priv->base, 0x3904c,
				     1, 0, 0, CFG_CLK_SRC_CXO, 0);
		return GCC_TCXO_RATE;
	/*
	 * The UFS root clocks below are groundwork for storage support and have
	 * no consumer yet; they are untested on hardware.
	 *
	 * They all go through clk_rcg_set_rate_mnd(), including the three whose
	 * RCG has no MND counter. That is deliberate: F() already stores the
	 * hardware-encoded divider (2 * div - 1), which is what _mnd writes, and
	 * _mnd is the only variant that CLEARS the divider field before setting
	 * it. clk_rcg_set_rate() would re-encode an already-encoded pre_div and
	 * OR it onto whatever ABL left behind. Passing mnd_width 0 masks M/N/D
	 * to zero, and the same call clears CFG_MODE, which bypasses the MND.
	 */
	case GCC_UFS_PHY_AXI_CLK:
		freq = qcom_find_freq(ftbl_gcc_ufs_phy_axi_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, 0x77034,
				     freq->pre_div, freq->m, freq->n, freq->src, 8);
		return freq->freq;
	case GCC_UFS_PHY_ICE_CORE_CLK:
		freq = qcom_find_freq(ftbl_gcc_ufs_phy_ice_core_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, 0x7708c,
				     freq->pre_div, 0, 0, freq->src, 0);
		return freq->freq;
	case GCC_UFS_PHY_UNIPRO_CORE_CLK:
		freq = qcom_find_freq(ftbl_gcc_ufs_phy_ice_core_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, 0x770a4,
				     freq->pre_div, 0, 0, freq->src, 0);
		return freq->freq;
	case GCC_UFS_PHY_PHY_AUX_CLK:
		freq = qcom_find_freq(ftbl_gcc_ufs_phy_phy_aux_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, 0x770c0,
				     freq->pre_div, 0, 0, freq->src, 0);
		return freq->freq;
	default:
		return 0;
	}
}

static const struct gate_clk kaanapali_clks[] = {
	GATE_CLK_POLLED(GCC_AGGRE_UFS_PHY_AXI_CLK,	0x770f0, BIT(0), 0x770f0),
	GATE_CLK_POLLED(GCC_AGGRE_USB3_PRIM_AXI_CLK,	0x39094, BIT(0), 0x39094),
	GATE_CLK_POLLED(GCC_CFG_NOC_USB3_PRIM_AXI_CLK,	0x39090, BIT(0), 0x39090),
	GATE_CLK_POLLED(GCC_UFS_PHY_AHB_CLK,		0x77028, BIT(0), 0x77028),
	GATE_CLK_POLLED(GCC_UFS_PHY_AXI_CLK,		0x77018, BIT(0), 0x77018),
	GATE_CLK_POLLED(GCC_UFS_PHY_ICE_CORE_CLK,	0x7707c, BIT(0), 0x7707c),
	GATE_CLK_POLLED(GCC_UFS_PHY_PHY_AUX_CLK,	0x770bc, BIT(0), 0x770bc),
	GATE_CLK_POLLED(GCC_UFS_PHY_UNIPRO_CORE_CLK,	0x7706c, BIT(0), 0x7706c),
	GATE_CLK_POLLED(GCC_USB30_PRIM_MASTER_CLK,	0x39018, BIT(0), 0x39018),
	GATE_CLK_POLLED(GCC_USB30_PRIM_MOCK_UTMI_CLK,	0x3902c, BIT(0), 0x3902c),
	GATE_CLK_POLLED(GCC_USB30_PRIM_SLEEP_CLK,	0x39028, BIT(0), 0x39028),
	GATE_CLK_POLLED(GCC_USB3_PRIM_PHY_AUX_CLK,	0x39068, BIT(0), 0x39068),
	GATE_CLK_POLLED(GCC_USB3_PRIM_PHY_COM_AUX_CLK,	0x3906c, BIT(0), 0x3906c),
	/*
	 * The symbol and pipe clocks are fed by their PHY and only start
	 * ticking once that PHY is up, so they cannot be polled here.
	 */
	GATE_CLK(GCC_UFS_PHY_RX_SYMBOL_0_CLK,		0x77030, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_RX_SYMBOL_1_CLK,		0x770d8, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_TX_SYMBOL_0_CLK,		0x7702c, BIT(0)),
	GATE_CLK(GCC_USB3_PRIM_PHY_PIPE_CLK,		0x39070, BIT(0)),
};

static int kaanapali_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_AGGRE_USB3_PRIM_AXI_CLK:
		qcom_gate_clk_en(priv, GCC_USB30_PRIM_MASTER_CLK);
		fallthrough;
	case GCC_USB30_PRIM_MASTER_CLK:
		qcom_gate_clk_en(priv, GCC_USB3_PRIM_PHY_AUX_CLK);
		qcom_gate_clk_en(priv, GCC_USB3_PRIM_PHY_COM_AUX_CLK);
		break;
	}

	return qcom_gate_clk_en(priv, clk->id);
}

static const struct qcom_reset_map kaanapali_gcc_resets[] = {
	[GCC_QUSB2PHY_PRIM_BCR] = { 0x12000 },
	[GCC_QUSB2PHY_SEC_BCR] = { 0x12004 },
	[GCC_UFS_PHY_BCR] = { 0x77000 },
	[GCC_USB30_PRIM_BCR] = { 0x39000 },
	[GCC_USB3_DP_PHY_PRIM_BCR] = { 0x50008 },
	[GCC_USB3_PHY_PRIM_BCR] = { 0x50000 },
	[GCC_USB3PHY_PHY_PRIM_BCR] = { 0x50004 },
};

static const struct qcom_power_map kaanapali_gdscs[] = {
	[GCC_UFS_MEM_PHY_GDSC] = { 0x9e000 },
	[GCC_UFS_PHY_GDSC] = { 0x77004 },
	[GCC_USB30_PRIM_GDSC] = { 0x39004 },
	[GCC_USB3_PHY_GDSC] = { 0x50018 },
};

static struct msm_clk_data kaanapali_gcc_data = {
	.resets = kaanapali_gcc_resets,
	.num_resets = ARRAY_SIZE(kaanapali_gcc_resets),
	.clks = kaanapali_clks,
	.num_clks = ARRAY_SIZE(kaanapali_clks),
	.power_domains = kaanapali_gdscs,
	.num_power_domains = ARRAY_SIZE(kaanapali_gdscs),

	.enable = kaanapali_enable,
	.set_rate = kaanapali_set_rate,
};

static const struct udevice_id gcc_kaanapali_of_match[] = {
	{
		.compatible = "qcom,kaanapali-gcc",
		.data = (ulong)&kaanapali_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_kaanapali) = {
	.name		= "gcc_kaanapali",
	.id		= UCLASS_NOP,
	.of_match	= gcc_kaanapali_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

static ulong kaanapali_rpmh_clk_set_rate(struct clk *clk, ulong rate)
{
	return (clk->rate = rate);
}

static ulong kaanapali_rpmh_clk_get_rate(struct clk *clk)
{
	switch (clk->id) {
	/*
	 * RPMH_CXO_CLK is the RPMh output itself (38.4MHz), NOT the 19.2MHz
	 * P_BI_TCXO that GCC sees: kaanapali.dtsi puts a second fixed-factor
	 * divide-by-two (bi_tcxo_div2) between them. Report the rate of the
	 * clock we are actually asked about, so that a devicetree which models
	 * that divider does not halve it a third time.
	 */
	case RPMH_CXO_CLK:
		return RPMH_CXO_RATE;
	default:
		return clk->rate;
	}
}

static int kaanapali_rpmh_clk_nop(struct clk *clk)
{
	return 0;
}

static struct clk_ops kaanapali_rpmh_clk_ops = {
	.set_rate = kaanapali_rpmh_clk_set_rate,
	.get_rate = kaanapali_rpmh_clk_get_rate,
	.enable = kaanapali_rpmh_clk_nop,
	.disable = kaanapali_rpmh_clk_nop,
};

static const struct udevice_id kaanapali_rpmh_clk_ids[] = {
	{ .compatible = "qcom,kaanapali-rpmh-clk" },
	{ }
};

U_BOOT_DRIVER(kaanapali_rpmh_clk) = {
	.name		= "kaanapali_rpmh_clk",
	.id		= UCLASS_CLK,
	.of_match	= kaanapali_rpmh_clk_ids,
	.ops		= &kaanapali_rpmh_clk_ops,
	.flags		= DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

/* TCSRCC */

/*
 * Linux marks all three clkref gates BRANCH_HALT_DELAY: these registers
 * have no halt bit, so polling the top bits can only fail spuriously
 * (upstream sm8550/sm8650 leave exactly these gates unpolled as well).
 */
static const struct gate_clk kaanapali_tcsr_clks[] = {
	GATE_CLK(TCSR_UFS_CLKREF_EN,	0x15054, BIT(0)),
	GATE_CLK(TCSR_USB3_CLKREF_EN,	0x1504c, BIT(0)),
	GATE_CLK(TCSR_USB2_CLKREF_EN,	0x1505c, BIT(0)),
};

static struct msm_clk_data kaanapali_tcsrcc_data = {
	.clks = kaanapali_tcsr_clks,
	.num_clks = ARRAY_SIZE(kaanapali_tcsr_clks),
};

static int tcsrcc_kaanapali_clk_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static ulong tcsrcc_kaanapali_clk_get_rate(struct clk *clk)
{
	return TCXO_RATE;
}

static int tcsrcc_kaanapali_clk_probe(struct udevice *dev)
{
	struct msm_clk_data *data = (struct msm_clk_data *)dev_get_driver_data(dev);
	struct msm_clk_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->data = data;

	return 0;
}

static struct clk_ops tcsrcc_kaanapali_clk_ops = {
	.enable = tcsrcc_kaanapali_clk_enable,
	.get_rate = tcsrcc_kaanapali_clk_get_rate,
};

static const struct udevice_id tcsrcc_kaanapali_of_match[] = {
	{
		.compatible = "qcom,kaanapali-tcsr",
		.data = (ulong)&kaanapali_tcsrcc_data,
	},
	{ }
};

U_BOOT_DRIVER(tcsrcc_kaanapali) = {
	.name		= "tcsrcc_kaanapali",
	.id		= UCLASS_CLK,
	.of_match	= tcsrcc_kaanapali_of_match,
	.ops		= &tcsrcc_kaanapali_clk_ops,
	.priv_auto	= sizeof(struct msm_clk_priv),
	.probe		= tcsrcc_kaanapali_clk_probe,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
