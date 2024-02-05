/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLF_api.hh"
#include "BLT_translation.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"

#include "WM_api.hh"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

/* ************************************************** */
/* GREASE PENCIL DRAWING */

/* ----- General Defines ------ */
/* flags for sflag */
enum eDrawStrokeFlags {
  /** don't draw status info */
  GP_DRAWDATA_NOSTATUS = (1 << 0),
  /** only draw 3d-strokes */
  GP_DRAWDATA_ONLY3D = (1 << 1),
  /** only draw 'canvas' strokes */
  GP_DRAWDATA_ONLYV2D = (1 << 2),
  /** only draw 'image' strokes */
  GP_DRAWDATA_ONLYI2D = (1 << 3),
  /** special hack for drawing strokes in Image Editor (weird coordinates) */
  GP_DRAWDATA_IEDITHACK = (1 << 4),
  /** don't draw xray in 3D view (which is default) */
  GP_DRAWDATA_NO_XRAY = (1 << 5),
  /** no onionskins should be drawn (for animation playback) */
  GP_DRAWDATA_NO_ONIONS = (1 << 6),
};

/* ----- Tool Buffer Drawing ------ */

static void annotation_draw_stroke_arrow_buffer(uint pos,
                                                const float *corner_point,
                                                const float *arrow_coords,
                                                const int arrow_style)
{
  immBeginAtMost(GPU_PRIM_LINE_STRIP, arrow_style);

  switch (arrow_style) {
    case GP_STROKE_ARROWSTYLE_SEGMENT:
      immVertex2f(pos, arrow_coords[0], arrow_coords[1]);
      immVertex2f(pos, arrow_coords[2], arrow_coords[3]);
      break;
    case GP_STROKE_ARROWSTYLE_CLOSED:
      immVertex2f(pos, arrow_coords[0], arrow_coords[1]);
      immVertex2f(pos, arrow_coords[2], arrow_coords[3]);
      immVertex2f(pos, arrow_coords[4], arrow_coords[5]);
      immVertex2f(pos, arrow_coords[0], arrow_coords[1]);
      break;
    case GP_STROKE_ARROWSTYLE_OPEN:
      immVertex2f(pos, arrow_coords[0], arrow_coords[1]);
      immVertex2f(pos, corner_point[0], corner_point[1]);
      immVertex2f(pos, arrow_coords[2], arrow_coords[3]);
      break;
    case GP_STROKE_ARROWSTYLE_SQUARE:
      immVertex2f(pos, corner_point[0], corner_point[1]);
      immVertex2f(pos, arrow_coords[0], arrow_coords[1]);
      immVertex2f(pos, arrow_coords[4], arrow_coords[5]);
      immVertex2f(pos, arrow_coords[6], arrow_coords[7]);
      immVertex2f(pos, arrow_coords[2], arrow_coords[3]);
      immVertex2f(pos, corner_point[0], corner_point[1]);
      break;
    default:
      break;
  }
  immEnd();
}

/**
 * Draw stroke defined in buffer (simple GPU lines/points for now, as dotted lines).
 */
static void annotation_draw_stroke_buffer(bGPdata *gps,
                                          short thickness,
                                          short dflag,
                                          const float ink[4])
{
  bGPdata_Runtime runtime = blender::dna::shallow_copy(gps->runtime);
  const tGPspoint *points = static_cast<const tGPspoint *>(runtime.sbuffer);
  int totpoints = runtime.sbuffer_used;
  short sflag = runtime.sbuffer_sflag;

  int draw_points = 0;

  /* error checking */
  if ((points == nullptr) || (totpoints <= 0)) {
    return;
  }

  /* check if buffer can be drawn */
  if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D)) {
    return;
  }

  if (sflag & GP_STROKE_ERASER) {
    /* don't draw stroke at all! */
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  const tGPspoint *pt = points;

  if (totpoints == 1) {
    /* if drawing a single point, draw it larger */
    GPU_point_size(float(thickness + 2) * points->pressure);
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
    immUniformColor3fvAlpha(ink, ink[3]);
    immBegin(GPU_PRIM_POINTS, 1);
    immVertex2fv(pos, pt->m_xy);
  }
  else {
    float oldpressure = points[0].pressure;

    /* draw stroke curve */
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

    float viewport[4];
    GPU_viewport_size_get_f(viewport);
    immUniform2fv("viewportSize", &viewport[2]);

    immUniform1f("lineWidth", max_ff(oldpressure * thickness, 1.0) * U.pixelsize);

    immUniformColor3fvAlpha(ink, ink[3]);

    immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints);

    for (int i = 0; i < totpoints; i++, pt++) {
      /* If there was a significant pressure change,
       * stop the curve, change the thickness of the stroke,
       * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP).
       */
      if (fabsf(pt->pressure - oldpressure) > 0.2f) {
        /* need to have 2 points to avoid immEnd assert error */
        if (draw_points < 2) {
          immVertex2fv(pos, (pt - 1)->m_xy);
        }

        immEnd();
        draw_points = 0;

        immUniform1f("lineWidth", max_ff(pt->pressure * thickness, 1.0) * U.pixelsize);
        immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1);

        /* need to roll-back one point to ensure that there are no gaps in the stroke */
        if (i != 0) {
          immVertex2fv(pos, (pt - 1)->m_xy);
          draw_points++;
        }

        oldpressure = pt->pressure; /* reset our threshold */
      }

      /* now the point we want */
      immVertex2fv(pos, pt->m_xy);
      draw_points++;
    }
    /* need to have 2 points to avoid immEnd assert error */
    if (draw_points < 2) {
      immVertex2fv(pos, (pt - 1)->m_xy);
    }
  }

  immEnd();

  /* Draw arrow stroke. */
  if (totpoints > 1) {
    /* Draw ending arrow stroke. */
    if ((sflag & GP_STROKE_USE_ARROW_END) &&
        (runtime.arrow_end_style != GP_STROKE_ARROWSTYLE_NONE))
    {
      float end[2];
      copy_v2_v2(end, points[1].m_xy);
      annotation_draw_stroke_arrow_buffer(pos, end, runtime.arrow_end, runtime.arrow_end_style);
    }
    /* Draw starting arrow stroke. */
    if ((sflag & GP_STROKE_USE_ARROW_START) &&
        (runtime.arrow_start_style != GP_STROKE_ARROWSTYLE_NONE))
    {
      float start[2];
      copy_v2_v2(start, points[0].m_xy);
      annotation_draw_stroke_arrow_buffer(
          pos, start, runtime.arrow_start, runtime.arrow_start_style);
    }
  }

  immUnbindProgram();
}

/* --------- 2D Stroke Drawing Helpers --------- */
/* change in parameter list */
static void annotation_calc_2d_stroke_fxy(
    const float pt[3], short sflag, int offsx, int offsy, int winx, int winy, float r_co[2])
{
  if (sflag & GP_STROKE_2DSPACE) {
    r_co[0] = pt[0];
    r_co[1] = pt[1];
  }
  else if (sflag & GP_STROKE_2DIMAGE) {
    const float x = float((pt[0] * winx) + offsx);
    const float y = float((pt[1] * winy) + offsy);

    r_co[0] = x;
    r_co[1] = y;
  }
  else {
    const float x = float(pt[0] / 100 * winx) + offsx;
    const float y = float(pt[1] / 100 * winy) + offsy;

    r_co[0] = x;
    r_co[1] = y;
  }
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void annotation_draw_stroke_point(const bGPDspoint *points,
                                         short thickness,
                                         short sflag,
                                         int offsx,
                                         int offsy,
                                         int winx,
                                         int winy,
                                         const float ink[4])
{
  const bGPDspoint *pt = points;

  /* get final position using parent matrix */
  float fpt[3];
  copy_v3_v3(fpt, &pt->x);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  if (sflag & GP_STROKE_3DSPACE) {
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

    /* get 2D coordinates of point */
    float co[3] = {0.0f};
    annotation_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
    copy_v3_v3(fpt, co);
  }

  /* set color */
  immUniformColor3fvAlpha(ink, ink[3]);

  /* set point thickness (since there's only one of these) */
  immUniform1f("size", float(thickness + 2) * pt->pressure);

  immBegin(GPU_PRIM_POINTS, 1);
  immVertex3fv(pos, fpt);
  immEnd();

  immUnbindProgram();
}

/**
 * Draw a given stroke in 3d (i.e. in 3d-space), using simple GPU lines.
 */
static void annotation_draw_stroke_3d(
    const bGPDspoint *points, int totpoints, short thickness, const float ink[4], bool cyclic)
{
  float curpressure = points[0].pressure;
  float cyclic_fpt[3];
  int draw_points = 0;

  /* if cyclic needs one vertex more */
  int cyclic_add = 0;
  if (cyclic) {
    cyclic_add++;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);

  immUniform1f("lineWidth", max_ff(curpressure * thickness, 1.0) * U.pixelsize);

  immUniformColor3fvAlpha(ink, ink[3]);

  /* draw stroke curve */
  immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints + cyclic_add);
  const bGPDspoint *pt = points;
  for (int i = 0; i < totpoints; i++, pt++) {
    /* If there was a significant pressure change, stop the curve,
     * change the thickness of the stroke, and continue drawing again
     * (since line-width cannot change in middle of GL_LINE_STRIP)
     * NOTE: we want more visible levels of pressures when thickness is bigger.
     */
    if (fabsf(pt->pressure - curpressure) > 0.2f / float(thickness)) {
      /* if the pressure changes before get at least 2 vertices,
       * need to repeat last point to avoid assert in immEnd() */
      if (draw_points < 2) {
        const bGPDspoint *pt2 = pt - 1;
        immVertex3fv(pos, &pt2->x);
      }
      immEnd();
      draw_points = 0;

      curpressure = pt->pressure;
      immUniform1f("lineWidth", max_ff(curpressure * thickness, 1.0) * U.pixelsize);
      immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1 + cyclic_add);

      /* need to roll-back one point to ensure that there are no gaps in the stroke */
      if (i != 0) {
        const bGPDspoint *pt2 = pt - 1;
        immVertex3fv(pos, &pt2->x);
        draw_points++;
      }
    }

    /* now the point we want */
    immVertex3fv(pos, &pt->x);
    draw_points++;

    if (cyclic && i == 0) {
      /* save first point to use in cyclic */
      copy_v3_v3(cyclic_fpt, &pt->x);
    }
  }

  if (cyclic) {
    /* draw line to first point to complete the cycle */
    immVertex3fv(pos, cyclic_fpt);
    draw_points++;
  }

  /* if less of two points, need to repeat last point to avoid assert in immEnd() */
  if (draw_points < 2) {
    const bGPDspoint *pt2 = pt - 1;
    immVertex3fv(pos, &pt2->x);
  }

  immEnd();
  immUnbindProgram();
}

/* Draw a given stroke in 2d. */
static void annotation_draw_stroke_2d(const bGPDspoint *points,
                                      int totpoints,
                                      short thickness_s,
                                      short sflag,
                                      int offsx,
                                      int offsy,
                                      int winx,
                                      int winy,
                                      const float ink[4])
{
  if (totpoints == 0) {
    return;
  }
  float thickness = float(thickness_s);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  const bGPDspoint *pt;
  const bGPDspoint *pt_prev;
  int draw_points = 0;
  float co[2];
  float oldpressure = points[0].pressure;
  if (totpoints == 1) {
    /* if drawing a single point, draw it larger */
    GPU_point_size(float(thickness + 2) * points->pressure);
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
    immUniformColor3fvAlpha(ink, ink[3]);
    immBegin(GPU_PRIM_POINTS, 1);

    annotation_calc_2d_stroke_fxy(&points->x, sflag, offsx, offsy, winx, winy, co);
    immVertex2fv(pos, co);
  }
  else {
    /* draw stroke curve */
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
    immUniformColor3fvAlpha(ink, ink[3]);

    float viewport[4];
    GPU_viewport_size_get_f(viewport);
    immUniform2fv("viewportSize", &viewport[2]);

    immUniform1f("lineWidth", max_ff(oldpressure * thickness, 1.0) * U.pixelsize);

    immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints);

    for (int i = 0; i < totpoints; i++) {
      pt = &points[i];
      /* If there was a significant pressure change,
       * stop the curve, change the thickness of the stroke,
       * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP).
       */
      if (fabsf(pt->pressure - oldpressure) > 0.2f) {
        /* need to have 2 points to avoid immEnd assert error */
        if (draw_points < 2) {
          pt_prev = &points[i - 1];
          annotation_calc_2d_stroke_fxy(&pt_prev->x, sflag, offsx, offsy, winx, winy, co);
          immVertex2fv(pos, co);
        }

        immEnd();
        draw_points = 0;

        immUniform1f("lineWidth", max_ff(pt->pressure * thickness, 1.0) * U.pixelsize);

        immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1);

        /* need to roll-back one point to ensure that there are no gaps in the stroke */
        if (i != 0) {
          pt_prev = &points[i - 1];
          annotation_calc_2d_stroke_fxy(&pt_prev->x, sflag, offsx, offsy, winx, winy, co);
          immVertex2fv(pos, co);
          draw_points++;
        }

        oldpressure = pt->pressure; /* reset our threshold */
      }

      /* now the point we want */
      annotation_calc_2d_stroke_fxy(&pt->x, sflag, offsx, offsy, winx, winy, co);
      immVertex2fv(pos, co);
      draw_points++;
    }
    /* need to have 2 points to avoid immEnd assert error */
    if (draw_points < 2) {
      pt_prev = &points[0];
      annotation_calc_2d_stroke_fxy(&pt_prev->x, sflag, offsx, offsy, winx, winy, co);
      immVertex2fv(pos, co);
    }
  }

  immEnd();
  immUnbindProgram();
}

/* ----- Strokes Drawing ------ */

/* Helper for doing all the checks on whether a stroke can be drawn */
static bool annotation_can_draw_stroke(const bGPDstroke *gps, const int dflag)
{
  /* skip stroke if it isn't in the right display space for this drawing context */
  /* 1) 3D Strokes */
  if ((dflag & GP_DRAWDATA_ONLY3D) && !(gps->flag & GP_STROKE_3DSPACE)) {
    return false;
  }
  if (!(dflag & GP_DRAWDATA_ONLY3D) && (gps->flag & GP_STROKE_3DSPACE)) {
    return false;
  }

  /* 2) Screen Space 2D Strokes */
  if ((dflag & GP_DRAWDATA_ONLYV2D) && !(gps->flag & GP_STROKE_2DSPACE)) {
    return false;
  }
  if (!(dflag & GP_DRAWDATA_ONLYV2D) && (gps->flag & GP_STROKE_2DSPACE)) {
    return false;
  }

  /* 3) Image Space (2D) */
  if ((dflag & GP_DRAWDATA_ONLYI2D) && !(gps->flag & GP_STROKE_2DIMAGE)) {
    return false;
  }
  if (!(dflag & GP_DRAWDATA_ONLYI2D) && (gps->flag & GP_STROKE_2DIMAGE)) {
    return false;
  }

  /* skip stroke if it doesn't have any valid data */
  if ((gps->points == nullptr) || (gps->totpoints < 1)) {
    return false;
  }

  /* stroke can be drawn */
  return true;
}

/* draw a set of strokes */
static void annotation_draw_strokes(const bGPDframe *gpf,
                                    int offsx,
                                    int offsy,
                                    int winx,
                                    int winy,
                                    int dflag,
                                    short lthick,
                                    const float color[4])
{
  GPU_program_point_size(true);

  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    /* check if stroke can be drawn */
    if (annotation_can_draw_stroke(gps, dflag) == false) {
      continue;
    }

    /* check which stroke-drawer to use */
    if (dflag & GP_DRAWDATA_ONLY3D) {
      const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);

      if (no_xray) {
        GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

        /* first arg is normally rv3d->dist, but this isn't
         * available here and seems to work quite well without */
        GPU_polygon_offset(1.0f, 1.0f);
      }

      /* 3D Lines - OpenGL primitives-based */
      if (gps->totpoints == 1) {
        annotation_draw_stroke_point(
            gps->points, lthick, gps->flag, offsx, offsy, winx, winy, color);
      }
      else {
        annotation_draw_stroke_3d(
            gps->points, gps->totpoints, lthick, color, gps->flag & GP_STROKE_CYCLIC);
      }

      if (no_xray) {
        GPU_depth_test(GPU_DEPTH_NONE);

        GPU_polygon_offset(0.0f, 0.0f);
      }
    }
    else {
      /* 2D Strokes... */
      if (gps->totpoints == 1) {
        annotation_draw_stroke_point(
            gps->points, lthick, gps->flag, offsx, offsy, winx, winy, color);
      }
      else {
        annotation_draw_stroke_2d(
            gps->points, gps->totpoints, lthick, gps->flag, offsx, offsy, winx, winy, color);
      }
    }
  }

  GPU_program_point_size(false);
}

/* ----- General Drawing ------ */
/* draw onion-skinning for a layer */
static void annotation_draw_onionskins(
    bGPDlayer *gpl, bGPDframe *gpf, int offsx, int offsy, int winx, int winy, int dflag)
{
  const float alpha = 1.0f;
  float color[4];

  /* 1) Draw Previous Frames First */
  copy_v3_v3(color, gpl->gcolor_prev);

  if (gpl->gstep > 0) {
    bGPDframe *gf;
    float fac;

    /* draw previous frames first */
    for (gf = gpf->prev; gf; gf = gf->prev) {
      /* check if frame is drawable */
      if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
        /* Alpha decreases with distance from current-frame index. */
        fac = 1.0f - (float(gpf->framenum - gf->framenum) / float(gpl->gstep + 1));
        color[3] = alpha * fac * 0.66f;
        annotation_draw_strokes(gf, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
      }
      else {
        break;
      }
    }
  }
  else if (gpl->gstep == 0) {
    /* draw the strokes for the ghost frames (at half of the alpha set by user) */
    if (gpf->prev) {
      color[3] = (alpha / 7);
      annotation_draw_strokes(gpf->prev, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
    }
  }
  else {
    /* don't draw - disabled */
  }

  /* 2) Now draw next frames */
  copy_v3_v3(color, gpl->gcolor_next);

  if (gpl->gstep_next > 0) {
    bGPDframe *gf;
    float fac;

    /* now draw next frames */
    for (gf = gpf->next; gf; gf = gf->next) {
      /* check if frame is drawable */
      if ((gf->framenum - gpf->framenum) <= gpl->gstep_next) {
        /* Alpha decreases with distance from current-frame index. */
        fac = 1.0f - (float(gf->framenum - gpf->framenum) / float(gpl->gstep_next + 1));
        color[3] = alpha * fac * 0.66f;
        annotation_draw_strokes(gf, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
      }
      else {
        break;
      }
    }
  }
  else if (gpl->gstep_next == 0) {
    /* draw the strokes for the ghost frames (at half of the alpha set by user) */
    if (gpf->next) {
      color[3] = (alpha / 4);
      annotation_draw_strokes(gpf->next, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
    }
  }
  else {
    /* don't draw - disabled */
  }
}

/* loop over gpencil data layers, drawing them */
static void annotation_draw_data_layers(
    bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
  float ink[4];

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* verify never thickness is less than 1 */
    CLAMP_MIN(gpl->thickness, 1.0f);
    short lthick = gpl->thickness;

    /* apply layer opacity */
    copy_v3_v3(ink, gpl->color);
    ink[3] = gpl->opacity;

    /* don't draw layer if hidden */
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* get frame to draw */
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV);
    if (gpf == nullptr) {
      continue;
    }

    /* Add layer drawing settings to the set of "draw flags"
     * NOTE: If the setting doesn't apply, it *must* be cleared,
     *       as dflag's carry over from the previous layer
     */

    /* xray... */
    SET_FLAG_FROM_TEST(dflag, gpl->flag & GP_LAYER_NO_XRAY, GP_DRAWDATA_NO_XRAY);

    /* Draw 'onionskins' (frame left + right) */
    if (gpl->onion_flag & GP_LAYER_ONIONSKIN) {
      annotation_draw_onionskins(gpl, gpf, offsx, offsy, winx, winy, dflag);
    }

    /* draw the strokes already in active frame */
    annotation_draw_strokes(gpf, offsx, offsy, winx, winy, dflag, lthick, ink);

    /* Check if may need to draw the active stroke cache, only if this layer is the active layer
     * that is being edited. (Stroke buffer is currently stored in gp-data)
     */
    if (ED_gpencil_session_active() && (gpl->flag & GP_LAYER_ACTIVE) &&
        (gpf->flag & GP_FRAME_PAINT))
    {
      /* Buffer stroke needs to be drawn with a different line-style
       * to help differentiate them from normal strokes.
       *
       * It should also be noted that #bGPdata_Runtime::sbuffer contains temporary point types
       * i.e. #tGPspoints NOT #bGPDspoints. */
      annotation_draw_stroke_buffer(gpd, lthick, dflag, ink);
    }
  }
}

/* draw grease-pencil datablock */
static void annotation_draw_data(
    bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
  /* turn on smooth lines (i.e. anti-aliasing) */
  GPU_line_smooth(true);

  /* turn on alpha-blending */
  GPU_blend(GPU_BLEND_ALPHA);

  /* Do not write to depth (avoid self-occlusion). */
  bool prev_depth_mask = GPU_depth_mask_get();
  GPU_depth_mask(false);

  /* draw! */
  annotation_draw_data_layers(gpd, offsx, offsy, winx, winy, cfra, dflag);

  /* turn off alpha blending, then smooth lines */
  GPU_blend(GPU_BLEND_NONE); /* alpha blending */
  GPU_line_smooth(false);    /* smooth lines */

  GPU_depth_mask(prev_depth_mask);
}

/* if we have strokes for scenes (3d view)/clips (movie clip editor)
 * and objects/tracks, multiple data blocks have to be drawn */
static void annotation_draw_data_all(Scene *scene,
                                     bGPdata *gpd,
                                     int offsx,
                                     int offsy,
                                     int winx,
                                     int winy,
                                     int cfra,
                                     int dflag,
                                     const eSpace_Type space_type)
{
  bGPdata *gpd_source = nullptr;

  if (scene) {
    if (space_type == SPACE_VIEW3D) {
      gpd_source = (scene->gpd ? scene->gpd : nullptr);
    }
    else if (space_type == SPACE_CLIP && scene->clip) {
      /* currently drawing only gpencil data from either clip or track,
       * but not both - XXX fix logic behind */
      gpd_source = (scene->clip->gpd ? scene->clip->gpd : nullptr);
    }

    if (gpd_source) {
      annotation_draw_data(gpd_source, offsx, offsy, winx, winy, cfra, dflag);
    }
  }

  /* scene/clip data has already been drawn, only object/track data is drawn here
   * if gpd_source == gpd, we don't have any object/track data and we can skip */
  if (gpd_source == nullptr || (gpd_source && gpd_source != gpd)) {
    annotation_draw_data(gpd, offsx, offsy, winx, winy, cfra, dflag);
  }
}

/* ----- Annotation Sketches Drawing API ------ */

void ED_annotation_draw_2dimage(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);

  int offsx, offsy, sizex, sizey;
  int dflag = GP_DRAWDATA_NOSTATUS;

  bGPdata *gpd = ED_annotation_data_get_active(C);
  if (gpd == nullptr) {
    return;
  }

  /* calculate rect */
  switch (area->spacetype) {
    case SPACE_IMAGE: /* image */
    case SPACE_CLIP:  /* clip */
    {
      /* just draw using standard scaling (settings here are currently ignored anyways) */
      /* FIXME: the opengl poly-strokes don't draw at right thickness when done this way,
       * so disabled. */
      offsx = 0;
      offsy = 0;
      sizex = region->winx;
      sizey = region->winy;

      wmOrtho2(
          region->v2d.cur.xmin, region->v2d.cur.xmax, region->v2d.cur.ymin, region->v2d.cur.ymax);

      dflag |= GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_IEDITHACK;
      break;
    }
    case SPACE_SEQ: /* sequence */
    {
      /* just draw using standard scaling (settings here are currently ignored anyways) */
      offsx = 0;
      offsy = 0;
      sizex = region->winx;
      sizey = region->winy;

      /* NOTE: I2D was used in 2.4x, but the old settings for that have been deprecated
       * and everything moved to standard View2d
       */
      dflag |= GP_DRAWDATA_ONLYV2D;
      break;
    }
    default: /* for spacetype not yet handled */
      offsx = 0;
      offsy = 0;
      sizex = region->winx;
      sizey = region->winy;

      dflag |= GP_DRAWDATA_ONLYI2D;
      break;
  }

  if (ED_screen_animation_playing(wm)) {
    /* Don't show onion-skins during animation playback/scrub (i.e. it obscures the poses)
     * OpenGL Renders (i.e. final output), or depth buffer (i.e. not real strokes). */
    dflag |= GP_DRAWDATA_NO_ONIONS;
  }

  /* draw it! */
  annotation_draw_data_all(
      scene, gpd, offsx, offsy, sizex, sizey, scene->r.cfra, dflag, eSpace_Type(area->spacetype));
}

void ED_annotation_draw_view2d(const bContext *C, bool onlyv2d)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int dflag = 0;

  /* check that we have grease-pencil stuff to draw */
  if (area == nullptr) {
    return;
  }
  bGPdata *gpd = ED_annotation_data_get_active(C);
  if (gpd == nullptr) {
    return;
  }

  /* special hack for Image Editor */
  /* FIXME: the opengl poly-strokes don't draw at right thickness when done this way,
   * so disabled. */
  if (ELEM(area->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
    dflag |= GP_DRAWDATA_IEDITHACK;
  }

  /* draw it! */
  if (onlyv2d) {
    dflag |= (GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_NOSTATUS);
  }
  if (ED_screen_animation_playing(wm)) {
    dflag |= GP_DRAWDATA_NO_ONIONS;
  }

  annotation_draw_data_all(scene,
                           gpd,
                           0,
                           0,
                           region->winx,
                           region->winy,
                           scene->r.cfra,
                           dflag,
                           eSpace_Type(area->spacetype));
}

void ED_annotation_draw_view3d(
    Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *region, bool only3d)
{
  int dflag = 0;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  int offsx, offsy, winx, winy;

  /* check that we have grease-pencil stuff to draw */
  /* XXX: Hardcoded reference here may get out of sync if we change how we fetch annotation data
   */
  bGPdata *gpd = scene->gpd;
  if (gpd == nullptr) {
    return;
  }

  /* When rendering to the off-screen buffer we don't want to
   * deal with the camera border, otherwise map the coords to the camera border. */
  if ((rv3d->persp == RV3D_CAMOB) && !(G.f & G_FLAG_RENDER_VIEWPORT)) {
    rctf rectf;
    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &rectf, true); /* no shift */

    offsx = round_fl_to_int(rectf.xmin);
    offsy = round_fl_to_int(rectf.ymin);
    winx = round_fl_to_int(rectf.xmax - rectf.xmin);
    winy = round_fl_to_int(rectf.ymax - rectf.ymin);
  }
  else {
    offsx = 0;
    offsy = 0;
    winx = region->winx;
    winy = region->winy;
  }

  /* set flags */
  if (only3d) {
    /* 3D strokes/3D space:
     * - only 3D space points
     * - don't status text either (as it's the wrong space)
     */
    dflag |= (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_NOSTATUS);
  }

  if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
    /* don't draw status text when "only render" flag is set */
    dflag |= GP_DRAWDATA_NOSTATUS;
  }

  /* draw it! */
  annotation_draw_data_all(
      scene, gpd, offsx, offsy, winx, winy, scene->r.cfra, dflag, eSpace_Type(v3d->spacetype));
}

void ED_annotation_draw_ex(
    Scene *scene, bGPdata *gpd, int winx, int winy, const int cfra, const char spacetype)
{
  int dflag = GP_DRAWDATA_NOSTATUS | GP_DRAWDATA_ONLYV2D;

  annotation_draw_data_all(scene, gpd, 0, 0, winx, winy, cfra, dflag, eSpace_Type(spacetype));
}

/* ************************************************** */
