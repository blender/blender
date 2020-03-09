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
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "DNA_meshdata_types.h"
#include "BKE_object_deform.h"
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
typedef struct tGP_BrushWeightpaintData {
  struct Main *bmain;
  Scene *scene;
  Object *object;

  ARegion *region;

  /* Current GPencil datablock */
  bGPdata *gpd;

  Brush *brush;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /* Is the brush currently painting? */
  bool is_painting;

  /* Start of new paint */
  bool first;

  /* Is multiframe editing enabled, and are we using falloff for that? */
  bool is_multiframe;
  bool use_multiframe_falloff;

  /* active vertex group */
  int vrgroup;

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
} tGP_BrushWeightpaintData;

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

/* Compute strength of effect. */
static float brush_influence_calc(tGP_BrushWeightpaintData *gso, const int radius, const int co[2])
{
  Brush *brush = gso->brush;

  /* basic strength factor from brush settings */
  float influence = brush->alpha;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mval_i[2];
  round_v2i_v2fl(mval_i, gso->mval);
  float distance = (float)len_v2v2_int(mval_i, co);
  influence *= 1.0f - (distance / max_ff(radius, 1e-8));

  /* Apply Brush curve. */
  float brush_fallof = BKE_brush_curve_strength(brush, distance, (float)radius);
  influence *= brush_fallof;

  /* apply multiframe falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}

/* Compute effect vector for directional brushes. */
static void brush_calc_dvec_2d(tGP_BrushWeightpaintData *gso)
{
  gso->dvec[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
  gso->dvec[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

  normalize_v2(gso->dvec);
}

/* ************************************************ */
/* Brush Callbacks
 * This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius. */

/* Draw Brush */
static bool brush_draw_apply(tGP_BrushWeightpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  /* create dvert */
  BKE_gpencil_dvert_ensure(gps);

  MDeformVert *dvert = gps->dvert + pt_index;
  float inf;

  /* Compute strength of effect */
  inf = brush_influence_calc(gso, radius, co);

  /* need a vertex group */
  if (gso->vrgroup == -1) {
    if (gso->object) {
      BKE_object_defgroup_add(gso->object);
      DEG_relations_tag_update(gso->bmain);
      gso->vrgroup = 0;
    }
  }
  else {
    bDeformGroup *defgroup = BLI_findlink(&gso->object->defbase, gso->vrgroup);
    if (defgroup->flag & DG_LOCK_WEIGHT) {
      return false;
    }
  }
  /* Get current weight and blend. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw) {
    dw->weight = interpf(gso->brush->weight, dw->weight, inf);
    CLAMP(dw->weight, 0.0f, 1.0f);
  }
  return true;
}

/* ************************************************ */
/* Header Info */
static void gp_weightpaint_brush_header_set(bContext *C)
{
  ED_workspace_status_text(C, TIP_("GPencil Weight Paint: LMB to paint | RMB/Escape to Exit"));
}

/* ************************************************ */
/* Grease Pencil Weight Paint Operator */

/* Init/Exit ----------------------------------------------- */

static bool gp_weightpaint_brush_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  Paint *paint = &ts->gp_weightpaint->paint;

  /* set the brush using the tool */
  tGP_BrushWeightpaintData *gso;

  /* setup operator data */
  gso = MEM_callocN(sizeof(tGP_BrushWeightpaintData), "tGP_BrushWeightpaintData");
  op->customdata = gso;

  gso->bmain = CTX_data_main(C);

  gso->brush = paint->brush;
  BKE_curvemapping_initialize(gso->brush->curve);

  gso->is_painting = false;
  gso->first = true;

  gso->pbuffer = NULL;
  gso->pbuffer_size = 0;
  gso->pbuffer_used = 0;

  gso->gpd = ED_gpencil_data_get_active(C);
  gso->scene = scene;
  gso->object = ob;
  if (ob) {
    gso->vrgroup = ob->actdef - 1;
    if (!BLI_findlink(&ob->defbase, gso->vrgroup)) {
      gso->vrgroup = -1;
    }
  }
  else {
    gso->vrgroup = -1;
  }

  gso->region = CTX_wm_region(C);

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
  gp_weightpaint_brush_header_set(C);

  return true;
}

static void gp_weightpaint_brush_exit(bContext *C, wmOperator *op)
{
  tGP_BrushWeightpaintData *gso = op->customdata;

  /* Disable headerprints. */
  ED_workspace_status_text(C, NULL);

  /* Free operator data */
  MEM_SAFE_FREE(gso->pbuffer);
  MEM_SAFE_FREE(gso);
  op->customdata = NULL;
}

/* Poll callback for stroke weight paint operator. */
static bool gp_weightpaint_brush_poll(bContext *C)
{
  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* Helper to save the points selected by the brush. */
static void gp_save_selected_point(tGP_BrushWeightpaintData *gso,
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
  copy_v2_v2_int(selected->pc, pc);
  copy_v4_v4(selected->color, pt->vert_color);

  gso->pbuffer_used++;
}

/* Select points in this stroke and add to an array to be used later. */
static void gp_weightpaint_select_stroke(tGP_BrushWeightpaintData *gso,
                                         bGPDstroke *gps,
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
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* Get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

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
        if (gp_stroke_inside_circle(
                gso->mval, gso->mval_prev, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {

          /* To each point individually... */
          pt = &gps->points[i];
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if (pt_active != NULL) {
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
            gp_save_selected_point(gso, gps_active, index, pc1);

            include_last = false;
          }
        }
      }
    }
  }
}

/* Apply weight paint brushes to strokes in the given frame. */
static bool gp_weightpaint_brush_do_frame(bContext *C,
                                          tGP_BrushWeightpaintData *gso,
                                          bGPDlayer *gpl,
                                          bGPDframe *gpf,
                                          const float diff_mat[4][4])
{
  Object *ob = CTX_data_active_object(C);
  char tool = gso->brush->gpencil_weight_tool;
  const int radius = (gso->brush->flag & GP_BRUSH_USE_PRESSURE) ?
                         gso->brush->size * gso->pressure :
                         gso->brush->size;
  tGP_Selected *selected = NULL;
  int i;

  /*---------------------------------------------------------------------
   * First step: select the points affected. This step is required to have
   * all selected points before apply the effect, because it could be
   * required to do some step. Now is not used, but the operator is ready.
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
    gp_weightpaint_select_stroke(gso, gps, diff_mat);
  }

  /*---------------------------------------------------------------------
   * Second step: Apply effect.
   *--------------------------------------------------------------------- */
  bool changed = false;
  for (i = 0; i < gso->pbuffer_used; i++) {
    changed = true;
    selected = &gso->pbuffer[i];

    switch (tool) {
      case GPWEIGHT_TOOL_DRAW: {
        brush_draw_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        changed |= true;
        break;
      }
      default:
        printf("ERROR: Unknown type of GPencil Weight Paint brush\n");
        break;
    }
  }
  /* Clear the selected array, but keep the memory allocation.*/
  gso->pbuffer = gpencil_select_buffer_ensure(
      gso->pbuffer, &gso->pbuffer_size, &gso->pbuffer_used, true);

  return changed;
}

/* Apply brush effect to all layers. */
static bool gp_weightpaint_brush_apply_to_layers(bContext *C, tGP_BrushWeightpaintData *gso)
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
          /* compute multiframe falloff factor */
          if (gso->use_multiframe_falloff) {
            /* Faloff depends on distance to active frame (relative to the overall frame range)
             */
            gso->mf_falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }
          else {
            /* No falloff */
            gso->mf_falloff = 1.0f;
          }

          /* affect strokes in this frame */
          changed |= gp_weightpaint_brush_do_frame(C, gso, gpl, gpf, diff_mat);
        }
      }
    }
    else {
      if (gpl->actframe != NULL) {
        /* Apply to active frame's strokes */
        gso->mf_falloff = 1.0f;
        changed |= gp_weightpaint_brush_do_frame(C, gso, gpl, gpl->actframe, diff_mat);
      }
    }
  }

  return changed;
}

/* Calculate settings for applying brush */
static void gp_weightpaint_brush_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
  tGP_BrushWeightpaintData *gso = op->customdata;
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

  changed = gp_weightpaint_brush_apply_to_layers(C, gso);

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
static void gp_weightpaint_brush_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = op->customdata;
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
  gp_weightpaint_brush_apply(C, op, &itemptr);
}

/* reapply */
static int gp_weightpaint_brush_exec(bContext *C, wmOperator *op)
{
  if (!gp_weightpaint_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    gp_weightpaint_brush_apply(C, op, &itemptr);
  }
  RNA_END;

  gp_weightpaint_brush_exit(C, op);

  return OPERATOR_FINISHED;
}

/* start modal painting */
static int gp_weightpaint_brush_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = NULL;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  const bool is_playing = ED_screen_animation_playing(CTX_wm_manager(C)) != NULL;

  /* the operator cannot work while play animation */
  if (is_playing) {
    BKE_report(op->reports, RPT_ERROR, "Cannot Paint while play animation");

    return OPERATOR_CANCELLED;
  }

  /* init painting data */
  if (!gp_weightpaint_brush_init(C, op)) {
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
    gp_weightpaint_brush_apply_event(C, op, event);

    /* redraw view with feedback */
    ED_region_tag_redraw(region);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* painting - handle events */
static int gp_weightpaint_brush_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = op->customdata;
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
        gp_weightpaint_brush_apply_event(C, op, event);

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

          gp_weightpaint_brush_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;

      /* Abort painting if any of the usual things are tried */
      case MIDDLEMOUSE:
      case RIGHTMOUSE:
      case ESCKEY:
        gp_weightpaint_brush_exit(C, op);
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

        gp_weightpaint_brush_apply_event(C, op, event);
        break;

      /* Exit modal operator, based on the "standard" ops */
      case RIGHTMOUSE:
      case ESCKEY:
        gp_weightpaint_brush_exit(C, op);
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
      case LEFTARROWKEY:
      case RIGHTARROWKEY:
      case UPARROWKEY:
      case DOWNARROWKEY:
        return OPERATOR_PASS_THROUGH;

      /* Camera/View Gizmo's - Allowed */
      /* (See rationale in gpencil_paint.c -> gpencil_draw_modal()) */
      case PAD0:
      case PAD1:
      case PAD2:
      case PAD3:
      case PAD4:
      case PAD5:
      case PAD6:
      case PAD7:
      case PAD8:
      case PAD9:
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

void GPENCIL_OT_weight_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stroke Weight Paint";
  ot->idname = "GPENCIL_OT_weight_paint";
  ot->description = "Paint stroke points with a color";

  /* api callbacks */
  ot->exec = gp_weightpaint_brush_exec;
  ot->invoke = gp_weightpaint_brush_invoke;
  ot->modal = gp_weightpaint_brush_modal;
  ot->cancel = gp_weightpaint_brush_exit;
  ot->poll = gp_weightpaint_brush_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
