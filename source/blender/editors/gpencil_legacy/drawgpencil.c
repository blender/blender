/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_paint.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_shader_shared.h"
#include "GPU_state.h"
#include "GPU_uniform_buffer.h"

#include "ED_gpencil_legacy.h"
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
  /** Don't draw XRAY in 3D view (which is default). */
  GP_DRAWDATA_NO_XRAY = (1 << 5),
  /** No onion-skins should be drawn (for animation playback). */
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

static void gpencil_set_point_varying_color(const bGPDspoint *pt,
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
static void gpencil_draw_stroke_volumetric_3d(const bGPDspoint *points,
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
    gpencil_set_point_varying_color(pt, ink, color, false);
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
static void gpencil_draw_stroke_3d(tGPDdraw *tgpw,
                                   short thickness,
                                   const float ink[4],
                                   bool cyclic)
{
  bGPDspoint *points = tgpw->gps->points;
  int totpoints = tgpw->gps->totpoints;

  const float viewport[2] = {(float)tgpw->winx, (float)tgpw->winy};
  const float min_thickness = 0.05f;

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

  float obj_scale = tgpw->ob ?
                        (tgpw->ob->scale[0] + tgpw->ob->scale[1] + tgpw->ob->scale[2]) / 3.0f :
                        1.0f;

  struct GPencilStrokeData gpencil_stroke_data;
  copy_v2_v2(gpencil_stroke_data.viewport, viewport);
  gpencil_stroke_data.pixsize = tgpw->rv3d->pixsize;
  gpencil_stroke_data.objscale = obj_scale;
  int keep_size = (int)((tgpw->gpd) && (tgpw->gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS));
  gpencil_stroke_data.keep_size = keep_size;
  gpencil_stroke_data.pixfactor = tgpw->gpd->pixfactor;
  /* X-ray mode always to 3D space to avoid wrong Z-depth calculation (#60051). */
  gpencil_stroke_data.xraymode = GP_XRAY_3DSPACE;
  gpencil_stroke_data.caps_start = tgpw->gps->caps[0];
  gpencil_stroke_data.caps_end = tgpw->gps->caps[1];
  gpencil_stroke_data.fill_stroke = tgpw->is_fill_stroke;

  GPUUniformBuf *ubo = GPU_uniformbuf_create_ex(
      sizeof(struct GPencilStrokeData), &gpencil_stroke_data, __func__);
  immBindUniformBuf("gpencil_stroke_data", ubo);

  /* draw stroke curve */
  immBeginAtMost(GPU_PRIM_LINE_STRIP_ADJ, totpoints + cyclic_add + 2);
  const bGPDspoint *pt = points;

  for (int i = 0; i < totpoints; i++, pt++) {
    /* first point for adjacency (not drawn) */
    if (i == 0) {
      gpencil_set_point_varying_color(points, ink, attr_id.color, (bool)tgpw->is_fill_stroke);

      if ((cyclic) && (totpoints > 2)) {
        immAttr1f(attr_id.thickness,
                  max_ff((points + totpoints - 1)->pressure * thickness, min_thickness));
        mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + totpoints - 1)->x);
      }
      else {
        immAttr1f(attr_id.thickness, max_ff((points + 1)->pressure * thickness, min_thickness));
        mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + 1)->x);
      }
      immVertex3fv(attr_id.pos, fpt);
    }
    /* set point */
    gpencil_set_point_varying_color(pt, ink, attr_id.color, (bool)tgpw->is_fill_stroke);
    immAttr1f(attr_id.thickness, max_ff(pt->pressure * thickness, min_thickness));
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
    gpencil_set_point_varying_color(
        points + totpoints - 2, ink, attr_id.color, (bool)tgpw->is_fill_stroke);

    immAttr1f(attr_id.thickness, max_ff((points + totpoints - 2)->pressure * thickness, 1.0f));
    mul_v3_m4v3(fpt, tgpw->diff_mat, &(points + totpoints - 2)->x);
    immVertex3fv(attr_id.pos, fpt);
  }

  immEnd();
  immUnbindProgram();

  GPU_uniformbuf_free(ubo);
}

/* ----- Strokes Drawing ------ */

/* Helper for doing all the checks on whether a stroke can be drawn */
static bool gpencil_can_draw_stroke(const bGPDstroke *gps, const int dflag)
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
static void gpencil_draw_strokes(tGPDdraw *tgpw)
{
  float tcolor[4];
  short sthickness;
  float ink[4];
  const bool is_unique = (tgpw->gps != NULL);
  const bool use_mat = (tgpw->gpd->mat != NULL);

  GPU_program_point_size(true);

  /* Do not write to depth (avoid self-occlusion). */
  bool prev_depth_mask = GPU_depth_mask_get();
  GPU_depth_mask(false);

  bGPDstroke *gps_init = (tgpw->gps) ? tgpw->gps : tgpw->t_gpf->strokes.first;

  for (bGPDstroke *gps = gps_init; gps; gps = gps->next) {
    /* check if stroke can be drawn */
    if (gpencil_can_draw_stroke(gps, tgpw->dflag) == false) {
      continue;
    }
    /* check if the color is visible */
    Material *ma = (use_mat) ? tgpw->gpd->mat[gps->mat_nr] : BKE_material_default_gpencil();
    MaterialGPencilStyle *gp_style = (ma) ? ma->gp_style : NULL;

    if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE) ||
        /* If onion and ghost flag do not draw. */
        (tgpw->onion && (gp_style->flag & GP_MATERIAL_HIDE_ONIONSKIN)))
    {
      continue;
    }

    /* if disable fill, the colors with fill must be omitted too except fill boundary strokes */
    if ((tgpw->disable_fill == 1) && (gp_style->fill_rgba[3] > 0.0f) &&
        ((gps->flag & GP_STROKE_NOFILL) == 0) && (gp_style->flag & GP_MATERIAL_FILL_SHOW))
    {
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

      if (no_xray) {
        GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

        /* first arg is normally rv3d->dist, but this isn't
         * available here and seems to work quite well without */
        GPU_polygon_offset(1.0f, 1.0f);
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
          gpencil_draw_stroke_volumetric_3d(gps->points, gps->totpoints, sthickness, ink);
        }
      }
      else {
        /* 3D Lines - OpenGL primitives-based */
        if (gps->totpoints > 1) {
          tgpw->gps = gps;
          gpencil_draw_stroke_3d(tgpw, sthickness, ink, gps->flag & GP_STROKE_CYCLIC);
        }
      }
      if (no_xray) {
        GPU_depth_test(GPU_DEPTH_NONE);

        GPU_polygon_offset(0.0f, 0.0f);
      }
    }
    /* if only one stroke, exit from loop */
    if (is_unique) {
      break;
    }
  }

  GPU_depth_mask(prev_depth_mask);
  GPU_program_point_size(false);
}

/* ----- General Drawing ------ */

void ED_gpencil_draw_fill(tGPDdraw *tgpw)
{
  gpencil_draw_strokes(tgpw);
}
