/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern const float ltc_mat_ggx[64 * 64 * 4];
extern const float ltc_mag_ggx[64 * 64 * 2];
extern const float bsdf_split_sum_ggx[64 * 64 * 2];
extern const float ltc_disk_integral[64 * 64];
extern const float btdf_split_sum_ggx[16][64 * 64 * 2];
extern const float blue_noise[64 * 64][4];

#ifdef __cplusplus
}
#endif
