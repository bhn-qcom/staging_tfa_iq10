/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QTI_SECBOOT_H
#define QTI_SECBOOT_H

/* Execute Qualcomm secure-boot provisioning after the BL31 boot milestone. */
void qti_secboot_post_milestone_setup(void);

/* Update boot-image anti-rollback fuse versions through TME. */
int qti_secboot_update_rollback_fuse_version(void);

#endif /* QTI_SECBOOT_H */
