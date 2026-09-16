/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <common/debug.h>
#include <stdbool.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <qti_plat.h>

#include <drivers/qti/fuseprov/fuseprov.h>
#include <drivers/qti/fuseprov/fuseprov_mrc_cfg.h>
#include <drivers/qti/fuseprov/fuseprov_port_tme.h>
#include <drivers/qti/pmic/pm_pon.h>
#include <TmeInterfaces.h>

#include <qti_secboot.h>

int qti_fuseprov_init(void);

#if defined(QTI_FUSEPROV_TEST)
#define QFPROM_RAW_OEM_CONFIG_ROW1_MSB 0x360C0164

static void qti_fuseprov_read_test(const fuseprov_transport_t *transport)
{
	uint32_t fuse_data[2] = { 0 };
	fuseprov_err_t ret;

	INFO("Fuseprov test: reading QFPROM_CORR_OEM_CONFIG_ROW1_MSB "
	     "(addr:0x%X)\n", QFPROM_RAW_OEM_CONFIG_ROW1_MSB);

	ret = fuseprov_row_read(transport, QFPROM_RAW_OEM_CONFIG_ROW1_MSB,
				FUSEPROV_ADDR_CORR, fuse_data);
	if (ret != FUSEPROV_OK) {
		INFO("Fuseprov test: read failed ret=%d\n", ret);
		return;
	}

	INFO("Fuseprov test: PASS fuseData:0x%08X%08X\n",
	     fuse_data[1], fuse_data[0]);
}
#endif

static __dead2 __unused void qti_fuseprov_trigger_reset(void)
{
	NOTICE("Fuseprov-Reset: pshold_configing\n");
	pm_app_ps_hold_cfg(RESET_TYPE_HARD_RESET);
	NOTICE("Fuseprov-Reset: pshold_cfgdone\n");
	qti_platform_psci_system_reset();
	mmio_write_32(QTI_PS_HOLD_REG, 0U);
	NOTICE("Fuseprov-Reset: Written the Register for PS Hold to Low\n");

	while (true)
		wfi();
}

/*
 * Qualcomm secure-boot provisioning entry point for the BL31 post-milestone
 * phase. qti_fuseprov_init() owns SEC.DAT processing and the associated
 * PS_HOLD reset sequence; do not add another reset here.
 */
void qti_secboot_post_milestone_setup(void)
{
	int ret;

	ret = qti_secboot_update_rollback_fuse_version();
	if (ret != E_SUCCESS) {
		ERROR("Secboot: rollback fuse version update failed (%d)\n", ret);
		return;
	}

	(void)qti_fuseprov_init();
}

int qti_secboot_update_rollback_fuse_version(void)
{
	return TmeUpdateRollbackVersion();
}

/* Locate the TME-authenticated SEC.DAT buffer and provision its fuses. */
int qti_fuseprov_init(void)
{
	fuseprov_error_etype ret;
	const fuseprov_transport_t *transport;
	uint32_t secelf_len = 4096;
	uintptr_t secelf_pa = 0x87452000;

	if (secelf_pa == 0 || secelf_len == 0 ||
	    secelf_len > FUSEPROV_SECDAT_BUFFER_SIZE) {
		ERROR("Fuseprov: sec.elf region out of bounds (0x%lx, %u bytes)\n",
		      (unsigned long)secelf_pa, secelf_len);
		return -1;
	}

	if (qti_mmap_add_dynamic_region(secelf_pa, secelf_len,
					MT_RO_DATA | MT_SECURE) != 0) {
		ERROR("Fuseprov: failed to map sec.elf buffer\n");
		return -1;
	}

	transport = fuseprov_port_tme_get();

#if defined(QTI_FUSEPROV_TEST)
	qti_fuseprov_read_test(transport);
#endif

	ret = fuseprov_blow_fuses_sec_elf_v3(transport, (uint8_t *)secelf_pa,
					     secelf_len);
	pm_app_ps_hold_cfg(RESET_TYPE_HARD_RESET);
	NOTICE("Fuseprov-Reset: psci_reset_done\n");
	NOTICE("Fuseprov-Reset: Writing the Register for PS Hold to Low\n");
	mmio_write_32(QTI_PS_HOLD_REG, 0U);
	NOTICE("Fuseprov-Reset: Written the Register for PS Hold to Low\n");

#if defined(QTI_FUSEPROV_TEST)
	qti_fuseprov_read_test(transport);
#endif

	switch (ret) {
	case FUSEPROV_SUCCESS:
		NOTICE("Fuseprov: fuse provisioning complete\n");
		break;
	case FUSEPROV_SECDAT_LOCK_BLOWN:
		NOTICE("Fuseprov: fuse provisioning skipped, write permission disabled\n");
		break;
	case FUSEPROV_SECDAT_MAGIC_MISMATCH:
	case FUSEPROV_SECDAT_DEFAULT_NOFUSES:
		NOTICE("Fuseprov: no fuses to blow\n");
		break;
	default:
		ERROR("Fuseprov: fuse blow failed with error %d\n", ret);
		break;
	}

	if (qti_mmap_remove_dynamic_region(secelf_pa, secelf_len) != 0)
		ERROR("Fuseprov: failed to unmap sec.elf buffer\n");

	if (ret == FUSEPROV_SUCCESS || ret == FUSEPROV_SECDAT_LOCK_BLOWN)
		return FUSEPROV_SUCCESS;

	return (int)ret;
}
