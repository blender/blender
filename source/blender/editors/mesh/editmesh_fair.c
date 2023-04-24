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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 *
 * Interactive editmesh knife tool.
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_smallhash.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_layer.h"
#include "BKE_mesh_fair.h"
#include "BKE_report.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h" /* own include */

static EnumPropertyItem prop_edit_mesh_fair_selection_mode_items[] = {
    {
        MESH_FAIRING_DEPTH_POSITION,
        "POSITION",
        0,
        "Position",
        "Fair positions",
    },
    {
        MESH_FAIRING_DEPTH_TANGENCY,
        "TANGENCY",
        0,
        "Tangency",
        "Fair tangency",
    },
    {0, NULL, 0, NULL, NULL},
};

static int edbm_fair_vertices_exec(bContext *C, wmOperator *op)
{
  const int mode = RNA_enum_get(op->ptr, "mode");
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      CTX_data_scene(C), view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if ((em->bm->totvertsel == 0)) {
      continue;
    }

    BMesh *bm = em->bm;
    BMVert *v;
    BMIter iter;
    int i;
    bool *fairing_mask = MEM_calloc_arrayN(bm->totvert, sizeof(bool), "fairing mask");
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (!BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        continue;
      }
      if (BM_vert_is_boundary(v)) {
        continue;
      }
      if (!BM_vert_is_manifold(v)) {
        continue;
      }
      fairing_mask[i] = true;
    }
    BKE_bmesh_prefair_and_fair_verts(bm, fairing_mask, mode);
    MEM_freeN(fairing_mask);

    EDBM_mesh_normals_update(em);
    EDBM_update(obedit->data,
                &(const struct EDBMUpdate_Params){
                    .calc_looptri = false,
                    .calc_normals = true,
                    .is_destructive = true,
                });
  }

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}
