/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void GPU_init(void);
void GPU_exit(void);
bool GPU_is_init(void);

#ifdef __cplusplus
}
#endif
