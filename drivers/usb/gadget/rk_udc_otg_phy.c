/*
 * Copyright 2015 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>

#include <usb/s3c_udc.h>
#include "regs-otg.h"

/* UOC control */
#define GRF_UOC0_CON5				0x017c

void otg_phy_init(struct s3c_udc *dev)
{
	struct s3c_usbotg_reg *reg = (struct s3c_usbotg_reg *)dev->pdata->regs_otg;

	/* Disable usb-uart bypass */
	writel(0x34000000, (dev->pdata->regs_phy + GRF_UOC0_CON5));

	/* soft disconnect */
	writel(readl(&reg->dctl) | ~0x02, &reg->dctl);

	/* Phy PLL recovering */
	writel(0x00030001, (dev->pdata->regs_phy + GRF_UOC0_CON5));
	mdelay(10);
	writel(0x00030002, (dev->pdata->regs_phy + GRF_UOC0_CON5));
	mdelay(500);

	/* Restart the Phy Clock */
	writel(0x00, (u32 *)dev->pdata->usb_phy_ctrl);	

	/* soft connect */
	writel(readl(&reg->dctl) & ~0x02, &reg->dctl);
}

void otg_phy_off(struct s3c_udc *dev)
{
	/* usbphy0 bypass disable and otg enable */
	writel(0x34000000, (dev->pdata->regs_phy + GRF_UOC0_CON5));

	/* usb phy enter suspend */
	writel(0x007f0055, (dev->pdata->regs_phy + GRF_UOC0_CON5));
}

