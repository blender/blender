/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_span.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

using blender::float3;
using blender::Span;

void BKE_gpencil_stroke_2d_flat(const bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[int(totpoints * 0.75)];

  float locx[3];
  float locy[3];
  float loc3[3];
  float normal[3];

  /* local X axis (p0 -> p1) */
  sub_v3_v3v3(locx, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  float v3[3];
  if (totpoints == 2) {
    mul_v3_v3fl(v3, &pt3->x, 0.001f);
  }
  else {
    copy_v3_v3(v3, &pt3->x);
  }

  sub_v3_v3v3(loc3, v3, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(normal, locx, loc3);

  /* local Y axis (cross to normal/x axis) */
  cross_v3_v3v3(locy, normal, locx);

  /* Normalize vectors */
  normalize_v3(locx);
  normalize_v3(locy);

  /* Calculate last point first. */
  const bGPDspoint *pt_last = &points[totpoints - 1];
  float tmp[3];
  sub_v3_v3v3(tmp, &pt_last->x, &pt0->x);

  points2d[totpoints - 1][0] = dot_v3v3(tmp, locx);
  points2d[totpoints - 1][1] = dot_v3v3(tmp, locy);

  /* Calculate the scalar cross product of the 2d points. */
  float cross = 0.0f;
  float *co_curr;
  float *co_prev = (float *)&points2d[totpoints - 1];

  /* Get all points in local space */
  for (int i = 0; i < totpoints - 1; i++) {
    const bGPDspoint *pt = &points[i];
    float loc[3];

    /* Get local space using first point as origin */
    sub_v3_v3v3(loc, &pt->x, &pt0->x);

    points2d[i][0] = dot_v3v3(loc, locx);
    points2d[i][1] = dot_v3v3(loc, locy);

    /* Calculate cross product. */
    co_curr = (&points2d[i][0]);
    cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
    co_prev = (&points2d[i][0]);
  }

  /* Concave (-1), Convex (1) */
  *r_direction = (cross >= 0.0f) ? 1 : -1;
}

/* Calc texture coordinates using flat projected points. */
static void gpencil_calc_stroke_fill_uv(const float (*points2d)[2],
                                        bGPDstroke *gps,
                                        const float minv[2],
                                        const float maxv[2],
                                        float (*r_uv)[2])
{
  const float s = sin(gps->uv_rotation);
  const float c = cos(gps->uv_rotation);

  /* Calc center for rotation. */
  const float center[2] = {0.5f, 0.5f};
  float d[2];
  d[0] = maxv[0] - minv[0];
  d[1] = maxv[1] - minv[1];
  for (int i = 0; i < gps->totpoints; i++) {
    r_uv[i][0] = (points2d[i][0] - minv[0]) / d[0];
    r_uv[i][1] = (points2d[i][1] - minv[1]) / d[1];

    /* Apply translation. */
    add_v2_v2(r_uv[i], gps->uv_translation);

    /* Apply Rotation. */
    r_uv[i][0] -= center[0];
    r_uv[i][1] -= center[1];

    float x = r_uv[i][0] * c - r_uv[i][1] * s;
    float y = r_uv[i][0] * s + r_uv[i][1] * c;

    r_uv[i][0] = x + center[0];
    r_uv[i][1] = y + center[1];

    /* Apply scale. */
    if (gps->uv_scale != 0.0f) {
      mul_v2_fl(r_uv[i], 1.0f / gps->uv_scale);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Fill Triangulate
 * \{ */

void BKE_gpencil_stroke_fill_triangulate(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints >= 3);

  /* allocate memory for temporary areas */
  gps->tot_triangles = gps->totpoints - 2;
  uint(*tmp_triangles)[3] = MEM_malloc_arrayN<uint[3]>(size_t(gps->tot_triangles),
                                                       "GP Stroke temp triangulation");
  float (*points2d)[2] = MEM_malloc_arrayN<float[2]>(size_t(gps->totpoints),
                                                     "GP Stroke temp 2d points");
  float (*uv)[2] = MEM_malloc_arrayN<float[2]>(size_t(gps->totpoints),
                                               "GP Stroke temp 2d uv data");

  int direction = 0;

  /* convert to 2d and triangulate */
  BKE_gpencil_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
  BLI_polyfill_calc(points2d, uint(gps->totpoints), direction, tmp_triangles);

  /* calc texture coordinates automatically */
  float minv[2];
  float maxv[2];
  /* first needs bounding box data */
  ARRAY_SET_ITEMS(minv, -1.0f, -1.0f);
  ARRAY_SET_ITEMS(maxv, 1.0f, 1.0f);

  /* calc uv data */
  gpencil_calc_stroke_fill_uv(points2d, gps, minv, maxv, uv);

  /* Save triangulation data. */
  if (gps->tot_triangles > 0) {
    MEM_SAFE_FREE(gps->triangles);
    gps->triangles = MEM_calloc_arrayN<bGPDtriangle>(gps->tot_triangles,
                                                     "GP Stroke triangulation");

    for (int i = 0; i < gps->tot_triangles; i++) {
      memcpy(gps->triangles[i].verts, tmp_triangles[i], sizeof(uint[3]));
    }

    /* Copy UVs to bGPDspoint. */
    for (int i = 0; i < gps->totpoints; i++) {
      copy_v2_v2(gps->points[i].uv_fill, uv[i]);
    }
  }
  else {
    /* No triangles needed - Free anything allocated previously */
    if (gps->triangles) {
      MEM_freeN(gps->triangles);
    }

    gps->triangles = nullptr;
  }

  /* clear memory */
  MEM_SAFE_FREE(tmp_triangles);
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(uv);
}

void BKE_gpencil_stroke_uv_update(bGPDstroke *gps)
{
  if (gps == nullptr || gps->totpoints == 0) {
    return;
  }

  bGPDspoint *pt = gps->points;
  float totlen = 0.0f;
  pt[0].uv_fac = totlen;
  for (int i = 1; i < gps->totpoints; i++) {
    totlen += len_v3v3(&pt[i - 1].x, &pt[i].x);
    pt[i].uv_fac = totlen;
  }
}

void BKE_gpencil_stroke_geometry_update(bGPdata * /*gpd*/, bGPDstroke *gps)
{
  if (gps == nullptr) {
    return;
  }

  if (gps->totpoints > 2) {
    BKE_gpencil_stroke_fill_triangulate(gps);
  }
  else {
    gps->tot_triangles = 0;
    MEM_SAFE_FREE(gps->triangles);
  }

  /* calc uv data along the stroke */
  BKE_gpencil_stroke_uv_update(gps);
}

/* Temp data for storing information about an "island" of points
 * that should be kept when splitting up a stroke. Used in:
 * gpencil_stroke_delete_tagged_points()
 */
struct tGPDeleteIsland {
  int start_idx;
  int end_idx;
};

static void gpencil_stroke_join_islands(bGPdata *gpd,
                                        bGPDframe *gpf,
                                        bGPDstroke *gps_first,
                                        bGPDstroke *gps_last)
{
  bGPDspoint *pt = nullptr;
  bGPDspoint *pt_final = nullptr;
  const int totpoints = gps_first->totpoints + gps_last->totpoints;

  /* create new stroke */
  bGPDstroke *join_stroke = BKE_gpencil_stroke_duplicate(gps_first, false, true);

  join_stroke->points = MEM_calloc_arrayN<bGPDspoint>(totpoints, __func__);
  join_stroke->totpoints = totpoints;
  join_stroke->flag &= ~GP_STROKE_CYCLIC;

  /* copy points (last before) */
  int e1 = 0;
  int e2 = 0;
  float delta = 0.0f;

  for (int i = 0; i < totpoints; i++) {
    pt_final = &join_stroke->points[i];
    if (i < gps_last->totpoints) {
      pt = &gps_last->points[e1];
      e1++;
    }
    else {
      pt = &gps_first->points[e2];
      e2++;
    }

    /* copy current point */
    copy_v3_v3(&pt_final->x, &pt->x);
    pt_final->pressure = pt->pressure;
    pt_final->strength = pt->strength;
    pt_final->time = delta;
    pt_final->flag = pt->flag;
    copy_v4_v4(pt_final->vert_color, pt->vert_color);

    /* retiming with fixed time interval (we cannot determine real time) */
    delta += 0.01f;
  }

  /* Copy over vertex weight data (if available) */
  if ((gps_first->dvert != nullptr) || (gps_last->dvert != nullptr)) {
    join_stroke->dvert = MEM_calloc_arrayN<MDeformVert>(totpoints, __func__);
    MDeformVert *dvert_src = nullptr;
    MDeformVert *dvert_dst = nullptr;

    /* Copy weights (last before). */
    e1 = 0;
    e2 = 0;
    for (int i = 0; i < totpoints; i++) {
      dvert_dst = &join_stroke->dvert[i];
      dvert_src = nullptr;
      if (i < gps_last->totpoints) {
        if (gps_last->dvert) {
          dvert_src = &gps_last->dvert[e1];
          e1++;
        }
      }
      else {
        if (gps_first->dvert) {
          dvert_src = &gps_first->dvert[e2];
          e2++;
        }
      }

      if ((dvert_src) && (dvert_src->dw)) {
        dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
      }
    }
  }

  /* add new stroke at head */
  BLI_addhead(&gpf->strokes, join_stroke);
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, join_stroke);

  /* remove first stroke */
  BLI_remlink(&gpf->strokes, gps_first);
  BKE_gpencil_free_stroke(gps_first);

  /* remove last stroke */
  BLI_remlink(&gpf->strokes, gps_last);
  BKE_gpencil_free_stroke(gps_last);
}

bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd,
                                                    bGPDframe *gpf,
                                                    bGPDstroke *gps,
                                                    bGPDstroke *next_stroke,
                                                    int tag_flags,
                                                    const bool select,
                                                    const bool flat_cap,
                                                    const int limit)
{
  /* The algorithm used here is as follows:
   * 1) We firstly identify the number of "islands" of non-tagged points
   *    which will all end up being in new strokes.
   *    - In the most extreme case (i.e. every other vert is a 1-vert island),
   *      we have at most `n / 2` islands
   *    - Once we start having larger islands than that, the number required
   *      becomes much less
   * 2) Each island gets converted to a new stroke
   * If the number of points is <= limit, the stroke is deleted. */

  tGPDeleteIsland *islands = MEM_calloc_arrayN<tGPDeleteIsland>((gps->totpoints + 1) / 2,
                                                                "gp_point_islands");
  bool in_island = false;
  int num_islands = 0;

  bGPDstroke *new_stroke = nullptr;
  bGPDstroke *gps_first = nullptr;
  const bool is_cyclic = bool(gps->flag & GP_STROKE_CYCLIC);

  /* First Pass: Identify start/end of islands */
  bGPDspoint *pt = gps->points;
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    if (pt->flag & tag_flags) {
      /* selected - stop accumulating to island */
      in_island = false;
    }
    else {
      /* unselected - start of a new island? */
      int idx;

      if (in_island) {
        /* extend existing island */
        idx = num_islands - 1;
        islands[idx].end_idx = i;
      }
      else {
        /* start of new island */
        in_island = true;
        num_islands++;

        idx = num_islands - 1;
        islands[idx].start_idx = islands[idx].end_idx = i;
      }
    }
  }

  /* Watch out for special case where No islands = All points selected = Delete Stroke only */
  if (num_islands) {
    /* There are islands, so create a series of new strokes,
     * adding them before the "next" stroke. */
    int idx;

    /* Create each new stroke... */
    for (idx = 0; idx < num_islands; idx++) {
      tGPDeleteIsland *island = &islands[idx];
      new_stroke = BKE_gpencil_stroke_duplicate(gps, false, true);
      if (flat_cap) {
        new_stroke->caps[1 - (idx % 2)] = GP_STROKE_CAP_FLAT;
      }

      /* if cyclic and first stroke, save to join later */
      if ((is_cyclic) && (gps_first == nullptr)) {
        gps_first = new_stroke;
      }

      new_stroke->flag &= ~GP_STROKE_CYCLIC;

      /* Compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
      new_stroke->totpoints = island->end_idx - island->start_idx + 1;

      /* Copy over the relevant point data */
      new_stroke->points = MEM_calloc_arrayN<bGPDspoint>(new_stroke->totpoints,
                                                         "gp delete stroke fragment");
      memcpy(static_cast<void *>(new_stroke->points),
             gps->points + island->start_idx,
             sizeof(bGPDspoint) * new_stroke->totpoints);

      /* Copy over vertex weight data (if available) */
      if (gps->dvert != nullptr) {
        /* Copy over the relevant vertex-weight points */
        new_stroke->dvert = MEM_calloc_arrayN<MDeformVert>(new_stroke->totpoints,
                                                           "gp delete stroke fragment weight");
        memcpy(new_stroke->dvert,
               gps->dvert + island->start_idx,
               sizeof(MDeformVert) * new_stroke->totpoints);

        /* Copy weights */
        int e = island->start_idx;
        for (int i = 0; i < new_stroke->totpoints; i++) {
          MDeformVert *dvert_src = &gps->dvert[e];
          MDeformVert *dvert_dst = &new_stroke->dvert[i];
          if (dvert_src->dw) {
            dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
          }
          e++;
        }
      }
      /* Each island corresponds to a new stroke.
       * We must adjust the timings of these new strokes:
       *
       * Each point's timing data is a delta from stroke's inittime, so as we erase some points
       * from the start of the stroke, we have to offset this inittime and all remaining points'
       * delta values. This way we get a new stroke with exactly the same timing as if user had
       * started drawing from the first non-removed point.
       */
      {
        bGPDspoint *pts;
        float delta = gps->points[island->start_idx].time;
        int j;

        new_stroke->inittime += double(delta);

        pts = new_stroke->points;
        for (j = 0; j < new_stroke->totpoints; j++, pts++) {
          /* Some points have time = 0, so check to not get negative time values. */
          pts->time = max_ff(pts->time - delta, 0.0f);
          /* set flag for select again later */
          if (select == true) {
            pts->flag &= ~GP_SPOINT_SELECT;
            pts->flag |= GP_SPOINT_TAG;
          }
        }
      }

      /* Add new stroke to the frame or delete if below limit */
      if ((limit > 0) && (new_stroke->totpoints <= limit)) {
        if (gps_first == new_stroke) {
          gps_first = nullptr;
        }
        BKE_gpencil_free_stroke(new_stroke);
      }
      else {
        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

        if (next_stroke) {
          BLI_insertlinkbefore(&gpf->strokes, next_stroke, new_stroke);
        }
        else {
          BLI_addtail(&gpf->strokes, new_stroke);
        }
      }
    }
    /* if cyclic, need to join last stroke with first stroke */
    if ((is_cyclic) && (gps_first != nullptr) && (gps_first != new_stroke)) {
      gpencil_stroke_join_islands(gpd, gpf, gps_first, new_stroke);
    }
  }

  /* free islands */
  MEM_freeN(islands);

  /* Delete the old stroke */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);

  return new_stroke;
}
