/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

void BKE_displist_tangent_calc(const DispList *dl, float (*fnormals)[3], float (**r_tangent)[4]);

#ifdef __cplusplus
}
#endif
