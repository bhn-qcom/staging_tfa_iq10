/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * TF-A clock driver: clock-group bring-up/teardown and source enables.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common/debug.h>
#include <lib/mmio.h>

#include <drivers/qti/clock/clock.h>
#include <drivers/qti/clock/clock_bsp.h>
#include <drivers/qti/clock/clock_driver.h>
#include <drivers/qti/clock/clock_rail.h>

/*
 * BSP configuration data, provided by the chipset back-end.
 */
extern struct clock_config clock_cfg;

static struct clock_drv_ctxt clock_drv_ctxt = {
	.cfg = &clock_cfg,
};

int clock_source_enable(struct clock_source *source)
{
	if (source == NULL) {
		return -1;
	}

	if (source->source != NULL) {
		if (clock_source_enable(source->source) != 0) {
			return -1;
		}
	}

	/* Only votable PLLs are driven here; no source declares an RPM resource. */
	if (source->ref_count == 0U) {
		clock_hal_enable_source(&source->hw_source);
		if (clock_hal_wait_for_source_on(&source->hw_source) != 0) {
			ERROR("Clock: source mode_addr=0x%lx failed to lock\n",
			      (unsigned long)source->hw_source.mode_addr);
			return -1;
		}
	}
	source->ref_count++;

	return 0;
}

static int clock_group_enable_internal(enum clock_group_type group_type,
					struct clock_group *group)
{
	struct clock_desc *clock;
	struct clock_power_domain_desc *pd;
	bool timeout = false;

	rail_vote_apply(group);

	if (group->access_clks != NULL) {
		group->access_clk_count = 0U;
		for (clock = group->access_clks; clock->cbcr_addr != 0U;
		     clock++) {
			group->access_clk_count++;

			if (chipinfo_is_part_disabled(clock->part,
						   clock->part_idx)) {
				continue;
			}

			/* Skip the accessor if TF-A already enabled it, so a retry
			 * after a failed group enable does not lose ownership. */
			if (!clock->tfa_enabled) {
				clock_hal_set_clock(clock, true);
			}
			if (clock_hal_wait_for_clock_on(clock) != 0) {
				uint32_t raw = mmio_read_32(clock->cbcr_addr);

				ERROR("Clock: group %d access clock cbcr=0x%lx timed out raw=0x%08x enable=%u hw_ctl=%u off=%u\n",
				      group_type, (unsigned long)clock->cbcr_addr,
				      raw,
				      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_ENABLE_FMSK) != 0U,
				      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_HW_CTL_FMSK) != 0U,
				      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_OFF_FMSK) != 0U);
				if (clock->vote_reg.addr != 0U) {
					uint32_t vote = mmio_read_32(clock->vote_reg.addr);

					ERROR("Clock: group %d access clock cbcr=0x%lx vote_reg=0x%lx raw=0x%08x voted=%u\n",
					      group_type, (unsigned long)clock->cbcr_addr,
					      (unsigned long)clock->vote_reg.addr,
					      vote, (vote & clock->vote_reg.mask) != 0U);
				}
				return -1;
			}
		}
	}

	if (group->pwr_domains != NULL) {
		group->pwr_domain_count = 0U;
		for (pd = group->pwr_domains;
		     (pd->gdscr_addr != 0U) || (pd->vote_reg.addr != 0U);
		     pd++) {
			group->pwr_domain_count++;

			if (chipinfo_is_part_disabled(pd->part, pd->part_idx)) {
				continue;
			}

			if (!pd->tfa_enabled) {
				clock_hal_enable_power_domain(pd);
			}

			/* Poll immediately: some chipsets have sibling GDSCs
			 * (e.g. GPU) that must see this one fully up before
			 * their own enable is issued. */
			if (clock_hal_wait_for_power_domain_on(pd) != 0) {
				uint32_t gdscr = mmio_read_32(pd->gdscr_addr);
				uint32_t cfg_gdscr = mmio_read_32(pd->gdscr_addr +
								  HAL_CLK_CFG_GDSCR_OFFSET);

				ERROR("Clock: group %d power domain gdscr=0x%lx timed out gdscr=0x%08x cfg_gdscr=0x%08x pwr_up_complete=%u\n",
				      group_type, (unsigned long)pd->gdscr_addr,
				      gdscr, cfg_gdscr,
				      (cfg_gdscr & HAL_CLK_CFG_GDSCR_POWER_UP_COMPLETE_FMSK) != 0U);
				return -1;
			}
		}
	}

	group->clk_count = 0U;
	for (clock = group->clks; clock->cbcr_addr != 0U; clock++) {
		group->clk_count++;

		if (chipinfo_is_part_disabled(clock->part, clock->part_idx)) {
			continue;
		}

		if (!clock->tfa_enabled) {
			clock_hal_set_clock(clock, true);
		}
	}
	for (clock = group->clks; clock->cbcr_addr != 0U; clock++) {
		if (chipinfo_is_part_disabled(clock->part, clock->part_idx)) {
			continue;
		}

		if (clock_hal_wait_for_clock_on(clock) != 0) {
			uint32_t raw = mmio_read_32(clock->cbcr_addr);

			ERROR("Clock: group %d clock cbcr=0x%lx timed out raw=0x%08x enable=%u hw_ctl=%u off=%u\n",
			      group_type, (unsigned long)clock->cbcr_addr, raw,
			      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_ENABLE_FMSK) != 0U,
			      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_HW_CTL_FMSK) != 0U,
			      (raw & HAL_CLK_BRANCH_CTRL_REG_CLK_OFF_FMSK) != 0U);
			if (clock->vote_reg.addr != 0U) {
				uint32_t vote = mmio_read_32(clock->vote_reg.addr);

				ERROR("Clock: group %d clock cbcr=0x%lx vote_reg=0x%lx raw=0x%08x voted=%u\n",
				      group_type, (unsigned long)clock->cbcr_addr,
				      (unsigned long)clock->vote_reg.addr,
				      vote, (vote & clock->vote_reg.mask) != 0U);
			}
			timeout = true;
		}
	}

	if (timeout) {
		ERROR("Clock: group %d clock enable timed out\n", group_type);
		return -1;
	}

	return 0;
}

static int clock_group_disable_internal(enum clock_group_type group_type,
					 struct clock_group *group)
{
	struct clock_desc *clock;
	struct clock_power_domain_desc *pd;
	uint32_t i;

	for (i = group->clk_count; i > 0U; i--) {
		clock = &group->clks[i - 1U];
		/* Disable the resource only if TF-A enabled it. */
		if (clock->tfa_enabled) {
			clock_hal_set_clock(clock, false);
		}
	}

	if (group->pwr_domains != NULL) {
		for (i = group->pwr_domain_count; i > 0U; i--) {
			pd = &group->pwr_domains[i - 1U];
			if (pd->tfa_enabled) {
				clock_hal_disable_power_domain(pd);
				/* Confirm the GDSC is off before the rail
				 * vote backing it is cleared below. */
				clock_hal_wait_for_power_domain_off(pd);
			}
		}
	}

	if (group->access_clks != NULL) {
		for (i = group->access_clk_count; i > 0U; i--) {
			clock = &group->access_clks[i - 1U];
			if (clock->tfa_enabled) {
				clock_hal_set_clock(clock, false);
			}
		}
	}

	rail_vote_clear(group);

	return 0;
}

static bool clock_init(void)
{
	int ret;

	if (clock_drv_ctxt.initialized) {
		return true;
	}

	rail_vote_init();

	ret = clock_init_image(&clock_drv_ctxt);
	if (ret != 0) {
		ERROR("Clock: init failed (%d)\n", ret);
		return false;
	}

	clock_drv_ctxt.initialized = true;
	return true;
}

static void clock_init_done(void)
{
	int ret;

	if (!clock_drv_ctxt.initialized) {
		return;
	}

	ret = clock_post_init_image(&clock_drv_ctxt);
	if (ret != 0) {
		ERROR("Clock: init done failed (%d)\n", ret);
	}

	/* Release the driver-lifetime cx/mx rail holds taken at init. */
	rail_vote_deinit();
}

void qti_clock_init(void (*fn)(void))
{
	if (!clock_init()) {
		panic();
	}

	if (fn != NULL) {
		fn();
	}

	clock_init_done();
}

int clock_group_enable(enum clock_group_type group_type)
{
	struct clock_group *group;

	if ((group_type >= CLOCK_GROUP_TOTAL) ||
	    (clock_drv_ctxt.cfg->clock_groups == NULL)) {
		ERROR("Clock: clock_group_enable: invalid group_type=%d\n",
		      group_type);
		return -1;
	}

	group = &clock_drv_ctxt.cfg->clock_groups[group_type];
	if (group->clks == NULL) {
		ERROR("Clock: clock_group_enable: group %d has no clocks\n",
		      group_type);
		return -1;
	}

	if (group->ref_count++ == 0U) {
		if (clock_group_enable_internal(group_type, group) != 0) {
			/* Roll back the vote so a retry re-runs bring-up. */
			group->ref_count--;
			ERROR("Clock: clock_group_enable(%d) failed\n",
			      group_type);
			return -1;
		}
	}

	return 0;
}

int clock_group_disable(enum clock_group_type group_type)
{
	struct clock_group *group;

	if ((group_type >= CLOCK_GROUP_TOTAL) ||
	    (clock_drv_ctxt.cfg->clock_groups == NULL)) {
		ERROR("Clock: clock_group_disable: invalid group_type=%d\n",
		      group_type);
		return -1;
	}

	group = &clock_drv_ctxt.cfg->clock_groups[group_type];
	if (group->clks == NULL) {
		ERROR("Clock: clock_group_disable: group %d has no clocks\n",
		      group_type);
		return -1;
	}

	if ((group->ref_count > 0U) && (group->ref_count-- == 1U)) {
		if (clock_group_disable_internal(group_type, group) != 0) {
			ERROR("Clock: clock_group_disable(%d) failed\n",
			      group_type);
			return -1;
		}
	}

	return 0;
}
