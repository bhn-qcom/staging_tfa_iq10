/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * BSP data types for the QTI clock driver: clock groups (clocks, power
 * domains, access clocks and voltage votes) and the PLL source tree.
 */

#ifndef CLOCK_BSP_H
#define CLOCK_BSP_H

#include <stdbool.h>
#include <stdint.h>

#include <drivers/qti/clock/clock_types.h>
#include <drivers/qti/pwr_utils/voltage_level.h>

/* Clock source node. */
struct clock_source {
	struct clock_source_desc	hw_source;
	uint32_t			ref_count;
	struct clock_source		*source;
};

/* Rail voltage request descriptor. */
struct clock_voltage_request {
	const char		*rail;
	enum rail_voltage_level	vlvl;
	uint32_t		rail_addr; /* runtime — 0 until first enable */
	int			hlvl;      /* runtime — 0 until first enable */
	enum chipinfo_part	part;
	uint32_t		part_idx;
};

/* Group of clocks/power-domains/requests managed together. */
struct clock_group {
	struct clock_desc		*clks;
	struct clock_power_domain_desc	*pwr_domains;
	struct clock_desc		*access_clks;
	struct clock_voltage_request	*volt_reqs;
	uint32_t			ref_count;
	uint32_t			clk_count;
	uint32_t			pwr_domain_count;
	uint32_t			access_clk_count;
	uint32_t			volt_req_count;
};

/* Top-level BSP data structure. */
struct clock_config {
	struct clock_group	*clock_groups;
	struct clock_source	*sources;
};

#endif /* CLOCK_BSP_H */
