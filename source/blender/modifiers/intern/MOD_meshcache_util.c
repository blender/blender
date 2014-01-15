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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshcache_util.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "MEM_guardedalloc.h"

#include "MOD_meshcache_util.h"

void MOD_meshcache_calc_range(const float frame, const char interp,
                              const int frame_tot,
                              int r_index_range[2], float *r_factor)
{
	if (interp == MOD_MESHCACHE_INTERP_NONE) {
		r_index_range[0] = r_index_range[1] = max_ii(0, min_ii(frame_tot - 1, iroundf(frame)));
		*r_factor = 1.0f; /* dummy */
	}
	else {
		const float tframe = floorf(frame);
		const float range  = frame - tframe;
		r_index_range[0] = (int)tframe;
		if (range <= FRAME_SNAP_EPS) {
			/* we're close enough not to need blending */
			r_index_range[1] = r_index_range[0];
			*r_factor = 1.0f; /* dummy */
		}
		else {
			/* blend between 2 frames */
			r_index_range[1] = r_index_range[0] + 1;
			*r_factor = range;
		}

		/* clamp */
		if ((r_index_range[0] >= frame_tot) ||
		    (r_index_range[1] >= frame_tot))
		{
			r_index_range[0] = r_index_range[1] = frame_tot - 1;
			*r_factor = 1.0f; /* dummy */
		}
		else if ((r_index_range[0] < 0) ||
		         (r_index_range[1] < 0))
		{
			r_index_range[0] = r_index_range[1] = 0;
			*r_factor = 1.0f; /* dummy */
		}
	}
}
