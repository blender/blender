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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/layer_utils.c
 *  \ingroup bke
 */

#include <string.h>

#include "BLI_array.h"
#include "BLI_listbase.h"

#include "BKE_collection.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

Base **BKE_view_layer_array_from_bases_in_mode_params(
        ViewLayer *view_layer, View3D *v3d, uint *r_len,
        const struct ObjectsInModeParams *params)
{
	if (params->no_dup_data) {
		FOREACH_BASE_IN_MODE_BEGIN(view_layer, v3d, params->object_mode, base_iter) {
			ID *id = base_iter->object->data;
			if (id) {
				id->tag |= LIB_TAG_DOIT;
			}
		} FOREACH_BASE_IN_MODE_END;
	}

	Base **base_array = NULL;
	BLI_array_declare(base_array);

	FOREACH_BASE_IN_MODE_BEGIN(view_layer, v3d, params->object_mode, base_iter) {
		if (params->filter_fn) {
			if (!params->filter_fn(base_iter->object, params->filter_userdata)) {
				continue;
			}
		}
		if (params->no_dup_data) {
			ID *id = base_iter->object->data;
			if (id) {
				if (id->tag & LIB_TAG_DOIT) {
					id->tag &= ~LIB_TAG_DOIT;
				}
				else {
					continue;
				}
			}
		}
		BLI_array_append(base_array, base_iter);
	} FOREACH_BASE_IN_MODE_END;

	base_array = MEM_reallocN(base_array, sizeof(*base_array) * BLI_array_len(base_array));
	/* We always need a valid allocation (prevent crash on free). */
	if (base_array == NULL) {
		base_array = MEM_mallocN(0, __func__);
	}
	*r_len = BLI_array_len(base_array);
	return base_array;
}

Object **BKE_view_layer_array_from_objects_in_mode_params(
        ViewLayer *view_layer, View3D *v3d, uint *r_len,
        const struct ObjectsInModeParams *params)
{
	Base **base_array = BKE_view_layer_array_from_bases_in_mode_params(
	        view_layer, v3d, r_len, params);
	if (base_array != NULL) {
		for (uint i = 0; i < *r_len; i++) {
			((Object **)base_array)[i] = base_array[i]->object;
		}
	}
	return (Object **)base_array;
}

bool BKE_view_layer_filter_edit_mesh_has_uvs(Object *ob, void *UNUSED(user_data))
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		if (em != NULL) {
			if (CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV) != -1) {
				return true;
			}
		}
	}
	return false;
}

bool BKE_view_layer_filter_edit_mesh_has_edges(Object *ob, void *UNUSED(user_data))
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		if (em != NULL) {
			if (em->bm->totedge != 0) {
				return true;
			}
		}
	}
	return false;
}
