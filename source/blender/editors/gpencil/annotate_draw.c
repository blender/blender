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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"

#include "WM_api.h"

#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_state.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_space_api.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"

/* ************************************************** */
/* GREASE PENCIL DRAWING */

/* ----- General Defines ------ */
/* flags for sflag */
typedef enum eDrawStrokeFlags {
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
} eDrawStrokeFlags;

/* ----- Tool Buffer Drawing ------ */

/* draw stroke defined in buffer (simple ogl lines/points for now, as dotted lines) */
static void annotation_draw_stroke_buffer(const tGPspoint *points,
                                          int totpoints,
                                          short thickness,
                                          short dflag,
                                          short sflag,
                                          float ink[4])
{
  int draw_points = 0;

  /* error checking */
  if ((points == NULL) || (totpoints <= 0)) {
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
    GPU_point_size((float)(thickness + 2) * points->pressure);
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
    immUniformColor3fvAlpha(ink, ink[3]);
    immBegin(GPU_PRIM_POINTS, 1);
    immVertex2fv(pos, &pt->x);
  }
  else {
    float oldpressure = points[0].pressure;

    /* draw stroke curve */
    GPU_line_width(max_ff(oldpressure * thickness, 1.0));

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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
          immVertex2fv(pos, &(pt - 1)->x);
        }

        immEnd();
        draw_points = 0;

        GPU_line_width(max_ff(pt->pressure * thickness, 1.0f));
        immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1);

        /* need to roll-back one point to ensure that there are no gaps in the stroke */
        if (i != 0) {
          immVertex2fv(pos, &(pt - 1)->x);
          draw_points++;
        }

        oldpressure = pt->pressure; /* reset our threshold */
      }

      /* now the point we want */
      immVertex2fv(pos, &pt->x);
      draw_points++;
    }
    /* need to have 2 points to avoid immEnd assert error */
    if (draw_points < 2) {
      immVertex2fv(pos, &(pt - 1)->x);
    }
  }

  immEnd();
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
    const float x = (float)((pt[0] * winx) + offsx);
    const float y = (float)((pt[1] * winy) + offsy);

    r_co[0] = x;
    r_co[1] = y;
  }
  else {
    const float x = (float)(pt[0] / 100 * winx) + offsx;
    const float y = (float)(pt[1] / 100 * winy) + offsy;

    r_co[0] = x;
    r_co[1] = y;
  }
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void annotation_draw_stroke_point(const bGPDspoint *points,
                                         short thickness,
                                         short UNUSED(dflag),
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
    immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

    /* get 2D coordinates of point */
    float co[3] = {0.0f};
    annotation_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
    copy_v3_v3(fpt, co);
  }

  /* set color */
  immUniformColor3fvAlpha(ink, ink[3]);

  /* set point thickness (since there's only one of these) */
  immUniform1f("size", (float)(thickness + 2) * pt->pressure);

  immBegin(GPU_PRIM_POINTS, 1);
  immVertex3fv(pos, fpt);
  immEnd();

  immUnbindProgram();
}

/* draw a given stroke in 3d (i.e. in 3d-space), using simple ogl lines */
static void annotation_draw_stroke_3d(const bGPDspoint *points,
                                      int totpoints,
                                      short thickness,
                                      short UNUSED(sflag),
                                      const float ink[4],
                                      bool cyclic)
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

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3fvAlpha(ink, ink[3]);

  /* draw stroke curve */
  GPU_line_width(max_ff(curpressure * thickness, 1.0f));
  immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints + cyclic_add);
  const bGPDspoint *pt = points;
  for (int i = 0; i < totpoints; i++, pt++) {
    /* If there was a significant pressure change, stop the curve,
     * change the thickness of the stroke, and continue drawing again
     * (since line-width cannot change in middle of GL_LINE_STRIP)
     * Note: we want more visible levels of pressures when thickness is bigger.
     */
    if (fabsf(pt->pressure - curpressure) > 0.2f / (float)thickness) {
      /* if the pressure changes before get at least 2 vertices,
       * need to repeat last point to avoid assert in immEnd() */
      if (draw_points < 2) {
        const bGPDspoint *pt2 = pt - 1;
        immVertex3fv(pos, &pt2->x);
      }
      immEnd();
      draw_points = 0;

      curpressure = pt->pressure;
      GPU_line_width(max_ff(curpressure * thickness, 1.0f));
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

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void annotation_draw_stroke_2d(const bGPDspoint *points,
                                      int totpoints,
                                      short thickness_s,
                                      short dflag,
                                      short sflag,
                                      int offsx,
                                      int offsy,
                                      int winx,
                                      int winy,
                                      const float ink[4])
{
  /* otherwise thickness is twice that of the 3D view */
  float thickness = (float)thickness_s * 0.5f;

  /* strokes in Image Editor need a scale factor, since units there are not pixels! */
  float scalefac = 1.0f;
  if ((dflag & GP_DRAWDATA_IEDITHACK) && (dflag & GP_DRAWDATA_ONLYV2D)) {
    scalefac = 0.001f;
  }

  /* Tessellation code - draw stroke as series of connected quads
   * (triangle strips in fact) with connection edges rotated to minimize shrinking artifacts,
   * and rounded endcaps.
   */
  {
    const bGPDspoint *pt1, *pt2;
    float s0[2], s1[2]; /* segment 'center' points */
    float pm[2];        /* normal from previous segment. */
    int i;

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor3fvAlpha(ink, ink[3]);
    immBegin(GPU_PRIM_TRI_STRIP, totpoints * 2 + 4);

    /* get x and y coordinates from first point */
    annotation_calc_2d_stroke_fxy(&points->x, sflag, offsx, offsy, winx, winy, s0);

    for (i = 0, pt1 = points, pt2 = points + 1; i < (totpoints - 1); i++, pt1++, pt2++) {
      float t0[2], t1[2]; /* tessellated coordinates */
      float m1[2], m2[2]; /* gradient and normal */
      float mt[2], sc[2]; /* gradient for thickness, point for end-cap */
      float pthick;       /* thickness at segment point */

      /* Get x and y coordinates from point2
       * (point1 has already been computed in previous iteration). */
      annotation_calc_2d_stroke_fxy(&pt2->x, sflag, offsx, offsy, winx, winy, s1);

      /* calculate gradient and normal - 'angle'=(ny/nx) */
      m1[1] = s1[1] - s0[1];
      m1[0] = s1[0] - s0[0];
      normalize_v2(m1);
      m2[1] = -m1[0];
      m2[0] = m1[1];

      /* always use pressure from first point here */
      pthick = (pt1->pressure * thickness * scalefac);

      /* if the first segment, start of segment is segment's normal */
      if (i == 0) {
        /* draw start cap first
         * - make points slightly closer to center (about halfway across)
         */
        mt[0] = m2[0] * pthick * 0.5f;
        mt[1] = m2[1] * pthick * 0.5f;
        sc[0] = s0[0] - (m1[0] * pthick * 0.75f);
        sc[1] = s0[1] - (m1[1] * pthick * 0.75f);

        t0[0] = sc[0] - mt[0];
        t0[1] = sc[1] - mt[1];
        t1[0] = sc[0] + mt[0];
        t1[1] = sc[1] + mt[1];

        /* First two points of cap. */
        immVertex2fv(pos, t0);
        immVertex2fv(pos, t1);

        /* calculate points for start of segment */
        mt[0] = m2[0] * pthick;
        mt[1] = m2[1] * pthick;

        t0[0] = s0[0] - mt[0];
        t0[1] = s0[1] - mt[1];
        t1[0] = s0[0] + mt[0];
        t1[1] = s0[1] + mt[1];

        /* Last two points of start cap (and first two points of first segment). */
        immVertex2fv(pos, t0);
        immVertex2fv(pos, t1);
      }
      /* if not the first segment, use bisector of angle between segments */
      else {
        float mb[2];        /* bisector normal */
        float athick, dfac; /* actual thickness, difference between thicknesses */

        /* calculate gradient of bisector (as average of normals) */
        mb[0] = (pm[0] + m2[0]) / 2;
        mb[1] = (pm[1] + m2[1]) / 2;
        normalize_v2(mb);

        /* calculate gradient to apply
         *  - as basis, use just pthick * bisector gradient
         * - if cross-section not as thick as it should be, add extra padding to fix it
         */
        mt[0] = mb[0] * pthick;
        mt[1] = mb[1] * pthick;
        athick = len_v2(mt);
        dfac = pthick - (athick * 2);

        if (((athick * 2.0f) < pthick) && (IS_EQF(athick, pthick) == 0)) {
          mt[0] += (mb[0] * dfac);
          mt[1] += (mb[1] * dfac);
        }

        /* calculate points for start of segment */
        t0[0] = s0[0] - mt[0];
        t0[1] = s0[1] - mt[1];
        t1[0] = s0[0] + mt[0];
        t1[1] = s0[1] + mt[1];

        /* Last two points of previous segment, and first two points of current segment. */
        immVertex2fv(pos, t0);
        immVertex2fv(pos, t1);
      }

      /* if last segment, also draw end of segment (defined as segment's normal) */
      if (i == totpoints - 2) {
        /* for once, we use second point's pressure (otherwise it won't be drawn) */
        pthick = (pt2->pressure * thickness * scalefac);

        /* calculate points for end of segment */
        mt[0] = m2[0] * pthick;
        mt[1] = m2[1] * pthick;

        t0[0] = s1[0] - mt[0];
        t0[1] = s1[1] - mt[1];
        t1[0] = s1[0] + mt[0];
        t1[1] = s1[1] + mt[1];

        /* Last two points of last segment (and first two points of end cap). */
        immVertex2fv(pos, t0);
        immVertex2fv(pos, t1);

        /* draw end cap as last step
         * - make points slightly closer to center (about halfway across)
         */
        mt[0] = m2[0] * pthick * 0.5f;
        mt[1] = m2[1] * pthick * 0.5f;
        sc[0] = s1[0] + (m1[0] * pthick * 0.75f);
        sc[1] = s1[1] + (m1[1] * pthick * 0.75f);

        t0[0] = sc[0] - mt[0];
        t0[1] = sc[1] - mt[1];
        t1[0] = sc[0] + mt[0];
        t1[1] = sc[1] + mt[1];

        /* Last two points of end cap. */
        immVertex2fv(pos, t0);
        immVertex2fv(pos, t1);
      }

      /* store computed point2 coordinates as point1 ones of next segment. */
      copy_v2_v2(s0, s1);
      /* store stroke's 'natural' normal for next stroke to use */
      copy_v2_v2(pm, m2);
    }

    immEnd();
    immUnbindProgram();
  }
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
  if ((gps->points == NULL) || (gps->totpoints < 1)) {
    return false;
  }

  /* stroke can be drawn */
  return true;
}

/* draw a set of strokes */
static void annotation_draw_strokes(bGPdata *UNUSED(gpd),
                                    bGPDlayer *UNUSED(gpl),
                                    const bGPDframe *gpf,
                                    int offsx,
                                    int offsy,
                                    int winx,
                                    int winy,
                                    int dflag,
                                    short lthick,
                                    const float color[4])
{
  GPU_enable_program_point_size();

  for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
    /* check if stroke can be drawn */
    if (annotation_can_draw_stroke(gps, dflag) == false) {
      continue;
    }

    /* check which stroke-drawer to use */
    if (dflag & GP_DRAWDATA_ONLY3D) {
      const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);
      int mask_orig = 0;

      if (no_xray) {
        glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
        glDepthMask(0);
        GPU_depth_test(true);

        /* first arg is normally rv3d->dist, but this isn't
         * available here and seems to work quite well without */
        bglPolygonOffset(1.0f, 1.0f);
      }

      /* 3D Lines - OpenGL primitives-based */
      if (gps->totpoints == 1) {
        annotation_draw_stroke_point(
            gps->points, lthick, dflag, gps->flag, offsx, offsy, winx, winy, color);
      }
      else {
        annotation_draw_stroke_3d(
            gps->points, gps->totpoints, lthick, gps->flag, color, gps->flag & GP_STROKE_CYCLIC);
      }

      if (no_xray) {
        glDepthMask(mask_orig);
        GPU_depth_test(false);

        bglPolygonOffset(0.0, 0.0);
      }
    }
    else {
      /* 2D Strokes... */
      if (gps->totpoints == 1) {
        annotation_draw_stroke_point(
            gps->points, lthick, dflag, gps->flag, offsx, offsy, winx, winy, color);
      }
      else {
        annotation_draw_stroke_2d(gps->points,
                                  gps->totpoints,
                                  lthick,
                                  dflag,
                                  gps->flag,
                                  offsx,
                                  offsy,
                                  winx,
                                  winy,
                                  color);
      }
    }
  }

  GPU_disable_program_point_size();
}

/* Draw selected verts for strokes being edited */
static void annotation_draw_strokes_edit(bGPdata *gpd,
                                         bGPDlayer *gpl,
                                         const bGPDframe *gpf,
                                         int offsx,
                                         int offsy,
                                         int winx,
                                         int winy,
                                         short dflag,
                                         short UNUSED(lflag),
                                         float alpha)
{
  /* if alpha 0 do not draw */
  if (alpha == 0.0f) {
    return;
  }

  const bool no_xray = (dflag & GP_DRAWDATA_NO_XRAY) != 0;
  int mask_orig = 0;

  /* set up depth masks... */
  if (dflag & GP_DRAWDATA_ONLY3D) {
    if (no_xray) {
      glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
      glDepthMask(0);
      GPU_depth_test(true);

      /* first arg is normally rv3d->dist, but this isn't
       * available here and seems to work quite well without */
      bglPolygonOffset(1.0f, 1.0f);
    }
  }

  GPU_enable_program_point_size();

  /* draw stroke verts */
  for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
    /* check if stroke can be drawn */
    if (annotation_can_draw_stroke(gps, dflag) == false) {
      continue;
    }

    /* Optimisation: only draw points for selected strokes
     * We assume that selected points can only occur in
     * strokes that are selected too.
     */
    if ((gps->flag & GP_STROKE_SELECT) == 0) {
      continue;
    }

    /* Get size of verts:
     * - The selected state needs to be larger than the unselected state so that
     *   they stand out more.
     * - We use the theme setting for size of the unselected verts
     */
    float bsize = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
    float vsize;
    if ((int)bsize > 8) {
      vsize = 10.0f;
      bsize = 8.0f;
    }
    else {
      vsize = bsize + 2;
    }

    float selectColor[4];
    UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
    selectColor[3] = alpha;

    GPUVertFormat *format = immVertexFormat();
    uint pos; /* specified later */
    uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    if (gps->flag & GP_STROKE_3DSPACE) {
      pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
    }
    else {
      pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_POINT_VARYING_SIZE_VARYING_COLOR);
    }

    immBegin(GPU_PRIM_POINTS, gps->totpoints);

    /* Draw start and end point differently if enabled stroke direction hint */
    bool show_direction_hint = (gpd->flag & GP_DATA_SHOW_DIRECTION) && (gps->totpoints > 1);

    /* Draw all the stroke points (selected or not) */
    bGPDspoint *pt = gps->points;
    for (int i = 0; i < gps->totpoints; i++, pt++) {
      /* size and color first */
      if (show_direction_hint && i == 0) {
        /* start point in green bigger */
        immAttr3f(color, 0.0f, 1.0f, 0.0f);
        immAttr1f(size, vsize + 4);
      }
      else if (show_direction_hint && (i == gps->totpoints - 1)) {
        /* end point in red smaller */
        immAttr3f(color, 1.0f, 0.0f, 0.0f);
        immAttr1f(size, vsize + 1);
      }
      else if (pt->flag & GP_SPOINT_SELECT) {
        immAttr3fv(color, selectColor);
        immAttr1f(size, vsize);
      }
      else {
        immAttr3fv(color, gpl->color);
        immAttr1f(size, bsize);
      }

      /* then position */
      if (gps->flag & GP_STROKE_3DSPACE) {
        immVertex3fv(pos, &pt->x);
      }
      else {
        float co[2];
        annotation_calc_2d_stroke_fxy(&pt->x, gps->flag, offsx, offsy, winx, winy, co);
        immVertex2fv(pos, co);
      }
    }

    immEnd();
    immUnbindProgram();
  }

  GPU_disable_program_point_size();

  /* clear depth mask */
  if (dflag & GP_DRAWDATA_ONLY3D) {
    if (no_xray) {
      glDepthMask(mask_orig);
      GPU_depth_test(false);

      bglPolygonOffset(0.0, 0.0);
#if 0
      glDisable(GL_POLYGON_OFFSET_LINE);
      glPolygonOffset(0, 0);
#endif
    }
  }
}

/* ----- General Drawing ------ */
/* draw onion-skinning for a layer */
static void annotation_draw_onionskins(bGPdata *gpd,
                                       bGPDlayer *gpl,
                                       bGPDframe *gpf,
                                       int offsx,
                                       int offsy,
                                       int winx,
                                       int winy,
                                       int UNUSED(cfra),
                                       int dflag)
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
        /* alpha decreases with distance from curframe index */
        fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(gpl->gstep + 1));
        color[3] = alpha * fac * 0.66f;
        annotation_draw_strokes(
            gpd, gpl, gf, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
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
      annotation_draw_strokes(
          gpd, gpl, gpf->prev, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
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
        /* alpha decreases with distance from curframe index */
        fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(gpl->gstep_next + 1));
        color[3] = alpha * fac * 0.66f;
        annotation_draw_strokes(
            gpd, gpl, gf, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
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
      annotation_draw_strokes(
          gpd, gpl, gpf->next, offsx, offsy, winx, winy, dflag, gpl->thickness, color);
    }
  }
  else {
    /* don't draw - disabled */
  }
}

/* loop over gpencil data layers, drawing them */
static void annotation_draw_data_layers(
    bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag, float alpha)
{
  float ink[4];

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
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
    bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }

    /* set basic stroke thickness */
    GPU_line_width(lthick);

    /* Add layer drawing settings to the set of "draw flags"
     * NOTE: If the setting doesn't apply, it *must* be cleared,
     *       as dflag's carry over from the previous layer
     */

    /* xray... */
    SET_FLAG_FROM_TEST(dflag, gpl->flag & GP_LAYER_NO_XRAY, GP_DRAWDATA_NO_XRAY);

    /* Draw 'onionskins' (frame left + right) */
    if (gpl->onion_flag & GP_LAYER_ONIONSKIN) {
      annotation_draw_onionskins(gpd, gpl, gpf, offsx, offsy, winx, winy, cfra, dflag);
    }

    /* draw the strokes already in active frame */
    annotation_draw_strokes(gpd, gpl, gpf, offsx, offsy, winx, winy, dflag, lthick, ink);

    /* Draw verts of selected strokes
     *  - when doing OpenGL renders, we don't want to be showing these, as that ends up flickering
     *  - locked layers can't be edited, so there's no point showing these verts
     *    as they will have no bearings on what gets edited
     *  - only show when in editmode, since operators shouldn't work otherwise
     *    (NOTE: doing it this way means that the toggling editmode shows visible change immediately)
     */
    /* XXX: perhaps we don't want to show these when users are drawing... */
    if ((G.f & G_FLAG_RENDER_VIEWPORT) == 0 && (gpl->flag & GP_LAYER_LOCKED) == 0 &&
        (gpd->flag & GP_DATA_STROKE_EDITMODE)) {
      annotation_draw_strokes_edit(
          gpd, gpl, gpf, offsx, offsy, winx, winy, dflag, gpl->flag, alpha);
    }

    /* Check if may need to draw the active stroke cache, only if this layer is the active layer
     * that is being edited. (Stroke buffer is currently stored in gp-data)
     */
    if (ED_gpencil_session_active() && (gpl->flag & GP_LAYER_ACTIVE) &&
        (gpf->flag & GP_FRAME_PAINT)) {
      /* Buffer stroke needs to be drawn with a different linestyle
       * to help differentiate them from normal strokes.
       *
       * It should also be noted that sbuffer contains temporary point types
       * i.e. tGPspoints NOT bGPDspoints
       */
      annotation_draw_stroke_buffer(gpd->runtime.sbuffer,
                                    gpd->runtime.sbuffer_size,
                                    lthick,
                                    dflag,
                                    gpd->runtime.sbuffer_sflag,
                                    ink);
    }
  }
}

/* draw a short status message in the top-right corner */
static void annotation_draw_status_text(const bGPdata *gpd, ARegion *ar)
{
  rcti rect;

  /* Cannot draw any status text when drawing OpenGL Renders */
  if (G.f & G_FLAG_RENDER_VIEWPORT) {
    return;
  }

  /* Get bounds of region - Necessary to avoid problems with region overlap */
  ED_region_visible_rect(ar, &rect);

  /* for now, this should only be used to indicate when we are in stroke editmode */
  if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
    const char *printable = IFACE_("GPencil Stroke Editing");
    float printable_size[2];

    int font_id = BLF_default();

    BLF_width_and_height(
        font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

    int xco = (rect.xmax - U.widget_unit) - (int)printable_size[0];
    int yco = (rect.ymax - U.widget_unit);

    /* text label */
    UI_FontThemeColor(font_id, TH_TEXT_HI);
#ifdef WITH_INTERNATIONAL
    BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
    BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif

    /* grease pencil icon... */
    // XXX: is this too intrusive?
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    xco -= U.widget_unit;
    yco -= (int)printable_size[1] / 2;

    UI_icon_draw(xco, yco, ICON_GREASEPENCIL);

    GPU_blend(false);
  }
}

/* draw grease-pencil datablock */
static void annotation_draw_data(
    bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag, float alpha)
{
  /* turn on smooth lines (i.e. anti-aliasing) */
  GPU_line_smooth(true);

  /* turn on alpha-blending */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  /* draw! */
  annotation_draw_data_layers(gpd, offsx, offsy, winx, winy, cfra, dflag, alpha);

  /* turn off alpha blending, then smooth lines */
  GPU_blend(false);        // alpha blending
  GPU_line_smooth(false);  // smooth lines
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
                                     const char spacetype)
{
  bGPdata *gpd_source = NULL;
  float alpha = 1.0f;

  if (scene) {
    if (spacetype == SPACE_VIEW3D) {
      gpd_source = (scene->gpd ? scene->gpd : NULL);
    }
    else if (spacetype == SPACE_CLIP && scene->clip) {
      /* currently drawing only gpencil data from either clip or track,
       * but not both - XXX fix logic behind */
      gpd_source = (scene->clip->gpd ? scene->clip->gpd : NULL);
    }

    if (gpd_source) {
      annotation_draw_data(gpd_source, offsx, offsy, winx, winy, cfra, dflag, alpha);
    }
  }

  /* scene/clip data has already been drawn, only object/track data is drawn here
   * if gpd_source == gpd, we don't have any object/track data and we can skip */
  if (gpd_source == NULL || (gpd_source && gpd_source != gpd)) {
    annotation_draw_data(gpd, offsx, offsy, winx, winy, cfra, dflag, alpha);
  }
}

/* ----- Grease Pencil Sketches Drawing API ------ */

/* ............................
 * XXX
 * We need to review the calls below, since they may be/are not that suitable for
 * the new ways that we intend to be drawing data...
 * ............................ */

/* draw grease-pencil sketches to specified 2d-view that uses ibuf corrections */
void ED_annotation_draw_2dimage(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);

  int offsx, offsy, sizex, sizey;
  int dflag = GP_DRAWDATA_NOSTATUS;

  bGPdata *gpd = ED_gpencil_data_get_active(C);  // XXX
  if (gpd == NULL) {
    return;
  }

  /* calculate rect */
  switch (sa->spacetype) {
    case SPACE_IMAGE: /* image */
    case SPACE_CLIP:  /* clip */
    {
      /* just draw using standard scaling (settings here are currently ignored anyways) */
      /* FIXME: the opengl poly-strokes don't draw at right thickness when done this way,
       * so disabled. */
      offsx = 0;
      offsy = 0;
      sizex = ar->winx;
      sizey = ar->winy;

      wmOrtho2(ar->v2d.cur.xmin, ar->v2d.cur.xmax, ar->v2d.cur.ymin, ar->v2d.cur.ymax);

      dflag |= GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_IEDITHACK;
      break;
    }
    case SPACE_SEQ: /* sequence */
    {
      /* just draw using standard scaling (settings here are currently ignored anyways) */
      offsx = 0;
      offsy = 0;
      sizex = ar->winx;
      sizey = ar->winy;

      /* NOTE: I2D was used in 2.4x, but the old settings for that have been deprecated
       * and everything moved to standard View2d
       */
      dflag |= GP_DRAWDATA_ONLYV2D;
      break;
    }
    default: /* for spacetype not yet handled */
      offsx = 0;
      offsy = 0;
      sizex = ar->winx;
      sizey = ar->winy;

      dflag |= GP_DRAWDATA_ONLYI2D;
      break;
  }

  if (ED_screen_animation_playing(wm)) {
    /* Don't show onion-skins during animation playback/scrub (i.e. it obscures the poses)
     * OpenGL Renders (i.e. final output), or depth buffer (i.e. not real strokes). */
    dflag |= GP_DRAWDATA_NO_ONIONS;
  }

  /* draw it! */
  annotation_draw_data_all(scene, gpd, offsx, offsy, sizex, sizey, CFRA, dflag, sa->spacetype);
}

/* draw grease-pencil sketches to specified 2d-view assuming that matrices are already set correctly
 * Note: this gets called twice - first time with onlyv2d=true to draw 'canvas' strokes,
 * second time with onlyv2d=false for screen-aligned strokes */
void ED_annotation_draw_view2d(const bContext *C, bool onlyv2d)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int dflag = 0;

  /* check that we have grease-pencil stuff to draw */
  if (sa == NULL) {
    return;
  }
  bGPdata *gpd = ED_gpencil_data_get_active(C);  // XXX
  if (gpd == NULL) {
    return;
  }

  /* special hack for Image Editor */
  /* FIXME: the opengl poly-strokes don't draw at right thickness when done this way, so disabled */
  if (ELEM(sa->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
    dflag |= GP_DRAWDATA_IEDITHACK;
  }

  /* draw it! */
  if (onlyv2d) {
    dflag |= (GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_NOSTATUS);
  }
  if (ED_screen_animation_playing(wm)) {
    dflag |= GP_DRAWDATA_NO_ONIONS;
  }

  annotation_draw_data_all(scene, gpd, 0, 0, ar->winx, ar->winy, CFRA, dflag, sa->spacetype);

  /* draw status text (if in screen/pixel-space) */
  if (!onlyv2d) {
    annotation_draw_status_text(gpd, ar);
  }
}

/* draw annotations sketches to specified 3d-view assuming that matrices are already set correctly
 * Note: this gets called twice - first time with only3d=true to draw 3d-strokes,
 * second time with only3d=false for screen-aligned strokes */
void ED_annotation_draw_view3d(
    Scene *scene, struct Depsgraph *depsgraph, View3D *v3d, ARegion *ar, bool only3d)
{
  int dflag = 0;
  RegionView3D *rv3d = ar->regiondata;
  int offsx, offsy, winx, winy;

  /* check that we have grease-pencil stuff to draw */
  /* XXX: Hardcoded reference here may get out of sync if we change how we fetch annotation data */
  bGPdata *gpd = scene->gpd;
  if (gpd == NULL) {
    return;
  }

  /* when rendering to the offscreen buffer we don't want to
   * deal with the camera border, otherwise map the coords to the camera border. */
  if ((rv3d->persp == RV3D_CAMOB) && !(G.f & G_FLAG_RENDER_VIEWPORT)) {
    rctf rectf;
    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &rectf, true); /* no shift */

    offsx = round_fl_to_int(rectf.xmin);
    offsy = round_fl_to_int(rectf.ymin);
    winx = round_fl_to_int(rectf.xmax - rectf.xmin);
    winy = round_fl_to_int(rectf.ymax - rectf.ymin);
  }
  else {
    offsx = 0;
    offsy = 0;
    winx = ar->winx;
    winy = ar->winy;
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
  annotation_draw_data_all(scene, gpd, offsx, offsy, winx, winy, CFRA, dflag, v3d->spacetype);
}

void ED_annotation_draw_ex(
    Scene *scene, bGPdata *gpd, int winx, int winy, const int cfra, const char spacetype)
{
  int dflag = GP_DRAWDATA_NOSTATUS | GP_DRAWDATA_ONLYV2D;

  annotation_draw_data_all(scene, gpd, 0, 0, winx, winy, cfra, dflag, spacetype);
}

/* ************************************************** */
