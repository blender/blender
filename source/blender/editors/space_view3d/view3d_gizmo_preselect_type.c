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

/** \file view3d_gizmo_preselect_type.c
 *  \ingroup wm
 *
 * \name Preselection Gizmo
 *
 * Use for tools to hover over data before activation.
 *
 * \note This is a slight mis-use of gizmo's, since clicking performs no action.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bmesh.h"

#include "ED_screen.h"
#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Edge-Ring Pre-Select Gizmo API
 *
 * \{ */

typedef struct MeshEdgeRingGizmo3D {
	wmGizmo gizmo;
	Object **objects;
	uint     objects_len;
	int object_index;
	int edge_index;
	struct EditMesh_PreSelEdgeRing *psel;
} MeshEdgeRingGizmo3D;

static void gizmo_preselect_edgering_draw(const bContext *UNUSED(C), wmGizmo *gz)
{
	MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
	if (gz_ring->object_index != -1) {
		Object *ob = gz_ring->objects[gz_ring->object_index];
		EDBM_preselect_edgering_draw(gz_ring->psel, ob->obmat);
	}
}

static int gizmo_preselect_edgering_test_select(
        bContext *C, wmGizmo *gz, const int mval[2])
{
	MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
	struct {
		Object *ob;
		BMEdge *eed;
		float dist;
		int ob_index;
	} best = {
		.dist = ED_view3d_select_dist_px(),
	};

	struct {
		int object_index;
		int edge_index;
	} prev = {
		.object_index = gz_ring->object_index,
		.edge_index = gz_ring->edge_index,
	};

	if (gz_ring->objects == NULL) {
		ViewLayer *view_layer = CTX_data_view_layer(C);
		gz_ring->objects = BKE_view_layer_array_from_objects_in_edit_mode(
		        view_layer, &gz_ring->objects_len);
	}

	ViewContext vc;
	em_setup_viewcontext(C, &vc);
	copy_v2_v2_int(vc.mval, mval);

	for (uint ob_index = 0; ob_index < gz_ring->objects_len; ob_index++) {
		Object *ob_iter = gz_ring->objects[ob_index];
		ED_view3d_viewcontext_init_object(&vc, ob_iter);
		BMEdge *eed_test = EDBM_edge_find_nearest_ex(&vc, &best.dist, NULL, false, false, NULL);
		if (eed_test) {
			best.ob = ob_iter;
			best.eed = eed_test;
			best.ob_index = ob_index;
		}
	}

	BMesh *bm = NULL;
	if (best.eed) {
		gz_ring->object_index = best.ob_index;
		bm = BKE_editmesh_from_object(gz_ring->objects[gz_ring->object_index])->bm;
		BM_mesh_elem_index_ensure(bm, BM_EDGE);
		gz_ring->edge_index = BM_elem_index_get(best.eed);
	}
	else {
		gz_ring->object_index = -1;
		gz_ring->edge_index = -1;
	}


	if ((prev.object_index == gz_ring->object_index) &&
	    (prev.edge_index == gz_ring->edge_index))
	{
		/* pass (only recalculate on change) */
	}
	else {
		if (best.eed) {
			const float (*coords)[3] = NULL;
			{
				Object *ob = gz_ring->objects[gz_ring->object_index];
				Depsgraph *depsgraph = CTX_data_depsgraph(C);
				Mesh *me_eval = (Mesh *)DEG_get_evaluated_id(depsgraph, ob->data);
				if (me_eval->runtime.edit_data) {
					coords = me_eval->runtime.edit_data->vertexCos;
				}
			}
			EDBM_preselect_edgering_update_from_edge(gz_ring->psel, bm, best.eed, 1, coords);
		}
		else {
			EDBM_preselect_edgering_clear(gz_ring->psel);
		}

		RNA_int_set(gz->ptr, "object_index", gz_ring->object_index);
		RNA_int_set(gz->ptr, "edge_index", gz_ring->edge_index);

		ARegion *ar = CTX_wm_region(C);
		ED_region_tag_redraw(ar);
	}

	// return best.eed ? 0 : -1;
	return -1;
}

static void gizmo_preselect_edgering_setup(wmGizmo *gz)
{
	MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
	if (gz_ring->psel == NULL) {
		gz_ring->psel = EDBM_preselect_edgering_create();
	}
	gz_ring->object_index = -1;
}

static void gizmo_preselect_edgering_free(wmGizmo *gz)
{
	MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
	EDBM_preselect_edgering_destroy(gz_ring->psel);
	gz_ring->psel = NULL;
	MEM_SAFE_FREE(gz_ring->objects);
}

static int gizmo_preselect_edgering_invoke(
        bContext *UNUSED(C), wmGizmo *UNUSED(gz), const wmEvent *UNUSED(event))
{
	return OPERATOR_PASS_THROUGH;
}


static void GIZMO_GT_preselect_edgering_3d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_preselect_edgering_3d";

	/* api callbacks */
	gzt->invoke = gizmo_preselect_edgering_invoke;
	gzt->draw = gizmo_preselect_edgering_draw;
	gzt->test_select = gizmo_preselect_edgering_test_select;
	gzt->setup = gizmo_preselect_edgering_setup;
	gzt->free = gizmo_preselect_edgering_free;

	gzt->struct_size = sizeof(MeshEdgeRingGizmo3D);

	RNA_def_int(gzt->srna, "object_index", -1, -1, INT_MAX, "Object Index", "", -1, INT_MAX);
	RNA_def_int(gzt->srna, "edge_index", -1, -1, INT_MAX, "Edge Index", "", -1, INT_MAX);
}

void ED_gizmotypes_preselect_3d(void)
{
	WM_gizmotype_append(GIZMO_GT_preselect_edgering_3d);
}

/** \} */
