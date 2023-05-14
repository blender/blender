/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_span.hh"

#include "DNA_anim_types.h"
#include "DNA_sequence_types.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_scene.h"

#include "BLF_api.h"

#include "GPU_batch.h"
#include "GPU_batch_utils.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gizmo_library.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "SEQ_iterator.h"
#include "SEQ_retiming.h"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

/* Own include. */
#include "sequencer_intern.h"

using blender::MutableSpan;

/** Size in pixels. */
#define RETIME_HANDLE_MOUSEOVER_THRESHOLD (16.0f * UI_SCALE_FAC)
/** Factor based on icon size. */
#define RETIME_BUTTON_SIZE 0.6f

static float remove_gizmo_height_get(const View2D *v2d)
{
  const float max_size = (SEQ_STRIP_OFSTOP - SEQ_STRIP_OFSBOTTOM) * UI_view2d_scale_get_y(v2d);
  return min_ff(14.0f * UI_SCALE_FAC, max_size * 0.4f);
}

static float strip_y_rescale(const Sequence *seq, const float y_value)
{
  const float y_range = SEQ_STRIP_OFSTOP - SEQ_STRIP_OFSBOTTOM;
  return (y_value * y_range) + seq->machine + SEQ_STRIP_OFSBOTTOM;
}

static float handle_x_get(const Scene *scene, const Sequence *seq, const SeqRetimingHandle *handle)
{

  const SeqRetimingHandle *last_handle = SEQ_retiming_last_handle_get(seq);
  const bool is_last_handle = (handle == last_handle);

  return SEQ_retiming_handle_timeline_frame_get(scene, seq, handle) + (is_last_handle ? 1 : 0);
}

static const SeqRetimingHandle *mouse_over_handle_get(const Scene *scene,
                                                      const Sequence *seq,
                                                      const View2D *v2d,
                                                      const int mval[2])
{
  int best_distance = INT_MAX;
  const SeqRetimingHandle *best_handle = nullptr;

  MutableSpan handles = SEQ_retiming_handles_get(seq);
  for (const SeqRetimingHandle &handle : handles) {
    int distance = round_fl_to_int(
        fabsf(UI_view2d_view_to_region_x(v2d, handle_x_get(scene, seq, &handle)) - mval[0]));

    if (distance < RETIME_HANDLE_MOUSEOVER_THRESHOLD && distance < best_distance) {
      best_distance = distance;
      best_handle = &handle;
    }
  }

  return best_handle;
}

static float pixels_to_view_width(const bContext *C, const float width)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  float scale_x = UI_view2d_view_to_region_x(v2d, 1) - UI_view2d_view_to_region_x(v2d, 0.0f);
  return width / scale_x;
}

static float pixels_to_view_height(const bContext *C, const float height)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  float scale_y = UI_view2d_view_to_region_y(v2d, 1) - UI_view2d_view_to_region_y(v2d, 0.0f);
  return height / scale_y;
}

static float strip_start_screenspace_get(const bContext *C, const Sequence *seq)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  const Scene *scene = CTX_data_scene(C);
  return UI_view2d_view_to_region_x(v2d, SEQ_time_left_handle_frame_get(scene, seq));
}

static float strip_end_screenspace_get(const bContext *C, const Sequence *seq)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  const Scene *scene = CTX_data_scene(C);
  return UI_view2d_view_to_region_x(v2d, SEQ_time_right_handle_frame_get(scene, seq));
}

static Sequence *active_seq_from_context(const bContext *C)
{
  const Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  return ed->act_seq;
}

static rctf strip_box_get(const bContext *C, const Sequence *seq)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  rctf rect;
  rect.xmin = strip_start_screenspace_get(C, seq);
  rect.xmax = strip_end_screenspace_get(C, seq);
  rect.ymin = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 0));
  rect.ymax = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 1));
  return rect;
}

static rctf remove_box_get(const bContext *C, const Sequence *seq)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  rctf rect = strip_box_get(C, seq);
  rect.ymax = rect.ymin + remove_gizmo_height_get(v2d);
  return rect;
}

static bool mouse_is_inside_box(const rctf *box, const int mval[2])
{
  return mval[0] >= box->xmin && mval[0] <= box->xmax && mval[1] >= box->ymin &&
         mval[1] <= box->ymax;
}

/* -------------------------------------------------------------------- */
/** \name Retiming Add Handle Gizmo
 * \{ */

typedef struct RetimeButtonGizmo {
  wmGizmo gizmo;
  int icon_id;
  const Sequence *seq_under_mouse;
  bool is_mouse_over_gizmo;
} RetimeButtonGizmo;

typedef struct ButtonDimensions {
  float height;
  float width;
  float x;
  float y;
} ButtonDimensions;

static ButtonDimensions button_dimensions_get(const bContext *C, const RetimeButtonGizmo *gizmo)
{
  const Scene *scene = CTX_data_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  const Sequence *seq = active_seq_from_context(C);

  const float icon_height = UI_icon_get_height(gizmo->icon_id) * UI_SCALE_FAC;
  const float icon_width = UI_icon_get_width(gizmo->icon_id) * UI_SCALE_FAC;
  const float icon_x = UI_view2d_view_to_region_x(v2d, BKE_scene_frame_get(scene)) +
                       icon_width / 2;
  const float icon_y = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 0.5)) -
                       icon_height / 2;
  const ButtonDimensions dimensions = {icon_height, icon_width, icon_x, icon_y};
  return dimensions;
}

static rctf button_box_get(const bContext *C, const RetimeButtonGizmo *gizmo)
{
  ButtonDimensions button_dimensions = button_dimensions_get(C, gizmo);

  float x_range = button_dimensions.width;
  float y_range = button_dimensions.height;

  rctf rect;
  rect.xmin = button_dimensions.x;
  rect.xmax = button_dimensions.x + x_range;
  rect.ymin = button_dimensions.y;
  rect.ymax = button_dimensions.y + y_range;

  return rect;
}

static void gizmo_retime_handle_add_draw(const bContext *C, wmGizmo *gz)
{
  RetimeButtonGizmo *gizmo = (RetimeButtonGizmo *)gz;

  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    return;
  }

  const ButtonDimensions button = button_dimensions_get(C, gizmo);
  const rctf strip_box = strip_box_get(C, active_seq_from_context(C));
  if (!BLI_rctf_isect_pt(&strip_box, button.x, button.y)) {
    return;
  }

  wmOrtho2_region_pixelspace(CTX_wm_region(C));
  GPU_blend(GPU_BLEND_ALPHA);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  const float alpha = gizmo->is_mouse_over_gizmo ? 1.0f : 0.6f;

  immUniformColor4f(0.2f, 0.2f, 0.2f, alpha);
  imm_draw_circle_fill_2d(pos,
                          button.x + button.width / 2,
                          button.y + button.height / 2,
                          button.width * RETIME_BUTTON_SIZE,
                          32);
  immUnbindProgram();

  GPU_polygon_smooth(false);
  UI_icon_draw_alpha(button.x, button.y, gizmo->icon_id, alpha);
  GPU_polygon_smooth(true);

  GPU_blend(GPU_BLEND_NONE);
}

static int gizmo_retime_handle_add_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  RetimeButtonGizmo *gizmo = (RetimeButtonGizmo *)gz;
  Sequence *seq = active_seq_from_context(C);

  Sequence *mouse_over_seq = nullptr;
  gizmo->is_mouse_over_gizmo = false;

  /* Store strip under mouse cursor. */
  const rctf strip_box = strip_box_get(C, seq);
  if (mouse_is_inside_box(&strip_box, mval)) {
    mouse_over_seq = seq;
  }

  if (gizmo->seq_under_mouse != mouse_over_seq) {
    gizmo->seq_under_mouse = mouse_over_seq;
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  }

  if (gizmo->seq_under_mouse == nullptr) {
    return -1;
  }

  const rctf button_box = button_box_get(C, gizmo);
  if (!mouse_is_inside_box(&button_box, mval)) {
    return -1;
  }

  gizmo->is_mouse_over_gizmo = true;
  const Scene *scene = CTX_data_scene(C);
  wmGizmoOpElem *op_elem = WM_gizmo_operator_get(gz, 0);
  RNA_int_set(&op_elem->ptr, "timeline_frame", BKE_scene_frame_get(scene));

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return 0;
}

static void gizmo_retime_handle_add_setup(wmGizmo *gz)
{
  RetimeButtonGizmo *gizmo = (RetimeButtonGizmo *)gz;
  gizmo->icon_id = ICON_ADD;
}

void GIZMO_GT_retime_handle_add(wmGizmoType *gzt)
{
  /* Identifiers. */
  gzt->idname = "GIZMO_GT_retime_handle_add";

  /* Api callbacks. */
  gzt->setup = gizmo_retime_handle_add_setup;
  gzt->draw = gizmo_retime_handle_add_draw;
  gzt->test_select = gizmo_retime_handle_add_test_select;
  gzt->struct_size = sizeof(RetimeButtonGizmo);

  /* Currently only used for cursor display. */
  RNA_def_boolean(gzt->srna, "show_drag", true, "Show Drag", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Move Handle Gizmo
 * \{ */

typedef struct RetimeHandleMoveGizmo {
  wmGizmo gizmo;
  const Sequence *mouse_over_seq;
  int mouse_over_handle_x;
} RetimeHandleMoveGizmo;

static void retime_handle_draw(const bContext *C,
                               const RetimeHandleMoveGizmo *gizmo,
                               uint pos,
                               const Sequence *seq,
                               const SeqRetimingHandle *handle)
{
  const Scene *scene = CTX_data_scene(C);
  const float handle_x = handle_x_get(scene, seq, handle);

  if (handle_x == SEQ_time_left_handle_frame_get(scene, seq)) {
    return;
  }
  if (handle_x == SEQ_time_right_handle_frame_get(scene, seq)) {
    return;
  }

  const View2D *v2d = UI_view2d_fromcontext(C);
  const rctf strip_box = strip_box_get(C, seq);
  if (!BLI_rctf_isect_x(&strip_box, UI_view2d_view_to_region_x(v2d, handle_x))) {
    return; /* Handle out of strip bounds. */
  }

  const int ui_triangle_size = remove_gizmo_height_get(v2d);
  const float bottom = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 0.0f)) + 2;
  const float top = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 1.0f)) - 2;
  const float handle_position = UI_view2d_view_to_region_x(v2d, handle_x);

  if (seq == gizmo->mouse_over_seq && handle_x == gizmo->mouse_over_handle_x) {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  }
  else {
    immUniformColor4f(0.65f, 0.65f, 0.65f, 1.0f);
  }

  immBegin(GPU_PRIM_TRI_FAN, 3);
  immVertex2f(pos, handle_position - ui_triangle_size / 2, bottom);
  immVertex2f(pos, handle_position + ui_triangle_size / 2, bottom);
  immVertex2f(pos, handle_position, bottom + ui_triangle_size);

  immEnd();

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, handle_position, bottom);
  immVertex2f(pos, handle_position, top);
  immEnd();
}

static void retime_speed_text_draw(const bContext *C,
                                   const Sequence *seq,
                                   const SeqRetimingHandle *handle)
{
  SeqRetimingHandle *last_handle = SEQ_retiming_last_handle_get(seq);
  if (handle == last_handle) {
    return;
  }

  const Scene *scene = CTX_data_scene(C);
  const int start_frame = SEQ_time_left_handle_frame_get(scene, seq);
  const int end_frame = SEQ_time_right_handle_frame_get(scene, seq);

  int next_handle_index = SEQ_retiming_handle_index_get(seq, handle) + 1;
  const SeqRetimingHandle *next_handle = &SEQ_retiming_handles_get(seq)[next_handle_index];
  if (handle_x_get(scene, seq, next_handle) < start_frame ||
      handle_x_get(scene, seq, handle) > end_frame)
  {
    return; /* Label out of strip bounds. */
  }

  const float speed = SEQ_retiming_handle_speed_get(seq, next_handle);

  char label_str[20];
  const size_t label_len = SNPRINTF_RLEN(label_str, "%d%%", round_fl_to_int(speed * 100.0f));

  const float width = pixels_to_view_width(C, BLF_width(BLF_default(), label_str, label_len));

  const float xmin = max_ff(SEQ_time_left_handle_frame_get(scene, seq),
                            handle_x_get(scene, seq, handle));
  const float xmax = min_ff(SEQ_time_right_handle_frame_get(scene, seq),
                            handle_x_get(scene, seq, next_handle));

  const float text_x = (xmin + xmax - width) / 2;
  const float text_y = strip_y_rescale(seq, 0) + pixels_to_view_height(C, 5);

  if (width > xmax - xmin) {
    return; /* Not enough space to draw label. */
  }

  const uchar col[4] = {255, 255, 255, 255};
  UI_view2d_text_cache_add(UI_view2d_fromcontext(C), text_x, text_y, label_str, label_len, col);
}

static void gizmo_retime_handle_draw(const bContext *C, wmGizmo *gz)
{
  const RetimeHandleMoveGizmo *gizmo = (RetimeHandleMoveGizmo *)gz;
  const View2D *v2d = UI_view2d_fromcontext(C);

  wmOrtho2_region_pixelspace(CTX_wm_region(C));
  GPU_blend(GPU_BLEND_ALPHA);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  Sequence *seq = active_seq_from_context(C);
  SEQ_retiming_data_ensure(seq);
  MutableSpan handles = SEQ_retiming_handles_get(seq);

  for (const SeqRetimingHandle &handle : handles) {
    retime_speed_text_draw(C, seq, &handle);

    if (&handle == handles.begin()) {
      continue; /* Ignore first handle. */
    }
    retime_handle_draw(C, gizmo, pos, seq, &handle);
  }

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  UI_view2d_text_cache_draw(CTX_wm_region(C));
  UI_view2d_view_ortho(v2d); /* 'UI_view2d_text_cache_draw()' messes up current view. */
}

static int gizmo_retime_handle_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  RetimeHandleMoveGizmo *gizmo = (RetimeHandleMoveGizmo *)gz;
  Scene *scene = CTX_data_scene(C);

  gizmo->mouse_over_seq = nullptr;

  Sequence *seq = active_seq_from_context(C);
  SEQ_retiming_data_ensure(seq);
  const SeqRetimingHandle *handle = mouse_over_handle_get(
      scene, seq, UI_view2d_fromcontext(C), mval);
  const int handle_index = SEQ_retiming_handle_index_get(seq, handle);

  if (handle == nullptr) {
    return -1;
  }

  if (handle_x_get(scene, seq, handle) == SEQ_time_left_handle_frame_get(scene, seq) ||
      handle_index == 0)
  {
    return -1;
  }

  const View2D *v2d = UI_view2d_fromcontext(C);
  rctf strip_box = strip_box_get(C, seq);
  BLI_rctf_resize_x(&strip_box, BLI_rctf_size_x(&strip_box) + 2 * remove_gizmo_height_get(v2d));
  if (!mouse_is_inside_box(&strip_box, mval)) {
    return -1;
  }

  gizmo->mouse_over_seq = seq;
  gizmo->mouse_over_handle_x = handle_x_get(scene, seq, handle);

  wmGizmoOpElem *op_elem = WM_gizmo_operator_get(gz, 0);
  RNA_int_set(&op_elem->ptr, "handle_index", handle_index);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return 0;
}

static int gizmo_retime_handle_cursor_get(wmGizmo *gz)
{
  if (RNA_boolean_get(gz->ptr, "show_drag")) {
    return WM_CURSOR_EW_SCROLL;
  }
  return WM_CURSOR_DEFAULT;
}

static void gizmo_retime_handle_setup(wmGizmo *gz)
{
  gz->flag = WM_GIZMO_DRAW_MODAL;
}

void GIZMO_GT_retime_handle(wmGizmoType *gzt)
{
  /* Identifiers. */
  gzt->idname = "GIZMO_GT_retime_handle_move";

  /* Api callbacks. */
  gzt->setup = gizmo_retime_handle_setup;
  gzt->draw = gizmo_retime_handle_draw;
  gzt->test_select = gizmo_retime_handle_test_select;
  gzt->cursor_get = gizmo_retime_handle_cursor_get;
  gzt->struct_size = sizeof(RetimeHandleMoveGizmo);

  /* Currently only used for cursor display. */
  RNA_def_boolean(gzt->srna, "show_drag", true, "Show Drag", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Remove Handle Gizmo
 * \{ */

static void gizmo_retime_remove_draw(const bContext * /* C */, wmGizmo * /* gz */)
{
  /* Pass. */
}

static int gizmo_retime_remove_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq = active_seq_from_context(C);

  SEQ_retiming_data_ensure(seq);
  const SeqRetimingHandle *handle = mouse_over_handle_get(
      scene, seq, UI_view2d_fromcontext(C), mval);
  const int handle_index = SEQ_retiming_handle_index_get(seq, handle);

  if (handle == nullptr) {
    return -1;
  }

  if (handle_x_get(scene, seq, handle) == SEQ_time_left_handle_frame_get(scene, seq) ||
      handle_index == 0)
  {
    return -1; /* Ignore first handle. */
  }

  SeqRetimingHandle *last_handle = SEQ_retiming_last_handle_get(seq);
  if (handle == last_handle) {
    return -1; /* Last handle can not be removed. */
  }

  const View2D *v2d = UI_view2d_fromcontext(C);
  rctf box = remove_box_get(C, seq);
  BLI_rctf_resize_x(&box, BLI_rctf_size_x(&box) + 2 * remove_gizmo_height_get(v2d));
  if (!mouse_is_inside_box(&box, mval)) {
    return -1;
  }

  wmGizmoOpElem *op_elem = WM_gizmo_operator_get(gz, 0);
  RNA_int_set(&op_elem->ptr, "handle_index", handle_index);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return 0;
}

static int gizmo_retime_remove_cursor_get(wmGizmo *gz)
{
  if (RNA_boolean_get(gz->ptr, "show_drag")) {
    return WM_CURSOR_ERASER;
  }
  return WM_CURSOR_DEFAULT;
}

void GIZMO_GT_retime_remove(wmGizmoType *gzt)
{
  /* Identifiers. */
  gzt->idname = "GIZMO_GT_retime_handle_remove";

  /* Api callbacks. */
  gzt->draw = gizmo_retime_remove_draw;
  gzt->test_select = gizmo_retime_remove_test_select;
  gzt->cursor_get = gizmo_retime_remove_cursor_get;
  gzt->struct_size = sizeof(wmGizmo);

  /* Currently only used for cursor display. */
  RNA_def_boolean(gzt->srna, "show_drag", true, "Show Drag", "");
}

/** \} */
