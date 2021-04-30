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

/** \file snap3d_gizmo.c
 *  \ingroup edgizmolib
 *
 * \name Snap Gizmo
 *
 * 3D Gizmo
 *
 * \brief Snap gizmo which exposes the location, normal and index in the props.
 */

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "ED_gizmo_library.h"
#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "UI_resources.h" /* icons */

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

typedef struct SnapGizmo3D {
  wmGizmo gizmo;

  /* We could have other snap contexts, for now only support 3D view. */
  SnapObjectContext *snap_context_v3d;

  /* Copy of the parameters of the last event state in order to detect updates. */
  struct {
    int x;
    int y;
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    short shift, ctrl, alt, oskey;
#endif
  } last_eventstate;

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  wmKeyMap *keymap;
  int snap_on;
  bool invert_snap;
#endif

  /* Setup. */
  eSnapGizmo flag;
  float *prevpoint;
  float prevpoint_stack[3];
  short snap_elem_force;

  /* Return values. */
  short snap_elem;
  float loc[3];
  float nor[3];
  int elem_index[3];

  /** Enabled when snap is activated, even if it didn't find anything. */
  bool is_enabled;
} SnapGizmo3D;

/* Checks if the current event is different from the one captured in the last update. */
static bool eventstate_has_changed(SnapGizmo3D *snap_gizmo, const wmWindowManager *wm)
{
  if (wm && wm->winactive) {
    const wmEvent *event = wm->winactive->eventstate;
    if ((event->x != snap_gizmo->last_eventstate.x) ||
        (event->y != snap_gizmo->last_eventstate.y)) {
      return true;
    }
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    if (!(snap_gizmo->flag & ED_SNAPGIZMO_TOGGLE_ALWAYS_TRUE)) {
      if ((event->ctrl != snap_gizmo->last_eventstate.ctrl) ||
          (event->shift != snap_gizmo->last_eventstate.shift) ||
          (event->alt != snap_gizmo->last_eventstate.alt) ||
          (event->oskey != snap_gizmo->last_eventstate.oskey)) {
        return true;
      }
    }
#endif
  }
  return false;
}

/* Copies the current eventstate. */
static void eventstate_save_xy(SnapGizmo3D *snap_gizmo, const wmWindowManager *wm)
{
  if (wm && wm->winactive) {
    const wmEvent *event = wm->winactive->eventstate;
    snap_gizmo->last_eventstate.x = event->x;
    snap_gizmo->last_eventstate.y = event->y;
  }
}

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
static bool invert_snap(SnapGizmo3D *snap_gizmo, const wmWindowManager *wm)
{
  if (!wm || !wm->winactive) {
    return false;
  }

  const wmEvent *event = wm->winactive->eventstate;
  if ((event->ctrl == snap_gizmo->last_eventstate.ctrl) &&
      (event->shift == snap_gizmo->last_eventstate.shift) &&
      (event->alt == snap_gizmo->last_eventstate.alt) &&
      (event->oskey == snap_gizmo->last_eventstate.oskey)) {
    /* Nothing has changed. */
    return snap_gizmo->invert_snap;
  }

  /* Save new eventstate. */
  snap_gizmo->last_eventstate.ctrl = event->ctrl;
  snap_gizmo->last_eventstate.shift = event->shift;
  snap_gizmo->last_eventstate.alt = event->alt;
  snap_gizmo->last_eventstate.oskey = event->oskey;

  const int snap_on = snap_gizmo->snap_on;

  wmKeyMap *keymap = WM_keymap_active(wm, snap_gizmo->keymap);
  for (wmKeyMapItem *kmi = keymap->items.first; kmi; kmi = kmi->next) {
    if (kmi->flag & KMI_INACTIVE) {
      continue;
    }

    if (kmi->propvalue == snap_on) {
      if ((ELEM(kmi->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) && event->ctrl) ||
          (ELEM(kmi->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY) && event->shift) ||
          (ELEM(kmi->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY) && event->alt) ||
          ((kmi->type == EVT_OSKEY) && event->oskey)) {
        return true;
      }
    }
  }
  return false;
}
#endif

static short snap_gizmo_snap_elements(SnapGizmo3D *snap_gizmo)
{
  int snap_elements = snap_gizmo->snap_elem_force;

  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(&snap_gizmo->gizmo, "snap_elements");
  if (gz_prop->prop) {
    snap_elements |= RNA_property_enum_get(&gz_prop->ptr, gz_prop->prop);
  }
  snap_elements &= (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE |
                    SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR);
  return (ushort)snap_elements;
}

/* -------------------------------------------------------------------- */
/** \name ED_gizmo_library specific API
 * \{ */

void ED_gizmotypes_snap_3d_draw_util(RegionView3D *rv3d,
                                     const float loc_prev[3],
                                     const float loc_curr[3],
                                     const float normal[3],
                                     const uchar color_line[4],
                                     const uchar color_point[4],
                                     const short snap_elem_type)
{
  if (!loc_prev && !loc_curr) {
    return;
  }

  float view_inv[4][4];
  copy_m4_m4(view_inv, rv3d->viewinv);

  /* The size of the circle is larger than the vertex size.
   * This prevents a drawing overlaps the other. */
  float radius = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (loc_curr) {
    immUniformColor4ubv(color_point);
    imm_drawcircball(loc_curr, ED_view3d_pixel_size(rv3d, loc_curr) * radius, view_inv, pos);

    /* draw normal if needed */
    if (normal) {
      immBegin(GPU_PRIM_LINES, 2);
      immVertex3fv(pos, loc_curr);
      immVertex3f(pos, loc_curr[0] + normal[0], loc_curr[1] + normal[1], loc_curr[2] + normal[2]);
      immEnd();
    }
  }

  if (loc_prev) {
    /* Draw an "X" indicating where the previous snap point is.
     * This is useful for indicating perpendicular snap. */

    /* v1, v2, v3 and v4 indicate the coordinates of the ends of the "X". */
    float vx[3], vy[3], v1[3], v2[3], v3[3], v4[4];

    /* Multiply by 0.75f so that the final size of the "X" is close to that of
     * the circle.
     * (A closer value is 0.7071f, but we don't need to be exact here). */
    float x_size = 0.75f * radius * ED_view3d_pixel_size(rv3d, loc_prev);

    mul_v3_v3fl(vx, view_inv[0], x_size);
    mul_v3_v3fl(vy, view_inv[1], x_size);

    add_v3_v3v3(v1, vx, vy);
    sub_v3_v3v3(v2, vx, vy);
    negate_v3_v3(v3, v1);
    negate_v3_v3(v4, v2);

    add_v3_v3(v1, loc_prev);
    add_v3_v3(v2, loc_prev);
    add_v3_v3(v3, loc_prev);
    add_v3_v3(v4, loc_prev);

    immUniformColor4ubv(color_line);
    immBegin(GPU_PRIM_LINES, 4);
    immVertex3fv(pos, v3);
    immVertex3fv(pos, v1);
    immVertex3fv(pos, v4);
    immVertex3fv(pos, v2);
    immEnd();

    if (loc_curr && (snap_elem_type & SCE_SNAP_MODE_EDGE_PERPENDICULAR)) {
      /* Dashed line. */
      immUnbindProgram();

      immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
      immUniform1f("dash_width", 6.0f * U.pixelsize);
      immUniform1f("dash_factor", 1.0f / 4.0f);
      immUniformColor4ubv(color_line);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex3fv(pos, loc_prev);
      immVertex3fv(pos, loc_curr);
      immEnd();
    }
  }

  immUnbindProgram();
}

SnapObjectContext *ED_gizmotypes_snap_3d_context_ensure(Scene *scene,
                                                        const ARegion *region,
                                                        const View3D *v3d,
                                                        wmGizmo *gz)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  if (snap_gizmo->snap_context_v3d == NULL) {
    snap_gizmo->snap_context_v3d = ED_transform_snap_object_context_create_view3d(
        scene, 0, region, v3d);
  }
  return snap_gizmo->snap_context_v3d;
}

void ED_gizmotypes_snap_3d_flag_set(struct wmGizmo *gz, eSnapGizmo flag)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  snap_gizmo->flag |= flag;
}

void ED_gizmotypes_snap_3d_flag_clear(struct wmGizmo *gz, eSnapGizmo flag)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  snap_gizmo->flag &= ~flag;
}

bool ED_gizmotypes_snap_3d_flag_test(struct wmGizmo *gz, eSnapGizmo flag)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  return (snap_gizmo->flag & flag) != 0;
}

bool ED_gizmotypes_snap_3d_invert_snap_get(struct wmGizmo *gz)
{
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  return snap_gizmo->invert_snap;
#else
  return false;
#endif
}

bool ED_gizmotypes_snap_3d_is_enabled(wmGizmo *gz)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  return snap_gizmo->is_enabled;
}

short ED_gizmotypes_snap_3d_update(wmGizmo *gz,
                                   struct Depsgraph *depsgraph,
                                   const ARegion *region,
                                   const View3D *v3d,
                                   const wmWindowManager *wm,
                                   const float mval_fl[2])
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  snap_gizmo->is_enabled = false;

  Scene *scene = DEG_get_input_scene(depsgraph);

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  if (!(snap_gizmo->flag & ED_SNAPGIZMO_TOGGLE_ALWAYS_TRUE)) {
    snap_gizmo->invert_snap = invert_snap(snap_gizmo, wm);

    const ToolSettings *ts = scene->toolsettings;
    if (snap_gizmo->invert_snap != !(ts->snap_flag & SCE_SNAP)) {
      snap_gizmo->snap_elem = 0;
      return 0;
    }
  }
#endif
  eventstate_save_xy(snap_gizmo, wm);

  snap_gizmo->is_enabled = true;

  float co[3], no[3];
  short snap_elem = 0;
  int snap_elem_index[3] = {-1, -1, -1};
  int index = -1;

  ushort snap_elements = snap_gizmo_snap_elements(snap_gizmo);

  if (snap_elements) {
    float prev_co[3] = {0.0f};
    if (snap_gizmo->prevpoint) {
      copy_v3_v3(prev_co, snap_gizmo->prevpoint);
    }
    else {
      snap_elements &= ~SCE_SNAP_MODE_EDGE_PERPENDICULAR;
    }

    eSnapSelect snap_select = (snap_gizmo->flag & ED_SNAPGIZMO_SNAP_ONLY_ACTIVE) ?
                                  SNAP_ONLY_ACTIVE :
                                  SNAP_ALL;

    eSnapEditType edit_mode_type = (snap_gizmo->flag & ED_SNAPGIZMO_SNAP_EDIT_GEOM_FINAL) ?
                                       SNAP_GEOM_FINAL :
                                       (snap_gizmo->flag & ED_SNAPGIZMO_SNAP_EDIT_GEOM_CAGE) ?
                                       SNAP_GEOM_CAGE :
                                       SNAP_GEOM_EDIT;

    bool use_occlusion_test = (snap_gizmo->flag & ED_SNAPGIZMO_OCCLUSION_ALWAYS_TRUE) ? false :
                                                                                        true;

    float dist_px = 12.0f * U.pixelsize;

    ED_gizmotypes_snap_3d_context_ensure(scene, region, v3d, gz);
    snap_elem = ED_transform_snap_object_project_view3d_ex(
        snap_gizmo->snap_context_v3d,
        depsgraph,
        snap_elements,
        &(const struct SnapObjectParams){
            .snap_select = snap_select,
            .edit_mode_type = edit_mode_type,
            .use_occlusion_test = use_occlusion_test,
        },
        mval_fl,
        prev_co,
        &dist_px,
        co,
        no,
        &index,
        NULL,
        NULL);
  }

  if (snap_elem == 0) {
    RegionView3D *rv3d = region->regiondata;
    ED_view3d_win_to_3d(v3d, region, rv3d->ofs, mval_fl, co);
    zero_v3(no);
  }
  else if (snap_elem == SCE_SNAP_MODE_VERTEX) {
    snap_elem_index[0] = index;
  }
  else if (snap_elem &
           (SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR)) {
    snap_elem_index[1] = index;
  }
  else if (snap_elem == SCE_SNAP_MODE_FACE) {
    snap_elem_index[2] = index;
  }

  snap_gizmo->snap_elem = snap_elem;
  copy_v3_v3(snap_gizmo->loc, co);
  copy_v3_v3(snap_gizmo->nor, no);
  copy_v3_v3_int(snap_gizmo->elem_index, snap_elem_index);

  return snap_elem;
}

void ED_gizmotypes_snap_3d_data_get(
    wmGizmo *gz, float r_loc[3], float r_nor[3], int r_elem_index[3], int *r_snap_elem)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  BLI_assert(snap_gizmo->is_enabled);
  if (r_loc) {
    copy_v3_v3(r_loc, snap_gizmo->loc);
  }
  if (r_nor) {
    copy_v3_v3(r_nor, snap_gizmo->nor);
  }
  if (r_elem_index) {
    copy_v3_v3_int(r_elem_index, snap_gizmo->elem_index);
  }
  if (r_snap_elem) {
    *r_snap_elem = snap_gizmo->snap_elem;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA callbacks
 * \{ */

/* Based on 'rna_GizmoProperties_find_operator'. */
static struct SnapGizmo3D *gizmo_snap_rna_find_operator(PointerRNA *ptr)
{
  IDProperty *properties = ptr->data;
  for (bScreen *screen = G_MAIN->screens.first; screen; screen = screen->id.next) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype != SPACE_VIEW3D) {
        continue;
      }
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW && region->gizmo_map) {
          wmGizmoMap *gzmap = region->gizmo_map;
          LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, WM_gizmomap_group_list(gzmap)) {
            LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
              if (gz->properties == properties) {
                return (SnapGizmo3D *)gz;
              }
            }
          }
        }
      }
    }
  }
  return NULL;
}

static int gizmo_snap_rna_snap_elements_force_get_fn(struct PointerRNA *ptr,
                                                     struct PropertyRNA *UNUSED(prop))
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    return snap_gizmo->snap_elem_force;
  }
  return 0;
}

static void gizmo_snap_rna_snap_elements_force_set_fn(struct PointerRNA *ptr,
                                                      struct PropertyRNA *UNUSED(prop),
                                                      int value)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    snap_gizmo->snap_elem_force = (short)value;
  }
}

static void gizmo_snap_rna_prevpoint_get_fn(struct PointerRNA *ptr,
                                            struct PropertyRNA *UNUSED(prop),
                                            float *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    copy_v3_v3(values, snap_gizmo->prevpoint_stack);
  }
}

static void gizmo_snap_rna_prevpoint_set_fn(struct PointerRNA *ptr,
                                            struct PropertyRNA *UNUSED(prop),
                                            const float *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    if (values) {
      copy_v3_v3(snap_gizmo->prevpoint_stack, values);
      snap_gizmo->prevpoint = snap_gizmo->prevpoint_stack;
    }
    else {
      snap_gizmo->prevpoint = NULL;
    }
  }
}

static void gizmo_snap_rna_location_get_fn(struct PointerRNA *ptr,
                                           struct PropertyRNA *UNUSED(prop),
                                           float *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    copy_v3_v3(values, snap_gizmo->loc);
  }
}

static void gizmo_snap_rna_location_set_fn(struct PointerRNA *ptr,
                                           struct PropertyRNA *UNUSED(prop),
                                           const float *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    copy_v3_v3(snap_gizmo->loc, values);
  }
}

static void gizmo_snap_rna_normal_get_fn(struct PointerRNA *ptr,
                                         struct PropertyRNA *UNUSED(prop),
                                         float *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    copy_v3_v3(values, snap_gizmo->nor);
  }
}

static void gizmo_snap_rna_snap_elem_index_get_fn(struct PointerRNA *ptr,
                                                  struct PropertyRNA *UNUSED(prop),
                                                  int *values)
{
  SnapGizmo3D *snap_gizmo = gizmo_snap_rna_find_operator(ptr);
  if (snap_gizmo) {
    copy_v3_v3_int(values, snap_gizmo->elem_index);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GIZMO_GT_snap_3d
 * \{ */

static void snap_gizmo_setup(wmGizmo *gz)
{
  gz->flag |= WM_GIZMO_NO_TOOLTIP;

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  snap_gizmo->keymap = WM_modalkeymap_find(gz->parent_gzgroup->type->keyconf,
                                           "Generic Gizmo Tweak Modal Map");
  RNA_enum_value_from_id(snap_gizmo->keymap->modal_items, "SNAP_ON", &snap_gizmo->snap_on);
#endif
}

static void snap_gizmo_draw(const bContext *C, wmGizmo *gz)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  if (snap_gizmo->snap_elem == 0) {
    return;
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  if (eventstate_has_changed(snap_gizmo, wm)) {
    /* The eventstate has changed but the snap has not been updated.
     * This means that the current position is no longer valid. */
    snap_gizmo->snap_elem = 0;
    return;
  }

  RegionView3D *rv3d = CTX_wm_region_data(C);
  if (rv3d->rflag & RV3D_NAVIGATING) {
    /* Don't draw the gizmo while navigating. It can be distracting. */
    snap_gizmo->snap_elem = 0;
    return;
  }

  uchar color_line[4], color_point[4];
  UI_GetThemeColor3ubv(TH_TRANSFORM, color_line);
  color_line[3] = 128;

  rgba_float_to_uchar(color_point, gz->color);

  GPU_line_smooth(false);

  GPU_line_width(1.0f);

  const float *prev_point = (snap_gizmo_snap_elements(snap_gizmo) &
                             SCE_SNAP_MODE_EDGE_PERPENDICULAR) ?
                                snap_gizmo->prevpoint :
                                NULL;

  ED_gizmotypes_snap_3d_draw_util(
      rv3d, prev_point, snap_gizmo->loc, NULL, color_line, color_point, snap_gizmo->snap_elem);
}

static int snap_gizmo_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  wmWindowManager *wm = CTX_wm_manager(C);
  ARegion *region = CTX_wm_region(C);

  /* FIXME: this hack is to ignore drag events, otherwise drag events
   * cause momentary snap gizmo re-positioning at the drag-start location, see: T87511. */
  if (wm && wm->winactive) {
    const wmEvent *event = wm->winactive->eventstate;
    int mval_compare[2] = {event->x - region->winrct.xmin, event->y - region->winrct.ymin};
    if (!equals_v2v2_int(mval_compare, mval)) {
      return snap_gizmo->snap_elem ? 0 : -1;
    }
  }

  if (!eventstate_has_changed(snap_gizmo, wm)) {
    /* Performance, do not update. */
    return snap_gizmo->snap_elem ? 0 : -1;
  }

  View3D *v3d = CTX_wm_view3d(C);
  const float mval_fl[2] = {UNPACK2(mval)};
  short snap_elem = ED_gizmotypes_snap_3d_update(
      gz, CTX_data_ensure_evaluated_depsgraph(C), region, v3d, wm, mval_fl);

  if (snap_elem) {
    ED_region_tag_redraw_editor_overlays(region);
    return 0;
  }

  return -1;
}

static int snap_gizmo_modal(bContext *UNUSED(C),
                            wmGizmo *UNUSED(gz),
                            const wmEvent *UNUSED(event),
                            eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
  return OPERATOR_RUNNING_MODAL;
}

static int snap_gizmo_invoke(bContext *UNUSED(C),
                             wmGizmo *UNUSED(gz),
                             const wmEvent *UNUSED(event))
{
  return OPERATOR_RUNNING_MODAL;
}

static void snap_gizmo_free(wmGizmo *gz)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  if (snap_gizmo->snap_context_v3d) {
    ED_transform_snap_object_context_destroy(snap_gizmo->snap_context_v3d);
    snap_gizmo->snap_context_v3d = NULL;
  }
}

static void GIZMO_GT_snap_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_snap_3d";

  /* api callbacks */
  gzt->setup = snap_gizmo_setup;
  gzt->draw = snap_gizmo_draw;
  gzt->test_select = snap_gizmo_test_select;
  gzt->modal = snap_gizmo_modal;
  gzt->invoke = snap_gizmo_invoke;
  gzt->free = snap_gizmo_free;

  gzt->struct_size = sizeof(SnapGizmo3D);

  const EnumPropertyItem *rna_enum_snap_element_items;
  {
    /* Get Snap Element Items enum. */
    bool free;
    PointerRNA toolsettings_ptr;
    RNA_pointer_create(NULL, &RNA_ToolSettings, NULL, &toolsettings_ptr);
    PropertyRNA *prop = RNA_struct_find_property(&toolsettings_ptr, "snap_elements");
    RNA_property_enum_items(
        NULL, &toolsettings_ptr, prop, &rna_enum_snap_element_items, NULL, &free);

    BLI_assert(free == false);
  }

  /* Setup. */
  PropertyRNA *prop;
  prop = RNA_def_enum_flag(gzt->srna,
                           "snap_elements_force",
                           rna_enum_snap_element_items,
                           SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE,
                           "Snap Elements",
                           "");

  RNA_def_property_enum_funcs_runtime(prop,
                                      gizmo_snap_rna_snap_elements_force_get_fn,
                                      gizmo_snap_rna_snap_elements_force_set_fn,
                                      NULL);

  prop = RNA_def_float_array(gzt->srna,
                             "prev_point",
                             3,
                             NULL,
                             FLT_MIN,
                             FLT_MAX,
                             "Previous Point",
                             "Point that defines the location of the perpendicular snap",
                             FLT_MIN,
                             FLT_MAX);

  RNA_def_property_float_array_funcs_runtime(
      prop, gizmo_snap_rna_prevpoint_get_fn, gizmo_snap_rna_prevpoint_set_fn, NULL);

  /* Returns. */
  prop = RNA_def_float_translation(gzt->srna,
                                   "location",
                                   3,
                                   NULL,
                                   FLT_MIN,
                                   FLT_MAX,
                                   "Location",
                                   "Snap Point Location",
                                   FLT_MIN,
                                   FLT_MAX);

  RNA_def_property_float_array_funcs_runtime(
      prop, gizmo_snap_rna_location_get_fn, gizmo_snap_rna_location_set_fn, NULL);

  prop = RNA_def_float_vector_xyz(gzt->srna,
                                  "normal",
                                  3,
                                  NULL,
                                  FLT_MIN,
                                  FLT_MAX,
                                  "Normal",
                                  "Snap Point Normal",
                                  FLT_MIN,
                                  FLT_MAX);

  RNA_def_property_float_array_funcs_runtime(prop, gizmo_snap_rna_normal_get_fn, NULL, NULL);

  prop = RNA_def_int_vector(gzt->srna,
                            "snap_elem_index",
                            3,
                            NULL,
                            INT_MIN,
                            INT_MAX,
                            "Snap Element",
                            "Array index of face, edge and vert snapped",
                            INT_MIN,
                            INT_MAX);

  RNA_def_property_int_array_funcs_runtime(
      prop, gizmo_snap_rna_snap_elem_index_get_fn, NULL, NULL);

  /* Read/Write. */
  WM_gizmotype_target_property_def(gzt, "snap_elements", PROP_ENUM, 1);
}

void ED_gizmotypes_snap_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_snap_3d);
}

/** \} */
