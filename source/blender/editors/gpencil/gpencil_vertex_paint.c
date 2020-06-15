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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * This is a new part of Blender
 * Brush based operators for editing Grease Pencil strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_material.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* General Brush Editing Context */
#define GP_SELECT_BUFFER_CHUNK 256
#define GP_GRID_PIXEL_SIZE 10.0f

/* Temp Flags while Painting. */
typedef enum eGPDvertex_brush_Flag {
  /* invert the effect of the brush */
  GP_VERTEX_FLAG_INVERT = (1 << 0),
  /* temporary invert action */
  GP_VERTEX_FLAG_TMP_INVERT = (1 << 1),
} eGPDvertex_brush_Flag;

/* Grid of Colors for Smear. */
typedef struct tGP_Grid {
  /** Lower right corner of rectangle of grid cell. */
  float bottom[2];
  /** Upper left corner of rectangle of grid cell. */
  float top[2];
  /** Average Color */
  float color[4];
  /** Total points included. */
  int totcol;

} tGP_Grid;

/* List of points affected by brush. */
typedef struct tGP_Selected {
  /** Referenced stroke. */
  bGPDstroke *gps;
  /** Point index in points array. */
  int pt_index;
  /** Position */
  int pc[2];
  /** Color */
  float color[4];
} tGP_Selected;

/* Context for brush operators */
typedef struct tGP_BrushVertexpaintData {
  Scene *scene;
  Object *object;

  ARegion *region;

  /* Current GPencil datablock */
  bGPdata *gpd;

  Brush *brush;
  float linear_color[3];
  eGPDvertex_brush_Flag flag;
  eGP_Vertex_SelectMaskFlag mask;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /* Is the brush currently painting? */
  bool is_painting;

  /* Start of new paint */
  bool first;

  /* Is multiframe editing enabled, and are we using falloff for that? */
  bool is_multiframe;
  bool use_multiframe_falloff;

  /* Brush Runtime Data: */
  /* - position and pressure
   * - the *_prev variants are the previous values
   */
  float mval[2], mval_prev[2];
  float pressure, pressure_prev;

  /* - Effect 2D vector */
  float dvec[2];

  /* - multiframe falloff factor */
  float mf_falloff;

  /* brush geometry (bounding box) */
  rcti brush_rect;

  /* Temp data to save selected points */
  /** Stroke buffer. */
  tGP_Selected *pbuffer;
  /** Number of elements currently used in cache. */
  int pbuffer_used;
  /** Number of total elements available in cache. */
  int pbuffer_size;

  /** Grid of average colors */
  tGP_Grid *grid;
  /** Total number of rows/cols. */
  int grid_size;
  /** Total number of cells elments in the grid array. */
  int grid_len;
  /** Grid sample position (used to determine distance of falloff) */
  int grid_sample[2];
  /** Grid is ready to use */
  bool grid_ready;

} tGP_BrushVertexpaintData;

/* Ensure the buffer to hold temp selected point size is enough to save all points selected. */
static tGP_Selected *gpencil_select_buffer_ensure(tGP_Selected *buffer_array,
                                                  int *buffer_size,
                                                  int *buffer_used,
                                                  const bool clear)
{
  tGP_Selected *p = NULL;

  /* By default a buffer is created with one block with a predefined number of free slots,
   * if the size is not enough, the cache is reallocated adding a new block of free slots.
   * This is done in order to keep cache small and improve speed. */
  if (*buffer_used + 1 > *buffer_size) {
    if ((*buffer_size == 0) || (buffer_array == NULL)) {
      p = MEM_callocN(sizeof(struct tGP_Selected) * GP_SELECT_BUFFER_CHUNK, __func__);
      *buffer_size = GP_SELECT_BUFFER_CHUNK;
    }
    else {
      *buffer_size += GP_SELECT_BUFFER_CHUNK;
      p = MEM_recallocN(buffer_array, sizeof(struct tGP_Selected) * *buffer_size);
    }

    if (p == NULL) {
      *buffer_size = *buffer_used = 0;
    }

    buffer_array = p;
  }

  /* clear old data */
  if (clear) {
    *buffer_used = 0;
    if (buffer_array != NULL) {
      memset(buffer_array, 0, sizeof(tGP_Selected) * *buffer_size);
    }
  }

  return buffer_array;
}

/* Brush Operations ------------------------------- */

/* Invert behavior of brush? */
static bool brush_invert_check(tGP_BrushVertexpaintData *gso)
{
  /* The basic setting is no inverted */
  bool invert = false;

  /* During runtime, the user can hold down the Ctrl key to invert the basic behavior */
  if (gso->flag & GP_VERTEX_FLAG_INVERT) {
    invert ^= true;
  }

  return invert;
}

/* Compute strength of effect. */
static float brush_influence_calc(tGP_BrushVertexpaintData *gso, const int radius, const int co[2])
{
  Brush *brush = gso->brush;
  float influence = brush->size;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mval_i[2];
  round_v2i_v2fl(mval_i, gso->mval);
  float distance = (float)len_v2v2_int(mval_i, co);

  /* Apply Brush curve. */
  float brush_fallof = BKE_brush_curve_strength(brush, distance, (float)radius);
  influence *= brush_fallof;

  /* apply multiframe falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}

/* Compute effect vector for directional brushes. */
static void brush_calc_dvec_2d(tGP_BrushVertexpaintData *gso)
{
  gso->dvec[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
  gso->dvec[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

  normalize_v2(gso->dvec);
}

/* Init a grid of cells around mouse position.
 *
 * For each Cell.
 *
 *          *--------* Top
 *          |        |
 *          |        |
 *   Bottom *--------*
 *
 * The number of cells is calculated using the brush size and a predefined
 * number of pixels (see: GP_GRID_PIXEL_SIZE)
 */

static void gp_grid_cells_init(tGP_BrushVertexpaintData *gso)
{
  tGP_Grid *grid;
  float bottom[2];
  float top[2];
  int grid_index = 0;

  /* The grid center is (0,0). */
  bottom[0] = gso->brush_rect.xmin - gso->mval[0];
  bottom[1] = gso->brush_rect.ymax - GP_GRID_PIXEL_SIZE - gso->mval[1];

  /* Calc all cell of the grid from top/left. */
  for (int y = gso->grid_size - 1; y >= 0; y--) {
    top[1] = bottom[1] + GP_GRID_PIXEL_SIZE;

    for (int x = 0; x < gso->grid_size; x++) {
      top[0] = bottom[0] + GP_GRID_PIXEL_SIZE;

      grid = &gso->grid[grid_index];

      copy_v2_v2(grid->bottom, bottom);
      copy_v2_v2(grid->top, top);

      bottom[0] += GP_GRID_PIXEL_SIZE;

      grid_index++;
    }

    /* Reset for new row. */
    bottom[0] = gso->brush_rect.xmin - gso->mval[0];
    bottom[1] -= GP_GRID_PIXEL_SIZE;
  }
}

/* Get the index used in the grid base on dvec. */
static void gp_grid_cell_average_color_idx_get(tGP_BrushVertexpaintData *gso, int r_idx[2])
{
  /* Lower direction. */
  if (gso->dvec[1] < 0.0f) {
    if ((gso->dvec[0] >= -1.0f) && (gso->dvec[0] < -0.8f)) {
      r_idx[0] = 0;
      r_idx[1] = -1;
    }
    else if ((gso->dvec[0] >= -0.8f) && (gso->dvec[0] < -0.6f)) {
      r_idx[0] = -1;
      r_idx[1] = -1;
    }
    else if ((gso->dvec[0] >= -0.6f) && (gso->dvec[0] < 0.6f)) {
      r_idx[0] = -1;
      r_idx[1] = 0;
    }
    else if ((gso->dvec[0] >= 0.6f) && (gso->dvec[0] < 0.8f)) {
      r_idx[0] = -1;
      r_idx[1] = 1;
    }
    else if (gso->dvec[0] >= 0.8f) {
      r_idx[0] = 0;
      r_idx[1] = 1;
    }
  }
  /* Upper direction. */
  else {
    if ((gso->dvec[0] >= -1.0f) && (gso->dvec[0] < -0.8f)) {
      r_idx[0] = 0;
      r_idx[1] = -1;
    }
    else if ((gso->dvec[0] >= -0.8f) && (gso->dvec[0] < -0.6f)) {
      r_idx[0] = 1;
      r_idx[1] = -1;
    }
    else if ((gso->dvec[0] >= -0.6f) && (gso->dvec[0] < 0.6f)) {
      r_idx[0] = 1;
      r_idx[1] = 0;
    }
    else if ((gso->dvec[0] >= 0.6f) && (gso->dvec[0] < 0.8f)) {
      r_idx[0] = 1;
      r_idx[1] = 1;
    }
    else if (gso->dvec[0] >= 0.8f) {
      r_idx[0] = 0;
      r_idx[1] = 1;
    }
  }
}

static int gp_grid_cell_index_get(tGP_BrushVertexpaintData *gso, int pc[2])
{
  float bottom[2], top[2];

  for (int i = 0; i < gso->grid_len; i++) {
    tGP_Grid *grid = &gso->grid[i];
    add_v2_v2v2(bottom, grid->bottom, gso->mval);
    add_v2_v2v2(top, grid->top, gso->mval);

    if (pc[0] >= bottom[0] && pc[0] <= top[0] && pc[1] >= bottom[1] && pc[1] <= top[1]) {
      return i;
    }
  }

  return -1;
}

/* Fill the grid with the color in each cell and assign point cell index. */
static void gp_grid_colors_calc(tGP_BrushVertexpaintData *gso)
{
  tGP_Selected *selected = NULL;
  bGPDstroke *gps_selected = NULL;
  bGPDspoint *pt = NULL;
  tGP_Grid *grid = NULL;

  /* Don't calculate again. */
  if (gso->grid_ready) {
    return;
  }

  /* Extract colors by cell. */
  for (int i = 0; i < gso->pbuffer_used; i++) {
    selected = &gso->pbuffer[i];
    gps_selected = selected->gps;
    pt = &gps_selected->points[selected->pt_index];
    int grid_index = gp_grid_cell_index_get(gso, selected->pc);

    if (grid_index > -1) {
      grid = &gso->grid[grid_index];
      /* Add stroke mix color (only if used). */
      if (pt->vert_color[3] > 0.0f) {
        add_v3_v3(grid->color, selected->color);
        grid->color[3] = 1.0f;
        grid->totcol++;
      }
    }
  }

  /* Average colors. */
  for (int i = 0; i < gso->grid_len; i++) {
    grid = &gso->grid[i];
    if (grid->totcol > 0) {
      mul_v3_fl(grid->color, (1.0f / (float)grid->totcol));
    }
  }

  /* Save sample position. */
  round_v2i_v2fl(gso->grid_sample, gso->mval);

  gso->grid_ready = true;

  return;
}

/* ************************************************ */
/* Brush Callbacks
 * This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius. */

/* Tint Brush */
static bool brush_tint_apply(tGP_BrushVertexpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  Brush *brush = gso->brush;

  /* Attenuate factor to get a smoother tinting. */
  float inf = (brush_influence_calc(gso, radius, co) * brush->gpencil_settings->draw_strength) /
              100.0f;
  float inf_fill = (gso->pressure * brush->gpencil_settings->draw_strength) / 1000.0f;

  CLAMP(inf, 0.0f, 1.0f);
  CLAMP(inf_fill, 0.0f, 1.0f);

  /* Apply color to Stroke point. */
  if (GPENCIL_TINT_VERTEX_COLOR_STROKE(brush) && (pt_index > -1)) {
    bGPDspoint *pt = &gps->points[pt_index];
    if (brush_invert_check(gso)) {
      pt->vert_color[3] -= inf;
      CLAMP_MIN(pt->vert_color[3], 0.0f);
    }
    else {
      /* Premult. */
      mul_v3_fl(pt->vert_color, pt->vert_color[3]);
      /* "Alpha over" blending. */
      interp_v3_v3v3(pt->vert_color, pt->vert_color, gso->linear_color, inf);
      pt->vert_color[3] = pt->vert_color[3] * (1.0 - inf) + inf;
      /* Un-premult. */
      if (pt->vert_color[3] > 0.0f) {
        mul_v3_fl(pt->vert_color, 1.0f / pt->vert_color[3]);
      }
    }
  }

  /* Apply color to Fill area (all with same color and factor). */
  if (GPENCIL_TINT_VERTEX_COLOR_FILL(brush)) {
    if (brush_invert_check(gso)) {
      gps->vert_color_fill[3] -= inf_fill;
      CLAMP_MIN(gps->vert_color_fill[3], 0.0f);
    }
    else {
      /* Premult. */
      mul_v3_fl(gps->vert_color_fill, gps->vert_color_fill[3]);
      /* "Alpha over" blending. */
      interp_v3_v3v3(gps->vert_color_fill, gps->vert_color_fill, gso->linear_color, inf_fill);
      gps->vert_color_fill[3] = gps->vert_color_fill[3] * (1.0 - inf_fill) + inf_fill;
      /* Un-premult. */
      if (gps->vert_color_fill[3] > 0.0f) {
        mul_v3_fl(gps->vert_color_fill, 1.0f / gps->vert_color_fill[3]);
      }
    }
  }

  return true;
}

/* Replace Brush (Don't use pressure or invert). */
static bool brush_replace_apply(tGP_BrushVertexpaintData *gso, bGPDstroke *gps, int pt_index)
{
  Brush *brush = gso->brush;
  bGPDspoint *pt = &gps->points[pt_index];

  /* Apply color to Stroke point. */
  if (GPENCIL_TINT_VERTEX_COLOR_STROKE(brush)) {
    if (pt->vert_color[3] > 0.0f) {
      copy_v3_v3(pt->vert_color, gso->linear_color);
    }
  }

  /* Apply color to Fill area (all with same color and factor). */
  if (GPENCIL_TINT_VERTEX_COLOR_FILL(brush)) {
    if (gps->vert_color_fill[3] > 0.0f) {
      copy_v3_v3(gps->vert_color_fill, gso->linear_color);
    }
  }

  return true;
}

/* Get surrounding color. */
static bool get_surrounding_color(tGP_BrushVertexpaintData *gso,
                                  bGPDstroke *gps,
                                  int pt_index,
                                  float r_color[3])
{
  tGP_Selected *selected = NULL;
  bGPDstroke *gps_selected = NULL;
  bGPDspoint *pt = NULL;

  int totcol = 0;
  zero_v3(r_color);

  /* Average the surrounding points except current one. */
  for (int i = 0; i < gso->pbuffer_used; i++) {
    selected = &gso->pbuffer[i];
    gps_selected = selected->gps;
    /* current point is not evaluated. */
    if ((gps_selected == gps) && (selected->pt_index == pt_index)) {
      continue;
    }

    pt = &gps_selected->points[selected->pt_index];

    /* Add stroke mix color (only if used). */
    if (pt->vert_color[3] > 0.0f) {
      add_v3_v3(r_color, selected->color);
      totcol++;
    }
  }
  if (totcol > 0) {
    mul_v3_fl(r_color, (1.0f / (float)totcol));
    return true;
  }

  return false;
}

/* Blur Brush */
static bool brush_blur_apply(tGP_BrushVertexpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  Brush *brush = gso->brush;

  /* Attenuate factor to get a smoother tinting. */
  float inf = (brush_influence_calc(gso, radius, co) * brush->gpencil_settings->draw_strength) /
              100.0f;
  float inf_fill = (gso->pressure * brush->gpencil_settings->draw_strength) / 1000.0f;

  bGPDspoint *pt = &gps->points[pt_index];

  /* Get surrounding color. */
  float blur_color[3];
  if (get_surrounding_color(gso, gps, pt_index, blur_color)) {
    /* Apply color to Stroke point. */
    if (GPENCIL_TINT_VERTEX_COLOR_STROKE(brush)) {
      interp_v3_v3v3(pt->vert_color, pt->vert_color, blur_color, inf);
    }

    /* Apply color to Fill area (all with same color and factor). */
    if (GPENCIL_TINT_VERTEX_COLOR_FILL(brush)) {
      interp_v3_v3v3(gps->vert_color_fill, gps->vert_color_fill, blur_color, inf_fill);
    }
    return true;
  }

  return false;
}

/* Average Brush */
static bool brush_average_apply(tGP_BrushVertexpaintData *gso,
                                bGPDstroke *gps,
                                int pt_index,
                                const int radius,
                                const int co[2],
                                float average_color[3])
{
  Brush *brush = gso->brush;

  /* Attenuate factor to get a smoother tinting. */
  float inf = (brush_influence_calc(gso, radius, co) * brush->gpencil_settings->draw_strength) /
              100.0f;
  float inf_fill = (gso->pressure * brush->gpencil_settings->draw_strength) / 1000.0f;

  bGPDspoint *pt = &gps->points[pt_index];

  float alpha = pt->vert_color[3];
  float alpha_fill = gps->vert_color_fill[3];

  if (brush_invert_check(gso)) {
    alpha -= inf;
    alpha_fill -= inf_fill;
  }
  else {
    alpha += inf;
    alpha_fill += inf_fill;
  }

  /* Apply color to Stroke point. */
  if (GPENCIL_TINT_VERTEX_COLOR_STROKE(brush)) {
    CLAMP(alpha, 0.0f, 1.0f);
    interp_v3_v3v3(pt->vert_color, pt->vert_color, average_color, inf);
    pt->vert_color[3] = alpha;
  }

  /* Apply color to Fill area (all with same color and factor). */
  if (GPENCIL_TINT_VERTEX_COLOR_FILL(brush)) {
    CLAMP(alpha_fill, 0.0f, 1.0f);
    copy_v3_v3(gps->vert_color_fill, average_color);
    gps->vert_color_fill[3] = alpha_fill;
  }

  return true;
}

/* Smear Brush */
static bool brush_smear_apply(tGP_BrushVertexpaintData *gso,
                              bGPDstroke *gps,
                              int pt_index,
                              tGP_Selected *selected)
{
  Brush *brush = gso->brush;
  tGP_Grid *grid = NULL;
  int average_idx[2];
  ARRAY_SET_ITEMS(average_idx, 0, 0);

  bool changed = false;

  /* Need some movement, so first input is not done. */
  if (gso->first) {
    return false;
  }

  bGPDspoint *pt = &gps->points[pt_index];

  /* Need get average colors in the grid. */
  if ((!gso->grid_ready) && (gso->pbuffer_used > 0)) {
    gp_grid_colors_calc(gso);
  }

  /* The influence is equal to strength and no decay around brush radius. */
  float inf = brush->gpencil_settings->draw_strength;
  if (brush->flag & GP_BRUSH_USE_PRESSURE) {
    inf *= gso->pressure;
  }

  /* Calc distance from initial sample location and add a fallof effect. */
  int mval_i[2];
  round_v2i_v2fl(mval_i, gso->mval);
  float distance = (float)len_v2v2_int(mval_i, gso->grid_sample);
  float fac = 1.0f - (distance / (float)(brush->size * 2));
  CLAMP(fac, 0.0f, 1.0f);
  inf *= fac;

  /* Retry row and col for average color. */
  gp_grid_cell_average_color_idx_get(gso, average_idx);

  /* Retry average color cell. */
  int grid_index = gp_grid_cell_index_get(gso, selected->pc);
  if (grid_index > -1) {
    int row = grid_index / gso->grid_size;
    int col = grid_index - (gso->grid_size * row);
    row += average_idx[0];
    col += average_idx[1];
    CLAMP(row, 0, gso->grid_size);
    CLAMP(col, 0, gso->grid_size);

    int new_index = (row * gso->grid_size) + col;
    CLAMP(new_index, 0, gso->grid_len - 1);
    grid = &gso->grid[new_index];
  }

  /* Apply color to Stroke point. */
  if (GPENCIL_TINT_VERTEX_COLOR_STROKE(brush)) {
    if (grid_index > -1) {
      if (grid->color[3] > 0.0f) {
        // copy_v3_v3(pt->vert_color, grid->color);
        interp_v3_v3v3(pt->vert_color, pt->vert_color, grid->color, inf);
        changed = true;
      }
    }
  }

  /* Apply color to Fill area (all with same color and factor). */
  if (GPENCIL_TINT_VERTEX_COLOR_FILL(brush)) {
    if (grid_index > -1) {
      if (grid->color[3] > 0.0f) {
        interp_v3_v3v3(gps->vert_color_fill, gps->vert_color_fill, grid->color, inf);
        changed = true;
      }
    }
  }

  return changed;
}

/* ************************************************ */
/* Header Info */
static void gp_vertexpaint_brush_header_set(bContext *C)
{
  ED_workspace_status_text(C,
                           TIP_("GPencil Vertex Paint: LMB to paint | RMB/Escape to Exit"
                                " | Ctrl to Invert Action"));
}

/* ************************************************ */
/* Grease Pencil Vertex Paint Operator */

/* Init/Exit ----------------------------------------------- */

static bool gp_vertexpaint_brush_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  Paint *paint = ob->mode == OB_MODE_VERTEX_GPENCIL ? &ts->gp_vertexpaint->paint :
                                                      &ts->gp_paint->paint;

  /* set the brush using the tool */
  tGP_BrushVertexpaintData *gso;

  /* setup operator data */
  gso = MEM_callocN(sizeof(tGP_BrushVertexpaintData), "tGP_BrushVertexpaintData");
  op->customdata = gso;

  gso->brush = paint->brush;
  srgb_to_linearrgb_v3_v3(gso->linear_color, gso->brush->rgb);
  BKE_curvemapping_initialize(gso->brush->curve);

  gso->is_painting = false;
  gso->first = true;

  gso->pbuffer = NULL;
  gso->pbuffer_size = 0;
  gso->pbuffer_used = 0;

  /* Alloc grid array */
  gso->grid_size = (int)(((gso->brush->size * 2.0f) / GP_GRID_PIXEL_SIZE) + 1.0);
  /* Square value. */
  gso->grid_len = gso->grid_size * gso->grid_size;
  gso->grid = MEM_callocN(sizeof(tGP_Grid) * gso->grid_len, "tGP_Grid");
  gso->grid_ready = false;

  gso->gpd = ED_gpencil_data_get_active(C);
  gso->scene = scene;
  gso->object = ob;

  gso->region = CTX_wm_region(C);

  /* Save mask. */
  gso->mask = ts->gpencil_selectmode_vertex;

  /* Multiframe settings. */
  gso->is_multiframe = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gso->gpd);
  gso->use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;

  /* Init multi-edit falloff curve data before doing anything,
   * so we won't have to do it again later. */
  if (gso->is_multiframe) {
    BKE_curvemapping_initialize(ts->gp_sculpt.cur_falloff);
  }

  /* Setup space conversions. */
  gp_point_conversion_init(C, &gso->gsc);

  /* Update header. */
  gp_vertexpaint_brush_header_set(C);

  return true;
}

static void gp_vertexpaint_brush_exit(bContext *C, wmOperator *op)
{
  tGP_BrushVertexpaintData *gso = op->customdata;

  /* Disable headerprints. */
  ED_workspace_status_text(C, NULL);

  /* Disable temp invert flag. */
  gso->brush->flag &= ~GP_VERTEX_FLAG_TMP_INVERT;

  /* Free operator data */
  MEM_SAFE_FREE(gso->pbuffer);
  MEM_SAFE_FREE(gso->grid);
  MEM_SAFE_FREE(gso);
  op->customdata = NULL;
}

/* Poll callback for stroke vertex paint operator. */
static bool gp_vertexpaint_brush_poll(bContext *C)
{
  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* Helper to save the points selected by the brush. */
static void gp_save_selected_point(tGP_BrushVertexpaintData *gso,
                                   bGPDstroke *gps,
                                   int index,
                                   int pc[2])
{
  tGP_Selected *selected;
  bGPDspoint *pt = &gps->points[index];

  /* Ensure the array to save the list of selected points is big enough. */
  gso->pbuffer = gpencil_select_buffer_ensure(
      gso->pbuffer, &gso->pbuffer_size, &gso->pbuffer_used, false);

  selected = &gso->pbuffer[gso->pbuffer_used];
  selected->gps = gps;
  selected->pt_index = index;
  /* Check the index is not a special case for fill. */
  if (index > -1) {
    copy_v2_v2_int(selected->pc, pc);
    copy_v4_v4(selected->color, pt->vert_color);
  }
  gso->pbuffer_used++;
}

/* Select points in this stroke and add to an array to be used later. */
static void gp_vertexpaint_select_stroke(tGP_BrushVertexpaintData *gso,
                                         bGPDstroke *gps,
                                         const char tool,
                                         const float diff_mat[4][4])
{
  GP_SpaceConversion *gsc = &gso->gsc;
  rcti *rect = &gso->brush_rect;
  Brush *brush = gso->brush;
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;

  bGPDspoint *pt1, *pt2;
  bGPDspoint *pt = NULL;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int index;
  bool include_last = false;

  /* Check if the stroke collide with brush. */
  if (!ED_gpencil_stroke_check_collision(gsc, gps, gso->mval, radius, diff_mat)) {
    return;
  }

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    pt = &gps->points[0];
    gp_point_to_parent_space(gps->points, diff_mat, &pt_temp);
    gp_point_to_xy(gsc, gps, &pt_temp, &pc1[0], &pc1[1]);

    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
    /* do boundbox check first */
    if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
      /* only check if point is inside */
      int mval_i[2];
      round_v2i_v2fl(mval_i, gso->mval);
      if (len_v2v2_int(mval_i, pc1) <= radius) {
        /* apply operation to this point */
        if (pt_active != NULL) {
          gp_save_selected_point(gso, gps_active, 0, pc1);
        }
      }
    }
  }
  else {
    /* Loop over the points in the stroke, checking for intersections
     * - an intersection means that we touched the stroke
     */
    bool hit = false;
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* Get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      /* Skip if neither one is selected
       * (and we are only allowed to edit/consider selected points) */
      if (GPENCIL_ANY_VERTEX_MASK(gso->mask)) {
        if (!(pt1->flag & GP_SPOINT_SELECT) && !(pt2->flag & GP_SPOINT_SELECT)) {
          include_last = false;
          continue;
        }
      }

      bGPDspoint npt;
      gp_point_to_parent_space(pt1, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &pc1[0], &pc1[1]);

      gp_point_to_parent_space(pt2, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &pc2[0], &pc2[1]);

      /* Check that point segment of the boundbox of the selection stroke */
      if (((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          ((!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1])) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1]))) {
        /* Check if point segment of stroke had anything to do with
         * brush region  (either within stroke painted, or on its lines)
         * - this assumes that linewidth is irrelevant
         */
        if (gp_stroke_inside_circle(gso->mval, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {

          /* To each point individually... */
          pt = &gps->points[i];
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if (pt_active != NULL) {
            /* If masked and the point is not selected, skip it. */
            if ((GPENCIL_ANY_VERTEX_MASK(gso->mask)) &&
                ((pt_active->flag & GP_SPOINT_SELECT) == 0)) {
              continue;
            }
            hit = true;
            gp_save_selected_point(gso, gps_active, index, pc1);
          }

          /* Only do the second point if this is the last segment,
           * and it is unlikely that the point will get handled
           * otherwise.
           *
           * NOTE: There is a small risk here that the second point wasn't really
           *       actually in-range. In that case, it only got in because
           *       the line linking the points was!
           */
          if (i + 1 == gps->totpoints - 1) {
            pt = &gps->points[i + 1];
            pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i + 1;
            if (pt_active != NULL) {
              hit = true;
              gp_save_selected_point(gso, gps_active, index, pc2);
              include_last = false;
            }
          }
          else {
            include_last = true;
          }
        }
        else if (include_last) {
          /* This case is for cases where for whatever reason the second vert (1st here)
           * doesn't get included because the whole edge isn't in bounds,
           * but it would've qualified since it did with the previous step
           * (but wasn't added then, to avoid double-ups).
           */
          pt = &gps->points[i];
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if (pt_active != NULL) {
            hit = true;
            gp_save_selected_point(gso, gps_active, index, pc1);

            include_last = false;
          }
        }
      }
    }

    /* If nothing hit, check if the mouse is inside any filled stroke. */
    if ((!hit) && (ELEM(tool, GPAINT_TOOL_TINT, GPVERTEX_TOOL_DRAW))) {
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(gso->object,
                                                                     gps_active->mat_nr + 1);
      if (gp_style->flag & GP_MATERIAL_FILL_SHOW) {
        int mval[2];
        round_v2i_v2fl(mval, gso->mval);
        bool hit_fill = ED_gpencil_stroke_point_is_inside(gps_active, gsc, mval, diff_mat);
        if (hit_fill) {
          /* Need repeat the effect because if we don't do that the tint process
           * is very slow. */
          for (int repeat = 0; repeat < 50; repeat++) {
            gp_save_selected_point(gso, gps_active, -1, NULL);
          }
        }
      }
    }
  }
}

/* Apply vertex paint brushes to strokes in the given frame. */
static bool gp_vertexpaint_brush_do_frame(bContext *C,
                                          tGP_BrushVertexpaintData *gso,
                                          bGPDlayer *gpl,
                                          bGPDframe *gpf,
                                          const float diff_mat[4][4])
{
  Object *ob = CTX_data_active_object(C);
  const char tool = ob->mode == OB_MODE_VERTEX_GPENCIL ? gso->brush->gpencil_vertex_tool :
                                                         gso->brush->gpencil_tool;
  const int radius = (gso->brush->flag & GP_BRUSH_USE_PRESSURE) ?
                         gso->brush->size * gso->pressure :
                         gso->brush->size;
  tGP_Selected *selected = NULL;
  int i;

  /*---------------------------------------------------------------------
   * First step: select the points affected. This step is required to have
   * all selected points before apply the effect, because it could be
   * required to average data.
   *--------------------------------------------------------------------- */
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    /* Skip strokes that are invalid for current view. */
    if (ED_gpencil_stroke_can_use(C, gps) == false) {
      continue;
    }
    /* Check if the color is editable. */
    if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
      continue;
    }

    /* Check points below the brush. */
    gp_vertexpaint_select_stroke(gso, gps, tool, diff_mat);
  }

  /* For Average tool, need calculate the average resulting color from all colors
   * under the brush. */
  float average_color[3] = {0};
  int totcol = 0;
  if ((tool == GPVERTEX_TOOL_AVERAGE) && (gso->pbuffer_used > 0)) {
    for (i = 0; i < gso->pbuffer_used; i++) {
      selected = &gso->pbuffer[i];
      bGPDstroke *gps = selected->gps;
      bGPDspoint *pt = &gps->points[selected->pt_index];

      /* Add stroke mix color (only if used). */
      if (pt->vert_color[3] > 0.0f) {
        add_v3_v3(average_color, pt->vert_color);
        totcol++;
      }

      /* If Fill color mix, add to average. */
      if (gps->vert_color_fill[3] > 0.0f) {
        add_v3_v3(average_color, gps->vert_color_fill);
        totcol++;
      }
    }

    /* Get average. */
    if (totcol > 0) {
      mul_v3_fl(average_color, (1.0f / (float)totcol));
    }
  }

  /*---------------------------------------------------------------------
   * Second step: Apply effect.
   *--------------------------------------------------------------------- */
  bool changed = false;
  for (i = 0; i < gso->pbuffer_used; i++) {
    changed = true;
    selected = &gso->pbuffer[i];

    switch (tool) {
      case GPAINT_TOOL_TINT:
      case GPVERTEX_TOOL_DRAW: {
        brush_tint_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        changed |= true;
        break;
      }
      case GPVERTEX_TOOL_BLUR: {
        brush_blur_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        changed |= true;
        break;
      }
      case GPVERTEX_TOOL_AVERAGE: {
        brush_average_apply(
            gso, selected->gps, selected->pt_index, radius, selected->pc, average_color);
        changed |= true;
        break;
      }
      case GPVERTEX_TOOL_SMEAR: {
        brush_smear_apply(gso, selected->gps, selected->pt_index, selected);
        changed |= true;
        break;
      }
      case GPVERTEX_TOOL_REPLACE: {
        brush_replace_apply(gso, selected->gps, selected->pt_index);
        changed |= true;
        break;
      }

      default:
        printf("ERROR: Unknown type of GPencil Vertex Paint brush\n");
        break;
    }
  }
  /* Clear the selected array, but keep the memory allocation.*/
  gso->pbuffer = gpencil_select_buffer_ensure(
      gso->pbuffer, &gso->pbuffer_size, &gso->pbuffer_used, true);

  return changed;
}

/* Apply brush effect to all layers. */
static bool gp_vertexpaint_brush_apply_to_layers(bContext *C, tGP_BrushVertexpaintData *gso)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = gso->object;
  bool changed = false;

  Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obact->id);
  bGPdata *gpd = (bGPdata *)ob_eval->data;

  /* Find visible strokes, and perform operations on those if hit */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* If locked or no active frame, don't do anything. */
    if ((!BKE_gpencil_layer_is_editable(gpl)) || (gpl->actframe == NULL)) {
      continue;
    }

    /* calculate difference matrix */
    float diff_mat[4][4];
    BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);

    /* Active Frame or MultiFrame? */
    if (gso->is_multiframe) {
      /* init multiframe falloff options */
      int f_init = 0;
      int f_end = 0;

      if (gso->use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        /* Always do active frame; Otherwise, only include selected frames */
        if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
          /* Compute multi-frame falloff factor. */
          if (gso->use_multiframe_falloff) {
            /* Falloff depends on distance to active frame (relative to the overall frame range) */
            gso->mf_falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }
          else {
            /* No falloff */
            gso->mf_falloff = 1.0f;
          }

          /* affect strokes in this frame */
          changed |= gp_vertexpaint_brush_do_frame(C, gso, gpl, gpf, diff_mat);
        }
      }
    }
    else {
      /* Apply to active frame's strokes */
      if (gpl->actframe != NULL) {
        gso->mf_falloff = 1.0f;
        changed |= gp_vertexpaint_brush_do_frame(C, gso, gpl, gpl->actframe, diff_mat);
      }
    }
  }

  return changed;
}

/* Calculate settings for applying brush */
static void gp_vertexpaint_brush_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
  tGP_BrushVertexpaintData *gso = op->customdata;
  Brush *brush = gso->brush;
  const int radius = ((brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                              gso->brush->size);
  float mousef[2];
  int mouse[2];
  bool changed = false;

  /* Get latest mouse coordinates */
  RNA_float_get_array(itemptr, "mouse", mousef);
  gso->mval[0] = mouse[0] = (int)(mousef[0]);
  gso->mval[1] = mouse[1] = (int)(mousef[1]);

  gso->pressure = RNA_float_get(itemptr, "pressure");

  if (RNA_boolean_get(itemptr, "pen_flip")) {
    gso->flag |= GP_VERTEX_FLAG_INVERT;
  }
  else {
    gso->flag &= ~GP_VERTEX_FLAG_INVERT;
  }

  /* Store coordinates as reference, if operator just started running */
  if (gso->first) {
    gso->mval_prev[0] = gso->mval[0];
    gso->mval_prev[1] = gso->mval[1];
    gso->pressure_prev = gso->pressure;
  }

  /* Update brush_rect, so that it represents the bounding rectangle of brush. */
  gso->brush_rect.xmin = mouse[0] - radius;
  gso->brush_rect.ymin = mouse[1] - radius;
  gso->brush_rect.xmax = mouse[0] + radius;
  gso->brush_rect.ymax = mouse[1] + radius;

  /* Calc 2D direction vector and relative angle. */
  brush_calc_dvec_2d(gso);

  /* Calc grid for smear tool. */
  gp_grid_cells_init(gso);

  changed = gp_vertexpaint_brush_apply_to_layers(C, gso);

  /* Updates */
  if (changed) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  }

  /* Store values for next step */
  gso->mval_prev[0] = gso->mval[0];
  gso->mval_prev[1] = gso->mval[1];
  gso->pressure_prev = gso->pressure;
  gso->first = false;
}

/* Running --------------------------------------------- */

/* helper - a record stroke, and apply paint event */
static void gp_vertexpaint_brush_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushVertexpaintData *gso = op->customdata;
  PointerRNA itemptr;
  float mouse[2];

  mouse[0] = event->mval[0] + 1;
  mouse[1] = event->mval[1] + 1;

  /* fill in stroke */
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  RNA_float_set_array(&itemptr, "mouse", mouse);
  RNA_boolean_set(&itemptr, "pen_flip", event->ctrl != false);
  RNA_boolean_set(&itemptr, "is_start", gso->first);

  /* Handle pressure sensitivity (which is supplied by tablets). */
  float pressure = event->tablet.pressure;
  CLAMP(pressure, 0.0f, 1.0f);
  RNA_float_set(&itemptr, "pressure", pressure);

  /* apply */
  gp_vertexpaint_brush_apply(C, op, &itemptr);
}

/* reapply */
static int gp_vertexpaint_brush_exec(bContext *C, wmOperator *op)
{
  if (!gp_vertexpaint_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    gp_vertexpaint_brush_apply(C, op, &itemptr);
  }
  RNA_END;

  gp_vertexpaint_brush_exit(C, op);

  return OPERATOR_FINISHED;
}

/* start modal painting */
static int gp_vertexpaint_brush_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushVertexpaintData *gso = NULL;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  const bool is_playing = ED_screen_animation_playing(CTX_wm_manager(C)) != NULL;

  /* the operator cannot work while play animation */
  if (is_playing) {
    BKE_report(op->reports, RPT_ERROR, "Cannot Paint while play animation");

    return OPERATOR_CANCELLED;
  }

  /* init painting data */
  if (!gp_vertexpaint_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  gso = op->customdata;

  /* register modal handler */
  WM_event_add_modal_handler(C, op);

  /* start drawing immediately? */
  if (is_modal == false) {
    ARegion *region = CTX_wm_region(C);

    /* apply first dab... */
    gso->is_painting = true;
    gp_vertexpaint_brush_apply_event(C, op, event);

    /* redraw view with feedback */
    ED_region_tag_redraw(region);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* painting - handle events */
static int gp_vertexpaint_brush_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushVertexpaintData *gso = op->customdata;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  bool redraw_region = false;
  bool redraw_toolsettings = false;

  /* The operator can be in 2 states: Painting and Idling */
  if (gso->is_painting) {
    /* Painting  */
    switch (event->type) {
      /* Mouse Move = Apply somewhere else */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        /* apply brush effect at new position */
        gp_vertexpaint_brush_apply_event(C, op, event);

        /* force redraw, so that the cursor will at least be valid */
        redraw_region = true;
        break;

      /* Painting mbut release = Stop painting (back to idle) */
      case LEFTMOUSE:
        if (is_modal) {
          /* go back to idling... */
          gso->is_painting = false;
        }
        else {
          /* end painting, since we're not modal */
          gso->is_painting = false;

          gp_vertexpaint_brush_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;

      /* Abort painting if any of the usual things are tried */
      case MIDDLEMOUSE:
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gp_vertexpaint_brush_exit(C, op);
        return OPERATOR_FINISHED;
    }
  }
  else {
    /* Idling */
    BLI_assert(is_modal == true);

    switch (event->type) {
      /* Painting mbut press = Start painting (switch to painting state) */
      case LEFTMOUSE:
        /* do initial "click" apply */
        gso->is_painting = true;
        gso->first = true;

        gp_vertexpaint_brush_apply_event(C, op, event);
        break;

      /* Exit modal operator, based on the "standard" ops */
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gp_vertexpaint_brush_exit(C, op);
        return OPERATOR_FINISHED;

      /* MMB is often used for view manipulations */
      case MIDDLEMOUSE:
        return OPERATOR_PASS_THROUGH;

      /* Mouse movements should update the brush cursor - Just redraw the active region */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        redraw_region = true;
        break;

      /* Change Frame - Allowed */
      case EVT_LEFTARROWKEY:
      case EVT_RIGHTARROWKEY:
      case EVT_UPARROWKEY:
      case EVT_DOWNARROWKEY:
        return OPERATOR_PASS_THROUGH;

      /* Camera/View Gizmo's - Allowed */
      /* (See rationale in gpencil_paint.c -> gpencil_draw_modal()) */
      case EVT_PAD0:
      case EVT_PAD1:
      case EVT_PAD2:
      case EVT_PAD3:
      case EVT_PAD4:
      case EVT_PAD5:
      case EVT_PAD6:
      case EVT_PAD7:
      case EVT_PAD8:
      case EVT_PAD9:
        return OPERATOR_PASS_THROUGH;

      /* Unhandled event */
      default:
        break;
    }
  }

  /* Redraw region? */
  if (redraw_region) {
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  /* Redraw toolsettings (brush settings)? */
  if (redraw_toolsettings) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  }

  return OPERATOR_RUNNING_MODAL;
}

void GPENCIL_OT_vertex_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stroke Vertex Paint";
  ot->idname = "GPENCIL_OT_vertex_paint";
  ot->description = "Paint stroke points with a color";

  /* api callbacks */
  ot->exec = gp_vertexpaint_brush_exec;
  ot->invoke = gp_vertexpaint_brush_invoke;
  ot->modal = gp_vertexpaint_brush_modal;
  ot->cancel = gp_vertexpaint_brush_exit;
  ot->poll = gp_vertexpaint_brush_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
