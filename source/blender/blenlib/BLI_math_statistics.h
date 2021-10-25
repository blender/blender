/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_STATISTICS_H__
#define __BLI_MATH_STATISTICS_H__

/** \file BLI_math_statistics.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/********************************** Covariance Matrices *********************************/

void BLI_covariance_m_vn_ex(
        const int n, const float *cos_vn, const int nbr_cos_v3, const float *center, const bool use_sample_correction,
        float *r_covmat);
void BLI_covariance_m3_v3n(
        const float (*cos_v3)[3], const int nbr_cos_v3, const bool use_sample_correction,
        float r_covmat[3][3], float r_center[3]);

/**************************** Inline Definitions ******************************/
#if 0  /* None so far. */
#  if BLI_MATH_DO_INLINE
#    include "intern/math_geom_inline.c"
#  endif
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_STATISTICS_H__ */

