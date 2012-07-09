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

#ifndef __UVEDIT_PARAMETRIZER_H__
#define __UVEDIT_PARAMETRIZER_H__

/** \file blender/editors/uvedit/uvedit_parametrizer.h
 *  \ingroup eduv
 */

#ifdef __cplusplus
extern "C" {
#endif
	
#include "BLO_sys_types.h" // for intptr_t support

typedef void ParamHandle;	/* handle to a set of charts */
typedef intptr_t ParamKey;		/* (hash) key for identifying verts and faces */
typedef enum ParamBool {
	PARAM_TRUE = 1,
	PARAM_FALSE = 0
} ParamBool;

/* Chart construction:
 * -------------------
 * - faces and seams may only be added between construct_{begin|end}
 * - the pointers to co and uv are stored, rather than being copied
 * - vertices are implicitly created
 * - in construct_end the mesh will be split up according to the seams
 * - the resulting charts must be:
 * - manifold, connected, open (at least one boundary loop)
 * - output will be written to the uv pointers
 */

ParamHandle *param_construct_begin(void);

void param_aspect_ratio(ParamHandle *handle, float aspx, float aspy);

void param_face_add(ParamHandle *handle,
					ParamKey key,
					int nverts,	
					ParamKey *vkeys,
					float **co,
					float **uv,
					ParamBool *pin,
					ParamBool *select, short *unwrap_flag);

void param_edge_set_seam(ParamHandle *handle,
						 ParamKey *vkeys);

void param_construct_end(ParamHandle *handle, ParamBool fill, ParamBool impl);
void param_delete(ParamHandle *chart);

/* Least Squares Conformal Maps:
 * -----------------------------
 * - charts with less than two pinned vertices are assigned 2 pins
 * - lscm is divided in three steps:
 * - begin: compute matrix and it's factorization (expensive)
 * - solve using pinned coordinates (cheap)
 * - end: clean up 
 * - uv coordinates are allowed to change within begin/end, for
 *   quick re-solving
 */

void param_lscm_begin(ParamHandle *handle, ParamBool live, ParamBool abf);
void param_lscm_solve(ParamHandle *handle);
void param_lscm_end(ParamHandle *handle);

/* Stretch */

void param_stretch_begin(ParamHandle *handle);
void param_stretch_blend(ParamHandle *handle, float blend);
void param_stretch_iter(ParamHandle *handle);
void param_stretch_end(ParamHandle *handle);

/* Area Smooth */

void param_smooth_area(ParamHandle *handle);

/* Packing */

void param_pack(ParamHandle *handle, float margin);

/* Average area for all charts */

void param_average(ParamHandle *handle);

/* Simple x,y scale */

void param_scale(ParamHandle *handle, float x, float y);

/* Flushing */

void param_flush(ParamHandle *handle);
void param_flush_restore(ParamHandle *handle);


#ifdef __cplusplus
}
#endif

#endif /*__UVEDIT_PARAMETRIZER_H__*/

