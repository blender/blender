/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_screen.h"
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
#include "ED_undo.h"
#include "ED_view3d.h"

#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h" /* own include */

static bool geometry_extract_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob != NULL && ob->mode == OB_MODE_SCULPT) {
    if (ob->sculpt->bm) {
      CTX_wm_operator_poll_msg_set(C, "The geometry can not be extracted with dyntopo activated");
      return false;
    }
    return ED_operator_object_active_editable_mesh(C);
  }
  return false;
}

typedef struct GeometryExtactParams {
  /* For extracting Face Sets. */
  int active_face_set;

  /* For extracting Mask. */
  float mask_threshold;

  /* Common parameters. */
  bool add_boundary_loop;
  int num_smooth_iterations;
  bool apply_shrinkwrap;
  bool add_solidify;
} GeometryExtractParams;

/* Function that tags in BMesh the faces that should be deleted in the extracted object. */
typedef void(GeometryExtractTagMeshFunc)(struct BMesh *, GeometryExtractParams *);

static int geometry_extract_apply(bContext *C,
                                  wmOperator *op,
                                  GeometryExtractTagMeshFunc *tag_fn,
                                  GeometryExtractParams *params)
{
  struct Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);

  ED_object_sculptmode_exit(C, depsgraph);

  BKE_sculpt_mask_layers_ensure(depsgraph, bmain, ob, NULL);

  /* Ensures that deformation from sculpt mode is taken into account before duplicating the mesh to
   * extract the geometry. */
  CTX_data_ensure_evaluated_depsgraph(C);

  Mesh *mesh = ob->data;
  Mesh *new_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

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
                         .calc_vert_normal = true,
                     }));

  BMEditMesh *em = BKE_editmesh_create(bm);

  /* Generate the tags for deleting geometry in the extracted object. */
  tag_fn(bm, params);

  /* Delete all tagged faces. */
  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMVert *v;
  BMEdge *ed;
  BMIter iter;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    mul_v3_v3(v->co, ob->scale);
  }

  if (params->add_boundary_loop) {
    BM_ITER_MESH (ed, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_set(ed, BM_ELEM_TAG, BM_edge_is_boundary(ed));
    }
    edbm_extrude_edges_indiv(em, op, BM_ELEM_TAG, false);

    for (int repeat = 0; repeat < params->num_smooth_iterations; repeat++) {
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

  BKE_editmesh_free_data(em);
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
  BKE_mesh_nomain_to_mesh(new_mesh, new_ob->data, new_ob);

  /* Remove the Face Sets as they need to be recreated when entering Sculpt Mode in the new object.
   * TODO(pablodobarro): In the future we can try to preserve them from the original mesh. */
  Mesh *new_ob_mesh = new_ob->data;
  CustomData_free_layer_named(&new_ob_mesh->pdata, ".sculpt_face_set", new_ob_mesh->totpoly);

  /* Remove the mask from the new object so it can be sculpted directly after extracting. */
  CustomData_free_layers(&new_ob_mesh->vdata, CD_PAINT_MASK, new_ob_mesh->totvert);

  BKE_mesh_copy_parameters_for_eval(new_ob_mesh, mesh);

  if (params->apply_shrinkwrap) {
    BKE_shrinkwrap_mesh_nearest_surface_deform(C, new_ob, ob);
  }

  if (params->add_solidify) {
    ED_object_modifier_add(
        op->reports, bmain, scene, new_ob, "geometry_extract_solidify", eModifierType_Solidify);
    SolidifyModifierData *sfmd = (SolidifyModifierData *)BKE_modifiers_findby_name(
        new_ob, "mask_extract_solidify");
    if (sfmd) {
      sfmd->offset = -0.05f;
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
  BKE_mesh_batch_cache_dirty_tag(new_ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_ob->data);

  return OPERATOR_FINISHED;
}

static void geometry_extract_tag_masked_faces(BMesh *bm, GeometryExtractParams *params)
{
  const float threshold = params->mask_threshold;

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    bool keep_face = true;
    BMVert *v;
    BMIter face_iter;
    BM_ITER_ELEM (v, &face_iter, f, BM_VERTS_OF_FACE) {
      const float mask = BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
      if (mask < threshold) {
        keep_face = false;
        break;
      }
    }
    BM_elem_flag_set(f, BM_ELEM_TAG, !keep_face);
  }
}

static void geometry_extract_tag_face_set(BMesh *bm, GeometryExtractParams *params)
{
  const int tag_face_set_id = params->active_face_set;

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  const int cd_face_sets_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int face_set_id = abs(BM_ELEM_CD_GET_INT(f, cd_face_sets_offset));
    BM_elem_flag_set(f, BM_ELEM_TAG, face_set_id != tag_face_set_id);
  }
}

static int paint_mask_extract_exec(bContext *C, wmOperator *op)
{
  GeometryExtractParams params;
  params.mask_threshold = RNA_float_get(op->ptr, "mask_threshold");
  params.num_smooth_iterations = RNA_int_get(op->ptr, "smooth_iterations");
  params.add_boundary_loop = RNA_boolean_get(op->ptr, "add_boundary_loop");
  params.apply_shrinkwrap = RNA_boolean_get(op->ptr, "apply_shrinkwrap");
  params.add_solidify = RNA_boolean_get(op->ptr, "add_solidify");

  /* Push an undo step prior to extraction.
   * Note: A second push happens after the operator due to
   * the OPTYPE_UNDO flag; having an initial undo step here
   * is just needed to preserve the active object pointer.
   * 
   * Fixes T103261.
   */
  ED_undo_push_op(C, op);

  return geometry_extract_apply(C, op, geometry_extract_tag_masked_faces, &params);
}

static int paint_mask_extract_invoke(bContext *C, wmOperator *op, const wmEvent *e)
{
  return WM_operator_props_popup_confirm(C, op, e);
}

static void geometry_extract_props(StructRNA *srna)
{
  RNA_def_boolean(srna,
                  "add_boundary_loop",
                  true,
                  "Add Boundary Loop",
                  "Add an extra edge loop to better preserve the shape when applying a "
                  "subdivision surface modifier");
  RNA_def_int(srna,
              "smooth_iterations",
              4,
              0,
              INT_MAX,
              "Smooth Iterations",
              "Smooth iterations applied to the extracted mesh",
              0,
              20);
  RNA_def_boolean(srna,
                  "apply_shrinkwrap",
                  true,
                  "Project to Sculpt",
                  "Project the extracted mesh into the original sculpt");
  RNA_def_boolean(srna,
                  "add_solidify",
                  true,
                  "Extract as Solid",
                  "Extract the mask as a solid object with a solidify modifier");
}

void MESH_OT_paint_mask_extract(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask Extract";
  ot->description = "Create a new mesh object from the current paint mask";
  ot->idname = "MESH_OT_paint_mask_extract";

  /* api callbacks */
  ot->poll = geometry_extract_poll;
  ot->invoke = paint_mask_extract_invoke;
  ot->exec = paint_mask_extract_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

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

  geometry_extract_props(ot->srna);
}

static int face_set_extract_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
  ED_workspace_status_text(C, TIP_("Click on the mesh to select a Face Set"));
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EYEDROPPER);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int face_set_extract_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        WM_cursor_modal_restore(CTX_wm_window(C));
        ED_workspace_status_text(C, NULL);

        /* This modal operator uses and eyedropper to pick a Face Set from the mesh. This ensures
         * that the mouse clicked in a viewport region and its coordinates can be used to ray-cast
         * the PBVH and update the active Face Set ID. */
        bScreen *screen = CTX_wm_screen(C);
        ARegion *region = BKE_screen_find_main_region_at_xy(screen, SPACE_VIEW3D, event->xy);

        if (!region) {
          return OPERATOR_CANCELLED;
        }

        const float mval[2] = {event->xy[0] - region->winrct.xmin,
                               event->xy[1] - region->winrct.ymin};

        Object *ob = CTX_data_active_object(C);
        const int face_set_id = ED_sculpt_face_sets_active_update_and_get(C, ob, mval);
        if (face_set_id == SCULPT_FACE_SET_NONE) {
          return OPERATOR_CANCELLED;
        }

        GeometryExtractParams params;
        params.active_face_set = face_set_id;
        params.num_smooth_iterations = 0;
        params.add_boundary_loop = false;
        params.apply_shrinkwrap = true;
        params.add_solidify = true;
        return geometry_extract_apply(C, op, geometry_extract_tag_face_set, &params);
      }
      break;
    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      WM_cursor_modal_restore(CTX_wm_window(C));
      ED_workspace_status_text(C, NULL);

      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

void MESH_OT_face_set_extract(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Face Set Extract";
  ot->description = "Create a new mesh object from the selected Face Set";
  ot->idname = "MESH_OT_face_set_extract";

  /* api callbacks */
  ot->poll = geometry_extract_poll;
  ot->invoke = face_set_extract_invoke;
  ot->modal = face_set_extract_modal;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  geometry_extract_props(ot->srna);
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

  BKE_sculpt_mask_layers_ensure(NULL, NULL, ob, NULL);

  Mesh *mesh = ob->data;
  Mesh *new_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob, op);
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
    Mesh *new_ob_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

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

    /* Remove the mask from the new object so it can be sculpted directly after slicing. */
    CustomData_free_layers(&new_ob_mesh->vdata, CD_PAINT_MASK, new_ob_mesh->totvert);

    BKE_mesh_nomain_to_mesh(new_ob_mesh, new_ob->data, new_ob);
    BKE_mesh_copy_parameters_for_eval(new_ob->data, mesh);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
    BKE_mesh_batch_cache_dirty_tag(new_ob->data, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_ob->data);
  }

  BKE_mesh_nomain_to_mesh(new_mesh, ob->data, ob);

  if (ob->mode == OB_MODE_SCULPT) {
    SculptSession *ss = ob->sculpt;
    ss->face_sets = CustomData_get_layer_named(
        &((Mesh *)ob->data)->pdata, CD_PROP_INT32, ".sculpt_face_set");
    if (ss->face_sets) {
      /* Assign a new Face Set ID to the new faces created by the slice operation. */
      const int next_face_set_id = ED_sculpt_face_sets_find_next_available_id(ob->data);
      ED_sculpt_face_sets_initialize_none_to_id(ob->data, next_face_set_id);
    }
    ED_sculpt_undo_geometry_end(ob);
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
  ot->poll = geometry_extract_poll;
  ot->exec = paint_mask_slice_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

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
