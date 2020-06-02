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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

#include "BLI_sys_types.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_dlrbTree.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_mask.h"
#include "BKE_nla.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

/* *************************************************** */
/* CURRENT FRAME DRAWING */

/* General call for drawing current frame indicator in animation editor */
void ANIM_draw_cfra(const bContext *C, View2D *v2d, short flag)
{
  Scene *scene = CTX_data_scene(C);

  const float time = scene->r.cfra + scene->r.subframe;
  const float x = (float)(time * scene->r.framelen);

  GPU_line_width((flag & DRAWCFRA_WIDE) ? 3.0 : 2.0);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* Draw a light green line to indicate current frame */
  immUniformThemeColor(TH_CFRAME);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, x, v2d->cur.ymin - 500.0f); /* XXX arbitrary... want it go to bottom */
  immVertex2f(pos, x, v2d->cur.ymax);
  immEnd();
  immUnbindProgram();
}

/* *************************************************** */
/* PREVIEW RANGE 'CURTAINS' */
/* Note: 'Preview Range' tools are defined in anim_ops.c */

/* Draw preview range 'curtains' for highlighting where the animation data is */
void ANIM_draw_previewrange(const bContext *C, View2D *v2d, int end_frame_width)
{
  Scene *scene = CTX_data_scene(C);

  /* only draw this if preview range is set */
  if (PRVRANGEON) {
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColorShadeAlpha(TH_ANIM_PREVIEW_RANGE, -25, -30);
    /* XXX: Fix this hardcoded color (anim_active) */
    // immUniformColor4f(0.8f, 0.44f, 0.1f, 0.2f);

    /* only draw two separate 'curtains' if there's no overlap between them */
    if (PSFRA < PEFRA + end_frame_width) {
      immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, (float)PSFRA, v2d->cur.ymax);
      immRectf(pos, (float)(PEFRA + end_frame_width), v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
    }
    else {
      immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
    }

    immUnbindProgram();

    GPU_blend(false);
  }
}

/* *************************************************** */
/* SCENE FRAME RANGE */

/* Draw frame range guides (for scene frame range) in background */
// TODO: Should we still show these when preview range is enabled?
void ANIM_draw_framerange(Scene *scene, View2D *v2d)
{
  /* draw darkened area outside of active timeline frame range */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -25, -100);

  if (SFRA < EFRA) {
    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
    immRectf(pos, (float)EFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
  }
  else {
    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
  }

  GPU_blend(false);

  /* thin lines where the actual frames are */
  immUniformThemeColorShade(TH_BACK, -60);

  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, (float)SFRA, v2d->cur.ymin);
  immVertex2f(pos, (float)SFRA, v2d->cur.ymax);

  immVertex2f(pos, (float)EFRA, v2d->cur.ymin);
  immVertex2f(pos, (float)EFRA, v2d->cur.ymax);

  immEnd();
  immUnbindProgram();
}

/* *************************************************** */
/* NLA-MAPPING UTILITIES (required for drawing and also editing keyframes)  */

/* Obtain the AnimData block providing NLA-mapping for the given channel (if applicable) */
// TODO: do not supply return this if the animdata tells us that there is no mapping to perform
AnimData *ANIM_nla_mapping_get(bAnimContext *ac, bAnimListElem *ale)
{
  /* sanity checks */
  if (ac == NULL) {
    return NULL;
  }

  /* abort if rendering - we may get some race condition issues... */
  if (G.is_rendering) {
    return NULL;
  }

  /* apart from strictly keyframe-related contexts, this shouldn't even happen */
  // XXX: nla and channel here may not be necessary...
  if (ELEM(ac->datatype,
           ANIMCONT_ACTION,
           ANIMCONT_SHAPEKEY,
           ANIMCONT_DOPESHEET,
           ANIMCONT_FCURVES,
           ANIMCONT_NLA,
           ANIMCONT_CHANNEL)) {
    /* handling depends on the type of animation-context we've got */
    if (ale) {
      /* NLA Control Curves occur on NLA strips,
       * and shouldn't be subjected to this kind of mapping. */
      if (ale->type != ANIMTYPE_NLACURVE) {
        return ale->adt;
      }
    }
  }

  /* cannot handle... */
  return NULL;
}

/* ------------------- */

/* Helper function for ANIM_nla_mapping_apply_fcurve() -> "restore",
 * i.e. mapping points back to action-time. */
static short bezt_nlamapping_restore(KeyframeEditData *ked, BezTriple *bezt)
{
  /* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
  AnimData *adt = (AnimData *)ked->data;
  short only_keys = (short)ked->i1;

  /* adjust BezTriple handles only if allowed to */
  if (only_keys == 0) {
    bezt->vec[0][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_UNMAP);
    bezt->vec[2][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_UNMAP);
  }

  bezt->vec[1][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_UNMAP);

  return 0;
}

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "apply",
 * i.e. mapping points to NLA-mapped global time */
static short bezt_nlamapping_apply(KeyframeEditData *ked, BezTriple *bezt)
{
  /* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
  AnimData *adt = (AnimData *)ked->data;
  short only_keys = (short)ked->i1;

  /* adjust BezTriple handles only if allowed to */
  if (only_keys == 0) {
    bezt->vec[0][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_MAP);
    bezt->vec[2][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_MAP);
  }

  bezt->vec[1][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_MAP);

  return 0;
}

/* Apply/Unapply NLA mapping to all keyframes in the nominated F-Curve
 * - restore = whether to map points back to non-mapped time
 * - only_keys = whether to only adjust the location of the center point of beztriples
 */
void ANIM_nla_mapping_apply_fcurve(AnimData *adt, FCurve *fcu, bool restore, bool only_keys)
{
  KeyframeEditData ked = {{NULL}};
  KeyframeEditFunc map_cb;

  /* init edit data
   * - AnimData is stored in 'data'
   * - only_keys is stored in 'i1'
   */
  ked.data = (void *)adt;
  ked.i1 = (int)only_keys;

  /* get editing callback */
  if (restore) {
    map_cb = bezt_nlamapping_restore;
  }
  else {
    map_cb = bezt_nlamapping_apply;
  }

  /* apply to F-Curve */
  ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, map_cb, NULL);
}

/* *************************************************** */
/* UNITS CONVERSION MAPPING (required for drawing and editing keyframes) */

/* Get flags used for normalization in ANIM_unit_mapping_get_factor. */
short ANIM_get_normalization_flags(bAnimContext *ac)
{
  if (ac->sl->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)ac->sl;
    bool use_normalization = (sipo->flag & SIPO_NORMALIZE) != 0;
    bool freeze_normalization = (sipo->flag & SIPO_NORMALIZE_FREEZE) != 0;
    return use_normalization ? (ANIM_UNITCONV_NORMALIZE |
                                (freeze_normalization ? ANIM_UNITCONV_NORMALIZE_FREEZE : 0)) :
                               0;
  }

  return 0;
}

static float normalization_factor_get(Scene *scene, FCurve *fcu, short flag, float *r_offset)
{
  float factor = 1.0f, offset = 0.0f;

  if (flag & ANIM_UNITCONV_RESTORE) {
    if (r_offset) {
      *r_offset = fcu->prev_offset;
    }

    return 1.0f / fcu->prev_norm_factor;
  }

  if (flag & ANIM_UNITCONV_NORMALIZE_FREEZE) {
    if (r_offset) {
      *r_offset = fcu->prev_offset;
    }
    if (fcu->prev_norm_factor == 0.0f) {
      /* Happens when Auto Normalize was disabled before
       * any curves were displayed.
       */
      return 1.0f;
    }
    return fcu->prev_norm_factor;
  }

  if (G.moving & G_TRANSFORM_FCURVES) {
    if (r_offset) {
      *r_offset = fcu->prev_offset;
    }
    if (fcu->prev_norm_factor == 0.0f) {
      /* Same as above. */
      return 1.0f;
    }
    return fcu->prev_norm_factor;
  }

  fcu->prev_norm_factor = 1.0f;
  if (fcu->bezt) {
    const bool use_preview_only = PRVRANGEON;
    const BezTriple *bezt;
    int i;
    float max_coord = -FLT_MAX;
    float min_coord = FLT_MAX;
    float range;

    if (fcu->totvert < 1) {
      return 1.0f;
    }

    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      if (use_preview_only && !IN_RANGE_INCL(bezt->vec[1][0], scene->r.psfra, scene->r.pefra)) {
        continue;
      }

      if (i == 0) {
        /* We ignore extrapolation flags and handle here, and use the
         * control point position only. so we normalize "interesting"
         * part of the curve.
         *
         * Here we handle left extrapolation.
         */
        max_coord = max_ff(max_coord, bezt->vec[1][1]);

        min_coord = min_ff(min_coord, bezt->vec[1][1]);
      }
      else {
        const BezTriple *prev_bezt = bezt - 1;
        if (!ELEM(prev_bezt->ipo, BEZT_IPO_BEZ, BEZT_IPO_BACK, BEZT_IPO_ELASTIC)) {
          /* The points on the curve will lie inside the start and end points.
           * Calculate min/max using both previous and current CV.
           */
          max_coord = max_ff(max_coord, bezt->vec[1][1]);
          min_coord = min_ff(min_coord, bezt->vec[1][1]);
          max_coord = max_ff(max_coord, prev_bezt->vec[1][1]);
          min_coord = min_ff(min_coord, prev_bezt->vec[1][1]);
        }
        else {
          const int resol = fcu->driver ?
                                32 :
                                min_ii((int)(5.0f * len_v2v2(bezt->vec[1], prev_bezt->vec[1])),
                                       32);
          if (resol < 2) {
            max_coord = max_ff(max_coord, prev_bezt->vec[1][1]);
            min_coord = min_ff(min_coord, prev_bezt->vec[1][1]);
          }
          else {
            if (!ELEM(prev_bezt->ipo, BEZT_IPO_BACK, BEZT_IPO_ELASTIC)) {
              /* Calculate min/max using bezier forward differencing. */
              float data[120];
              float v1[2], v2[2], v3[2], v4[2];

              v1[0] = prev_bezt->vec[1][0];
              v1[1] = prev_bezt->vec[1][1];
              v2[0] = prev_bezt->vec[2][0];
              v2[1] = prev_bezt->vec[2][1];

              v3[0] = bezt->vec[0][0];
              v3[1] = bezt->vec[0][1];
              v4[0] = bezt->vec[1][0];
              v4[1] = bezt->vec[1][1];

              correct_bezpart(v1, v2, v3, v4);

              BKE_curve_forward_diff_bezier(
                  v1[0], v2[0], v3[0], v4[0], data, resol, sizeof(float) * 3);
              BKE_curve_forward_diff_bezier(
                  v1[1], v2[1], v3[1], v4[1], data + 1, resol, sizeof(float) * 3);

              for (int j = 0; j <= resol; ++j) {
                const float *fp = &data[j * 3];
                max_coord = max_ff(max_coord, fp[1]);
                min_coord = min_ff(min_coord, fp[1]);
              }
            }
            else {
              /* Calculate min/max using full fcurve evaluation.
               * [slower than bezier forward differencing but evaluates Back/Elastic interpolation
               * as well].*/
              float step_size = (bezt->vec[1][0] - prev_bezt->vec[1][0]) / resol;
              for (int j = 0; j <= resol; j++) {
                float eval_time = prev_bezt->vec[1][0] + step_size * j;
                float eval_value = evaluate_fcurve_only_curve(fcu, eval_time);
                max_coord = max_ff(max_coord, eval_value);
                min_coord = min_ff(min_coord, eval_value);
              }
            }
          }
        }
      }
    }

    if (max_coord > min_coord) {
      range = max_coord - min_coord;
      if (range > FLT_EPSILON) {
        factor = 2.0f / range;
      }
      offset = -min_coord - range / 2.0f;
    }
    else if (max_coord == min_coord) {
      factor = 1.0f;
      offset = -min_coord;
    }
  }
  BLI_assert(factor != 0.0f);
  if (r_offset) {
    *r_offset = offset;
  }

  fcu->prev_norm_factor = factor;
  fcu->prev_offset = offset;
  return factor;
}

/* Get unit conversion factor for given ID + F-Curve */
float ANIM_unit_mapping_get_factor(Scene *scene, ID *id, FCurve *fcu, short flag, float *r_offset)
{
  if (flag & ANIM_UNITCONV_NORMALIZE) {
    return normalization_factor_get(scene, fcu, flag, r_offset);
  }

  if (r_offset) {
    *r_offset = 0.0f;
  }

  /* sanity checks */
  if (id && fcu && fcu->rna_path) {
    PointerRNA ptr, id_ptr;
    PropertyRNA *prop;

    /* get RNA property that F-Curve affects */
    RNA_id_pointer_create(id, &id_ptr);
    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      /* rotations: radians <-> degrees? */
      if (RNA_SUBTYPE_UNIT(RNA_property_subtype(prop)) == PROP_UNIT_ROTATION) {
        /* if the radians flag is not set, default to using degrees which need conversions */
        if ((scene) && (scene->unit.system_rotation == USER_UNIT_ROT_RADIANS) == 0) {
          if (flag & ANIM_UNITCONV_RESTORE) {
            return DEG2RADF(1.0f); /* degrees to radians */
          }
          else {
            return RAD2DEGF(1.0f); /* radians to degrees */
          }
        }
      }

      /* TODO: other rotation types here as necessary */
    }
  }

  /* no mapping needs to occur... */
  return 1.0f;
}

static bool find_prev_next_keyframes(struct bContext *C, int *r_nextfra, int *r_prevfra)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Mask *mask = CTX_data_edit_mask(C);
  bDopeSheet ads = {NULL};
  DLRBT_Tree keys;
  ActKeyColumn *aknext, *akprev;
  float cfranext, cfraprev;
  bool donenext = false, doneprev = false;
  int nextcount = 0, prevcount = 0;

  cfranext = cfraprev = (float)(CFRA);

  /* init binarytree-list for getting keyframes */
  BLI_dlrbTree_init(&keys);

  /* seed up dummy dopesheet context with flags to perform necessary filtering */
  if ((scene->flag & SCE_KEYS_NO_SELONLY) == 0) {
    /* only selected channels are included */
    ads.filterflag |= ADS_FILTER_ONLYSEL;
  }

  /* populate tree with keyframe nodes */
  scene_to_keylist(&ads, scene, &keys, 0);
  gpencil_to_keylist(&ads, scene->gpd, &keys, false);

  if (ob) {
    ob_to_keylist(&ads, ob, &keys, 0);
    gpencil_to_keylist(&ads, ob->data, &keys, false);
  }

  if (mask) {
    MaskLayer *masklay = BKE_mask_layer_active(mask);
    mask_to_keylist(&ads, masklay, &keys);
  }

  /* find matching keyframe in the right direction */
  do {
    aknext = (ActKeyColumn *)BLI_dlrbTree_search_next(&keys, compare_ak_cfraPtr, &cfranext);

    if (aknext) {
      if (CFRA == (int)aknext->cfra) {
        /* make this the new starting point for the search and ignore */
        cfranext = aknext->cfra;
      }
      else {
        /* this changes the frame, so set the frame and we're done */
        if (++nextcount == U.view_frame_keyframes) {
          donenext = true;
        }
      }
      cfranext = aknext->cfra;
    }
  } while ((aknext != NULL) && (donenext == false));

  do {
    akprev = (ActKeyColumn *)BLI_dlrbTree_search_prev(&keys, compare_ak_cfraPtr, &cfraprev);

    if (akprev) {
      if (CFRA == (int)akprev->cfra) {
        /* make this the new starting point for the search */
      }
      else {
        /* this changes the frame, so set the frame and we're done */
        if (++prevcount == U.view_frame_keyframes) {
          doneprev = true;
        }
      }
      cfraprev = akprev->cfra;
    }
  } while ((akprev != NULL) && (doneprev == false));

  /* free temp stuff */
  BLI_dlrbTree_free(&keys);

  /* any success? */
  if (doneprev || donenext) {
    if (doneprev) {
      *r_prevfra = cfraprev;
    }
    else {
      *r_prevfra = CFRA - (cfranext - CFRA);
    }

    if (donenext) {
      *r_nextfra = cfranext;
    }
    else {
      *r_nextfra = CFRA + (CFRA - cfraprev);
    }

    return true;
  }

  return false;
}

void ANIM_center_frame(struct bContext *C, int smooth_viewtx)
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  float w = BLI_rctf_size_x(&region->v2d.cur);
  rctf newrct;
  int nextfra, prevfra;

  switch (U.view_frame_type) {
    case ZOOM_FRAME_MODE_SECONDS: {
      const float fps = FPS;
      newrct.xmax = scene->r.cfra + U.view_frame_seconds * fps + 1;
      newrct.xmin = scene->r.cfra - U.view_frame_seconds * fps - 1;
      newrct.ymax = region->v2d.cur.ymax;
      newrct.ymin = region->v2d.cur.ymin;
      break;
    }

    /* hardest case of all, look for all keyframes around frame and display those */
    case ZOOM_FRAME_MODE_KEYFRAMES:
      if (find_prev_next_keyframes(C, &nextfra, &prevfra)) {
        newrct.xmax = nextfra;
        newrct.xmin = prevfra;
        newrct.ymax = region->v2d.cur.ymax;
        newrct.ymin = region->v2d.cur.ymin;
        break;
      }
      /* else drop through, keep range instead */
      ATTR_FALLTHROUGH;

    case ZOOM_FRAME_MODE_KEEP_RANGE:
    default:
      newrct.xmax = scene->r.cfra + (w / 2);
      newrct.xmin = scene->r.cfra - (w / 2);
      newrct.ymax = region->v2d.cur.ymax;
      newrct.ymin = region->v2d.cur.ymin;
      break;
  }

  UI_view2d_smooth_view(C, region, &newrct, smooth_viewtx);
}
/* *************************************************** */
