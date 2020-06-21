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

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "BLI_math.h"
#include "BLI_polyfill_2d.h"
#include "BLI_utildefines.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_paint.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"

#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "IMB_imbuf_types.h"

#include "gpencil_intern.h"

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
  /** draw strokes as "volumetric" circular billboards */
  GP_DRAWDATA_VOLUMETRIC = (1 << 7),
  /** fill insides/bounded-regions of strokes */
  GP_DRAWDATA_FILL = (1 << 8),
} eDrawStrokeFlags;

/* thickness above which we should use special drawing */
#if 0
#  define GP_DRAWTHICKNESS_SPECIAL 3
#endif

/* conversion utility (float --> normalized unsigned byte) */
#define F2UB(x) (uchar)(255.0f * x)

/* ----- Tool Buffer Drawing ------ */
/* helper functions to set color of buffer point */

static void gp_set_point_varying_color(const bGPDspoint *pt,
                                       const float ink[4],
                                       uint attr_id,
                                       bool fix_strength)
{
  float alpha = ink[3] * pt->strength;
  if ((fix_strength) && (alpha >= 0.1f)) {
    alpha = 1.0f;
  }
  CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
  immAttr4ub(attr_id, F2UB(ink[0]), F2UB(ink[1]), F2UB(ink[2]), F2UB(alpha));
}

/* ----------- Volumetric Strokes --------------- */

/* draw a 3D stroke in "volumetric" style */
static void gp_draw_stroke_volumetric_3d(const bGPDspoint *points,
                                         int totpoints,
                                         short thickness,
                                         const float ink[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
  GPU_program_point_size(true);
  immBegin(GPU_PRIM_POINTS, totpoints);

  const bGPDspoint *pt = points;
  for (int i = 0; i < totpoints && pt; i++, pt++) {
    gp_set_point_varying_color(pt, ink, color, false);
    /* TODO: scale based on view transform */
    immAttr1f(size, pt->pressure * thickness);
    /* we can adjust size in vertex shader based on view/projection! */
    immVertex3fv(pos, &pt->x);
  }

  immEnd();
  immUnbindProgram();
  GPU_program_point_size(false);
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke in 3d (i.e. in 3d-space) */
static void gp_draw_stroke_3d(tGPDdraw *tgpw, short thickness, const float ink[4], bool cyclic)
{
  bGPDspoint *points = tgpw->gps->points;
  int totpoints = tgpw->gps->totpoints;

  const float viewport[2] = {(float)tgpw->winx, (float)tgpw->winy};
  float curpressure = points[0].pressure;
  float fpt[3];

  /* if cyclic needs more vertex */
  int cyclic_add = (cyclic) ? 1 : 0;

  GPUVertFormat *format = immVertexFormat();
  const struct {
    uint pos, color, thickness;
  } attr_id = {
      .pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT),
      .color = GPU_vertformat_attr_add(
          format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT),
      .thickness = GPU_vertformat_attr_add(format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT),
  };

  immBindBuiltinProgram(GPU_SHADER_GPENCIL_STROKE);
  immUniform2fv("Viewport", viewport);
  immUniform1f("pixsize", tgpw->rv3d->pixsize);
  float obj_scale = tgpw->ob ?
                        (tgpw->ob->scale[0] + tgpw->ob->scale[1] + tgpw->ob->scale[2]) / 3.0f :
                        1.0f;

  immUniform1f("objscale", obj_scale);
  int keep_size = (int)((tgpw->gpd) && (tgpw->gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS));
  immUniform1i("keep_size", keep_size);
  immUniform1f("pixfactor", tgpw->gpd->pixfactor);
  /* xray mode always to 3D space to avoid wrong zdepth calculation (T60051) */
  immUniform1i("xraymode", GP_XRAY_3DSPACE);
  immUniform1i("caps_start", (int)tgpw->gps->caps[0]);
  immUniform1i("caps_end", (int)tgpw->gps->caps[1]);
  immUniform1i("fill_stroke", (int)tgpw->is_fill_stroke);

  /* draw stroke curve */
  GPU_line_width(max_ff(curpressure * thickness, 1.0f));
  immBeginAtMost(GPU_PRIM_LINE_STRIP_ADJ, totpoints + cyclic_add + 2);
  const bGPDspoint *pt = points;

  for (int i = 0; i < totpoints; i++, pt++) {
    /* first point for adjacency (not drawn) */
    if (i == 0) {
      gp_set_point_varying_color(points, ink, attr_id.color, (bool)tgpw->is_fill_stroke);

      if ((cyclic) && (totpoints > 2)) {
        immAttr1f(attr_id.thickness, max_ff((points + totpoints - 1)->pressure * thickness, 1.0f));
        mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + totpoints - 1)->x);
      }
      else {
        immAttr1f(attr_id.thickness, max_ff((points + 1)->pressure * thickness, 1.0f));
        mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + 1)->x);
      }
      immVertex3fv(attr_id.pos, fpt);
    }
    /* set point */
    gp_set_point_varying_color(pt, ink, attr_id.color, (bool)tgpw->is_fill_stroke);
    immAttr1f(attr_id.thickness, max_ff(pt->pressure * thickness, 1.0f));
    mul_v3_m4v3(fpt, tgpw->diff_mat, &pt->x);
    immVertex3fv(attr_id.pos, fpt);
  }

  if (cyclic && totpoints > 2) {
    /* draw line to first point to complete the cycle */
    immAttr1f(attr_id.thickness, max_ff(points->pressure * thickness, 1.0f));
    mul_v3_m4v3(fpt, tgpw->diff_mat, &points->x);
    immVertex3fv(attr_id.pos, fpt);

    /* now add adjacency point (not drawn) */
    immAttr1f(attr_id.thickness, max_ff((points + 1)->pressure * thickness, 1.0f));
    mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + 1)->x);
    immVertex3fv(attr_id.pos, fpt);
  }
  /* last adjacency point (not drawn) */
  else {
    gp_set_point_varying_color(
        points + totpoints - 2, ink, attr_id.color, (bool)tgpw->is_fill_stroke);

    immAttr1f(attr_id.thickness, max_ff((points + totpoints - 2)->pressure * thickness, 1.0f));
    mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + totpoints - 2)->x);
    immVertex3fv(attr_id.pos, fpt);
  }

  immEnd();
  immUnbindProgram();
}

/* ----- Strokes Drawing ------ */

/* Helper for doing all the checks on whether a stroke can be drawn */
static bool gp_can_draw_stroke(const bGPDstroke *gps, const int dflag)
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
static void gp_draw_strokes(tGPDdraw *tgpw)
{
  float tcolor[4];
  short sthickness;
  float ink[4];
  const bool is_unique = (tgpw->gps != NULL);
  const bool use_mat = (tgpw->gpd->mat != NULL);

  GPU_program_point_size(true);

  bGPDstroke *gps_init = (tgpw->gps) ? tgpw->gps : tgpw->t_gpf->strokes.first;

  for (bGPDstroke *gps = gps_init; gps; gps = gps->next) {
    /* check if stroke can be drawn */
    if (gp_can_draw_stroke(gps, tgpw->dflag) == false) {
      continue;
    }
    /* check if the color is visible */
    Material *ma = (use_mat) ? tgpw->gpd->mat[gps->mat_nr] : BKE_material_default_gpencil();
    MaterialGPencilStyle *gp_style = (ma) ? ma->gp_style : NULL;

    if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE) ||
        /* if onion and ghost flag do not draw*/
        (tgpw->onion && (gp_style->flag & GP_MATERIAL_HIDE_ONIONSKIN))) {
      continue;
    }

    /* if disable fill, the colors with fill must be omitted too except fill boundary strokes */
    if ((tgpw->disable_fill == 1) && (gp_style->fill_rgba[3] > 0.0f) &&
        ((gps->flag & GP_STROKE_NOFILL) == 0) && (gp_style->flag & GP_MATERIAL_FILL_SHOW)) {
      continue;
    }

    /* calculate thickness */
    sthickness = gps->thickness + tgpw->lthick;

    if (tgpw->is_fill_stroke) {
      sthickness = (short)max_ii(1, sthickness / 2);
    }

    if (sthickness <= 0) {
      continue;
    }

    /* check which stroke-drawer to use */
    if (tgpw->dflag & GP_DRAWDATA_ONLY3D) {
      const int no_xray = (tgpw->dflag & GP_DRAWDATA_NO_XRAY);
      int mask_orig = 0;

      if (no_xray) {
        glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
        glDepthMask(0);
        GPU_depth_test(true);

        /* first arg is normally rv3d->dist, but this isn't
         * available here and seems to work quite well without */
        bglPolygonOffset(1.0f, 1.0f);
      }

      /* 3D Stroke */
      /* set color using material tint color and opacity */
      if (!tgpw->onion) {
        interp_v3_v3v3(tcolor, gp_style->stroke_rgba, tgpw->tintcolor, tgpw->tintcolor[3]);
        tcolor[3] = gp_style->stroke_rgba[3] * tgpw->opacity;
        copy_v4_v4(ink, tcolor);
      }
      else {
        if (tgpw->custonion) {
          copy_v4_v4(ink, tgpw->tintcolor);
        }
        else {
          ARRAY_SET_ITEMS(tcolor, UNPACK3(gp_style->stroke_rgba), tgpw->opacity);
          copy_v4_v4(ink, tcolor);
        }
      }

      /* if used for fill, set opacity to 1 */
      if (tgpw->is_fill_stroke) {
        if (ink[3] >= GPENCIL_ALPHA_OPACITY_THRESH) {
          ink[3] = 1.0f;
        }
      }

      if (gp_style->mode == GP_MATERIAL_MODE_DOT) {
        /* volumetric stroke drawing */
        if (tgpw->disable_fill != 1) {
          gp_draw_stroke_volumetric_3d(gps->points, gps->totpoints, sthickness, ink);
        }
      }
      else {
        /* 3D Lines - OpenGL primitives-based */
        if (gps->totpoints > 1) {
          tgpw->gps = gps;
          gp_draw_stroke_3d(tgpw, sthickness, ink, gps->flag & GP_STROKE_CYCLIC);
        }
      }
      if (no_xray) {
        glDepthMask(mask_orig);
        GPU_depth_test(false);

        bglPolygonOffset(0.0, 0.0);
      }
    }
    /* if only one stroke, exit from loop */
    if (is_unique) {
      break;
    }
  }

  GPU_program_point_size(false);
}

/* ----- General Drawing ------ */

/* wrapper to draw strokes for filling operator */
void ED_gp_draw_fill(tGPDdraw *tgpw)
{
  gp_draw_strokes(tgpw);
}
