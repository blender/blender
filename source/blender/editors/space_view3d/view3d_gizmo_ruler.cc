/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_unit.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"

#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"
#include "ED_gpencil_legacy.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "DEG_depsgraph_query.h"

#include "view3d_intern.h" /* own include */

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.h"

using blender::float2;
using blender::float2x2;
using blender::float3;
using blender::float3x2;
using blender::float3x3;
using blender::float4;

/**
 * Supporting transform features could be removed if the actual transform system is used.
 * Keep the option open since each transform feature is duplicating logic.
 */
#define USE_AXIS_CONSTRAINTS

static const char *view3d_gzgt_ruler_id = "VIEW3D_GGT_ruler";

#define MVAL_MAX_PX_DIST 12.0f

/* -------------------------------------------------------------------- */
/* Ruler Item (we can have many) */

enum {
  /** Use protractor. */
  RULERITEM_USE_ANGLE = (1 << 0),
  /** Protractor vertex is selected (deleting removes it). */
  RULERITEM_USE_ANGLE_ACTIVE = (1 << 1),
};

/* keep smaller than selection, since we may want click elsewhere without selecting a ruler */
#define RULER_PICK_DIST 12.0f
#define RULER_PICK_DIST_SQ (RULER_PICK_DIST * RULER_PICK_DIST)

/* not clicking on a point */
#define PART_LINE 0xff

/* -------------------------------------------------------------------- */
/* Ruler Info (wmGizmoGroup customdata) */

enum {
  RULER_STATE_NORMAL = 0,
  RULER_STATE_DRAG,
};

#ifdef USE_AXIS_CONSTRAINTS
/* Constrain axes */
enum {
  CONSTRAIN_AXIS_NONE = -1,
  CONSTRAIN_AXIS_X = 0,
  CONSTRAIN_AXIS_Y = 1,
  CONSTRAIN_AXIS_Z = 2,
};

/**
 * Constraining modes.
 * Off / Scene orientation / Global (or Local if Scene orientation is Global).
 */
enum {
  CONSTRAIN_MODE_OFF = 0,
  CONSTRAIN_MODE_1 = 1,
  CONSTRAIN_MODE_2 = 2,
};
#endif /* USE_AXIS_CONSTRAINTS */

struct RulerItem;

struct RulerInfo {
  RulerItem *item_active;
  int flag;
  int snap_flag;
  int state;

#ifdef USE_AXIS_CONSTRAINTS
  short constrain_axis, constrain_mode;
#endif

  /* wm state */
  wmWindowManager *wm;
  wmWindow *win;
  ScrArea *area;
  ARegion *region; /* re-assigned every modal update */

  /* Track changes in state. */
  struct {
#ifndef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    bool do_snap;
#endif
    bool do_thickness;
  } drag_state_prev;

  struct {
    wmGizmo *gizmo;
    PropertyRNA *prop_prevpoint;
  } snap_data;
};

/* -------------------------------------------------------------------- */
/* Ruler Item (two or three points) */

struct RulerItem {
  wmGizmo gz;

  /** World-space coords, middle being optional. */
  float3x3 co;

  int flag;
  int raycast_dir; /* RULER_DIRECTION_* */
};

struct RulerInteraction {
  /* selected coord */
  char co_index; /* 0 -> 2 */
  float3 drag_start_co;
};

/* -------------------------------------------------------------------- */
/** \name Internal Ruler Utilities
 * \{ */

static RulerItem *ruler_item_add(wmGizmoGroup *gzgroup)
{
  /* could pass this as an arg */
  const wmGizmoType *gzt_ruler = WM_gizmotype_find("VIEW3D_GT_ruler_item", true);
  RulerItem *ruler_item = (RulerItem *)WM_gizmo_new_ptr(gzt_ruler, gzgroup, nullptr);
  WM_gizmo_set_flag(&ruler_item->gz, WM_GIZMO_DRAW_MODAL, true);
  return ruler_item;
}

static void ruler_item_remove(bContext *C, wmGizmoGroup *gzgroup, RulerItem *ruler_item)
{
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
  if (ruler_info->item_active == ruler_item) {
    ruler_info->item_active = nullptr;
  }
  WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, &ruler_item->gz, C);
}

static void ruler_item_as_string(
    RulerItem *ruler_item, UnitSettings *unit, char *numstr, size_t numstr_size, int prec)
{
  if (ruler_item->flag & RULERITEM_USE_ANGLE) {
    const float ruler_angle = angle_v3v3v3(
        ruler_item->co[0], ruler_item->co[1], ruler_item->co[2]);

    if (unit->system == USER_UNIT_NONE) {
      BLI_snprintf(numstr, numstr_size, "%.*f°", prec, RAD2DEGF(ruler_angle));
    }
    else {
      BKE_unit_value_as_string(
          numstr, numstr_size, double(ruler_angle), prec, B_UNIT_ROTATION, unit, false);
    }
  }
  else {
    const float ruler_len = len_v3v3(ruler_item->co[0], ruler_item->co[2]);

    if (unit->system == USER_UNIT_NONE) {
      BLI_snprintf(numstr, numstr_size, "%.*f", prec, ruler_len);
    }
    else {
      BKE_unit_value_as_string(numstr,
                               numstr_size,
                               double(ruler_len * unit->scale_length),
                               prec,
                               B_UNIT_LENGTH,
                               unit,
                               false);
    }
  }
}

static bool view3d_ruler_pick(wmGizmoGroup *gzgroup,
                              RulerItem *ruler_item,
                              const float2 mval,
                              int *r_co_index)
{
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
  ARegion *region = ruler_info->region;
  bool found = false;

  float dist_best = RULER_PICK_DIST_SQ;
  int co_index_best = -1;

  {
    float3x2 co_ss;
    float dist;
    int j;

    /* should these be checked? - ok for now not to */
    for (j = 0; j < 3; j++) {
      ED_view3d_project_float_global(region, ruler_item->co[j], co_ss[j], V3D_PROJ_TEST_NOP);
    }

    if (ruler_item->flag & RULERITEM_USE_ANGLE) {
      dist = min_ff(dist_squared_to_line_segment_v2(mval, co_ss[0], co_ss[1]),
                    dist_squared_to_line_segment_v2(mval, co_ss[1], co_ss[2]));
      if (dist < dist_best) {
        dist_best = dist;
        found = true;

        {
          const float3 dist_points = {
              blender::math::distance_squared(co_ss[0], mval),
              blender::math::distance_squared(co_ss[1], mval),
              blender::math::distance_squared(co_ss[2], mval),
          };
          if (min_fff(UNPACK3(dist_points)) < RULER_PICK_DIST_SQ) {
            co_index_best = min_axis_v3(dist_points);
          }
          else {
            co_index_best = -1;
          }
        }
      }
    }
    else {
      dist = dist_squared_to_line_segment_v2(mval, co_ss[0], co_ss[2]);
      if (dist < dist_best) {
        dist_best = dist;
        found = true;

        {
          const float2 dist_points = {
              blender::math::distance_squared(co_ss[0], mval),
              blender::math::distance_squared(co_ss[2], mval),
          };
          if (min_ff(UNPACK2(dist_points)) < RULER_PICK_DIST_SQ) {
            co_index_best = (dist_points[0] < dist_points[1]) ? 0 : 2;
          }
          else {
            co_index_best = -1;
          }
        }
      }
    }
  }

  *r_co_index = co_index_best;
  return found;
}

/**
 * Ensure the 'snap_context' is only cached while dragging,
 * needed since the user may toggle modes between tool use.
 */
static void ruler_state_set(RulerInfo *ruler_info, int state)
{
  if (state == ruler_info->state) {
    return;
  }

  if (state == RULER_STATE_NORMAL) {
    WM_gizmo_set_flag(ruler_info->snap_data.gizmo, WM_GIZMO_DRAW_VALUE, false);
  }
  else if (state == RULER_STATE_DRAG) {
    memset(&ruler_info->drag_state_prev, 0x0, sizeof(ruler_info->drag_state_prev));

    /* Force the snap cursor to appear even though it is not highlighted. */
    WM_gizmo_set_flag(ruler_info->snap_data.gizmo, WM_GIZMO_DRAW_VALUE, true);
  }
  else {
    BLI_assert(0);
  }

  ruler_info->state = state;
}

static void view3d_ruler_item_project(RulerInfo *ruler_info, float3 &r_co, const int xy[2])
{
  ED_view3d_win_to_3d_int(static_cast<const View3D *>(ruler_info->area->spacedata.first),
                          ruler_info->region,
                          r_co,
                          xy,
                          r_co);
}

/**
 * Use for mouse-move events.
 */
static bool view3d_ruler_item_mousemove(const bContext *C,
                                        Depsgraph *depsgraph,
                                        RulerInfo *ruler_info,
                                        RulerItem *ruler_item,
                                        const int mval[2],
                                        const bool do_thickness,
                                        const bool do_snap)
{
  wmGizmo *snap_gizmo = ruler_info->snap_data.gizmo;
  constexpr float eps_bias = 0.0002f;
  float dist_px = MVAL_MAX_PX_DIST * U.pixelsize; /* snap dist */

  if (ruler_item) {
    RulerInteraction *inter = static_cast<RulerInteraction *>(ruler_item->gz.interaction_data);
    float3 &co = ruler_item->co[inter->co_index];
    /* restore the initial depth */
    co = inter->drag_start_co;
    view3d_ruler_item_project(ruler_info, co, mval);
    if (do_thickness && inter->co_index != 1) {
      Scene *scene = DEG_get_input_scene(depsgraph);
      View3D *v3d = static_cast<View3D *>(ruler_info->area->spacedata.first);
      SnapObjectContext *snap_context = ED_gizmotypes_snap_3d_context_ensure(scene, snap_gizmo);
      const float2 mval_fl = {float(mval[0]), float(mval[1])};
      float3 ray_normal;
      float3 ray_start;
      float3 &co_other = ruler_item->co[inter->co_index == 0 ? 2 : 0];

      SnapObjectParams snap_object_params{};
      snap_object_params.snap_target_select = SCE_SNAP_TARGET_ALL;
      snap_object_params.edit_mode_type = SNAP_GEOM_CAGE;

      eSnapMode hit = ED_transform_snap_object_project_view3d(snap_context,
                                                              depsgraph,
                                                              ruler_info->region,
                                                              v3d,
                                                              SCE_SNAP_MODE_FACE,
                                                              &snap_object_params,
                                                              nullptr,
                                                              mval_fl,
                                                              nullptr,
                                                              &dist_px,
                                                              co,
                                                              ray_normal);
      if (hit) {
        /* add some bias */
        ray_start = co - ray_normal * eps_bias;
        ED_transform_snap_object_project_ray(snap_context,
                                             depsgraph,
                                             v3d,
                                             &snap_object_params,
                                             ray_start,
                                             -ray_normal,
                                             nullptr,
                                             co_other,
                                             nullptr);
      }
    }
    else {
      View3D *v3d = static_cast<View3D *>(ruler_info->area->spacedata.first);
      if (do_snap) {
        float3 *prev_point = nullptr;
        BLI_assert(ED_gizmotypes_snap_3d_is_enabled(snap_gizmo));

        if (inter->co_index != 1) {
          if (ruler_item->flag & RULERITEM_USE_ANGLE) {
            prev_point = &ruler_item->co[1];
          }
          else if (inter->co_index == 0) {
            prev_point = &ruler_item->co[2];
          }
          else {
            prev_point = &ruler_item->co[0];
          }
        }
        if (prev_point != nullptr) {
          RNA_property_float_set_array(
              snap_gizmo->ptr, ruler_info->snap_data.prop_prevpoint, *prev_point);
        }

        ED_gizmotypes_snap_3d_data_get(C, snap_gizmo, co, nullptr, nullptr, nullptr);
      }

#ifdef USE_AXIS_CONSTRAINTS
      if (!(ruler_item->flag & RULERITEM_USE_ANGLE) &&
          ruler_info->constrain_mode != CONSTRAIN_MODE_OFF) {

        Scene *scene = DEG_get_input_scene(depsgraph);
        ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
        RegionView3D *rv3d = static_cast<RegionView3D *>(ruler_info->region->regiondata);
        BKE_view_layer_synced_ensure(scene, view_layer);
        Object *ob = BKE_view_layer_active_object_get(view_layer);
        Object *obedit = OBEDIT_FROM_OBACT(ob);

        short orient_index = BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);

        if (ruler_info->constrain_mode == CONSTRAIN_MODE_2) {
          orient_index = (orient_index == V3D_ORIENT_GLOBAL) ? V3D_ORIENT_LOCAL :
                                                               V3D_ORIENT_GLOBAL;
        }

        const int pivot_point = scene->toolsettings->transform_pivot_point;
        float3x3 mat;

        ED_transform_calc_orientation_from_type_ex(
            scene, view_layer, v3d, rv3d, ob, obedit, orient_index, pivot_point, mat.ptr());

        ruler_item->co = blender::math::invert(mat) * ruler_item->co;

        /* Loop through the axes and constrain the dragged point to the current constrained axis.
         */
        for (int i = 0; i <= 2; i++) {
          if (ruler_info->constrain_axis != i) {
            ruler_item->co[inter->co_index][i] = ruler_item->co[(inter->co_index == 0) ? 2 : 0][i];
          }
        }
        ruler_item->co = mat * ruler_item->co;
      }
#endif
    }
    return true;
  }
  return false;
}

/**
 * When the gizmo-group has been created immediately before running an operator
 * to manipulate rulers, it's possible the new gizmo-group has not yet been initialized.
 * in 3.0 this happened because left-click drag would both select and add a new ruler,
 * significantly increasing the likelihood of this happening.
 * Workaround this crash by checking the gizmo's custom-data has not been cleared.
 * The key-map has also been modified not to trigger this bug, see #95591.
 */
static bool gizmo_ruler_check_for_operator(const wmGizmoGroup *gzgroup)
{
  return gzgroup->customdata != nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler/Grease Pencil Conversion
 * \{ */

/* Helper: Find the layer created as ruler. */
static bGPDlayer *view3d_ruler_layer_get(bGPdata *gpd)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_IS_RULER) {
      return gpl;
    }
  }
  return nullptr;
}

static RulerItem *gzgroup_ruler_item_first_get(wmGizmoGroup *gzgroup)
{
#ifndef NDEBUG
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
  BLI_assert(gzgroup->gizmos.first == ruler_info->snap_data.gizmo);
#endif
  return (RulerItem *)((wmGizmo *)gzgroup->gizmos.first)->next;
}

#define RULER_ID "RulerData3D"
static bool view3d_ruler_to_gpencil(bContext *C, wmGizmoGroup *gzgroup)
{
  // RulerInfo *ruler_info = gzgroup->customdata;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  bGPdata *gpd;
  bGPDlayer *gpl;
  bGPDframe *gpf;
  bGPDstroke *gps;
  RulerItem *ruler_item;
  const char *ruler_name = RULER_ID;
  bool changed = false;

  if (scene->gpd == nullptr) {
    scene->gpd = BKE_gpencil_data_addnew(bmain, "Annotations");
  }
  gpd = scene->gpd;

  gpl = view3d_ruler_layer_get(gpd);
  if (gpl == nullptr) {
    gpl = BKE_gpencil_layer_addnew(gpd, ruler_name, false, false);
    copy_v4_v4(gpl->color, U.gpencil_new_layer_col);
    gpl->thickness = 1;
    gpl->flag |= GP_LAYER_HIDE | GP_LAYER_IS_RULER;
  }

  gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_ADD_NEW);
  BKE_gpencil_free_strokes(gpf);

  for (ruler_item = gzgroup_ruler_item_first_get(gzgroup); ruler_item;
       ruler_item = (RulerItem *)ruler_item->gz.next)
  {
    bGPDspoint *pt;
    int j;

    /* allocate memory for a new stroke */
    gps = (bGPDstroke *)MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
    if (ruler_item->flag & RULERITEM_USE_ANGLE) {
      gps->totpoints = 3;
      pt = gps->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * gps->totpoints,
                                                   "gp_stroke_points");
      for (j = 0; j < 3; j++) {
        copy_v3_v3(&pt->x, ruler_item->co[j]);
        pt->pressure = 1.0f;
        pt->strength = 1.0f;
        pt++;
      }
    }
    else {
      gps->totpoints = 2;
      pt = gps->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * gps->totpoints,
                                                   "gp_stroke_points");
      for (j = 0; j < 3; j += 2) {
        copy_v3_v3(&pt->x, ruler_item->co[j]);
        pt->pressure = 1.0f;
        pt->strength = 1.0f;
        pt++;
      }
    }
    gps->flag = GP_STROKE_3DSPACE;
    gps->thickness = 3;
    gps->hardeness = 1.0f;
    gps->fill_opacity_fac = 1.0f;
    copy_v2_fl(gps->aspect_ratio, 1.0f);
    gps->uv_scale = 1.0f;

    BLI_addtail(&gpf->strokes, gps);
    changed = true;
  }

  return changed;
}

static bool view3d_ruler_from_gpencil(const bContext *C, wmGizmoGroup *gzgroup)
{
  Scene *scene = CTX_data_scene(C);
  bool changed = false;

  if (scene->gpd) {
    bGPDlayer *gpl;
    gpl = view3d_ruler_layer_get(scene->gpd);
    if (gpl) {
      bGPDframe *gpf;
      gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);
      if (gpf) {
        bGPDstroke *gps;
        for (gps = static_cast<bGPDstroke *>(gpf->strokes.first); gps; gps = gps->next) {
          bGPDspoint *pt = gps->points;
          int j;
          RulerItem *ruler_item = nullptr;
          if (gps->totpoints == 3) {
            ruler_item = ruler_item_add(gzgroup);
            for (j = 0; j < 3; j++) {
              copy_v3_v3(ruler_item->co[j], &pt->x);
              pt++;
            }
            ruler_item->flag |= RULERITEM_USE_ANGLE;
            changed = true;
          }
          else if (gps->totpoints == 2) {
            ruler_item = ruler_item_add(gzgroup);
            for (j = 0; j < 3; j += 2) {
              copy_v3_v3(ruler_item->co[j], &pt->x);
              pt++;
            }
            changed = true;
          }
        }
      }
    }
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler Item Gizmo Type
 * \{ */

static void gizmo_ruler_draw(const bContext *C, wmGizmo *gz)
{
  Scene *scene = CTX_data_scene(C);
  UnitSettings *unit = &scene->unit;
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gz->parent_gzgroup->customdata);
  RulerItem *ruler_item = (RulerItem *)gz;
  ARegion *region = ruler_info->region;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const float cap_size = 4.0f * UI_SCALE_FAC;
  const float bg_margin = 4.0f * UI_SCALE_FAC;
  const float arc_size = 64.0f * UI_SCALE_FAC;
  constexpr int arc_steps = 24;
  const float4 color_act = {1.0f, 1.0f, 1.0f, 1.0f};
  const float4 color_base = {0.0f, 0.0f, 0.0f, 1.0f};
  uchar color_text[3];
  uchar color_wire[3];
  float4 color_back = {1.0f, 1.0f, 1.0f, 0.5f};

  /* Pixel Space. */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_identity_set();
  wmOrtho2_region_pixelspace(region);

  /* anti-aliased lines for more consistent appearance */
  GPU_line_smooth(true);
  GPU_line_width(1.0f);

  BLF_enable(blf_mono_font, BLF_ROTATION);
  BLF_size(blf_mono_font, 14.0f * UI_SCALE_FAC);
  BLF_rotation(blf_mono_font, 0.0f);

  UI_GetThemeColor3ubv(TH_TEXT, color_text);
  UI_GetThemeColor3ubv(TH_WIRE, color_wire);

  /* Avoid white on white text. (TODO: Fix by using theme). */
  if (int(color_text[0]) + int(color_text[1]) + int(color_text[2]) > 127 * 3 * 0.6f) {
    copy_v3_fl(color_back, 0.0f);
  }

  const bool is_act = (ruler_info->item_active == ruler_item);
  float2 dir_ruler;
  float3x2 co_ss;
  bool proj_ok[3];
  int j;

  /* Check if each corner is behind the near plane. If it is, we do not draw certain lines. */
  for (j = 0; j < 3; j++) {
    eV3DProjStatus status = ED_view3d_project_float_global(
        region, ruler_item->co[j], co_ss[j], V3D_PROJ_TEST_CLIP_NEAR);
    proj_ok[j] = (status == V3D_PROJ_RET_OK);
  }

  /* 3d drawing. */

  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);

  GPU_blend(GPU_BLEND_ALPHA);

  const uint shdr_pos_3d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  if (ruler_item->flag & RULERITEM_USE_ANGLE) {
    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float4 viewport_size(0.0f);
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniform1i("colors_len", 2); /* "advanced" mode */
    const float4 &col = is_act ? color_act : color_base;
    immUniform4f("color", 0.67f, 0.67f, 0.67f, 1.0f);
    immUniform4fv("color2", col);
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);

    immBegin(GPU_PRIM_LINE_STRIP, 3);

    immVertex3fv(shdr_pos_3d, ruler_item->co[0]);
    immVertex3fv(shdr_pos_3d, ruler_item->co[1]);
    immVertex3fv(shdr_pos_3d, ruler_item->co[2]);

    immEnd();

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* arc */
    {
      float3 dir_tmp;
      float3 ar_coord;

      float3 dir_a;
      float3 dir_b;
      float4 quat;
      float3 axis;
      float angle;
      const float px_scale = (ED_view3d_pixel_size_no_ui_scale(rv3d, ruler_item->co[1]) *
                              min_fff(arc_size,
                                      blender::math::distance(co_ss[0], co_ss[1]) / 2.0f,
                                      blender::math::distance(co_ss[2], co_ss[1]) / 2.0f));

      dir_a = blender::math::normalize(ruler_item->co[0] - ruler_item->co[1]);
      dir_b = blender::math::normalize(ruler_item->co[2] - ruler_item->co[1]);
      axis = blender::math::cross(dir_a, dir_b);
      angle = angle_normalized_v3v3(dir_a, dir_b);

      axis_angle_to_quat(quat, axis, angle / arc_steps);

      dir_tmp = dir_a;

      immUniformColor3ubv(color_wire);

      immBegin(GPU_PRIM_LINE_STRIP, arc_steps + 1);

      for (j = 0; j <= arc_steps; j++) {
        ar_coord = ruler_item->co[1] + dir_tmp * px_scale;
        mul_qt_v3(quat, dir_tmp);

        immVertex3fv(shdr_pos_3d, ar_coord);
      }

      immEnd();
    }

    immUnbindProgram();
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniform1i("colors_len", 2); /* "advanced" mode */
    const float *col = is_act ? color_act : color_base;
    immUniform4f("color", 0.67f, 0.67f, 0.67f, 1.0f);
    immUniform4fv("color2", col);
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);

    immBegin(GPU_PRIM_LINES, 2);

    immVertex3fv(shdr_pos_3d, ruler_item->co[0]);
    immVertex3fv(shdr_pos_3d, ruler_item->co[2]);

    immEnd();

    immUnbindProgram();
  }

  /* 2d drawing. */

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  const uint shdr_pos_2d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (ruler_item->flag & RULERITEM_USE_ANGLE) {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    /* capping */
    {
      float2 cap;

      dir_ruler = co_ss[0] - co_ss[1];
      float2 rot_90_vec_a = blender::math::normalize(float2{-dir_ruler[1], dir_ruler[0]});

      dir_ruler = co_ss[1] - co_ss[2];
      float2 rot_90_vec_b = blender::math::normalize(float2{-dir_ruler[1], dir_ruler[0]});

      GPU_blend(GPU_BLEND_ALPHA);

      if (proj_ok[1] && is_act && (ruler_item->flag & RULERITEM_USE_ANGLE_ACTIVE)) {
        GPU_line_width(3.0f);
        immUniformColor3fv(color_act);
        immBegin(GPU_PRIM_LINES, 4);
        /* angle vertex */
        immVertex2f(shdr_pos_2d, co_ss[1][0] - cap_size, co_ss[1][1] - cap_size);
        immVertex2f(shdr_pos_2d, co_ss[1][0] + cap_size, co_ss[1][1] + cap_size);
        immVertex2f(shdr_pos_2d, co_ss[1][0] - cap_size, co_ss[1][1] + cap_size);
        immVertex2f(shdr_pos_2d, co_ss[1][0] + cap_size, co_ss[1][1] - cap_size);

        immEnd();
        GPU_line_width(1.0f);
      }

      immUniformColor3ubv(color_wire);

      if (proj_ok[0] || proj_ok[2] || proj_ok[1]) {
        immBegin(GPU_PRIM_LINES, proj_ok[0] * 2 + proj_ok[2] * 2 + proj_ok[1] * 4);

        if (proj_ok[0]) {
          cap = co_ss[0] + rot_90_vec_a * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
          cap = co_ss[0] - rot_90_vec_a * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
        }

        if (proj_ok[2]) {
          cap = co_ss[2] + rot_90_vec_b * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
          cap = co_ss[2] - rot_90_vec_b * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
        }

        /* angle vertex */
        if (proj_ok[1]) {
          immVertex2f(shdr_pos_2d, co_ss[1][0] - cap_size, co_ss[1][1] - cap_size);
          immVertex2f(shdr_pos_2d, co_ss[1][0] + cap_size, co_ss[1][1] + cap_size);
          immVertex2f(shdr_pos_2d, co_ss[1][0] - cap_size, co_ss[1][1] + cap_size);
          immVertex2f(shdr_pos_2d, co_ss[1][0] + cap_size, co_ss[1][1] - cap_size);
        }

        immEnd();
      }

      GPU_blend(GPU_BLEND_NONE);
    }

    /* text */
    char numstr[256];
    float2 numstr_size;
    float posit[2];
    const int prec = 2; /* XXX, todo, make optional */

    ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

    BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

    /* Center text. */
    posit[0] = co_ss[1][0] - (numstr_size[0] / 2.0f);
    posit[1] = co_ss[1][1] - (numstr_size[1] / 2.0f);

    /* Adjust text position to help readability. */
    sub_v2_v2v2(dir_ruler, co_ss[0], co_ss[1]);
    float rot_90_vec[2] = {-dir_ruler[1], dir_ruler[0]};
    normalize_v2(rot_90_vec);
    posit[1] += rot_90_vec[0] * numstr_size[1];
    posit[0] += (rot_90_vec[1] < 0) ? numstr_size[0] : -numstr_size[0];

    /* draw text (bg) */
    if (proj_ok[1]) {
      immUniformColor4fv(color_back);
      GPU_blend(GPU_BLEND_ALPHA);
      immRectf(shdr_pos_2d,
               posit[0] - bg_margin,
               posit[1] - bg_margin,
               posit[0] + bg_margin + numstr_size[0],
               posit[1] + bg_margin + numstr_size[1]);
      GPU_blend(GPU_BLEND_NONE);
    }

    immUnbindProgram();

    /* draw text */
    if (proj_ok[1]) {
      BLF_color3ubv(blf_mono_font, color_text);
      BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
      BLF_rotation(blf_mono_font, 0.0f);
      BLF_draw(blf_mono_font, numstr, sizeof(numstr));
    }
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    dir_ruler = co_ss[0] - co_ss[2];
    float2 rot_90_vec = blender::math::normalize(float2{-dir_ruler[1], dir_ruler[0]});

    /* capping */
    {
      float2 cap;

      GPU_blend(GPU_BLEND_ALPHA);

      immUniformColor3ubv(color_wire);

      if (proj_ok[0] || proj_ok[2]) {
        immBegin(GPU_PRIM_LINES, proj_ok[0] * 2 + proj_ok[2] * 2);

        if (proj_ok[0]) {
          cap = co_ss[0] + rot_90_vec * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
          cap = co_ss[0] - rot_90_vec * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
        }

        if (proj_ok[2]) {
          cap = co_ss[2] + rot_90_vec * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
          cap = co_ss[2] - rot_90_vec * cap_size;
          immVertex2fv(shdr_pos_2d, cap);
        }

        immEnd();
      }

      GPU_blend(GPU_BLEND_NONE);
    }

    /* text */
    char numstr[256];
    float2 numstr_size;
    const int prec = 6; /* XXX, todo, make optional */
    float2 posit;

    ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

    BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

    posit = (co_ss[0] + co_ss[2]) / 2.0f;

    /* center text */
    posit -= numstr_size / 2.0f;

    /* Adjust text position if this helps readability. */

    const float len = len_v2v2(co_ss[0], co_ss[2]);

    if ((len < (numstr_size[1] * 2.5f)) ||
        ((len < (numstr_size[0] + bg_margin + bg_margin)) && (fabs(rot_90_vec[0]) < 0.5f)))
    {
      /* Super short, or quite short and also shallow angle. Position below line. */
      posit[1] = MIN2(co_ss[0][1], co_ss[2][1]) - numstr_size[1] - bg_margin - bg_margin;
    }
    else if (fabs(rot_90_vec[0]) < 0.2f) {
      /* Very shallow angle. Shift down by text height. */
      posit[1] -= numstr_size[1];
    }

    /* draw text (bg) */
    if (proj_ok[0] && proj_ok[2]) {
      immUniformColor4fv(color_back);
      GPU_blend(GPU_BLEND_ALPHA);
      immRectf(shdr_pos_2d,
               posit[0] - bg_margin,
               posit[1] - bg_margin,
               posit[0] + bg_margin + numstr_size[0],
               posit[1] + bg_margin + numstr_size[1]);
      GPU_blend(GPU_BLEND_NONE);
    }

    immUnbindProgram();

    /* draw text */
    if (proj_ok[0] && proj_ok[2]) {
      BLF_color3ubv(blf_mono_font, color_text);
      BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
      BLF_draw(blf_mono_font, numstr, sizeof(numstr));
    }
  }

  GPU_line_smooth(false);

  BLF_disable(blf_mono_font, BLF_ROTATION);

  GPU_matrix_pop();
  GPU_matrix_pop_projection();
}

static int gizmo_ruler_test_select(bContext *, wmGizmo *gz, const int mval[2])
{
  RulerItem *ruler_item_pick = (RulerItem *)gz;
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  int co_index;

  /* select and drag */
  if (view3d_ruler_pick(gz->parent_gzgroup, ruler_item_pick, mval_fl, &co_index)) {
    if (co_index == -1) {
      if ((ruler_item_pick->flag & RULERITEM_USE_ANGLE) == 0) {
        return PART_LINE;
      }
    }
    else {
      return co_index;
    }
  }
  return -1;
}

static int gizmo_ruler_modal(bContext *C,
                             wmGizmo *gz,
                             const wmEvent *event,
                             eWM_GizmoFlagTweak tweak_flag)
{
  bool do_draw = false;
  int exit_code = OPERATOR_RUNNING_MODAL;
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gz->parent_gzgroup->customdata);
  RulerItem *ruler_item = (RulerItem *)gz;
  ARegion *region = CTX_wm_region(C);
  bool do_cursor_update = (event->val == KM_RELEASE) || (event->type == MOUSEMOVE);

  ruler_info->region = region;

#ifdef USE_AXIS_CONSTRAINTS
  if ((event->val == KM_PRESS) && ELEM(event->type, EVT_XKEY, EVT_YKEY, EVT_ZKEY)) {
    /* Go to Mode 1 if a new axis is selected. */
    if (event->type == EVT_XKEY && ruler_info->constrain_axis != CONSTRAIN_AXIS_X) {
      ruler_info->constrain_axis = CONSTRAIN_AXIS_X;
      ruler_info->constrain_mode = CONSTRAIN_MODE_1;
    }
    else if (event->type == EVT_YKEY && ruler_info->constrain_axis != CONSTRAIN_AXIS_Y) {
      ruler_info->constrain_axis = CONSTRAIN_AXIS_Y;
      ruler_info->constrain_mode = CONSTRAIN_MODE_1;
    }
    else if (event->type == EVT_ZKEY && ruler_info->constrain_axis != CONSTRAIN_AXIS_Z) {
      ruler_info->constrain_axis = CONSTRAIN_AXIS_Z;
      ruler_info->constrain_mode = CONSTRAIN_MODE_1;
    }
    else {
      /* Cycle to the next mode if the same key is pressed again. */
      if (ruler_info->constrain_mode != CONSTRAIN_MODE_2) {
        ruler_info->constrain_mode++;
      }
      else {
        ruler_info->constrain_mode = CONSTRAIN_MODE_OFF;
        ruler_info->constrain_axis = CONSTRAIN_AXIS_NONE;
      }
    }
    do_cursor_update = true;
  }
#endif

#ifndef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  const bool do_snap = !(tweak_flag & WM_GIZMO_TWEAK_SNAP);
#else
  /* Ensure snap is up to date. */
  ED_gizmotypes_snap_3d_data_get(
      C, ruler_info->snap_data.gizmo, nullptr, nullptr, nullptr, nullptr);
  const bool do_snap = ED_gizmotypes_snap_3d_is_enabled(ruler_info->snap_data.gizmo);
#endif

  const bool do_thickness = tweak_flag & WM_GIZMO_TWEAK_PRECISE;
  if (ruler_info->drag_state_prev.do_thickness != do_thickness) {
    do_cursor_update = true;
  }

  if (do_cursor_update) {
    if (ruler_info->state == RULER_STATE_DRAG) {
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      if (view3d_ruler_item_mousemove(
              C, depsgraph, ruler_info, ruler_item, event->mval, do_thickness, do_snap))
      {
        do_draw = true;
      }
    }
  }

  ruler_info->drag_state_prev.do_thickness = do_thickness;

  if (do_draw) {
    ED_region_tag_redraw_editor_overlays(region);
  }
  return exit_code;
}

static int gizmo_ruler_invoke(bContext *C, wmGizmo *gz, const wmEvent *event)
{
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
  RulerItem *ruler_item_pick = (RulerItem *)gz;
  RulerInteraction *inter = (RulerInteraction *)MEM_callocN(sizeof(RulerInteraction), __func__);
  gz->interaction_data = inter;

  ARegion *region = ruler_info->region;

  float mval_fl[2];
  WM_event_drag_start_mval_fl(event, region, mval_fl);

#ifdef USE_AXIS_CONSTRAINTS
  ruler_info->constrain_axis = CONSTRAIN_AXIS_NONE;
  ruler_info->constrain_mode = CONSTRAIN_MODE_OFF;
#endif

  /* select and drag */
  if (gz->highlight_part == PART_LINE) {
    if ((ruler_item_pick->flag & RULERITEM_USE_ANGLE) == 0) {
      /* Add Center Point */
      ruler_item_pick->flag |= RULERITEM_USE_ANGLE;
      inter->co_index = 1;
      ruler_state_set(ruler_info, RULER_STATE_DRAG);

      /* find the factor */
      {
        float2x2 co_ss;
        float fac;

        ED_view3d_project_float_global(
            region, ruler_item_pick->co[0], co_ss[0], V3D_PROJ_TEST_NOP);
        ED_view3d_project_float_global(
            region, ruler_item_pick->co[2], co_ss[1], V3D_PROJ_TEST_NOP);

        fac = line_point_factor_v2(mval_fl, co_ss[0], co_ss[1]);
        CLAMP(fac, 0.0f, 1.0f);

        ruler_item_pick->co[1] = blender::math::interpolate(
            ruler_item_pick->co[0], ruler_item_pick->co[2], fac);
      }

      /* update the new location */
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      view3d_ruler_item_mousemove(
          C, depsgraph, ruler_info, ruler_item_pick, event->mval, false, false);
    }
  }
  else {
    inter->co_index = gz->highlight_part;
    ruler_state_set(ruler_info, RULER_STATE_DRAG);

    /* store the initial depth */
    inter->drag_start_co = ruler_item_pick->co[inter->co_index];
  }

  if (inter->co_index == 1) {
    ruler_item_pick->flag |= RULERITEM_USE_ANGLE_ACTIVE;
  }
  else {
    ruler_item_pick->flag &= ~RULERITEM_USE_ANGLE_ACTIVE;
  }

  {
    /* Set Snap prev point. */
    float3 *prev_point;
    if (ruler_item_pick->flag & RULERITEM_USE_ANGLE) {
      prev_point = (inter->co_index != 1) ? &ruler_item_pick->co[1] : nullptr;
    }
    else if (inter->co_index == 0) {
      prev_point = &ruler_item_pick->co[2];
    }
    else {
      prev_point = &ruler_item_pick->co[0];
    }

    if (prev_point) {
      RNA_property_float_set_array(
          ruler_info->snap_data.gizmo->ptr, ruler_info->snap_data.prop_prevpoint, *prev_point);
    }
    else {
      RNA_property_unset(ruler_info->snap_data.gizmo->ptr, ruler_info->snap_data.prop_prevpoint);
    }
  }

  ruler_info->item_active = ruler_item_pick;

  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_ruler_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);

  if (!cancel) {
    if (ruler_info->state == RULER_STATE_DRAG) {
      RNA_property_unset(ruler_info->snap_data.gizmo->ptr, ruler_info->snap_data.prop_prevpoint);
      ruler_state_set(ruler_info, RULER_STATE_NORMAL);
    }
    /* We could convert only the current gizmo, for now just re-generate. */
    view3d_ruler_to_gpencil(C, gzgroup);
  }

  MEM_SAFE_FREE(gz->interaction_data);

  ruler_state_set(ruler_info, RULER_STATE_NORMAL);
}

static int gizmo_ruler_cursor_get(wmGizmo *gz)
{
  if (gz->highlight_part == PART_LINE) {
    return WM_CURSOR_CROSS;
  }
  return WM_CURSOR_NSEW_SCROLL;
}

void VIEW3D_GT_ruler_item(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "VIEW3D_GT_ruler_item";

  /* api callbacks */
  gzt->draw = gizmo_ruler_draw;
  gzt->test_select = gizmo_ruler_test_select;
  gzt->modal = gizmo_ruler_modal;
  gzt->invoke = gizmo_ruler_invoke;
  gzt->exit = gizmo_ruler_exit;
  gzt->cursor_get = gizmo_ruler_cursor_get;

  gzt->struct_size = sizeof(RulerItem);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler Gizmo Group
 * \{ */

static void WIDGETGROUP_ruler_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  RulerInfo *ruler_info = (RulerInfo *)MEM_callocN(sizeof(RulerInfo), __func__);

  wmGizmo *gizmo;
  {
    /* The gizmo snap has to be the first gizmo. */
    const wmGizmoType *gzt_snap;
    gzt_snap = WM_gizmotype_find("GIZMO_GT_snap_3d", true);
    gizmo = WM_gizmo_new_ptr(gzt_snap, gzgroup, nullptr);

    ED_gizmotypes_snap_3d_flag_set(gizmo, V3D_SNAPCURSOR_SNAP_EDIT_GEOM_CAGE);
    WM_gizmo_set_color(gizmo, blender::float4(1.0f));

    wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_ruler_add", true);
    WM_gizmo_operator_set(gizmo, 0, ot, nullptr);
  }

  if (view3d_ruler_from_gpencil(C, gzgroup)) {
    /* nop */
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  ruler_info->wm = wm;
  ruler_info->win = win;
  ruler_info->area = area;
  ruler_info->region = region;
  ruler_info->snap_data.gizmo = gizmo;
  ruler_info->snap_data.prop_prevpoint = RNA_struct_find_property(gizmo->ptr, "prev_point");

  gzgroup->customdata = ruler_info;
}

void VIEW3D_GGT_ruler(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Ruler Widgets";
  gzgt->idname = view3d_gzgt_ruler_id;

  gzgt->flag |= WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_SCALE | WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_ruler_setup;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Ruler Operator
 * \{ */

static bool view3d_ruler_poll(bContext *C)
{
  bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
  if ((tref_rt == nullptr) || !STREQ(view3d_gzgt_ruler_id, tref_rt->gizmo_group) ||
      CTX_wm_region_view3d(C) == nullptr)
  {
    return false;
  }
  return true;
}

static int view3d_ruler_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);

  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_TOOL)) {
    BKE_report(op->reports, RPT_WARNING, "Gizmos hidden in this view");
    return OPERATOR_CANCELLED;
  }

  wmGizmoMap *gzmap = region->gizmo_map;
  wmGizmoGroup *gzgroup = WM_gizmomap_group_find(gzmap, view3d_gzgt_ruler_id);

  if (!gizmo_ruler_check_for_operator(gzgroup)) {
    return OPERATOR_CANCELLED;
  }

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);

  /* Create new line */
  RulerItem *ruler_item;
  ruler_item = ruler_item_add(gzgroup);

  /* This is a little weak, but there is no real good way to tweak directly. */
  WM_gizmo_highlight_set(gzmap, &ruler_item->gz);
  if (WM_operator_name_call(
          C, "GIZMOGROUP_OT_gizmo_tweak", WM_OP_INVOKE_REGION_WIN, nullptr, event) ==
      OPERATOR_RUNNING_MODAL)
  {
    RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
    RulerInteraction *inter = static_cast<RulerInteraction *>(ruler_item->gz.interaction_data);
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    inter->co_index = 0;

#ifndef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    /* Snap the first point added, not essential but handy. */
    const bool do_snap = true;
#else
    const bool do_snap = ED_gizmotypes_snap_3d_is_enabled(ruler_info->snap_data.gizmo);
#endif

    view3d_ruler_item_mousemove(C, depsgraph, ruler_info, ruler_item, mval, false, do_snap);
    copy_v3_v3(inter->drag_start_co, ruler_item->co[inter->co_index]);
    RNA_property_float_set_array(ruler_info->snap_data.gizmo->ptr,
                                 ruler_info->snap_data.prop_prevpoint,
                                 inter->drag_start_co);

    copy_v3_v3(ruler_item->co[2], ruler_item->co[0]);
    ruler_item->gz.highlight_part = inter->co_index = 2;
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_ruler_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Ruler Add";
  ot->idname = "VIEW3D_OT_ruler_add";
  ot->description = "Add ruler";

  ot->invoke = view3d_ruler_add_invoke;
  ot->poll = view3d_ruler_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Ruler Operator
 * \{ */

static int view3d_ruler_remove_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);

  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_TOOL)) {
    BKE_report(op->reports, RPT_WARNING, "Gizmos hidden in this view");
    return OPERATOR_CANCELLED;
  }

  wmGizmoMap *gzmap = region->gizmo_map;
  wmGizmoGroup *gzgroup = WM_gizmomap_group_find(gzmap, view3d_gzgt_ruler_id);
  if (gzgroup) {
    if (!gizmo_ruler_check_for_operator(gzgroup)) {
      return OPERATOR_CANCELLED;
    }
    RulerInfo *ruler_info = static_cast<RulerInfo *>(gzgroup->customdata);
    if (ruler_info->item_active) {
      RulerItem *ruler_item = ruler_info->item_active;
      if ((ruler_item->flag & RULERITEM_USE_ANGLE) &&
          (ruler_item->flag & RULERITEM_USE_ANGLE_ACTIVE)) {
        ruler_item->flag &= ~(RULERITEM_USE_ANGLE | RULERITEM_USE_ANGLE_ACTIVE);
      }
      else {
        ruler_item_remove(C, gzgroup, ruler_item);
      }

      /* Update the annotation layer. */
      view3d_ruler_to_gpencil(C, gzgroup);

      ED_region_tag_redraw_editor_overlays(region);
      return OPERATOR_FINISHED;
    }
  }
  return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_ruler_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Ruler Remove";
  ot->idname = "VIEW3D_OT_ruler_remove";

  ot->invoke = view3d_ruler_remove_invoke;
  ot->poll = view3d_ruler_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */
