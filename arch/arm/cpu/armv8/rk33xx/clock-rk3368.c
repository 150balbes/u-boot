/*
 * (C) Copyright 2008-2014 Rockchip Electronics
 * Peter, Software Engineering, <superpeter.cai@gmail.com>.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/rkplat.h>


DECLARE_GLOBAL_DATA_PTR;


/* ARM/General/Codec pll freq config */
#define CONFIG_RKCLK_APLL_FREQ		816 /* MHZ */
#define CONFIG_RKCLK_GPLL_FREQ		297 /* MHZ */
#define CONFIG_RKCLK_CPLL_FREQ		384 /* MHZ */


/* Cpu clock source select */
#define CPU_SRC_ARM_PLL			0
#define CPU_SRC_GENERAL_PLL		1

/* Periph clock source select */
#define PERIPH_SRC_GENERAL_PLL		1
#define PERIPH_SRC_CODEC_PLL		0

/* bus clock source select */
#define BUS_SRC_GENERAL_PLL		1
#define BUS_SRC_CODEC_PLL		0

/* DDR clock source select */
#define DDR_SRC_DDR_PLL			0
#define DDR_SRC_GENERAL_PLL		1


struct pll_clk_set {
	unsigned long	rate;
	u32	pllcon0;
	u32	pllcon1;
	u32	pllcon2; //bit0 - bit11: nb = bwadj+1 = nf/2
	u32	rst_dly; //us

	u8	a53_core_div;
	u8	axi_core_div;
	u8	dbg_core_div;
	u8	atclk_core_div;

	u8	aclk_peri_div;
	u8	hclk_peri_div;
	u8	pclk_peri_div;
	u8	pad2;

	u8	axi_bus_div;
	u8	aclk_bus_div;
	u8	hclk_bus_div;
	u8	pclk_bus_div;
};


#define _APLL_SET_CLKS(khz, nr, nf, no, _a53_div, _axi_div, _atclk_div, _pclk_dbg_div) \
{ \
	.rate			= khz * KHZ, \
	.pllcon0		= PLL_CLKR_SET(nr) | PLL_CLKOD_SET(no), \
	.pllcon1		= PLL_CLKF_SET(nf), \
	.pllcon2		= PLL_CLK_BWADJ_SET(nf >> 1), \
	.rst_dly		= ((nr*500)/24+1), \
	.a53_core_div		= CLK_DIV_##_a53_div, \
	.axi_core_div		= CLK_DIV_##_axi_div, \
	.dbg_core_div		= CLK_DIV_##_pclk_dbg_div, \
	.atclk_core_div		= CLK_DIV_##_atclk_div, \
}

#define _GPLL_SET_CLKS(khz, nr, nf, no, _axi_peri_div, _ahb_peri_div, _apb_peri_div, _aclk_bus_div, _ahb_bus_div, _apb_bus_div) \
{ \
	.rate		= khz * KHZ, \
	.pllcon0	= PLL_CLKR_SET(nr) | PLL_CLKOD_SET(no), \
	.pllcon1	= PLL_CLKF_SET(nf), \
	.pllcon2	= PLL_CLK_BWADJ_SET(nf >> 1), \
	.rst_dly	= ((nr*500)/24+1), \
	.aclk_peri_div	= CLK_DIV_##_axi_peri_div, \
	.hclk_peri_div	= CLK_DIV_##_ahb_peri_div, \
	.pclk_peri_div	= CLK_DIV_##_apb_peri_div, \
	.aclk_bus_div	= CLK_DIV_##_aclk_bus_div, \
	.hclk_bus_div	= CLK_DIV_##_ahb_bus_div, \
	.pclk_bus_div	= CLK_DIV_##_apb_bus_div, \
}

#define _CPLL_SET_CLKS(khz, nr, nf, no) \
{ \
	.rate		= khz * KHZ, \
	.pllcon0	= PLL_CLKR_SET(nr) | PLL_CLKOD_SET(no), \
	.pllcon1	= PLL_CLKF_SET(nf), \
	.pllcon2	= PLL_CLK_BWADJ_SET(nf >> 1), \
	.rst_dly	= ((nr*500)/24+1), \
}

struct pll_data {
	u32 id;
	u32 size;
	struct pll_clk_set *clkset;
};

#define SET_PLL_DATA(_pll_id, _table, _size) \
{\
	.id = (u32)(_pll_id), \
	.size = (u32)(_size), \
	.clkset = (struct pll_clk_set *)(_table), \
}



/* 		rk3688 pll notice 		*/
/* 
 * Fref = Fin / nr
 * Fvco = Fin * nf / nr
 * Fout = Fvco / no
 *
 * Fin value range requirement:		32KHz ~ 2200MHz
 * Fref value range requirement:	32KHz ~ 50MHz
 * Fvco value range requirement:	1100MHz ~ 2200MHz
 * Fout value range requirement:	30MHz ~ 2200MHz
 *
 */

/* apll clock table, should be from high to low */
static const struct pll_clk_set apll_clks[] = {
	//rate, nr, nf, no,			a53_div, axi_div, atclk, pclk_dbg
	_APLL_SET_CLKS(1008000,1, 84, 2,	1, 4, 4, 4),
	_APLL_SET_CLKS(816000, 1, 68, 2,	1, 4, 4, 4),
	_APLL_SET_CLKS(600000, 1, 50, 2,	1, 4, 4, 4),
};


/* gpll clock table, should be from high to low */
static const struct pll_clk_set gpll_clks[] = {
	//rate, nr, nf, no,	aclk_peri_div, hclk_peri_div, pclk_peri_div,	aclk_bus_div, hclk_bus_div, pclk_bus_div
	_GPLL_SET_CLKS(768000, 1,  64, 2,    2, 2, 4,				2, 2, 4),
	_GPLL_SET_CLKS(594000, 2, 198, 4,    2, 2, 4,				2, 2, 4),
	_GPLL_SET_CLKS(300000, 1,  50, 4,    1, 2, 4,				1, 2, 4),
	_GPLL_SET_CLKS(297000, 2, 198, 8,    1, 2, 4,				1, 2, 4),
};


/* cpll clock table, should be from high to low */
static const struct pll_clk_set cpll_clks[] = {
	//rate, nr, nf, no
	_CPLL_SET_CLKS(798000, 2, 133, 2),
	_CPLL_SET_CLKS(594000, 2, 198, 4),
	_CPLL_SET_CLKS(384000, 2, 128, 4),
};


static struct pll_data rkpll_data[END_PLL_ID] = {
	SET_PLL_DATA(APLLB_ID, apll_clks, ARRAY_SIZE(apll_clks)),
	SET_PLL_DATA(DPLL_ID, NULL, 0),
	SET_PLL_DATA(CPLL_ID, cpll_clks, ARRAY_SIZE(cpll_clks)),
	SET_PLL_DATA(GPLL_ID, gpll_clks, ARRAY_SIZE(gpll_clks)),
};


/* Waiting for pll locked by pll id */
static void rkclk_pll_wait_lock(enum rk_plls_id pll_id)
{
	/* delay for pll lock */
	while (1) {
		if (cru_readl(PLL_CONS(pll_id, 1)) & (0x01 << 31)) {
			break;
		}
		clk_loop_delayus(1);
	}
}


/* Set pll mode by id, normal mode or slow mode */
static void rkclk_pll_set_mode(enum rk_plls_id pll_id, int pll_mode)
{
	uint32 con;
	uint32 nr, dly;

	con = cru_readl(PLL_CONS(pll_id, 0));
	nr = PLL_NR(con);
	dly = (nr * 500) / 24 + 1;

	if (pll_mode == RKCLK_PLL_MODE_NORMAL) {
		cru_writel(PLL_PWR_ON | PLL_PWR_DN_W_MSK, PLL_CONS(pll_id, 3));
		clk_loop_delayus(dly);
		rkclk_pll_wait_lock(pll_id);
		/* PLL enter normal-mode */
		cru_writel(PLL_MODE_NORM | PLL_MODE_W_MSK, PLL_CONS(pll_id, 3));
	} else {
		/* PLL enter slow-mode */
		cru_writel(PLL_MODE_SLOW | PLL_MODE_W_MSK, PLL_CONS(pll_id, 3));
		cru_writel(PLL_PWR_DN | PLL_PWR_DN_W_MSK, PLL_CONS(pll_id, 3));
	}
}


/* Set pll rate by id */
static int rkclk_pll_set_rate(enum rk_plls_id pll_id, uint32 mHz, pll_callback_f cb_f)
{
	struct pll_data *pll = NULL;
	struct pll_clk_set *clkset = NULL;
	unsigned long rate = mHz * MHZ;
	int i = 0;

	/* Find pll rate set */
	for (i=0; i<END_PLL_ID; i++) {
		if (rkpll_data[i].id == pll_id) {
			pll = &rkpll_data[i];
			break;
		}
	}
	if ((pll == NULL) || (pll->clkset == NULL)) {
		return -1;
	}

	/* Find clock set */
	for (i=0; i<pll->size; i++) {
		if (pll->clkset[i].rate <= rate) {
			clkset = &(pll->clkset[i]);
			break;
		}
	}
	if (clkset == NULL) {
		return -1;
	}

	/* PLL enter slow-mode */
	cru_writel(PLL_MODE_SLOW | PLL_MODE_W_MSK, PLL_CONS(pll_id, 3));
	/* enter rest */
	cru_writel((PLL_RESET | PLL_RESET_W_MSK), PLL_CONS(pll_id, 3));

	cru_writel(clkset->pllcon0, PLL_CONS(pll_id, 0));
	cru_writel(clkset->pllcon1, PLL_CONS(pll_id, 1));
	cru_writel(clkset->pllcon2, PLL_CONS(pll_id, 2));

	clk_loop_delayus(5);
	/* return form rest */
	cru_writel(PLL_RESET_RESUME | PLL_RESET_W_MSK, PLL_CONS(pll_id, 3));

	clk_loop_delayus(clkset->rst_dly);
	/* waiting for pll lock */
	rkclk_pll_wait_lock(pll_id);

	if (cb_f != NULL) {
		cb_f(clkset);
	}

	/* PLL enter normal-mode */
	cru_writel(PLL_MODE_NORM | PLL_MODE_W_MSK, PLL_CONS(pll_id, 3));

	return 0;
}


/* Get pll rate by id */
static uint32 rkclk_pll_get_rate(enum rk_plls_id pll_id)
{
	uint32 nr, no, nf;
	uint32 con;

	con = cru_readl(PLL_CONS(pll_id, 3));
	con = (con & PLL_MODE_MSK) >> 8;
	if (con == 0) {
		/* slow mode */
		return (24 * MHZ);
	} else if (con == 1) {
		/* normal mode */
		con = cru_readl(PLL_CONS(pll_id, 0));
		no = PLL_NO(con);
		nr = PLL_NR(con);
		con = cru_readl(PLL_CONS(pll_id, 1));
		nf = PLL_NF(con);

		return (24 * nf / (nr * no)) * MHZ;
	} else {
		/* deep slow mode */
		return 32768;
	}
}


/*
 * rkplat clock set bus clock from codec pll or general pll
 * 	when call this function, make sure pll is in slow mode
 */
static void rkclk_bus_ahpclk_set(uint32 pll_src, uint32 aclk_div, uint32 hclk_div, uint32 pclk_div)
{
	uint32 pll_sel = 0, a_div = 0, h_div = 0, p_div = 0;

	/* pd bus clock source select: 0: codec pll, 1: general pll */
	if (pll_src == 0) {
		pll_sel = PDBUS_ACLK_SEL_CPLL;
	} else {
		pll_sel = PDBUS_ACLK_SEL_GPLL;
	}

	/* pd bus aclk - aclk_pdbus = clk_src / (aclk_div_con + 1) */
	a_div = (aclk_div == 0) ? 1 : (aclk_div - 1);

	/* pd bus hclk -  hclk_pdbus = clk_src / (hclk_div_con + 1) */
	h_div = (hclk_div == 0) ? 1 : (hclk_div - 1);

	/* pd bus pclk - pclk_pdbus = clk_src / (pclk_div_con + 1) */
	p_div = (pclk_div == 0) ? 1 : (pclk_div - 1);

	cru_writel((PDBUS_ACLK_SEL_PLL_W_MSK | pll_sel)
			| (PDBUS_PCLK_DIV_W_MSK | (p_div << PDBUS_PCLK_DIV_OFF))
			| (PDBUS_HCLK_DIV_W_MSK | (h_div << PDBUS_HCLK_DIV_OFF))
			| (PDBUS_ACLK_DIV_W_MSK | (a_div << PDBUS_ACLK_DIV_OFF)), CRU_CLKSELS_CON(8));
}


/*
 * rkplat clock set periph clock from general pll
 * 	when call this function, make sure pll is in slow mode
 */
static void rkclk_periph_ahpclk_set(uint32 pll_src, uint32 aclk_div, uint32 hclk_div, uint32 pclk_div)
{
	uint32 pll_sel = 0, a_div = 0, h_div = 0, p_div = 0;

	/* periph clock source select: 0: codec pll, 1: general pll */
	if (pll_src == 0) {
		pll_sel = PERI_SEL_CPLL;
	} else {
		pll_sel = PERI_SEL_GPLL;
	}

	/* periph aclk - aclk_periph = periph_clk_src / (peri_aclk_div_con + 1) */
	a_div = (aclk_div == 0) ? 1 : (aclk_div - 1);

	/* periph hclk - aclk_bus: hclk_bus = 1:1 or 2:1 or 4:1 */
	switch (hclk_div)
	{
		case CLK_DIV_1:
			h_div = 0;
			break;
		case CLK_DIV_2:
			h_div = 1;
			break;
		case CLK_DIV_4:
			h_div = 2;
			break;
		default:
			h_div = 1;
			break;
	}

	/* periph pclk - aclk_bus: pclk_bus = 1:1 or 2:1 or 4:1 or 8:1 */
	switch (pclk_div)
	{
		case CLK_DIV_1:
			p_div = 0;
			break;
		case CLK_DIV_2:
			p_div = 1;
			break;
		case CLK_DIV_4:
			p_div = 2;
			break;
		case CLK_DIV_8:
			p_div = 3;
			break;
		default:
			p_div = 2;
			break;
	}

	cru_writel((PERI_SEL_PLL_W_MSK | pll_sel)
			| (PERI_PCLK_DIV_W_MSK | (p_div << PERI_PCLK_DIV_OFF))
			| (PERI_HCLK_DIV_W_MSK | (h_div << PERI_HCLK_DIV_OFF))
			| (PERI_ACLK_DIV_W_MSK | (a_div << PERI_ACLK_DIV_OFF)), CRU_CLKSELS_CON(9));
}


/*
 * rkplat clock set cpu clock from arm pll
 * 	when call this function, make sure pll is in slow mode
 */
static void rkclk_core_b_clk_set(uint32 pll_src, uint32 a53_core_b_div, uint32 aclkm_core_b_div, uint32 dbg_core_b_div, uint32 atclk_core_b_div)
{
	uint32_t pll_sel = 0, a53_div = 0, axi_div = 0, dbg_div = 0, atclk_div = 0;

	/* cpu clock source select: 0: arm pll, 1: general pll */
	if (pll_src == 0) {
		pll_sel = CORE_B_SEL_APLL;
	} else {
		pll_sel = CORE_B_SEL_GPLL;
	}

	/* a53 core clock div: clk_core = clk_src / (div_con + 1) */
	a53_div = (a53_core_b_div == 0) ? 1 : (a53_core_b_div - 1);

	/* aclkm core axi clock div: clk = clk_src / (div_con + 1) */
	axi_div = (aclkm_core_b_div == 0) ? 1 : (aclkm_core_b_div - 1);

	/* pclk dbg core axi clock div: clk = clk_src / (div_con + 1) */
	dbg_div = (dbg_core_b_div == 0) ? 1 : (dbg_core_b_div - 1);

	/* pclk dbg core axi clock div: clk = clk_src / (div_con + 1) */
	atclk_div = (atclk_core_b_div == 0) ? 1 : (atclk_core_b_div - 1);

	cru_writel((CORE_B_SEL_PLL_MSK | pll_sel)
			| (CORE_B_CLK_DIV_W_MSK | (a53_div << CORE_B_CLK_DIV_OFF))
			| (CORE_B_AXI_CLK_DIV_W_MSK | (axi_div << CORE_B_AXI_CLK_DIV_OFF)), CRU_CLKSELS_CON(0));

	cru_writel((DEBUG_B_PCLK_DIV_W_MSK | (dbg_div << DEBUG_B_PCLK_DIV_OFF))
			| (CORE_B_ATB_DIV_W_MSK | (atclk_div << CORE_B_ATB_DIV_OFF)), CRU_CLKSELS_CON(1));
}


static void rkclk_apll_cb(struct pll_clk_set *clkset)
{
	rkclk_core_b_clk_set(CPU_SRC_ARM_PLL, clkset->a53_core_div, clkset->axi_core_div, clkset->dbg_core_div, clkset->atclk_core_div);
}


static void rkclk_gpll_cb(struct pll_clk_set *clkset)
{
	rkclk_bus_ahpclk_set(BUS_SRC_GENERAL_PLL, clkset->aclk_bus_div, clkset->hclk_bus_div, clkset->pclk_bus_div);
	rkclk_periph_ahpclk_set(PERIPH_SRC_GENERAL_PLL, clkset->aclk_peri_div, clkset->hclk_peri_div, clkset->pclk_peri_div);
}


static uint32 rkclk_get_bus_aclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(8));
	div = ((con & PDBUS_ACLK_DIV_MSK) >> PDBUS_ACLK_DIV_OFF) + 1;

	return div;
}


static uint32 rkclk_get_bus_hclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(8));
	switch ((con & PDBUS_HCLK_DIV_MSK) >> PDBUS_HCLK_DIV_OFF)
	{
		case 0:
			div = CLK_DIV_1;
			break;
		case 1:
			div = CLK_DIV_2;
			break;
		case 2:
			div = CLK_DIV_4;
			break;
		default:
			div = CLK_DIV_2;
			break;
	}

	return div;
}


static uint32 rkclk_get_bus_pclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(8));
	switch ((con & PDBUS_PCLK_DIV_MSK) >> PDBUS_PCLK_DIV_OFF)
	{
		case 0:
			div = CLK_DIV_1;
			break;
		case 1:
			div = CLK_DIV_2;
			break;
		case 2:
			div = CLK_DIV_4;
			break;
		case 3:
			div = CLK_DIV_8;
			break;
		default:
			div = CLK_DIV_4;
	}

	return div;
}


static uint32 rkclk_get_periph_aclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(9));
	div = ((con & PERI_ACLK_DIV_MSK) >> PERI_ACLK_DIV_OFF) + 1;

	return div;
}


static uint32 rkclk_get_periph_hclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(9));
	switch ((con & PERI_HCLK_DIV_MSK) >> PERI_HCLK_DIV_OFF)
	{
		case 0:
			div = CLK_DIV_1;
			break;
		case 1:
			div = CLK_DIV_2;
			break;
		case 2:
			div = CLK_DIV_4;
			break;
		default:
			div = CLK_DIV_2;
			break;
	}

	return div;
}


static uint32 rkclk_get_periph_pclk_div(void)
{
	uint32 con, div;

	con = cru_readl(CRU_CLKSELS_CON(9));
	switch ((con & PERI_PCLK_DIV_MSK) >> PERI_PCLK_DIV_OFF)
	{
		case 0:
			div = CLK_DIV_1;
			break;
		case 1:
			div = CLK_DIV_2;
			break;
		case 2:
			div = CLK_DIV_4;
			break;
		case 3:
			div = CLK_DIV_8;
			break;
		default:
			div = CLK_DIV_4;
			break;
	}

	return div;
}


/*
 * rkplat clock set pll mode
 */
void rkclk_pll_mode(int pll_id, int pll_mode)
{
	rkclk_pll_set_mode(pll_id, pll_mode);
}


/*
 * rkplat clock set pll rate by id
 */
void rkclk_set_pll_rate_by_id(enum rk_plls_id pll_id, uint32 mHz)
{
	pll_callback_f cb_f = NULL;

	if (APLLB_ID == pll_id) {
		cb_f = rkclk_apll_cb;
	} else if (GPLL_ID == pll_id) {
		cb_f = rkclk_gpll_cb;
	}

	rkclk_pll_set_rate(pll_id, mHz, cb_f);
}


/*
 * rkplat clock set for arm and general pll
 */
void rkclk_set_pll(void)
{
	rkclk_pll_set_rate(APLLB_ID, CONFIG_RKCLK_APLL_FREQ, rkclk_apll_cb);
	rkclk_pll_set_rate(GPLL_ID, CONFIG_RKCLK_GPLL_FREQ, rkclk_gpll_cb);
	rkclk_pll_set_rate(CPLL_ID, CONFIG_RKCLK_CPLL_FREQ, NULL);
}


/*
 * rkplat clock get pll rate by id
 */
uint32 rkclk_get_pll_rate_by_id(enum rk_plls_id pll_id)
{
	return rkclk_pll_get_rate(pll_id);
}


/*
 * rkplat clock get arm pll, general pll and so on
 */
void rkclk_get_pll(void)
{
	uint32 div;

	/* cpu / periph / ddr freq */
	gd->cpu_clk = rkclk_pll_get_rate(APLLB_ID);
	gd->bus_clk = rkclk_pll_get_rate(GPLL_ID);
	gd->mem_clk = rkclk_pll_get_rate(DPLL_ID);
	gd->pci_clk = rkclk_pll_get_rate(CPLL_ID);

	/* periph aclk */
	div = rkclk_get_periph_aclk_div();
	gd->arch.aclk_periph_rate_hz = gd->bus_clk / div;

	/* periph hclk */
	div = rkclk_get_periph_hclk_div();
	gd->arch.hclk_periph_rate_hz = gd->arch.aclk_periph_rate_hz / div;

	/* periph pclk */
	div = rkclk_get_periph_pclk_div();
	gd->arch.pclk_periph_rate_hz = gd->arch.aclk_periph_rate_hz / div;

	/* bus aclk */
	div = rkclk_get_bus_aclk_div();
	gd->arch.aclk_bus_rate_hz = gd->bus_clk / div;

	/* bus hclk */
	div = rkclk_get_bus_hclk_div();
	gd->arch.hclk_bus_rate_hz = gd->arch.aclk_bus_rate_hz / div;

	/* bus pclk */
	div = rkclk_get_bus_pclk_div();
	gd->arch.pclk_bus_rate_hz = gd->arch.aclk_bus_rate_hz / div;
}


/*
 * rkplat clock dump pll information
 */
void rkclk_dump_pll(void)
{
	printf("CPU's clock information:\n");

	printf("    arm pll = %ldHZ", gd->cpu_clk);
	printf("\n");

	printf("    periph pll = %ldHZ", gd->bus_clk);
	debug(", aclk_periph = %ldHZ, hclk_periph = %ldHZ, pclk_periph = %ldHZ\n",
		gd->arch.aclk_periph_rate_hz, gd->arch.hclk_periph_rate_hz, gd->arch.pclk_periph_rate_hz);
	debug("               aclk_bus = %ldHZ, hclk_bus = %ldHZ, pclk_bus = %ldHZ",
		gd->arch.aclk_bus_rate_hz, gd->arch.hclk_bus_rate_hz, gd->arch.pclk_bus_rate_hz);
	printf("\n");

	printf("    ddr pll = %ldHZ\n", gd->mem_clk);

	printf("    codec pll = %ldHZ\n", gd->pci_clk);
}


/*
 * rkplat pll select and clock div calcate
 * clock: device request freq HZ
 * even: if div needs even
 * return value:
 * high 16bit: 0 - codec pll, 1 - general pll
 * low 16bit : div
 */
static uint32 rkclk_calc_pll_and_div(uint32 clock, uint32 even)
{
	uint32 div = 0, gdiv = 0, cdiv = 0;
	uint32 pll_sel = 0; // 0: general pll, 1: codec pll

	gdiv = rkclk_calc_clkdiv(gd->bus_clk, clock, even); // general pll div
	cdiv = rkclk_calc_clkdiv(gd->pci_clk, clock, even); // codec pll div

	pll_sel = (gd->bus_clk / gdiv) >= (gd->pci_clk / cdiv);

	div = pll_sel ? gdiv : cdiv;

	return (pll_sel << 16) | div;
}


/*
 * rkplat lcdc aclk config
 * lcdc_id (lcdc id select) : 0 - lcdc0
 * pll_sel (lcdc aclk source pll select) : 0 - codec pll, 1 - general pll, 2 - usbphy pll
 * div (lcdc aclk div from pll) : 0x01 - 0x20
 */
static int rkclk_lcdc_aclk_config(uint32 lcdc_id, uint32 pll_sel, uint32 div)
{
	uint32 con = 0;

	if (lcdc_id > 0) {
		return -1;
	}

	/* lcdc0 register bit offset */
	con = 0;

	/* aclk div */
	div = (div - 1) & 0x1f;
	con |= (0x1f << (0 + 16)) | (div << 0);

	/* aclk pll source select */
	if (pll_sel == 0) {
		con |= (3 << (6 + 16)) | (0 << 6);
	} else if (pll_sel == 1){
		con |= (3 << (6 + 16)) | (1 << 6);
	} else {
		con |= (3 << (6 + 16)) | (2 << 6);
	}

	cru_writel(con, CRU_CLKSELS_CON(19));

	return 0;
}


int rkclk_lcdc_aclk_set(uint32 lcdc_id, uint32 aclk_hz)
{
	uint32 aclk_info = 0;
	uint32 pll_sel = 0, div = 0;

	/* lcdc aclk from general pll */
	pll_sel = 1;
	div = rkclk_calc_clkdiv(gd->bus_clk, aclk_hz, 0);
	aclk_info = (pll_sel << 16) | div;
	debug("rk lcdc aclk config: aclk = %dHZ, pll select = %d, div = %d\n", aclk_hz, pll_sel, div);

	rkclk_lcdc_aclk_config(lcdc_id, pll_sel, div);

	return aclk_info;
}


/*
 * rkplat vio hclk config from aclk vio0
 * div (lcdc hclk div from aclk) : 0x01 - 0x20
 */
static int rkclk_vio_hclk_set(uint32 lcdc_id, uint32 div)
{
	uint32 con = 0;

	/* dclk div */
	div = (div - 1) & 0x1f;
	con = (0x1f << (0 + 16)) | (div << 0);

	cru_writel(con, CRU_CLKSELS_CON(21));

	return 0;
}

/*
 * rkplat lcdc dclk config
 * lcdc_id (lcdc id select) : 0 - lcdc0
 * pll_sel (lcdc dclk source pll select) : 0 - codec pll, 1 - general pll, 2 - new pll
 * div (lcdc dclk div from pll) : 0x01 - 0x100
 */
static int rkclk_lcdc_dclk_config(uint32 lcdc_id, uint32 pll_sel, uint32 div)
{
	uint32 con = 0;

	if (lcdc_id > 0) {
		return -1;
	}

	con = 0;

	/* dclk pll source select */
	if (pll_sel == 0) {
		con |= (3 << (8 + 16)) | (0 << 8);
	} else if (pll_sel == 1) {
		con |= (3 << (8 + 16)) | (1 << 8);
	} else {
		con |= (3 << (8 + 16)) | (2 << 8);
	}

	/* dclk div */
	div = (div - 1) & 0xff;
	con |= (0xff << (0 + 16)) | (div << 0);

	cru_writel(con, CRU_CLKSELS_CON(20));

	return 0;
}


int rkclk_lcdc_dclk_set(uint32 lcdc_id, uint32 dclk_hz)
{
	uint32 dclk_info = 0;
	uint32 pll_sel = 0, div = 0;

	/* audi lcdc dclk from general pll */
	pll_sel = 1;
	div = rkclk_calc_clkdiv(gd->bus_clk, dclk_hz, 0);
	dclk_info = (pll_sel << 16) | div;
	debug("rk lcdc dclk set: dclk = %dHZ, pll select = %d, div = %d\n", dclk_hz, pll_sel, div);

	rkclk_lcdc_dclk_config(lcdc_id, pll_sel, div);

	return dclk_info;
}

/*
 * rkplat lcdc dclk and aclk parent pll source
 * lcdc_id (lcdc id select) : 0 - lcdc0, 1 - lcdc1
 * dclk_hz: dclk rate
 * return dclk rate
 */
int rkclk_lcdc_clk_set(uint32 lcdc_id, uint32 dclk_hz)
{
	uint32 pll_src;
	uint32 dclk_div;
	uint32 dclk_info = 0;

	rkclk_lcdc_aclk_set(lcdc_id, 300 * MHZ);
	dclk_info = rkclk_lcdc_dclk_set(lcdc_id, dclk_hz);

	dclk_div = dclk_info & 0x0000FFFF;
	// general pll
	return (rkclk_pll_get_rate(GPLL_ID) / dclk_div);
}


/*
 * rkplat set nandc clock div
 * nandc_id:	nandc id
 * pllsrc:	0: codec pll; 1: general pll;
 * freq:	nandc max freq request
 */
int rkclk_set_nandc_div(uint32 nandc_id, uint32 pllsrc, uint32 freq)
{
	uint32 parent = 0;
	uint con = 0, div = 0;

	if (pllsrc == 0) {
		con = (0 << 7) | (1 << (7 + 16));
		parent = gd->pci_clk;
	} else {
		con = (1 << 7) | (1 << (7 + 16));
		parent = gd->bus_clk;
	}

	div = rkclk_calc_clkdiv(parent, freq, 0);
	if (div == 0) {
		div = 1;
	}
	con |= (((div - 1) << 0) | (0x1f << (0 + 16)));
	cru_writel(con, CRU_CLKSELS_CON(47));

	debug("nandc clock src rate = %d, div = %d\n", parent, div);
	return 0;
}

/*
 * rkplat set mmc clock source
 * 0: codec pll; 1: general pll; 2: usbphy 480M; 3: 24M
 */
void rkclk_set_mmc_clk_src(uint32 sdid, uint32 src)
{
	src &= 0x03;
	if (0 == sdid) {
		/* sdmmc */
		cru_writel((src << 6) | (0x03 << (8 + 16)), CRU_CLKSELS_CON(50));
	} else if (1 == sdid) {
		/* sdio0 */
		cru_writel((src << 8) | (0x03 << (8 + 16)), CRU_CLKSELS_CON(48));
	} else if (2 == sdid) {
		/* emmc */
		cru_writel((src << 8) | (0x03 << (8 + 16)), CRU_CLKSELS_CON(51));
	}
}


/*
 * rkplat get mmc clock rate
 */
unsigned int rkclk_get_mmc_clk(uint32 sdid)
{
	uint32 con;
	uint32 sel;

	if (0 == sdid) {
		/* sdmmc */
		con =  cru_readl(CRU_CLKSELS_CON(50));
		sel = (con >> 8) & 0x3;
	} else if (1 == sdid) {
		/* sdio0 */
		con =  cru_readl(CRU_CLKSELS_CON(48));
		sel = (con >> 8) & 0x3;
	} else if (2 == sdid) {
		/* emmc */
		con =  cru_readl(CRU_CLKSELS_CON(51));
		sel = (con >> 8) & 0x3;
	} else {
		return 0;
	}

	/* rk3288 sd clk pll can be from 24M/usbphy 480M/general pll/codec pll, defualt 24M */
	if (sel == 0) {
		return gd->pci_clk;
	} else if (sel == 1) {
		return gd->bus_clk;
	} else if (sel == 2) {
		return (480 * MHZ);
	} else if (sel == 3) {
		return (24 * MHZ);
	} else {
		return 0;
	}
}


/*
 * rkplat set mmc clock div
 * here no check clkgate, because chip default is enable.
 */
int rkclk_set_mmc_clk_div(uint32 sdid, uint32 div)
{
	if (div == 0) {
		return -1;
	}

	if (0 == sdid) {
		/* sdmmc */
		cru_writel(((0x7Ful<<0)<<16) | ((div-1)<<0), CRU_CLKSELS_CON(50));
	} else if (1 == sdid) {
		/* sdio0 */
		cru_writel(((0x7Ful<<0)<<16) | ((div-1)<<0), CRU_CLKSELS_CON(48));
	} else if (2 == sdid) {
		/* emmc */
		cru_writel(((0x7Ful<<0)<<16) | ((div-1)<<0), CRU_CLKSELS_CON(51));
	} else {
		return -1;
	}

	return 0;
}


/*
 * rkplat get PWM clock, from pclk_bus
 * here no check clkgate, because chip default is enable.
 */
unsigned int rkclk_get_pwm_clk(uint32 pwm_id)
{
	return gd->arch.pclk_bus_rate_hz;
}


/*
 * rkplat get I2C clock, I2c0 and i2c1 from pclk_cpu, I2c2 and i2c3 from pclk_periph
 * here no check clkgate, because chip default is enable.
 */
unsigned int rkclk_get_i2c_clk(uint32 i2c_bus_id)
{
	if (i2c_bus_id == 0 || i2c_bus_id == 1) {
		return gd->arch.pclk_bus_rate_hz;
	} else {
		return gd->arch.pclk_periph_rate_hz;
	}
}


/*
 * rkplat get spi clock, spi0-2 from  cpll or gpll
 * here no check clkgate, because chip default is enable.
 */
unsigned int rkclk_get_spi_clk(uint32 spi_bus)
{
	uint32 con;
	uint32 sel;
	uint32 div;

	if (spi_bus > 2) {
		return 0;
	}

	if (spi_bus == 2) {
		con = cru_readl(CRU_CLKSELS_CON(46));
		sel = (con >> 15) & 0x1;
		div = ((con >> 8) & 0x7F) + 1;
	} else {
		con = cru_readl(CRU_CLKSELS_CON(45));
		sel = (con >> (7 + 8 * spi_bus)) & 0x1;
		div = ((con >> (0 + 8 * spi_bus)) & 0x7F) + 1;
	}

	/* rk3368 spi clk pll can be from codec pll/general pll, defualt codec pll */
	if (sel == 0) {
		return gd->pci_clk / div;
	} else {
		return gd->bus_clk / div;
	}
}


#ifdef CONFIG_SECUREBOOT_CRYPTO
/*
 * rkplat set crypto clock
 * here no check clkgate, because chip default is enable.
 */
void rkclk_set_crypto_clk(uint32 rate)
{
	uint32 parent = 0;
	uint32 div;

	parent = gd->arch.aclk_bus_rate_hz;
	div = rkclk_calc_clkdiv(parent, rate, 0);
	if (div == 0) {
		div = 1;
	}

	debug("crypto clk div = %d\n", div);
	cru_writel((3 << (14 + 16)) | ((div-1) << 14), CRU_CLKSELS_CON(10));
}
#endif /* CONFIG_SECUREBOOT_CRYPTO */
