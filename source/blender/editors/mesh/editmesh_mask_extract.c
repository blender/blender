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
 * The Original Code is Copyright (C) 2019 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_shrinkwrap.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"

#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h" /* own include */

static bool paint_mask_extract_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob != NULL && ob->mode == OB_MODE_SCULPT) {
    if (ob->sculpt->bm) {
      CTX_wm_operator_poll_msg_set(C, "The mask can not be extracted with dyntopo activated");
      return false;
    }
    else {
      return ED_operator_object_active_editable_mesh(C);
    }
  }
  return false;
}

static int paint_mask_extract_exec(bContext *C, wmOperator *op)
{
  struct Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);

  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  ED_object_sculptmode_exit(C, depsgraph);

  BKE_sculpt_mask_layers_ensure(ob, NULL);

  Mesh *mesh = ob->data;
  Mesh *new_mesh = BKE_mesh_copy(bmain, mesh);

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(new_mesh);
  BMesh *bm;
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     new_mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BMEditMesh *em = BKE_editmesh_create(bm, false);
  BMVert *v;
  BMEdge *ed;
  BMFace *f;
  BMIter iter;
  BMIter face_iter;

  /* Delete all unmasked faces */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

  float mask_threshold = RNA_float_get(op->ptr, "mask_threshold");
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    bool keep_face = true;
    BM_ITER_ELEM (v, &face_iter, f, BM_VERTS_OF_FACE) {
      const float mask = BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
      if (mask < mask_threshold) {
        keep_face = false;
        break;
      }
    }
    BM_elem_flag_set(f, BM_ELEM_TAG, !keep_face);
  }

  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    mul_v3_v3(v->co, ob->scale);
  }

  if (RNA_boolean_get(op->ptr, "add_boundary_loop")) {
    BM_ITER_MESH (ed, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_set(ed, BM_ELEM_TAG, BM_edge_is_boundary(ed));
    }
    edbm_extrude_edges_indiv(em, op, BM_ELEM_TAG, false);

    int smooth_iterations = RNA_int_get(op->ptr, "smooth_iterations");
    for (int repeat = 0; repeat < smooth_iterations; repeat++) {
      BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_set(v, BM_ELEM_TAG, !BM_vert_is_boundary(v));
      }
      for (int i = 0; i < 3; i++) {
        if (!EDBM_op_callf(em,
                           op,
                           "smooth_vert verts=%hv factor=%f mirror_clip_x=%b mirror_clip_y=%b "
                           "mirror_clip_z=%b "
                           "clip_dist=%f use_axis_x=%b use_axis_y=%b use_axis_z=%b",
                           BM_ELEM_TAG,
                           1.0,
                           false,
                           false,
                           false,
                           0.1,
                           true,
                           true,
                           true)) {
          continue;
        }
      }

      BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_set(v, BM_ELEM_TAG, BM_vert_is_boundary(v));
      }
      for (int i = 0; i < 1; i++) {
        if (!EDBM_op_callf(em,
                           op,
                           "smooth_vert verts=%hv factor=%f mirror_clip_x=%b mirror_clip_y=%b "
                           "mirror_clip_z=%b "
                           "clip_dist=%f use_axis_x=%b use_axis_y=%b use_axis_z=%b",
                           BM_ELEM_TAG,
                           0.5,
                           false,
                           false,
                           false,
                           0.1,
                           true,
                           true,
                           true)) {
          continue;
        }
      }
    }
  }

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

  BKE_id_free(bmain, new_mesh);
  new_mesh = BKE_mesh_from_bmesh_nomain(bm,
                                        (&(struct BMeshToMeshParams){
                                            .calc_object_remap = false,
                                        }),
                                        mesh);

  BKE_editmesh_free(em);
  MEM_freeN(em);

  if (new_mesh->totvert == 0) {
    BKE_id_free(bmain, new_mesh);
    return OPERATOR_FINISHED;
  }

  ushort local_view_bits = 0;
  if (v3d && v3d->localvd) {
    local_view_bits = v3d->local_view_uuid;
  }
  Object *new_ob = ED_object_add_type(C, OB_MESH, NULL, ob->loc, ob->rot, false, local_view_bits);
  BKE_mesh_nomain_to_mesh(new_mesh, new_ob->data, new_ob, &CD_MASK_EVERYTHING, true);

  /* Remove the Face Sets as they need to be recreated when entering Sculpt Mode in the new object.
   * TODO(pablodobarro): In the future we can try to preserve them from the original mesh. */
  Mesh *new_ob_mesh = new_ob->data;
  CustomData_free_layers(&new_ob_mesh->pdata, CD_SCULPT_FACE_SETS, new_ob_mesh->totpoly);

  if (RNA_boolean_get(op->ptr, "apply_shrinkwrap")) {
    BKE_shrinkwrap_mesh_nearest_surface_deform(C, new_ob, ob);
  }

  if (RNA_boolean_get(op->ptr, "add_solidify")) {
    ED_object_modifier_add(
        op->reports, bmain, scene, new_ob, "mask_extract_solidify", eModifierType_Solidify);
    SolidifyModifierData *sfmd = (SolidifyModifierData *)BKE_modifiers_findby_name(
        new_ob, "mask_extract_solidify");
    if (sfmd) {
      sfmd->offset = -0.05f;
    }
  }

  BKE_mesh_calc_normals(new_ob->data);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
  BKE_mesh_batch_cache_dirty_tag(new_ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_ob->data);

  return OPERATOR_FINISHED;
}

void MESH_OT_paint_mask_extract(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask Extract";
  ot->description = "Create a new mesh object from the current paint mask";
  ot->idname = "MESH_OT_paint_mask_extract";

  /* api callbacks */
  ot->poll = paint_mask_extract_poll;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = paint_mask_extract_exec;

  ot->flag = OPTYPE_REGISTER;

  RNA_def_float(
      ot->srna,
      "mask_threshold",
      0.5f,
      0.0f,
      1.0f,
      "Threshold",
      "Minimum mask value to consider the vertex valid to extract a face from the original mesh",
      0.0f,
      1.0f);
  RNA_def_boolean(ot->srna,
                  "add_boundary_loop",
                  true,
                  "Add Boundary Loop",
                  "Add an extra edge loop to better preserve the shape when applying a "
                  "subdivision surface modifier");
  RNA_def_int(ot->srna,
              "smooth_iterations",
              4,
              0,
              INT_MAX,
              "Smooth Iterations",
              "Smooth iterations applied to the extracted mesh",
              0,
              20);
  RNA_def_boolean(ot->srna,
                  "apply_shrinkwrap",
                  true,
                  "Project to Sculpt",
                  "Project the extracted mesh into the original sculpt");
  RNA_def_boolean(ot->srna,
                  "add_solidify",
                  true,
                  "Extract as Solid",
                  "Extract the mask as a solid object with a solidify modifier");
}

static void slice_paint_mask(BMesh *bm, bool invert, bool fill_holes, float mask_threshold)
{
  BMVert *v;
  BMFace *f;
  BMIter iter;
  BMIter face_iter;

  /* Delete all masked faces */
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  BLI_assert(cd_vert_mask_offset != -1);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    bool keep_face = true;
    BM_ITER_ELEM (v, &face_iter, f, BM_VERTS_OF_FACE) {
      const float mask = BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
      if (mask < mask_threshold) {
        keep_face = false;
        break;
      }
    }
    if (invert) {
      keep_face = !keep_face;
    }
    BM_elem_flag_set(f, BM_ELEM_TAG, keep_face);
  }

  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_mesh_elem_hflag_enable_all(bm, BM_EDGE, BM_ELEM_TAG, false);

  if (fill_holes) {
    BM_mesh_edgenet(bm, false, true);
    BM_mesh_normals_update(bm);
    BMO_op_callf(bm,
                 (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                 "triangulate faces=%hf quad_method=%i ngon_method=%i",
                 BM_ELEM_TAG,
                 0,
                 0);

    BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
    BMO_op_callf(bm,
                 (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                 "recalc_face_normals faces=%hf",
                 BM_ELEM_TAG);
    BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  }
}

static int paint_mask_slice_exec(bContext *C, wmOperator *op)
{
  struct Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);

  BKE_sculpt_mask_layers_ensure(ob, NULL);

  Mesh *mesh = ob->data;
  Mesh *new_mesh = BKE_mesh_copy(bmain, mesh);

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob, "mask slice");
    /* TODO: The ideal functionality would be to preserve the current face sets and add a new one
     * for the new triangles, but this data-layer needs to be rebuild in order to make sculpt mode
     * not crash when modifying the geometry. */
    CustomData_free_layers(&mesh->pdata, CD_SCULPT_FACE_SETS, mesh->totpoly);
  }

  BMesh *bm;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(new_mesh);
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     new_mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  slice_paint_mask(
      bm, false, RNA_boolean_get(op->ptr, "fill_holes"), RNA_float_get(op->ptr, "mask_threshold"));
  BKE_id_free(bmain, new_mesh);
  new_mesh = BKE_mesh_from_bmesh_nomain(bm,
                                        (&(struct BMeshToMeshParams){
                                            .calc_object_remap = false,
                                        }),
                                        mesh);
  BM_mesh_free(bm);

  if (RNA_boolean_get(op->ptr, "new_object")) {
    ushort local_view_bits = 0;
    if (v3d && v3d->localvd) {
      local_view_bits = v3d->local_view_uuid;
    }
    Object *new_ob = ED_object_add_type(
        C, OB_MESH, NULL, ob->loc, ob->rot, false, local_view_bits);
    Mesh *new_ob_mesh = BKE_mesh_copy(bmain, mesh);

    const BMAllocTemplate allocsize_new_ob = BMALLOC_TEMPLATE_FROM_ME(new_ob_mesh);
    bm = BM_mesh_create(&allocsize_new_ob,
                        &((struct BMeshCreateParams){
                            .use_toolflags = true,
                        }));

    BM_mesh_bm_from_me(bm,
                       new_ob_mesh,
                       (&(struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));

    slice_paint_mask(bm,
                     true,
                     RNA_boolean_get(op->ptr, "fill_holes"),
                     RNA_float_get(op->ptr, "mask_threshold"));
    BKE_id_free(bmain, new_ob_mesh);
    new_ob_mesh = BKE_mesh_from_bmesh_nomain(bm,
                                             (&(struct BMeshToMeshParams){
                                                 .calc_object_remap = false,
                                             }),
                                             mesh);
    BM_mesh_free(bm);

    BKE_mesh_nomain_to_mesh(new_ob_mesh, new_ob->data, new_ob, &CD_MASK_MESH, true);
    BKE_mesh_calc_normals(new_ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
    BKE_mesh_batch_cache_dirty_tag(new_ob->data, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_ob->data);
  }

  BKE_mesh_nomain_to_mesh(new_mesh, ob->data, ob, &CD_MASK_MESH, true);
  BKE_mesh_calc_normals(ob->data);

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_end(ob);
    SculptSession *ss = ob->sculpt;
    /* Rebuild a new valid Face Set layer for the object. */
    ss->face_sets = CustomData_add_layer(
        &mesh->pdata, CD_SCULPT_FACE_SETS, CD_CALLOC, NULL, mesh->totpoly);
    for (int i = 0; i < mesh->totpoly; i++) {
      ss->face_sets[i] = 1;
    }
  }

  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void MESH_OT_paint_mask_slice(wmOperatorType *ot)
{
  PropertyRNA *prop;
  /* identifiers */
  ot->name = "Mask Slice";
  ot->description = "Slices the paint mask from the mesh";
  ot->idname = "MESH_OT_paint_mask_slice";

  /* api callbacks */
  ot->poll = paint_mask_extract_poll;
  ot->exec = paint_mask_slice_exec;

  ot->flag = OPTYPE_REGISTER;

  RNA_def_float(
      ot->srna,
      "mask_threshold",
      0.5f,
      0.0f,
      1.0f,
      "Threshold",
      "Minimum mask value to consider the vertex valid to extract a face from the original mesh",
      0.0f,
      1.0f);
  prop = RNA_def_boolean(
      ot->srna, "fill_holes", true, "Fill Holes", "Fill holes after slicing the mask");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "new_object",
                         true,
                         "Slice to New Object",
                         "Create a new object from the sliced mask");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
