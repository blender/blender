/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

void BLI_jitter_init(float (*jitarr)[2], int num);
void BLI_jitterate1(float (*jit1)[2], float (*jit2)[2], int num, float radius1);
void BLI_jitterate2(float (*jit1)[2], float (*jit2)[2], int num, float radius2);

#ifdef __cplusplus
}
#endif
