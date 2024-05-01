/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"
#include "BKE_shrinkwrap.hh"

#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "bmesh_tools.hh"

#include "MEM_guardedalloc.h"

#include "mesh_intern.hh" /* own include */

static bool geometry_extract_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob != nullptr && ob->mode == OB_MODE_SCULPT) {
    if (ob->sculpt->bm) {
      CTX_wm_operator_poll_msg_set(C, "The geometry cannot be extracted with dyntopo activated");
      return false;
    }
    return ED_operator_object_active_editable_mesh(C);
  }
  return false;
}

struct GeometryExtractParams {
  /* For extracting Face Sets. */
  int active_face_set;

  /* For extracting Mask. */
  float mask_threshold;

  /* Common parameters. */
  bool add_boundary_loop;
  int num_smooth_iterations;
  bool apply_shrinkwrap;
  bool add_solidify;
};

/* Function that tags in BMesh the faces that should be deleted in the extracted object. */
using GeometryExtractTagMeshFunc = void(BMesh *, GeometryExtractParams *);

static int geometry_extract_apply(bContext *C,
                                  wmOperator *op,
                                  GeometryExtractTagMeshFunc *tag_fn,
                                  GeometryExtractParams *params)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);

  ED_object_sculptmode_exit(C, depsgraph);

  BKE_sculpt_mask_layers_ensure(depsgraph, bmain, ob, nullptr);

  /* Ensures that deformation from sculpt mode is taken into account before duplicating the mesh to
   * extract the geometry. */
  CTX_data_ensure_evaluated_depsgraph(C);

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  Mesh *new_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(new_mesh);
  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams mesh_to_bm_params{};
  mesh_to_bm_params.calc_face_normal = true;
  mesh_to_bm_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, new_mesh, &mesh_to_bm_params);

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
                           true))
        {
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
                           true))
        {
          continue;
        }
      }
    }
  }

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

  BKE_id_free(bmain, new_mesh);
  BMeshToMeshParams bm_to_mesh_params{};
  bm_to_mesh_params.calc_object_remap = false;
  new_mesh = BKE_mesh_from_bmesh_nomain(bm, &bm_to_mesh_params, mesh);

  /* Remove the Face Sets as they need to be recreated when entering Sculpt Mode in the new object.
   * TODO(pablodobarro): In the future we can try to preserve them from the original mesh. */
  new_mesh->attributes_for_write().remove(".sculpt_face_set");

  /* Remove the mask from the new object so it can be sculpted directly after extracting. */
  new_mesh->attributes_for_write().remove(".sculpt_mask");

  BKE_editmesh_free_data(em);
  MEM_freeN(em);

  if (new_mesh->verts_num == 0) {
    BKE_id_free(bmain, new_mesh);
    return OPERATOR_FINISHED;
  }

  ushort local_view_bits = 0;
  if (v3d && v3d->localvd) {
    local_view_bits = v3d->local_view_uid;
  }
  Object *new_ob = blender::ed::object::add_type(
      C, OB_MESH, nullptr, ob->loc, ob->rot, false, local_view_bits);
  BKE_mesh_nomain_to_mesh(new_mesh, static_cast<Mesh *>(new_ob->data), new_ob);

  if (params->apply_shrinkwrap) {
    BKE_shrinkwrap_mesh_nearest_surface_deform(CTX_data_depsgraph_pointer(C), scene, new_ob, ob);
  }

  if (params->add_solidify) {
    blender::ed::object::modifier_add(
        op->reports, bmain, scene, new_ob, "geometry_extract_solidify", eModifierType_Solidify);
    SolidifyModifierData *sfmd = (SolidifyModifierData *)BKE_modifiers_findby_name(
        new_ob, "mask_extract_solidify");
    if (sfmd) {
      sfmd->offset = -0.05f;
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
  BKE_mesh_batch_cache_dirty_tag(static_cast<Mesh *>(new_ob->data), BKE_MESH_BATCH_DIRTY_ALL);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_ob->data);

  return OPERATOR_FINISHED;
}

static void geometry_extract_tag_masked_faces(BMesh *bm, GeometryExtractParams *params)
{
  const float threshold = params->mask_threshold;

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

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
    const int face_set = BM_ELEM_CD_GET_INT(f, cd_face_sets_offset);
    BM_elem_flag_set(f, BM_ELEM_TAG, face_set != tag_face_set_id);
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
   * NOTE: A second push happens after the operator due to
   * the OPTYPE_UNDO flag; having an initial undo step here
   * is just needed to preserve the active object pointer.
   *
   * Fixes #103261.
   */
  ED_undo_push_op(C, op);

  return geometry_extract_apply(C, op, geometry_extract_tag_masked_faces, &params);
}

static int paint_mask_extract_invoke(bContext *C, wmOperator *op, const wmEvent *e)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, e, IFACE_("Create Mesh From Paint Mask"), IFACE_("Extract"));
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

  RNA_def_float_factor(
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

static int face_set_extract_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  using namespace blender::ed;
  if (!CTX_wm_region_view3d(C)) {
    return OPERATOR_CANCELLED;
  }
  ARegion *region = CTX_wm_region(C);

  const float mval[2] = {float(event->xy[0] - region->winrct.xmin),
                         float(event->xy[1] - region->winrct.ymin)};

  Object *ob = CTX_data_active_object(C);
  const int face_set_id = sculpt_paint::face_set::active_update_and_get(C, ob, mval);
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

void MESH_OT_face_set_extract(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Face Set Extract";
  ot->description = "Create a new mesh object from the selected Face Set";
  ot->idname = "MESH_OT_face_set_extract";

  /* api callbacks */
  ot->poll = geometry_extract_poll;
  ot->invoke = face_set_extract_invoke;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  geometry_extract_props(ot->srna);
}

static void slice_paint_mask(BMesh *bm, bool invert, bool fill_holes, float mask_threshold)
{
  BMVert *v;
  BMFace *f;
  BMIter iter;
  BMIter face_iter;

  /* Delete all masked faces */
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
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
  using namespace blender;
  using namespace blender::ed;
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);

  BKE_sculpt_mask_layers_ensure(nullptr, nullptr, ob, nullptr);

  bool create_new_object = RNA_boolean_get(op->ptr, "new_object");
  bool fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
  float mask_threshold = RNA_float_get(op->ptr, "mask_threshold");

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  Mesh *new_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

  /* Undo crashes when new object is created in the middle of a sculpt, see #87243. */
  if (ob->mode == OB_MODE_SCULPT && !create_new_object) {
    sculpt_paint::undo::geometry_begin(ob, op);
  }

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(new_mesh);
  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams mesh_to_bm_params{};
  mesh_to_bm_params.calc_face_normal = true;
  BM_mesh_bm_from_me(bm, new_mesh, &mesh_to_bm_params);

  slice_paint_mask(bm, false, fill_holes, mask_threshold);
  BKE_id_free(bmain, new_mesh);
  BMeshToMeshParams bm_to_mesh_params{};
  bm_to_mesh_params.calc_object_remap = false;
  new_mesh = BKE_mesh_from_bmesh_nomain(bm, &bm_to_mesh_params, mesh);
  BM_mesh_free(bm);

  if (create_new_object) {
    ushort local_view_bits = 0;
    if (v3d && v3d->localvd) {
      local_view_bits = v3d->local_view_uid;
    }
    Object *new_ob = blender::ed::object::add_type(
        C, OB_MESH, nullptr, ob->loc, ob->rot, false, local_view_bits);
    Mesh *new_ob_mesh = (Mesh *)BKE_id_copy(bmain, &mesh->id);

    const BMAllocTemplate allocsize_new_ob = BMALLOC_TEMPLATE_FROM_ME(new_ob_mesh);
    bm = BM_mesh_create(&allocsize_new_ob, &bm_create_params);

    BM_mesh_bm_from_me(bm, new_ob_mesh, &mesh_to_bm_params);

    slice_paint_mask(bm, true, fill_holes, mask_threshold);
    BKE_id_free(bmain, new_ob_mesh);
    new_ob_mesh = BKE_mesh_from_bmesh_nomain(bm, &bm_to_mesh_params, mesh);
    BM_mesh_free(bm);

    /* Remove the mask from the new object so it can be sculpted directly after slicing. */
    new_ob_mesh->attributes_for_write().remove(".sculpt_mask");

    Mesh *new_mesh = static_cast<Mesh *>(new_ob->data);
    BKE_mesh_nomain_to_mesh(new_ob_mesh, new_mesh, new_ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, new_ob);
    BKE_mesh_batch_cache_dirty_tag(new_mesh, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&new_ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, new_mesh);
  }

  mesh = static_cast<Mesh *>(ob->data);
  BKE_mesh_nomain_to_mesh(new_mesh, mesh, ob);

  if (ob->mode == OB_MODE_SCULPT) {
    if (mesh->attributes().contains(".sculpt_face_set")) {
      /* Assign a new Face Set ID to the new faces created by the slice operation. */
      const int next_face_set_id = sculpt_paint::face_set::find_next_available_id(*ob);
      sculpt_paint::face_set::initialize_none_to_id(mesh, next_face_set_id);
    }
    if (!create_new_object) {
      sculpt_paint::undo::geometry_end(ob);
    }
  }

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

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
