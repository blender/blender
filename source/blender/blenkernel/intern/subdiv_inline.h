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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subdiv_inline.h
 *  \ingroup bke
 */

#ifndef __BKE_SUBDIV_INLINE_H__
#define __BKE_SUBDIV_INLINE_H__

#include "BKE_subdiv.h"

BLI_INLINE void BKE_subdiv_ptex_face_uv_to_grid_uv(
        const float ptex_u, const float ptex_v,
        float *r_grid_u, float *r_grid_v)
{
	*r_grid_u = 1.0f - ptex_v;
	*r_grid_v = 1.0f - ptex_u;
}

BLI_INLINE int BKE_subdiv_grid_size_from_level(const int level)
{
	return (1 << (level - 1)) + 1;
}

#endif  /* __BKE_SUBDIV_INLINE_H__ */
