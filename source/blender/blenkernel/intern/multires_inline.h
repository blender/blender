/*
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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup bke
 */

#ifndef __MULTIRES_INLINE_H__
#define __MULTIRES_INLINE_H__

#include "BKE_multires.h"
#include "BLI_math_vector.h"

BLI_INLINE void BKE_multires_construct_tangent_matrix(
        float tangent_matrix[3][3],
        const float dPdu[3],
        const float dPdv[3],
        const int corner)
{
	if (corner == 0) {
		copy_v3_v3(tangent_matrix[0], dPdv);
		copy_v3_v3(tangent_matrix[1], dPdu);
		mul_v3_fl(tangent_matrix[0], -1.0f);
		mul_v3_fl(tangent_matrix[1], -1.0f);
	}
	else if (corner == 1) {
		copy_v3_v3(tangent_matrix[0], dPdu);
		copy_v3_v3(tangent_matrix[1], dPdv);
		mul_v3_fl(tangent_matrix[1], -1.0f);
	}
	else if (corner == 2) {
		copy_v3_v3(tangent_matrix[0], dPdv);
		copy_v3_v3(tangent_matrix[1], dPdu);
	}
	else if (corner == 3) {
		copy_v3_v3(tangent_matrix[0], dPdu);
		copy_v3_v3(tangent_matrix[1], dPdv);
		mul_v3_fl(tangent_matrix[0], -1.0f);
	}
	cross_v3_v3v3(tangent_matrix[2], dPdu, dPdv);
	normalize_v3(tangent_matrix[0]);
	normalize_v3(tangent_matrix[1]);
	normalize_v3(tangent_matrix[2]);
}

#endif  /* __MULTIRES_INLINE_H__ */
