/* arch/arm/mach-omap2/omap44xx_muxtbl.h
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __OMAP44XX_MUXTBL_H__
#define __OMAP44XX_MUXTBL_H__

#include <linux/io.h>
#include "iomap.h"
#include "mux44xx.h"
#include "soc.h"

#define OMAP4_MUXTBL_DOMAIN_CORE	0
#define OMAP4_MUXTBL_DOMAIN_WKUP	1

#define OMAP4_MUXTBL(_domain, _M0, _mux_value, _gpio, _label)	\
{									\
	.gpio = {							\
		.gpio = _gpio,						\
		.label = _label,					\
	},								\
	.domain = _domain,						\
	.mux = OMAP4_MUX(_M0, _mux_value),				\
	.pin = #_M0,							\
}

extern void __init omap4_muxtbl_init(void);

extern int __init omap4_muxtbl_add_mux(struct omap_muxtbl *muxtbl);

static inline u32 omap_readl(u32 pa)
{
	return __raw_readl(OMAP2_L4_IO_ADDRESS(pa));
}

static inline void omap_writew(u16 v, u32 pa)
{
	__raw_writew(v, OMAP2_L4_IO_ADDRESS(pa));
}

static inline void omap_writel(u32 v, u32 pa)
{
	__raw_writel(v, OMAP2_L4_IO_ADDRESS(pa));
}

#endif /* __OMAP44XX_MUXTBL_H__ */
