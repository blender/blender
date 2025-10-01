/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_rect.h"

#include "BLT_translation.hh"

#include "BKE_brush.hh"
#include "BKE_bvhutils.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "RE_texture.h"

#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "ED_mesh.hh" /* for face mask functions */

#include "WM_api.hh"
#include "WM_types.hh"

#include "paint_intern.hh"

bool paint_convert_bb_to_rect(rcti *rect,
                              const float bb_min[3],
                              const float bb_max[3],
                              const ARegion &region,
                              const RegionView3D &rv3d,
                              const Object &ob)
{
  int i, j, k;

  BLI_rcti_init_minmax(rect);

  /* return zero if the bounding box has non-positive volume */
  if (bb_min[0] > bb_max[0] || bb_min[1] > bb_max[1] || bb_min[2] > bb_max[2]) {
    return false;
  }

  const blender::float4x4 projection = ED_view3d_ob_project_mat_get(&rv3d, &ob);

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 2; j++) {
      for (k = 0; k < 2; k++) {
        float vec[3];
        int proj_i[2];
        vec[0] = i ? bb_min[0] : bb_max[0];
        vec[1] = j ? bb_min[1] : bb_max[1];
        vec[2] = k ? bb_min[2] : bb_max[2];
        /* convert corner to screen space */
        const blender::float2 proj = ED_view3d_project_float_v2_m4(&region, vec, projection);
        /* expand 2D rectangle */

        /* we could project directly to int? */
        proj_i[0] = proj[0];
        proj_i[1] = proj[1];

        BLI_rcti_do_minmax_v(rect, proj_i);
      }
    }
  }

  /* return false if the rectangle has non-positive area */
  return rect->xmin < rect->xmax && rect->ymin < rect->ymax;
}

float paint_calc_object_space_radius(const ViewContext &vc,
                                     const blender::float3 &center,
                                     const float pixel_radius)
{
  Object *ob = vc.obact;
  float delta[3], scale, loc[3];
  const float xy_delta[2] = {pixel_radius, 0.0f};

  mul_v3_m4v3(loc, ob->object_to_world().ptr(), center);

  const float zfac = ED_view3d_calc_zfac(vc.rv3d, loc);
  ED_view3d_win_to_delta(vc.region, xy_delta, zfac, delta);

  scale = fabsf(mat4_to_scale(ob->object_to_world().ptr()));
  scale = (scale == 0.0f) ? 1.0f : scale;

  return len_v3(delta) / scale;
}

bool paint_get_tex_pixel(const MTex *mtex,
                         float u,
                         float v,
                         ImagePool *pool,
                         int thread,
                         /* Return arguments. */
                         float *r_intensity,
                         float r_rgba[4])
{
  const float co[3] = {u, v, 0.0f};
  float intensity;
  const bool has_rgb = RE_texture_evaluate(
      mtex, co, thread, pool, false, false, &intensity, r_rgba);
  *r_intensity = intensity;

  if (!has_rgb) {
    r_rgba[0] = intensity;
    r_rgba[1] = intensity;
    r_rgba[2] = intensity;
    r_rgba[3] = 1.0f;
  }

  return has_rgb;
}

void paint_stroke_operator_properties(wmOperatorType *ot)
{
  static const EnumPropertyItem stroke_mode_items[] = {
      {BRUSH_STROKE_NORMAL, "NORMAL", 0, "Regular", "Apply brush normally"},
      {BRUSH_STROKE_INVERT,
       "INVERT",
       0,
       "Invert",
       "Invert action of brush for duration of stroke"},
      {BRUSH_STROKE_SMOOTH,
       "SMOOTH",
       0,
       "Smooth",
       "Switch brush to smooth mode for duration of stroke"},
      {BRUSH_STROKE_ERASE,
       "ERASE",
       0,
       "Erase",
       "Switch brush to erase mode for duration of stroke"},
      {0},
  };

  PropertyRNA *prop;

  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "mode",
                      stroke_mode_items,
                      BRUSH_STROKE_NORMAL,
                      "Stroke Mode",
                      "Action taken when a paint stroke is made");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* TODO: Pen flip logic should likely be combined into the stroke mode logic instead of being
   * an entirely separate concept. */
  prop = RNA_def_boolean(
      ot->srna, "pen_flip", false, "Pen Flip", "Whether a tablet's eraser mode is being used");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* face-select ops */
static wmOperatorStatus paint_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  paintface_select_linked(C, CTX_data_active_object(C), nullptr, true);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->description = "Select linked faces";
  ot->idname = "PAINT_OT_face_select_linked";

  ot->exec = paint_select_linked_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus paint_select_linked_pick_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  view3d_operator_needs_gpu(C);
  paintface_select_linked(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked_pick(wmOperatorType *ot)
{
  ot->name = "Select Linked Pick";
  ot->description = "Select linked faces under the cursor";
  ot->idname = "PAINT_OT_face_select_linked_pick";

  ot->invoke = paint_select_linked_pick_invoke;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Deselect rather than select items");
}

static wmOperatorStatus face_select_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (paintface_deselect_all_visible(C, ob, RNA_enum_get(op->ptr, "action"), true)) {
    ED_region_tag_redraw(CTX_wm_region(C));
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_face_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->description = "Change selection for all faces";
  ot->idname = "PAINT_OT_face_select_all";

  ot->exec = face_select_all_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static wmOperatorStatus paint_select_more_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintface_select_more(mesh, face_step);
  paintface_flush_flags(C, ob, true, false);

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->description = "Select Faces connected to existing selection";
  ot->idname = "PAINT_OT_face_select_more";

  ot->exec = paint_select_more_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also select faces that only touch on a corner");
}

static wmOperatorStatus paint_select_less_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintface_select_less(mesh, face_step);
  paintface_flush_flags(C, ob, true, false);

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->description = "Deselect Faces connected to existing selection";
  ot->idname = "PAINT_OT_face_select_less";

  ot->exec = paint_select_less_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also deselect faces that only touch on a corner");
}

static wmOperatorStatus paintface_select_loop_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  if (!extend) {
    paintface_deselect_all_visible(C, CTX_data_active_object(C), SEL_DESELECT, false);
  }
  view3d_operator_needs_gpu(C);
  paintface_select_loop(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_loop(wmOperatorType *ot)
{
  ot->name = "Select Loop";
  ot->description = "Select face loop under the cursor";
  ot->idname = "PAINT_OT_face_select_loop";

  ot->invoke = paintface_select_loop_invoke;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "If false, faces will be deselected");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

static wmOperatorStatus vert_select_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  paintvert_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), true);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->description = "Change selection for all vertices";
  ot->idname = "PAINT_OT_vert_select_all";

  ot->exec = vert_select_all_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static wmOperatorStatus vert_select_ungrouped_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (BLI_listbase_is_empty(&mesh->vertex_group_names) || mesh->deform_verts().is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No weights/vertex groups on object");
    return OPERATOR_CANCELLED;
  }

  paintvert_select_ungrouped(ob, RNA_boolean_get(op->ptr, "extend"), true);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_ungrouped(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Ungrouped";
  ot->idname = "PAINT_OT_vert_select_ungrouped";
  ot->description = "Select vertices without a group";

  /* API callbacks. */
  ot->exec = vert_select_ungrouped_exec;
  ot->poll = vert_paint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

static wmOperatorStatus paintvert_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  paintvert_select_linked(C, CTX_data_active_object(C));
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked Vertices";
  ot->description = "Select linked vertices";
  ot->idname = "PAINT_OT_vert_select_linked";

  ot->exec = paintvert_select_linked_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus paintvert_select_linked_pick_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent *event)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  view3d_operator_needs_gpu(C);

  paintvert_select_linked_pick(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_linked_pick(wmOperatorType *ot)
{
  ot->name = "Select Linked Vertices Pick";
  ot->description = "Select linked vertices under the cursor";
  ot->idname = "PAINT_OT_vert_select_linked_pick";

  ot->invoke = paintvert_select_linked_pick_invoke;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "select",
                  true,
                  "Select",
                  "Whether to select or deselect linked vertices under the cursor");
}

static wmOperatorStatus paintvert_select_more_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintvert_select_more(mesh, face_step);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->description = "Select Vertices connected to existing selection";
  ot->idname = "PAINT_OT_vert_select_more";

  ot->exec = paintvert_select_more_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also select faces that only touch on a corner");
}

static wmOperatorStatus paintvert_select_less_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintvert_select_less(mesh, face_step);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->description = "Deselect Vertices connected to existing selection";
  ot->idname = "PAINT_OT_vert_select_less";

  ot->exec = paintvert_select_less_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also deselect faces that only touch on a corner");
}

static wmOperatorStatus face_select_hide_exec(bContext *C, wmOperator *op)
{
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  Object *ob = CTX_data_active_object(C);
  paintface_hide(C, ob, unselected);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_hide(wmOperatorType *ot)
{
  ot->name = "Face Select Hide";
  ot->description = "Hide selected faces";
  ot->idname = "PAINT_OT_face_select_hide";

  ot->exec = face_select_hide_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected objects");
}

static wmOperatorStatus vert_select_hide_exec(bContext *C, wmOperator *op)
{
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  Object *ob = CTX_data_active_object(C);
  paintvert_hide(C, ob, unselected);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_hide(wmOperatorType *ot)
{
  ot->name = "Vertex Select Hide";
  ot->description = "Hide selected vertices";
  ot->idname = "PAINT_OT_vert_select_hide";

  ot->exec = vert_select_hide_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "unselected",
                  false,
                  "Unselected",
                  "Hide unselected rather than selected vertices");
}

static wmOperatorStatus face_vert_reveal_exec(bContext *C, wmOperator *op)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  Object *ob = CTX_data_active_object(C);

  if (BKE_paint_select_vert_test(ob)) {
    paintvert_reveal(C, ob, select);
  }
  else {
    paintface_reveal(C, ob, select);
  }

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

static bool face_vert_reveal_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  /* Allow using this operator when no selection is enabled but hiding is applied. */
  return BKE_paint_select_elem_test(ob) || BKE_paint_always_hide_test(ob);
}

void PAINT_OT_face_vert_reveal(wmOperatorType *ot)
{
  ot->name = "Reveal Faces/Vertices";
  ot->description = "Reveal hidden faces and vertices";
  ot->idname = "PAINT_OT_face_vert_reveal";

  ot->exec = face_vert_reveal_exec;
  ot->poll = face_vert_reveal_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "select",
                  true,
                  "Select",
                  "Specifies whether the newly revealed geometry should be selected");
}
