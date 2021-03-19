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
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

#define LEAK_HORZ 0
#define LEAK_VERT 1
#define MIN_WINDOW_SIZE 128

/* Set to 1 to debug filling internal image. By default, the value must be 0. */
#define FILL_DEBUG 0

/* Duplicated: etempFlags */
enum {
  GP_DRAWFILLS_NOSTATUS = (1 << 0), /* don't draw status info */
  GP_DRAWFILLS_ONLY3D = (1 << 1),   /* only draw 3d-strokes */
};

/* Temporary fill operation data `op->customdata`. */
typedef struct tGPDfill {
  bContext *C;
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  /** window where painting originated */
  struct wmWindow *win;
  /** current scene from context */
  struct Scene *scene;
  /** current active gp object */
  struct Object *ob;
  /** area where painting originated */
  struct ScrArea *area;
  /** region where painting originated */
  struct RegionView3D *rv3d;
  /** view3 where painting originated */
  struct View3D *v3d;
  /** region where painting originated */
  struct ARegion *region;
  /** Current GP data-block. */
  struct bGPdata *gpd;
  /** current material */
  struct Material *mat;
  /** current brush */
  struct Brush *brush;
  /** layer */
  struct bGPDlayer *gpl;
  /** frame */
  struct bGPDframe *gpf;
  /** Temp mouse position stroke. */
  struct bGPDstroke *gps_mouse;
  /** Pointer to report messages.  */
  struct ReportList *reports;
  /** flags */
  short flag;
  /** avoid too fast events */
  short oldkey;
  /** send to back stroke */
  bool on_back;
  /** Flag for render mode */
  bool is_render;
  /** Flag to check something was done. */
  bool done;
  /** mouse fill center position */
  int mouse[2];
  /** windows width */
  int sizex;
  /** window height */
  int sizey;
  /** lock to viewport axis */
  int lock_axis;

  /** number of pixel to consider the leak is too small (x 2) */
  short fill_leak;
  /** factor for transparency */
  float fill_threshold;
  /** number of simplify steps */
  int fill_simplylvl;
  /** boundary limits drawing mode */
  int fill_draw_mode;
  /* scaling factor */
  float fill_factor;

  /* Frame to use. */
  int active_cfra;

  /** number of elements currently in cache */
  short sbuffer_used;
  /** temporary points */
  void *sbuffer;
  /** depth array for reproject */
  float *depth_arr;

  /** temp image */
  Image *ima;
  /** temp points data */
  BLI_Stack *stack;
  /** handle for drawing strokes while operator is running 3d stuff */
  void *draw_handle_3d;

  /* Temporary size x. */
  int bwinx;
  /* Temporary size y. */
  int bwiny;
  rcti brect;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /** Zoom factor. */
  float zoom;

  /** Factor of extension. */
  float fill_extend_fac;

} tGPDfill;

bool skip_layer_check(short fill_layer_mode, int gpl_active_index, int gpl_index);
static void gpencil_draw_boundary_lines(const struct bContext *UNUSED(C), struct tGPDfill *tgpf);

/* Delete any temporary stroke. */
static void gpencil_delete_temp_stroke_extension(tGPDfill *tgpf, const bool all_frames)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &tgpf->gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    bGPDframe *init_gpf = (all_frames) ? gpl->frames.first :
                                         BKE_gpencil_layer_frame_get(
                                             gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (init_gpf == NULL) {
      continue;
    }
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        /* free stroke */
        if ((gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_TAG)) {
          BLI_remlink(&gpf->strokes, gps);
          BKE_gpencil_free_stroke(gps);
        }
      }
      if (!all_frames) {
        break;
      }
    }
  }
}

static void extrapolate_points_by_length(bGPDspoint *a,
                                         bGPDspoint *b,
                                         float length,
                                         float r_point[3])
{
  float ab[3];
  sub_v3_v3v3(ab, &b->x, &a->x);
  normalize_v3(ab);
  mul_v3_fl(ab, length);
  add_v3_v3v3(r_point, &b->x, ab);
}

/* Loop all layers create stroke extensions. */
static void gpencil_create_extensions(tGPDfill *tgpf)
{
  Object *ob = tgpf->ob;
  bGPdata *gpd = tgpf->gpd;
  Brush *brush = tgpf->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;

  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  BLI_assert(gpl_active != NULL);

  const int gpl_active_index = BLI_findindex(&gpd->layers, gpl_active);
  BLI_assert(gpl_active_index >= 0);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* Decide if the strokes of layers are included or not depending on the layer mode. */
    const int gpl_index = BLI_findindex(&gpd->layers, gpl);
    bool skip = skip_layer_check(brush_settings->fill_layer_mode, gpl_active_index, gpl_index);
    if (skip) {
      continue;
    }

    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }

    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      /* Check if stroke can be drawn. */
      if ((gps->points == NULL) || (gps->totpoints < 2)) {
        continue;
      }
      if (gps->flag & (GP_STROKE_NOFILL | GP_STROKE_TAG)) {
        continue;
      }
      /* Check if the color is visible. */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE)) {
        continue;
      }

      /* Extend start. */
      bGPDspoint *pt0 = &gps->points[1];
      bGPDspoint *pt1 = &gps->points[0];
      bGPDstroke *gps_new = BKE_gpencil_stroke_new(gps->mat_nr, 2, gps->thickness);
      gps_new->flag |= GP_STROKE_NOFILL | GP_STROKE_TAG;
      BLI_addtail(&gpf->strokes, gps_new);

      bGPDspoint *pt = &gps_new->points[0];
      copy_v3_v3(&pt->x, &pt1->x);
      pt->strength = 1.0f;
      pt->pressure = 1.0f;

      pt = &gps_new->points[1];
      pt->strength = 1.0f;
      pt->pressure = 1.0f;
      extrapolate_points_by_length(pt0, pt1, tgpf->fill_extend_fac * 0.1f, &pt->x);

      /* Extend end. */
      pt0 = &gps->points[gps->totpoints - 2];
      pt1 = &gps->points[gps->totpoints - 1];
      gps_new = BKE_gpencil_stroke_new(gps->mat_nr, 2, gps->thickness);
      gps_new->flag |= GP_STROKE_NOFILL | GP_STROKE_TAG;
      BLI_addtail(&gpf->strokes, gps_new);

      pt = &gps_new->points[0];
      copy_v3_v3(&pt->x, &pt1->x);
      pt->strength = 1.0f;
      pt->pressure = 1.0f;

      pt = &gps_new->points[1];
      pt->strength = 1.0f;
      pt->pressure = 1.0f;
      extrapolate_points_by_length(pt0, pt1, tgpf->fill_extend_fac * 0.1f, &pt->x);
    }
  }
}

static void gpencil_update_extend(tGPDfill *tgpf)
{
  gpencil_delete_temp_stroke_extension(tgpf, false);

  if (tgpf->fill_extend_fac > 0.0f) {
    gpencil_create_extensions(tgpf);
  }
  WM_event_add_notifier(tgpf->C, NC_GPENCIL | NA_EDITED, NULL);
}

static bool gpencil_stroke_is_drawable(tGPDfill *tgpf, bGPDstroke *gps)
{
  if (tgpf->is_render) {
    return true;
  }

  const bool show_help = (tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) != 0;
  const bool show_extend = (tgpf->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES) != 0;
  const bool is_extend = (gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_TAG);

  if ((!show_help) && (show_extend)) {
    if (!is_extend) {
      return false;
    }
  }

  if ((show_help) && (!show_extend)) {
    if (is_extend) {
      return false;
    }
  }

  return true;
}

/* draw a given stroke using same thickness and color for all points */
static void gpencil_draw_basic_stroke(tGPDfill *tgpf,
                                      bGPDstroke *gps,
                                      const float diff_mat[4][4],
                                      const bool cyclic,
                                      const float ink[4],
                                      const int flag,
                                      const float thershold,
                                      const float thickness)
{
  bGPDspoint *points = gps->points;

  Material *ma = tgpf->mat;
  MaterialGPencilStyle *gp_style = ma->gp_style;

  int totpoints = gps->totpoints;
  float fpt[3];
  float col[4];
  const float extend_col[4] = {0.0f, 1.0f, 1.0f, 1.0f};
  const bool is_extend = (gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_TAG);

  if (!gpencil_stroke_is_drawable(tgpf, gps)) {
    return;
  }

  if ((is_extend) && (!tgpf->is_render)) {
    copy_v4_v4(col, extend_col);
  }
  else {
    copy_v4_v4(col, ink);
  }
  /* if cyclic needs more vertex */
  int cyclic_add = (cyclic) ? 1 : 0;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

  /* draw stroke curve */
  GPU_line_width((!is_extend) ? thickness : thickness * 2.0f);
  immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints + cyclic_add);
  const bGPDspoint *pt = points;

  for (int i = 0; i < totpoints; i++, pt++) {

    if (flag & GP_BRUSH_FILL_HIDE) {
      float alpha = gp_style->stroke_rgba[3] * pt->strength;
      CLAMP(alpha, 0.0f, 1.0f);
      col[3] = alpha <= thershold ? 0.0f : 1.0f;
    }
    else {
      col[3] = 1.0f;
    }
    /* set point */
    immAttr4fv(color, col);
    mul_v3_m4v3(fpt, diff_mat, &pt->x);
    immVertex3fv(pos, fpt);
  }

  if (cyclic && totpoints > 2) {
    /* draw line to first point to complete the cycle */
    immAttr4fv(color, col);
    mul_v3_m4v3(fpt, diff_mat, &points->x);
    immVertex3fv(pos, fpt);
  }

  immEnd();
  immUnbindProgram();
}

static void draw_mouse_position(tGPDfill *tgpf)
{
  if (tgpf->gps_mouse == NULL) {
    return;
  }
  uchar mouse_color[4] = {0, 0, 255, 255};

  bGPDspoint *pt = &tgpf->gps_mouse->points[0];
  float point_size = (tgpf->zoom == 1.0f) ? 4.0f * tgpf->fill_factor :
                                            (0.5f * tgpf->zoom) + tgpf->fill_factor;
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  /* Draw mouse click position in Blue. */
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);
  GPU_point_size(point_size);
  immBegin(GPU_PRIM_POINTS, 1);
  immAttr4ubv(col, mouse_color);
  immVertex3fv(pos, &pt->x);
  immEnd();
  immUnbindProgram();
}

/* Helper: Check if must skip the layer */
bool skip_layer_check(short fill_layer_mode, int gpl_active_index, int gpl_index)
{
  bool skip = false;

  switch (fill_layer_mode) {
    case GP_FILL_GPLMODE_ACTIVE: {
      if (gpl_index != gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ABOVE: {
      if (gpl_index != gpl_active_index + 1) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_BELOW: {
      if (gpl_index != gpl_active_index - 1) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ALL_ABOVE: {
      if (gpl_index <= gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ALL_BELOW: {
      if (gpl_index >= gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_VISIBLE:
    default:
      break;
  }

  return skip;
}

/* Loop all layers to draw strokes. */
static void gpencil_draw_datablock(tGPDfill *tgpf, const float ink[4])
{
  Object *ob = tgpf->ob;
  bGPdata *gpd = tgpf->gpd;
  Brush *brush = tgpf->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  ToolSettings *ts = tgpf->scene->toolsettings;

  tGPDdraw tgpw;
  tgpw.rv3d = tgpf->rv3d;
  tgpw.depsgraph = tgpf->depsgraph;
  tgpw.ob = ob;
  tgpw.gpd = gpd;
  tgpw.offsx = 0;
  tgpw.offsy = 0;
  tgpw.winx = tgpf->sizex;
  tgpw.winy = tgpf->sizey;
  tgpw.dflag = 0;
  tgpw.disable_fill = 1;
  tgpw.dflag |= (GP_DRAWFILLS_ONLY3D | GP_DRAWFILLS_NOSTATUS);

  GPU_blend(GPU_BLEND_ALPHA);

  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  BLI_assert(gpl_active != NULL);

  const int gpl_active_index = BLI_findindex(&gpd->layers, gpl_active);
  BLI_assert(gpl_active_index >= 0);

  /* Draw blue point where click with mouse. */
  draw_mouse_position(tgpf);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* do not draw layer if hidden */
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* calculate parent position */
    BKE_gpencil_layer_transform_matrix_get(tgpw.depsgraph, ob, gpl, tgpw.diff_mat);

    /* Decide if the strokes of layers are included or not depending on the layer mode.
     * Cannot skip the layer because it can use boundary strokes and must be used. */
    const int gpl_index = BLI_findindex(&gpd->layers, gpl);
    bool skip = skip_layer_check(brush_settings->fill_layer_mode, gpl_active_index, gpl_index);

    /* if active layer and no keyframe, create a new one */
    if (gpl == tgpf->gpl) {
      if ((gpl->actframe == NULL) || (gpl->actframe->framenum != tgpf->active_cfra)) {
        short add_frame_mode;
        if (IS_AUTOKEY_ON(tgpf->scene)) {
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

        BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, add_frame_mode);
      }
    }

    /* get frame to draw */
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }

    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      /* check if stroke can be drawn */
      if ((gps->points == NULL) || (gps->totpoints < 2)) {
        continue;
      }
      /* check if the color is visible */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE)) {
        continue;
      }

      /* If the layer must be skipped, but the stroke is not boundary, skip stroke. */
      if ((skip) && ((gps->flag & GP_STROKE_NOFILL) == 0)) {
        continue;
      }

      tgpw.gps = gps;
      tgpw.gpl = gpl;
      tgpw.gpf = gpf;
      tgpw.t_gpf = gpf;

      tgpw.is_fill_stroke = (tgpf->fill_draw_mode == GP_FILL_DMODE_CONTROL) ? false : true;
      /* Reduce thickness to avoid gaps. */
      tgpw.lthick = gpl->line_change;
      tgpw.opacity = 1.0;
      copy_v4_v4(tgpw.tintcolor, ink);
      tgpw.onion = true;
      tgpw.custonion = true;

      /* Normal strokes. */
      if (ELEM(tgpf->fill_draw_mode, GP_FILL_DMODE_STROKE, GP_FILL_DMODE_BOTH)) {
        if (gpencil_stroke_is_drawable(tgpf, gps) && ((gps->flag & GP_STROKE_TAG) == 0)) {
          ED_gpencil_draw_fill(&tgpw);
        }
      }

      /* 3D Lines with basic shapes and invisible lines */
      if (ELEM(tgpf->fill_draw_mode, GP_FILL_DMODE_CONTROL, GP_FILL_DMODE_BOTH)) {
        gpencil_draw_basic_stroke(tgpf,
                                  gps,
                                  tgpw.diff_mat,
                                  gps->flag & GP_STROKE_CYCLIC,
                                  ink,
                                  tgpf->flag,
                                  tgpf->fill_threshold,
                                  1.0f);
      }
    }
  }

  GPU_blend(GPU_BLEND_NONE);
}

/* Draw strokes in off-screen buffer. */
static bool gpencil_render_offscreen(tGPDfill *tgpf)
{
  bool is_ortho = false;
  float winmat[4][4];

  if (!tgpf->gpd) {
    return false;
  }

  /* set temporary new size */
  tgpf->bwinx = tgpf->region->winx;
  tgpf->bwiny = tgpf->region->winy;
  tgpf->brect = tgpf->region->winrct;

  /* resize region */
  tgpf->region->winrct.xmin = 0;
  tgpf->region->winrct.ymin = 0;
  tgpf->region->winrct.xmax = max_ii((int)tgpf->region->winx * tgpf->fill_factor, MIN_WINDOW_SIZE);
  tgpf->region->winrct.ymax = max_ii((int)tgpf->region->winy * tgpf->fill_factor, MIN_WINDOW_SIZE);
  tgpf->region->winx = (short)abs(tgpf->region->winrct.xmax - tgpf->region->winrct.xmin);
  tgpf->region->winy = (short)abs(tgpf->region->winrct.ymax - tgpf->region->winrct.ymin);

  /* save new size */
  tgpf->sizex = (int)tgpf->region->winx;
  tgpf->sizey = (int)tgpf->region->winy;

  char err_out[256] = "unknown";
  GPUOffScreen *offscreen = GPU_offscreen_create(tgpf->sizex, tgpf->sizey, true, false, err_out);
  if (offscreen == NULL) {
    printf("GPencil - Fill - Unable to create fill buffer\n");
    return false;
  }

  GPU_offscreen_bind(offscreen, true);
  uint flag = IB_rectfloat;
  ImBuf *ibuf = IMB_allocImBuf(tgpf->sizex, tgpf->sizey, 32, flag);

  rctf viewplane;
  float clip_start, clip_end;

  is_ortho = ED_view3d_viewplane_get(tgpf->depsgraph,
                                     tgpf->v3d,
                                     tgpf->rv3d,
                                     tgpf->sizex,
                                     tgpf->sizey,
                                     &viewplane,
                                     &clip_start,
                                     &clip_end,
                                     NULL);

  /* Rescale `viewplane` to fit all strokes. */
  float width = viewplane.xmax - viewplane.xmin;
  float height = viewplane.ymax - viewplane.ymin;

  float width_new = width * tgpf->zoom;
  float height_new = height * tgpf->zoom;
  float scale_x = (width_new - width) / 2.0f;
  float scale_y = (height_new - height) / 2.0f;

  viewplane.xmin -= scale_x;
  viewplane.xmax += scale_x;
  viewplane.ymin -= scale_y;
  viewplane.ymax += scale_y;

  if (is_ortho) {
    orthographic_m4(winmat,
                    viewplane.xmin,
                    viewplane.xmax,
                    viewplane.ymin,
                    viewplane.ymax,
                    -clip_end,
                    clip_end);
  }
  else {
    perspective_m4(winmat,
                   viewplane.xmin,
                   viewplane.xmax,
                   viewplane.ymin,
                   viewplane.ymax,
                   clip_start,
                   clip_end);
  }

  GPU_matrix_push_projection();
  GPU_matrix_identity_projection_set();
  GPU_matrix_push();
  GPU_matrix_identity_set();

  GPU_depth_mask(true);
  GPU_clear_color(0.0f, 0.0f, 0.0f, 0.0f);
  GPU_clear_depth(1.0f);

  ED_view3d_update_viewmat(
      tgpf->depsgraph, tgpf->scene, tgpf->v3d, tgpf->region, NULL, winmat, NULL, true);
  /* set for opengl */
  GPU_matrix_projection_set(tgpf->rv3d->winmat);
  GPU_matrix_set(tgpf->rv3d->viewmat);

  /* draw strokes */
  const float ink[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  gpencil_draw_datablock(tgpf, ink);

  GPU_depth_mask(false);

  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  /* create a image to see result of template */
  if (ibuf->rect_float) {
    GPU_offscreen_read_pixels(offscreen, GPU_DATA_FLOAT, ibuf->rect_float);
  }
  else if (ibuf->rect) {
    GPU_offscreen_read_pixels(offscreen, GPU_DATA_UBYTE, ibuf->rect);
  }
  if (ibuf->rect_float && ibuf->rect) {
    IMB_rect_from_float(ibuf);
  }

  tgpf->ima = BKE_image_add_from_imbuf(tgpf->bmain, ibuf, "GP_fill");
  tgpf->ima->id.tag |= LIB_TAG_DOIT;

  BKE_image_release_ibuf(tgpf->ima, ibuf, NULL);

  /* Switch back to window-system-provided frame-buffer. */
  GPU_offscreen_unbind(offscreen, true);
  GPU_offscreen_free(offscreen);

  return true;
}

/* Return pixel data (RGBA) at index. */
static void get_pixel(const ImBuf *ibuf, const int idx, float r_col[4])
{
  BLI_assert(ibuf->rect_float != NULL);
  memcpy(r_col, &ibuf->rect_float[idx * 4], sizeof(float[4]));
}

/* Set pixel data (RGBA) at index. */
static void set_pixel(ImBuf *ibuf, int idx, const float col[4])
{
  BLI_assert(ibuf->rect_float != NULL);
  float *rrectf = &ibuf->rect_float[idx * 4];
  copy_v4_v4(rrectf, col);
}

/* Helper: Check if one image row is empty. */
static bool is_row_filled(const ImBuf *ibuf, const int row_index)
{
  float *row = &ibuf->rect_float[ibuf->x * 4 * row_index];
  return (row[0] == 0.0f && memcmp(row, row + 1, ((ibuf->x * 4) - 1) * sizeof(float)) != 0);
}

/**
 * Check if the size of the leak is narrow to determine if the stroke is closed
 * this is used for strokes with small gaps between them to get a full fill
 * and do not get a full screen fill.
 *
 * This function assumes that if the furthest pixel is occupied,
 * the other pixels are occupied.
 *
 * \param ibuf: Image pixel data.
 * \param maxpixel: Maximum index.
 * \param limit: Limit of pixels to analyze.
 * \param index: Index of current pixel.
 * \param type: 0-Horizontal 1-Vertical.
 */
static bool is_leak_narrow(ImBuf *ibuf, const int maxpixel, int limit, int index, int type)
{
  float rgba[4];
  int pt;
  bool t_a = false;
  bool t_b = false;
  const int extreme = limit - 1;

  /* Horizontal leak (check vertical pixels)
   * X
   * X
   * xB7
   * X
   * X
   */
  if (type == LEAK_HORZ) {
    /* pixels on top */
    pt = index + (ibuf->x * extreme);
    if (pt <= maxpixel) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_a = true;
      }
    }
    else {
      /* edge of image*/
      t_a = true;
    }
    /* pixels on bottom */
    pt = index - (ibuf->x * extreme);
    if (pt >= 0) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_b = true;
      }
    }
    else {
      /* edge of image*/
      t_b = true;
    }
  }

  /* Vertical leak (check horizontal pixels)
   *
   * XXXxB7XX
   */
  if (type == LEAK_VERT) {
    /* get pixel range of the row */
    int row = index / ibuf->x;
    int lowpix = row * ibuf->x;
    int higpix = lowpix + ibuf->x - 1;

    /* pixels to right */
    pt = index - extreme;
    if (pt >= lowpix) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_a = true;
      }
    }
    else {
      t_a = true; /* edge of image*/
    }
    /* pixels to left */
    pt = index + extreme;
    if (pt <= higpix) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_b = true;
      }
    }
    else {
      t_b = true; /* edge of image */
    }
  }
  return (bool)(t_a && t_b);
}

/**
 * Boundary fill inside strokes
 * Fills the space created by a set of strokes using the stroke color as the boundary
 * of the shape to fill.
 *
 * \param tgpf: Temporary fill data.
 */
static bool gpencil_boundaryfill_area(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  float rgba[4];
  void *lock;
  const float fill_col[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  const int maxpixel = (ibuf->x * ibuf->y) - 1;
  bool border_contact = false;

  BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);

  /* Calculate index of the seed point using the position of the mouse looking
   * for a blue pixel. */
  int index = -1;
  for (int i = 0; i < maxpixel; i++) {
    get_pixel(ibuf, i, rgba);
    if (rgba[2] == 1.0f) {
      index = i;
      break;
    }
  }

  if ((index >= 0) && (index <= maxpixel)) {
    if (!FILL_DEBUG) {
      BLI_stack_push(stack, &index);
    }
  }

  /**
   * The fill use a stack to save the pixel list instead of the common recursive
   * 4-contact point method.
   * The problem with recursive calls is that for big fill areas, we can get max limit
   * of recursive calls and STACK_OVERFLOW error.
   *
   * The 4-contact point analyze the pixels to the left, right, bottom and top
   * <pre>
   * -----------
   * |    X    |
   * |   XoX   |
   * |    X    |
   * -----------
   * </pre>
   */
  while (!BLI_stack_is_empty(stack)) {
    int v;

    BLI_stack_pop(stack, &v);

    get_pixel(ibuf, v, rgba);

    /* Determine if the flood contacts with external borders. */
    if (rgba[3] == 0.5f) {
      border_contact = true;
    }

    /* check if no border(red) or already filled color(green) */
    if ((rgba[0] != 1.0f) && (rgba[1] != 1.0f)) {
      /* fill current pixel with green */
      set_pixel(ibuf, v, fill_col);

      /* add contact pixels */
      /* pixel left */
      if (v - 1 >= 0) {
        index = v - 1;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel right */
      if (v + 1 <= maxpixel) {
        index = v + 1;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel top */
      if (v + ibuf->x <= maxpixel) {
        index = v + ibuf->x;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel bottom */
      if (v - ibuf->x >= 0) {
        index = v - ibuf->x;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
          BLI_stack_push(stack, &index);
        }
      }
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
  /* free temp stack data */
  BLI_stack_free(stack);

  return border_contact;
}

/* Set a border to create image limits. */
static void gpencil_set_borders(tGPDfill *tgpf, const bool transparent)
{
  ImBuf *ibuf;
  void *lock;
  const float fill_col[2][4] = {{1.0f, 0.0f, 0.0f, 0.5f}, {0.0f, 0.0f, 0.0f, 0.0f}};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  int idx;
  int pixel = 0;
  const int coloridx = transparent ? 0 : 1;

  /* horizontal lines */
  for (idx = 0; idx < ibuf->x; idx++) {
    /* bottom line */
    set_pixel(ibuf, idx, fill_col[coloridx]);
    /* top line */
    pixel = idx + (ibuf->x * (ibuf->y - 1));
    set_pixel(ibuf, pixel, fill_col[coloridx]);
  }
  /* vertical lines */
  for (idx = 0; idx < ibuf->y; idx++) {
    /* left line */
    set_pixel(ibuf, ibuf->x * idx, fill_col[coloridx]);
    /* right line */
    pixel = ibuf->x * idx + (ibuf->x - 1);
    set_pixel(ibuf, pixel, fill_col[coloridx]);
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/* Invert image to paint inverse area. */
static void gpencil_invert_image(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  void *lock;
  const float fill_col[3][4] = {
      {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);

  const int maxpixel = (ibuf->x * ibuf->y) - 1;
  const int center = ibuf->x / 2;

  for (int v = maxpixel; v != 0; v--) {
    float color[4];
    get_pixel(ibuf, v, color);
    /* Green->Red. */
    if (color[1] == 1.0f) {
      set_pixel(ibuf, v, fill_col[0]);
    }
    /* Red->Green */
    else if (color[0] == 1.0f) {
      set_pixel(ibuf, v, fill_col[1]);
      /* Add thickness of 2 pixels to avoid too thin lines. */
      int offset = (v % ibuf->x < center) ? 1 : -1;
      set_pixel(ibuf, v + offset, fill_col[1]);
    }
    else {
      /* Set to Transparent. */
      set_pixel(ibuf, v, fill_col[2]);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/* Mark and clear processed areas. */
static void gpencil_erase_processed_area(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  void *lock;
  const float blue_col[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  const float clear_col[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  tGPspoint *point2D;

  if (tgpf->sbuffer_used == 0) {
    return;
  }

  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  point2D = (tGPspoint *)tgpf->sbuffer;

  /* First set in blue the perimeter. */
  for (int i = 0; i < tgpf->sbuffer_used && point2D; i++, point2D++) {
    int image_idx = ibuf->x * (int)point2D->y + (int)point2D->x;
    set_pixel(ibuf, image_idx, blue_col);
  }

  /* Second, clean by lines any pixel between blue pixels. */
  float rgba[4];

  for (int idy = 0; idy < ibuf->y; idy++) {
    int init = -1;
    int end = -1;
    for (int idx = 0; idx < ibuf->x; idx++) {
      int image_idx = ibuf->x * idy + idx;
      get_pixel(ibuf, image_idx, rgba);
      /* Blue. */
      if (rgba[2] == 1.0f) {
        if (init < 0) {
          init = image_idx;
        }
        else {
          end = image_idx;
        }
      }
      /* Red. */
      else if (rgba[0] == 1.0f) {
        if (init > -1) {
          for (int i = init; i <= max_ii(init, end); i++) {
            set_pixel(ibuf, i, clear_col);
          }
          init = -1;
          end = -1;
        }
      }
    }
    /* Check last segment. */
    if (init > -1) {
      for (int i = init; i <= max_ii(init, end); i++) {
        set_pixel(ibuf, i, clear_col);
      }
      set_pixel(ibuf, init, clear_col);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/**
 * Naive dilate
 *
 * Expand green areas into enclosing red areas.
 * Using stack prevents creep when replacing colors directly.
 * <pre>
 * -----------
 *  XXXXXXX
 *  XoooooX
 *  XXooXXX
 *   XXXX
 * -----------
 * </pre>
 */
static bool dilate_shape(ImBuf *ibuf)
{
  bool done = false;

  BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);
  const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  // const int maxpixel = (ibuf->x * ibuf->y) - 1;
  /* detect pixels and expand into red areas */
  for (int row = 0; row < ibuf->y; row++) {
    if (!is_row_filled(ibuf, row)) {
      continue;
    }
    int maxpixel = (ibuf->x * (row + 1)) - 1;
    int minpixel = ibuf->x * row;

    for (int v = maxpixel; v != minpixel; v--) {
      float color[4];
      int index;
      get_pixel(ibuf, v, color);
      if (color[1] == 1.0f) {
        int tp = 0;
        int bm = 0;
        int lt = 0;
        int rt = 0;

        /* pixel left */
        if (v - 1 >= 0) {
          index = v - 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
            lt = index;
          }
        }
        /* pixel right */
        if (v + 1 <= maxpixel) {
          index = v + 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
            rt = index;
          }
        }
        /* pixel top */
        if (v + (ibuf->x * 1) <= maxpixel) {
          index = v + (ibuf->x * 1);
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
            tp = index;
          }
        }
        /* pixel bottom */
        if (v - (ibuf->x * 1) >= 0) {
          index = v - (ibuf->x * 1);
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
            bm = index;
          }
        }
        /* pixel top-left */
        if (tp && lt) {
          index = tp - 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel top-right */
        if (tp && rt) {
          index = tp + 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel bottom-left */
        if (bm && lt) {
          index = bm - 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel bottom-right */
        if (bm && rt) {
          index = bm + 1;
          get_pixel(ibuf, index, color);
          if (color[0] == 1.0f) {
            BLI_stack_push(stack, &index);
          }
        }
      }
    }
  }
  /* set dilated pixels */
  while (!BLI_stack_is_empty(stack)) {
    int v;
    BLI_stack_pop(stack, &v);
    set_pixel(ibuf, v, green);
    done = true;
  }
  BLI_stack_free(stack);

  return done;
}

/* Get the outline points of a shape using Moore Neighborhood algorithm
 *
 * This is a Blender customized version of the general algorithm described
 * in https://en.wikipedia.org/wiki/Moore_neighborhood
 */
static void gpencil_get_outline_points(tGPDfill *tgpf, const bool dilate)
{
  ImBuf *ibuf;
  float rgba[4];
  void *lock;
  int v[2];
  int boundary_co[2];
  int start_co[2];
  int first_co[2] = {-1, -1};
  int backtracked_co[2];
  int current_check_co[2];
  int prev_check_co[2];
  int backtracked_offset[1][2] = {{0, 0}};
  bool first_pixel = false;
  bool start_found = false;
  const int NEIGHBOR_COUNT = 8;

  const int offset[8][2] = {
      {-1, -1},
      {0, -1},
      {1, -1},
      {1, 0},
      {1, 1},
      {0, 1},
      {-1, 1},
      {-1, 0},
  };

  tgpf->stack = BLI_stack_new(sizeof(int[2]), __func__);

  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  int imagesize = ibuf->x * ibuf->y;

  /* Dilate. */
  if (dilate) {
    dilate_shape(ibuf);
  }

  for (int idx = imagesize - 1; idx != 0; idx--) {
    get_pixel(ibuf, idx, rgba);
    if (rgba[1] == 1.0f) {
      boundary_co[0] = idx % ibuf->x;
      boundary_co[1] = idx / ibuf->x;
      copy_v2_v2_int(start_co, boundary_co);
      backtracked_co[0] = (idx - 1) % ibuf->x;
      backtracked_co[1] = (idx - 1) / ibuf->x;
      backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
      backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];
      copy_v2_v2_int(prev_check_co, start_co);

      BLI_stack_push(tgpf->stack, &boundary_co);
      start_found = true;
      break;
    }
  }

  while (start_found) {
    int cur_back_offset = -1;
    for (int i = 0; i < NEIGHBOR_COUNT; i++) {
      if (backtracked_offset[0][0] == offset[i][0] && backtracked_offset[0][1] == offset[i][1]) {
        /* Finding the back-tracked pixel offset index */
        cur_back_offset = i;
        break;
      }
    }

    int loop = 0;
    while (loop < (NEIGHBOR_COUNT - 1) && cur_back_offset != -1) {
      int offset_idx = (cur_back_offset + 1) % NEIGHBOR_COUNT;
      current_check_co[0] = boundary_co[0] + offset[offset_idx][0];
      current_check_co[1] = boundary_co[1] + offset[offset_idx][1];

      int image_idx = ibuf->x * current_check_co[1] + current_check_co[0];
      get_pixel(ibuf, image_idx, rgba);

      /* find next boundary pixel */
      if (rgba[1] == 1.0f) {
        copy_v2_v2_int(boundary_co, current_check_co);
        copy_v2_v2_int(backtracked_co, prev_check_co);
        backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
        backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];

        BLI_stack_push(tgpf->stack, &boundary_co);

        break;
      }
      copy_v2_v2_int(prev_check_co, current_check_co);
      cur_back_offset++;
      loop++;
    }
    /* Current pixel is equal to starting or first pixel. */
    if ((boundary_co[0] == start_co[0] && boundary_co[1] == start_co[1]) ||
        (boundary_co[0] == first_co[0] && boundary_co[1] == first_co[1])) {
      BLI_stack_pop(tgpf->stack, &v);
      break;
    }

    if (!first_pixel) {
      first_pixel = true;
      copy_v2_v2_int(first_co, boundary_co);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);
}

/* Get z-depth array to reproject on surface. */
static void gpencil_get_depth_array(tGPDfill *tgpf)
{
  tGPspoint *ptc;
  ToolSettings *ts = tgpf->scene->toolsettings;
  int totpoints = tgpf->sbuffer_used;
  int i = 0;

  if (totpoints == 0) {
    return;
  }

  /* for surface sketching, need to set the right OpenGL context stuff so that
   * the conversions will project the values correctly...
   */
  if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_VIEW) {
    /* need to restore the original projection settings before packing up */
    view3d_region_operator_needs_opengl(tgpf->win, tgpf->region);
    ED_view3d_autodist_init(tgpf->depsgraph, tgpf->region, tgpf->v3d, 0);

    /* Since strokes are so fine, when using their depth we need a margin
     * otherwise they might get missed. */
    int depth_margin = 0;

    /* get an array of depths, far depths are blended */
    int mval_prev[2] = {0};
    int interp_depth = 0;
    int found_depth = 0;

    tgpf->depth_arr = MEM_mallocN(sizeof(float) * totpoints, "depth_points");

    for (i = 0, ptc = tgpf->sbuffer; i < totpoints; i++, ptc++) {

      int mval_i[2];
      round_v2i_v2fl(mval_i, &ptc->x);

      if ((ED_view3d_autodist_depth(tgpf->region, mval_i, depth_margin, tgpf->depth_arr + i) ==
           0) &&
          (i &&
           (ED_view3d_autodist_depth_seg(
                tgpf->region, mval_i, mval_prev, depth_margin + 1, tgpf->depth_arr + i) == 0))) {
        interp_depth = true;
      }
      else {
        found_depth = true;
      }

      copy_v2_v2_int(mval_prev, mval_i);
    }

    if (found_depth == false) {
      /* Sigh! not much we can do here. Ignore depth in this case. */
      for (i = totpoints - 1; i >= 0; i--) {
        tgpf->depth_arr[i] = 0.9999f;
      }
    }
    else {
      if (interp_depth) {
        interp_sparse_array(tgpf->depth_arr, totpoints, FLT_MAX);
      }
    }
  }
}

/* create array of points using stack as source */
static int gpencil_points_from_stack(tGPDfill *tgpf)
{
  tGPspoint *point2D;
  int totpoints = BLI_stack_count(tgpf->stack);
  if (totpoints == 0) {
    return 0;
  }

  tgpf->sbuffer_used = (short)totpoints;
  tgpf->sbuffer = MEM_callocN(sizeof(tGPspoint) * totpoints, __func__);

  point2D = tgpf->sbuffer;
  while (!BLI_stack_is_empty(tgpf->stack)) {
    int v[2];
    BLI_stack_pop(tgpf->stack, &v);
    copy_v2fl_v2i(&point2D->x, v);
    /* shift points to center of pixel */
    add_v2_fl(&point2D->x, 0.5f);
    point2D->pressure = 1.0f;
    point2D->strength = 1.0f;
    point2D->time = 0.0f;
    point2D++;
  }

  return totpoints;
}

/* create a grease pencil stroke using points in buffer */
static void gpencil_stroke_from_buffer(tGPDfill *tgpf)
{
  ToolSettings *ts = tgpf->scene->toolsettings;
  const char align_flag = ts->gpencil_v3d_align;
  const bool is_depth = (bool)(align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE));
  const bool is_lock_axis_view = (bool)(ts->gp_sculpt.lock_axis == 0);
  const bool is_camera = is_lock_axis_view && (tgpf->rv3d->persp == RV3D_CAMOB) && (!is_depth);

  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  if (brush == NULL) {
    return;
  }

  bGPDspoint *pt;
  MDeformVert *dvert = NULL;
  tGPspoint *point2D;

  if (tgpf->sbuffer_used == 0) {
    return;
  }

  /* Set as done. */
  tgpf->done = true;

  /* Get frame or create a new one. */
  tgpf->gpf = BKE_gpencil_layer_frame_get(tgpf->gpl,
                                          tgpf->active_cfra,
                                          IS_AUTOKEY_ON(tgpf->scene) ? GP_GETFRAME_ADD_NEW :
                                                                       GP_GETFRAME_USE_PREV);

  /* Set frame as selected. */
  tgpf->gpf->flag |= GP_FRAME_SELECT;

  /* create new stroke */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "bGPDstroke");
  gps->thickness = brush->size;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = brush->gpencil_settings->hardeness;
  copy_v2_v2(gps->aspect_ratio, brush->gpencil_settings->aspect_ratio);
  gps->inittime = 0.0f;

  /* Apply the vertex color to fill. */
  ED_gpencil_fill_vertex_color_set(ts, brush, gps);

  /* the polygon must be closed, so enabled cyclic */
  gps->flag |= GP_STROKE_CYCLIC;
  gps->flag |= GP_STROKE_3DSPACE;

  gps->mat_nr = BKE_gpencil_object_material_get_index_from_brush(tgpf->ob, brush);
  if (gps->mat_nr < 0) {
    if (tgpf->ob->actcol - 1 < 0) {
      gps->mat_nr = 0;
    }
    else {
      gps->mat_nr = tgpf->ob->actcol - 1;
    }
  }

  /* allocate memory for storage points */
  gps->totpoints = tgpf->sbuffer_used;
  gps->points = MEM_callocN(sizeof(bGPDspoint) * tgpf->sbuffer_used, "gp_stroke_points");

  /* add stroke to frame */
  if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) || (tgpf->on_back == true)) {
    BLI_addhead(&tgpf->gpf->strokes, gps);
  }
  else {
    BLI_addtail(&tgpf->gpf->strokes, gps);
  }

  /* add points */
  pt = gps->points;
  point2D = (tGPspoint *)tgpf->sbuffer;

  const int def_nr = tgpf->ob->actdef - 1;
  const bool have_weight = (bool)BLI_findlink(&tgpf->ob->defbase, def_nr);

  if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
    BKE_gpencil_dvert_ensure(gps);
    dvert = gps->dvert;
  }

  for (int i = 0; i < tgpf->sbuffer_used && point2D; i++, point2D++, pt++) {
    /* convert screen-coordinates to 3D coordinates */
    gpencil_stroke_convertcoords_tpoint(tgpf->scene,
                                        tgpf->region,
                                        tgpf->ob,
                                        point2D,
                                        tgpf->depth_arr ? tgpf->depth_arr + i : NULL,
                                        &pt->x);

    pt->pressure = 1.0f;
    pt->strength = 1.0f;
    pt->time = 0.0f;

    /* Apply the vertex color to point. */
    ED_gpencil_point_vertex_color_set(ts, brush, pt, NULL);

    if ((ts->gpencil_flags & GP_TOOL_FLAG_CREATE_WEIGHTS) && (have_weight)) {
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
      if (dw) {
        dw->weight = ts->vgroup_weight;
      }

      dvert++;
    }
    else {
      if (dvert != NULL) {
        dvert->totweight = 0;
        dvert->dw = NULL;
        dvert++;
      }
    }
  }

  /* smooth stroke */
  float reduce = 0.0f;
  float smoothfac = 1.0f;
  for (int r = 0; r < 1; r++) {
    for (int i = 0; i < gps->totpoints; i++) {
      BKE_gpencil_stroke_smooth(gps, i, smoothfac - reduce);
    }
    reduce += 0.25f; /* reduce the factor */
  }

  /* if axis locked, reproject to plane locked */
  if ((tgpf->lock_axis > GP_LOCKAXIS_VIEW) &&
      ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_VIEW) == 0)) {
    float origin[3];
    ED_gpencil_drawing_reference_get(tgpf->scene, tgpf->ob, ts->gpencil_v3d_align, origin);
    ED_gpencil_project_stroke_to_plane(
        tgpf->scene, tgpf->ob, tgpf->rv3d, tgpf->gpl, gps, origin, tgpf->lock_axis - 1);
  }

  /* if parented change position relative to parent object */
  for (int a = 0; a < tgpf->sbuffer_used; a++) {
    pt = &gps->points[a];
    gpencil_apply_parent_point(tgpf->depsgraph, tgpf->ob, tgpf->gpl, pt);
  }

  /* If camera view or view projection, reproject flat to view to avoid perspective effect. */
  if ((!is_depth) && (((align_flag & GP_PROJECT_VIEWSPACE) && is_lock_axis_view) || (is_camera))) {
    ED_gpencil_project_stroke_to_view(tgpf->C, tgpf->gpl, gps);
  }

  /* simplify stroke */
  for (int b = 0; b < tgpf->fill_simplylvl; b++) {
    BKE_gpencil_stroke_simplify_fixed(tgpf->gpd, gps);
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(tgpf->gpd, gps);
}

/* ----------------------- */
/* Drawing                 */
/* Helper: Draw status message while the user is running the operator */
static void gpencil_fill_status_indicators(bContext *C)
{
  const char *status_str = TIP_("Fill: ESC/RMB cancel, LMB Fill, Shift Draw on Back");
  ED_workspace_status_text(C, status_str);
}

/* draw boundary lines to see fill limits */
static void gpencil_draw_boundary_lines(const bContext *UNUSED(C), tGPDfill *tgpf)
{
  if (!tgpf->gpd) {
    return;
  }
  const float ink[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  gpencil_draw_datablock(tgpf, ink);
}

/* Drawing callback for modal operator in 3d mode */
static void gpencil_fill_draw_3d(const bContext *C, ARegion *UNUSED(region), void *arg)
{
  tGPDfill *tgpf = (tGPDfill *)arg;
  /* draw only in the region that originated operator. This is required for multiwindow */
  ARegion *region = CTX_wm_region(C);
  if (region != tgpf->region) {
    return;
  }
  gpencil_draw_boundary_lines(C, tgpf);
}

/* check if context is suitable for filling */
static bool gpencil_fill_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  if (ED_operator_regionactive(C)) {
    ScrArea *area = CTX_wm_area(C);
    if (area->spacetype == SPACE_VIEW3D) {
      if ((obact == NULL) || (obact->type != OB_GPENCIL) ||
          (obact->mode != OB_MODE_PAINT_GPENCIL)) {
        return false;
      }

      return true;
    }
    CTX_wm_operator_poll_msg_set(C, "Active region not valid for filling operator");
    return false;
  }

  CTX_wm_operator_poll_msg_set(C, "Active region not set");
  return false;
}

/* Allocate memory and initialize values */
static tGPDfill *gpencil_session_init_fill(bContext *C, wmOperator *op)
{
  tGPDfill *tgpf = MEM_callocN(sizeof(tGPDfill), "GPencil Fill Data");

  /* define initial values */
  ToolSettings *ts = CTX_data_tool_settings(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  /* set current scene and window info */
  tgpf->C = C;
  tgpf->bmain = CTX_data_main(C);
  tgpf->scene = scene;
  tgpf->ob = CTX_data_active_object(C);
  tgpf->area = CTX_wm_area(C);
  tgpf->region = CTX_wm_region(C);
  tgpf->rv3d = tgpf->region->regiondata;
  tgpf->v3d = tgpf->area->spacedata.first;
  tgpf->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  tgpf->win = CTX_wm_window(C);
  tgpf->active_cfra = CFRA;
  tgpf->reports = op->reports;

  /* Setup space conversions. */
  gpencil_point_conversion_init(C, &tgpf->gsc);
  tgpf->zoom = 1.0f;

  /* set GP datablock */
  tgpf->gpd = gpd;
  tgpf->gpl = BKE_gpencil_layer_active_get(gpd);
  if (tgpf->gpl == NULL) {
    tgpf->gpl = BKE_gpencil_layer_addnew(tgpf->gpd, DATA_("GP_Layer"), true);
  }

  tgpf->lock_axis = ts->gp_sculpt.lock_axis;

  tgpf->oldkey = -1;
  tgpf->is_render = false;
  tgpf->sbuffer_used = 0;
  tgpf->sbuffer = NULL;
  tgpf->depth_arr = NULL;

  /* save filling parameters */
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  tgpf->brush = brush;
  tgpf->flag = brush->gpencil_settings->flag;
  tgpf->fill_threshold = brush->gpencil_settings->fill_threshold;
  tgpf->fill_simplylvl = brush->gpencil_settings->fill_simplylvl;
  tgpf->fill_draw_mode = brush->gpencil_settings->fill_draw_mode;
  tgpf->fill_extend_fac = brush->gpencil_settings->fill_extend_fac;
  tgpf->fill_factor = max_ff(GPENCIL_MIN_FILL_FAC,
                             min_ff(brush->gpencil_settings->fill_factor, GPENCIL_MAX_FILL_FAC));
  tgpf->fill_leak = (int)ceil((float)brush->gpencil_settings->fill_leak * tgpf->fill_factor);

  int totcol = tgpf->ob->totcol;

  /* get color info */
  Material *ma = BKE_gpencil_object_material_ensure_from_active_input_brush(
      bmain, tgpf->ob, brush);

  tgpf->mat = ma;

  /* Untag strokes to be sure nothing is pending due any canceled process. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &tgpf->gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        gps->flag &= ~GP_STROKE_TAG;
      }
    }
  }

  /* check whether the material was newly added */
  if (totcol != tgpf->ob->totcol) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_PROPERTIES, NULL);
  }

  /* init undo */
  gpencil_undo_init(tgpf->gpd);

  /* return context data for running operator */
  return tgpf;
}

/* end operator */
static void gpencil_fill_exit(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  /* clear undo stack */
  gpencil_undo_finish();

  /* restore cursor to indicate end of fill */
  WM_cursor_modal_restore(CTX_wm_window(C));

  tGPDfill *tgpf = op->customdata;

  /* don't assume that operator data exists at all */
  if (tgpf) {
    /* clear status message area */
    ED_workspace_status_text(C, NULL);

    MEM_SAFE_FREE(tgpf->sbuffer);
    MEM_SAFE_FREE(tgpf->depth_arr);

    /* Remove any temp stroke. */
    gpencil_delete_temp_stroke_extension(tgpf, true);

    /* remove drawing handler */
    if (tgpf->draw_handle_3d) {
      ED_region_draw_cb_exit(tgpf->region->type, tgpf->draw_handle_3d);
    }

    /* finally, free memory used by temp data */
    MEM_freeN(tgpf);
  }

  /* clear pointer */
  op->customdata = NULL;

  /* drawing batch cache is dirty now */
  if ((ob) && (ob->type == OB_GPENCIL) && (ob->data)) {
    bGPdata *gpd2 = ob->data;
    DEG_id_tag_update(&gpd2->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    gpd2->flag |= GP_DATA_CACHE_IS_DIRTY;
  }

  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

static void gpencil_fill_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  gpencil_fill_exit(C, op);
}

/* Init: Allocate memory and set init values */
static int gpencil_fill_init(bContext *C, wmOperator *op)
{
  tGPDfill *tgpf;
  /* cannot paint in locked layer */
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  if ((gpl) && (gpl->flag & GP_LAYER_LOCKED)) {
    return 0;
  }

  /* check context */
  tgpf = op->customdata = gpencil_session_init_fill(C, op);
  if (tgpf == NULL) {
    /* something wasn't set correctly in context */
    gpencil_fill_exit(C, op);
    return 0;
  }

  /* everything is now setup ok */
  return 1;
}

/* start of interactive part of operator */
static int gpencil_fill_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
  tGPDfill *tgpf = NULL;

  /* Fill tool needs a material (cannot use default material) */
  bool valid = true;
  if ((brush) && (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED)) {
    if (brush->gpencil_settings->material == NULL) {
      valid = false;
    }
  }
  else {
    if (BKE_object_material_get(ob, ob->actcol) == NULL) {
      valid = false;
    }
  }
  if (!valid) {
    BKE_report(op->reports, RPT_ERROR, "Fill tool needs active material");
    return OPERATOR_CANCELLED;
  }

  /* try to initialize context data needed */
  if (!gpencil_fill_init(C, op)) {
    gpencil_fill_exit(C, op);
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    return OPERATOR_CANCELLED;
  }

  tgpf = op->customdata;

  /* Enable custom drawing handlers to show help lines */
  const bool do_extend = (tgpf->fill_extend_fac > 0.0f);
  const bool help_lines = ((tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) ||
                           ((tgpf->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES) && (do_extend)));

  if (help_lines) {
    tgpf->draw_handle_3d = ED_region_draw_cb_activate(
        tgpf->region->type, gpencil_fill_draw_3d, tgpf, REGION_DRAW_POST_VIEW);
  }

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_PAINT_BRUSH);

  gpencil_fill_status_indicators(C);

  DEG_id_tag_update(&tgpf->gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* add a modal handler for this operator*/
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Helper: Calc the maximum bounding box size of strokes to get the zoom level of the viewport.
 * For each stroke, the 2D projected bounding box is calculated and using this data, the total
 * object bounding box (all strokes) is calculated. */
static void gpencil_zoom_level_set(tGPDfill *tgpf)
{
  Brush *brush = tgpf->brush;
  if (brush->gpencil_settings->flag & GP_BRUSH_FILL_FIT_DISABLE) {
    tgpf->zoom = 1.0f;
    return;
  }

  Object *ob = tgpf->ob;
  bGPdata *gpd = tgpf->gpd;
  BrushGpencilSettings *brush_settings = tgpf->brush->gpencil_settings;
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  BLI_assert(gpl_active != NULL);

  const int gpl_active_index = BLI_findindex(&gpd->layers, gpl_active);
  BLI_assert(gpl_active_index >= 0);

  /* Init maximum boundbox size. */
  rctf rect_max;
  const float winx_half = tgpf->region->winx / 2.0f;
  const float winy_half = tgpf->region->winy / 2.0f;
  BLI_rctf_init(&rect_max,
                0.0f - winx_half,
                tgpf->region->winx + winx_half,
                0.0f - winy_half,
                tgpf->region->winy + winy_half);

  float objectbox_min[2], objectbox_max[2];
  INIT_MINMAX2(objectbox_min, objectbox_max);
  rctf rect_bound;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    float diff_mat[4][4];
    /* calculate parent matrix */
    BKE_gpencil_layer_transform_matrix_get(tgpf->depsgraph, ob, gpl, diff_mat);

    /* Decide if the strokes of layers are included or not depending on the layer mode.
     * Cannot skip the layer because it can use boundary strokes and must be used. */
    const int gpl_index = BLI_findindex(&gpd->layers, gpl);
    bool skip = skip_layer_check(brush_settings->fill_layer_mode, gpl_active_index, gpl_index);

    /* Get frame to check. */
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }

    /* Read all strokes. */
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      /* check if stroke can be drawn */
      if ((gps->points == NULL) || (gps->totpoints < 2)) {
        continue;
      }
      /* check if the color is visible */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE)) {
        continue;
      }

      /* If the layer must be skipped, but the stroke is not boundary, skip stroke. */
      if ((skip) && ((gps->flag & GP_STROKE_NOFILL) == 0)) {
        continue;
      }

      float boundbox_min[2];
      float boundbox_max[2];
      ED_gpencil_projected_2d_bound_box(&tgpf->gsc, gps, diff_mat, boundbox_min, boundbox_max);
      minmax_v2v2_v2(objectbox_min, objectbox_max, boundbox_min);
      minmax_v2v2_v2(objectbox_min, objectbox_max, boundbox_max);
    }
  }
  /* Clamp max bound box. */
  BLI_rctf_init(
      &rect_bound, objectbox_min[0], objectbox_max[0], objectbox_min[1], objectbox_max[1]);
  float r_xy[2];
  BLI_rctf_clamp(&rect_bound, &rect_max, r_xy);

  /* Calculate total width used. */
  float width = tgpf->region->winx;
  if (rect_bound.xmin < 0.0f) {
    width -= rect_bound.xmin;
  }
  if (rect_bound.xmax > tgpf->region->winx) {
    width += rect_bound.xmax - tgpf->region->winx;
  }
  /* Calculate total height used. */
  float height = tgpf->region->winy;
  if (rect_bound.ymin < 0.0f) {
    height -= rect_bound.ymin;
  }
  if (rect_bound.ymax > tgpf->region->winy) {
    height += rect_bound.ymax - tgpf->region->winy;
  }

  width = ceilf(width);
  height = ceilf(height);

  float zoomx = (width > tgpf->region->winx) ? width / (float)tgpf->region->winx : 1.0f;
  float zoomy = (height > tgpf->region->winy) ? height / (float)tgpf->region->winy : 1.0f;
  if ((zoomx != 1.0f) || (zoomy != 1.0f)) {
    tgpf->zoom = min_ff(max_ff(zoomx, zoomy) * 1.5f, 5.0f);
  }
}

static bool gpencil_do_frame_fill(tGPDfill *tgpf, const bool is_inverted)
{
  wmWindow *win = CTX_wm_window(tgpf->C);

  /* render screen to temp image */
  int totpoints = 1;
  if (gpencil_render_offscreen(tgpf)) {

    /* Set red borders to create a external limit. */
    gpencil_set_borders(tgpf, true);

    /* apply boundary fill */
    const bool border_contact = gpencil_boundaryfill_area(tgpf);

    /* Fill only if it never comes in contact with an edge. It is better not to fill than
     * to fill the entire area, as this is confusing for the artist. */
    if ((!border_contact) || (is_inverted)) {
      /* Invert direction if press Ctrl. */
      if (is_inverted) {
        gpencil_invert_image(tgpf);
      }

      /* Clean borders to avoid infinite loops. */
      gpencil_set_borders(tgpf, false);
      WM_cursor_time(win, 50);
      int totpoints_prv = 0;
      int loop_limit = 0;
      while (totpoints > 0) {
        /* analyze outline */
        gpencil_get_outline_points(tgpf, (totpoints == 1) ? true : false);

        /* create array of points from stack */
        totpoints = gpencil_points_from_stack(tgpf);

        /* create z-depth array for reproject */
        gpencil_get_depth_array(tgpf);

        /* create stroke and reproject */
        gpencil_stroke_from_buffer(tgpf);

        if (is_inverted) {
          gpencil_erase_processed_area(tgpf);
        }
        else {
          /* Exit of the loop. */
          totpoints = 0;
        }

        /* free temp stack data */
        if (tgpf->stack) {
          BLI_stack_free(tgpf->stack);
        }
        WM_cursor_time(win, 100);

        /* Free memory. */
        MEM_SAFE_FREE(tgpf->sbuffer);
        MEM_SAFE_FREE(tgpf->depth_arr);

        /* Limit very small areas. */
        if (totpoints < 3) {
          break;
        }
        /* Limit infinite loops is some corner cases. */
        if (totpoints_prv == totpoints) {
          loop_limit++;
          if (loop_limit > 3) {
            break;
          }
        }
        totpoints_prv = totpoints;
      }
    }
    else {
      BKE_report(tgpf->reports, RPT_INFO, "Unable to fill unclosed areas");
    }

    /* Delete temp image. */
    if ((tgpf->ima) && (!FILL_DEBUG)) {
      BKE_id_free(tgpf->bmain, tgpf->ima);
    }

    return true;
  }

  return false;
}

/* events handling during interactive part of operator */
static int gpencil_fill_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPDfill *tgpf = op->customdata;
  Brush *brush = tgpf->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  tgpf->on_back = RNA_boolean_get(op->ptr, "on_back");

  const bool is_brush_inv = brush_settings->fill_direction == BRUSH_DIR_IN;
  const bool is_inverted = (is_brush_inv && !event->ctrl) || (!is_brush_inv && event->ctrl);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(tgpf->gpd);
  const bool do_extend = (tgpf->fill_extend_fac > 0.0f);
  const bool help_lines = ((tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) ||
                           ((tgpf->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES) && (do_extend)));
  int estate = OPERATOR_RUNNING_MODAL;

  switch (event->type) {
    case EVT_ESCKEY:
    case RIGHTMOUSE:
      estate = OPERATOR_CANCELLED;
      break;
    case LEFTMOUSE:
      if (!IS_AUTOKEY_ON(tgpf->scene) && (!is_multiedit) && (tgpf->gpl->actframe == NULL)) {
        BKE_report(op->reports, RPT_INFO, "No available frame for creating stroke");
        estate = OPERATOR_CANCELLED;
        break;
      }

      /* first time the event is not enabled to show help lines. */
      if ((tgpf->oldkey != -1) || (!help_lines)) {
        ARegion *region = BKE_area_find_region_xy(
            CTX_wm_area(C), RGN_TYPE_ANY, event->x, event->y);
        if (region) {
          bool in_bounds = false;
          /* Perform bounds check */
          in_bounds = BLI_rcti_isect_pt(&region->winrct, event->x, event->y);

          if ((in_bounds) && (region->regiontype == RGN_TYPE_WINDOW)) {
            tgpf->mouse[0] = event->mval[0];
            tgpf->mouse[1] = event->mval[1];
            tgpf->is_render = true;
            /* Define Zoom level. */
            gpencil_zoom_level_set(tgpf);

            /* Create Temp stroke. */
            tgpf->gps_mouse = BKE_gpencil_stroke_new(0, 1, 10.0f);
            tGPspoint point2D;
            bGPDspoint *pt = &tgpf->gps_mouse->points[0];
            copy_v2fl_v2i(&point2D.x, tgpf->mouse);
            gpencil_stroke_convertcoords_tpoint(
                tgpf->scene, tgpf->region, tgpf->ob, &point2D, NULL, &pt->x);

            /* Hash of selected frames.*/
            GHash *frame_list = BLI_ghash_int_new_ex(__func__, 64);

            /* If not multiframe and there is no frame in CFRA for the active layer, create
             * a new frame. */
            if (!is_multiedit) {
              tgpf->gpf = BKE_gpencil_layer_frame_get(
                  tgpf->gpl,
                  tgpf->active_cfra,
                  IS_AUTOKEY_ON(tgpf->scene) ? GP_GETFRAME_ADD_NEW : GP_GETFRAME_USE_PREV);
              tgpf->gpf->flag |= GP_FRAME_SELECT;

              BLI_ghash_insert(
                  frame_list, POINTER_FROM_INT(tgpf->active_cfra), tgpf->gpl->actframe);
            }
            else {
              BKE_gpencil_frame_selected_hash(tgpf->gpd, frame_list);
            }

            /* Loop all frames. */
            wmWindow *win = CTX_wm_window(C);

            GHashIterator gh_iter;
            int total = BLI_ghash_len(frame_list);
            int i = 1;
            GHASH_ITER (gh_iter, frame_list) {
              /* Set active frame as current for filling. */
              tgpf->active_cfra = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
              int step = ((float)i / (float)total) * 100.0f;
              WM_cursor_time(win, step);

              if (do_extend) {
                gpencil_update_extend(tgpf);
              }

              /* Repeat loop until get something. */
              tgpf->done = false;
              int loop_limit = 0;
              while ((!tgpf->done) && (loop_limit < 2)) {
                WM_cursor_time(win, loop_limit + 1);
                /* Render screen to temp image and do fill. */
                gpencil_do_frame_fill(tgpf, is_inverted);

                /* restore size */
                tgpf->region->winx = (short)tgpf->bwinx;
                tgpf->region->winy = (short)tgpf->bwiny;
                tgpf->region->winrct = tgpf->brect;
                if (!tgpf->done) {
                  /* If the zoom was not set before, avoid a loop. */
                  if (tgpf->zoom == 1.0f) {
                    loop_limit++;
                  }
                  else {
                    tgpf->zoom = 1.0f;
                    tgpf->fill_factor = max_ff(
                        GPENCIL_MIN_FILL_FAC,
                        min_ff(brush->gpencil_settings->fill_factor, GPENCIL_MAX_FILL_FAC));
                  }
                }
                loop_limit++;
              }

              if (do_extend) {
                gpencil_delete_temp_stroke_extension(tgpf, true);
              }

              i++;
            }
            WM_cursor_modal_restore(win);
            /* Free hash table. */
            BLI_ghash_free(frame_list, NULL, NULL);

            /* Free temp stroke. */
            BKE_gpencil_free_stroke(tgpf->gps_mouse);

            /* push undo data */
            gpencil_undo_push(tgpf->gpd);

            /* Save extend value for next operation. */
            brush_settings->fill_extend_fac = tgpf->fill_extend_fac;

            estate = OPERATOR_FINISHED;
          }
          else {
            estate = OPERATOR_CANCELLED;
          }
        }
        else {
          estate = OPERATOR_CANCELLED;
        }
      }
      else if (do_extend) {
        gpencil_update_extend(tgpf);
      }
      tgpf->oldkey = event->type;
      break;
    case EVT_PAGEUPKEY:
    case WHEELUPMOUSE:
      if (tgpf->oldkey == 1) {
        tgpf->fill_extend_fac -= (event->shift) ? 0.01f : 0.1f;
        CLAMP_MIN(tgpf->fill_extend_fac, 0.0f);
        gpencil_update_extend(tgpf);
      }
      break;
    case EVT_PAGEDOWNKEY:
    case WHEELDOWNMOUSE:
      if (tgpf->oldkey == 1) {
        tgpf->fill_extend_fac += (event->shift) ? 0.01f : 0.1f;
        CLAMP_MAX(tgpf->fill_extend_fac, 100.0f);
        gpencil_update_extend(tgpf);
      }
      break;
    default:
      break;
  }
  /* process last operations before exiting */
  switch (estate) {
    case OPERATOR_FINISHED:
      gpencil_fill_exit(C, op);
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
      break;

    case OPERATOR_CANCELLED:
      gpencil_fill_exit(C, op);
      break;

    default:
      break;
  }

  /* return status code */
  return estate;
}

void GPENCIL_OT_fill(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Grease Pencil Fill";
  ot->idname = "GPENCIL_OT_fill";
  ot->description = "Fill with color the shape formed by strokes";

  /* api callbacks */
  ot->invoke = gpencil_fill_invoke;
  ot->modal = gpencil_fill_modal;
  ot->poll = gpencil_fill_poll;
  ot->cancel = gpencil_fill_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  prop = RNA_def_boolean(ot->srna, "on_back", false, "Draw on Back", "Send new stroke to back");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
