/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_CAPI_H_
#define OPENSUBDIV_CAPI_H_

#include "opensubdiv_capi_type.h"

#ifdef __cplusplus
extern "C" {
#endif

// Global initialization/deinitialization.
//
// Supposed to be called from main thread.
void openSubdiv_init(void);
void openSubdiv_cleanup(void);

int openSubdiv_getVersionHex(void);

#ifdef __cplusplus
}
#endif

#endif  // OPENSUBDIV_CAPI_H_
