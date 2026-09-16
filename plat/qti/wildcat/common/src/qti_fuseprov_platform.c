/*
 * Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <stddef.h>
#include <stdint.h>

#include <drivers/qti/fuseprov/fuseprov.h>
#include <drivers/qti/fuseprov/fuseprov_port_tme.h>

/* Blow fuses from an explicitly supplied SEC.DAT buffer. */
int qti_fuseprov_blow_fuses_and_reset(const uint8_t *secdat_buffer,
					      size_t secdat_len)
{
	fuseprov_error_etype ret;
	const fuseprov_transport_t *transport;

	if (secdat_buffer == NULL || secdat_len == 0) {
		ERROR("Fuseprov: Invalid SEC.DAT buffer\n");
		return -1;
	}

	NOTICE("Fuseprov: Starting fuse provisioning and reset sequence\n");

	transport = fuseprov_port_tme_get();
	if (transport == NULL) {
		ERROR("Fuseprov: Failed to get TME transport\n");
		return -1;
	}

	ret = fuseprov_blow_fuses_sec_elf_v3(transport,
					     (uint8_t *)secdat_buffer,
					     secdat_len);
	if (ret != FUSEPROV_SUCCESS && ret != FUSEPROV_SECDAT_LOCK_BLOWN) {
		ERROR("Fuseprov: Fuse provisioning failed with error %d\n", ret);
		return ret;
	}

	NOTICE("Fuseprov: Fuse provisioning complete, reset deferred\n");
	return FUSEPROV_SUCCESS;
}
