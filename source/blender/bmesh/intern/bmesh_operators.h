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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_OPERATORS_H__
#define __BMESH_OPERATORS_H__

/** \file blender/bmesh/intern/bmesh_operators.h
 *  \ingroup bmesh
 */

/*see comments in intern/bmesh_opdefines.c for documentation of specific operators*/

/*--------defines/enumerations for specific operators-------*/

/*quad innervert values*/
enum {
	SUBD_CORNER_INNERVERT,
	SUBD_CORNER_PATH,
	SUBD_CORNER_FAN,
	SUBD_CORNER_STRAIGHT_CUT
};

/* aligned with PROP_SMOOTH and friends */
enum {
	SUBD_FALLOFF_SMOOTH = 0,
	SUBD_FALLOFF_SPHERE,
	SUBD_FALLOFF_ROOT,
	SUBD_FALLOFF_SHARP,
	SUBD_FALLOFF_LIN,
};

enum {
	SUBDIV_SELECT_NONE,
	SUBDIV_SELECT_ORIG,
	SUBDIV_SELECT_INNER,
	SUBDIV_SELECT_LOOPCUT
};

enum {
	SIM_CMP_EQ = 0,
	SIM_CMP_GT,
	SIM_CMP_LT
};

/* subdivide_edgering */
enum {
	/* just subdiv */
	SUBD_RING_INTERP_LINEAR,

	/* single bezier spline - curve follows bezier rotation */
	SUBD_RING_INTERP_PATH,

	/* beziers based on adjacent faces (fallback to tangent) */
	SUBD_RING_INTERP_SURF,
};

/* similar face selection slot values */
enum {
	SIMFACE_MATERIAL = 201,
	SIMFACE_IMAGE,
	SIMFACE_AREA,
	SIMFACE_SIDES,
	SIMFACE_PERIMETER,
	SIMFACE_NORMAL,
	SIMFACE_COPLANAR,
#ifdef WITH_FREESTYLE
	SIMFACE_FREESTYLE
#endif
};

/* similar edge selection slot values */
enum {
	SIMEDGE_LENGTH = 101,
	SIMEDGE_DIR,
	SIMEDGE_FACE,
	SIMEDGE_FACE_ANGLE,
	SIMEDGE_CREASE,
	SIMEDGE_BEVEL,
	SIMEDGE_SEAM,
	SIMEDGE_SHARP,
#ifdef WITH_FREESTYLE
	SIMEDGE_FREESTYLE
#endif
};

/* similar vertex selection slot values */
enum {
	SIMVERT_NORMAL = 0,
	SIMVERT_FACE,
	SIMVERT_VGROUP,
	SIMVERT_EDGE
};

/* Poke face center calculation */
enum {
	BMOP_POKE_MEAN_WEIGHTED = 0,
	BMOP_POKE_MEAN,
	BMOP_POKE_BOUNDS
};

/* Bevel offset_type slot values */
enum {
	BEVEL_AMT_OFFSET,
	BEVEL_AMT_WIDTH,
	BEVEL_AMT_DEPTH,
	BEVEL_AMT_PERCENT
};

extern const BMOpDefine *bmo_opdefines[];
extern const int         bmo_opdefines_total;

/*------specific operator helper functions-------*/
void BM_mesh_esubdivide(BMesh *bm, const char edge_hflag,
                        const float smooth, const short smooth_falloff, const bool use_smooth_even,
                        const float fractal, const float along_normal,
                        const int numcuts,
                        const int seltype, const int cornertype,
                        const short use_single_edge, const short use_grid_fill,
                        const short use_only_quads,
                        const int seed);

#include "intern/bmesh_operator_api_inline.h"

#endif /* __BMESH_OPERATORS_H__ */
