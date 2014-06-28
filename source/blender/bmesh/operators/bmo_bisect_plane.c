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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_bisect_plane.c
 *  \ingroup bmesh
 *
 * Wrapper around #BM_mesh_bisect_plane
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_stackdefines.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define ELE_NEW 1
#define ELE_INPUT 2

void bmo_bisect_plane_exec(BMesh *bm, BMOperator *op)
{
	const float dist  = BMO_slot_float_get(op->slots_in, "dist");
	const bool use_snap_center = BMO_slot_bool_get(op->slots_in,  "use_snap_center");
	const bool clear_outer = BMO_slot_bool_get(op->slots_in,  "clear_outer");
	const bool clear_inner = BMO_slot_bool_get(op->slots_in,  "clear_inner");

	float plane_co[3];
	float plane_no[3];
	float plane[4];

	BMO_slot_vec_get(op->slots_in, "plane_co", plane_co);
	BMO_slot_vec_get(op->slots_in, "plane_no", plane_no);

	if (is_zero_v3(plane_no)) {
		BMO_error_raise(bm, op, BMERR_MESH_ERROR, "Zero normal given");
		return;
	}

	plane_from_point_normal_v3(plane, plane_co, plane_no);

	/* tag geometry to bisect */
	BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
	BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "geom", BM_ALL_NOLOOP, ELE_INPUT);


	BM_mesh_bisect_plane(bm, plane, use_snap_center, true,
	                     ELE_NEW, dist);


	if (clear_outer || clear_inner) {
		/* Use an array of vertices because 'geom' contains both vers and edges that may use them.
		 * Removing a vert may remove and edge which is later checked by BMO_ITER.
		 * over-alloc the total possible vert count */
		const int vert_arr_max = min_ii(bm->totvert, BMO_slot_buffer_count(op->slots_in, "geom"));
		BMVert **vert_arr = MEM_mallocN(sizeof(*vert_arr) * (size_t)vert_arr_max, __func__);
		BMOIter siter;
		BMVert *v;
		float plane_inner[4];
		float plane_outer[4];

		STACK_DECLARE(vert_arr);

		copy_v3_v3(plane_outer, plane);
		copy_v3_v3(plane_inner, plane);
		plane_outer[3] = plane[3] - dist;
		plane_inner[3] = plane[3] + dist;

		STACK_INIT(vert_arr, vert_arr_max);

		BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
			if ((clear_outer && plane_point_side_v3(plane_outer, v->co) > 0.0f) ||
			    (clear_inner && plane_point_side_v3(plane_inner, v->co) < 0.0f))
			{
				STACK_PUSH(vert_arr, v);
			}
		}

		while ((v = STACK_POP(vert_arr))) {
			BM_vert_kill(bm, v);
		}

		MEM_freeN(vert_arr);
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, ELE_NEW | ELE_INPUT);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_cut.out", BM_VERT | BM_EDGE, ELE_NEW);
}
