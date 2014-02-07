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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_vertex_proj.c
 *  \ingroup edsculpt
 *
 * Utility functions for getting vertex locations while painting
 * (since they may be instanced multiple times in a DerivedMesh)
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"  /* own include */


/* Opaque Structs for internal use */

/* stored while painting */
struct VertProjHandle {
	DMCoNo *vcosnos;

	bool use_update;

	/* use for update */
	float *dists_sq;

	Object *ob;
	Scene *scene;
};

/* only for passing to the callbacks */
struct VertProjUpdate {
	struct VertProjHandle *vp_handle;

	/* runtime */
	ARegion *ar;
	const float *mval_fl;
};


/* -------------------------------------------------------------------- */
/* Internal Init */

static void vpaint_proj_dm_map_cosnos_init__map_cb(void *userData, int index, const float co[3],
                                                   const float no_f[3], const short no_s[3])
{
	struct VertProjHandle *vp_handle = userData;
	DMCoNo *co_no = &vp_handle->vcosnos[index];

	/* check if we've been here before (normal should not be 0) */
	if (!is_zero_v3(co_no->no)) {
		/* remember that multiple dm verts share the same source vert */
		vp_handle->use_update = true;
		return;
	}

	copy_v3_v3(co_no->co, co);
	if (no_f) {
		copy_v3_v3(co_no->no, no_f);
	}
	else {
		normal_short_to_float_v3(co_no->no, no_s);
	}
}

static void vpaint_proj_dm_map_cosnos_init(Scene *scene, Object *ob,
                                           struct VertProjHandle *vp_handle)
{
	Mesh *me = ob->data;
	DerivedMesh *dm;

	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH | CD_MASK_ORIGINDEX);

	if (dm->foreachMappedVert) {
		memset(vp_handle->vcosnos, 0, sizeof(DMCoNo) * me->totvert);
		dm->foreachMappedVert(dm, vpaint_proj_dm_map_cosnos_init__map_cb, vp_handle, DM_FOREACH_USE_NORMAL);
	}
	else {
		DMCoNo *v_co_no = vp_handle->vcosnos;
		int a;
		for (a = 0; a < me->totvert; a++, v_co_no++) {
			dm->getVertCo(dm, a, v_co_no->co);
			dm->getVertNo(dm, a, v_co_no->no);
		}
	}

	dm->release(dm);
}


/* -------------------------------------------------------------------- */
/* Internal Update */

/* Same as init but take mouse location into account */

static void vpaint_proj_dm_map_cosnos_update__map_cb(void *userData, int index, const float co[3],
                                                     const float no_f[3], const short no_s[3])
{
	struct VertProjUpdate *vp_update = userData;
	struct VertProjHandle *vp_handle = vp_update->vp_handle;

	DMCoNo *co_no = &vp_handle->vcosnos[index];

	/* find closest vertex */
	{
		/* first find distance to this vertex */
		float co_ss[2];  /* screenspace */

		if (ED_view3d_project_float_object(vp_update->ar,
		                                   co, co_ss,
		                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
		{
			const float dist_sq = len_squared_v2v2(vp_update->mval_fl, co_ss);
			if (dist_sq > vp_handle->dists_sq[index]) {
				/* bail out! */
				return;
			}

			vp_handle->dists_sq[index] = dist_sq;
		}
	}
	/* continue with regular functionality */

	copy_v3_v3(co_no->co, co);
	if (no_f) {
		copy_v3_v3(co_no->no, no_f);
	}
	else {
		normal_short_to_float_v3(co_no->no, no_s);
	}
}

static void vpaint_proj_dm_map_cosnos_update(struct VertProjHandle *vp_handle,
                                             ARegion *ar, const float mval_fl[2])
{
	struct VertProjUpdate vp_update = {vp_handle, ar, mval_fl};

	Scene *scene = vp_handle->scene;
	Object *ob = vp_handle->ob;
	Mesh *me = ob->data;
	DerivedMesh *dm;

	/* quick sanity check - we shouldn't have to run this if there are no modifiers */
	BLI_assert(BLI_listbase_is_empty(&ob->modifiers) == false);

	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH | CD_MASK_ORIGINDEX);

	/* highly unlikely this will become unavailable once painting starts (perhaps with animated modifiers) */
	if (LIKELY(dm->foreachMappedVert)) {
		fill_vn_fl(vp_handle->dists_sq, me->totvert, FLT_MAX);

		dm->foreachMappedVert(dm, vpaint_proj_dm_map_cosnos_update__map_cb, &vp_update, DM_FOREACH_USE_NORMAL);
	}

	dm->release(dm);
}


/* -------------------------------------------------------------------- */
/* Public Functions */

struct VertProjHandle *ED_vpaint_proj_handle_create(Scene *scene, Object *ob,
                                                    DMCoNo **r_vcosnos)
{
	struct VertProjHandle *vp_handle = MEM_mallocN(sizeof(struct VertProjHandle), __func__);
	Mesh *me = ob->data;

	/* setup the handle */
	vp_handle->vcosnos = MEM_mallocN(sizeof(DMCoNo) * me->totvert, "vertexcosnos map");
	vp_handle->use_update = false;

	/* sets 'use_update' if needed */
	vpaint_proj_dm_map_cosnos_init(scene, ob, vp_handle);

	if (vp_handle->use_update) {
		vp_handle->dists_sq = MEM_mallocN(sizeof(float) * me->totvert, __func__);

		vp_handle->ob = ob;
		vp_handle->scene = scene;
	}
	else {
		vp_handle->dists_sq = NULL;

		vp_handle->ob = NULL;
		vp_handle->scene = NULL;
	}

	*r_vcosnos = vp_handle->vcosnos;
	return vp_handle;
}

void  ED_vpaint_proj_handle_update(struct VertProjHandle *vp_handle,
                                   ARegion *ar, const float mval_fl[2])
{
	if (vp_handle->use_update) {
		vpaint_proj_dm_map_cosnos_update(vp_handle, ar, mval_fl);
	}
}

void  ED_vpaint_proj_handle_free(struct VertProjHandle *vp_handle)
{
	if (vp_handle->use_update) {
		MEM_freeN(vp_handle->dists_sq);
	}

	MEM_freeN(vp_handle->vcosnos);
	MEM_freeN(vp_handle);
}
