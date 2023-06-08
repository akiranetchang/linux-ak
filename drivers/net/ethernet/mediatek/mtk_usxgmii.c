/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Henry Yen <henry.yen@mediatek.com>
 */

#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include "mtk_eth_soc.h"

int mtk_usxgmii_init(struct mtk_eth *eth)
{
	struct device_node *r = eth->dev->of_node;
	struct mtk_xgmii *xs = eth->xgmii;
	struct device *dev = eth->dev;
	struct device_node *np;
	int i;

	xs->regmap_usxgmii = devm_kzalloc(dev, sizeof(*xs->regmap_usxgmii) *
					  eth->soc->num_devs, GFP_KERNEL);
	if (!xs->regmap_usxgmii)
		return -ENOMEM;

	for (i = 0; i < eth->soc->num_devs; i++) {
		np = of_parse_phandle(r, "mediatek,usxgmiisys", i);
		if (!np)
			break;

		xs->regmap_usxgmii[i] = syscon_node_to_regmap(np);
		if (IS_ERR(xs->regmap_usxgmii[i]))
			return PTR_ERR(xs->regmap_usxgmii[i]);
	}

	return 0;
}

int mtk_xfi_pextp_init(struct mtk_eth *eth)
{
	struct device *dev = eth->dev;
	struct device_node *r = dev->of_node;
	struct mtk_xgmii *xs = eth->xgmii;
	struct device_node *np;
	int i;

	xs->regmap_pextp = devm_kzalloc(dev, sizeof(*xs->regmap_pextp) *
				        eth->soc->num_devs, GFP_KERNEL);
	if (!xs->regmap_pextp)
		return -ENOMEM;

	for (i = 0; i < eth->soc->num_devs; i++) {
		np = of_parse_phandle(r, "mediatek,xfi_pextp", i);
		if (!np)
			break;

		xs->regmap_pextp[i] = syscon_node_to_regmap(np);
		if (IS_ERR(xs->regmap_pextp[i]))
			return PTR_ERR(xs->regmap_pextp[i]);
	}

	return 0;
}

int mtk_xfi_pll_init(struct mtk_eth *eth)
{
	struct device_node *r = eth->dev->of_node;
	struct mtk_xgmii *xs = eth->xgmii;
	struct device_node *np;

	np = of_parse_phandle(r, "mediatek,xfi_pll", 0);
	if (!np)
		return -1;

	xs->regmap_pll = syscon_node_to_regmap(np);
	if (IS_ERR(xs->regmap_pll))
		return PTR_ERR(xs->regmap_pll);

	return 0;
}

int mtk_toprgu_init(struct mtk_eth *eth)
{
	struct device_node *r = eth->dev->of_node;
	struct device_node *np;

	np = of_parse_phandle(r, "mediatek,toprgu", 0);
	if (!np)
		return -1;

	eth->toprgu = syscon_node_to_regmap(np);
	if (IS_ERR(eth->toprgu))
		return PTR_ERR(eth->toprgu);

	return 0;
}

int mtk_xfi_pll_enable(struct mtk_eth *eth)
{
	struct mtk_xgmii *xs = eth->xgmii;
	u32 val = 0;

	if (!xs->regmap_pll)
		return -EINVAL;

	/* Add software workaround for USXGMII PLL TCL issue */
	regmap_write(xs->regmap_pll, XFI_PLL_ANA_GLB8, RG_XFI_PLL_ANA_SWWA);

	regmap_read(xs->regmap_pll, XFI_PLL_DIG_GLB8, &val);
	val |= RG_XFI_PLL_EN;
	regmap_write(xs->regmap_pll, XFI_PLL_DIG_GLB8, val);

	return 0;
}

static int mtk_mac2xgmii_id(struct mtk_eth *eth, int mac_id)
{
	int xgmii_id = mac_id;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		switch (mac_id) {
		case MTK_GMAC1_ID:
		case MTK_GMAC2_ID:
			xgmii_id = 1;
			break;
		case MTK_GMAC3_ID:
			xgmii_id = 0;
			break;
		default:
			xgmii_id = -1;
		}
	}

	return xgmii_id;
}

void mtk_usxgmii_setup_phya_an_10000(struct mtk_eth *eth, int mac_id)
{
	struct mtk_xgmii *xs = eth->xgmii;
	u32 id = mtk_mac2xgmii_id(eth, mac_id);

	if (id >= eth->soc->num_devs ||
	    !xs->regmap_usxgmii[id] || !xs->regmap_pextp[id])
		return;

	regmap_write(xs->regmap_usxgmii[id], RG_PCS_AN_CTRL0, 0x000FFE6D);
	regmap_write(xs->regmap_usxgmii[id], 0x818, 0x07B1EC7B);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, 0x30000000);
	ndelay(1020);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, 0x10000000);
	ndelay(1020);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, 0x00000000);

	regmap_write(xs->regmap_pextp[id], 0x9024, 0x00C9071C);
	regmap_write(xs->regmap_pextp[id], 0x2020, 0xAA8585AA);
	regmap_write(xs->regmap_pextp[id], 0x2030, 0x0C020707);
	regmap_write(xs->regmap_pextp[id], 0x2034, 0x0E050F0F);
	regmap_write(xs->regmap_pextp[id], 0x2040, 0x00140032);
	regmap_write(xs->regmap_pextp[id], 0x50F0, 0x00C014AA);
	regmap_write(xs->regmap_pextp[id], 0x50E0, 0x3777C12B);
	regmap_write(xs->regmap_pextp[id], 0x506C, 0x005F9CFF);
	regmap_write(xs->regmap_pextp[id], 0x5070, 0x9D9DFAFA);
	regmap_write(xs->regmap_pextp[id], 0x5074, 0x27273F3F);
	regmap_write(xs->regmap_pextp[id], 0x5078, 0xA7883C68);
	regmap_write(xs->regmap_pextp[id], 0x507C, 0x11661166);
	regmap_write(xs->regmap_pextp[id], 0x5080, 0x0E000AAF);
	regmap_write(xs->regmap_pextp[id], 0x5084, 0x08080D0D);
	regmap_write(xs->regmap_pextp[id], 0x5088, 0x02030909);
	regmap_write(xs->regmap_pextp[id], 0x50E4, 0x0C0C0000);
	regmap_write(xs->regmap_pextp[id], 0x50E8, 0x04040000);
	regmap_write(xs->regmap_pextp[id], 0x50EC, 0x0F0F0C06);
	regmap_write(xs->regmap_pextp[id], 0x50A8, 0x506E8C8C);
	regmap_write(xs->regmap_pextp[id], 0x6004, 0x18190000);
	regmap_write(xs->regmap_pextp[id], 0x00F8, 0x01423342);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F20);
	regmap_write(xs->regmap_pextp[id], 0x0030, 0x00050C00);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x02002800);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000020);
	regmap_write(xs->regmap_pextp[id], 0x3028, 0x00008A01);
	regmap_write(xs->regmap_pextp[id], 0x302C, 0x0000A884);
	regmap_write(xs->regmap_pextp[id], 0x3024, 0x00083002);
	regmap_write(xs->regmap_pextp[id], 0x3010, 0x00022220);
	regmap_write(xs->regmap_pextp[id], 0x5064, 0x0F020A01);
	regmap_write(xs->regmap_pextp[id], 0x50B4, 0x06100600);
	regmap_write(xs->regmap_pextp[id], 0x3048, 0x40704000);
	regmap_write(xs->regmap_pextp[id], 0x3050, 0xA8000000);
	regmap_write(xs->regmap_pextp[id], 0x3054, 0x000000AA);
	regmap_write(xs->regmap_pextp[id], 0x306C, 0x00000F00);
	regmap_write(xs->regmap_pextp[id], 0xA060, 0x00040000);
	regmap_write(xs->regmap_pextp[id], 0x90D0, 0x00000001);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200E800);
	udelay(150);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C101);
	udelay(15);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C101);
	udelay(100);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000030);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F00);
	regmap_write(xs->regmap_pextp[id], 0x3040, 0x30000000);
	udelay(400);
}

void mtk_usxgmii_setup_phya_force_5000(struct mtk_eth *eth, int mac_id)
{
	unsigned int val;
	struct mtk_xgmii *xs = eth->xgmii;
	u32 id = mtk_mac2xgmii_id(eth, mac_id);

	if (id >= eth->soc->num_devs ||
	    !xs->regmap_usxgmii[id] || !xs->regmap_pextp[id])
		return;

	/* Setup USXGMII speed */
	val = FIELD_PREP(RG_XFI_RX_MODE, RG_XFI_RX_MODE_5G) |
	      FIELD_PREP(RG_XFI_TX_MODE, RG_XFI_TX_MODE_5G);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	/* Disable USXGMII AN mode */
	regmap_read(xs->regmap_usxgmii[id], RG_PCS_AN_CTRL0, &val);
	val &= ~RG_AN_ENABLE;
	regmap_write(xs->regmap_usxgmii[id], RG_PCS_AN_CTRL0, val);

	/* Gated USXGMII */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val |= RG_MAC_CK_GATED;
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	ndelay(1020);

	/* USXGMII force mode setting */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val |= RG_USXGMII_RATE_UPDATE_MODE;
	val |= RG_IF_FORCE_EN;
	val |= FIELD_PREP(RG_RATE_ADAPT_MODE, RG_RATE_ADAPT_MODE_X1);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	/* Un-gated USXGMII */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val &= ~RG_MAC_CK_GATED;
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	ndelay(1020);

	regmap_write(xs->regmap_pextp[id], 0x9024, 0x00D9071C);
	regmap_write(xs->regmap_pextp[id], 0x2020, 0xAAA5A5AA);
	regmap_write(xs->regmap_pextp[id], 0x2030, 0x0C020707);
	regmap_write(xs->regmap_pextp[id], 0x2034, 0x0E050F0F);
	regmap_write(xs->regmap_pextp[id], 0x2040, 0x00140032);
	regmap_write(xs->regmap_pextp[id], 0x50F0, 0x00C018AA);
	regmap_write(xs->regmap_pextp[id], 0x50E0, 0x3777812B);
	regmap_write(xs->regmap_pextp[id], 0x506C, 0x005C9CFF);
	regmap_write(xs->regmap_pextp[id], 0x5070, 0x9DFAFAFA);
	regmap_write(xs->regmap_pextp[id], 0x5074, 0x273F3F3F);
	regmap_write(xs->regmap_pextp[id], 0x5078, 0xA8883868);
	regmap_write(xs->regmap_pextp[id], 0x507C, 0x14661466);
	regmap_write(xs->regmap_pextp[id], 0x5080, 0x0E001ABF);
	regmap_write(xs->regmap_pextp[id], 0x5084, 0x080B0D0D);
	regmap_write(xs->regmap_pextp[id], 0x5088, 0x02050909);
	regmap_write(xs->regmap_pextp[id], 0x50E4, 0x0C000000);
	regmap_write(xs->regmap_pextp[id], 0x50E8, 0x04000000);
	regmap_write(xs->regmap_pextp[id], 0x50EC, 0x0F0F0C06);
	regmap_write(xs->regmap_pextp[id], 0x50A8, 0x50808C8C);
	regmap_write(xs->regmap_pextp[id], 0x6004, 0x18000000);
	regmap_write(xs->regmap_pextp[id], 0x00F8, 0x00A132A1);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F20);
	regmap_write(xs->regmap_pextp[id], 0x0030, 0x00050C00);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x02002800);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000020);
	regmap_write(xs->regmap_pextp[id], 0x3028, 0x00008A01);
	regmap_write(xs->regmap_pextp[id], 0x302C, 0x0000A884);
	regmap_write(xs->regmap_pextp[id], 0x3024, 0x00083002);
	regmap_write(xs->regmap_pextp[id], 0x3010, 0x00022220);
	regmap_write(xs->regmap_pextp[id], 0x5064, 0x0F020A01);
	regmap_write(xs->regmap_pextp[id], 0x50B4, 0x06100600);
	regmap_write(xs->regmap_pextp[id], 0x3048, 0x40704000);
	regmap_write(xs->regmap_pextp[id], 0x3050, 0xA8000000);
	regmap_write(xs->regmap_pextp[id], 0x3054, 0x000000AA);
	regmap_write(xs->regmap_pextp[id], 0x306C, 0x00000F00);
	regmap_write(xs->regmap_pextp[id], 0xA060, 0x00040000);
	regmap_write(xs->regmap_pextp[id], 0x90D0, 0x00000003);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200E800);
	udelay(150);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C101);
	udelay(15);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C101);
	udelay(100);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000030);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F00);
	regmap_write(xs->regmap_pextp[id], 0x3040, 0x30000000);
	udelay(400);
}

void mtk_usxgmii_setup_phya_force_10000(struct mtk_eth *eth, int mac_id)
{
	struct mtk_xgmii *xs = eth->xgmii;
	unsigned int val;
	u32 id = mtk_mac2xgmii_id(eth, mac_id);

	if (id >= eth->soc->num_devs ||
	    !xs->regmap_usxgmii[id] || !xs->regmap_pextp[id])
		return;

	/* Setup USXGMII speed */
	val = FIELD_PREP(RG_XFI_RX_MODE, RG_XFI_RX_MODE_10G) |
	      FIELD_PREP(RG_XFI_TX_MODE, RG_XFI_TX_MODE_10G);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	/* Disable USXGMII AN mode */
	regmap_read(xs->regmap_usxgmii[id], RG_PCS_AN_CTRL0, &val);
	val &= ~RG_AN_ENABLE;
	regmap_write(xs->regmap_usxgmii[id], RG_PCS_AN_CTRL0, val);

	/* Gated USXGMII */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val |= RG_MAC_CK_GATED;
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	ndelay(1020);

	/* USXGMII force mode setting */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val |= RG_USXGMII_RATE_UPDATE_MODE;
	val |= RG_IF_FORCE_EN;
	val |= FIELD_PREP(RG_RATE_ADAPT_MODE, RG_RATE_ADAPT_MODE_X1);
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	/* Un-gated USXGMII */
	regmap_read(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, &val);
	val &= ~RG_MAC_CK_GATED;
	regmap_write(xs->regmap_usxgmii[id], RG_PHY_TOP_SPEED_CTRL1, val);

	ndelay(1020);

	regmap_write(xs->regmap_pextp[id], 0x9024, 0x00C9071C);
	regmap_write(xs->regmap_pextp[id], 0x2020, 0xAA8585AA);
	regmap_write(xs->regmap_pextp[id], 0x2030, 0x0C020707);
	regmap_write(xs->regmap_pextp[id], 0x2034, 0x0E050F0F);
	regmap_write(xs->regmap_pextp[id], 0x2040, 0x00140032);
	regmap_write(xs->regmap_pextp[id], 0x50F0, 0x00C014AA);
	regmap_write(xs->regmap_pextp[id], 0x50E0, 0x3777C12B);
	regmap_write(xs->regmap_pextp[id], 0x506C, 0x005F9CFF);
	regmap_write(xs->regmap_pextp[id], 0x5070, 0x9D9DFAFA);
	regmap_write(xs->regmap_pextp[id], 0x5074, 0x27273F3F);
	regmap_write(xs->regmap_pextp[id], 0x5078, 0xA7883C68);
	regmap_write(xs->regmap_pextp[id], 0x507C, 0x11661166);
	regmap_write(xs->regmap_pextp[id], 0x5080, 0x0E000AAF);
	regmap_write(xs->regmap_pextp[id], 0x5084, 0x08080D0D);
	regmap_write(xs->regmap_pextp[id], 0x5088, 0x02030909);
	regmap_write(xs->regmap_pextp[id], 0x50E4, 0x0C0C0000);
	regmap_write(xs->regmap_pextp[id], 0x50E8, 0x04040000);
	regmap_write(xs->regmap_pextp[id], 0x50EC, 0x0F0F0C06);
	regmap_write(xs->regmap_pextp[id], 0x50A8, 0x506E8C8C);
	regmap_write(xs->regmap_pextp[id], 0x6004, 0x18190000);
	regmap_write(xs->regmap_pextp[id], 0x00F8, 0x01423342);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F20);
	regmap_write(xs->regmap_pextp[id], 0x0030, 0x00050C00);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x02002800);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000020);
	regmap_write(xs->regmap_pextp[id], 0x3028, 0x00008A01);
	regmap_write(xs->regmap_pextp[id], 0x302C, 0x0000A884);
	regmap_write(xs->regmap_pextp[id], 0x3024, 0x00083002);
	regmap_write(xs->regmap_pextp[id], 0x3010, 0x00022220);
	regmap_write(xs->regmap_pextp[id], 0x5064, 0x0F020A01);
	regmap_write(xs->regmap_pextp[id], 0x50B4, 0x06100600);
	regmap_write(xs->regmap_pextp[id], 0x3048, 0x49664100);
	regmap_write(xs->regmap_pextp[id], 0x3050, 0x00000000);
	regmap_write(xs->regmap_pextp[id], 0x3054, 0x00000000);
	regmap_write(xs->regmap_pextp[id], 0x306C, 0x00000F00);
	regmap_write(xs->regmap_pextp[id], 0xA060, 0x00040000);
	regmap_write(xs->regmap_pextp[id], 0x90D0, 0x00000001);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200E800);
	udelay(150);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C101);
	udelay(15);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0202C101);
	udelay(100);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000030);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F00);
	regmap_write(xs->regmap_pextp[id], 0x3040, 0x30000000);
	udelay(400);
}

void mtk_usxgmii_reset(struct mtk_eth *eth, int mac_id)
{
	u32 id = mtk_mac2xgmii_id(eth, mac_id);

	if (id >= eth->soc->num_devs || !eth->toprgu)
		return;

	switch (mac_id) {
	case MTK_GMAC2_ID:
		regmap_write(eth->toprgu, 0xFC, 0x0000A004);
		regmap_write(eth->toprgu, 0x18, 0x88F0A004);
		regmap_write(eth->toprgu, 0xFC, 0x00000000);
		regmap_write(eth->toprgu, 0x18, 0x88F00000);
		regmap_write(eth->toprgu, 0x18, 0x00F00000);
		break;
	case MTK_GMAC3_ID:
		regmap_write(eth->toprgu, 0xFC, 0x00005002);
		regmap_write(eth->toprgu, 0x18, 0x88F05002);
		regmap_write(eth->toprgu, 0xFC, 0x00000000);
		regmap_write(eth->toprgu, 0x18, 0x88F00000);
		regmap_write(eth->toprgu, 0x18, 0x00F00000);
		break;
	}

	mdelay(10);
}

int mtk_usxgmii_setup_mode_an(struct mtk_eth *eth, int mac_id, int max_speed)
{
	if (mac_id < 0 || mac_id >= eth->soc->num_devs)
		return -EINVAL;

	if ((max_speed != SPEED_10000) && (max_speed != SPEED_5000))
		return -EINVAL;

	mtk_xfi_pll_enable(eth);
	mtk_usxgmii_reset(eth, mac_id);
	mtk_usxgmii_setup_phya_an_10000(eth, mac_id);

	return 0;
}

int mtk_usxgmii_setup_mode_force(struct mtk_eth *eth, int mac_id,
				 const struct phylink_link_state *state)
{
	if (mac_id < 0 || mac_id >= eth->soc->num_devs)
		return -EINVAL;

	mtk_xfi_pll_enable(eth);
	mtk_usxgmii_reset(eth, mac_id);
	if (state->interface == PHY_INTERFACE_MODE_5GBASER)
		mtk_usxgmii_setup_phya_force_5000(eth, mac_id);
	else
		mtk_usxgmii_setup_phya_force_10000(eth, mac_id);

	return 0;
}

void mtk_sgmii_setup_phya_gen1(struct mtk_eth *eth, int mac_id)
{
	u32 id = mtk_mac2xgmii_id(eth, mac_id);
	struct mtk_xgmii *xs = eth->xgmii;

	if (id >= eth->soc->num_devs || !xs->regmap_pextp[id])
		return;

	regmap_write(xs->regmap_pextp[id], 0x9024, 0x00D9071C);
	regmap_write(xs->regmap_pextp[id], 0x2020, 0xAA8585AA);
	regmap_write(xs->regmap_pextp[id], 0x2030, 0x0C020207);
	regmap_write(xs->regmap_pextp[id], 0x2034, 0x0E05050F);
	regmap_write(xs->regmap_pextp[id], 0x2040, 0x00200032);
	regmap_write(xs->regmap_pextp[id], 0x50F0, 0x00C014BA);
	regmap_write(xs->regmap_pextp[id], 0x50E0, 0x3777C12B);
	regmap_write(xs->regmap_pextp[id], 0x506C, 0x005F9CFF);
	regmap_write(xs->regmap_pextp[id], 0x5070, 0x9D9DFAFA);
	regmap_write(xs->regmap_pextp[id], 0x5074, 0x27273F3F);
	regmap_write(xs->regmap_pextp[id], 0x5078, 0xA7883C68);
	regmap_write(xs->regmap_pextp[id], 0x507C, 0x11661166);
	regmap_write(xs->regmap_pextp[id], 0x5080, 0x0E000EAF);
	regmap_write(xs->regmap_pextp[id], 0x5084, 0x08080E0D);
	regmap_write(xs->regmap_pextp[id], 0x5088, 0x02030B09);
	regmap_write(xs->regmap_pextp[id], 0x50E4, 0x0C0C0000);
	regmap_write(xs->regmap_pextp[id], 0x50E8, 0x04040000);
	regmap_write(xs->regmap_pextp[id], 0x50EC, 0x0F0F0606);
	regmap_write(xs->regmap_pextp[id], 0x50A8, 0x506E8C8C);
	regmap_write(xs->regmap_pextp[id], 0x6004, 0x18190000);
	regmap_write(xs->regmap_pextp[id], 0x00F8, 0x00FA32FA);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F21);
	regmap_write(xs->regmap_pextp[id], 0x0030, 0x00050C00);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x02002800);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000020);
	regmap_write(xs->regmap_pextp[id], 0x3028, 0x00008A01);
	regmap_write(xs->regmap_pextp[id], 0x302C, 0x0000A884);
	regmap_write(xs->regmap_pextp[id], 0x3024, 0x00083002);
	regmap_write(xs->regmap_pextp[id], 0x3010, 0x00011110);
	regmap_write(xs->regmap_pextp[id], 0x3048, 0x40704000);
	regmap_write(xs->regmap_pextp[id], 0x3064, 0x0000C000);
	regmap_write(xs->regmap_pextp[id], 0x3050, 0xA8000000);
	regmap_write(xs->regmap_pextp[id], 0x3054, 0x000000AA);
	regmap_write(xs->regmap_pextp[id], 0x306C, 0x20200F00);
	regmap_write(xs->regmap_pextp[id], 0xA060, 0x00050000);
	regmap_write(xs->regmap_pextp[id], 0x90D0, 0x00000007);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200E800);
	udelay(150);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C101);
	udelay(15);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0201C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0201C101);
	udelay(100);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000030);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F01);
	regmap_write(xs->regmap_pextp[id], 0x3040, 0x30000000);
	udelay(400);
}

void mtk_sgmii_setup_phya_gen2(struct mtk_eth *eth, int mac_id)
{
	struct mtk_xgmii *xs = eth->xgmii;
	u32 id = mtk_mac2xgmii_id(eth, mac_id);

	if (id >= eth->soc->num_devs || !xs->regmap_pextp[id])
		return;

	regmap_write(xs->regmap_pextp[id], 0x9024, 0x00D9071C);
	regmap_write(xs->regmap_pextp[id], 0x2020, 0xAA8585AA);
	regmap_write(xs->regmap_pextp[id], 0x2030, 0x0C020707);
	regmap_write(xs->regmap_pextp[id], 0x2034, 0x0E050F0F);
	regmap_write(xs->regmap_pextp[id], 0x2040, 0x00140032);
	regmap_write(xs->regmap_pextp[id], 0x50F0, 0x00C014AA);
	regmap_write(xs->regmap_pextp[id], 0x50E0, 0x3777C12B);
	regmap_write(xs->regmap_pextp[id], 0x506C, 0x005F9CFF);
	regmap_write(xs->regmap_pextp[id], 0x5070, 0x9D9DFAFA);
	regmap_write(xs->regmap_pextp[id], 0x5074, 0x27273F3F);
	regmap_write(xs->regmap_pextp[id], 0x5078, 0xA7883C68);
	regmap_write(xs->regmap_pextp[id], 0x507C, 0x11661166);
	regmap_write(xs->regmap_pextp[id], 0x5080, 0x0E000AAF);
	regmap_write(xs->regmap_pextp[id], 0x5084, 0x08080D0D);
	regmap_write(xs->regmap_pextp[id], 0x5088, 0x02030909);
	regmap_write(xs->regmap_pextp[id], 0x50E4, 0x0C0C0000);
	regmap_write(xs->regmap_pextp[id], 0x50E8, 0x04040000);
	regmap_write(xs->regmap_pextp[id], 0x50EC, 0x0F0F0C06);
	regmap_write(xs->regmap_pextp[id], 0x50A8, 0x506E8C8C);
	regmap_write(xs->regmap_pextp[id], 0x6004, 0x18190000);
	regmap_write(xs->regmap_pextp[id], 0x00F8, 0x009C329C);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F21);
	regmap_write(xs->regmap_pextp[id], 0x0030, 0x00050C00);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x02002800);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000020);
	regmap_write(xs->regmap_pextp[id], 0x3028, 0x00008A01);
	regmap_write(xs->regmap_pextp[id], 0x302C, 0x0000A884);
	regmap_write(xs->regmap_pextp[id], 0x3024, 0x00083002);
	regmap_write(xs->regmap_pextp[id], 0x3010, 0x00011110);
	regmap_write(xs->regmap_pextp[id], 0x3048, 0x40704000);
	regmap_write(xs->regmap_pextp[id], 0x3050, 0xA8000000);
	regmap_write(xs->regmap_pextp[id], 0x3054, 0x000000AA);
	regmap_write(xs->regmap_pextp[id], 0x306C, 0x22000F00);
	regmap_write(xs->regmap_pextp[id], 0xA060, 0x00050000);
	regmap_write(xs->regmap_pextp[id], 0x90D0, 0x00000005);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200E800);
	udelay(150);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0200C101);
	udelay(15);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0201C111);
	ndelay(1020);
	regmap_write(xs->regmap_pextp[id], 0x0070, 0x0201C101);
	udelay(100);
	regmap_write(xs->regmap_pextp[id], 0x30B0, 0x00000030);
	regmap_write(xs->regmap_pextp[id], 0x00F4, 0x80201F01);
	regmap_write(xs->regmap_pextp[id], 0x3040, 0x30000000);
	udelay(400);
}

void mtk_sgmii_reset(struct mtk_eth *eth, int mac_id)
{
	u32 id = mtk_mac2xgmii_id(eth, mac_id);
	u32 val = 0;

	if (id >= eth->soc->num_devs || !eth->toprgu)
		return;

	switch (mac_id) {
	case MTK_GMAC2_ID:
		/* Enable software reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST_EN, &val);
		val |= SWSYSRST_XFI_PEXPT1_GRST |
		       SWSYSRST_SGMII1_GRST;
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST_EN, val);

		/* Assert SGMII reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST, &val);
		val |= FIELD_PREP(SWSYSRST_UNLOCK_KEY, 0x88) |
		       SWSYSRST_XFI_PEXPT1_GRST |
		       SWSYSRST_SGMII1_GRST;
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST, val);

		udelay(100);

		/* De-assert SGMII reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST, &val);
		val |= FIELD_PREP(SWSYSRST_UNLOCK_KEY, 0x88);
		val &= ~(SWSYSRST_XFI_PEXPT1_GRST |
			 SWSYSRST_SGMII1_GRST);
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST, val);

		/* Disable software reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST_EN, &val);
		val &= ~(SWSYSRST_XFI_PEXPT1_GRST |
			 SWSYSRST_SGMII1_GRST);
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST_EN, val);
		break;
	case MTK_GMAC3_ID:
		/* Enable Software reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST_EN, &val);
		val |= SWSYSRST_XFI_PEXPT0_GRST |
		       SWSYSRST_SGMII0_GRST;
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST_EN, val);

		/* Assert SGMII reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST, &val);
		val |= FIELD_PREP(SWSYSRST_UNLOCK_KEY, 0x88) |
		       SWSYSRST_XFI_PEXPT0_GRST |
		       SWSYSRST_SGMII0_GRST;
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST, val);

		udelay(100);

		/* De-assert SGMII reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST, &val);
		val |= FIELD_PREP(SWSYSRST_UNLOCK_KEY, 0x88);
		val &= ~(SWSYSRST_XFI_PEXPT0_GRST |
			 SWSYSRST_SGMII0_GRST);
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST, val);

		/* Disable software reset */
		regmap_read(eth->toprgu, TOPRGU_SWSYSRST_EN, &val);
		val &= ~(SWSYSRST_XFI_PEXPT0_GRST |
			 SWSYSRST_SGMII0_GRST);
		regmap_write(eth->toprgu, TOPRGU_SWSYSRST_EN, val);
		break;
	}

	mdelay(1);
}
