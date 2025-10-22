/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_curve_types.h"

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.hh"
#include "BKE_curve.hh"
#include "BKE_fcurve.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "BLO_read_write.hh"

/* ********************************* color curve ********************* */

/* ***************** operations on full struct ************* */

void BKE_curvemapping_set_defaults(CurveMapping *cumap,
                                   int tot,
                                   float minx,
                                   float miny,
                                   float maxx,
                                   float maxy,
                                   short default_handle_type)
{
  int a;
  float clipminx, clipminy, clipmaxx, clipmaxy;

  cumap->flag = CUMA_DO_CLIP | CUMA_EXTEND_EXTRAPOLATE;
  if (tot == 4) {
    cumap->cur = 3; /* rhms, hack for 'col' curve? */
  }

  clipminx = min_ff(minx, maxx);
  clipminy = min_ff(miny, maxy);
  clipmaxx = max_ff(minx, maxx);
  clipmaxy = max_ff(miny, maxy);

  BLI_rctf_init(&cumap->curr, clipminx, clipmaxx, clipminy, clipmaxy);
  cumap->clipr = cumap->curr;

  cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
  cumap->bwmul[0] = cumap->bwmul[1] = cumap->bwmul[2] = 1.0f;

  for (a = 0; a < tot; a++) {
    if (default_handle_type == HD_VECT) {
      cumap->cm[a].default_handle_type = CUMA_HANDLE_VECTOR;
    }
    else if (default_handle_type == HD_AUTO_ANIM) {
      cumap->cm[a].default_handle_type = CUMA_HANDLE_AUTO_ANIM;
    }

    cumap->cm[a].totpoint = 2;
    cumap->cm[a].curve = MEM_calloc_arrayN<CurveMapPoint>(2, "curve points");

    cumap->cm[a].curve[0].x = minx;
    cumap->cm[a].curve[0].y = miny;
    cumap->cm[a].curve[0].flag |= default_handle_type;
    cumap->cm[a].curve[1].x = maxx;
    cumap->cm[a].curve[1].y = maxy;
    cumap->cm[a].curve[1].flag |= default_handle_type;
  }

  cumap->changed_timestamp = 0;
}

CurveMapping *BKE_curvemapping_add(int tot, float minx, float miny, float maxx, float maxy)
{
  CurveMapping *cumap;

  cumap = MEM_callocN<CurveMapping>("new curvemap");

  BKE_curvemapping_set_defaults(cumap, tot, minx, miny, maxx, maxy, HD_AUTO);

  return cumap;
}

void BKE_curvemapping_free_data(CurveMapping *cumap)
{
  int a;

  for (a = 0; a < CM_TOT; a++) {
    if (cumap->cm[a].curve) {
      MEM_freeN(cumap->cm[a].curve);
      cumap->cm[a].curve = nullptr;
    }
    if (cumap->cm[a].table) {
      MEM_freeN(cumap->cm[a].table);
      cumap->cm[a].table = nullptr;
    }
    if (cumap->cm[a].premultable) {
      MEM_freeN(cumap->cm[a].premultable);
      cumap->cm[a].premultable = nullptr;
    }
  }
}

void BKE_curvemapping_free(CurveMapping *cumap)
{
  if (cumap) {
    BKE_curvemapping_free_data(cumap);
    MEM_freeN(cumap);
  }
}

void BKE_curvemapping_copy_data(CurveMapping *target, const CurveMapping *cumap)
{
  int a;

  *target = *cumap;

  for (a = 0; a < CM_TOT; a++) {
    if (cumap->cm[a].curve) {
      target->cm[a].curve = static_cast<CurveMapPoint *>(MEM_dupallocN(cumap->cm[a].curve));
    }
    if (cumap->cm[a].table) {
      target->cm[a].table = static_cast<CurveMapPoint *>(MEM_dupallocN(cumap->cm[a].table));
    }
    if (cumap->cm[a].premultable) {
      target->cm[a].premultable = static_cast<CurveMapPoint *>(
          MEM_dupallocN(cumap->cm[a].premultable));
    }
  }
}

CurveMapping *BKE_curvemapping_copy(const CurveMapping *cumap)
{
  if (cumap) {
    CurveMapping *cumapn = static_cast<CurveMapping *>(MEM_dupallocN(cumap));
    BKE_curvemapping_copy_data(cumapn, cumap);
    return cumapn;
  }
  return nullptr;
}

void BKE_curvemapping_set_black_white_ex(const float black[3],
                                         const float white[3],
                                         float r_bwmul[3])
{
  int a;

  for (a = 0; a < 3; a++) {
    const float delta = max_ff(white[a] - black[a], 1e-5f);
    r_bwmul[a] = 1.0f / delta;
  }
}

void BKE_curvemapping_set_black_white(CurveMapping *cumap,
                                      const float black[3],
                                      const float white[3])
{
  if (white) {
    copy_v3_v3(cumap->white, white);
  }
  if (black) {
    copy_v3_v3(cumap->black, black);
  }

  BKE_curvemapping_set_black_white_ex(cumap->black, cumap->white, cumap->bwmul);
  cumap->changed_timestamp++;
}

/* ***************** operations on single curve ************* */
/* ********** NOTE: requires BKE_curvemapping_changed() call after ******** */

bool BKE_curvemap_remove_point(CurveMap *cuma, CurveMapPoint *point)
{
  CurveMapPoint *cmp;
  int a, b, removed = 0;

  /* must have 2 points minimum */
  if (cuma->totpoint <= 2) {
    return false;
  }

  cmp = MEM_malloc_arrayN<CurveMapPoint>(size_t(cuma->totpoint), "curve points");

  /* well, lets keep the two outer points! */
  for (a = 0, b = 0; a < cuma->totpoint; a++) {
    if (&cuma->curve[a] != point) {
      cmp[b] = cuma->curve[a];
      b++;
    }
    else {
      removed++;
    }
  }

  MEM_freeN(cuma->curve);
  cuma->curve = cmp;
  cuma->totpoint -= removed;
  return (removed != 0);
}

void BKE_curvemap_remove(CurveMap *cuma, const short flag)
{
  CurveMapPoint *cmp = MEM_malloc_arrayN<CurveMapPoint>(size_t(cuma->totpoint), "curve points");
  int a, b, removed = 0;

  /* well, lets keep the two outer points! */
  cmp[0] = cuma->curve[0];
  for (a = 1, b = 1; a < cuma->totpoint - 1; a++) {
    if (!(cuma->curve[a].flag & flag)) {
      cmp[b] = cuma->curve[a];
      b++;
    }
    else {
      removed++;
    }
  }
  cmp[b] = cuma->curve[a];

  MEM_freeN(cuma->curve);
  cuma->curve = cmp;
  cuma->totpoint -= removed;
}

CurveMapPoint *BKE_curvemap_insert(CurveMap *cuma, float x, float y)
{
  CurveMapPoint *cmp = MEM_calloc_arrayN<CurveMapPoint>(size_t(cuma->totpoint) + 1,
                                                        "curve points");
  CurveMapPoint *newcmp = nullptr;
  int a, b;
  bool foundloc = false;

  /* insert fragments of the old one and the new point to the new curve */
  cuma->totpoint++;
  for (a = 0, b = 0; a < cuma->totpoint; a++) {
    if ((foundloc == false) && ((a + 1 == cuma->totpoint) || (x < cuma->curve[a].x))) {
      cmp[a].x = x;
      cmp[a].y = y;
      cmp[a].flag = CUMA_SELECT;
      cmp[a].flag |= cuma->default_handle_type;
      foundloc = true;
      newcmp = &cmp[a];
    }
    else {
      cmp[a].x = cuma->curve[b].x;
      cmp[a].y = cuma->curve[b].y;
      /* make sure old points don't remain selected */
      cmp[a].flag = cuma->curve[b].flag & ~CUMA_SELECT;
      cmp[a].shorty = cuma->curve[b].shorty;
      b++;
    }
  }

  /* free old curve and replace it with new one */
  MEM_freeN(cuma->curve);
  cuma->curve = cmp;

  return newcmp;
}

void BKE_curvemap_reset(CurveMap *cuma, const rctf *clipr, int preset, CurveMapSlopeType slope)
{
  if (cuma->curve) {
    MEM_freeN(cuma->curve);
  }

  switch (preset) {
    case CURVE_PRESET_LINE:
    case CURVE_PRESET_CONSTANT_MEDIAN:
      cuma->totpoint = 2;
      break;
    case CURVE_PRESET_SHARP:
      cuma->totpoint = 5;
      break;
    case CURVE_PRESET_SMOOTH:
      cuma->totpoint = 8;
      break;
    case CURVE_PRESET_MAX:
      cuma->totpoint = 2;
      break;
    case CURVE_PRESET_MID8:
      cuma->totpoint = 8;
      break;
    case CURVE_PRESET_ROUND:
      cuma->totpoint = 6;
      break;
    case CURVE_PRESET_ROOT:
      cuma->totpoint = 6;
      break;
    case CURVE_PRESET_GAUSS:
      cuma->totpoint = 7;
      break;
    case CURVE_PRESET_BELL:
      cuma->totpoint = 3;
      break;
  }

  cuma->curve = MEM_calloc_arrayN<CurveMapPoint>(cuma->totpoint, "curve points");

  for (int i = 0; i < cuma->totpoint; i++) {
    cuma->curve[i].flag = cuma->default_handle_type;
  }

  switch (preset) {
    case CURVE_PRESET_LINE:
      cuma->curve[0].x = clipr->xmin;
      cuma->curve[0].y = clipr->ymax;
      cuma->curve[1].x = clipr->xmax;
      cuma->curve[1].y = clipr->ymin;
      if (slope == CurveMapSlopeType::PositiveNegative) {
        cuma->curve[0].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[1].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[0].flag |= CUMA_HANDLE_VECTOR;
        cuma->curve[1].flag |= CUMA_HANDLE_VECTOR;
      }
      break;
    case CURVE_PRESET_CONSTANT_MEDIAN:
      cuma->curve[0].x = clipr->xmin;
      cuma->curve[0].y = (clipr->ymin + clipr->ymax) / 2.0f;
      cuma->curve[1].x = clipr->xmax;
      cuma->curve[1].y = (clipr->ymin + clipr->ymax) / 2.0f;
      break;
    case CURVE_PRESET_SHARP:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 1;
      cuma->curve[1].x = 0.25;
      cuma->curve[1].y = 0.5625;
      cuma->curve[2].x = 0.50;
      cuma->curve[2].y = 0.25;
      cuma->curve[3].x = 0.75;
      cuma->curve[3].y = 0.0625;
      cuma->curve[4].x = 1;
      cuma->curve[4].y = 0;
      if (slope == CurveMapSlopeType::PositiveNegative) {
        cuma->curve[0].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[0].flag |= CUMA_HANDLE_VECTOR;
        cuma->curve[4].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[4].flag |= CUMA_HANDLE_VECTOR;
      }
      break;
    case CURVE_PRESET_SMOOTH:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 1;
      cuma->curve[1].x = 0.12;
      cuma->curve[1].y = 0.96;
      cuma->curve[2].x = 0.25;
      cuma->curve[2].y = 0.84;
      cuma->curve[3].x = 0.42;
      cuma->curve[3].y = 0.62;
      cuma->curve[4].x = 0.58;
      cuma->curve[4].y = 0.38;
      cuma->curve[5].x = 0.75;
      cuma->curve[5].y = 0.16;
      cuma->curve[6].x = 0.88;
      cuma->curve[6].y = 0.04;
      cuma->curve[7].x = 1;
      cuma->curve[7].y = 0;
      break;
    case CURVE_PRESET_MAX:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 1;
      cuma->curve[1].x = 1;
      cuma->curve[1].y = 1;
      break;
    case CURVE_PRESET_MID8: {
      for (int i = 0; i < cuma->totpoint; i++) {
        cuma->curve[i].x = i / float(cuma->totpoint);
        cuma->curve[i].y = 0.5;
      }
      break;
    }
    case CURVE_PRESET_ROUND:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 1;
      cuma->curve[1].x = 0.5;
      cuma->curve[1].y = 0.866;
      cuma->curve[2].x = 0.6765;
      cuma->curve[2].y = 0.7364;
      cuma->curve[3].x = 0.8582;
      cuma->curve[3].y = 0.5133;
      cuma->curve[4].x = 0.967;
      cuma->curve[4].y = 0.2547;
      cuma->curve[5].x = 1;
      cuma->curve[5].y = 0;
      break;
    case CURVE_PRESET_ROOT:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 1;
      cuma->curve[1].x = 0.25;
      cuma->curve[1].y = 0.866;
      cuma->curve[2].x = 0.5;
      cuma->curve[2].y = 0.707;
      cuma->curve[3].x = 0.75;
      cuma->curve[3].y = 0.5;
      cuma->curve[4].x = 0.9375;
      cuma->curve[4].y = 0.25;
      cuma->curve[5].x = 1;
      cuma->curve[5].y = 0;
      if (slope == CurveMapSlopeType::PositiveNegative) {
        cuma->curve[0].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[0].flag |= CUMA_HANDLE_VECTOR;
        cuma->curve[5].flag &= ~CUMA_HANDLE_AUTO_ANIM;
        cuma->curve[5].flag |= CUMA_HANDLE_VECTOR;
      }
      break;
    case CURVE_PRESET_GAUSS:
      cuma->curve[0].x = 0;
      cuma->curve[0].y = 0.025f;
      cuma->curve[1].x = 0.16f;
      cuma->curve[1].y = 0.135f;
      cuma->curve[2].x = 0.298f;
      cuma->curve[2].y = 0.36f;

      cuma->curve[3].x = 0.50f;
      cuma->curve[3].y = 1.0f;

      cuma->curve[4].x = 0.70f;
      cuma->curve[4].y = 0.36f;
      cuma->curve[5].x = 0.84f;
      cuma->curve[5].y = 0.135f;
      cuma->curve[6].x = 1.0f;
      cuma->curve[6].y = 0.025f;
      break;
    case CURVE_PRESET_BELL:
      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.025f;

      cuma->curve[1].x = 0.50f;
      cuma->curve[1].y = 1.0f;

      cuma->curve[2].x = 1.0f;
      cuma->curve[2].y = 0.025f;
      break;
  }

  /* mirror curve in x direction to have positive slope
   * rather than default negative slope */
  if (slope == CurveMapSlopeType::Positive) {
    if (ELEM(preset, CURVE_PRESET_LINE, CURVE_PRESET_CONSTANT_MEDIAN)) {
      BLI_assert(cuma->totpoint == 2);
      /* The LINE and CONSTANT_MEDIAN presets are defined by a single pair of points, relative to
       * the clip region. */
      std::swap(cuma->curve[0].y, cuma->curve[1].y);
    }
    else {
      int i, last = cuma->totpoint - 1;
      /* For all curves other than the LINE and CONSTANT_MEDIAN curves, we assume that the x period
       * is from [0.0, 1.0] inclusive. Resetting the curve for these presets does not take into
       * account the current clipping region. */
      BLI_assert(cuma->curve[0].x == 0.0f && cuma->curve[last].x == 1.0f);
      CurveMapPoint *newpoints = static_cast<CurveMapPoint *>(MEM_dupallocN(cuma->curve));
      for (i = 0; i < cuma->totpoint; i++) {
        newpoints[i].x = 1.0f - cuma->curve[last - i].x;
        newpoints[i].y = cuma->curve[last - i].y;
      }
      MEM_freeN(cuma->curve);
      cuma->curve = newpoints;
    }
  }
  else if (slope == CurveMapSlopeType::PositiveNegative) {
    const int num_points = cuma->totpoint * 2 - 1;
    CurveMapPoint *new_points = MEM_malloc_arrayN<CurveMapPoint>(size_t(num_points),
                                                                 "curve symmetric points");
    for (int i = 0; i < cuma->totpoint; i++) {
      const int src_last_point = cuma->totpoint - i - 1;
      const int dst_last_point = num_points - i - 1;
      new_points[i] = cuma->curve[src_last_point];
      new_points[i].x = (1.0f - cuma->curve[src_last_point].x) * 0.5f;
      new_points[dst_last_point] = new_points[i];
      new_points[dst_last_point].x = 0.5f + cuma->curve[src_last_point].x * 0.5f;
    }
    cuma->totpoint = num_points;
    MEM_freeN(cuma->curve);
    cuma->curve = new_points;
  }

  if (cuma->table) {
    MEM_freeN(cuma->table);
    cuma->table = nullptr;
  }
}

void BKE_curvemap_handle_set(CurveMap *cuma, int type)
{
  int a;

  for (a = 0; a < cuma->totpoint; a++) {
    if (cuma->curve[a].flag & CUMA_SELECT) {
      cuma->curve[a].flag &= ~(CUMA_HANDLE_VECTOR | CUMA_HANDLE_AUTO_ANIM);
      if (type == HD_VECT) {
        cuma->curve[a].flag |= CUMA_HANDLE_VECTOR;
      }
      else if (type == HD_AUTO_ANIM) {
        cuma->curve[a].flag |= CUMA_HANDLE_AUTO_ANIM;
      }
      else {
        /* pass */
      }
    }
  }
}

/* *********************** Making the tables and display ************** */

/**
 * Reduced copy of #calchandleNurb_intern code in `curve.cc`.
 */
static void calchandle_curvemap(BezTriple *bezt, const BezTriple *prev, const BezTriple *next)
{
/* defines to avoid confusion */
#define p2_h1 ((p2) - 3)
#define p2_h2 ((p2) + 3)

  const float *p1, *p3;
  float *p2;
  float pt[3];
  float len, len_a, len_b;
  float dvec_a[2], dvec_b[2];

  if (bezt->h1 == 0 && bezt->h2 == 0) {
    return;
  }

  p2 = bezt->vec[1];

  if (prev == nullptr) {
    p3 = next->vec[1];
    pt[0] = 2.0f * p2[0] - p3[0];
    pt[1] = 2.0f * p2[1] - p3[1];
    p1 = pt;
  }
  else {
    p1 = prev->vec[1];
  }

  if (next == nullptr) {
    p1 = prev->vec[1];
    pt[0] = 2.0f * p2[0] - p1[0];
    pt[1] = 2.0f * p2[1] - p1[1];
    p3 = pt;
  }
  else {
    p3 = next->vec[1];
  }

  sub_v2_v2v2(dvec_a, p2, p1);
  sub_v2_v2v2(dvec_b, p3, p2);

  len_a = len_v2(dvec_a);
  len_b = len_v2(dvec_b);

  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) || ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) { /* auto */
    float tvec[2];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;

    len = len_v2(tvec) * 2.5614f;
    if (len != 0.0f) {

      if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM)) {
        len_a /= len;
        madd_v2_v2v2fl(p2_h1, p2, tvec, -len_a);

        if ((bezt->h1 == HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
          const float ydiff1 = prev->vec[1][1] - bezt->vec[1][1];
          const float ydiff2 = next->vec[1][1] - bezt->vec[1][1];
          if ((ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f)) {
            bezt->vec[0][1] = bezt->vec[1][1];
          }
          else { /* handles should not be beyond y coord of two others */
            if (ydiff1 <= 0.0f) {
              bezt->vec[0][1] = std::max(prev->vec[1][1], bezt->vec[0][1]);
            }
            else {
              bezt->vec[0][1] = std::min(prev->vec[1][1], bezt->vec[0][1]);
            }
          }
        }
      }
      if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) {
        len_b /= len;
        madd_v2_v2v2fl(p2_h2, p2, tvec, len_b);

        if ((bezt->h2 == HD_AUTO_ANIM) && next && prev) { /* keep horizontal if extrema */
          const float ydiff1 = prev->vec[1][1] - bezt->vec[1][1];
          const float ydiff2 = next->vec[1][1] - bezt->vec[1][1];
          if ((ydiff1 <= 0.0f && ydiff2 <= 0.0f) || (ydiff1 >= 0.0f && ydiff2 >= 0.0f)) {
            bezt->vec[2][1] = bezt->vec[1][1];
          }
          else { /* handles should not be beyond y coord of two others */
            if (ydiff1 <= 0.0f) {
              bezt->vec[2][1] = std::min(next->vec[1][1], bezt->vec[2][1]);
            }
            else {
              bezt->vec[2][1] = std::max(next->vec[1][1], bezt->vec[2][1]);
            }
          }
        }
      }
    }
  }

  if (bezt->h1 == HD_VECT) { /* vector */
    madd_v2_v2v2fl(p2_h1, p2, dvec_a, -1.0f / 3.0f);
  }
  if (bezt->h2 == HD_VECT) {
    madd_v2_v2v2fl(p2_h2, p2, dvec_b, 1.0f / 3.0f);
  }

#undef p2_h1
#undef p2_h2
}

/* in X, out Y.
 * X is presumed to be outside first or last */
static float curvemap_calc_extend(const CurveMapping *cumap,
                                  const CurveMap *cuma,
                                  float x,
                                  const float first[2],
                                  const float last[2])
{
  if (x <= first[0]) {
    if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
      /* extrapolate horizontally */
      return first[1];
    }

    if (cuma->ext_in[0] == 0.0f) {
      return first[1] + cuma->ext_in[1] * 10000.0f;
    }

    return first[1] + cuma->ext_in[1] * (x - first[0]) / cuma->ext_in[0];
  }
  if (x >= last[0]) {
    if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
      /* extrapolate horizontally */
      return last[1];
    }

    if (cuma->ext_out[0] == 0.0f) {
      return last[1] - cuma->ext_out[1] * 10000.0f;
    }

    return last[1] + cuma->ext_out[1] * (x - last[0]) / cuma->ext_out[0];
  }
  return 0.0f;
}

/* Evaluates CM_RESOL number of points on the Bezier segment defined by the given start and end
 * Bezier triples, writing the output to the points array. */
static void curve_eval_bezier_point(float start[3][3], float end[3][3], float *point)
{
  BKE_curve_correct_bezpart(start[1], start[2], end[0], end[1]);
  BKE_curve_forward_diff_bezier(
      start[1][0], start[2][0], end[0][0], end[1][0], point, CM_RESOL - 1, sizeof(float[2]));
  BKE_curve_forward_diff_bezier(
      start[1][1], start[2][1], end[0][1], end[1][1], point + 1, CM_RESOL - 1, sizeof(float[2]));
}

/* only creates a table for a single channel in CurveMapping */
static void curvemap_make_table(const CurveMapping *cumap, CurveMap *cuma)
{
  const rctf *clipr = &cumap->clipr;

  /* Wrapping ensures that the heights of the first and last points are the same. It adds two
   * virtual points, which are copies of the first and last points, and moves them to the opposite
   * side of the curve offset by the table range. The handles of these points are calculated, as if
   * they were between the last and first real points. */

  const bool use_wrapping = cumap->flag & CUMA_USE_WRAPPING;

  if (cuma->curve == nullptr) {
    return;
  }

  /* default rect also is table range */
  cuma->mintable = clipr->xmin;
  cuma->maxtable = clipr->xmax;
  const int bezt_totpoint = max_ii(cuma->totpoint, 2);

  /* Rely on Blender interpolation for bezier curves, support extra functionality here as well. */
  BezTriple *bezt = MEM_calloc_arrayN<BezTriple>(bezt_totpoint, "beztarr");

  /* Valid curve has at least 2 points. */
  if (cuma->totpoint >= 2) {
    CurveMapPoint *cmp = cuma->curve;

    for (int a = 0; a < bezt_totpoint; a++) {
      cuma->mintable = min_ff(cuma->mintable, cmp[a].x);
      cuma->maxtable = max_ff(cuma->maxtable, cmp[a].x);
      bezt[a].vec[1][0] = cmp[a].x;
      bezt[a].vec[1][1] = cmp[a].y;
      if (cmp[a].flag & CUMA_HANDLE_VECTOR) {
        bezt[a].h1 = bezt[a].h2 = HD_VECT;
      }
      else if (cmp[a].flag & CUMA_HANDLE_AUTO_ANIM) {
        bezt[a].h1 = bezt[a].h2 = HD_AUTO_ANIM;
      }
      else {
        bezt[a].h1 = bezt[a].h2 = HD_AUTO;
      }
    }
  }
  else {
    /* Fallback when points are missing. */
    cuma->mintable = 0.0f;
    cuma->maxtable = 0.0f;
    zero_v2(bezt[0].vec[1]);
    zero_v2(bezt[1].vec[1]);
    bezt[0].h1 = HD_AUTO;
    bezt[0].h2 = HD_AUTO;
    bezt[1].h1 = HD_AUTO;
    bezt[1].h2 = HD_AUTO;
  }

  const BezTriple *bezt_next = nullptr;
  const BezTriple *bezt_prev = nullptr;

  /* Create two extra points for wrapping curves. */
  BezTriple bezt_pre = bezt[bezt_totpoint - 1];
  BezTriple bezt_post = bezt[0];

  BezTriple *bezt_post_ptr;

  float table_range = cuma->maxtable - cuma->mintable;
  if (use_wrapping) {
    /* Handle location of pre and post points for wrapping curves. */
    bezt_pre.h1 = bezt_pre.h2 = bezt[bezt_totpoint - 1].h2;
    bezt_pre.vec[1][0] = bezt[bezt_totpoint - 1].vec[1][0] - table_range;
    bezt_pre.vec[1][1] = bezt[bezt_totpoint - 1].vec[1][1];

    bezt_post.h1 = bezt_post.h2 = bezt[0].h1;
    bezt_post.vec[1][0] = bezt[0].vec[1][0] + table_range;
    bezt_post.vec[1][1] = bezt[0].vec[1][1];

    bezt_prev = &bezt_pre;
    bezt_post_ptr = &bezt_post;
  }
  else {
    bezt_prev = nullptr;
    bezt_post_ptr = nullptr;
  }

  /* Process middle elements */
  for (int a = 0; a < bezt_totpoint; a++) {
    bezt_next = (a != bezt_totpoint - 1) ? &bezt[a + 1] : bezt_post_ptr;
    calchandle_curvemap(&bezt[a], bezt_prev, bezt_next);
    bezt_prev = &bezt[a];
  }

  /* Correct handles of pre and post points for wrapping curves. */
  bezt_pre.vec[0][0] = bezt[bezt_totpoint - 1].vec[0][0] - table_range;
  bezt_pre.vec[0][1] = bezt[bezt_totpoint - 1].vec[0][1];
  bezt_pre.vec[2][0] = bezt[bezt_totpoint - 1].vec[2][0] - table_range;
  bezt_pre.vec[2][1] = bezt[bezt_totpoint - 1].vec[2][1];

  bezt_post.vec[0][0] = bezt[0].vec[0][0] + table_range;
  bezt_post.vec[0][1] = bezt[0].vec[0][1];
  bezt_post.vec[2][0] = bezt[0].vec[2][0] + table_range;
  bezt_post.vec[2][1] = bezt[0].vec[2][1];

  /* first and last handle need correction, instead of pointing to center of next/prev,
   * we let it point to the closest handle */
  if (bezt_totpoint > 2 && !use_wrapping) {
    float hlen, nlen, vec[3];

    if (bezt[0].h2 == HD_AUTO) {

      hlen = len_v3v3(bezt[0].vec[1], bezt[0].vec[2]); /* original handle length */
      /* clip handle point */
      copy_v3_v3(vec, bezt[1].vec[0]);
      vec[0] = std::max(vec[0], bezt[0].vec[1][0]);

      sub_v3_v3(vec, bezt[0].vec[1]);
      nlen = len_v3(vec);
      if (nlen > FLT_EPSILON) {
        mul_v3_fl(vec, hlen / nlen);
        add_v3_v3v3(bezt[0].vec[2], vec, bezt[0].vec[1]);
        sub_v3_v3v3(bezt[0].vec[0], bezt[0].vec[1], vec);
      }
    }
    int a = bezt_totpoint - 1;
    if (bezt[a].h2 == HD_AUTO) {

      hlen = len_v3v3(bezt[a].vec[1], bezt[a].vec[0]); /* original handle length */
      /* clip handle point */
      copy_v3_v3(vec, bezt[a - 1].vec[2]);
      vec[0] = std::min(vec[0], bezt[a].vec[1][0]);

      sub_v3_v3(vec, bezt[a].vec[1]);
      nlen = len_v3(vec);
      if (nlen > FLT_EPSILON) {
        mul_v3_fl(vec, hlen / nlen);
        add_v3_v3v3(bezt[a].vec[0], vec, bezt[a].vec[1]);
        sub_v3_v3v3(bezt[a].vec[2], bezt[a].vec[1], vec);
      }
    }
  }

  /* make the bezier curve */
  if (cuma->table) {
    MEM_freeN(cuma->table);
  }

  const int totpoint = use_wrapping ? (bezt_totpoint + 1) * CM_RESOL :
                                      (bezt_totpoint - 1) * CM_RESOL;
  float *allpoints = MEM_calloc_arrayN<float>(size_t(totpoint) * 2, "table");
  float *point = allpoints;

  /* Handle pre point for wrapping */
  if (use_wrapping) {
    curve_eval_bezier_point(bezt_pre.vec, bezt[0].vec, point);
    point += 2 * CM_RESOL;
  }

  /* Process middle elements */
  for (int a = 0; a < bezt_totpoint - 1; a++, point += 2 * CM_RESOL) {
    int b = a + 1;
    curve_eval_bezier_point(bezt[a].vec, bezt[b].vec, point);
  }

  if (use_wrapping) {
    /* Handle post point for wrapping */
    curve_eval_bezier_point(bezt[bezt_totpoint - 1].vec, bezt_post.vec, point);
  }
  /* Store first and last handle for extrapolation, unit length. (Only relevant when not using
   * wrapping.) */
  cuma->ext_in[0] = bezt[0].vec[0][0] - bezt[0].vec[1][0];
  cuma->ext_in[1] = bezt[0].vec[0][1] - bezt[0].vec[1][1];
  float ext_in_range = sqrtf(cuma->ext_in[0] * cuma->ext_in[0] +
                             cuma->ext_in[1] * cuma->ext_in[1]);
  cuma->ext_in[0] /= ext_in_range;
  cuma->ext_in[1] /= ext_in_range;

  int out_a = bezt_totpoint - 1;
  cuma->ext_out[0] = bezt[out_a].vec[1][0] - bezt[out_a].vec[2][0];
  cuma->ext_out[1] = bezt[out_a].vec[1][1] - bezt[out_a].vec[2][1];
  float ext_out_range = sqrtf(cuma->ext_out[0] * cuma->ext_out[0] +
                              cuma->ext_out[1] * cuma->ext_out[1]);
  cuma->ext_out[0] /= ext_out_range;
  cuma->ext_out[1] /= ext_out_range;

  /* cleanup */
  MEM_freeN(bezt);

  float range = CM_TABLEDIV * table_range;
  cuma->range = 1.0f / range;

  /* now make a table with CM_TABLE equal x distances */
  float *firstpoint = allpoints;
  float *lastpoint = allpoints + 2 * (totpoint - 1);
  point = allpoints;

  CurveMapPoint *cmp = MEM_calloc_arrayN<CurveMapPoint>(CM_TABLE + 1, "dist table");

  for (int a = 0; a <= CM_TABLE; a++) {
    float cur_x = cuma->mintable + range * float(a);
    cmp[a].x = cur_x;

    /* Get the first point with x coordinate larger than cur_x. */
    while (cur_x >= point[0] && point != lastpoint) {
      point += 2;
    }
    /* Check if we are on or outside the start or end point. */
    if ((point == firstpoint || (point == lastpoint && cur_x >= point[0])) && !use_wrapping) {
      if (compare_ff(cur_x, point[0], 1e-6f)) {
        /* When on the point exactly, use the value directly to avoid precision
         * issues with extrapolation of extreme slopes. */
        cmp[a].y = point[1];
      }
      else {
        /* Extrapolate values that lie outside the start and end point. */
        cmp[a].y = curvemap_calc_extend(cumap, cuma, cur_x, firstpoint, lastpoint);
      }
    }
    else {
      float fac1 = point[0] - point[-2];
      float fac2 = point[0] - cur_x;
      if (fac1 > FLT_EPSILON) {
        fac1 = fac2 / fac1;
      }
      else {
        fac1 = 0.0f;
      }
      cmp[a].y = fac1 * point[-1] + (1.0f - fac1) * point[1];
    }
  }

  MEM_freeN(allpoints);
  cuma->table = cmp;
}

void BKE_curvemapping_premultiply(CurveMapping *cumap, bool restore)
{
  /* It uses a flag to prevent pre-multiply or free to happen twice. */

  int a;

  if (restore) {
    if (cumap->flag & CUMA_PREMULLED) {
      for (a = 0; a < 3; a++) {
        MEM_freeN(cumap->cm[a].table);
        cumap->cm[a].table = cumap->cm[a].premultable;
        cumap->cm[a].premultable = nullptr;

        copy_v2_v2(cumap->cm[a].ext_in, cumap->cm[a].premul_ext_in);
        copy_v2_v2(cumap->cm[a].ext_out, cumap->cm[a].premul_ext_out);
        zero_v2(cumap->cm[a].premul_ext_in);
        zero_v2(cumap->cm[a].premul_ext_out);
      }

      cumap->flag &= ~CUMA_PREMULLED;
    }
  }
  else {
    if ((cumap->flag & CUMA_PREMULLED) == 0) {
      /* verify and copy */
      for (a = 0; a < 3; a++) {
        if (cumap->cm[a].table == nullptr) {
          curvemap_make_table(cumap, cumap->cm + a);
        }
        cumap->cm[a].premultable = cumap->cm[a].table;
        cumap->cm[a].table = MEM_malloc_arrayN<CurveMapPoint>(CM_TABLE + 1, "premul table");
        memcpy(
            cumap->cm[a].table, cumap->cm[a].premultable, (CM_TABLE + 1) * sizeof(CurveMapPoint));
      }

      if (cumap->cm[3].table == nullptr) {
        curvemap_make_table(cumap, cumap->cm + 3);
      }

      /* premul */
      for (a = 0; a < 3; a++) {
        int b;
        for (b = 0; b <= CM_TABLE; b++) {
          cumap->cm[a].table[b].y = BKE_curvemap_evaluateF(
              cumap, cumap->cm + 3, cumap->cm[a].table[b].y);
        }

        copy_v2_v2(cumap->cm[a].premul_ext_in, cumap->cm[a].ext_in);
        copy_v2_v2(cumap->cm[a].premul_ext_out, cumap->cm[a].ext_out);
        mul_v2_v2(cumap->cm[a].ext_in, cumap->cm[3].ext_in);
        mul_v2_v2(cumap->cm[a].ext_out, cumap->cm[3].ext_out);
      }

      cumap->flag |= CUMA_PREMULLED;
    }
  }
}

/* ************************ more CurveMapping calls *************** */

void BKE_curvemapping_changed(CurveMapping *cumap, const bool rem_doubles)
{
  CurveMap *cuma = cumap->cm + cumap->cur;
  CurveMapPoint *cmp = cuma->curve;
  const rctf *clipr = &cumap->clipr;
  float thresh = 0.01f * BLI_rctf_size_x(clipr);
  float dx = 0.0f, dy = 0.0f;
  int a;

  cumap->changed_timestamp++;

  /* clamp with clip */
  if (cumap->flag & CUMA_DO_CLIP) {
    for (a = 0; a < cuma->totpoint; a++) {
      if (cmp[a].flag & CUMA_SELECT) {
        if (cmp[a].x < clipr->xmin) {
          dx = min_ff(dx, cmp[a].x - clipr->xmin);
        }
        else if (cmp[a].x > clipr->xmax) {
          dx = max_ff(dx, cmp[a].x - clipr->xmax);
        }
        if (cmp[a].y < clipr->ymin) {
          dy = min_ff(dy, cmp[a].y - clipr->ymin);
        }
        else if (cmp[a].y > clipr->ymax) {
          dy = max_ff(dy, cmp[a].y - clipr->ymax);
        }
      }
    }
    for (a = 0; a < cuma->totpoint; a++) {
      if (cmp[a].flag & CUMA_SELECT) {
        cmp[a].x -= dx;
        cmp[a].y -= dy;
      }
    }

    /* ensure zoom-level respects clipping */
    if (BLI_rctf_size_x(&cumap->curr) > BLI_rctf_size_x(&cumap->clipr)) {
      cumap->curr.xmin = cumap->clipr.xmin;
      cumap->curr.xmax = cumap->clipr.xmax;
    }
    if (BLI_rctf_size_y(&cumap->curr) > BLI_rctf_size_y(&cumap->clipr)) {
      cumap->curr.ymin = cumap->clipr.ymin;
      cumap->curr.ymax = cumap->clipr.ymax;
    }
  }

  std::stable_sort(cuma->curve,
                   cuma->curve + cuma->totpoint,
                   [](const CurveMapPoint &a, const CurveMapPoint &b) { return a.x < b.x; });

  /* remove doubles, threshold set on 1% of default range */
  if (rem_doubles && cuma->totpoint > 2) {
    for (a = 0; a < cuma->totpoint - 1; a++) {
      dx = cmp[a].x - cmp[a + 1].x;
      dy = cmp[a].y - cmp[a + 1].y;
      if (sqrtf(dx * dx + dy * dy) < thresh) {
        if (a == 0) {
          cmp[a + 1].flag |= CUMA_REMOVE;
          if (cmp[a + 1].flag & CUMA_SELECT) {
            cmp[a].flag |= CUMA_SELECT;
          }
        }
        else {
          cmp[a].flag |= CUMA_REMOVE;
          if (cmp[a].flag & CUMA_SELECT) {
            cmp[a + 1].flag |= CUMA_SELECT;
          }
        }
        break; /* we assume 1 deletion per edit is ok */
      }
    }
    if (a != cuma->totpoint - 1) {
      BKE_curvemap_remove(cuma, CUMA_REMOVE);
    }
  }
  curvemap_make_table(cumap, cuma);
}

void BKE_curvemapping_changed_all(CurveMapping *cumap)
{
  int a, cur = cumap->cur;

  for (a = 0; a < CM_TOT; a++) {
    if (cumap->cm[a].curve) {
      cumap->cur = a;
      BKE_curvemapping_changed(cumap, false);
    }
  }

  cumap->cur = cur;
}

void BKE_curvemapping_reset_view(CurveMapping *cumap)
{
  cumap->curr = cumap->clipr;
}

float BKE_curvemap_evaluateF(const CurveMapping *cumap, const CurveMap *cuma, float value)
{
  /* index in table */
  float fi = (value - cuma->mintable) * cuma->range;
  int i = int(fi);

  /* fi is table float index and should check against table range i.e. [0.0 CM_TABLE] */
  if (fi < 0.0f || fi > CM_TABLE) {
    return curvemap_calc_extend(cumap, cuma, value, &cuma->table[0].x, &cuma->table[CM_TABLE].x);
  }

  if (i < 0) {
    return cuma->table[0].y;
  }
  if (i >= CM_TABLE) {
    return cuma->table[CM_TABLE].y;
  }

  fi = fi - float(i);
  return (1.0f - fi) * cuma->table[i].y + (fi)*cuma->table[i + 1].y;
}

float BKE_curvemapping_evaluateF(const CurveMapping *cumap, int cur, float value)
{
  const CurveMap *cuma = cumap->cm + cur;
  float val = BKE_curvemap_evaluateF(cumap, cuma, value);

  /* account for clipping */
  if (cumap->flag & CUMA_DO_CLIP) {
    if (val < cumap->clipr.ymin) {
      val = cumap->clipr.ymin;
    }
    else if (val > cumap->clipr.ymax) {
      val = cumap->clipr.ymax;
    }
  }

  return val;
}

void BKE_curvemapping_evaluate3F(const CurveMapping *cumap, float vecout[3], const float vecin[3])
{
  vecout[0] = BKE_curvemap_evaluateF(cumap, &cumap->cm[0], vecin[0]);
  vecout[1] = BKE_curvemap_evaluateF(cumap, &cumap->cm[1], vecin[1]);
  vecout[2] = BKE_curvemap_evaluateF(cumap, &cumap->cm[2], vecin[2]);
}

void BKE_curvemapping_evaluateRGBF(const CurveMapping *cumap,
                                   float vecout[3],
                                   const float vecin[3])
{
  vecout[0] = BKE_curvemap_evaluateF(
      cumap, &cumap->cm[0], BKE_curvemap_evaluateF(cumap, &cumap->cm[3], vecin[0]));
  vecout[1] = BKE_curvemap_evaluateF(
      cumap, &cumap->cm[1], BKE_curvemap_evaluateF(cumap, &cumap->cm[3], vecin[1]));
  vecout[2] = BKE_curvemap_evaluateF(
      cumap, &cumap->cm[2], BKE_curvemap_evaluateF(cumap, &cumap->cm[3], vecin[2]));
}

/* Contrary to standard tone curve implementations, the film-like implementation tries to preserve
 * the hue of the colors as much as possible. To understand why this might be a problem, consider
 * the violet color (0.5, 0.0, 1.0). If this color was to be evaluated at a power curve x^4, the
 * color will be blue (0.0625, 0.0, 1.0). So the color changes and not just its luminosity, which
 * is what film-like tone curves tries to avoid.
 *
 * First, the channels with the lowest and highest values are identified and evaluated at the
 * curve. Then, the third channel---the median---is computed while maintaining the original hue of
 * the color. To do that, we look at the equation for deriving the hue from RGB values. Assuming
 * the maximum, minimum, and median channels are known, and ignoring the 1/3 period offset of the
 * hue, the equation is:
 *
 *   hue = (median - min) / (max - min)                                  [1]
 *
 * Since we have the new values for the minimum and maximum after evaluating at the curve, we also
 * have:
 *
 *   hue = (new_median - new_min) / (new_max - new_min)                  [2]
 *
 * Since we want the hue to be equivalent, by equating [1] and [2] and rearranging:
 *
 *   (new_median - new_min) / (new_max - new_min) = (median - min) / (max - min)
 *   new_median - new_min = (new_max - new_min) * (median - min) / (max - min)
 *   new_median = new_min + (new_max - new_min) * (median - min) / (max - min)
 *   new_median = new_min + (median - min) * ((new_max - new_min) / (max - min))  [QED]
 *
 * Which gives us the median color that preserves the hue. More intuitively, the median is computed
 * such that the change in the distance from the median to the minimum is proportional to the
 * change in the distance from the minimum to the maximum. Finally, each of the new minimum,
 * maximum, and median values are written to the color channel that they were originally extracted
 * from. */
static blender::float3 evaluate_film_like(const CurveMapping *curve_mapping, blender::float3 input)
{
  /* Film-like curves are only evaluated on the combined curve, which is the fourth curve map. */
  const CurveMap *curve_map = curve_mapping->cm + 3;

  /* Find the maximum, minimum, and median of the color channels. */
  const float minimum = blender::math::reduce_min(input);
  const float maximum = blender::math::reduce_max(input);
  const float median = blender::math::max(
      blender::math::min(input.x, input.y),
      blender::math::min(input.z, blender::math::max(input.x, input.y)));

  const float new_min = BKE_curvemap_evaluateF(curve_mapping, curve_map, minimum);
  const float new_max = BKE_curvemap_evaluateF(curve_mapping, curve_map, maximum);

  /* Compute the new median using the ratio between the new and the original range. */
  const float scaling_ratio = (new_max - new_min) / (maximum - minimum);
  const float new_median = new_min + (median - minimum) * scaling_ratio;

  /* Write each value to its original channel. */
  const blender::float3 median_or_min = blender::float3(input.x == minimum ? new_min : new_median,
                                                        input.y == minimum ? new_min : new_median,
                                                        input.z == minimum ? new_min : new_median);
  return blender::float3(input.x == maximum ? new_max : median_or_min.x,
                         input.y == maximum ? new_max : median_or_min.y,
                         input.z == maximum ? new_max : median_or_min.z);
}

void BKE_curvemapping_evaluate_premulRGBF_ex(const CurveMapping *cumap,
                                             float vecout[3],
                                             const float vecin[3],
                                             const float black[3],
                                             const float bwmul[3])
{
  const float r = (vecin[0] - black[0]) * bwmul[0];
  const float g = (vecin[1] - black[1]) * bwmul[1];
  const float b = (vecin[2] - black[2]) * bwmul[2];
  const float balanced_color[3] = {r, g, b};

  switch (cumap->tone) {
    default:
    case CURVE_TONE_STANDARD: {
      vecout[0] = BKE_curvemap_evaluateF(cumap, &cumap->cm[0], r);
      vecout[1] = BKE_curvemap_evaluateF(cumap, &cumap->cm[1], g);
      vecout[2] = BKE_curvemap_evaluateF(cumap, &cumap->cm[2], b);
      break;
    }
    case CURVE_TONE_FILMLIKE: {
      const blender::float3 output = evaluate_film_like(cumap, balanced_color);
      copy_v3_v3(vecout, output);
      break;
    }
  }
}

void BKE_curvemapping_evaluate_premulRGBF(const CurveMapping *cumap,
                                          float vecout[3],
                                          const float vecin[3])
{
  BKE_curvemapping_evaluate_premulRGBF_ex(cumap, vecout, vecin, cumap->black, cumap->bwmul);
}

void BKE_curvemapping_evaluate_premulRGB(const CurveMapping *cumap,
                                         uchar vecout_byte[3],
                                         const uchar vecin_byte[3])
{
  float vecin[3], vecout[3];

  vecin[0] = float(vecin_byte[0]) / 255.0f;
  vecin[1] = float(vecin_byte[1]) / 255.0f;
  vecin[2] = float(vecin_byte[2]) / 255.0f;

  BKE_curvemapping_evaluate_premulRGBF(cumap, vecout, vecin);

  vecout_byte[0] = unit_float_to_uchar_clamp(vecout[0]);
  vecout_byte[1] = unit_float_to_uchar_clamp(vecout[1]);
  vecout_byte[2] = unit_float_to_uchar_clamp(vecout[2]);
}

bool BKE_curvemapping_RGBA_does_something(const CurveMapping *cumap)
{
  if (cumap->black[0] != 0.0f) {
    return true;
  }
  if (cumap->black[1] != 0.0f) {
    return true;
  }
  if (cumap->black[2] != 0.0f) {
    return true;
  }
  if (cumap->white[0] != 1.0f) {
    return true;
  }
  if (cumap->white[1] != 1.0f) {
    return true;
  }
  if (cumap->white[2] != 1.0f) {
    return true;
  }

  for (int a = 0; a < CM_TOT; a++) {
    if (cumap->cm[a].curve) {
      if (cumap->cm[a].totpoint != 2) {
        return true;
      }

      if (cumap->cm[a].curve[0].x != 0.0f) {
        return true;
      }
      if (cumap->cm[a].curve[0].y != 0.0f) {
        return true;
      }
      if (cumap->cm[a].curve[1].x != 1.0f) {
        return true;
      }
      if (cumap->cm[a].curve[1].y != 1.0f) {
        return true;
      }
    }
  }
  return false;
}

void BKE_curvemapping_get_range_minimums(const CurveMapping *curve_mapping, float minimums[CM_TOT])
{
  for (int i = 0; i < CM_TOT; i++) {
    minimums[i] = curve_mapping->cm[i].mintable;
  }
}

void BKE_curvemapping_compute_range_dividers(const CurveMapping *curve_mapping,
                                             float dividers[CM_TOT])
{
  for (int i = 0; i < CM_TOT; i++) {
    const CurveMap *curve_map = &curve_mapping->cm[i];
    dividers[i] = 1.0f / max_ff(1e-8f, curve_map->maxtable - curve_map->mintable);
  }
}

void BKE_curvemapping_compute_slopes(const CurveMapping *curve_mapping,
                                     float start_slopes[CM_TOT],
                                     float end_slopes[CM_TOT])
{
  float range_dividers[CM_TOT];
  BKE_curvemapping_compute_range_dividers(curve_mapping, range_dividers);
  for (int i = 0; i < CM_TOT; i++) {
    const CurveMap *curve_map = &curve_mapping->cm[i];
    /* If extrapolation is not enabled, the slopes are horizontal. */
    if (!(curve_mapping->flag & CUMA_EXTEND_EXTRAPOLATE)) {
      start_slopes[i] = 0.0f;
      end_slopes[i] = 0.0f;
      continue;
    }

    if (curve_map->ext_in[0] != 0.0f) {
      start_slopes[i] = curve_map->ext_in[1] / (curve_map->ext_in[0] * range_dividers[i]);
    }
    else {
      start_slopes[i] = 1e8f;
    }

    if (curve_map->ext_out[0] != 0.0f) {
      end_slopes[i] = curve_map->ext_out[1] / (curve_map->ext_out[0] * range_dividers[i]);
    }
    else {
      end_slopes[i] = 1e8f;
    }
  }
}

bool BKE_curvemapping_is_map_identity(const CurveMapping *curve_mapping, int index)
{
  if (!(curve_mapping->flag & CUMA_EXTEND_EXTRAPOLATE)) {
    return false;
  }
  const CurveMap *curve_map = &curve_mapping->cm[index];
  if (curve_map->maxtable - curve_map->mintable != 1.0f) {
    return false;
  }
  if (curve_map->ext_in[0] != curve_map->ext_in[1]) {
    return false;
  }
  if (curve_map->ext_out[0] != curve_map->ext_out[1]) {
    return false;
  }
  if (curve_map->totpoint != 2) {
    return false;
  }
  if (curve_map->curve[0].x != 0 || curve_map->curve[0].y != 0) {
    return false;
  }
  if (curve_map->curve[1].x != 0 || curve_map->curve[1].y != 0) {
    return false;
  }
  return true;
}

void BKE_curvemapping_init(CurveMapping *cumap)
{
  int a;

  if (cumap == nullptr) {
    return;
  }

  for (a = 0; a < CM_TOT; a++) {
    if (cumap->cm[a].table == nullptr) {
      curvemap_make_table(cumap, cumap->cm + a);
    }
  }
}

void BKE_curvemapping_table_F(const CurveMapping *cumap, float **array, int *size)
{
  int a;

  *size = CM_TABLE + 1;
  *array = MEM_calloc_arrayN<float>(4 * size_t(*size), "CurveMapping");

  for (a = 0; a < *size; a++) {
    if (cumap->cm[0].table) {
      (*array)[a * 4 + 0] = cumap->cm[0].table[a].y;
    }
  }
}

void BKE_curvemapping_table_RGBA(const CurveMapping *cumap, float **array, int *size)
{
  int a;

  *size = CM_TABLE + 1;
  *array = MEM_calloc_arrayN<float>(4 * size_t(*size), "CurveMapping");

  for (a = 0; a < *size; a++) {
    if (cumap->cm[0].table) {
      (*array)[a * 4 + 0] = cumap->cm[0].table[a].y;
    }
    if (cumap->cm[1].table) {
      (*array)[a * 4 + 1] = cumap->cm[1].table[a].y;
    }
    if (cumap->cm[2].table) {
      (*array)[a * 4 + 2] = cumap->cm[2].table[a].y;
    }
    if (cumap->cm[3].table) {
      (*array)[a * 4 + 3] = cumap->cm[3].table[a].y;
    }
  }
}

void BKE_curvemapping_blend_write(BlendWriter *writer, const CurveMapping *cumap)
{
  BLO_write_struct(writer, CurveMapping, cumap);
  BKE_curvemapping_curves_blend_write(writer, cumap);
}

void BKE_curvemapping_curves_blend_write(BlendWriter *writer, const CurveMapping *cumap)
{
  for (int a = 0; a < CM_TOT; a++) {
    BLO_write_struct_array(writer, CurveMapPoint, cumap->cm[a].totpoint, cumap->cm[a].curve);
  }
}

void BKE_curvemapping_blend_read(BlendDataReader *reader, CurveMapping *cumap)
{
  /* flag seems to be able to hang? Maybe old files... not bad to clear anyway */
  cumap->flag &= ~CUMA_PREMULLED;

  for (int a = 0; a < CM_TOT; a++) {
    BLO_read_struct_array(reader, CurveMapPoint, cumap->cm[a].totpoint, &cumap->cm[a].curve);
    cumap->cm[a].table = nullptr;
    cumap->cm[a].premultable = nullptr;
  }
}

/* ***************** Histogram **************** */

#define INV_255 (1.0f / 255.0f)

BLI_INLINE int get_bin_float(float f)
{
  int bin = int((f * 255.0f) + 0.5f); /* 0.5 to prevent quantization differences */

  /* NOTE: clamp integer instead of float to avoid problems with NaN. */
  CLAMP(bin, 0, 255);

  return bin;
}

static void save_sample_line(
    Scopes *scopes, const int idx, const float fx, const float rgb[3], const float ycc[3])
{
  float yuv[3];

  /* Vector-scope. */
  rgb_to_yuv(rgb[0], rgb[1], rgb[2], &yuv[0], &yuv[1], &yuv[2], BLI_YUV_ITU_BT709);
  scopes->vecscope[idx + 0] = yuv[1] * SCOPES_VEC_U_SCALE;
  scopes->vecscope[idx + 1] = yuv[2] * SCOPES_VEC_V_SCALE;

  int color_idx = (idx / 2) * 3;
  scopes->vecscope_rgb[color_idx + 0] = rgb[0];
  scopes->vecscope_rgb[color_idx + 1] = rgb[1];
  scopes->vecscope_rgb[color_idx + 2] = rgb[2];

  /* Waveform. */
  switch (scopes->wavefrm_mode) {
    case SCOPES_WAVEFRM_RGB:
    case SCOPES_WAVEFRM_RGB_PARADE:
      scopes->waveform_1[idx + 0] = fx;
      scopes->waveform_1[idx + 1] = rgb[0];
      scopes->waveform_2[idx + 0] = fx;
      scopes->waveform_2[idx + 1] = rgb[1];
      scopes->waveform_3[idx + 0] = fx;
      scopes->waveform_3[idx + 1] = rgb[2];
      break;
    case SCOPES_WAVEFRM_LUMA:
      scopes->waveform_1[idx + 0] = fx;
      scopes->waveform_1[idx + 1] = ycc[0];
      break;
    case SCOPES_WAVEFRM_YCC_JPEG:
    case SCOPES_WAVEFRM_YCC_709:
    case SCOPES_WAVEFRM_YCC_601:
      scopes->waveform_1[idx + 0] = fx;
      scopes->waveform_1[idx + 1] = ycc[0];
      scopes->waveform_2[idx + 0] = fx;
      scopes->waveform_2[idx + 1] = ycc[1];
      scopes->waveform_3[idx + 0] = fx;
      scopes->waveform_3[idx + 1] = ycc[2];
      break;
  }
}

void BKE_histogram_update_sample_line(Histogram *hist,
                                      ImBuf *ibuf,
                                      const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings)
{
  int i, x, y;
  const float *fp;
  uchar *cp;

  int x1 = roundf(hist->co[0][0] * ibuf->x);
  int x2 = roundf(hist->co[1][0] * ibuf->x);
  int y1 = roundf(hist->co[0][1] * ibuf->y);
  int y2 = roundf(hist->co[1][1] * ibuf->y);

  ColormanageProcessor *cm_processor = nullptr;

  hist->channels = 3;
  hist->x_resolution = 256;
  hist->xmax = 1.0f;
  // hist->ymax = 1.0f; /* now do this on the operator _only_ */

  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return;
  }

  if (ibuf->float_buffer.data) {
    cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);
  }

  for (i = 0; i < 256; i++) {
    x = int(0.5f + x1 + float(i) * (x2 - x1) / 255.0f);
    y = int(0.5f + y1 + float(i) * (y2 - y1) / 255.0f);

    if (x < 0 || y < 0 || x >= ibuf->x || y >= ibuf->y) {
      hist->data_luma[i] = hist->data_r[i] = hist->data_g[i] = hist->data_b[i] = hist->data_a[i] =
          0.0f;
    }
    else {
      if (ibuf->float_buffer.data) {
        float rgba[4];
        fp = (ibuf->float_buffer.data + (ibuf->channels) * (y * ibuf->x + x));

        switch (ibuf->channels) {
          case 4:
            copy_v4_v4(rgba, fp);
            IMB_colormanagement_processor_apply_v4(cm_processor, rgba);
            break;
          case 3:
            copy_v3_v3(rgba, fp);
            IMB_colormanagement_processor_apply_v3(cm_processor, rgba);
            rgba[3] = 1.0f;
            break;
          case 2:
            copy_v3_fl(rgba, fp[0]);
            rgba[3] = fp[1];
            break;
          case 1:
            copy_v3_fl(rgba, fp[0]);
            rgba[3] = 1.0f;
            break;
          default:
            BLI_assert_unreachable();
        }

        hist->data_luma[i] = IMB_colormanagement_get_luminance(rgba);
        hist->data_r[i] = rgba[0];
        hist->data_g[i] = rgba[1];
        hist->data_b[i] = rgba[2];
        hist->data_a[i] = rgba[3];
      }
      else if (ibuf->byte_buffer.data) {
        cp = ibuf->byte_buffer.data + 4 * (y * ibuf->x + x);
        hist->data_luma[i] = float(IMB_colormanagement_get_luminance_byte(cp)) / 255.0f;
        hist->data_r[i] = float(cp[0]) / 255.0f;
        hist->data_g[i] = float(cp[1]) / 255.0f;
        hist->data_b[i] = float(cp[2]) / 255.0f;
        hist->data_a[i] = float(cp[3]) / 255.0f;
      }
    }
  }

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
}

/* if view_settings, it also applies this to byte buffers */
struct ScopesUpdateData {
  Scopes *scopes;
  const ImBuf *ibuf;
  ColormanageProcessor *cm_processor;
  const uchar *display_buffer;
  int ycc_mode;
};

struct ScopesUpdateDataChunk {
  uint bin_lum[256];
  uint bin_r[256];
  uint bin_g[256];
  uint bin_b[256];
  uint bin_a[256];
  float min[3], max[3];
};

static void scopes_update_cb(void *__restrict userdata,
                             const int y,
                             const TaskParallelTLS *__restrict tls)
{
  const ScopesUpdateData *data = static_cast<const ScopesUpdateData *>(userdata);

  Scopes *scopes = data->scopes;
  const ImBuf *ibuf = data->ibuf;
  ColormanageProcessor *cm_processor = data->cm_processor;
  const uchar *display_buffer = data->display_buffer;
  const int ycc_mode = data->ycc_mode;

  ScopesUpdateDataChunk *data_chunk = static_cast<ScopesUpdateDataChunk *>(tls->userdata_chunk);
  uint *bin_lum = data_chunk->bin_lum;
  uint *bin_r = data_chunk->bin_r;
  uint *bin_g = data_chunk->bin_g;
  uint *bin_b = data_chunk->bin_b;
  uint *bin_a = data_chunk->bin_a;
  float *min = data_chunk->min;
  float *max = data_chunk->max;

  const float *rf = nullptr;
  const uchar *rc = nullptr;
  const int rows_per_sample_line = ibuf->y / scopes->sample_lines;
  const int savedlines = y / rows_per_sample_line;
  const bool do_sample_line = (savedlines < scopes->sample_lines) &&
                              (y % rows_per_sample_line) == 0;
  const bool is_float = (ibuf->float_buffer.data != nullptr);

  if (is_float) {
    rf = ibuf->float_buffer.data + size_t(y) * ibuf->x * ibuf->channels;
  }
  else {
    rc = display_buffer + size_t(y) * ibuf->x * ibuf->channels;
  }

  for (int x = 0; x < ibuf->x; x++) {
    float rgba[4], ycc[3], luma;

    if (is_float) {
      switch (ibuf->channels) {
        case 4:
          copy_v4_v4(rgba, rf);
          IMB_colormanagement_processor_apply_v4(cm_processor, rgba);
          break;
        case 3:
          copy_v3_v3(rgba, rf);
          IMB_colormanagement_processor_apply_v3(cm_processor, rgba);
          rgba[3] = 1.0f;
          break;
        case 2:
          copy_v3_fl(rgba, rf[0]);
          rgba[3] = rf[1];
          break;
        case 1:
          copy_v3_fl(rgba, rf[0]);
          rgba[3] = 1.0f;
          break;
        default:
          BLI_assert_unreachable();
      }
    }
    else {
      for (int c = 4; c--;) {
        rgba[c] = rc[c] * INV_255;
      }
    }

    /* we still need luma for histogram */
    luma = IMB_colormanagement_get_luminance(rgba);

    /* check for min max */
    if (ycc_mode == -1) {
      minmax_v3v3_v3(min, max, rgba);
    }
    else {
      rgb_to_ycc(rgba[0], rgba[1], rgba[2], &ycc[0], &ycc[1], &ycc[2], ycc_mode);
      mul_v3_fl(ycc, INV_255);
      minmax_v3v3_v3(min, max, ycc);
    }
    /* Increment count for histogram. */
    bin_lum[get_bin_float(luma)]++;
    bin_r[get_bin_float(rgba[0])]++;
    bin_g[get_bin_float(rgba[1])]++;
    bin_b[get_bin_float(rgba[2])]++;
    bin_a[get_bin_float(rgba[3])]++;

    /* save sample if needed */
    if (do_sample_line) {
      const float fx = float(x) / float(ibuf->x);
      const int idx = 2 * (ibuf->x * savedlines + x);
      save_sample_line(scopes, idx, fx, rgba, ycc);
    }

    rf += ibuf->channels;
    rc += ibuf->channels;
  }
}

static void scopes_update_reduce(const void *__restrict /*userdata*/,
                                 void *__restrict chunk_join,
                                 void *__restrict chunk)
{
  ScopesUpdateDataChunk *join_chunk = static_cast<ScopesUpdateDataChunk *>(chunk_join);
  const ScopesUpdateDataChunk *data_chunk = static_cast<const ScopesUpdateDataChunk *>(chunk);

  uint *bin_lum = join_chunk->bin_lum;
  uint *bin_r = join_chunk->bin_r;
  uint *bin_g = join_chunk->bin_g;
  uint *bin_b = join_chunk->bin_b;
  uint *bin_a = join_chunk->bin_a;
  const uint *bin_lum_c = data_chunk->bin_lum;
  const uint *bin_r_c = data_chunk->bin_r;
  const uint *bin_g_c = data_chunk->bin_g;
  const uint *bin_b_c = data_chunk->bin_b;
  const uint *bin_a_c = data_chunk->bin_a;

  const float *min = data_chunk->min;
  const float *max = data_chunk->max;

  for (int b = 256; b--;) {
    bin_lum[b] += bin_lum_c[b];
    bin_r[b] += bin_r_c[b];
    bin_g[b] += bin_g_c[b];
    bin_b[b] += bin_b_c[b];
    bin_a[b] += bin_a_c[b];
  }

  for (int c = 3; c--;) {
    join_chunk->min[c] = std::min(min[c], join_chunk->min[c]);
    join_chunk->max[c] = std::max(max[c], join_chunk->max[c]);
  }
}

void BKE_scopes_update(Scopes *scopes,
                       ImBuf *ibuf,
                       const ColorManagedViewSettings *view_settings,
                       const ColorManagedDisplaySettings *display_settings)
{
  int a;
  uint nl, na, nr, ng, nb;
  double divl, diva, divr, divg, divb;
  const uchar *display_buffer = nullptr;
  int ycc_mode = -1;
  void *cache_handle = nullptr;
  ColormanageProcessor *cm_processor = nullptr;

  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return;
  }

  if (scopes->ok == 1) {
    return;
  }

  if (scopes->hist.ymax == 0.0f) {
    scopes->hist.ymax = 1.0f;
  }

  /* hmmmm */
  if (!ELEM(ibuf->channels, 3, 4)) {
    return;
  }

  scopes->hist.channels = 3;
  scopes->hist.x_resolution = 256;

  switch (scopes->wavefrm_mode) {
    case SCOPES_WAVEFRM_RGB:
    /* fall-through */
    case SCOPES_WAVEFRM_RGB_PARADE:
      ycc_mode = -1;
      break;
    case SCOPES_WAVEFRM_LUMA:
    case SCOPES_WAVEFRM_YCC_JPEG:
      ycc_mode = BLI_YCC_JFIF_0_255;
      break;
    case SCOPES_WAVEFRM_YCC_601:
      ycc_mode = BLI_YCC_ITU_BT601;
      break;
    case SCOPES_WAVEFRM_YCC_709:
      ycc_mode = BLI_YCC_ITU_BT709;
      break;
  }

  /* convert to number of lines with logarithmic scale */
  scopes->sample_lines = (scopes->accuracy * 0.01f) * (scopes->accuracy * 0.01f) * ibuf->y;
  CLAMP_MIN(scopes->sample_lines, 1);

  if (scopes->sample_full) {
    scopes->sample_lines = ibuf->y;
  }

  /* scan the image */
  for (a = 0; a < 3; a++) {
    scopes->minmax[a][0] = 25500.0f;
    scopes->minmax[a][1] = -25500.0f;
  }

  scopes->waveform_tot = ibuf->x * scopes->sample_lines;

  if (scopes->waveform_1) {
    MEM_freeN(scopes->waveform_1);
  }
  if (scopes->waveform_2) {
    MEM_freeN(scopes->waveform_2);
  }
  if (scopes->waveform_3) {
    MEM_freeN(scopes->waveform_3);
  }
  if (scopes->vecscope) {
    MEM_freeN(scopes->vecscope);
  }
  if (scopes->vecscope_rgb) {
    MEM_freeN(scopes->vecscope_rgb);
  }

  scopes->waveform_1 = MEM_calloc_arrayN<float>(2 * size_t(scopes->waveform_tot),
                                                "waveform point channel 1");
  scopes->waveform_2 = MEM_calloc_arrayN<float>(2 * size_t(scopes->waveform_tot),
                                                "waveform point channel 2");
  scopes->waveform_3 = MEM_calloc_arrayN<float>(2 * size_t(scopes->waveform_tot),
                                                "waveform point channel 3");
  scopes->vecscope = MEM_calloc_arrayN<float>(2 * size_t(scopes->waveform_tot),
                                              "vectorscope point channel");
  scopes->vecscope_rgb = MEM_calloc_arrayN<float>(3 * size_t(scopes->waveform_tot),
                                                  "vectorscope color channel");

  if (ibuf->float_buffer.data) {
    cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);
  }
  else {
    display_buffer = (const uchar *)IMB_display_buffer_acquire(
        ibuf, view_settings, display_settings, &cache_handle);
  }

  /* Keep number of threads in sync with the merge parts below. */
  ScopesUpdateData data{};
  data.scopes = scopes;
  data.ibuf = ibuf;
  data.cm_processor = cm_processor;
  data.display_buffer = display_buffer;
  data.ycc_mode = ycc_mode;

  ScopesUpdateDataChunk data_chunk = {{0}};
  INIT_MINMAX(data_chunk.min, data_chunk.max);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (ibuf->y > 256);
  settings.userdata_chunk = &data_chunk;
  settings.userdata_chunk_size = sizeof(data_chunk);
  settings.func_reduce = scopes_update_reduce;
  BLI_task_parallel_range(0, ibuf->y, &data, scopes_update_cb, &settings);

  /* convert hist data to float (proportional to max count) */
  nl = na = nr = nb = ng = 0;
  for (a = 0; a < 256; a++) {
    nl = std::max(data_chunk.bin_lum[a], nl);
    nr = std::max(data_chunk.bin_r[a], nr);
    ng = std::max(data_chunk.bin_g[a], ng);
    nb = std::max(data_chunk.bin_b[a], nb);
    na = std::max(data_chunk.bin_a[a], na);
  }
  divl = nl ? 1.0 / double(nl) : 1.0;
  diva = na ? 1.0 / double(na) : 1.0;
  divr = nr ? 1.0 / double(nr) : 1.0;
  divg = ng ? 1.0 / double(ng) : 1.0;
  divb = nb ? 1.0 / double(nb) : 1.0;

  for (a = 0; a < 256; a++) {
    scopes->hist.data_luma[a] = data_chunk.bin_lum[a] * divl;
    scopes->hist.data_r[a] = data_chunk.bin_r[a] * divr;
    scopes->hist.data_g[a] = data_chunk.bin_g[a] * divg;
    scopes->hist.data_b[a] = data_chunk.bin_b[a] * divb;
    scopes->hist.data_a[a] = data_chunk.bin_a[a] * diva;
  }

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
  if (cache_handle) {
    IMB_display_buffer_release(cache_handle);
  }

  scopes->ok = 1;
}

void BKE_scopes_free(Scopes *scopes)
{
  MEM_SAFE_FREE(scopes->waveform_1);
  MEM_SAFE_FREE(scopes->waveform_2);
  MEM_SAFE_FREE(scopes->waveform_3);
  MEM_SAFE_FREE(scopes->vecscope);
  MEM_SAFE_FREE(scopes->vecscope_rgb);
}

void BKE_scopes_new(Scopes *scopes)
{
  scopes->accuracy = 30.0;
  scopes->hist.mode = HISTO_MODE_RGB;
  scopes->wavefrm_alpha = 0.3;
  scopes->vecscope_alpha = 0.3;
  scopes->wavefrm_height = 100;
  scopes->vecscope_height = 100;
  scopes->hist.height = 100;
  scopes->ok = 0;
  scopes->waveform_1 = nullptr;
  scopes->waveform_2 = nullptr;
  scopes->waveform_3 = nullptr;
  scopes->vecscope = nullptr;
  scopes->vecscope_rgb = nullptr;
}

void BKE_color_managed_display_settings_init(ColorManagedDisplaySettings *settings)
{
  const char *display_name = IMB_colormanagement_display_get_default_name();

  STRNCPY_UTF8(settings->display_device, display_name);
  settings->emulation = COLORMANAGE_DISPLAY_EMULATION_AUTO;
}

void BKE_color_managed_display_settings_copy(ColorManagedDisplaySettings *new_settings,
                                             const ColorManagedDisplaySettings *settings)
{
  STRNCPY_UTF8(new_settings->display_device, settings->display_device);
  new_settings->emulation = settings->emulation;
}

void BKE_color_managed_view_settings_init(ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings,
                                          const char *view_transform)
{
  const ColorManagedDisplay *display = IMB_colormanagement_display_get_named(
      display_settings->display_device);
  BLI_assert(display);

  if (!view_transform) {
    view_transform = IMB_colormanagement_display_get_default_view_transform_name(display);
  }

  /* TODO(sergey): Find a way to make look query more reliable with non
   * default configuration. */
  STRNCPY_UTF8(view_settings->view_transform, view_transform);
  STRNCPY_UTF8(view_settings->look, "None");

  view_settings->flag = 0;
  view_settings->gamma = 1.0f;
  view_settings->exposure = 0.0f;
  view_settings->curve_mapping = nullptr;

  IMB_colormanagement_validate_settings(display_settings, view_settings);
}

void BKE_color_managed_view_settings_copy(ColorManagedViewSettings *new_settings,
                                          const ColorManagedViewSettings *settings)
{
  BKE_color_managed_view_settings_copy_keep_curve_mapping(new_settings, settings);

  if (settings->curve_mapping) {
    new_settings->curve_mapping = BKE_curvemapping_copy(settings->curve_mapping);
  }
  else {
    new_settings->curve_mapping = nullptr;
  }
}

void BKE_color_managed_view_settings_copy_keep_curve_mapping(
    ColorManagedViewSettings *new_settings, const ColorManagedViewSettings *settings)
{
  STRNCPY_UTF8(new_settings->look, settings->look);
  STRNCPY_UTF8(new_settings->view_transform, settings->view_transform);

  new_settings->flag = settings->flag;
  new_settings->exposure = settings->exposure;
  new_settings->gamma = settings->gamma;
  new_settings->temperature = settings->temperature;
  new_settings->tint = settings->tint;
}

void BKE_color_managed_view_settings_free(ColorManagedViewSettings *settings)
{
  if (settings->curve_mapping) {
    BKE_curvemapping_free(settings->curve_mapping);
    settings->curve_mapping = nullptr;
  }
}

void BKE_color_managed_view_settings_blend_write(BlendWriter *writer,
                                                 const ColorManagedViewSettings *settings)
{
  if (settings->curve_mapping) {
    BKE_curvemapping_blend_write(writer, settings->curve_mapping);
  }
}

void BKE_color_managed_view_settings_blend_read_data(BlendDataReader *reader,
                                                     ColorManagedViewSettings *settings)
{
  BLO_read_struct(reader, CurveMapping, &settings->curve_mapping);

  if (settings->curve_mapping) {
    BKE_curvemapping_blend_read(reader, settings->curve_mapping);
  }
}

void BKE_color_managed_colorspace_settings_init(
    ColorManagedColorspaceSettings *colorspace_settings)
{
  STRNCPY_UTF8(colorspace_settings->name, "");
}

void BKE_color_managed_colorspace_settings_copy(
    ColorManagedColorspaceSettings *colorspace_settings,
    const ColorManagedColorspaceSettings *settings)
{
  STRNCPY_UTF8(colorspace_settings->name, settings->name);
}

bool BKE_color_managed_colorspace_settings_equals(const ColorManagedColorspaceSettings *settings1,
                                                  const ColorManagedColorspaceSettings *settings2)
{
  return STREQ(settings1->name, settings2->name);
}
