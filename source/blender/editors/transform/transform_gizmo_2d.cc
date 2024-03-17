/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * \name 2D Transform Gizmo
 *
 * Used for UV/Image Editor
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"

#include "RNA_access.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "ED_gizmo_library.hh"
#include "ED_gizmo_utils.hh"
#include "ED_image.hh"
#include "ED_screen.hh"
#include "ED_uvedit.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "transform.hh"
#include "transform_gizmo.hh"

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Shared Callback's
 * \{ */

static bool gizmo2d_generic_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }

  if ((U.gizmo_flag & USER_GIZMO_DRAW) == 0) {
    return false;
  }

  if (G.moving) {
    return false;
  }

  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr) {
    return false;
  }

  /* NOTE: below this is assumed to be a tool gizmo.
   * If there are cases that need to check other flags - this function could be split. */
  switch (area->spacetype) {
    case SPACE_IMAGE: {
      const SpaceImage *sima = static_cast<const SpaceImage *>(area->spacedata.first);
      Object *obedit = CTX_data_edit_object(C);
      if (!ED_space_image_show_uvedit(sima, obedit)) {
        return false;
      }
      break;
    }
    case SPACE_SEQ: {
      const SpaceSeq *sseq = static_cast<const SpaceSeq *>(area->spacedata.first);
      if (sseq->gizmo_flag & (SEQ_GIZMO_HIDE | SEQ_GIZMO_HIDE_TOOL)) {
        return false;
      }
      if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
        return false;
      }
      Scene *scene = CTX_data_scene(C);
      Editing *ed = SEQ_editing_get(scene);
      if (ed == nullptr) {
        return false;
      }
      break;
    }
  }

  return true;
}

static void gizmo2d_pivot_point_message_subscribe(wmGizmoGroup *gzgroup,
                                                  wmMsgBus *mbus, /* Additional args. */
                                                  bScreen *screen,
                                                  ScrArea *area,
                                                  ARegion *region)
{
  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh{};
  msg_sub_value_gz_tag_refresh.owner = region;
  msg_sub_value_gz_tag_refresh.user_data = gzgroup->parent_gzmap;
  msg_sub_value_gz_tag_refresh.notify = WM_gizmo_do_msg_notify_tag_refresh;

  switch (area->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
      PointerRNA ptr = RNA_pointer_create(&screen->id, &RNA_SpaceImageEditor, sima);
      {
        const PropertyRNA *props[] = {
            &rna_SpaceImageEditor_pivot_point,
            (sima->around == V3D_AROUND_CURSOR) ? &rna_SpaceImageEditor_cursor_location : nullptr,
        };
        for (int i = 0; i < ARRAY_SIZE(props); i++) {
          if (props[i] == nullptr) {
            continue;
          }
          WM_msg_subscribe_rna(mbus, &ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
        }
      }
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arrow / Cage Gizmo Group
 *
 * Defines public functions, not the gizmo itself:
 *
 * - #ED_widgetgroup_gizmo2d_xform_callbacks_set
 * - #ED_widgetgroup_gizmo2d_xform_no_cage_callbacks_set
 *
 * \{ */

/* Axes as index. */
enum {
  MAN2D_AXIS_TRANS_X = 0,
  MAN2D_AXIS_TRANS_Y,

  MAN2D_AXIS_LAST,
};

struct GizmoGroup2D {
  wmGizmo *translate_xy[3];
  wmGizmo *cage;

  /* Current origin in view space, used to update widget origin for possible view changes. */
  float origin[2];
  float min[2];
  float max[2];
  float rotation;

  bool no_cage;
};

/* **************** Utilities **************** */

static void gizmo2d_get_axis_color(const int axis_idx, float *r_col, float *r_col_hi)
{
  const float alpha = 0.6f;
  const float alpha_hi = 1.0f;
  int col_id;

  switch (axis_idx) {
    case MAN2D_AXIS_TRANS_X:
      col_id = TH_AXIS_X;
      break;
    case MAN2D_AXIS_TRANS_Y:
      col_id = TH_AXIS_Y;
      break;
    default:
      BLI_assert(0);
      col_id = TH_AXIS_Y;
      break;
  }

  UI_GetThemeColor4fv(col_id, r_col);

  copy_v4_v4(r_col_hi, r_col);
  r_col[3] *= alpha;
  r_col_hi[3] *= alpha_hi;
}

static GizmoGroup2D *gizmogroup2d_init(wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_cage = WM_gizmotype_find("GIZMO_GT_cage_2d", true);
  const wmGizmoType *gzt_button = WM_gizmotype_find("GIZMO_GT_button_2d", true);

  GizmoGroup2D *ggd = static_cast<GizmoGroup2D *>(MEM_callocN(sizeof(GizmoGroup2D), __func__));

  ggd->translate_xy[0] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->translate_xy[1] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->translate_xy[2] = WM_gizmo_new_ptr(gzt_button, gzgroup, nullptr);
  ggd->cage = WM_gizmo_new_ptr(gzt_cage, gzgroup, nullptr);

  RNA_enum_set(ggd->cage->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE |
                   ED_GIZMO_CAGE_XFORM_FLAG_ROTATE);

  return ggd;
}

/**
 * Calculates origin in view space, use with #gizmo2d_origin_to_region.
 */
static bool gizmo2d_calc_bounds(const bContext *C, float *r_center, float *r_min, float *r_max)
{
  float min_buf[2], max_buf[2];
  if (r_min == nullptr) {
    r_min = min_buf;
  }
  if (r_max == nullptr) {
    r_max = max_buf;
  }

  ScrArea *area = CTX_wm_area(C);
  bool has_select = false;
  if (area->spacetype == SPACE_IMAGE) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
        scene, view_layer, nullptr);
    if (ED_uvedit_minmax_multi(scene, objects, r_min, r_max)) {
      has_select = true;
    }
  }
  else if (area->spacetype == SPACE_SEQ) {
    Scene *scene = CTX_data_scene(C);
    Editing *ed = SEQ_editing_get(scene);
    ListBase *seqbase = SEQ_active_seqbase_get(ed);
    ListBase *channels = SEQ_channels_displayed_get(ed);
    blender::VectorSet strips = SEQ_query_rendered_strips(
        scene, channels, seqbase, scene->r.cfra, 0);
    strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });
    int selected_strips = strips.size();
    if (selected_strips > 0) {
      has_select = true;
      SEQ_image_transform_bounding_box_from_collection(
          scene, strips, selected_strips != 1, r_min, r_max);
    }
    if (selected_strips > 1) {
      /* Don't draw the cage as transforming multiple strips isn't currently very useful as it
       * doesn't behave as one would expect.
       *
       * This is because our current transform system doesn't support shearing which would make the
       * scaling transforms of the bounding box behave weirdly.
       * In addition to this, the rotation of the bounding box can not currently be hooked up
       * properly to read the result from the transform system (when transforming multiple strips).
       */
      const int pivot_point = scene->toolsettings->sequencer_tool_settings->pivot_point;
      if (pivot_point == V3D_AROUND_CURSOR) {
        SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);
        SEQ_image_preview_unit_to_px(scene, sseq->cursor, r_center);
      }
      else {
        mid_v2_v2v2(r_center, r_min, r_max);
      }
      zero_v2(r_min);
      zero_v2(r_max);
      return has_select;
    }
  }

  if (has_select == false) {
    zero_v2(r_min);
    zero_v2(r_max);
  }

  mid_v2_v2v2(r_center, r_min, r_max);
  return has_select;
}

static int gizmo2d_calc_transform_orientation(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area->spacetype != SPACE_SEQ) {
    return V3D_ORIENT_GLOBAL;
  }

  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  blender::VectorSet strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, 0);
  strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });

  bool use_local_orient = strips.size() == 1;

  if (use_local_orient) {
    return V3D_ORIENT_LOCAL;
  }
  return V3D_ORIENT_GLOBAL;
}

static float gizmo2d_calc_rotation(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area->spacetype != SPACE_SEQ) {
    return 0.0f;
  }

  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  blender::VectorSet strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, 0);
  strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });

  if (strips.size() == 1) {
    /* Only return the strip rotation if only one is selected. */
    for (Sequence *seq : strips) {
      StripTransform *transform = seq->strip->transform;
      float mirror[2];
      SEQ_image_transform_mirror_factor_get(seq, mirror);
      return transform->rotation * mirror[0] * mirror[1];
    }
  }

  return 0.0f;
}

static bool seq_get_strip_pivot_median(const Scene *scene, float r_pivot[2])
{
  zero_v2(r_pivot);

  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  blender::VectorSet strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, 0);
  strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });
  bool has_select = !strips.is_empty();

  if (has_select) {
    for (Sequence *seq : strips) {
      float origin[2];
      SEQ_image_transform_origin_offset_pixelspace_get(scene, seq, origin);
      add_v2_v2(r_pivot, origin);
    }
    mul_v2_fl(r_pivot, 1.0f / strips.size());
  }

  return has_select;
}

static bool gizmo2d_calc_transform_pivot(const bContext *C, float r_pivot[2])
{
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);
  bool has_select = false;

  if (area->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    ED_uvedit_center_from_pivot_ex(sima, scene, view_layer, r_pivot, sima->around, &has_select);
  }
  else if (area->spacetype == SPACE_SEQ) {
    SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);
    const int pivot_point = scene->toolsettings->sequencer_tool_settings->pivot_point;

    if (pivot_point == V3D_AROUND_CURSOR) {
      SEQ_image_preview_unit_to_px(scene, sseq->cursor, r_pivot);

      Editing *ed = SEQ_editing_get(scene);
      ListBase *seqbase = SEQ_active_seqbase_get(ed);
      ListBase *channels = SEQ_channels_displayed_get(ed);
      blender::VectorSet strips = SEQ_query_rendered_strips(
          scene, channels, seqbase, scene->r.cfra, 0);
      strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });
      has_select = !strips.is_empty();
    }
    else if (pivot_point == V3D_AROUND_CENTER_BOUNDS) {
      has_select = gizmo2d_calc_bounds(C, r_pivot, nullptr, nullptr);
    }
    else {
      has_select = seq_get_strip_pivot_median(scene, r_pivot);
    }
  }
  else {
    BLI_assert_msg(0, "Unhandled space type!");
  }
  return has_select;
}

/**
 * Convert origin (or any other point) from view to region space.
 */
BLI_INLINE void gizmo2d_origin_to_region(ARegion *region, float *r_origin)
{
  UI_view2d_view_to_region_fl(&region->v2d, r_origin[0], r_origin[1], &r_origin[0], &r_origin[1]);
}

/**
 * Custom handler for gizmo widgets
 */
static int gizmo2d_modal(bContext *C,
                         wmGizmo *widget,
                         const wmEvent * /*event*/,
                         eWM_GizmoFlagTweak /*tweak_flag*/)
{
  ARegion *region = CTX_wm_region(C);
  float origin[3];

  gizmo2d_calc_transform_pivot(C, origin);
  gizmo2d_origin_to_region(region, origin);
  WM_gizmo_set_matrix_location(widget, origin);

  ED_region_tag_redraw_editor_overlays(region);

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo2d_xform_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  wmOperatorType *ot_translate = WM_operatortype_find("TRANSFORM_OT_translate", true);
  GizmoGroup2D *ggd = gizmogroup2d_init(gzgroup);
  gzgroup->customdata = ggd;

  for (int i = 0; i < ARRAY_SIZE(ggd->translate_xy); i++) {
    wmGizmo *gz = ggd->translate_xy[i];

    /* Custom handler! */
    WM_gizmo_set_fn_custom_modal(gz, gizmo2d_modal);

    if (i < 2) {
      float color[4], color_hi[4];
      gizmo2d_get_axis_color(i, color, color_hi);

      /* Set up widget data. */
      RNA_float_set(gz->ptr, "length", 0.8f);
      float axis[3] = {0.0f};
      axis[i] = 1.0f;
      WM_gizmo_set_matrix_rotation_from_z_axis(gz, axis);

      float offset[3] = {0, 0, 0};
      offset[2] = 0.18f;
      WM_gizmo_set_matrix_offset_location(gz, offset);
      gz->flag |= WM_GIZMO_DRAW_OFFSET_SCALE;

      WM_gizmo_set_line_width(gz, GIZMO_AXIS_LINE_WIDTH);
      WM_gizmo_set_color(gz, color);
      WM_gizmo_set_color_highlight(gz, color_hi);

      WM_gizmo_set_scale(gz, 1.0f);
    }
    else {
      float color[4], color_hi[4];
      UI_GetThemeColor4fv(TH_GIZMO_VIEW_ALIGN, color);
      copy_v4_v4(color_hi, color);
      color[3] *= 0.6f;

      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
      RNA_property_enum_set(gz->ptr, prop, ICON_NONE);

      RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_BACKDROP);
      /* Make the center low alpha. */
      WM_gizmo_set_line_width(gz, 2.0f);
      RNA_float_set(gz->ptr, "backdrop_fill_alpha", 0.0);
      WM_gizmo_set_color(gz, color);
      WM_gizmo_set_color_highlight(gz, color_hi);

      WM_gizmo_set_scale(gz, 0.2f);
    }

    /* Assign operator. */
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot_translate, nullptr);
    if (i < 2) {
      bool constraint[3] = {false};
      constraint[i] = true;
      if (RNA_struct_find_property(ptr, "constraint_axis")) {
        RNA_boolean_set_array(ptr, "constraint_axis", constraint);
      }
    }

    RNA_boolean_set(ptr, "release_confirm", true);
  }

  {
    wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
    wmOperatorType *ot_rotate = WM_operatortype_find("TRANSFORM_OT_rotate", true);
    PointerRNA *ptr;

    /* Assign operator. */
    ptr = WM_gizmo_operator_set(ggd->cage, 0, ot_translate, nullptr);
    RNA_boolean_set(ptr, "release_confirm", true);

    const bool constraint_x[3] = {true, false, false};
    const bool constraint_y[3] = {false, true, false};

    ptr = WM_gizmo_operator_set(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X, ot_resize, nullptr);
    PropertyRNA *prop_release_confirm = RNA_struct_find_property(ptr, "release_confirm");
    PropertyRNA *prop_constraint_axis = RNA_struct_find_property(ptr, "constraint_axis");
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_x);
    ptr = WM_gizmo_operator_set(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_x);
    ptr = WM_gizmo_operator_set(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_y);
    ptr = WM_gizmo_operator_set(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_y);

    ptr = WM_gizmo_operator_set(
        ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MIN_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    ptr = WM_gizmo_operator_set(
        ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MAX_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    ptr = WM_gizmo_operator_set(
        ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MIN_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    ptr = WM_gizmo_operator_set(
        ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MAX_Y, ot_resize, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
    ptr = WM_gizmo_operator_set(ggd->cage, ED_GIZMO_CAGE2D_PART_ROTATE, ot_rotate, nullptr);
    RNA_property_boolean_set(ptr, prop_release_confirm, true);
  }
}

static void rotate_around_center_v2(float point[2], const float center[2], const float angle)
{
  float tmp[2];

  sub_v2_v2v2(tmp, point, center);
  rotate_v2_v2fl(point, tmp, angle);
  add_v2_v2(point, center);
}

static void gizmo2d_xform_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroup2D *ggd = static_cast<GizmoGroup2D *>(gzgroup->customdata);
  bool has_select;
  if (ggd->no_cage) {
    has_select = gizmo2d_calc_transform_pivot(C, ggd->origin);
  }
  else {
    has_select = gizmo2d_calc_bounds(C, ggd->origin, ggd->min, ggd->max);
    ggd->rotation = gizmo2d_calc_rotation(C);
  }

  bool show_cage = !ggd->no_cage && !equals_v2v2(ggd->min, ggd->max);

  if (has_select == false) {
    /* Nothing selected. Disable gizmo drawing and return. */
    ggd->cage->flag |= WM_GIZMO_HIDDEN;
    for (int i = 0; i < ARRAY_SIZE(ggd->translate_xy); i++) {
      ggd->translate_xy[i]->flag |= WM_GIZMO_HIDDEN;
    }
    return;
  }

  if (!show_cage) {
    /* Disable cage gizmo drawing and return. */
    ggd->cage->flag |= WM_GIZMO_HIDDEN;
    for (int i = 0; i < ARRAY_SIZE(ggd->translate_xy); i++) {
      ggd->translate_xy[i]->flag &= ~WM_GIZMO_HIDDEN;
    }
    return;
  }

  /* We will show the cage gizmo! Setup all necessary data. */
  ggd->cage->flag &= ~WM_GIZMO_HIDDEN;
  for (int i = 0; i < ARRAY_SIZE(ggd->translate_xy); i++) {
    ggd->translate_xy[i]->flag |= WM_GIZMO_HIDDEN;
  }
}

static void gizmo2d_xform_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  GizmoGroup2D *ggd = static_cast<GizmoGroup2D *>(gzgroup->customdata);
  float origin[3] = {UNPACK2(ggd->origin), 0.0f};

  gizmo2d_origin_to_region(region, origin);

  for (int i = 0; i < ARRAY_SIZE(ggd->translate_xy); i++) {
    wmGizmo *gz = ggd->translate_xy[i];
    WM_gizmo_set_matrix_location(gz, origin);
  }

  UI_view2d_view_to_region_m4(&region->v2d, ggd->cage->matrix_space);
  /* Define the bounding box of the gizmo in the offset transform matrix. */
  unit_m4(ggd->cage->matrix_offset);
  const float min_gizmo_pixel_size = 0.001f; /* Draw Gizmo larger than this many pixels. */
  const float min_scale_axis_x = min_gizmo_pixel_size / ggd->cage->matrix_space[0][0];
  const float min_scale_axis_y = min_gizmo_pixel_size / ggd->cage->matrix_space[1][1];
  ggd->cage->matrix_offset[0][0] = max_ff(min_scale_axis_x, ggd->max[0] - ggd->min[0]);
  ggd->cage->matrix_offset[1][1] = max_ff(min_scale_axis_y, ggd->max[1] - ggd->min[1]);

  ScrArea *area = CTX_wm_area(C);

  if (area->spacetype == SPACE_SEQ) {
    Scene *scene = CTX_data_scene(C);
    seq_get_strip_pivot_median(scene, origin);

    float matrix_rotate[4][4];
    unit_m4(matrix_rotate);
    copy_v3_v3(matrix_rotate[3], origin);
    rotate_m4(matrix_rotate, 'Z', ggd->rotation);
    unit_m4(ggd->cage->matrix_basis);
    mul_m4_m4m4(ggd->cage->matrix_basis, matrix_rotate, ggd->cage->matrix_basis);

    float mid[2];
    sub_v2_v2v2(mid, origin, ggd->origin);
    mul_v2_fl(mid, -1.0f);
    copy_v2_v2(ggd->cage->matrix_offset[3], mid);
  }
  else {
    const float origin_aa[3] = {UNPACK2(ggd->origin), 0.0f};
    WM_gizmo_set_matrix_offset_location(ggd->cage, origin_aa);
  }
}

static void gizmo2d_xform_invoke_prepare(const bContext *C,
                                         wmGizmoGroup *gzgroup,
                                         wmGizmo * /*gz*/,
                                         const wmEvent * /*event*/)
{
  GizmoGroup2D *ggd = static_cast<GizmoGroup2D *>(gzgroup->customdata);
  wmGizmoOpElem *gzop;
  const float *mid = ggd->origin;
  const float *min = ggd->min;
  const float *max = ggd->max;

  /* Define the different transform center points that will be used when grabbing the corners or
   * rotating with the gizmo.
   *
   * The coordinates are referred to as their cardinal directions:
   *       N
   *       o
   *NW     |     NE
   * x-----------x
   * |           |
   *W|     C     |E
   * |           |
   * x-----------x
   *SW     S     SE
   */
  float n[3] = {mid[0], max[1], 0.0f};
  float w[3] = {min[0], mid[1], 0.0f};
  float e[3] = {max[0], mid[1], 0.0f};
  float s[3] = {mid[0], min[1], 0.0f};

  float nw[3] = {min[0], max[1], 0.0f};
  float ne[3] = {max[0], max[1], 0.0f};
  float sw[3] = {min[0], min[1], 0.0f};
  float se[3] = {max[0], min[1], 0.0f};

  float c[3] = {mid[0], mid[1], 0.0f};

  float orient_matrix[3][3];
  unit_m3(orient_matrix);

  ScrArea *area = CTX_wm_area(C);

  if (ggd->rotation != 0.0f && area->spacetype == SPACE_SEQ) {
    float origin[3];
    Scene *scene = CTX_data_scene(C);
    seq_get_strip_pivot_median(scene, origin);
    /* We need to rotate the cardinal points so they align with the rotated bounding box. */

    rotate_around_center_v2(n, origin, ggd->rotation);
    rotate_around_center_v2(w, origin, ggd->rotation);
    rotate_around_center_v2(e, origin, ggd->rotation);
    rotate_around_center_v2(s, origin, ggd->rotation);

    rotate_around_center_v2(nw, origin, ggd->rotation);
    rotate_around_center_v2(ne, origin, ggd->rotation);
    rotate_around_center_v2(sw, origin, ggd->rotation);
    rotate_around_center_v2(se, origin, ggd->rotation);

    rotate_around_center_v2(c, origin, ggd->rotation);

    axis_angle_to_mat3_single(orient_matrix, 'Z', ggd->rotation);
  }

  int orient_type = gizmo2d_calc_transform_orientation(C);

  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X);
  PropertyRNA *prop_center_override = RNA_struct_find_property(&gzop->ptr, "center_override");
  PropertyRNA *prop_mouse_dir = RNA_struct_find_property(&gzop->ptr, "mouse_dir_constraint");
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, e);
  RNA_property_float_set_array(&gzop->ptr, prop_mouse_dir, orient_matrix[0]);
  RNA_enum_set(&gzop->ptr, "orient_type", orient_type);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, w);
  RNA_property_float_set_array(&gzop->ptr, prop_mouse_dir, orient_matrix[0]);
  RNA_enum_set(&gzop->ptr, "orient_type", orient_type);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, n);
  RNA_property_float_set_array(&gzop->ptr, prop_mouse_dir, orient_matrix[1]);
  RNA_enum_set(&gzop->ptr, "orient_type", orient_type);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, s);
  RNA_property_float_set_array(&gzop->ptr, prop_mouse_dir, orient_matrix[1]);
  RNA_enum_set(&gzop->ptr, "orient_type", orient_type);

  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MIN_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, ne);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MAX_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, se);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MIN_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, nw);
  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MAX_Y);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, sw);

  gzop = WM_gizmo_operator_get(ggd->cage, ED_GIZMO_CAGE2D_PART_ROTATE);
  RNA_property_float_set_array(&gzop->ptr, prop_center_override, c);
}

void ED_widgetgroup_gizmo2d_xform_callbacks_set(wmGizmoGroupType *gzgt)
{
  gzgt->poll = gizmo2d_generic_poll;
  gzgt->setup = gizmo2d_xform_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = gizmo2d_xform_refresh;
  gzgt->draw_prepare = gizmo2d_xform_draw_prepare;
  gzgt->invoke_prepare = gizmo2d_xform_invoke_prepare;
}

static void gizmo2d_xform_setup_no_cage(const bContext *C, wmGizmoGroup *gzgroup)
{
  gizmo2d_xform_setup(C, gzgroup);
  GizmoGroup2D *ggd = static_cast<GizmoGroup2D *>(gzgroup->customdata);
  ggd->no_cage = true;
}

static void gizmo2d_xform_no_cage_message_subscribe(const bContext *C,
                                                    wmGizmoGroup *gzgroup,
                                                    wmMsgBus *mbus)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo2d_pivot_point_message_subscribe(gzgroup, mbus, screen, area, region);
}

void ED_widgetgroup_gizmo2d_xform_no_cage_callbacks_set(wmGizmoGroupType *gzgt)
{
  ED_widgetgroup_gizmo2d_xform_callbacks_set(gzgt);
  gzgt->setup = gizmo2d_xform_setup_no_cage;
  gzgt->message_subscribe = gizmo2d_xform_no_cage_message_subscribe;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scale Handles
 *
 * Defines public functions, not the gizmo itself:
 *
 * - #ED_widgetgroup_gizmo2d_resize_callbacks_set
 *
 * \{ */

struct GizmoGroup_Resize2D {
  wmGizmo *gizmo_xy[3];
  float origin[2];
  float rotation;
};

static GizmoGroup_Resize2D *gizmogroup2d_resize_init(wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_button = WM_gizmotype_find("GIZMO_GT_button_2d", true);

  GizmoGroup_Resize2D *ggd = static_cast<GizmoGroup_Resize2D *>(
      MEM_callocN(sizeof(GizmoGroup_Resize2D), __func__));

  ggd->gizmo_xy[0] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->gizmo_xy[1] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->gizmo_xy[2] = WM_gizmo_new_ptr(gzt_button, gzgroup, nullptr);

  return ggd;
}

static void gizmo2d_resize_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroup_Resize2D *ggd = static_cast<GizmoGroup_Resize2D *>(gzgroup->customdata);
  float origin[3];
  const bool has_select = gizmo2d_calc_transform_pivot(C, origin);

  if (has_select == false) {
    for (int i = 0; i < ARRAY_SIZE(ggd->gizmo_xy); i++) {
      ggd->gizmo_xy[i]->flag |= WM_GIZMO_HIDDEN;
    }
  }
  else {
    for (int i = 0; i < ARRAY_SIZE(ggd->gizmo_xy); i++) {
      ggd->gizmo_xy[i]->flag &= ~WM_GIZMO_HIDDEN;
    }
    copy_v2_v2(ggd->origin, origin);
    ggd->rotation = gizmo2d_calc_rotation(C);
  }
}

static void gizmo2d_resize_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  GizmoGroup_Resize2D *ggd = static_cast<GizmoGroup_Resize2D *>(gzgroup->customdata);
  float origin[3] = {UNPACK2(ggd->origin), 0.0f};

  gizmo2d_origin_to_region(region, origin);

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmo_xy); i++) {
    wmGizmo *gz = ggd->gizmo_xy[i];
    WM_gizmo_set_matrix_location(gz, origin);

    if (i < 2) {
      float axis[3] = {0.0f}, rotated_axis[3];
      axis[i] = 1.0f;
      rotate_v3_v3v3fl(rotated_axis, axis, blender::float3{0, 0, 1}, ggd->rotation);
      WM_gizmo_set_matrix_rotation_from_z_axis(gz, rotated_axis);
    }
  }
}

static void gizmo2d_resize_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{

  wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
  GizmoGroup_Resize2D *ggd = gizmogroup2d_resize_init(gzgroup);
  gzgroup->customdata = ggd;

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmo_xy); i++) {
    wmGizmo *gz = ggd->gizmo_xy[i];

    /* Custom handler! */
    WM_gizmo_set_fn_custom_modal(gz, gizmo2d_modal);

    if (i < 2) {
      float color[4], color_hi[4];
      gizmo2d_get_axis_color(i, color, color_hi);

      /* Set up widget data. */
      RNA_float_set(gz->ptr, "length", 1.0f);
      RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_BOX);

      WM_gizmo_set_line_width(gz, GIZMO_AXIS_LINE_WIDTH);
      WM_gizmo_set_color(gz, color);
      WM_gizmo_set_color_highlight(gz, color_hi);

      WM_gizmo_set_scale(gz, 1.0f);
    }
    else {
      float color[4], color_hi[4];
      UI_GetThemeColor4fv(TH_GIZMO_VIEW_ALIGN, color);
      copy_v4_v4(color_hi, color);
      color[3] *= 0.6f;

      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
      RNA_property_enum_set(gz->ptr, prop, ICON_NONE);

      RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_BACKDROP);
      /* Make the center low alpha. */
      WM_gizmo_set_line_width(gz, 2.0f);
      RNA_float_set(gz->ptr, "backdrop_fill_alpha", 0.0);
      WM_gizmo_set_color(gz, color);
      WM_gizmo_set_color_highlight(gz, color_hi);

      WM_gizmo_set_scale(gz, 1.2f);
    }

    /* Assign operator. */
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot_resize, nullptr);
    if (i < 2) {
      bool constraint[3] = {false};
      constraint[i] = true;
      if (RNA_struct_find_property(ptr, "constraint_axis")) {
        RNA_boolean_set_array(ptr, "constraint_axis", constraint);
      }
    }
    RNA_boolean_set(ptr, "release_confirm", true);
  }
}

static void gizmo2d_resize_invoke_prepare(const bContext *C,
                                          wmGizmoGroup * /*gzgroup*/,
                                          wmGizmo *gz,
                                          const wmEvent * /*event*/)
{
  wmGizmoOpElem *gzop;
  int orient_type = gizmo2d_calc_transform_orientation(C);

  gzop = WM_gizmo_operator_get(gz, 0);
  RNA_enum_set(&gzop->ptr, "orient_type", orient_type);
}

static void gizmo2d_resize_message_subscribe(const bContext *C,
                                             wmGizmoGroup *gzgroup,
                                             wmMsgBus *mbus)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo2d_pivot_point_message_subscribe(gzgroup, mbus, screen, area, region);
}

void ED_widgetgroup_gizmo2d_resize_callbacks_set(wmGizmoGroupType *gzgt)
{
  gzgt->poll = gizmo2d_generic_poll;
  gzgt->setup = gizmo2d_resize_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = gizmo2d_resize_refresh;
  gzgt->draw_prepare = gizmo2d_resize_draw_prepare;
  gzgt->invoke_prepare = gizmo2d_resize_invoke_prepare;
  gzgt->message_subscribe = gizmo2d_resize_message_subscribe;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotate Handles
 *
 * Defines public functions, not the gizmo itself:
 *
 * - #ED_widgetgroup_gizmo2d_rotate_setup
 *
 * \{ */

struct GizmoGroup_Rotate2D {
  wmGizmo *gizmo;
  float origin[2];
};

static GizmoGroup_Rotate2D *gizmogroup2d_rotate_init(wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_button = WM_gizmotype_find("GIZMO_GT_button_2d", true);

  GizmoGroup_Rotate2D *ggd = static_cast<GizmoGroup_Rotate2D *>(
      MEM_callocN(sizeof(GizmoGroup_Rotate2D), __func__));

  ggd->gizmo = WM_gizmo_new_ptr(gzt_button, gzgroup, nullptr);

  return ggd;
}

static void gizmo2d_rotate_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroup_Rotate2D *ggd = static_cast<GizmoGroup_Rotate2D *>(gzgroup->customdata);
  float origin[3];
  const bool has_select = gizmo2d_calc_transform_pivot(C, origin);

  if (has_select == false) {
    ggd->gizmo->flag |= WM_GIZMO_HIDDEN;
  }
  else {
    ggd->gizmo->flag &= ~WM_GIZMO_HIDDEN;
    copy_v2_v2(ggd->origin, origin);
  }
}

static void gizmo2d_rotate_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  GizmoGroup_Rotate2D *ggd = static_cast<GizmoGroup_Rotate2D *>(gzgroup->customdata);
  float origin[3] = {UNPACK2(ggd->origin), 0.0f};

  gizmo2d_origin_to_region(region, origin);

  wmGizmo *gz = ggd->gizmo;
  WM_gizmo_set_matrix_location(gz, origin);
}

static void gizmo2d_rotate_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{

  wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_rotate", true);
  GizmoGroup_Rotate2D *ggd = gizmogroup2d_rotate_init(gzgroup);
  gzgroup->customdata = ggd;

  /* Other setup functions iterate over axis. */
  {
    wmGizmo *gz = ggd->gizmo;

    /* Custom handler! */
    WM_gizmo_set_fn_custom_modal(gz, gizmo2d_modal);
    WM_gizmo_set_scale(gz, 1.2f);

    {
      float color[4];
      UI_GetThemeColor4fv(TH_GIZMO_VIEW_ALIGN, color);

      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
      RNA_property_enum_set(gz->ptr, prop, ICON_NONE);

      RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_BACKDROP);
      /* Make the center low alpha. */
      WM_gizmo_set_line_width(gz, 2.0f);
      RNA_float_set(gz->ptr, "backdrop_fill_alpha", 0.0);
      WM_gizmo_set_color(gz, color);
      WM_gizmo_set_color_highlight(gz, color);
    }

    /* Assign operator. */
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot_resize, nullptr);
    RNA_boolean_set(ptr, "release_confirm", true);
  }
}

static void gizmo2d_rotate_message_subscribe(const bContext *C,
                                             wmGizmoGroup *gzgroup,
                                             wmMsgBus *mbus)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo2d_pivot_point_message_subscribe(gzgroup, mbus, screen, area, region);
}

void ED_widgetgroup_gizmo2d_rotate_callbacks_set(wmGizmoGroupType *gzgt)
{
  gzgt->poll = gizmo2d_generic_poll;
  gzgt->setup = gizmo2d_rotate_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = gizmo2d_rotate_refresh;
  gzgt->draw_prepare = gizmo2d_rotate_draw_prepare;
  gzgt->message_subscribe = gizmo2d_rotate_message_subscribe;
}

/** \} */
