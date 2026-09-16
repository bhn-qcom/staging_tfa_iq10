/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>

#include "IxErrno.h"
#include "TmeInterfaces.h"
#include "TmeMessage.h"
#include "TmeMessagesTags.h"

/* TME_MSG_CBOR_TAG_UPDATE_ROLLBACK_VERSION has an empty request payload. */
int TmeUpdateRollbackVersion(void)
{
	int ret = E_FAILURE;
	tmeUpdateRollbackVersionRsp_t response = { E_FAILURE, 0U };
	size_t response_len = 0U;

	if (TransceiveMessage(TME_MSG_CBOR_TAG_UPDATE_ROLLBACK_VERSION,
				     NULL, 0U, &response, sizeof(response),
				     &response_len) == E_SUCCESS &&
	    response_len == sizeof(response) && response.status == E_SUCCESS) {
		ret = E_SUCCESS;
	}

	return ret;
}
