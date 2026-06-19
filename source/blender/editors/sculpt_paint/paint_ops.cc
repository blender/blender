/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.hh"
#include "BLI_listbase.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_utildefines.hh"

#include "IMB_interp.hh"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"

#include "ED_image.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "IMB_colormanagement.hh"

#include "paint_intern.hh"

#include "curves/sculpt_intern.hh"
#include "mesh/paint_hide.hh"
#include "mesh/paint_mask.hh"
#include "mesh/sculpt_intern.hh"

namespace blender {

static wmOperatorStatus brush_scale_size_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  float scalar = RNA_float_get(op->ptr, "scalar");

  /* Grease Pencil brushes in Paint mode do not use unified size. */
  const bool use_unified_size = !(brush && brush->gpencil_settings &&
                                  brush->ob_mode == OB_MODE_PAINT_GREASE_PENCIL);

  if (brush) {
    /* Pixel diameter. */
    {
      const int old_size = (use_unified_size) ? BKE_brush_size_get(paint, brush) : brush->size;
      int size = int(scalar * old_size);

      if (abs(old_size - size) < U.pixelsize) {
        if (scalar > 1) {
          size += U.pixelsize;
        }
        else if (scalar < 1) {
          size -= U.pixelsize;
        }
      }

      if (use_unified_size) {
        BKE_brush_size_set(paint, brush, size);
      }
      else {
        brush->size = max_ii(size, 1);
        BKE_brush_tag_unsaved_changes(brush);
      }
    }

    /* Unprojected diameter. */
    {
      float unprojected_size = scalar * (use_unified_size ?
                                             BKE_brush_unprojected_size_get(paint, brush) :
                                             brush->unprojected_size);

      unprojected_size = std::max(unprojected_size, 0.001f);

      if (use_unified_size) {
        BKE_brush_unprojected_size_set(paint, brush, unprojected_size);
      }
      else {
        brush->unprojected_size = unprojected_size;
        BKE_brush_tag_unsaved_changes(brush);
      }
    }

    WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
  }

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_scale_size(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scale Sculpt/Paint Brush Size";
  ot->description = "Change brush size by a scalar";
  ot->idname = "BRUSH_OT_scale_size";

  /* API callbacks. */
  ot->exec = brush_scale_size_exec;

  /* flags */
  ot->flag = 0;

  RNA_def_float(ot->srna, "scalar", 1, 0, 2, "Scalar", "Factor to scale brush size by", 0, 2);
}

/***** Stencil Control *****/

enum StencilControlMode {
  STENCIL_TRANSLATE,
  STENCIL_SCALE,
  STENCIL_ROTATE,
};

enum StencilTextureMode {
  STENCIL_PRIMARY = 0,
  STENCIL_SECONDARY = 1,
};

enum StencilConstraint {
  STENCIL_CONSTRAINT_X = 1,
  STENCIL_CONSTRAINT_Y = 2,
};

struct StencilControlData {
  float init_mouse[2];
  float init_spos[2];
  float init_sdim[2];
  float init_rot;
  float init_angle;
  float lenorig;
  float area_size[2];
  StencilControlMode mode;
  StencilConstraint constrain_mode;
  /** We are tweaking mask or color stencil. */
  int mask;
  Brush *br;
  float *dim_target;
  float *rot_target;
  float *pos_target;
  short launch_event;
};

static void stencil_set_target(StencilControlData *scd)
{
  Brush *br = scd->br;
  float mdiff[2];
  if (scd->mask) {
    copy_v2_v2(scd->init_sdim, br->mask_stencil_dimension);
    copy_v2_v2(scd->init_spos, br->mask_stencil_pos);
    scd->init_rot = br->mask_mtex.rot;

    scd->dim_target = br->mask_stencil_dimension;
    scd->rot_target = &br->mask_mtex.rot;
    scd->pos_target = br->mask_stencil_pos;

    sub_v2_v2v2(mdiff, scd->init_mouse, br->mask_stencil_pos);
  }
  else {
    copy_v2_v2(scd->init_sdim, br->stencil_dimension);
    copy_v2_v2(scd->init_spos, br->stencil_pos);
    scd->init_rot = br->mtex.rot;

    scd->dim_target = br->stencil_dimension;
    scd->rot_target = &br->mtex.rot;
    scd->pos_target = br->stencil_pos;

    sub_v2_v2v2(mdiff, scd->init_mouse, br->stencil_pos);
  }

  scd->lenorig = len_v2(mdiff);

  scd->init_angle = atan2f(mdiff[1], mdiff[0]);
}

static wmOperatorStatus stencil_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  const float mvalf[2] = {float(event->mval[0]), float(event->mval[1])};
  ARegion *region = CTX_wm_region(C);
  StencilControlData *scd;
  int mask = RNA_enum_get(op->ptr, "texmode");

  if (mask) {
    if (br->mask_mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    if (br->mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL) {
      return OPERATOR_CANCELLED;
    }
  }

  scd = MEM_new_uninitialized<StencilControlData>(__func__);
  scd->mask = mask;
  scd->br = br;

  copy_v2_v2(scd->init_mouse, mvalf);

  stencil_set_target(scd);

  scd->mode = StencilControlMode(RNA_enum_get(op->ptr, "mode"));
  scd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);
  scd->area_size[0] = region->winx;
  scd->area_size[1] = region->winy;

  op->customdata = scd;
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void stencil_restore(StencilControlData *scd)
{
  copy_v2_v2(scd->dim_target, scd->init_sdim);
  copy_v2_v2(scd->pos_target, scd->init_spos);
  *scd->rot_target = scd->init_rot;
}

static void stencil_control_cancel(bContext * /*C*/, wmOperator *op)
{
  StencilControlData *scd = static_cast<StencilControlData *>(op->customdata);
  stencil_restore(scd);
  MEM_delete(scd);
}

static void stencil_control_calculate(StencilControlData *scd, const int mval[2])
{
#define PIXEL_MARGIN 5

  float mdiff[2];
  const float mvalf[2] = {float(mval[0]), float(mval[1])};
  switch (scd->mode) {
    case STENCIL_TRANSLATE:
      sub_v2_v2v2(mdiff, mvalf, scd->init_mouse);
      add_v2_v2v2(scd->pos_target, scd->init_spos, mdiff);
      CLAMP(scd->pos_target[0],
            -scd->dim_target[0] + PIXEL_MARGIN,
            scd->area_size[0] + scd->dim_target[0] - PIXEL_MARGIN);

      CLAMP(scd->pos_target[1],
            -scd->dim_target[1] + PIXEL_MARGIN,
            scd->area_size[1] + scd->dim_target[1] - PIXEL_MARGIN);
      BKE_brush_tag_unsaved_changes(scd->br);

      break;
    case STENCIL_SCALE: {
      float len, factor;
      sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
      len = len_v2(mdiff);
      factor = len / scd->lenorig;
      copy_v2_v2(mdiff, scd->init_sdim);
      if (scd->constrain_mode != STENCIL_CONSTRAINT_Y) {
        mdiff[0] = factor * scd->init_sdim[0];
      }
      if (scd->constrain_mode != STENCIL_CONSTRAINT_X) {
        mdiff[1] = factor * scd->init_sdim[1];
      }
      clamp_v2(mdiff, 5.0f, 10000.0f);
      copy_v2_v2(scd->dim_target, mdiff);
      BKE_brush_tag_unsaved_changes(scd->br);
      break;
    }
    case STENCIL_ROTATE: {
      float angle;
      sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
      angle = atan2f(mdiff[1], mdiff[0]);
      angle = scd->init_rot + angle - scd->init_angle;
      if (angle < 0.0f) {
        angle += float(2 * M_PI);
      }
      if (angle > float(2 * M_PI)) {
        angle -= float(2 * M_PI);
      }
      *scd->rot_target = angle;
      BKE_brush_tag_unsaved_changes(scd->br);
      break;
    }
  }
#undef PIXEL_MARGIN
}

static wmOperatorStatus stencil_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  StencilControlData *scd = static_cast<StencilControlData *>(op->customdata);

  if (event->type == scd->launch_event && event->val == KM_RELEASE) {
    MEM_delete(scd);
    WM_event_add_notifier(C, NC_WINDOW, nullptr);
    return OPERATOR_FINISHED;
  }

  switch (event->type) {
    case MOUSEMOVE:
      stencil_control_calculate(scd, event->mval);
      break;
    case EVT_ESCKEY:
      if (event->val == KM_PRESS) {
        stencil_control_cancel(C, op);
        WM_event_add_notifier(C, NC_WINDOW, nullptr);
        return OPERATOR_CANCELLED;
      }
      break;
    case EVT_XKEY:
      if (event->val == KM_PRESS) {

        if (scd->constrain_mode == STENCIL_CONSTRAINT_X) {
          scd->constrain_mode = StencilConstraint(0);
        }
        else {
          scd->constrain_mode = STENCIL_CONSTRAINT_X;
        }

        stencil_control_calculate(scd, event->mval);
      }
      break;
    case EVT_YKEY:
      if (event->val == KM_PRESS) {
        if (scd->constrain_mode == STENCIL_CONSTRAINT_Y) {
          scd->constrain_mode = StencilConstraint(0);
        }
        else {
          scd->constrain_mode = STENCIL_CONSTRAINT_Y;
        }

        stencil_control_calculate(scd, event->mval);
      }
      break;
    default:
      break;
  }

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_RUNNING_MODAL;
}

static bool stencil_control_poll(bContext *C)
{
  PaintMode mode = BKE_paintmode_get_active_from_context(C);

  Paint *paint;
  Brush *br;

  if (!ed::sculpt_paint::paint_supports_texture(mode)) {
    return false;
  }

  paint = BKE_paint_get_active_from_context(C);
  br = BKE_paint_brush(paint);
  return (br && (br->mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL ||
                 br->mask_mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL));
}

static void BRUSH_OT_stencil_control(wmOperatorType *ot)
{
  static const EnumPropertyItem stencil_control_items[] = {
      {STENCIL_TRANSLATE, "TRANSLATION", 0, "Translation", ""},
      {STENCIL_SCALE, "SCALE", 0, "Scale", ""},
      {STENCIL_ROTATE, "ROTATION", 0, "Rotation", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem stencil_texture_items[] = {
      {STENCIL_PRIMARY, "PRIMARY", 0, "Primary", ""},
      {STENCIL_SECONDARY, "SECONDARY", 0, "Secondary", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  /* identifiers */
  ot->name = "Stencil Brush Control";
  ot->description = "Control the stencil brush";
  ot->idname = "BRUSH_OT_stencil_control";

  /* API callbacks. */
  ot->invoke = stencil_control_invoke;
  ot->modal = stencil_control_modal;
  ot->cancel = stencil_control_cancel;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = 0;

  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "mode", stencil_control_items, STENCIL_TRANSLATE, "Tool", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna, "texmode", stencil_texture_items, STENCIL_PRIMARY, "Tool", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static wmOperatorStatus stencil_fit_image_aspect_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  bool use_scale = RNA_boolean_get(op->ptr, "use_scale");
  bool use_repeat = RNA_boolean_get(op->ptr, "use_repeat");
  bool do_mask = RNA_boolean_get(op->ptr, "mask");
  Tex *tex = nullptr;
  MTex *mtex = nullptr;
  if (br) {
    mtex = do_mask ? &br->mask_mtex : &br->mtex;
    tex = mtex->tex;
  }

  if (tex && tex->type == TEX_IMAGE && tex->ima) {
    float aspx, aspy;
    Image *ima = tex->ima;
    float orig_area, stencil_area, factor;
    ED_image_get_uv_aspect(ima, nullptr, &aspx, &aspy);

    if (use_scale) {
      aspx *= mtex->size[0];
      aspy *= mtex->size[1];
    }

    if (use_repeat && tex->extend == TEX_REPEAT) {
      aspx *= tex->xrepeat;
      aspy *= tex->yrepeat;
    }

    orig_area = fabsf(aspx * aspy);

    if (do_mask) {
      stencil_area = fabsf(br->mask_stencil_dimension[0] * br->mask_stencil_dimension[1]);
    }
    else {
      stencil_area = fabsf(br->stencil_dimension[0] * br->stencil_dimension[1]);
    }

    factor = sqrtf(stencil_area / orig_area);

    if (do_mask) {
      br->mask_stencil_dimension[0] = fabsf(factor * aspx);
      br->mask_stencil_dimension[1] = fabsf(factor * aspy);
    }
    else {
      br->stencil_dimension[0] = fabsf(factor * aspx);
      br->stencil_dimension[1] = fabsf(factor * aspy);
    }
    BKE_brush_tag_unsaved_changes(br);
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_stencil_fit_image_aspect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Image Aspect";
  ot->description =
      "When using an image texture, adjust the stencil size to fit the image aspect ratio";
  ot->idname = "BRUSH_OT_stencil_fit_image_aspect";

  /* API callbacks. */
  ot->exec = stencil_fit_image_aspect_exec;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "use_repeat", true, "Use Repeat", "Use repeat mapping values");
  RNA_def_boolean(ot->srna, "use_scale", true, "Use Scale", "Use texture scale values");
  RNA_def_boolean(
      ot->srna, "mask", false, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

static wmOperatorStatus stencil_reset_transform_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  bool do_mask = RNA_boolean_get(op->ptr, "mask");

  if (!br) {
    return OPERATOR_CANCELLED;
  }

  if (do_mask) {
    br->mask_stencil_pos[0] = 256;
    br->mask_stencil_pos[1] = 256;

    br->mask_stencil_dimension[0] = 256;
    br->mask_stencil_dimension[1] = 256;

    br->mask_mtex.rot = 0;
  }
  else {
    br->stencil_pos[0] = 256;
    br->stencil_pos[1] = 256;

    br->stencil_dimension[0] = 256;
    br->stencil_dimension[1] = 256;

    br->mtex.rot = 0;
  }

  BKE_brush_tag_unsaved_changes(br);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_stencil_reset_transform(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Transform";
  ot->description = "Reset the stencil transformation to the default";
  ot->idname = "BRUSH_OT_stencil_reset_transform";

  /* API callbacks. */
  ot->exec = stencil_reset_transform_exec;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "mask", false, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

/**************************** registration **********************************/

void ED_operatormacros_paint()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("PAINTCURVE_OT_add_point_slide",
                                    "Add Curve Point and Slide",
                                    "Add new curve point and slide it",
                                    OPTYPE_UNDO);
  ot->description = "Add new curve point and slide it";
  WM_operatortype_macro_define(ot, "PAINTCURVE_OT_add_point");
  otmacro = WM_operatortype_macro_define(ot, "PAINTCURVE_OT_slide");
  RNA_boolean_set(otmacro->ptr, "align", true);
  RNA_boolean_set(otmacro->ptr, "select", false);
}

void ED_operatortypes_paint()
{
  /* palette */
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(PALETTE_OT_new);
  WM_operatortype_append(PALETTE_OT_color_add);
  WM_operatortype_append(PALETTE_OT_color_delete);

  WM_operatortype_append(PALETTE_OT_extract_from_image);
  WM_operatortype_append(PALETTE_OT_sort);
  WM_operatortype_append(PALETTE_OT_color_move);
  WM_operatortype_append(PALETTE_OT_join);

  /* paint curve */
  WM_operatortype_append(PAINTCURVE_OT_new);
  WM_operatortype_append(PAINTCURVE_OT_add_point);
  WM_operatortype_append(PAINTCURVE_OT_delete_point);
  WM_operatortype_append(PAINTCURVE_OT_select);
  WM_operatortype_append(PAINTCURVE_OT_slide);
  WM_operatortype_append(PAINTCURVE_OT_draw);
  WM_operatortype_append(PAINTCURVE_OT_cursor);

  /* brush */
  WM_operatortype_append(BRUSH_OT_scale_size);
  WM_operatortype_append(BRUSH_OT_stencil_control);
  WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
  WM_operatortype_append(BRUSH_OT_stencil_reset_transform);
  WM_operatortype_append(BRUSH_OT_asset_activate);
  WM_operatortype_append(BRUSH_OT_asset_save_as);
  WM_operatortype_append(BRUSH_OT_asset_edit_metadata);
  WM_operatortype_append(BRUSH_OT_asset_load_preview);
  WM_operatortype_append(BRUSH_OT_asset_delete);
  WM_operatortype_append(BRUSH_OT_asset_save);
  WM_operatortype_append(BRUSH_OT_asset_revert);

  /* image */
  WM_operatortype_append(PAINT_OT_texture_paint_toggle);
  WM_operatortype_append(PAINT_OT_image_paint);
  WM_operatortype_append(PAINT_OT_sample_color);
  WM_operatortype_append(PAINT_OT_grab_clone);
  WM_operatortype_append(PAINT_OT_project_image);
  WM_operatortype_append(PAINT_OT_image_from_view);
  WM_operatortype_append(PAINT_OT_brush_colors_flip);
  WM_operatortype_append(PAINT_OT_add_texture_paint_slot);
  WM_operatortype_append(PAINT_OT_add_simple_uvs);

  /* weight */
  WM_operatortype_append(PAINT_OT_weight_paint_toggle);
  WM_operatortype_append(PAINT_OT_weight_paint);
  WM_operatortype_append(PAINT_OT_weight_set);
  WM_operatortype_append(PAINT_OT_weight_from_bones);
  WM_operatortype_append(PAINT_OT_weight_gradient);
  WM_operatortype_append(PAINT_OT_weight_sample);
  WM_operatortype_append(PAINT_OT_weight_sample_group);

  /* uv */
  WM_operatortype_append(SCULPT_OT_uv_sculpt_grab);
  WM_operatortype_append(SCULPT_OT_uv_sculpt_relax);
  WM_operatortype_append(SCULPT_OT_uv_sculpt_pinch);

  /* vertex selection */
  WM_operatortype_append(PAINT_OT_vert_select_all);
  WM_operatortype_append(PAINT_OT_vert_select_ungrouped);
  WM_operatortype_append(PAINT_OT_vert_select_hide);
  WM_operatortype_append(PAINT_OT_vert_select_linked);
  WM_operatortype_append(PAINT_OT_vert_select_linked_pick);
  WM_operatortype_append(PAINT_OT_vert_select_more);
  WM_operatortype_append(PAINT_OT_vert_select_less);
  WM_operatortype_append(PAINT_OT_vert_select_loop);

  /* vertex */
  WM_operatortype_append(PAINT_OT_vertex_paint_toggle);
  WM_operatortype_append(PAINT_OT_vertex_paint);
  WM_operatortype_append(PAINT_OT_vertex_color_set);
  WM_operatortype_append(PAINT_OT_vertex_color_smooth);

  WM_operatortype_append(PAINT_OT_vertex_color_brightness_contrast);
  WM_operatortype_append(PAINT_OT_vertex_color_hsv);
  WM_operatortype_append(PAINT_OT_vertex_color_invert);
  WM_operatortype_append(PAINT_OT_vertex_color_levels);
  WM_operatortype_append(PAINT_OT_vertex_color_from_weight);

  /* face-select */
  WM_operatortype_append(PAINT_OT_face_select_linked);
  WM_operatortype_append(PAINT_OT_face_select_linked_pick);
  WM_operatortype_append(PAINT_OT_face_select_all);
  WM_operatortype_append(PAINT_OT_face_select_more);
  WM_operatortype_append(PAINT_OT_face_select_less);
  WM_operatortype_append(PAINT_OT_face_select_hide);
  WM_operatortype_append(PAINT_OT_face_select_loop);

  WM_operatortype_append(PAINT_OT_face_vert_reveal);

  /* partial visibility */
  WM_operatortype_append(hide::PAINT_OT_hide_show_all);
  WM_operatortype_append(hide::PAINT_OT_hide_show_masked);
  WM_operatortype_append(hide::PAINT_OT_hide_show);
  WM_operatortype_append(hide::PAINT_OT_hide_show_lasso_gesture);
  WM_operatortype_append(hide::PAINT_OT_hide_show_line_gesture);
  WM_operatortype_append(hide::PAINT_OT_hide_show_polyline_gesture);
  WM_operatortype_append(hide::PAINT_OT_visibility_invert);
  WM_operatortype_append(hide::PAINT_OT_visibility_filter);

  /* paint masking */
  WM_operatortype_append(mask::PAINT_OT_mask_flood_fill);
  WM_operatortype_append(mask::PAINT_OT_mask_lasso_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_box_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_line_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_polyline_gesture);
}

void ED_keymap_paint(wmKeyConfig *keyconf)
{
  using namespace blender::ed::sculpt_paint;
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(keyconf, "Paint Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = paint_curve_poll;

  /* Sculpt mode */
  keymap = WM_keymap_ensure(keyconf, "Sculpt", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = sculpt_mode_poll;

  /* Vertex Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Vertex Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = vertex_paint_mode_poll;

  /* Weight Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Weight Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = weight_paint_mode_poll;

  /* Weight paint's Vertex Selection Mode. */
  keymap = WM_keymap_ensure(
      keyconf, "Paint Vertex Selection (Weight, Vertex)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = vert_paint_poll;

  /* Image/Texture Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Image Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = image_texture_paint_poll;

  /* face-mask mode */
  keymap = WM_keymap_ensure(
      keyconf, "Paint Face Mask (Weight, Vertex, Texture)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = facemask_paint_poll;

  /* paint stroke */
  keymap = paint_stroke_modal_keymap(keyconf);
  WM_modalkeymap_assign(keymap, "SCULPT_OT_brush_stroke");
  WM_modalkeymap_assign(keymap, "PAINT_OT_vertex_paint");
  WM_modalkeymap_assign(keymap, "PAINT_OT_weight_paint");
  WM_modalkeymap_assign(keymap, "PAINT_OT_image_paint");

  /* Curves Sculpt mode. */
  keymap = WM_keymap_ensure(keyconf, "Sculpt Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = curves_sculpt_poll;

  /* sculpt expand. */
  expand::modal_keymap(keyconf);
}

}  // namespace blender
