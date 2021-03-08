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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 * Operators for creating new Grease Pencil primitives (boxes, circles, ...)
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

#define MIN_EDGES 2
#define MAX_EDGES 128
#define MAX_CP 128

#define IDLE 0
#define IN_PROGRESS 1
#define IN_CURVE_EDIT 2
#define IN_MOVE 3
#define IN_BRUSH_SIZE 4
#define IN_BRUSH_STRENGTH 5
#define IN_POLYLINE 6

#define SELECT_NONE 0
#define SELECT_START 1
#define SELECT_CP1 2
#define SELECT_CP2 3
#define SELECT_END 4

#define BIG_SIZE_CTL 15
#define MID_SIZE_CTL 10
#define SMALL_SIZE_CTL 8

#define MOVE_NONE 0
#define MOVE_ENDS 1
#define MOVE_CP 2

/* ************************************************ */
/* Core/Shared Utilities */
static const EnumPropertyItem gpencil_primitive_type[] = {
    {GP_STROKE_BOX, "BOX", 0, "Box", ""},
    {GP_STROKE_LINE, "LINE", 0, "Line", ""},
    {GP_STROKE_POLYLINE, "POLYLINE", 0, "Polyline", ""},
    {GP_STROKE_CIRCLE, "CIRCLE", 0, "Circle", ""},
    {GP_STROKE_ARC, "ARC", 0, "Arc", ""},
    {GP_STROKE_CURVE, "CURVE", 0, "Curve", ""},
    {0, NULL, 0, NULL, NULL},
};
/* clear the session buffers (call this before AND after a paint operation) */
static void gpencil_session_validatebuffer(tGPDprimitive *p)
{
  bGPdata *gpd = p->gpd;

  /* clear memory of buffer (or allocate it if starting a new session) */
  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, true);

  /* reset flags */
  gpd->runtime.sbuffer_sflag = 0;
  gpd->runtime.sbuffer_sflag |= GP_STROKE_3DSPACE;

  /* Set vertex colors for buffer. */
  ED_gpencil_sbuffer_vertex_color_set(p->depsgraph,
                                      p->ob,
                                      p->scene->toolsettings,
                                      p->brush,
                                      p->material,
                                      p->random_settings.hsv,
                                      1.0f);

  if (ELEM(p->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
    gpd->runtime.sbuffer_sflag |= GP_STROKE_CYCLIC;
  }
}

static void gpencil_init_colors(tGPDprimitive *p)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;

  /* use brush material */
  p->material = BKE_gpencil_object_material_ensure_from_active_input_brush(p->bmain, p->ob, brush);

  gpd->runtime.matid = BKE_object_material_slot_find_index(p->ob, p->material);
  gpd->runtime.sbuffer_brush = brush;
}

/* Helper to square a primitive */
static void gpencil_primitive_to_square(tGPDprimitive *tgpi, const float x, const float y)
{
  float w = fabsf(x);
  float h = fabsf(y);
  if ((x > 0 && y > 0) || (x < 0 && y < 0)) {
    if (w > h) {
      tgpi->end[1] = tgpi->origin[1] + x;
    }
    else {
      tgpi->end[0] = tgpi->origin[0] + y;
    }
  }
  else {
    if (w > h) {
      tgpi->end[1] = tgpi->origin[1] - x;
    }
    else {
      tgpi->end[0] = tgpi->origin[0] - y;
    }
  }
}

/* Helper to constrain a primitive */
static void gpencil_primitive_constrain(tGPDprimitive *tgpi, bool line_mode)
{
  float x = tgpi->end[0] - tgpi->origin[0];
  float y = tgpi->end[1] - tgpi->origin[1];

  if (line_mode) {
    float angle = fabsf(atan2f(y, x));
    if (angle < 0.4f || angle > (M_PI - 0.4f)) {
      tgpi->end[1] = tgpi->origin[1];
    }
    else if (angle > (M_PI_2 - 0.4f) && angle < (M_PI_2 + 0.4f)) {
      tgpi->end[0] = tgpi->origin[0];
    }
    else {
      gpencil_primitive_to_square(tgpi, x, y);
    }
  }
  else {
    gpencil_primitive_to_square(tgpi, x, y);
  }
}

/* Helper to rotate point around origin */
static void gpencil_rotate_v2_v2v2fl(float v[2],
                                     const float p[2],
                                     const float origin[2],
                                     const float angle)
{
  float pt[2];
  float r[2];
  sub_v2_v2v2(pt, p, origin);
  rotate_v2_v2fl(r, pt, angle);
  add_v2_v2v2(v, r, origin);
}

/* Helper to rotate line around line center. */
static void gpencil_primitive_rotate_line(
    float va[2], float vb[2], const float a[2], const float b[2], const float angle)
{
  float midpoint[2];
  mid_v2_v2v2(midpoint, a, b);
  gpencil_rotate_v2_v2v2fl(va, a, midpoint, angle);
  gpencil_rotate_v2_v2v2fl(vb, b, midpoint, angle);
}

/* Helper to update cps */
static void gpencil_primitive_update_cps(tGPDprimitive *tgpi)
{
  if (!tgpi->curve) {
    mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
    copy_v2_v2(tgpi->cp1, tgpi->midpoint);
    copy_v2_v2(tgpi->cp2, tgpi->cp1);
  }
  else if (tgpi->type == GP_STROKE_CURVE) {
    mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
    interp_v2_v2v2(tgpi->cp1, tgpi->midpoint, tgpi->start, 0.33f);
    interp_v2_v2v2(tgpi->cp2, tgpi->midpoint, tgpi->end, 0.33f);
  }
  else if (tgpi->type == GP_STROKE_ARC) {
    if (tgpi->flip) {
      gpencil_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->start, tgpi->end, M_PI_2);
    }
    else {
      gpencil_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->end, tgpi->start, M_PI_2);
    }
  }
}

/* Poll callback for primitive operators */
static bool gpencil_primitive_add_poll(bContext *C)
{
  /* only 3D view */
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  /* need data to create primitive */
  bGPdata *gpd = CTX_data_gpencil_data(C);
  if (gpd == NULL) {
    return false;
  }

  /* only in edit and paint modes
   * - paint as it's the "drawing/creation mode"
   * - edit as this is more of an atomic editing operation
   *   (similar to copy/paste), and also for consistency
   */
  if ((gpd->flag & (GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE)) == 0) {
    CTX_wm_operator_poll_msg_set(C, "Primitives can only be added in Draw or Edit modes");
    return false;
  }

  /* don't allow operator to function if the active layer is locked/hidden
   * (BUT, if there isn't an active layer, we are free to add new layer when the time comes)
   */
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  if ((gpl) && (gpl->flag & (GP_LAYER_LOCKED | GP_LAYER_HIDE))) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Primitives cannot be added as active layer is locked or hidden");
    return false;
  }

  return true;
}

/* Allocate memory to stroke, adds MAX_EDGES on every call */
static void gpencil_primitive_allocate_memory(tGPDprimitive *tgpi)
{
  tgpi->point_count += (tgpi->type == GP_STROKE_BOX) ? (MAX_EDGES * 4 + 1) : (MAX_EDGES + 1);
  bGPDstroke *gpsf = tgpi->gpf->strokes.first;
  gpsf->points = MEM_reallocN(gpsf->points, sizeof(bGPDspoint) * tgpi->point_count);
  if (gpsf->dvert != NULL) {
    gpsf->dvert = MEM_reallocN(gpsf->dvert, sizeof(MDeformVert) * tgpi->point_count);
  }
  tgpi->points = MEM_reallocN(tgpi->points, sizeof(tGPspoint) * tgpi->point_count);
}

/* ****************** Primitive Interactive *********************** */

/* Helper: Create internal strokes primitives data */
static void gpencil_primitive_set_initdata(bContext *C, tGPDprimitive *tgpi)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Brush *brush = tgpi->brush;
  int cfra = CFRA;

  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  /* if layer doesn't exist, create a new one */
  if (gpl == NULL) {
    gpl = BKE_gpencil_layer_addnew(tgpi->gpd, DATA_("Primitives"), true);
  }
  tgpi->gpl = gpl;

  /* Recalculate layer transform matrix to avoid problems if props are animated. */
  loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);

  /* create a new temporary frame */
  tgpi->gpf = MEM_callocN(sizeof(bGPDframe), "Temp bGPDframe");
  tgpi->gpf->framenum = tgpi->cframe = cfra;

  /* create new temp stroke */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "Temp bGPDstroke");
  gps->thickness = 2.0f;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = 1.0f;
  copy_v2_fl(gps->aspect_ratio, 1.0f);
  gps->uv_scale = 1.0f;
  gps->inittime = 0.0f;

  /* Apply the vertex color to fill. */
  ED_gpencil_fill_vertex_color_set(ts, brush, gps);

  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_reset(gps);
  /* the polygon must be closed, so enabled cyclic */
  if (ELEM(tgpi->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
    gps->flag |= GP_STROKE_CYCLIC;
  }

  gps->flag |= GP_STROKE_3DSPACE;

  gps->mat_nr = BKE_gpencil_object_material_get_index_from_brush(tgpi->ob, tgpi->brush);
  if (gps->mat_nr < 0) {
    if (tgpi->ob->actcol - 1 < 0) {
      gps->mat_nr = 0;
    }
    else {
      gps->mat_nr = tgpi->ob->actcol - 1;
    }
  }

  /* allocate memory for storage points, but keep empty */
  gps->totpoints = 0;
  gps->points = MEM_callocN(sizeof(bGPDspoint), "gp_stroke_points");
  gps->dvert = NULL;

  /* initialize triangle memory to dummy data */
  gps->tot_triangles = 0;
  gps->triangles = NULL;

  /* add to strokes */
  BLI_addtail(&tgpi->gpf->strokes, gps);

  /* allocate memory for storage points */
  gpencil_primitive_allocate_memory(tgpi);

  /* Random generator, only init once. */
  uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
  tgpi->rng = BLI_rng_new(rng_seed);

  DEG_id_tag_update(&tgpi->gpd->id, ID_RECALC_COPY_ON_WRITE);
}

/* add new segment to curve */
static void gpencil_primitive_add_segment(tGPDprimitive *tgpi)
{
  if (tgpi->tot_stored_edges > 0) {
    tgpi->tot_stored_edges += (tgpi->tot_edges - 1);
  }
  else {
    tgpi->tot_stored_edges += tgpi->tot_edges;
  }
  gpencil_primitive_allocate_memory(tgpi);
}

/* Helper: set control point */
static void gpencil_primitive_set_cp(tGPDprimitive *tgpi,
                                     const float p[2],
                                     float color[4],
                                     int size)
{
  if (tgpi->flag == IN_PROGRESS) {
    return;
  }

  bGPDcontrolpoint *cp_points = tgpi->gpd->runtime.cp_points;

  if (tgpi->gpd->runtime.tot_cp_points < MAX_CP) {
    CLAMP(size, 5, 20);
    bGPDcontrolpoint *cp = &cp_points[tgpi->gpd->runtime.tot_cp_points];
    copy_v2_v2(&cp->x, p);
    copy_v4_v4(cp->color, color);
    color[3] = 0.8f;
    cp->size = size;
    tgpi->gpd->runtime.tot_cp_points += 1;
  }
}

/* Helper: Draw status message while the user is running the operator */
static void gpencil_primitive_status_indicators(bContext *C, tGPDprimitive *tgpi)
{
  Scene *scene = tgpi->scene;
  char status_str[UI_MAX_DRAW_STR];
  char msg_str[UI_MAX_DRAW_STR];

  if (tgpi->type == GP_STROKE_LINE) {
    BLI_strncpy(msg_str,
                TIP_("Line: ESC to cancel, LMB set origin, Enter/MMB to confirm, WHEEL/+- to "
                     "adjust subdivision number, Shift to align, Alt to center, E: extrude"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_POLYLINE) {
    BLI_strncpy(msg_str,
                TIP_("Polyline: ESC to cancel, LMB to set, Enter/MMB to confirm, WHEEL/+- to "
                     "adjust subdivision number, Shift to align"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_BOX) {
    BLI_strncpy(msg_str,
                TIP_("Rectangle: ESC to cancel, LMB set origin, Enter/MMB to confirm, WHEEL/+- "
                     "to adjust subdivision number, Shift to square, Alt to center"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_CIRCLE) {
    BLI_strncpy(msg_str,
                TIP_("Circle: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust subdivision "
                     "number, Shift to square, Alt to center"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_ARC) {
    BLI_strncpy(
        msg_str,
        TIP_("Arc: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust subdivision number, "
             "Shift to square, Alt to center, M: Flip, E: extrude"),
        UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_CURVE) {
    BLI_strncpy(msg_str,
                TIP_("Curve: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust subdivision "
                     "number, Shift to square, Alt to center, E: extrude"),
                UI_MAX_DRAW_STR);
  }

  if (ELEM(tgpi->type,
           GP_STROKE_CIRCLE,
           GP_STROKE_ARC,
           GP_STROKE_LINE,
           GP_STROKE_BOX,
           GP_STROKE_POLYLINE)) {
    if (hasNumInput(&tgpi->num)) {
      char str_ofs[NUM_STR_REP_LEN];

      outputNumInput(&tgpi->num, str_ofs, &scene->unit);
      BLI_snprintf(status_str, sizeof(status_str), "%s: %s", msg_str, str_ofs);
    }
    else {
      if (tgpi->flag == IN_PROGRESS) {
        BLI_snprintf(status_str,
                     sizeof(status_str),
                     "%s: %d (%d, %d) (%d, %d)",
                     msg_str,
                     tgpi->tot_edges,
                     (int)tgpi->start[0],
                     (int)tgpi->start[1],
                     (int)tgpi->end[0],
                     (int)tgpi->end[1]);
      }
      else {
        BLI_snprintf(status_str,
                     sizeof(status_str),
                     "%s: %d (%d, %d)",
                     msg_str,
                     tgpi->tot_edges,
                     (int)tgpi->end[0],
                     (int)tgpi->end[1]);
      }
    }
  }
  else {
    if (tgpi->flag == IN_PROGRESS) {
      BLI_snprintf(status_str,
                   sizeof(status_str),
                   "%s: %d (%d, %d) (%d, %d)",
                   msg_str,
                   tgpi->tot_edges,
                   (int)tgpi->start[0],
                   (int)tgpi->start[1],
                   (int)tgpi->end[0],
                   (int)tgpi->end[1]);
    }
    else {
      BLI_snprintf(status_str,
                   sizeof(status_str),
                   "%s: (%d, %d)",
                   msg_str,
                   (int)tgpi->end[0],
                   (int)tgpi->end[1]);
    }
  }
  ED_workspace_status_text(C, status_str);
}

/* create a rectangle */
static void gpencil_primitive_rectangle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  float coords[5][2];

  coords[0][0] = tgpi->start[0];
  coords[0][1] = tgpi->start[1];
  coords[1][0] = tgpi->end[0];
  coords[1][1] = tgpi->start[1];
  coords[2][0] = tgpi->end[0];
  coords[2][1] = tgpi->end[1];
  coords[3][0] = tgpi->start[0];
  coords[3][1] = tgpi->end[1];
  coords[4][0] = tgpi->start[0];
  coords[4][1] = tgpi->start[1];

  if (tgpi->tot_edges == 1) {
    for (int j = 0; j < 4; j++) {
      tGPspoint *p2d = &points2D[j];
      copy_v2_v2(&p2d->x, coords[j]);
    }
  }
  else {
    const float step = 1.0f / (float)(tgpi->tot_edges);
    int i = tgpi->tot_stored_edges;
    for (int j = 0; j < 4; j++) {
      float a = 0.0f;
      for (int k = 0; k < tgpi->tot_edges; k++) {
        tGPspoint *p2d = &points2D[i];
        interp_v2_v2v2(&p2d->x, coords[j], coords[j + 1], a);
        a += step;
        i++;
      }
    }
  }

  mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gpencil_primitive_set_cp(tgpi, tgpi->midpoint, color, SMALL_SIZE_CTL);
}

/* create a line */
static void gpencil_primitive_line(tGPDprimitive *tgpi, tGPspoint *points2D, bool editable)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = 1.0f / (float)(tgpi->tot_edges - 1);
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    interp_v2_v2v2(&p2d->x, tgpi->start, tgpi->end, a);
    a += step;
  }

  if (editable) {
    float color[4];
    UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
    gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
    if (tgpi->tot_stored_edges) {
      UI_GetThemeColor4fv(TH_REDALERT, color);
      gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
    }
    else {
      gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
    }
  }
  else {
    float color[4];
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
}

/* create an arc */
static void gpencil_primitive_arc(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = M_PI_2 / (float)(tgpi->tot_edges - 1);
  float start[2];
  float end[2];
  float cp1[2];
  float corner[2];
  float midpoint[2];
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
  copy_v2_v2(start, tgpi->start);
  copy_v2_v2(end, tgpi->end);
  copy_v2_v2(cp1, tgpi->cp1);
  copy_v2_v2(midpoint, tgpi->midpoint);

  corner[0] = midpoint[0] - (cp1[0] - midpoint[0]);
  corner[1] = midpoint[1] - (cp1[1] - midpoint[1]);

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    p2d->x = corner[0] + (end[0] - corner[0]) * sinf(a) + (start[0] - corner[0]) * cosf(a);
    p2d->y = corner[1] + (end[1] - corner[1]) * sinf(a) + (start[1] - corner[1]) * cosf(a);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
}

/* create a bezier */
static void gpencil_primitive_bezier(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = 1.0f / (float)(tgpi->tot_edges - 1);
  float bcp1[2];
  float bcp2[2];
  float bcp3[2];
  float bcp4[2];
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  copy_v2_v2(bcp1, tgpi->start);
  copy_v2_v2(bcp2, tgpi->cp1);
  copy_v2_v2(bcp3, tgpi->cp2);
  copy_v2_v2(bcp4, tgpi->end);

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    interp_v2_v2v2v2v2_cubic(&p2d->x, bcp1, bcp2, bcp3, bcp4, a);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
  gpencil_primitive_set_cp(tgpi, tgpi->cp2, color, BIG_SIZE_CTL * 0.9f);
}

/* create a circle */
static void gpencil_primitive_circle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = (2.0f * M_PI) / (float)(tgpi->tot_edges);
  float center[2];
  float radius[2];
  float a = 0.0f;

  center[0] = tgpi->start[0] + ((tgpi->end[0] - tgpi->start[0]) / 2.0f);
  center[1] = tgpi->start[1] + ((tgpi->end[1] - tgpi->start[1]) / 2.0f);
  radius[0] = fabsf(((tgpi->end[0] - tgpi->start[0]) / 2.0f));
  radius[1] = fabsf(((tgpi->end[1] - tgpi->start[1]) / 2.0f));

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    p2d->x = (center[0] + cosf(a) * radius[0]);
    p2d->y = (center[1] + sinf(a) * radius[1]);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gpencil_primitive_set_cp(tgpi, center, color, SMALL_SIZE_CTL);
}

/* Helper: Update shape of the stroke */
static void gpencil_primitive_update_strokes(bContext *C, tGPDprimitive *tgpi)
{
  ToolSettings *ts = tgpi->scene->toolsettings;
  bGPdata *gpd = tgpi->gpd;
  Brush *brush = tgpi->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  GpRandomSettings random_settings = tgpi->random_settings;
  bGPDstroke *gps = tgpi->gpf->strokes.first;
  GP_Sculpt_Settings *gset = &ts->gp_sculpt;
  int depth_margin = (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 4 : 0;
  const char align_flag = ts->gpencil_v3d_align;
  bool is_depth = (bool)(align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE));
  const bool is_lock_axis_view = (bool)(ts->gp_sculpt.lock_axis == 0);
  const bool is_camera = is_lock_axis_view && (tgpi->rv3d->persp == RV3D_CAMOB) && (!is_depth);

  if (tgpi->type == GP_STROKE_BOX) {
    tgpi->tot_edges--;
    gps->totpoints = (tgpi->tot_edges * 4 + tgpi->tot_stored_edges);
  }
  else {
    gps->totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  }

  if (tgpi->tot_stored_edges) {
    gps->totpoints--;
  }

  tgpi->gpd->runtime.tot_cp_points = 0;

  /* compute screen-space coordinates for points */
  tGPspoint *points2D = tgpi->points;

  if (tgpi->tot_edges > 0) {
    switch (tgpi->type) {
      case GP_STROKE_BOX:
        gpencil_primitive_rectangle(tgpi, points2D);
        break;
      case GP_STROKE_LINE:
        gpencil_primitive_line(tgpi, points2D, true);
        break;
      case GP_STROKE_POLYLINE:
        gpencil_primitive_line(tgpi, points2D, false);
        break;
      case GP_STROKE_CIRCLE:
        gpencil_primitive_circle(tgpi, points2D);
        break;
      case GP_STROKE_ARC:
        gpencil_primitive_arc(tgpi, points2D);
        break;
      case GP_STROKE_CURVE:
        gpencil_primitive_bezier(tgpi, points2D);
      default:
        break;
    }
  }

  /* convert screen-coordinates to 3D coordinates */
  gpencil_session_validatebuffer(tgpi);
  gpencil_init_colors(tgpi);
  if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
    BKE_curvemapping_init(ts->gp_sculpt.cur_primitive);
  }
  if (brush_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
    BKE_curvemapping_init(brush_settings->curve_jitter);
  }
  if (brush_settings->flag & GP_BRUSH_USE_STRENGTH_PRESSURE) {
    BKE_curvemapping_init(brush_settings->curve_strength);
  }

  /* get an array of depths, far depths are blended */
  float *depth_arr = NULL;
  if (is_depth) {
    int mval_i[2], mval_prev[2] = {0};
    bool interp_depth = false;
    bool found_depth = false;

    /* need to restore the original projection settings before packing up */
    view3d_region_operator_needs_opengl(tgpi->win, tgpi->region);
    ED_view3d_autodist_init(tgpi->depsgraph,
                            tgpi->region,
                            tgpi->v3d,
                            (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);

    depth_arr = MEM_mallocN(sizeof(float) * gps->totpoints, "depth_points");
    tGPspoint *ptc = &points2D[0];
    for (int i = 0; i < gps->totpoints; i++, ptc++) {
      round_v2i_v2fl(mval_i, &ptc->x);
      if ((ED_view3d_autodist_depth(tgpi->region, mval_i, depth_margin, depth_arr + i) == 0) &&
          (i && (ED_view3d_autodist_depth_seg(
                     tgpi->region, mval_i, mval_prev, depth_margin + 1, depth_arr + i) == 0))) {
        interp_depth = true;
      }
      else {
        found_depth = true;
      }
      copy_v2_v2_int(mval_prev, mval_i);
    }

    if (!found_depth) {
      for (int i = 0; i < gps->totpoints; i++) {
        depth_arr[i] = 0.9999f;
      }
    }
    else {
      /* if all depth are too high disable */
      bool valid_depth = false;
      for (int i = 0; i < gps->totpoints; i++) {
        if (depth_arr[i] < 0.9999f) {
          valid_depth = true;
          break;
        }
      }
      if (!valid_depth) {
        MEM_SAFE_FREE(depth_arr);
        is_depth = false;
      }
      else {
        if ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) &&
            ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_ENDPOINTS) ||
             (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_FIRST))) {
          int first_valid = 0;
          int last_valid = 0;

          /* find first valid contact point */
          int i;
          for (i = 0; i < gps->totpoints; i++) {
            if (depth_arr[i] != FLT_MAX) {
              break;
            }
          }
          first_valid = i;

          /* find last valid contact point */
          if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_FIRST) {
            last_valid = first_valid;
          }
          else {
            for (i = gps->totpoints - 1; i >= 0; i--) {
              if (depth_arr[i] != FLT_MAX) {
                break;
              }
            }
            last_valid = i;
          }

          /* invalidate any other point, to interpolate between
           * first and last contact in an imaginary line between them */
          for (i = 0; i < gps->totpoints; i++) {
            if (!ELEM(i, first_valid, last_valid)) {
              depth_arr[i] = FLT_MAX;
            }
          }
          interp_depth = true;
        }

        if (interp_depth) {
          interp_sparse_array(depth_arr, gps->totpoints, FLT_MAX);
        }
      }
    }
  }

  /* load stroke points and sbuffer */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    tGPspoint *p2d = &points2D[i];

    /* set rnd value for reuse */
    if ((brush_settings->flag & GP_BRUSH_GROUP_RANDOM) && (p2d->rnd_dirty != true)) {
      p2d->rnd[0] = BLI_rng_get_float(tgpi->rng);
      p2d->rnd[1] = BLI_rng_get_float(tgpi->rng);
      p2d->rnd_dirty = true;
    }

    /* Copy points to buffer */
    tGPspoint *tpt = ((tGPspoint *)(gpd->runtime.sbuffer) + gpd->runtime.sbuffer_used);

    /* Store original points */
    float tmp_xyp[2];
    copy_v2_v2(tmp_xyp, &p2d->x);

    /* calc pressure */
    float curve_pressure = 1.0;
    float pressure = 1.0;
    float strength = brush_settings->draw_strength;

    /* normalize value to evaluate curve */
    if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
      float value = (float)i / (gps->totpoints - 1);
      curve_pressure = BKE_curvemapping_evaluateF(gset->cur_primitive, 0, value);
      pressure = curve_pressure;
    }

    /* apply jitter to position */
    if ((brush_settings->flag & GP_BRUSH_GROUP_RANDOM) && (brush_settings->draw_jitter > 0.0f)) {
      float jitter;

      if (brush_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
        jitter = BKE_curvemapping_evaluateF(brush_settings->curve_jitter, 0, curve_pressure);
      }
      else {
        jitter = brush_settings->draw_jitter;
      }

      /* exponential value */
      const float exfactor = square_f(brush_settings->draw_jitter + 2.0f);
      const float fac = p2d->rnd[0] * exfactor * jitter;

      /* vector */
      float mvec[2], svec[2];
      if (i > 0) {
        mvec[0] = (p2d->x - (p2d - 1)->x);
        mvec[1] = (p2d->y - (p2d - 1)->y);
        normalize_v2(mvec);
      }
      else {
        zero_v2(mvec);
      }
      svec[0] = -mvec[1];
      svec[1] = mvec[0];

      if (p2d->rnd[1] > 0.5f) {
        mul_v2_fl(svec, -fac);
      }
      else {
        mul_v2_fl(svec, fac);
      }
      add_v2_v2(&p2d->x, svec);
    }

    /* color strength */
    if (brush_settings->flag & GP_BRUSH_USE_STRENGTH_PRESSURE) {
      float curvef = BKE_curvemapping_evaluateF(brush_settings->curve_strength, 0, curve_pressure);
      strength *= curvef;
      strength *= brush_settings->draw_strength;
    }

    CLAMP(strength, GPENCIL_STRENGTH_MIN, 1.0f);

    if (brush_settings->flag & GP_BRUSH_GROUP_RANDOM) {
      /* Apply randomness to pressure. */
      if (brush_settings->draw_random_press > 0.0f) {
        if ((brush_settings->flag2 & GP_BRUSH_USE_PRESS_AT_STROKE) == 0) {
          float rand = BLI_rng_get_float(tgpi->rng) * 2.0f - 1.0f;
          pressure *= 1.0 + rand * 2.0 * brush_settings->draw_random_press;
        }
        else {
          pressure *= 1.0 + random_settings.pressure * brush_settings->draw_random_press;
        }

        /* Apply random curve. */
        if (brush_settings->flag2 & GP_BRUSH_USE_PRESSURE_RAND_PRESS) {
          pressure *= BKE_curvemapping_evaluateF(brush_settings->curve_rand_pressure, 0, pressure);
        }

        CLAMP(pressure, 0.1f, 1.0f);
      }

      /* Apply randomness to color strength. */
      if (brush_settings->draw_random_strength) {
        if ((brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_AT_STROKE) == 0) {
          float rand = BLI_rng_get_float(tgpi->rng) * 2.0f - 1.0f;
          strength *= 1.0 + rand * brush_settings->draw_random_strength;
        }
        else {
          strength *= 1.0 + random_settings.strength * brush_settings->draw_random_strength;
        }

        /* Apply random curve. */
        if (brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_RAND_PRESS) {
          strength *= BKE_curvemapping_evaluateF(brush_settings->curve_rand_strength, 0, pressure);
        }

        CLAMP(strength, GPENCIL_STRENGTH_MIN, 1.0f);
      }
    }

    copy_v2_v2(&tpt->x, &p2d->x);

    tpt->pressure = pressure;
    tpt->strength = strength;
    tpt->time = p2d->time;

    /* Set vertex colors for buffer. */
    ED_gpencil_sbuffer_vertex_color_set(tgpi->depsgraph,
                                        tgpi->ob,
                                        tgpi->scene->toolsettings,
                                        tgpi->brush,
                                        tgpi->material,
                                        tgpi->random_settings.hsv,
                                        strength);

    /* point uv */
    if (gpd->runtime.sbuffer_used > 0) {
      tGPspoint *tptb = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
      bGPDspoint spt, spt2;

      /* get origin to reproject point */
      float origin[3];
      ED_gpencil_drawing_reference_get(tgpi->scene, tgpi->ob, ts->gpencil_v3d_align, origin);
      /* reproject current */
      ED_gpencil_tpoint_to_point(tgpi->region, origin, tpt, &spt);
      ED_gpencil_project_point_to_plane(
          tgpi->scene, tgpi->ob, tgpi->gpl, tgpi->rv3d, origin, tgpi->lock_axis - 1, &spt);

      /* reproject previous */
      ED_gpencil_tpoint_to_point(tgpi->region, origin, tptb, &spt2);
      ED_gpencil_project_point_to_plane(
          tgpi->scene, tgpi->ob, tgpi->gpl, tgpi->rv3d, origin, tgpi->lock_axis - 1, &spt2);
      tgpi->totpixlen += len_v3v3(&spt.x, &spt2.x);
      tpt->uv_fac = tgpi->totpixlen;
    }
    else {
      tgpi->totpixlen = 0.0f;
      tpt->uv_fac = 0.0f;
    }

    tpt->uv_rot = 0.0f;

    gpd->runtime.sbuffer_used++;

    /* check if still room in buffer or add more */
    gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
        gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

    /* add small offset to keep stroke over the surface */
    if ((depth_arr) && (gpd->zdepth_offset > 0.0f)) {
      depth_arr[i] *= (1.0f - (gpd->zdepth_offset / 1000.0f));
    }

    /* convert screen-coordinates to 3D coordinates */
    gpencil_stroke_convertcoords_tpoint(
        tgpi->scene, tgpi->region, tgpi->ob, p2d, depth_arr ? depth_arr + i : NULL, &pt->x);

    pt->pressure = pressure;
    pt->strength = strength;
    pt->time = 0.0f;
    pt->flag = 0;
    pt->uv_fac = tpt->uv_fac;
    pt->uv_rot = 0.0f;
    ED_gpencil_point_vertex_color_set(ts, brush, pt, tpt);

    if (gps->dvert != NULL) {
      MDeformVert *dvert = &gps->dvert[i];
      dvert->totweight = 0;
      dvert->dw = NULL;
    }

    /* Restore original points */
    copy_v2_v2(&p2d->x, tmp_xyp);
  }

  /* store cps and convert coords */
  if (tgpi->gpd->runtime.tot_cp_points > 0) {
    bGPDcontrolpoint *cps = tgpi->gpd->runtime.cp_points;
    for (int i = 0; i < tgpi->gpd->runtime.tot_cp_points; i++) {
      bGPDcontrolpoint *cp = &cps[i];
      gpencil_stroke_convertcoords_tpoint(
          tgpi->scene, tgpi->region, tgpi->ob, (tGPspoint *)cp, NULL, &cp->x);
    }
  }

  /* reproject to plane */
  if (!is_depth) {
    float origin[3];
    ED_gpencil_drawing_reference_get(tgpi->scene, tgpi->ob, ts->gpencil_v3d_align, origin);
    ED_gpencil_project_stroke_to_plane(
        tgpi->scene, tgpi->ob, tgpi->rv3d, tgpi->gpl, gps, origin, ts->gp_sculpt.lock_axis - 1);
  }

  /* if parented change position relative to parent object */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    gpencil_apply_parent_point(tgpi->depsgraph, tgpi->ob, tgpi->gpl, pt);
  }

  /* If camera view or view projection, reproject flat to view to avoid perspective effect. */
  if ((!is_depth) && (((align_flag & GP_PROJECT_VIEWSPACE) && is_lock_axis_view) || (is_camera))) {
    ED_gpencil_project_stroke_to_view(C, tgpi->gpl, gps);
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  /* Update evaluated data. */
  ED_gpencil_sbuffer_update_eval(tgpi->gpd, tgpi->ob_eval);

  MEM_SAFE_FREE(depth_arr);

  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

/* Update screen and stroke */
static void gpencil_primitive_update(bContext *C, wmOperator *op, tGPDprimitive *tgpi)
{
  /* update indicator in header */
  gpencil_primitive_status_indicators(C, tgpi);
  /* apply... */
  tgpi->type = RNA_enum_get(op->ptr, "type");
  tgpi->tot_edges = RNA_int_get(op->ptr, "edges");
  /* update points position */
  gpencil_primitive_update_strokes(C, tgpi);
}

/* Initialize mouse points. */
static void gpencil_primitive_interaction_begin(tGPDprimitive *tgpi, const wmEvent *event)
{
  copy_v2fl_v2i(tgpi->mval, event->mval);
  copy_v2_v2(tgpi->origin, tgpi->mval);
  copy_v2_v2(tgpi->start, tgpi->mval);
  copy_v2_v2(tgpi->end, tgpi->mval);
  copy_v2_v2(tgpi->cp1, tgpi->mval);
  copy_v2_v2(tgpi->cp2, tgpi->mval);
}

/* Exit and free memory */
static void gpencil_primitive_exit(bContext *C, wmOperator *op)
{
  tGPDprimitive *tgpi = op->customdata;
  bGPdata *gpd = tgpi->gpd;

  /* don't assume that operator data exists at all */
  if (tgpi) {
    /* clear status message area */
    ED_workspace_status_text(C, NULL);

    MEM_SAFE_FREE(tgpi->points);
    tgpi->gpd->runtime.tot_cp_points = 0;
    MEM_SAFE_FREE(tgpi->gpd->runtime.cp_points);
    /* finally, free memory used by temp data */
    BKE_gpencil_free_strokes(tgpi->gpf);
    MEM_SAFE_FREE(tgpi->gpf);

    /* free random seed */
    if (tgpi->rng != NULL) {
      BLI_rng_free(tgpi->rng);
    }

    MEM_freeN(tgpi);
  }

  /* free stroke buffer */
  if ((gpd != NULL) && (gpd->runtime.sbuffer)) {
    MEM_SAFE_FREE(gpd->runtime.sbuffer);
    gpd->runtime.sbuffer = NULL;

    /* clear flags */
    gpd->runtime.sbuffer_used = 0;
    gpd->runtime.sbuffer_size = 0;
    gpd->runtime.sbuffer_sflag = 0;
  }

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* clear pointer */
  op->customdata = NULL;
}

/* Init new temporary primitive data */
static void gpencil_primitive_init(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Paint *paint = &ts->gp_paint->paint;

  /* create temporary operator data */
  tGPDprimitive *tgpi = MEM_callocN(sizeof(tGPDprimitive), "GPencil Primitive Data");
  op->customdata = tgpi;

  tgpi->points = MEM_callocN(sizeof(tGPspoint), "gp primitive points2D");

  /* set current scene and window info */
  tgpi->bmain = CTX_data_main(C);
  tgpi->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  tgpi->scene = scene;
  tgpi->ob = CTX_data_active_object(C);
  tgpi->ob_eval = (Object *)DEG_get_evaluated_object(tgpi->depsgraph, tgpi->ob);
  tgpi->area = CTX_wm_area(C);
  tgpi->region = CTX_wm_region(C);
  tgpi->rv3d = tgpi->region->regiondata;
  tgpi->v3d = tgpi->area->spacedata.first;
  tgpi->win = CTX_wm_window(C);

  /* save original type */
  tgpi->orign_type = RNA_enum_get(op->ptr, "type");

  /* set current frame number */
  tgpi->cframe = CFRA;

  /* set GP datablock */
  tgpi->gpd = gpd;

  /* Setup space conversions. */
  gpencil_point_conversion_init(C, &tgpi->gsc);

  /* if brush doesn't exist, create a new set (fix damaged files from old versions) */
  if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
    BKE_brush_gpencil_paint_presets(bmain, ts, true);
  }

  /* Set Draw brush. */
  Brush *brush = BKE_paint_toolslots_brush_get(paint, 0);

  BKE_brush_tool_set(brush, paint, 0);
  BKE_paint_brush_set(paint, brush);
  tgpi->brush = brush;

  /* control points */
  tgpi->gpd->runtime.cp_points = MEM_callocN(sizeof(bGPDcontrolpoint) * MAX_CP,
                                             "gp primitive cpoint");
  tgpi->gpd->runtime.tot_cp_points = 0;

  /* getcolor info */
  tgpi->material = BKE_gpencil_object_material_ensure_from_active_input_toolsettings(
      bmain, tgpi->ob, ts);

  /* set parameters */
  tgpi->type = RNA_enum_get(op->ptr, "type");

  if (ELEM(tgpi->type, GP_STROKE_ARC, GP_STROKE_CURVE)) {
    tgpi->curve = true;
  }
  else {
    tgpi->curve = false;
  }

  tgpi->tot_stored_edges = 0;

  tgpi->subdiv = RNA_int_get(op->ptr, "subdivision");
  RNA_int_set(op->ptr, "edges", tgpi->subdiv + 2);
  tgpi->tot_edges = RNA_int_get(op->ptr, "edges");
  tgpi->flag = IDLE;
  tgpi->lock_axis = ts->gp_sculpt.lock_axis;

  /* set temp layer, frame and stroke */
  gpencil_primitive_set_initdata(C, tgpi);
}

/* Invoke handler: Initialize the operator */
static int gpencil_primitive_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  tGPDprimitive *tgpi = NULL;

  if (!IS_AUTOKEY_ON(scene)) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
    if ((gpl == NULL) || (gpl->actframe == NULL)) {
      BKE_report(op->reports, RPT_INFO, "No available frame for creating stroke");
      return OPERATOR_CANCELLED;
    }
  }

  /* initialize operator runtime data */
  gpencil_primitive_init(C, op);
  tgpi = op->customdata;

  /* Init random settings. */
  ED_gpencil_init_random_settings(tgpi->brush, event->mval, &tgpi->random_settings);

  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  if (!is_modal) {
    tgpi->flag = IN_PROGRESS;
    gpencil_primitive_interaction_begin(tgpi, event);
  }

  /* if in tools region, wait till we get to the main (3d-space)
   * region before allowing drawing to take place.
   */
  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  /* set cursor to indicate modal */
  WM_cursor_modal_set(win, WM_CURSOR_CROSS);

  /* Updates indicator in header. */
  gpencil_primitive_status_indicators(C, tgpi);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* add a modal handler for this operator */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Helper to complete a primitive */
static void gpencil_primitive_interaction_end(bContext *C,
                                              wmOperator *op,
                                              wmWindow *win,
                                              tGPDprimitive *tgpi)
{
  bGPDframe *gpf;
  bGPDstroke *gps;

  ToolSettings *ts = tgpi->scene->toolsettings;
  Brush *brush = tgpi->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;

  const int def_nr = tgpi->ob->actdef - 1;
  const bool have_weight = (bool)BLI_findlink(&tgpi->ob->defbase, def_nr);

  /* return to normal cursor and header status */
  ED_workspace_status_text(C, NULL);
  WM_cursor_modal_restore(win);

  /* insert keyframes as required... */
  short add_frame_mode;
  if (IS_AUTOKEY_ON(tgpi->scene)) {
    if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
      add_frame_mode = GP_GETFRAME_ADD_COPY;
    }
    else {
      add_frame_mode = GP_GETFRAME_ADD_NEW;
    }
  }
  else {
    add_frame_mode = GP_GETFRAME_USE_PREV;
  }

  bool need_tag = tgpi->gpl->actframe == NULL;
  gpf = BKE_gpencil_layer_frame_get(tgpi->gpl, tgpi->cframe, add_frame_mode);
  /* Only if there wasn't an active frame, need update. */
  if (need_tag) {
    DEG_id_tag_update(&tgpi->gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* prepare stroke to get transferred */
  gps = tgpi->gpf->strokes.first;
  if (gps) {
    gps->thickness = brush->size;
    gps->hardeness = brush_settings->hardeness;
    copy_v2_v2(gps->aspect_ratio, brush_settings->aspect_ratio);

    /* Calc geometry data. */
    BKE_gpencil_stroke_geometry_update(tgpi->gpd, gps);
  }

  /* transfer stroke from temporary buffer to the actual frame */
  if (ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) {
    BLI_movelisttolist_reverse(&gpf->strokes, &tgpi->gpf->strokes);
  }
  else {
    BLI_movelisttolist(&gpf->strokes, &tgpi->gpf->strokes);
  }
  BLI_assert(BLI_listbase_is_empty(&tgpi->gpf->strokes));

  /* add weights if required */
  if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
    BKE_gpencil_dvert_ensure(gps);
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformVert *ve = &gps->dvert[i];
      MDeformWeight *dw = BKE_defvert_ensure_index(ve, def_nr);
      if (dw) {
        dw->weight = ts->vgroup_weight;
      }
    }
  }

  /* Join previous stroke. */
  if (ts->gpencil_flags & GP_TOOL_FLAG_AUTOMERGE_STROKE) {
    if (ELEM(tgpi->type, GP_STROKE_ARC, GP_STROKE_LINE, GP_STROKE_CURVE, GP_STROKE_POLYLINE)) {
      if (gps->prev != NULL) {
        int pt_index = 0;
        bool doit = true;
        while (doit && gps) {
          bGPDstroke *gps_target = ED_gpencil_stroke_nearest_to_ends(
              C, &tgpi->gsc, tgpi->gpl, gpf, gps, GPENCIL_MINIMUM_JOIN_DIST, &pt_index);
          if (gps_target != NULL) {
            gps = ED_gpencil_stroke_join_and_trim(tgpi->gpd, gpf, gps, gps_target, pt_index);
          }
          else {
            doit = false;
          }
        }
      }
      ED_gpencil_stroke_close_by_distance(gps, 0.02f);
    }
    BKE_gpencil_stroke_geometry_update(tgpi->gpd, gps);
  }

  /* In Multiframe mode, duplicate the stroke in other frames. */
  if (GPENCIL_MULTIEDIT_SESSIONS_ON(tgpi->gpd)) {
    const bool tail = (ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK);
    BKE_gpencil_stroke_copy_to_keyframes(tgpi->gpd, tgpi->gpl, gpf, gps, tail);
  }

  DEG_id_tag_update(&tgpi->gpd->id, ID_RECALC_COPY_ON_WRITE);
  DEG_id_tag_update(&tgpi->gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* clean up temp data */
  gpencil_primitive_exit(C, op);
}

/* edit event handling */
static void gpencil_primitive_edit_event_handling(
    bContext *C, wmOperator *op, wmWindow *win, const wmEvent *event, tGPDprimitive *tgpi)
{
  /* calculate nearest point then set cursor */
  int move = MOVE_NONE;
  float a = len_v2v2(tgpi->mval, tgpi->start);
  float b = len_v2v2(tgpi->mval, tgpi->end);

  float c = len_v2v2(tgpi->mval, tgpi->cp1);
  float d = len_v2v2(tgpi->mval, tgpi->cp2);

  if (tgpi->flag == IN_CURVE_EDIT) {
    if ((a < BIG_SIZE_CTL && tgpi->tot_stored_edges == 0) || b < BIG_SIZE_CTL) {
      move = MOVE_ENDS;
      WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
    }
    else if (tgpi->curve) {
      move = MOVE_CP;
      WM_cursor_modal_set(win, WM_CURSOR_HAND);
    }
    else {
      WM_cursor_modal_set(win, WM_CURSOR_CROSS);
    }
  }
  else if (tgpi->flag == IN_PROGRESS) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  switch (event->type) {
    case LEFTMOUSE: {
      if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS)) {
        /* set control points and enter edit mode */
        if (ELEM(tgpi->type, GP_STROKE_POLYLINE)) {
          gpencil_primitive_add_segment(tgpi);
          copy_v2_v2(tgpi->start, tgpi->end);
          copy_v2_v2(tgpi->origin, tgpi->start);
          gpencil_primitive_update_cps(tgpi);

          tgpi->flag = IN_POLYLINE;
          WM_cursor_modal_set(win, WM_CURSOR_CROSS);
        }
        else {
          tgpi->flag = IN_CURVE_EDIT;
          gpencil_primitive_update_cps(tgpi);
          gpencil_primitive_update(C, op, tgpi);
        }
      }
      else if ((event->val == KM_PRESS) && !ELEM(tgpi->type, GP_STROKE_POLYLINE)) {
        /* find nearest cp based on stroke end points */
        if (move == MOVE_ENDS) {
          tgpi->sel_cp = (a < b) ? SELECT_START : SELECT_END;
        }
        else if (move == MOVE_CP) {
          tgpi->sel_cp = (c < d) ? SELECT_CP1 : SELECT_CP2;
        }
        else {
          tgpi->sel_cp = SELECT_NONE;
        }
        break;
      }
      else {
        tgpi->sel_cp = SELECT_NONE;
      }
      break;
    }
    case MOUSEMOVE: {
      if ((event->val == KM_PRESS) && tgpi->sel_cp != SELECT_NONE) {
        if (tgpi->sel_cp == SELECT_START && tgpi->tot_stored_edges == 0) {
          copy_v2_v2(tgpi->start, tgpi->mval);
        }
        else if (tgpi->sel_cp == SELECT_END) {
          copy_v2_v2(tgpi->end, tgpi->mval);
        }
        else if (tgpi->sel_cp == SELECT_CP1 ||
                 (tgpi->sel_cp == SELECT_CP2 && tgpi->type != GP_STROKE_CURVE)) {
          float dx = (tgpi->mval[0] - tgpi->mvalo[0]);
          float dy = (tgpi->mval[1] - tgpi->mvalo[1]);
          tgpi->cp1[0] += dx;
          tgpi->cp1[1] += dy;
          if (event->shift) {
            copy_v2_v2(tgpi->cp2, tgpi->cp1);
          }
        }
        else if (tgpi->sel_cp == SELECT_CP2) {
          float dx = (tgpi->mval[0] - tgpi->mvalo[0]);
          float dy = (tgpi->mval[1] - tgpi->mvalo[1]);
          tgpi->cp2[0] += dx;
          tgpi->cp2[1] += dy;
          if (event->shift) {
            copy_v2_v2(tgpi->cp1, tgpi->cp2);
          }
        }
        /* update screen */
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EVT_MKEY: {
      if ((event->val == KM_PRESS) && (tgpi->curve) && (ELEM(tgpi->orign_type, GP_STROKE_ARC))) {
        tgpi->flip ^= 1;
        gpencil_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EVT_EKEY: {
      if (tgpi->flag == IN_CURVE_EDIT && !ELEM(tgpi->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
        tgpi->flag = IN_PROGRESS;
        WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
        gpencil_primitive_add_segment(tgpi);
        copy_v2_v2(tgpi->start, tgpi->end);
        copy_v2_v2(tgpi->origin, tgpi->start);
        gpencil_primitive_update_cps(tgpi);
      }
      break;
    }
  }
}

/* brush strength */
static void gpencil_primitive_strength(tGPDprimitive *tgpi, bool reset)
{
  Brush *brush = tgpi->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;

  if (brush) {
    if (reset) {
      brush_settings->draw_strength = tgpi->brush_strength;
      tgpi->brush_strength = 0.0f;
    }
    else {
      if (tgpi->brush_strength == 0.0f) {
        tgpi->brush_strength = brush_settings->draw_strength;
      }
      float move[2];
      sub_v2_v2v2(move, tgpi->mval, tgpi->mvalo);
      float adjust = (move[1] > 0.0f) ? 0.01f : -0.01f;
      brush_settings->draw_strength += adjust * fabsf(len_manhattan_v2(move));
    }

    /* limit low limit because below 0.2f the stroke is invisible */
    CLAMP(brush_settings->draw_strength, 0.2f, 1.0f);
  }
}

/* brush size */
static void gpencil_primitive_size(tGPDprimitive *tgpi, bool reset)
{
  Brush *brush = tgpi->brush;
  if (brush) {
    if (reset) {
      brush->size = tgpi->brush_size;
      tgpi->brush_size = 0;
    }
    else {
      if (tgpi->brush_size == 0) {
        tgpi->brush_size = brush->size;
      }
      float move[2];
      sub_v2_v2v2(move, tgpi->mval, tgpi->mvalo);
      int adjust = (move[1] > 0.0f) ? 1 : -1;
      brush->size += adjust * (int)fabsf(len_manhattan_v2(move));
    }
    CLAMP_MIN(brush->size, 1);
  }
}

/* move */
static void gpencil_primitive_move(tGPDprimitive *tgpi, bool reset)
{
  float move[2];
  zero_v2(move);

  if (reset) {
    sub_v2_v2(move, tgpi->move);
    zero_v2(tgpi->move);
  }
  else {
    sub_v2_v2v2(move, tgpi->mval, tgpi->mvalo);
    add_v2_v2(tgpi->move, move);
  }

  bGPDstroke *gps = tgpi->gpf->strokes.first;
  tGPspoint *points2D = tgpi->points;

  for (int i = 0; i < gps->totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    add_v2_v2(&p2d->x, move);
  }

  add_v2_v2(tgpi->start, move);
  add_v2_v2(tgpi->end, move);
  add_v2_v2(tgpi->cp1, move);
  add_v2_v2(tgpi->cp2, move);
  add_v2_v2(tgpi->origin, move);
}

/* Modal handler: Events handling during interactive part */
static int gpencil_primitive_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPDprimitive *tgpi = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  const bool has_numinput = hasNumInput(&tgpi->num);

  copy_v2fl_v2i(tgpi->mval, event->mval);

  if (tgpi->flag == IN_MOVE) {

    switch (event->type) {
      case MOUSEMOVE: {
        gpencil_primitive_move(tgpi, false);
        gpencil_primitive_update(C, op, tgpi);
        break;
      }
      case EVT_ESCKEY:
      case LEFTMOUSE: {
        zero_v2(tgpi->move);
        tgpi->flag = IN_CURVE_EDIT;
        break;
      }
      case RIGHTMOUSE: {
        if (event->val == KM_RELEASE) {
          tgpi->flag = IN_CURVE_EDIT;
          gpencil_primitive_move(tgpi, true);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
      }
    }
    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }

  if (tgpi->flag == IN_POLYLINE) {

    switch (event->type) {

      case EVT_ESCKEY: {
        /* return to normal cursor and header status */
        ED_workspace_status_text(C, NULL);
        WM_cursor_modal_restore(win);

        /* clean up temp data */
        gpencil_primitive_exit(C, op);

        /* canceled! */
        return OPERATOR_CANCELLED;
      }
      case LEFTMOUSE: {
        if (event->val == KM_PRESS) {
          WM_cursor_modal_set(win, WM_CURSOR_CROSS);
          gpencil_primitive_add_segment(tgpi);

          gpencil_primitive_update(C, op, tgpi);
          copy_v2_v2(tgpi->start, tgpi->end);
          copy_v2_v2(tgpi->origin, tgpi->end);
        }
        break;
      }
      case EVT_SPACEKEY: /* confirm */
      case MIDDLEMOUSE:
      case EVT_RETKEY:
      case RIGHTMOUSE: {
        if (event->val == KM_PRESS) {
          tgpi->flag = IDLE;
          int last_edges = tgpi->tot_edges;
          tgpi->tot_edges = tgpi->tot_stored_edges ? 1 : 0;
          RNA_int_set(op->ptr, "edges", tgpi->tot_edges);
          gpencil_primitive_update_strokes(C, tgpi);
          gpencil_primitive_interaction_end(C, op, win, tgpi);
          RNA_int_set(op->ptr, "edges", last_edges);
          return OPERATOR_FINISHED;
        }
        break;
      }
      case MOUSEMOVE: {
        WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
        copy_v2_v2(tgpi->end, tgpi->mval);

        if (event->shift) {
          gpencil_primitive_constrain(tgpi, true);
        }

        gpencil_primitive_update(C, op, tgpi);
        break;
      }
      case EVT_PADPLUSKEY:
      case WHEELUPMOUSE: {
        if ((event->val != KM_RELEASE)) {
          tgpi->tot_edges = tgpi->tot_edges + 1;
          CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
          RNA_int_set(op->ptr, "edges", tgpi->tot_edges);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
      }
      case EVT_PADMINUS:
      case WHEELDOWNMOUSE: {
        if ((event->val != KM_RELEASE)) {
          tgpi->tot_edges = tgpi->tot_edges - 1;
          CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
          RNA_int_set(op->ptr, "edges", tgpi->tot_edges);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
      }
      case EVT_FKEY: /* brush thickness/ brush strength */
      {
        if ((event->val == KM_PRESS)) {
          if (event->shift) {
            tgpi->prev_flag = tgpi->flag;
            tgpi->flag = IN_BRUSH_STRENGTH;
          }
          else {
            tgpi->prev_flag = tgpi->flag;
            tgpi->flag = IN_BRUSH_SIZE;
          }
          WM_cursor_modal_set(win, WM_CURSOR_NS_SCROLL);
        }
        break;
      }
    }

    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }

  if (tgpi->flag == IN_BRUSH_SIZE) {
    switch (event->type) {
      case MOUSEMOVE:
        gpencil_primitive_size(tgpi, false);
        gpencil_primitive_update(C, op, tgpi);
        break;
      case EVT_ESCKEY:
      case MIDDLEMOUSE:
      case LEFTMOUSE:
        tgpi->brush_size = 0;
        tgpi->flag = tgpi->prev_flag;
        break;
      case RIGHTMOUSE:
        if (event->val == KM_RELEASE) {
          tgpi->flag = tgpi->prev_flag;
          gpencil_primitive_size(tgpi, true);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
    }
    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }

  if (tgpi->flag == IN_BRUSH_STRENGTH) {
    switch (event->type) {
      case MOUSEMOVE:
        gpencil_primitive_strength(tgpi, false);
        gpencil_primitive_update(C, op, tgpi);
        break;
      case EVT_ESCKEY:
      case MIDDLEMOUSE:
      case LEFTMOUSE:
        tgpi->brush_strength = 0.0f;
        tgpi->flag = tgpi->prev_flag;
        break;
      case RIGHTMOUSE:
        if (event->val == KM_RELEASE) {
          tgpi->flag = tgpi->prev_flag;
          gpencil_primitive_strength(tgpi, true);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
    }
    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }

  if (!ELEM(tgpi->flag, IDLE) && !ELEM(tgpi->type, GP_STROKE_POLYLINE)) {
    gpencil_primitive_edit_event_handling(C, op, win, event, tgpi);
  }

  switch (event->type) {
    case LEFTMOUSE: {
      if ((event->val == KM_PRESS) && (tgpi->flag == IDLE)) {
        /* start drawing primitive */
        /* TODO: Ignore if not in main region yet */
        tgpi->flag = IN_PROGRESS;
        gpencil_primitive_interaction_begin(tgpi, event);
        if (ELEM(tgpi->type, GP_STROKE_POLYLINE)) {
          tgpi->flag = IN_POLYLINE;
          gpencil_primitive_update(C, op, tgpi);
          return OPERATOR_RUNNING_MODAL;
        }
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS) &&
               (!ELEM(tgpi->type, GP_STROKE_POLYLINE))) {
        /* set control points and enter edit mode */
        tgpi->flag = IN_CURVE_EDIT;
        gpencil_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS) &&
               (!ELEM(tgpi->type, GP_STROKE_CURVE, GP_STROKE_POLYLINE))) {
        /* stop drawing primitive */
        tgpi->flag = IDLE;
        gpencil_primitive_interaction_end(C, op, win, tgpi);
        /* done! */
        return OPERATOR_FINISHED;
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS) &&
               (ELEM(tgpi->type, GP_STROKE_POLYLINE))) {
        /* set control points and enter edit mode */
        tgpi->flag = IN_POLYLINE;
        gpencil_primitive_update(C, op, tgpi);
        copy_v2_v2(tgpi->mvalo, tgpi->mval);
        return OPERATOR_RUNNING_MODAL;
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_MOVE)) {
        tgpi->flag = IN_CURVE_EDIT;
      }
      else {
        if (G.debug & G_DEBUG) {
          printf("GP Add Primitive Modal: LEFTMOUSE %d, Status = %d\n", event->val, tgpi->flag);
        }
      }
      break;
    }
    case EVT_SPACEKEY: /* confirm */
    case MIDDLEMOUSE:
    case EVT_PADENTER:
    case EVT_RETKEY: {
      tgpi->flag = IDLE;
      gpencil_primitive_interaction_end(C, op, win, tgpi);
      /* done! */
      return OPERATOR_FINISHED;
    }
    case RIGHTMOUSE: {
      /* exception to cancel current stroke when we have previous strokes in buffer */
      if (tgpi->tot_stored_edges > 0) {
        tgpi->flag = IDLE;
        tgpi->tot_edges = tgpi->tot_stored_edges ? 1 : 0;
        gpencil_primitive_update_strokes(C, tgpi);
        gpencil_primitive_interaction_end(C, op, win, tgpi);
        /* done! */
        return OPERATOR_FINISHED;
      }
      ATTR_FALLTHROUGH;
    }
    case EVT_ESCKEY: {
      /* return to normal cursor and header status */
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* clean up temp data */
      gpencil_primitive_exit(C, op);

      /* canceled! */
      return OPERATOR_CANCELLED;
    }
    case EVT_PADPLUSKEY:
    case WHEELUPMOUSE: {
      if ((event->val != KM_RELEASE)) {
        tgpi->tot_edges = tgpi->tot_edges + 1;
        CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
        RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

        /* update screen */
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EVT_PADMINUS:
    case WHEELDOWNMOUSE: {
      if ((event->val != KM_RELEASE)) {
        tgpi->tot_edges = tgpi->tot_edges - 1;
        CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
        RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

        /* update screen */
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EVT_GKEY: /* grab mode */
    {
      if ((event->val == KM_PRESS)) {
        tgpi->flag = IN_MOVE;
        WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
      }
      break;
    }
    case EVT_FKEY: /* brush thickness/ brush strength */
    {
      if ((event->val == KM_PRESS)) {
        if (event->shift) {
          tgpi->prev_flag = tgpi->flag;
          tgpi->flag = IN_BRUSH_STRENGTH;
        }
        else {
          tgpi->prev_flag = tgpi->flag;
          tgpi->flag = IN_BRUSH_SIZE;
        }
        WM_cursor_modal_set(win, WM_CURSOR_NS_SCROLL);
      }
      break;
    }
    case EVT_CKEY: /* curve mode */
    {
      if ((event->val == KM_PRESS) && (tgpi->orign_type == GP_STROKE_CURVE)) {
        switch (tgpi->type) {
          case GP_STROKE_CURVE:
            tgpi->type = GP_STROKE_ARC;
            break;
          default:
          case GP_STROKE_ARC:
            tgpi->type = GP_STROKE_CURVE;
            break;
        }

        RNA_enum_set(op->ptr, "type", tgpi->type);
        gpencil_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EVT_TABKEY: {
      if (tgpi->flag == IN_CURVE_EDIT) {
        tgpi->flag = IN_PROGRESS;
        WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
        gpencil_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case MOUSEMOVE: /* calculate new position */
    {
      if (ELEM(tgpi->flag, IN_CURVE_EDIT)) {
        break;
      }
      /* only handle mousemove if not doing numinput */
      if (has_numinput == false) {
        /* update position of mouse */
        copy_v2_v2(tgpi->end, tgpi->mval);
        copy_v2_v2(tgpi->start, tgpi->origin);
        if (tgpi->flag == IDLE) {
          copy_v2_v2(tgpi->origin, tgpi->mval);
        }
        /* Keep square if shift key */
        if (event->shift) {
          gpencil_primitive_constrain(
              tgpi, (ELEM(tgpi->type, GP_STROKE_LINE, GP_STROKE_POLYLINE) || tgpi->curve));
        }
        /* Center primitive if alt key */
        if (event->alt && !ELEM(tgpi->type, GP_STROKE_POLYLINE)) {
          tgpi->start[0] = tgpi->origin[0] - (tgpi->end[0] - tgpi->origin[0]);
          tgpi->start[1] = tgpi->origin[1] - (tgpi->end[1] - tgpi->origin[1]);
        }
        gpencil_primitive_update_cps(tgpi);
        /* update screen */
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    default: {
      if (tgpi->flag != IN_CURVE_EDIT && (event->val == KM_PRESS) &&
          handleNumInput(C, &tgpi->num, event)) {
        float value;

        /* Grab data from numeric input, and store this new value (the user see an int) */
        value = tgpi->tot_edges;
        applyNumInput(&tgpi->num, &value);
        tgpi->tot_edges = value;

        CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
        RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

        /* update screen */
        gpencil_primitive_update(C, op, tgpi);

        break;
      }

      /* unhandled event - allow to pass through */
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }
  }

  copy_v2_v2(tgpi->mvalo, tgpi->mval);
  /* still running... */
  return OPERATOR_RUNNING_MODAL;
}

/* Cancel handler */
static void gpencil_primitive_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  gpencil_primitive_exit(C, op);
}

static void gpencil_primitive_common_props(wmOperatorType *ot, int subdiv, int type)
{
  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna,
                     "subdivision",
                     subdiv,
                     0,
                     MAX_EDGES,
                     "Subdivisions",
                     "Number of subdivision by edges",
                     0,
                     MAX_EDGES);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Internal prop. */
  prop = RNA_def_int(ot->srna,
                     "edges",
                     MIN_EDGES,
                     MIN_EDGES,
                     MAX_EDGES,
                     "Edges",
                     "Number of points by edge",
                     MIN_EDGES,
                     MAX_EDGES);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  RNA_def_enum(ot->srna, "type", gpencil_primitive_type, type, "Type", "Type of shape");

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void GPENCIL_OT_primitive_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Box Shape";
  ot->idname = "GPENCIL_OT_primitive_box";
  ot->description = "Create predefined grease pencil stroke box shapes";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  gpencil_primitive_common_props(ot, 3, GP_STROKE_BOX);
}

void GPENCIL_OT_primitive_line(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Line Shape";
  ot->idname = "GPENCIL_OT_primitive_line";
  ot->description = "Create predefined grease pencil stroke lines";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  gpencil_primitive_common_props(ot, 6, GP_STROKE_LINE);
}

void GPENCIL_OT_primitive_polyline(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Polyline Shape";
  ot->idname = "GPENCIL_OT_primitive_polyline";
  ot->description = "Create predefined grease pencil stroke polylines";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  gpencil_primitive_common_props(ot, 6, GP_STROKE_POLYLINE);
}

void GPENCIL_OT_primitive_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Circle Shape";
  ot->idname = "GPENCIL_OT_primitive_circle";
  ot->description = "Create predefined grease pencil stroke circle shapes";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  gpencil_primitive_common_props(ot, 94, GP_STROKE_CIRCLE);
}

void GPENCIL_OT_primitive_curve(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Curve Shape";
  ot->idname = "GPENCIL_OT_primitive_curve";
  ot->description = "Create predefined grease pencil stroke curve shapes";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  gpencil_primitive_common_props(ot, 62, GP_STROKE_CURVE);
}
