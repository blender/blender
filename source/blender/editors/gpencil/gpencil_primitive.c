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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rand.h"

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
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_space_api.h"

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

/* clear the session buffers (call this before AND after a paint operation) */
static void gp_session_validatebuffer(tGPDprimitive *p)
{
  bGPdata *gpd = p->gpd;

  /* clear memory of buffer (or allocate it if starting a new session) */
  if (gpd->runtime.sbuffer) {
    memset(gpd->runtime.sbuffer, 0, sizeof(tGPspoint) * GP_STROKE_BUFFER_MAX);
  }
  else {
    gpd->runtime.sbuffer = MEM_callocN(sizeof(tGPspoint) * GP_STROKE_BUFFER_MAX,
                                       "gp_session_strokebuffer");
  }

  /* reset indices */
  gpd->runtime.sbuffer_size = 0;

  /* reset flags */
  gpd->runtime.sbuffer_sflag = 0;
  gpd->runtime.sbuffer_sflag |= GP_STROKE_3DSPACE;

  if (ELEM(p->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
    gpd->runtime.sbuffer_sflag |= GP_STROKE_CYCLIC;
  }
}

static void gp_init_colors(tGPDprimitive *p)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;

  MaterialGPencilStyle *gp_style = NULL;

  /* use brush material */
  p->mat = BKE_gpencil_object_material_ensure_from_active_input_brush(p->bmain, p->ob, brush);

  /* assign color information to temp data */
  gp_style = p->mat->gp_style;
  if (gp_style) {

    /* set colors */
    if (gp_style->flag & GP_STYLE_STROKE_SHOW) {
      copy_v4_v4(gpd->runtime.scolor, gp_style->stroke_rgba);
    }
    else {
      /* if no stroke, use fill */
      copy_v4_v4(gpd->runtime.scolor, gp_style->fill_rgba);
    }

    copy_v4_v4(gpd->runtime.sfill, gp_style->fill_rgba);
    /* add some alpha to make easy the filling without hide strokes */
    if (gpd->runtime.sfill[3] > 0.8f) {
      gpd->runtime.sfill[3] = 0.8f;
    }

    gpd->runtime.mode = (short)gp_style->mode;
    gpd->runtime.bstroke_style = gp_style->stroke_style;
    gpd->runtime.bfill_style = gp_style->fill_style;
  }
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

/* Helper to rotate point around origin */
static void gp_rotate_v2_v2v2fl(float v[2],
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

/* Helper to rotate line around line centre */
static void gp_primitive_rotate_line(
    float va[2], float vb[2], const float a[2], const float b[2], const float angle)
{
  float midpoint[2];
  mid_v2_v2v2(midpoint, a, b);
  gp_rotate_v2_v2v2fl(va, a, midpoint, angle);
  gp_rotate_v2_v2v2fl(vb, b, midpoint, angle);
}

/* Helper to update cps */
static void gp_primitive_update_cps(tGPDprimitive *tgpi)
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
      gp_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->start, tgpi->end, M_PI_2);
    }
    else {
      gp_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->end, tgpi->start, M_PI_2);
    }
  }
}

/* Helper to reflect point */
static void UNUSED_FUNCTION(gp_reflect_point_v2_v2v2v2)(float va[2],
                                                        const float p[2],
                                                        const float a[2],
                                                        const float b[2])
{
  float point[2];
  closest_to_line_v2(point, p, a, b);
  va[0] = point[0] - (p[0] - point[0]);
  va[1] = point[1] - (p[1] - point[1]);
}

/* Poll callback for primitive operators */
static bool gpencil_primitive_add_poll(bContext *C)
{
  /* only 3D view */
  ScrArea *sa = CTX_wm_area(C);
  if (sa && sa->spacetype != SPACE_VIEW3D) {
    return 0;
  }

  /* need data to create primitive */
  bGPdata *gpd = CTX_data_gpencil_data(C);
  if (gpd == NULL) {
    return 0;
  }

  /* only in edit and paint modes
   * - paint as it's the "drawing/creation mode"
   * - edit as this is more of an atomic editing operation
   *   (similar to copy/paste), and also for consistency
   */
  if ((gpd->flag & (GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE)) == 0) {
    CTX_wm_operator_poll_msg_set(C, "Primitives can only be added in Draw or Edit modes");
    return 0;
  }

  /* don't allow operator to function if the active layer is locked/hidden
   * (BUT, if there isn't an active layer, we are free to add new layer when the time comes)
   */
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
  if ((gpl) && (gpl->flag & (GP_LAYER_LOCKED | GP_LAYER_HIDE))) {
    CTX_wm_operator_poll_msg_set(C,
                                 "Primitives cannot be added as active layer is locked or hidden");
    return 0;
  }

  return 1;
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
static void gp_primitive_set_initdata(bContext *C, tGPDprimitive *tgpi)
{
  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;

  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  /* if layer doesn't exist, create a new one */
  if (gpl == NULL) {
    gpl = BKE_gpencil_layer_addnew(tgpi->gpd, DATA_("Primitives"), true);
  }
  tgpi->gpl = gpl;

  /* create a new temporary frame */
  tgpi->gpf = MEM_callocN(sizeof(bGPDframe), "Temp bGPDframe");
  tgpi->gpf->framenum = tgpi->cframe = cfra_eval;

  /* create new temp stroke */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "Temp bGPDstroke");
  gps->thickness = 2.0f;
  gps->gradient_f = 1.0f;
  gps->gradient_s[0] = 1.0f;
  gps->gradient_s[1] = 1.0f;
  gps->inittime = 0.0f;

  /* enable recalculation flag by default */
  gps->flag |= GP_STROKE_RECALC_GEOMETRY;
  gps->flag &= ~GP_STROKE_SELECT;
  /* the polygon must be closed, so enabled cyclic */
  if (ELEM(tgpi->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
    gps->flag |= GP_STROKE_CYCLIC;
  }

  gps->flag |= GP_STROKE_3DSPACE;

  gps->mat_nr = BKE_gpencil_object_material_get_index(tgpi->ob, tgpi->mat);

  /* allocate memory for storage points, but keep empty */
  gps->totpoints = 0;
  gps->points = MEM_callocN(sizeof(bGPDspoint), "gp_stroke_points");
  /* initialize triangle memory to dummy data */
  gps->tot_triangles = 0;
  gps->triangles = NULL;
  gps->flag |= GP_STROKE_RECALC_GEOMETRY;

  /* add to strokes */
  BLI_addtail(&tgpi->gpf->strokes, gps);

  /* allocate memory for storage points */
  gpencil_primitive_allocate_memory(tgpi);

  /* Random generator, only init once. */
  uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
  tgpi->rng = BLI_rng_new(rng_seed);
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
static void gp_primitive_set_cp(tGPDprimitive *tgpi, float p[2], float color[4], int size)
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
  else if (tgpi->type == GP_STROKE_BOX) {
    BLI_strncpy(msg_str,
                TIP_("Rectangle: ESC to cancel, LMB set origin, Enter/MMB to confirm, WHEEL/+- "
                     "to adjust subdivision number, Shift to square, Alt to center"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_CIRCLE) {
    BLI_strncpy(msg_str,
                TIP_("Circle: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust edge "
                     "number, Shift to square, Alt to center"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_ARC) {
    BLI_strncpy(msg_str,
                TIP_("Arc: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust edge number, "
                     "Shift to square, Alt to center, M: Flip, E: extrude"),
                UI_MAX_DRAW_STR);
  }
  else if (tgpi->type == GP_STROKE_CURVE) {
    BLI_strncpy(msg_str,
                TIP_("Curve: ESC to cancel, Enter/MMB to confirm, WHEEL/+- to adjust edge "
                     "number, Shift to square, Alt to center, E: extrude"),
                UI_MAX_DRAW_STR);
  }

  if (ELEM(tgpi->type, GP_STROKE_CIRCLE, GP_STROKE_ARC, GP_STROKE_LINE, GP_STROKE_BOX)) {
    if (hasNumInput(&tgpi->num)) {
      char str_offs[NUM_STR_REP_LEN];

      outputNumInput(&tgpi->num, str_offs, &scene->unit);
      BLI_snprintf(status_str, sizeof(status_str), "%s: %s", msg_str, str_offs);
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
static void gp_primitive_rectangle(tGPDprimitive *tgpi, tGPspoint *points2D)
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

  mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gp_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gp_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gp_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gp_primitive_set_cp(tgpi, tgpi->midpoint, color, SMALL_SIZE_CTL);
}

/* create a line */
static void gp_primitive_line(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = 1.0f / (float)(tgpi->tot_edges - 1);
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    interp_v2_v2v2(&p2d->x, tgpi->start, tgpi->end, a);
    a += step;
  }

  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gp_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gp_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gp_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
}

/* create an arc */
static void gp_primitive_arc(tGPDprimitive *tgpi, tGPspoint *points2D)
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
  gp_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gp_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gp_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gp_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
}

/* create a bezier */
static void gp_primitive_bezier(tGPDprimitive *tgpi, tGPspoint *points2D)
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
  gp_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gp_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gp_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gp_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
  gp_primitive_set_cp(tgpi, tgpi->cp2, color, BIG_SIZE_CTL * 0.9f);
}

/* create a circle */
static void gp_primitive_circle(tGPDprimitive *tgpi, tGPspoint *points2D)
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
  gp_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  gp_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gp_primitive_set_cp(tgpi, center, color, SMALL_SIZE_CTL);
}

/* Helper: Update shape of the stroke */
static void gp_primitive_update_strokes(bContext *C, tGPDprimitive *tgpi)
{
  ToolSettings *ts = tgpi->scene->toolsettings;
  bGPdata *gpd = tgpi->gpd;
  Brush *brush = tgpi->brush;
  bGPDstroke *gps = tgpi->gpf->strokes.first;
  GP_Sculpt_Settings *gset = &ts->gp_sculpt;
  int depth_margin = (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 4 : 0;
  const char *align_flag = &ts->gpencil_v3d_align;
  bool is_depth = (bool)(*align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE));
  const bool is_camera = (bool)(ts->gp_sculpt.lock_axis == 0) &&
                         (tgpi->rv3d->persp == RV3D_CAMOB) && (!is_depth);

  if (tgpi->type == GP_STROKE_BOX) {
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

  if (tgpi->tot_edges > 1) {
    switch (tgpi->type) {
      case GP_STROKE_BOX:
        gp_primitive_rectangle(tgpi, points2D);
        break;
      case GP_STROKE_LINE:
        gp_primitive_line(tgpi, points2D);
        break;
      case GP_STROKE_CIRCLE:
        gp_primitive_circle(tgpi, points2D);
        break;
      case GP_STROKE_ARC:
        gp_primitive_arc(tgpi, points2D);
        break;
      case GP_STROKE_CURVE:
        gp_primitive_bezier(tgpi, points2D);
      default:
        break;
    }
  }

  /* convert screen-coordinates to 3D coordinates */
  gp_session_validatebuffer(tgpi);
  gp_init_colors(tgpi);
  if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
    curvemapping_initialize(ts->gp_sculpt.cur_primitive);
  }
  if (tgpi->brush->gpencil_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
    curvemapping_initialize(tgpi->brush->gpencil_settings->curve_jitter);
  }
  if (tgpi->brush->gpencil_settings->flag & GP_BRUSH_USE_STENGTH_PRESSURE) {
    curvemapping_initialize(tgpi->brush->gpencil_settings->curve_strength);
  }

  /* get an array of depths, far depths are blended */
  float *depth_arr = NULL;
  if (is_depth) {
    int i;
    int mval_i[2], mval_prev[2] = {0};
    bool interp_depth = false;
    bool found_depth = false;

    /* need to restore the original projection settings before packing up */
    view3d_region_operator_needs_opengl(tgpi->win, tgpi->ar);
    ED_view3d_autodist_init(tgpi->depsgraph,
                            tgpi->ar,
                            tgpi->v3d,
                            (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);

    depth_arr = MEM_mallocN(sizeof(float) * gps->totpoints, "depth_points");
    tGPspoint *ptc = &points2D[0];
    for (i = 0; i < gps->totpoints; i++, ptc++) {
      round_v2i_v2fl(mval_i, &ptc->x);
      if ((ED_view3d_autodist_depth(tgpi->ar, mval_i, depth_margin, depth_arr + i) == 0) &&
          (i && (ED_view3d_autodist_depth_seg(
                     tgpi->ar, mval_i, mval_prev, depth_margin + 1, depth_arr + i) == 0))) {
        interp_depth = true;
      }
      else {
        found_depth = true;
      }
      copy_v2_v2_int(mval_prev, mval_i);
    }

    if (!found_depth) {
      for (i = 0; i < gps->totpoints; i++) {
        depth_arr[i] = 0.9999f;
      }
    }
    else {
      /* if all depth are too high disable */
      bool valid_depth = false;
      for (i = 0; i < gps->totpoints; i++) {
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
            if ((i != first_valid) && (i != last_valid)) {
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
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) && (p2d->rnd_dirty != true)) {
      p2d->rnd[0] = BLI_rng_get_float(tgpi->rng);
      p2d->rnd[1] = BLI_rng_get_float(tgpi->rng);
      p2d->rnd[2] = BLI_rng_get_float(tgpi->rng);
      p2d->rnd_dirty = true;
    }

    /* Copy points to buffer */
    tGPspoint *tpt = ((tGPspoint *)(gpd->runtime.sbuffer) + gpd->runtime.sbuffer_size);

    /* Store original points */
    float tmp_xyp[2];
    copy_v2_v2(tmp_xyp, &p2d->x);

    /* calc pressure */
    float curve_pressure = 1.0;
    float pressure = 1.0;
    float strength = brush->gpencil_settings->draw_strength;

    /* normalize value to evaluate curve */
    if (gset->flag & GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE) {
      float value = (float)i / (gps->totpoints - 1);
      curve_pressure = curvemapping_evaluateF(gset->cur_primitive, 0, value);
      pressure = curve_pressure;
    }

    /* apply jitter to position */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
        (brush->gpencil_settings->draw_jitter > 0.0f)) {
      float jitter;

      if (brush->gpencil_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
        jitter = curvemapping_evaluateF(brush->gpencil_settings->curve_jitter, 0, curve_pressure);
        jitter *= brush->gpencil_settings->draw_sensitivity;
      }
      else {
        jitter = brush->gpencil_settings->draw_jitter;
      }

      /* exponential value */
      const float exfactor = SQUARE(brush->gpencil_settings->draw_jitter + 2.0f);
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

    /* apply randomness to pressure */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
        (brush->gpencil_settings->draw_random_press > 0.0f)) {
      if (p2d->rnd[0] > 0.5f) {
        pressure -= brush->gpencil_settings->draw_random_press * p2d->rnd[1];
      }
      else {
        pressure += brush->gpencil_settings->draw_random_press * p2d->rnd[2];
      }
    }

    /* color strength */
    if (brush->gpencil_settings->flag & GP_BRUSH_USE_STENGTH_PRESSURE) {
      float curvef = curvemapping_evaluateF(
          brush->gpencil_settings->curve_strength, 0, curve_pressure);
      strength *= curvef * brush->gpencil_settings->draw_sensitivity;
      strength *= brush->gpencil_settings->draw_strength;
    }

    CLAMP(strength, GPENCIL_STRENGTH_MIN, 1.0f);

    /* apply randomness to color strength */
    if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
        (brush->gpencil_settings->draw_random_strength > 0.0f)) {
      if (p2d->rnd[2] > 0.5f) {
        strength -= strength * brush->gpencil_settings->draw_random_strength * p2d->rnd[0];
      }
      else {
        strength += strength * brush->gpencil_settings->draw_random_strength * p2d->rnd[1];
      }
      CLAMP(strength, GPENCIL_STRENGTH_MIN, 1.0f);
    }

    copy_v2_v2(&tpt->x, &p2d->x);

    CLAMP_MIN(pressure, 0.1f);

    tpt->pressure = pressure;
    tpt->strength = strength;
    tpt->time = p2d->time;

    /* point uv */
    if (gpd->runtime.sbuffer_size > 0) {
      MaterialGPencilStyle *gp_style = tgpi->mat->gp_style;
      const float pixsize = gp_style->texture_pixsize / 1000000.0f;
      tGPspoint *tptb = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_size - 1;
      bGPDspoint spt, spt2;

      /* get origin to reproject point */
      float origin[3];
      ED_gp_get_drawing_reference(tgpi->scene, tgpi->ob, tgpi->gpl, ts->gpencil_v3d_align, origin);
      /* reproject current */
      ED_gpencil_tpoint_to_point(tgpi->ar, origin, tpt, &spt);
      ED_gp_project_point_to_plane(
          tgpi->scene, tgpi->ob, tgpi->rv3d, origin, tgpi->lock_axis - 1, &spt);

      /* reproject previous */
      ED_gpencil_tpoint_to_point(tgpi->ar, origin, tptb, &spt2);
      ED_gp_project_point_to_plane(
          tgpi->scene, tgpi->ob, tgpi->rv3d, origin, tgpi->lock_axis - 1, &spt2);
      tgpi->totpixlen += len_v3v3(&spt.x, &spt2.x) / pixsize;
      tpt->uv_fac = tgpi->totpixlen;
      if ((gp_style) && (gp_style->sima)) {
        tpt->uv_fac /= gp_style->sima->gen_x;
      }
    }
    else {
      tgpi->totpixlen = 0.0f;
      tpt->uv_fac = 0.0f;
    }

    tpt->uv_rot = p2d->uv_rot;

    gpd->runtime.sbuffer_size++;

    /* add small offset to keep stroke over the surface */
    if ((depth_arr) && (gpd->zdepth_offset > 0.0f)) {
      depth_arr[i] *= (1.0f - gpd->zdepth_offset);
    }

    /* convert screen-coordinates to 3D coordinates */
    gp_stroke_convertcoords_tpoint(
        tgpi->scene, tgpi->ar, tgpi->ob, tgpi->gpl, p2d, depth_arr ? depth_arr + i : NULL, &pt->x);

    pt->pressure = pressure;
    pt->strength = strength;
    pt->time = 0.0f;
    pt->flag = 0;
    pt->uv_fac = tpt->uv_fac;

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
      gp_stroke_convertcoords_tpoint(
          tgpi->scene, tgpi->ar, tgpi->ob, tgpi->gpl, (tGPspoint *)cp, NULL, &cp->x);
    }
  }

  /* reproject to plane */
  if (!is_depth) {
    float origin[3];
    ED_gp_get_drawing_reference(tgpi->scene, tgpi->ob, tgpi->gpl, ts->gpencil_v3d_align, origin);
    ED_gp_project_stroke_to_plane(
        tgpi->scene, tgpi->ob, tgpi->rv3d, gps, origin, ts->gp_sculpt.lock_axis - 1);
  }

  /* if parented change position relative to parent object */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    gp_apply_parent_point(tgpi->depsgraph, tgpi->ob, tgpi->gpd, tgpi->gpl, pt);
  }

  /* if camera view, reproject flat to view to avoid perspective effect */
  if (is_camera) {
    ED_gpencil_project_stroke_to_view(C, tgpi->gpl, gps);
  }

  /* force fill recalc */
  gps->flag |= GP_STROKE_RECALC_GEOMETRY;

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
  gp_primitive_update_strokes(C, tgpi);
}

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

  int cfra_eval = CFRA;

  /* create temporary operator data */
  tGPDprimitive *tgpi = MEM_callocN(sizeof(tGPDprimitive), "GPencil Primitive Data");
  op->customdata = tgpi;

  tgpi->points = MEM_callocN(sizeof(tGPspoint), "gp primitive points2D");

  /* set current scene and window info */
  tgpi->bmain = CTX_data_main(C);
  tgpi->scene = scene;
  tgpi->ob = CTX_data_active_object(C);
  tgpi->sa = CTX_wm_area(C);
  tgpi->ar = CTX_wm_region(C);
  tgpi->rv3d = tgpi->ar->regiondata;
  tgpi->v3d = tgpi->sa->spacedata.first;
  tgpi->depsgraph = CTX_data_depsgraph(C);
  tgpi->win = CTX_wm_window(C);

  /* save original type */
  tgpi->orign_type = RNA_enum_get(op->ptr, "type");

  /* set current frame number */
  tgpi->cframe = cfra_eval;

  /* set GP datablock */
  tgpi->gpd = gpd;
  /* region where paint was originated */
  tgpi->gpd->runtime.ar = tgpi->ar;

  /* if brush doesn't exist, create a new set (fix damaged files from old versions) */
  if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
    BKE_brush_gpencil_presets(C);
  }
  tgpi->brush = paint->brush;

  /* control points */
  tgpi->gpd->runtime.cp_points = MEM_callocN(sizeof(bGPDcontrolpoint) * MAX_CP,
                                             "gp primitive cpoint");
  tgpi->gpd->runtime.tot_cp_points = 0;

  /* getcolor info */
  tgpi->mat = BKE_gpencil_object_material_ensure_from_active_input_toolsettings(
      bmain, tgpi->ob, ts);

  /* set parameters */
  tgpi->type = RNA_enum_get(op->ptr, "type");

  if (ELEM(tgpi->type, GP_STROKE_ARC, GP_STROKE_CURVE)) {
    tgpi->curve = true;
  }
  else {
    tgpi->curve = false;
  }

  /* set default edge count */
  switch (tgpi->type) {
    case GP_STROKE_LINE: {
      RNA_int_set(op->ptr, "edges", 8);
      break;
    }
    case GP_STROKE_BOX: {
      RNA_int_set(op->ptr, "edges", 8);
      break;
    }
    case GP_STROKE_CIRCLE: {
      RNA_int_set(op->ptr, "edges", 96);
      break;
    }
    default: {
      RNA_int_set(op->ptr, "edges", 64);
      break;
    }
  }

  tgpi->tot_stored_edges = 0;
  tgpi->tot_edges = RNA_int_get(op->ptr, "edges");
  tgpi->flag = IDLE;
  tgpi->lock_axis = ts->gp_sculpt.lock_axis;

  /* set temp layer, frame and stroke */
  gp_primitive_set_initdata(C, tgpi);
}

/* Invoke handler: Initialize the operator */
static int gpencil_primitive_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  tGPDprimitive *tgpi = NULL;

  /* initialize operator runtime data */
  gpencil_primitive_init(C, op);
  tgpi = op->customdata;

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
  WM_cursor_modal_set(win, BC_CROSSCURSOR);

  /* update sindicator in header */
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

  const int def_nr = tgpi->ob->actdef - 1;
  const bool have_weight = (bool)BLI_findlink(&tgpi->ob->defbase, def_nr);

  /* return to normal cursor and header status */
  ED_workspace_status_text(C, NULL);
  WM_cursor_modal_restore(win);

  /* insert keyframes as required... */
  short add_frame_mode;
  if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
    add_frame_mode = GP_GETFRAME_ADD_COPY;
  }
  else {
    add_frame_mode = GP_GETFRAME_ADD_NEW;
  }

  gpf = BKE_gpencil_layer_getframe(tgpi->gpl, tgpi->cframe, add_frame_mode);

  /* prepare stroke to get transferred */
  gps = tgpi->gpf->strokes.first;
  if (gps) {
    gps->thickness = brush->size;
    gps->gradient_f = brush->gpencil_settings->gradient_f;
    copy_v2_v2(gps->gradient_s, brush->gpencil_settings->gradient_s);

    gps->flag |= GP_STROKE_RECALC_GEOMETRY;
    gps->tot_triangles = 0;

    /* calculate UVs along the stroke */
    ED_gpencil_calc_stroke_uv(tgpi->ob, gps);
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
      MDeformWeight *dw = defvert_verify_index(ve, def_nr);
      if (dw) {
        dw->weight = ts->vgroup_weight;
      }
    }
  }

  /* Close stroke with geometry */
  if ((tgpi->type == GP_STROKE_BOX) || (tgpi->type == GP_STROKE_CIRCLE)) {
    BKE_gpencil_close_stroke(gps);
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
      WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
    }
    else if (tgpi->curve) {
      move = MOVE_CP;
      WM_cursor_modal_set(win, BC_HANDCURSOR);
    }
    else {
      WM_cursor_modal_set(win, BC_CROSSCURSOR);
    }
  }
  else if (tgpi->flag == IN_PROGRESS) {
    WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
  }

  switch (event->type) {
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
    case LEFTMOUSE: {
      if ((event->val == KM_PRESS)) {
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
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS)) {
        /* set control points and enter edit mode */
        tgpi->flag = IN_CURVE_EDIT;
        gp_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      else {
        tgpi->sel_cp = SELECT_NONE;
      }
      break;
    }
    case MKEY: {
      if ((event->val == KM_PRESS) && (tgpi->curve) && (ELEM(tgpi->orign_type, GP_STROKE_ARC))) {
        tgpi->flip ^= 1;
        gp_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case EKEY: {
      if (tgpi->flag == IN_CURVE_EDIT && !ELEM(tgpi->type, GP_STROKE_BOX, GP_STROKE_CIRCLE)) {
        tgpi->flag = IN_PROGRESS;
        WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
        gpencil_primitive_add_segment(tgpi);
        copy_v2_v2(tgpi->start, tgpi->end);
        copy_v2_v2(tgpi->origin, tgpi->start);
        gp_primitive_update_cps(tgpi);
      }
      break;
    }
  }
}

/* brush strength */
static void gpencil_primitive_strength(tGPDprimitive *tgpi, bool reset)
{
  Brush *brush = tgpi->brush;
  if (brush) {
    if (reset) {
      brush->gpencil_settings->draw_strength = tgpi->brush_strength;
      tgpi->brush_strength = 0.0f;
    }
    else {
      if (tgpi->brush_strength == 0.0f) {
        tgpi->brush_strength = brush->gpencil_settings->draw_strength;
      }
      float move[2];
      sub_v2_v2v2(move, tgpi->mval, tgpi->mvalo);
      float adjust = (move[1] > 0.0f) ? 0.01f : -0.01f;
      brush->gpencil_settings->draw_strength += adjust * fabsf(len_manhattan_v2(move));
    }

    /* limit low limit because below 0.2f the stroke is invisible */
    CLAMP(brush->gpencil_settings->draw_strength, 0.2f, 1.0f);
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
      case ESCKEY:
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
  else if (tgpi->flag == IN_BRUSH_SIZE) {
    switch (event->type) {
      case MOUSEMOVE:
        gpencil_primitive_size(tgpi, false);
        gpencil_primitive_update(C, op, tgpi);
        break;
      case ESCKEY:
      case MIDDLEMOUSE:
      case LEFTMOUSE:
        tgpi->brush_size = 0;
        tgpi->flag = IN_CURVE_EDIT;
        break;
      case RIGHTMOUSE:
        if (event->val == KM_RELEASE) {
          tgpi->flag = IN_CURVE_EDIT;
          gpencil_primitive_size(tgpi, true);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
    }
    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }
  else if (tgpi->flag == IN_BRUSH_STRENGTH) {
    switch (event->type) {
      case MOUSEMOVE:
        gpencil_primitive_strength(tgpi, false);
        gpencil_primitive_update(C, op, tgpi);
        break;
      case ESCKEY:
      case MIDDLEMOUSE:
      case LEFTMOUSE:
        tgpi->brush_strength = 0.0f;
        tgpi->flag = IN_CURVE_EDIT;
        break;
      case RIGHTMOUSE:
        if (event->val == KM_RELEASE) {
          tgpi->flag = IN_CURVE_EDIT;
          gpencil_primitive_strength(tgpi, true);
          gpencil_primitive_update(C, op, tgpi);
        }
        break;
    }
    copy_v2_v2(tgpi->mvalo, tgpi->mval);
    return OPERATOR_RUNNING_MODAL;
  }
  else if (tgpi->flag != IDLE) {
    gpencil_primitive_edit_event_handling(C, op, win, event, tgpi);
  }

  switch (event->type) {
    case LEFTMOUSE: {
      if ((event->val == KM_PRESS) && (tgpi->flag == IDLE)) {
        /* start drawing primitive */
        /* TODO: Ignore if not in main region yet */
        tgpi->flag = IN_PROGRESS;
        gpencil_primitive_interaction_begin(tgpi, event);
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_MOVE)) {
        tgpi->flag = IN_CURVE_EDIT;
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS)) {
        /* set control points and enter edit mode */
        tgpi->flag = IN_CURVE_EDIT;
        gp_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS) &&
               (tgpi->type != GP_STROKE_CURVE)) {
        /* stop drawing primitive */
        tgpi->flag = IDLE;
        gpencil_primitive_interaction_end(C, op, win, tgpi);
        /* done! */
        return OPERATOR_FINISHED;
      }
      else {
        if (G.debug & G_DEBUG) {
          printf("GP Add Primitive Modal: LEFTMOUSE %d, Status = %d\n", event->val, tgpi->flag);
        }
      }
      break;
    }
    case SPACEKEY: /* confirm */
    case MIDDLEMOUSE:
    case RETKEY: {
      tgpi->flag = IDLE;
      gpencil_primitive_interaction_end(C, op, win, tgpi);
      /* done! */
      return OPERATOR_FINISHED;
    }
    case RIGHTMOUSE: {
      /* exception to cancel current stroke when we have previous strokes in buffer */
      if (tgpi->tot_stored_edges > 0) {
        tgpi->flag = IDLE;
        tgpi->tot_edges = 0;
        gp_primitive_update_strokes(C, tgpi);
        gpencil_primitive_interaction_end(C, op, win, tgpi);
        /* done! */
        return OPERATOR_FINISHED;
      }
      ATTR_FALLTHROUGH;
    }
    case ESCKEY: {
      /* return to normal cursor and header status */
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* clean up temp data */
      gpencil_primitive_exit(C, op);

      /* canceled! */
      return OPERATOR_CANCELLED;
    }
    case PADPLUSKEY:
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
    case PADMINUS:
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
    case GKEY: /* grab mode */
    {
      if ((event->val == KM_PRESS)) {
        tgpi->flag = IN_MOVE;
        WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
      }
      break;
    }
    case FKEY: /* brush thickness/ brush strength */
    {
      if ((event->val == KM_PRESS)) {
        if (event->shift) {
          tgpi->flag = IN_BRUSH_STRENGTH;
        }
        else {
          tgpi->flag = IN_BRUSH_SIZE;
        }
        WM_cursor_modal_set(win, BC_NS_SCROLLCURSOR);
      }
      break;
    }
    case CKEY: /* curve mode */
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
        gp_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case TABKEY: {
      if (tgpi->flag == IN_CURVE_EDIT) {
        tgpi->flag = IN_PROGRESS;
        WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
        gp_primitive_update_cps(tgpi);
        gpencil_primitive_update(C, op, tgpi);
      }
      break;
    }
    case MOUSEMOVE: /* calculate new position */
    {
      if (tgpi->flag == IN_CURVE_EDIT) {
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
          float x = tgpi->end[0] - tgpi->origin[0];
          float y = tgpi->end[1] - tgpi->origin[1];
          if (tgpi->type == GP_STROKE_LINE || tgpi->curve) {
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
        /* Center primitive if alt key */
        if (event->alt) {
          tgpi->start[0] = tgpi->origin[0] - (tgpi->end[0] - tgpi->origin[0]);
          tgpi->start[1] = tgpi->origin[1] - (tgpi->end[1] - tgpi->origin[1]);
        }
        gp_primitive_update_cps(tgpi);
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
      else {
        /* unhandled event - allow to pass through */
        return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
      }
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

void GPENCIL_OT_primitive(wmOperatorType *ot)
{
  static EnumPropertyItem primitive_type[] = {
      {GP_STROKE_BOX, "BOX", 0, "Box", ""},
      {GP_STROKE_LINE, "LINE", 0, "Line", ""},
      {GP_STROKE_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {GP_STROKE_ARC, "ARC", 0, "Arc", ""},
      {GP_STROKE_CURVE, "CURVE", 0, "Curve", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Grease Pencil Shapes";
  ot->idname = "GPENCIL_OT_primitive";
  ot->description = "Create predefined grease pencil stroke shapes";

  /* callbacks */
  ot->invoke = gpencil_primitive_invoke;
  ot->modal = gpencil_primitive_modal;
  ot->cancel = gpencil_primitive_cancel;
  ot->poll = gpencil_primitive_add_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna,
                     "edges",
                     4,
                     MIN_EDGES,
                     MAX_EDGES,
                     "Edges",
                     "Number of polygon edges",
                     MIN_EDGES,
                     MAX_EDGES);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_enum(ot->srna, "type", primitive_type, GP_STROKE_BOX, "Type", "Type of shape");

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
