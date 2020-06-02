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

#include "BLI_math.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"

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
#include "wm.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

typedef struct SnapGizmo3D {
  wmGizmo gizmo;
  PropertyRNA *prop_prevpoint;
  PropertyRNA *prop_location;
  PropertyRNA *prop_normal;
  PropertyRNA *prop_elem_index;
  PropertyRNA *prop_snap_force;

  /* We could have other snap contexts, for now only support 3D view. */
  SnapObjectContext *snap_context_v3d;
  int mval[2];

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  wmKeyMap *keymap;
  int snap_on;
  bool invert_snap;
#endif
  int use_snap_override;
  short snap_elem;
} SnapGizmo3D;

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
static bool invert_snap(const wmGizmo *gz, const wmWindowManager *wm, const wmEvent *event)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  wmKeyMap *keymap = WM_keymap_active(wm, gizmo_snap->keymap);

  const int snap_on = gizmo_snap->snap_on;
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
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  if (gizmo_snap->snap_context_v3d == NULL) {
    gizmo_snap->snap_context_v3d = ED_transform_snap_object_context_create_view3d(
        scene, 0, region, v3d);
  }
  return gizmo_snap->snap_context_v3d;
}

bool ED_gizmotypes_snap_3d_invert_snap_get(struct wmGizmo *gz)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  return gizmo_snap->invert_snap;
}
void ED_gizmotypes_snap_3d_toggle_set(wmGizmo *gz, bool enable)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  gizmo_snap->use_snap_override = (int)enable;
}

void ED_gizmotypes_snap_3d_toggle_clear(wmGizmo *gz)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  gizmo_snap->use_snap_override = -1;
}

short ED_gizmotypes_snap_3d_update(wmGizmo *gz,
                                   struct Depsgraph *depsgraph,
                                   const ARegion *region,
                                   const View3D *v3d,
                                   const wmWindowManager *wm,
                                   const float mval_fl[2],
                                   float r_loc[3],
                                   float r_nor[3])
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  Scene *scene = DEG_get_input_scene(depsgraph);
  float co[3], no[3];
  short snap_elem = 0;
  int snap_elem_index[3] = {-1, -1, -1};
  int index = -1;

  if (gizmo_snap->use_snap_override != -1) {
    if (gizmo_snap->use_snap_override == false) {
      gizmo_snap->snap_elem = 0;
      return 0;
    }
  }

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  if (wm && wm->winactive) {
    gizmo_snap->invert_snap = invert_snap(gz, wm, wm->winactive->eventstate);
  }

  if (gizmo_snap->use_snap_override == -1) {
    const ToolSettings *ts = scene->toolsettings;
    if (gizmo_snap->invert_snap != !(ts->snap_flag & SCE_SNAP)) {
      gizmo_snap->snap_elem = 0;
      return 0;
    }
  }
#else
  UNUSED_VARS(wm);
#endif

  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "snap_elements");
  int snap_elements = RNA_property_enum_get(&gz_prop->ptr, gz_prop->prop);
  if (gz_prop->prop != gizmo_snap->prop_snap_force) {
    int snap_elements_force = RNA_property_enum_get(gz->ptr, gizmo_snap->prop_snap_force);
    snap_elements |= snap_elements_force;
  }
  snap_elements &= (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE |
                    SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR);

  if (snap_elements) {
    float prev_co[3] = {0.0f};
    if (RNA_property_is_set(gz->ptr, gizmo_snap->prop_prevpoint)) {
      RNA_property_float_get_array(gz->ptr, gizmo_snap->prop_prevpoint, prev_co);
    }
    else {
      snap_elements &= ~SCE_SNAP_MODE_EDGE_PERPENDICULAR;
    }

    float dist_px = 12.0f * U.pixelsize;

    ED_gizmotypes_snap_3d_context_ensure(scene, region, v3d, gz);
    snap_elem = ED_transform_snap_object_project_view3d_ex(gizmo_snap->snap_context_v3d,
                                                           depsgraph,
                                                           snap_elements,
                                                           &(const struct SnapObjectParams){
                                                               .snap_select = SNAP_ALL,
                                                               .use_object_edit_cage = true,
                                                               .use_occlusion_test = true,
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

  gizmo_snap->snap_elem = snap_elem;
  RNA_property_float_set_array(gz->ptr, gizmo_snap->prop_location, co);
  RNA_property_float_set_array(gz->ptr, gizmo_snap->prop_normal, no);
  RNA_property_int_set_array(gz->ptr, gizmo_snap->prop_elem_index, snap_elem_index);

  if (r_loc) {
    copy_v3_v3(r_loc, co);
  }
  if (r_nor) {
    copy_v3_v3(r_nor, no);
  }

  return snap_elem;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GIZMO_GT_snap_3d
 * \{ */

static void gizmo_snap_setup(wmGizmo *gz)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;

  /* For quick access to the props. */
  gizmo_snap->prop_prevpoint = RNA_struct_find_property(gz->ptr, "prev_point");
  gizmo_snap->prop_location = RNA_struct_find_property(gz->ptr, "location");
  gizmo_snap->prop_normal = RNA_struct_find_property(gz->ptr, "normal");
  gizmo_snap->prop_elem_index = RNA_struct_find_property(gz->ptr, "snap_elem_index");
  gizmo_snap->prop_snap_force = RNA_struct_find_property(gz->ptr, "snap_elements_force");

  gizmo_snap->use_snap_override = -1;

  /* Prop fallback. */
  WM_gizmo_target_property_def_rna(gz, "snap_elements", gz->ptr, "snap_elements_force", -1);

  /* Flags. */
  gz->flag |= WM_GIZMO_NO_TOOLTIP;
}

static void gizmo_snap_draw(const bContext *C, wmGizmo *gz)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  if (gizmo_snap->snap_elem == 0) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = region->regiondata;

  /* Ideally, we shouldn't assign values here.
   * But `test_select` is not called during navigation.
   * And `snap_elem` is not really useful in this case. */
  if ((rv3d->rflag & RV3D_NAVIGATING) ||
      (!(gz->state & WM_GIZMO_STATE_HIGHLIGHT) && !wm_gizmomap_modal_get(region->gizmo_map))) {
    gizmo_snap->snap_elem = 0;
    return;
  }

  float location[3], prev_point_stack[3], *prev_point = NULL;
  uchar color_line[4], color_point[4];

  RNA_property_float_get_array(gz->ptr, gizmo_snap->prop_location, location);

  UI_GetThemeColor3ubv(TH_TRANSFORM, color_line);
  color_line[3] = 128;

  rgba_float_to_uchar(color_point, gz->color);

  if (RNA_property_is_set(gz->ptr, gizmo_snap->prop_prevpoint)) {
    RNA_property_float_get_array(gz->ptr, gizmo_snap->prop_prevpoint, prev_point_stack);
    prev_point = prev_point_stack;
  }

  GPU_line_smooth(false);

  GPU_line_width(1.0f);
  ED_gizmotypes_snap_3d_draw_util(
      rv3d, prev_point, location, NULL, color_line, color_point, gizmo_snap->snap_elem);
}

static int gizmo_snap_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  wmWindowManager *wm = CTX_wm_manager(C);
  if (gizmo_snap->keymap == NULL) {
    gizmo_snap->keymap = WM_modalkeymap_find(wm->defaultconf, "Generic Gizmo Tweak Modal Map");
    RNA_enum_value_from_id(gizmo_snap->keymap->modal_items, "SNAP_ON", &gizmo_snap->snap_on);
  }

  const bool invert = wm->winactive ? invert_snap(gz, wm, wm->winactive->eventstate) : false;
  if (gizmo_snap->invert_snap == invert && gizmo_snap->mval[0] == mval[0] &&
      gizmo_snap->mval[1] == mval[1]) {
    /* Performance, do not update. */
    return gizmo_snap->snap_elem ? 0 : -1;
  }

  gizmo_snap->invert_snap = invert;
#else
  if (gizmo_snap->mval[0] == mval[0] && gizmo_snap->mval[1] == mval[1]) {
    /* Performance, do not update. */
    return gizmo_snap->snap_elem ? 0 : -1;
  }
#endif
  copy_v2_v2_int(gizmo_snap->mval, mval);

  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  const float mval_fl[2] = {UNPACK2(mval)};
  short snap_elem = ED_gizmotypes_snap_3d_update(
      gz, CTX_data_ensure_evaluated_depsgraph(C), region, v3d, NULL, mval_fl, NULL, NULL);

  if (snap_elem) {
    ED_region_tag_redraw_editor_overlays(region);
    return 0;
  }

  return -1;
}

static int gizmo_snap_modal(bContext *UNUSED(C),
                            wmGizmo *UNUSED(gz),
                            const wmEvent *UNUSED(event),
                            eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
  return OPERATOR_RUNNING_MODAL;
}

static int gizmo_snap_invoke(bContext *UNUSED(C),
                             wmGizmo *UNUSED(gz),
                             const wmEvent *UNUSED(event))
{
  return OPERATOR_RUNNING_MODAL;
}

static void gizmo_snap_free(wmGizmo *gz)
{
  SnapGizmo3D *gizmo_snap = (SnapGizmo3D *)gz;
  if (gizmo_snap->snap_context_v3d) {
    ED_transform_snap_object_context_destroy(gizmo_snap->snap_context_v3d);
    gizmo_snap->snap_context_v3d = NULL;
  }
}

static void GIZMO_GT_snap_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_snap_3d";

  /* api callbacks */
  gzt->setup = gizmo_snap_setup;
  gzt->draw = gizmo_snap_draw;
  gzt->test_select = gizmo_snap_test_select;
  gzt->modal = gizmo_snap_modal;
  gzt->invoke = gizmo_snap_invoke;
  gzt->free = gizmo_snap_free;

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
  RNA_def_enum_flag(gzt->srna,
                    "snap_elements_force",
                    rna_enum_snap_element_items,
                    SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE,
                    "Snap Elements",
                    "");

  RNA_def_float_vector(gzt->srna,
                       "prev_point",
                       3,
                       NULL,
                       FLT_MIN,
                       FLT_MAX,
                       "Previous Point",
                       "Point that defines the location of the perpendicular snap",
                       FLT_MIN,
                       FLT_MAX);

  /* Returns. */
  RNA_def_float_vector(gzt->srna,
                       "location",
                       3,
                       NULL,
                       FLT_MIN,
                       FLT_MAX,
                       "Location",
                       "Snap Point Location",
                       FLT_MIN,
                       FLT_MAX);

  RNA_def_float_vector(gzt->srna,
                       "normal",
                       3,
                       NULL,
                       FLT_MIN,
                       FLT_MAX,
                       "Normal",
                       "Snap Point Normal",
                       FLT_MIN,
                       FLT_MAX);

  RNA_def_int_vector(gzt->srna,
                     "snap_elem_index",
                     3,
                     NULL,
                     INT_MIN,
                     INT_MAX,
                     "Snap Element",
                     "Array index of face, edge and vert snapped",
                     INT_MIN,
                     INT_MAX);

  /* Read/Write. */
  WM_gizmotype_target_property_def(gzt, "snap_elements", PROP_ENUM, 1);
}

void ED_gizmotypes_snap_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_snap_3d);
}

/** \} */
