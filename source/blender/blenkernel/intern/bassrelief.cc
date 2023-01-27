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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_float4x4.hh"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_math_solvers.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_bassrelief.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h" /* for OMP limits. */
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"

#include <queue>

using namespace blender::threading;
using IndexRange = blender::IndexRange;

static void bassrelief_snap_point_to_surface(const struct BassReliefTreeData *tree,
                                             const struct SpaceTransform *transform,
                                             int hit_idx,
                                             const float hit_co[3],
                                             const float hit_no[3],
                                             float goal_dist,
                                             const float point_co[3],
                                             float r_point_co[3]);
/* for timing... */
#if 0
#  include "PIL_time_utildefines.h"
#else
#  define TIMEIT_BENCH(expr, id) (expr)
#endif

/* Util macros */
#define OUT_OF_MEMORY() ((void)printf("Shrinkwrap: Out of memory\n"))

/*
on factor;
off period;

load_package avector;

*/

enum { RV_HAS_HIT = 1 << 0, RV_VISIT = 1 << 1, RV_TAG = 1 << 2, RV_IS_BOUNDARY = 1 << 3 };

// struct ReliefEdge {
//} ReliefEdge;

namespace blender {
namespace bassrelief {

/* this could stand to be more C++-afied */
struct ReliefVertex {
  float co[3], origco[3];
  float no[3], origno[3];
  uint index;
  float targetco[3];
  float targetno[3];
  float ray[3], ray_dist, origray[3], origray_dist;
  float bound_dist, angle;
  ReliefVertex *bound_vert;
  int flag;
  struct ReliefVertex **neighbors;
  float *neighbor_restlens;
  int totneighbor, i;

  float tan[3], bin[3], disp[3], tnor[3];
  float mat[3][3];

  float smoothco[3], svel[3];

  float tan_field;  // scalar field for parameterization
  // tangents are redrived from this
};

struct ReliefOptimizer {
  ReliefVertex *verts;
  int totvert;
  float compress_ratio;
  struct MemArena *arena;
  float rmindis, rmaxdis, rdis_scale;
  float bmindis, bmaxdis, bdis_scale;
  float tmindis, tmaxdis, tdis_scale;
  MPropCol *debugColors[MAX_BASSRELIEF_DEBUG_COLORS];
  MLoopTri *mlooptri;
  int totlooptri;
  const MLoop *mloop;
  int boundSmoothSteps;
  float normalScale, boundWidth;

  ReliefOptimizer(const float (*cos)[3],
                  const float (*nos)[3],
                  int totvert_,
                  const MEdge *medge_,
                  int totedge_,
                  MPropCol *_debugColors[MAX_BASSRELIEF_DEBUG_COLORS],
                  const MLoopTri *_mlooptri,
                  int totlooptri_,
                  const MLoop *mloop_,
                  float optimizeNormalsScale,
                  float boundSmoothScale_,
                  int boundSmoothSteps_)
      : totvert(totvert_),
        totlooptri(totlooptri_),
        mloop(mloop_),
        boundSmoothSteps(boundSmoothSteps_),
        normalScale(optimizeNormalsScale),
        boundWidth(boundSmoothScale_)
  {
    rmindis = rmaxdis = bmindis = bmaxdis = 0.0f;
    rdis_scale = bdis_scale = 0.0f;
    compress_ratio = 1.0f;

    /* copy mlooptri in case it's later freed */

    mlooptri = new MLoopTri[(uint)totlooptri];
    for (int i = 0; i < totlooptri; i++) {
      mlooptri[i] = _mlooptri[i];
    }

    for (int i = 0; i < MAX_BASSRELIEF_DEBUG_COLORS; i++) {
      debugColors[i] = _debugColors[i];
    }

    arena = BLI_memarena_new(1024 * 32, "relief optimizer arena");
    verts = new ReliefVertex[(uint)totvert];
    compress_ratio = 0.5f;

    ReliefVertex *rv = verts;

    for (uint i = 0; i < (uint)totvert; i++, rv++) {
      memset(static_cast<void *>(rv), 0, sizeof(ReliefVertex));

      copy_v3_v3(rv->co, cos[i]);
      copy_v3_v3(rv->origco, cos[i]);
      zero_v3(rv->svel);
      zero_v3(rv->disp);
      zero_m3(rv->mat);
      rv->mat[0][0] = rv->mat[1][1] = rv->mat[2][2] = 1.0f;
      rv->tan_field = rv->bound_dist = rv->ray_dist = 0.0f;

      zero_v3(rv->tan);
      zero_v3(rv->bin);
      copy_v3_v3(rv->smoothco, rv->co);

      copy_v3_v3(rv->origno, nos[i]);

      rv->index = i;
      rv->bound_vert = NULL;
      rv->totneighbor = 0;
      rv->i = 0;
      rv->bound_dist = 0.0f;
      rv->flag = 0;
    }

    const MEdge *me = medge_;
    for (int i = 0; i < totedge_; i++, me++) {
      verts[me->v1].totneighbor++;
      verts[me->v2].totneighbor++;
    }

    rv = verts;
    for (int i = 0; i < totvert; i++, rv++) {
      rv->neighbors = reinterpret_cast<ReliefVertex **>(
          BLI_memarena_alloc(arena, sizeof(void *) * (size_t)rv->totneighbor * 2ULL));
      rv->neighbor_restlens = reinterpret_cast<float *>(
          BLI_memarena_alloc(arena, sizeof(float) * (size_t)rv->totneighbor * 2ULL));
    }

    me = medge_;
    for (int i = 0; i < totedge_; i++, me++) {
      for (int j = 0; j < 2; j++) {
        ReliefVertex *rv2 = j ? verts + me->v2 : verts + me->v1;
        ReliefVertex *rv_other = j ? verts + me->v1 : verts + me->v2;

        rv2->neighbors[rv2->i] = rv_other;
        rv2->neighbor_restlens[rv2->i] = len_v3v3(rv_other->co, rv2->co);
        rv2->i++;
      }
    }

    recalc_normals();

#if 0
    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(verts[i].origno, verts[i].no);
    }
#endif

#if 1
    /* sort vertex neighbors geometrically */
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv1 = verts + i;
      float start[3];

      if (!rv1->totneighbor) {
        continue;
      }

      copy_v3_v3(start, rv1->neighbors[0]->co);

      for (int j = 0; j < rv1->totneighbor; j++) {
        ReliefVertex *rv2 = rv1->neighbors[j];

        rv2->angle = angle_signed_on_axis_v3v3_v3(start, rv2->co, rv1->no);
      }

      /* insertion sort */
      for (int j = 0; j < rv1->totneighbor; j++) {
        ReliefVertex *rv2 = rv1->neighbors[j];

        int cur = 0;

        for (int k = 0; k < j; k++, cur++) {
          if (rv2->angle > rv1->neighbors[k]->angle) {
            break;
          }
        }

        for (int k = j; k > cur; k--) {
          rv1->neighbors[k] = rv1->neighbors[k - 1];
          rv1->neighbors[k]->i = k;
        }

        rv2->i = cur;
        rv1->neighbors[cur] = rv2;
      }
    }
#endif
  }

  ~ReliefOptimizer()
  {
    if (verts) {
      delete[] verts;
    }

    if (mlooptri) {
      delete[] mlooptri;
    }

    BLI_memarena_free(arena);
  }

  void smooth_rays()
  {
    blender::Array<blender::float3> rays(totvert);

    /* clang-format off */
    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        copy_v3_v3(rays[i], verts[i].ray);
      }
    });
    /* clang-format on */

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        float avg[3] = {0.0f, 0.0f, 0.0f};
        float dist = 0.0f;
        float tot = 0.0f;

        ReliefVertex *rv = verts + i;
        float factor = 0.75f;

        if (rv->flag & RV_HAS_HIT) {
          float w = 1.0f - (rv->bound_dist - bmindis) * bdis_scale;

          factor *= w * w * w * w;
        }

        for (int j = 0; j < rv->totneighbor; j++) {
          ReliefVertex *rv2 = rv->neighbors[j];

          add_v3_v3(avg, rays[rv2->index]);
          dist += rv2->ray_dist;
          tot += 1.0f;
        }

        if (tot > 0.0f) {
          dist /= tot;
          mul_v3_fl(avg, 1.0f / tot);

          interp_v3_v3v3(rv->ray, rv->ray, avg, factor);
          normalize_v3(rv->ray);

          rv->ray_dist += (dist - rv->ray_dist) * factor;
        }
      }
    });

    calc_raydist_bounds();
  }

  void set_boundary_flags()
  {
    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        ReliefVertex *rv = verts + i;

        rv->flag &= ~RV_IS_BOUNDARY;

        if (!(rv->flag & RV_HAS_HIT)) {
          continue;
        }

        for (int j = 0; j < rv->totneighbor; j++) {
          if (!(rv->neighbors[j]->flag & RV_HAS_HIT)) {
            rv->flag |= RV_IS_BOUNDARY;
            break;
          }
        }
      }
    });
  }

  /*build geodesic field from boundaries*/
  void solve_geodesic()
  {
    std::deque<ReliefVertex *> queue;

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!(rv->flag & RV_IS_BOUNDARY)) {
        rv->flag &= ~RV_VISIT;
        continue;
      }

      rv->flag |= RV_VISIT;
      rv->bound_vert = rv;
      queue.push_front(rv);
    }

    while (!queue.empty()) {
      ReliefVertex *rv = queue.back();
      queue.pop_back();

      for (int j = 0; j < rv->totneighbor; j++) {
        ReliefVertex *rv2 = rv->neighbors[j];
        ReliefVertex *rv3 = rv->neighbors[(j + 1) % rv->totneighbor];

        float dist = rv->bound_dist + len_v3v3(rv->co, rv2->co);
        if (rv3->flag & (RV_IS_BOUNDARY | RV_VISIT)) {
          dist = geodesic_distance_propagate_across_triangle(
              rv2->co, rv->co, rv3->co, rv->bound_dist, rv3->bound_dist);
        }

        if (!(rv2->flag & RV_VISIT)) {
          rv2->flag |= RV_VISIT;
          queue.push_front(rv2);

          rv2->bound_dist = dist;
          rv2->bound_vert = rv->bound_vert;
        }
        else {
          if (dist < rv2->bound_dist) {
            rv2->bound_dist = dist;
            rv2->bound_vert = rv->bound_vert;
          }
        }
      }
    }

    calc_bounddist_bounds();
  }

  void calc_bounddist_bounds()
  {
    bmindis = 1e17f;
    bmaxdis = -1e17f;

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        bmindis = min_ff(bmindis, verts[i].bound_dist);
        bmaxdis = max_ff(bmaxdis, verts[i].bound_dist);
      }
    });

    bdis_scale = (bmaxdis - bmindis);

    if (bdis_scale > 0.0f) {
      bdis_scale = 1.0f / bdis_scale;
    }
  }

  void calc_raydist_bounds()
  {
    rmindis = 1e17f;
    rmaxdis = -1e17f;

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        rmindis = min_ff(rmindis, verts[i].ray_dist);
        rmaxdis = max_ff(rmaxdis, verts[i].ray_dist);
      }
    });

    rdis_scale = (rmaxdis - rmindis);

    if (rdis_scale > 0.0f) {
      rdis_scale = 1.0f / rdis_scale;
    }
  }

  void calc_tanfield_bounds()
  {
    tmindis = 1e17f;
    tmaxdis = -1e17f;

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        if (!(verts[i].flag & RV_HAS_HIT)) {
          continue;
        }

        tmindis = min_ff(tmindis, verts[i].tan_field);
        tmaxdis = max_ff(tmaxdis, verts[i].tan_field);
      }
    });

    tdis_scale = (tmaxdis - tmindis);

    if (tdis_scale > 0.0f) {
      tdis_scale = 1.0f / tdis_scale;
    }
  }

  void smooth_cos(bool restricted, float fac = 0.75f, float projection = 0.5f)
  {
    blender::Array<blender::float3> cos(totvert);

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        copy_v3_v3(cos[i], verts[i].co);
      }
    });

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        ReliefVertex *rv = verts + i;
        float avg[3] = {0.0f, 0.0f, 0.0f}, tot = 0.0f;
        float tmp[3];

        add_v3_v3(rv->co, rv->svel);
        float factor = fac;

        if (restricted && (rv->flag & RV_HAS_HIT)) {
          float w = 1.0f - (rv->bound_dist - bmindis) * bdis_scale;
          w *= w * w * w * w;

          factor *= w;
          // continue;
        }

        for (int j = 0; j < rv->totneighbor; j++) {
          ReliefVertex *rv2 = rv->neighbors[j];

          sub_v3_v3v3(tmp, cos[rv2->index], cos[rv->index]);
          float fac2 = dot_v3v3(tmp, rv->no);

          if (0) {  // bound && !bound2) {
            madd_v3_v3fl(avg, rv->no, fac2);
          }
          else {
            madd_v3_v3fl(tmp, rv->no, -fac2 * projection);
            add_v3_v3(avg, tmp);
          }

          tot += 1.0f;
        }

        if (tot < 2.0f) {
          continue;
        }

        mul_v3_fl(avg, factor / tot);
        add_v3_v3(rv->co, avg);
        add_v3_v3(rv->svel, avg);
        mul_v3_fl(rv->svel, 0.0f);
        // interp_v3_v3v3(rv->co, rv->co, avg, fac);
      }
    });
  }

  void recalc_normals()
  {
    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        zero_v3(verts[i].no);
      }
    });

    const MLoopTri *lt = mlooptri;

    for (int i = 0; i < totlooptri; i++, lt++) {
      ReliefVertex *vtri[3] = {
          verts + mloop[lt->tri[0]].v, verts + mloop[lt->tri[1]].v, verts + mloop[lt->tri[2]].v};

      float n[3];
      normal_tri_v3(n, vtri[0]->co, vtri[1]->co, vtri[2]->co);

      add_v3_v3(vtri[0]->no, n);
      add_v3_v3(vtri[1]->no, n);
      add_v3_v3(vtri[2]->no, n);
    }

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        normalize_v3(verts[i].no);

        /* ensure non-zero normals even for isolated verts */
        if (dot_v3v3(verts[i].no, verts[i].no) == 0.0f) {
          verts[i].no[2] = 1.0f;
        }
      }
    });
  }

  void smooth_geodesic()
  {
    std::vector<float> dists((size_t)totvert);

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        float f = verts[i].bound_dist;
        // f = (f - bmindis) * bdis_scale;

        dists[(size_t)i] = f;
      }
    });

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        ReliefVertex *rv = verts + i;

        if (rv->flag & RV_IS_BOUNDARY) {
          continue;
        }

        float dist = 0.0f;
        float tot = 0.0f;

        for (int j = 0; j < rv->totneighbor; j++) {
          dist += dists[rv->neighbors[j]->index];
          tot += 1.0f;
        }

        if (tot > 0.0f) {
          dist /= tot;

          rv->bound_dist += (dist - rv->bound_dist) * 0.75f;
        }
      }
    });

    calc_bounddist_bounds();
  }

  void smooth_tangent_field()
  {
    std::vector<float> dists((size_t)totvert);

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        dists[(size_t)i] = verts[i].tan_field;
      }
    });

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        ReliefVertex *rv = verts + i;

        if (rv->flag & RV_IS_BOUNDARY) {
          // continue;
        }

        float dist = 0.0f;
        float tot = 0.0f;

        for (int j = 0; j < rv->totneighbor; j++) {
          dist += dists[rv->neighbors[j]->index];
          tot += 1.0f;
        }

        if (tot > 0.0f) {
          dist /= tot;

          rv->tan_field += (dist - rv->tan_field) * 0.75f;
        }
      }
    });

    calc_bounddist_bounds();
  }

  void calc_tangent_scalar_field()
  {
    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        verts[i].flag &= ~RV_VISIT;
        verts[i].tan_field = 0.0f;
      }
    });

    for (int i = 0; i < totvert; i++) {
      if (verts[i].flag & RV_VISIT) {
        continue;
      }

      std::deque<ReliefVertex *> queue;
      queue.push_front(verts + i);
      verts[i].flag |= RV_VISIT;

      while (!queue.empty()) {
        ReliefVertex *rv = queue.back();
        queue.pop_back();

        for (int j = 0; j < rv->totneighbor; j++) {
          ReliefVertex *rv2 = rv->neighbors[j];
          ReliefVertex *rv3 = rv->neighbors[(j + 1) % rv->totneighbor];

          float dist = rv->tan_field + len_v3v3(rv->co, rv2->co);
          if (rv3->flag & RV_VISIT) {
            dist = geodesic_distance_propagate_across_triangle(
                rv2->co, rv->co, rv3->co, rv->tan_field, rv3->tan_field);
          }

          if (!(rv2->flag & RV_VISIT)) {
            rv2->tan_field = dist;
            queue.push_front(rv2);
            rv2->flag |= RV_VISIT;
          }
          else {
            rv2->tan_field = min_ff(rv2->tan_field, dist);
          }
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      smooth_tangent_field();
    }

    calc_tanfield_bounds();

    if (debugColors[4]) {
      MPropCol *vcol = debugColors[4];

      for (int i = 0; i < totvert; i++, vcol++) {
        auto color = vcol->color;
        float f = (verts[i].tan_field - tmindis) * tdis_scale;

        // f *= 5.0f;
        // f -= floorf(f);

        color[0] = color[1] = color[2] = f;
        color[3] = 1.0f;
      }
    }
  }

  void calc_tangents()
  {
    calc_tangent_scalar_field();

    blender::threading::parallel_for(IndexRange(totvert), 512, [&](IndexRange subrange) {
      for (auto i : subrange) {
        ReliefVertex *rv = verts + i;
        float tan[3] = {0.0f, 0.0f, 0.0f};

        if (!rv->totneighbor) {
          zero_v3(rv->tan);
          zero_v3(rv->bin);

          rv->tan[0] = 1.0f;
          rv->bin[1] = 1.0f;

          continue;
        }

        bool bad = false;

        for (int j = 0; j < rv->totneighbor; j++) {
          ReliefVertex *rv2 = rv->neighbors[j];
          float tan2[3];

          if (rv2->flag & RV_IS_BOUNDARY) {
            bad = true;
            break;
          }

          sub_v3_v3v3(tan2, rv2->co, rv->co);
          madd_v3_v3fl(tan2, rv->no, -dot_v3v3(tan2, rv->no));
          float df = rv2->tan_field - rv->tan_field;
          // float df = rv2->bound_dist - rv->bound_dist;

          mul_v3_fl(tan2, df);
          add_v3_v3(tan, tan2);
        }

        if (bad) {
          continue;
        }

        normalize_v3(tan);

        if (0 && (dot_v3v3(tan, tan) == 0.0f || fabsf(dot_v3v3(tan, rv->no)) > 0.99999f)) {
          /* try to derive something workabel */
          float tmp[3] = {0.0f, 0.0f, 0.0f};

          /* find a world axis */
          float ax = fabsf(rv->no[0]);
          float ay = fabsf(rv->no[1]);
          float az = fabsf(rv->no[2]);

          int axis;
          if (ax > ay && ax > az) {
            axis = 1;
          }
          else if (ay > ax && ay > az) {
            axis = 2;
          }
          else {
            axis = 0;
          }
          tmp[axis] = 1.0f;

          cross_v3_v3v3(tan, tmp, rv->no);
          normalize_v3(tan);
        }

        float fac = dot_v3v3(tan, rv->no);
        madd_v3_v3fl(tan, rv->no, -fac);

        copy_v3_v3(rv->tan, tan);
        cross_v3_v3v3(rv->bin, tan, rv->no);
        normalize_v3(rv->bin);

        /*  */
        copy_v3_v3(rv->mat[0], rv->tan);
        copy_v3_v3(rv->mat[1], rv->bin);
        copy_v3_v3(rv->mat[2], rv->no);

        invert_m3(rv->mat);
      }

      if (debugColors[2]) {
        MPropCol *vcol = debugColors[2];

        for (int i = 0; i < totvert; i++, vcol++) {
          ReliefVertex *rv = verts + i;
          float *color = vcol->color;

          color[0] = rv->tan[0] * 0.5f + 0.5f;
          color[1] = rv->tan[1] * 0.5f + 0.5f;
          color[2] = rv->tan[2] * 0.5f + 0.5f;

          color[3] = 1.0f;
        }
      }
    });
  }

  void calc_displacements()
  {
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;
      float disp[3];

      if ((rv->flag & RV_IS_BOUNDARY)) {
        continue;
      }

      sub_v3_v3v3(disp, (rv->flag & RV_HAS_HIT) ? rv->targetco : rv->origco, rv->co);

#if 1
      float dfac = dot_v3v3(disp, rv->no);
      copy_v3_v3(disp, rv->no);
      mul_v3_fl(disp, dfac);
#endif

      mul_m3_v3(rv->mat, disp);

      copy_v3_v3(rv->disp, disp);
      copy_v3_v3(rv->tnor, (rv->flag & RV_HAS_HIT) ? rv->targetno : rv->no);
      mul_m3_v3(rv->mat, rv->tnor);

      if (debugColors[3]) {
        MPropCol *vcol = debugColors[3] + i;
        auto color = vcol->color;

        float sf = 1.0f / (rmaxdis - rmindis);

        color[0] = (rv->disp[0] * sf) * 0.5f + 0.5f;
        color[1] = (rv->disp[1] * sf) * 0.5f + 0.5f;
        color[2] = (rv->disp[2] * sf) * 0.5f + 0.5f;

        // mul_v3_fl(color, 2.0f / sf);

        color[3] = 1.0f;
      }
    }
  }

  void smooth_boundary()
  {
    smooth_boundary_intern(offsetof(ReliefVertex, targetco));
    smooth_boundary_intern(offsetof(ReliefVertex, origco));
    smooth_boundary_intern(offsetof(ReliefVertex, co));

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      copy_v3_v3(rv->smoothco, rv->co);

      sub_v3_v3v3(rv->ray, rv->targetco, rv->origco);
      rv->ray_dist = normalize_v3(rv->ray);
    }

    calc_raydist_bounds();
  }

  void smooth_boundary_intern(size_t member)
  {
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      copy_v3_v3(rv->smoothco, rv->co);

      char *ptr = reinterpret_cast<char *>(rv);
      float *co = reinterpret_cast<float *>(ptr + member);

      copy_v3_v3(rv->co, co);
    }

    recalc_normals();

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!(rv->flag & RV_IS_BOUNDARY)) {
        continue;
      }

      float tot = 0.0f;
      float avg[3] = {0.0f, 0.0f, 0.0f};

      for (int j = 0; j < rv->totneighbor; j++) {
        ReliefVertex *rv2 = rv->neighbors[j];

        float tmp[3];

        sub_v3_v3v3(tmp, rv2->co, rv->co);

        if (!(rv2->flag & RV_IS_BOUNDARY)) {
          // madd_v3_v3fl(avg, rv->no, 0.95f*dot_v3v3(rv->no, tmp));
        }
        else {
          madd_v3_v3fl(tmp, rv->no, -0.95f * dot_v3v3(rv->no, tmp));
          add_v3_v3(avg, tmp);
        }

        tot += 1.0f;
      }

      if (tot == 0.0f) {
        continue;
      }

      const float factor = 0.75;

      mul_v3_fl(avg, factor / tot);
      add_v3_v3(rv->co, avg);
    }

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      char *ptr = reinterpret_cast<char *>(rv);
      float *co = reinterpret_cast<float *>(ptr + member);

      copy_v3_v3(co, rv->co);

      if (member != offsetof(ReliefVertex, co)) {
        copy_v3_v3(rv->co, rv->smoothco);
      }
    }
  }

  void project_outside()
  {
    // smooth_rays();

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (rv->flag & RV_HAS_HIT) {
        continue;
      }

      float vec[3];

      float fac = 1.0f - (rv->bound_dist - bmindis) * bdis_scale * boundWidth;

      CLAMP(fac, 0.0f, 1.0f);
      fac *= fac;
      // fac = fac * fac * (3.0f - 2.0f * fac);

      if (!rv->bound_vert) {
        continue;
      }

      copy_v3_v3(vec, rv->bound_vert->ray);
      mul_v3_fl(vec, fac * rv->bound_vert->ray_dist);

      copy_v3_v3(rv->targetco, rv->origco);
      madd_v3_v3fl(rv->targetco, vec, 1.0f);
    }

    for (int i = 0; i < boundSmoothSteps; i++) {
      smooth_outside();
      // smooth_cos(true, 0.75f, 0.0f);
      // recalc_normals();
    }

    recalc_normals();

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (rv->flag & RV_HAS_HIT) {
        continue;
      }

      sub_v3_v3v3(rv->ray, rv->targetco, rv->origco);
      rv->ray_dist = normalize_v3(rv->ray);
    }

    calc_raydist_bounds();
  }

  void smooth_outside()
  {
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!rv->totneighbor || (rv->flag & RV_HAS_HIT)) {  // && !(rv->flag & RV_IS_BOUNDARY)) {
        continue;
      }

      madd_v3_v3fl(rv->targetco, rv->svel, 1.0f);

      float avg[3] = {0.0f, 0.0f, 0.0f};
      float tot = 0.0f;

      float invtot = 1.0f / (float)rv->totneighbor;

      for (int j = 0; j < rv->totneighbor; j++) {
        ReliefVertex *rv2 = rv->neighbors[j];

        add_v3_v3(avg, rv2->targetco);
        madd_v3_v3fl(rv2->svel, rv->svel, invtot * 0.5f);

        tot += 1.0f;
      }

      if (tot == 0.0f) {
        continue;
      }

      mul_v3_fl(avg, 1.0f / tot);
      sub_v3_v3(avg, rv->targetco);
      mul_v3_fl(avg, 0.75f);

      add_v3_v3(rv->targetco, avg);
      add_v3_v3(rv->svel, avg);
      mul_v3_fl(rv->svel, 0.95f);
    }
  }
  void solve(int maxSteps, float (*deformOut)[3])
  {
    set_boundary_flags();

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!(rv->flag & RV_HAS_HIT)) {
        copy_v3_v3(rv->targetco, rv->co);
      }
    }

    for (int i = 0; i < 5; i++) {
      smooth_boundary();
    }

#if 0
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      interp_v3_v3v3(deformOut[i], rv->co, rv->targetco, compress_ratio);
    }

    return;
#endif

    recalc_normals();
    calc_raydist_bounds();

#if 0
    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!(rv->flag & RV_HAS_HIT)) {
        continue;
      };

      float t = (rv->ray_dist - rmindis) * rdis_scale;

      // t = t * t;
      t = powf(t, 0.5);
      t = t * (rmaxdis - rmindis) + rmindis;

      float ray[3];
      sub_v3_v3v3(ray, rv->targetco, rv->origco);
      normalize_v3(ray);

      copy_v3_v3(rv->ray, ray);

      madd_v3_v3v3fl(rv->co, rv->origco, rv->ray, t * compress_ratio);
    }
#endif

#if 1

    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(verts[i].smoothco, verts[i].co);
      zero_v3(verts[i].svel);
    }

    // for (int i = 0; i < 15; i++) {
    //  smooth_cos();
    //  recalc_normals();
    //}

    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(verts[i].co, verts[i].origco);
    }

    recalc_normals();
    solve_geodesic();
    smooth_geodesic();

    project_outside();

    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(verts[i].co, verts[i].targetco);
    }

    recalc_normals();
    // resolve boundary distance field
    solve_geodesic();

    for (int i = 0; i < 5; i++) {
      smooth_geodesic();
    }

    // smooth for tangent field
    for (int i = 0; i < maxSteps; i++) {
      smooth_cos(false, 0.75f, 0.0f);
      recalc_normals();
    }

    calc_tangents();
    calc_displacements();

    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(verts[i].co, verts[i].origco);
    }

    recalc_normals();

    for (int i = 0; i < totvert; i++) {
      for (int j = 0; j < verts[i].totneighbor; j++) {
        verts[i].neighbor_restlens[j] = len_v3v3(verts[i].co, verts[i].neighbors[j]->co);
      }
    }

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      sub_v3_v3v3(rv->ray, rv->targetco, rv->origco);
      rv->ray_dist = len_v3(rv->ray) * compress_ratio;
      normalize_v3(rv->ray);

      copy_v3_v3(rv->origray, rv->ray);
      rv->origray_dist = rv->ray_dist;
    }

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      madd_v3_v3fl(rv->co, rv->ray, rv->ray_dist);
    }

    recalc_normals();
    for (int i = 0; i < maxSteps; i++) {
      smooth_cos(false, 0.75f, 0.0f);
      recalc_normals();
    }

    /* update tangent field */
    calc_tangents();

    for (int i = 0; i < totvert; i++) {
      ReliefVertex *rv = verts + i;

      if (!(rv->flag & RV_HAS_HIT) || (rv->flag & RV_IS_BOUNDARY)) {
        continue;
      }

      float mat[3][3];
      copy_m3_m3(mat, rv->mat);
      invert_m3(mat);

      float disp[3];
      copy_v3_v3(disp, rv->disp);
      mul_m3_v3(mat, disp);

      float bw = (rv->bound_dist - bmindis) * bdis_scale;
      bw = pow(bw, 0.25f);

      madd_v3_v3fl(rv->co, disp, normalScale);
    }

    recalc_normals();
#endif

    if (debugColors[0]) {
      MPropCol *vcol = debugColors[0];

      for (int i = 0; i < totvert; i++, vcol++) {
        ReliefVertex *rv = verts + i;
        float *color = vcol->color;

        float bdis = (rv->bound_dist - bmindis) * bdis_scale;

        color[0] = color[1] = color[2] = bdis;
        color[3] = 1.0f;
      }
    }

    if (debugColors[1]) {
      MPropCol *vcol = debugColors[1];

      for (int i = 0; i < totvert; i++, vcol++) {
        ReliefVertex *rv = verts + i;
        float *color = vcol->color;

        color[0] = rv->no[0] * 0.5f + 0.5f;
        color[1] = rv->no[1] * 0.5f + 0.5f;
        color[2] = rv->no[2] * 0.5f + 0.5f;
        color[3] = 1.0f;
      }
    }

    for (int i = 0; i < totvert; i++) {
      copy_v3_v3(deformOut[i], verts[i].co);
    }
  }
};
}  // namespace bassrelief
}  // namespace blender

using namespace blender::bassrelief;

struct BassReliefCalcData {
  BassReliefModifierData *smd; /* shrinkwrap modifier data */

  struct Object *ob; /* object we are applying shrinkwrap to */

  const float (
      *vert_positions)[3]; /* Array of verts being projected (to fetch normals or other data) */
  float (*vertexCos)[3];   /* vertexs being shrinkwraped */
  const float (*vertexNos)[3]; /* vertexs being shrinkwraped */
  int numVerts;

  const struct MDeformVert *dvert; /* Pointer to mdeform array */
  int vgroup;                      /* Vertex group num */
  bool invert_vgroup;              /* invert vertex group influence */

  struct Mesh *target; /* mesh we are shrinking to */

  std::vector<BassReliefTreeData> trees;

  float keepDist;    /* Distance to keep above target surface (units are in local space) */
  float shrinkRatio; /* interp vertices towards original positions by shrinkRatio */

  ReliefOptimizer *ropt;
  MPropCol *debugColors[MAX_BASSRELIEF_DEBUG_COLORS];
};

struct BassReliefTreeRayHit {
  BVHTreeRayHit hit;
  BassReliefTreeData *tree;
  float origin[3];
};

struct BassReliefCalcCBData {
  BassReliefCalcData *calc;
  float *proj_axis;
};

/* Initializes the mesh data structure from the given mesh and settings. */
bool BKE_bassrelief_init_tree(
    Depsgraph *depsgraph, Object *ob_local, Object *ob, BassReliefTreeData *data, float keepDist)
{
  memset(data, 0, sizeof(*data));

  if (ob == nullptr) {
    return false;
  }

  Object *ob_target = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);

  if (mesh == nullptr) {
    return false;
  }

  /* We could create a BVH tree from the edit mesh,
   * however accessing normals from the face/loop normals gets more involved.
   * Convert mesh data since this isn't typically used in edit-mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);

  if (mesh->totvert <= 0) {
    return false;
  }

  data->mesh = mesh;
  data->mpoly = mesh->polys().data();

  if (mesh->totpoly <= 0) {
    return false;
  }

  data->bvh = BKE_bvhtree_from_mesh_get(&data->treeData, mesh, BVHTREE_FROM_LOOPTRI, 4);

  data->vert_normals = BKE_mesh_vertex_normals_ensure(mesh);

  if (data->bvh == nullptr) {
    return false;
  }

  data->pnors = reinterpret_cast<decltype(data->pnors)>(
      CustomData_get_layer_for_write(&mesh->pdata, CD_NORMAL, mesh->totpoly));

  if ((mesh->flag & ME_AUTOSMOOTH) != 0) {
    data->clnors = reinterpret_cast<decltype(data->clnors)>(
        CustomData_get_layer_for_write(&mesh->ldata, CD_NORMAL, mesh->totloop));
  }

  /* TODO: there might be several "bugs" with non-uniform scales matrices
   * because it will no longer be nearest surface, not sphere projection
   * because space has been deformed */
  BLI_SPACE_TRANSFORM_SETUP(&data->transform, ob_local, ob_target);

  data->keepDist = keepDist;

  return true;
}

/* Frees the tree data if necessary. */
void BKE_bassrelief_free_tree(BassReliefTreeData *data)
{
  free_bvhtree_from_mesh(&data->treeData);
}

struct ShrinkWrapRayData {
  BVHTreeFromMesh *data;
  int cull_mask;
  float dist;
};

static void shrinkwrap_ray_callback(void *userdata,
                                    int index,
                                    const BVHTreeRay *ray,
                                    BVHTreeRayHit *hit)
{
  ShrinkWrapRayData *sdata = (ShrinkWrapRayData *)userdata;
  const BVHTreeFromMesh *data = sdata->data;
  const float(*vert_positions)[3] = data->vert_positions;
  const MLoopTri *lt = &data->looptri[index];
  const float *vtri_co[3] = {
      vert_positions[data->loop[lt->tri[0]].v],
      vert_positions[data->loop[lt->tri[1]].v],
      vert_positions[data->loop[lt->tri[2]].v],
  };
  float dist;

  float no[3];
  float n1[3], n2[3];
  sub_v3_v3v3(n1, vtri_co[1], vtri_co[0]);
  sub_v3_v3v3(n2, vtri_co[2], vtri_co[0]);
  cross_v3_v3v3(no, n1, n2);

  int mask = (dot_v3v3(no, ray->direction) <= 0.0f) + 1;
  if (!(mask & sdata->cull_mask)) {
    return;
  }

  if (ray->radius == 0.0f) {
    dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));
  }
  else {
    dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, UNPACK3(vtri_co));
  }

  if (dist < 0.0f || dist == FLT_MAX) {
    return;
  }

  float sign = mask == 1 ? -1.0f : 1.0f;

  if (dist * sign < sdata->dist * sign) {
    /* can't set hit->dist here since we need to bypass early
       return optimization in bvh code */
    hit->index = index;
    sdata->dist = dist;

    madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);
    normal_tri_v3(hit->no, UNPACK3(vtri_co));
  }
}

/*
 * This function raycast a single vertex and updates the hit if the "hit" is considered valid.
 * Returns true if "hit" was updated.
 * Opts control whether an hit is valid or not
 * Supported options are:
 * - MOD_BASSRELIEF_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 * - MOD_BASSRELIEF_CULL_TARGET_BACKFACE (back faces hits are ignored)
 */
bool BKE_bassrelief_project_normal(char options,
                                   const float co[3],
                                   const float no[3],
                                   const float ray_radius,
                                   const SpaceTransform *transf,
                                   BassReliefTreeData *tree,
                                   BVHTreeRayHit *hit)
{
  float sign = 1.0f;
  float start;

  int mask = 0;
  if (options & MOD_BASSRELIEF_CULL_TARGET_MASK) {
    if (options & MOD_BASSRELIEF_CULL_TARGET_FRONTFACE) {
      mask |= 1;
      sign = -1.0f;
      start = 0.0f;
    }
    if (options & MOD_BASSRELIEF_CULL_TARGET_BACKFACE) {
      mask |= 2;
      start = BVH_RAYCAST_DIST_MAX;
    }
  }
  else {
    mask = 2;
  }

  ShrinkWrapRayData sdata = {&tree->treeData, mask, start};

  BVHTreeRayHit hit_tmp = *hit;

  hit_tmp.index = -1;

  /* we can't use hit->dist since we might be in
     max-distance mode, have to be careful not to trigger
     early optimizations in the bvh node walking code.*/
  hit_tmp.dist = BVH_RAYCAST_DIST_MAX;

  BLI_bvhtree_ray_cast(tree->bvh, co, no, ray_radius, &hit_tmp, shrinkwrap_ray_callback, &sdata);

  if (hit_tmp.index != -1) {
    float dt = sdata.dist - hit->dist;

    if (options & MOD_BASSRELIEF_CULL_TARGET_FRONTFACE) {
      dt = -dt;
    }

    if (dt < 0.0f) {
      *hit = hit_tmp;
      hit->dist = sdata.dist;

      return true;
    }
  }

  return false;
}

ATTR_NO_OPT static void shrinkwrap_calc_normal_projection_cb_ex_intern(
    BassReliefCalcCBData *data,
    const int i,
    BassReliefTreeData *tree,
    BassReliefTreeRayHit *treehit)
{
  BassReliefCalcData *calc = data->calc;
  float *proj_axis = data->proj_axis;
  BVHTreeRayHit *hit = &treehit->hit;

  const float proj_limit = calc->smd->projLimit;
  float *co = calc->vertexCos[i];
  float tmp_co[3], tmp_no[3];

  BVHTreeRayHit hit_tmp = *hit;
  hit_tmp.index = -1;

  if (calc->vert_positions != nullptr &&
      calc->smd->projAxis == MOD_BASSRELIEF_PROJECT_OVER_NORMAL) {
    /* calc->vert contains verts from evaluated mesh. */
    /* These coordinates are deformed by vertexCos only for normal projection
     * (to get correct normals) for other cases calc->verts contains undeformed coordinates and
     * vertexCos should be used */
    copy_v3_v3(tmp_co, calc->vert_positions[i]);
    copy_v3_v3(tmp_no, calc->vertexNos[i]);
  }
  else {
    copy_v3_v3(tmp_co, co);
    copy_v3_v3(tmp_no, proj_axis);
  }

  BLI_space_transform_apply(&tree->transform, tmp_co);
  BLI_space_transform_apply_normal(&tree->transform, tmp_no);

  if (hit->index != -1) {
    BLI_space_transform_apply(&tree->transform, hit_tmp.co);
    BLI_space_transform_apply_normal(&tree->transform, hit_tmp.no);
    hit_tmp.dist = len_v3v3(hit_tmp.co, tmp_co);
  }

  /* Project over positive direction of axis */
  if (calc->smd->shrinkOpts & MOD_BASSRELIEF_PROJECT_ALLOW_POS_DIR) {
    BKE_bassrelief_project_normal(
        calc->smd->shrinkOpts, tmp_co, tmp_no, 0.0, &tree->transform, tree, &hit_tmp);
  }

  /* Project over negative direction of axis */
  if (calc->smd->shrinkOpts & MOD_BASSRELIEF_PROJECT_ALLOW_NEG_DIR) {
    float inv_no[3];
    negate_v3_v3(inv_no, tmp_no);

    char options = calc->smd->shrinkOpts;

    if ((options & MOD_BASSRELIEF_INVERT_CULL_TARGET) &&
        (options & MOD_BASSRELIEF_CULL_TARGET_MASK)) {
      options ^= MOD_BASSRELIEF_CULL_TARGET_MASK;
    }

    BKE_bassrelief_project_normal(options, tmp_co, inv_no, 0.0, &tree->transform, tree, &hit_tmp);
  }

  if (hit_tmp.index != -1) {
    /* convert back to target space */
    BLI_space_transform_invert_normal(&tree->transform, hit_tmp.no);
    BLI_space_transform_invert(&tree->transform, hit_tmp.co);
    BLI_space_transform_invert(&tree->transform, tmp_co);

    hit_tmp.dist = len_v3v3(tmp_co, hit_tmp.co);

    if (proj_limit > 0.0f && hit_tmp.dist > proj_limit) {
      return;
    }

    *hit = hit_tmp;
    treehit->tree = tree;

    copy_v3_v3(treehit->origin, tmp_co);
  }
}

static void shrinkwrap_calc_normal_projection_cb_ex(void *__restrict userdata,
                                                    const int i,
                                                    const TaskParallelTLS *__restrict tls)
{
  BassReliefCalcCBData *data = (BassReliefCalcCBData *)userdata;
  BassReliefCalcData *calc = data->calc;
  float *co = calc->vertexCos[i];

  BassReliefTreeRayHit hit;

  hit.hit.index = -1;

  /* TODO: we should use FLT_MAX here, but sweepsphere code isn't prepared for that */
  hit.hit.dist = (calc->smd->shrinkOpts & MOD_BASSRELIEF_CULL_TARGET_FRONTFACE) ?
                     0.0f :
                     BVH_RAYCAST_DIST_MAX;

  for (BassReliefTreeData &tree : calc->trees) {
    shrinkwrap_calc_normal_projection_cb_ex_intern(data, i, &tree, &hit);
  }

  float weight = BKE_defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

  if (calc->invert_vgroup) {
    weight = 1.0f - weight;
  }

  if (weight == 0.0f) {
    return;
  }

  if (hit.hit.index != -1) {
    bassrelief_snap_point_to_surface(hit.tree,
                                     &hit.tree->transform,
                                     hit.hit.index,
                                     hit.hit.co,
                                     hit.hit.no,
                                     hit.tree->keepDist,
                                     hit.origin,
                                     hit.hit.co);

    ReliefVertex *rv;
    if (calc->ropt) {
      rv = calc->ropt->verts + i;
      copy_v3_v3(rv->targetco, hit.hit.co);
      copy_v3_v3(rv->targetno, hit.hit.no);
      rv->ray_dist = hit.hit.dist;
      rv->flag |= RV_HAS_HIT;
    }

    interp_v3_v3v3(co, co, hit.hit.co, weight * calc->shrinkRatio);

    if (calc->ropt) {
      copy_v3_v3(rv->co, co);
    }
  }
}

static void shrinkwrap_calc_normal_projection(BassReliefCalcData *calc)
{
  /* Options about projection direction */
  float proj_axis[3] = {0.0f, 0.0f, 0.0f};

  /* Ray-cast and tree stuff. */

  /* If the user doesn't allows to project in any direction of projection axis
   * then there's nothing todo. */
  if ((calc->smd->shrinkOpts &
       (MOD_BASSRELIEF_PROJECT_ALLOW_POS_DIR | MOD_BASSRELIEF_PROJECT_ALLOW_NEG_DIR)) == 0) {
    return;
  }

  /* Prepare data to retrieve the direction in which we should project each vertex */
  if (calc->smd->projAxis == MOD_BASSRELIEF_PROJECT_OVER_NORMAL) {
    if (calc->vert_positions == nullptr) {
      return;
    }
  }
  else {
    /* The code supports any axis that is a combination of X,Y,Z
     * although currently UI only allows to set the 3 different axis */
    if (calc->smd->projAxis & MOD_BASSRELIEF_PROJECT_OVER_X_AXIS) {
      proj_axis[0] = 1.0f;
    }
    if (calc->smd->projAxis & MOD_BASSRELIEF_PROJECT_OVER_Y_AXIS) {
      proj_axis[1] = 1.0f;
    }
    if (calc->smd->projAxis & MOD_BASSRELIEF_PROJECT_OVER_Z_AXIS) {
      proj_axis[2] = 1.0f;
    }

    normalize_v3(proj_axis);

    /* Invalid projection direction */
    if (len_squared_v3(proj_axis) < FLT_EPSILON) {
      return;
    }
  }

  /* After successfully build the trees, start projection vertices. */
  BassReliefCalcCBData data = {
      calc,
      proj_axis,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = false;  // XXX (calc->numVerts > BKE_MESH_OMP_LIMIT);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_normal_projection_cb_ex, &settings);

  /* free data structures */
}

/*
 * Shrinkwrap Target Surface Project mode
 *
 * It uses Newton's method to find a surface location with its
 * smooth normal pointing at the original point.
 *
 * The equation system on barycentric weights and normal multiplier:
 *
 *   (w0*V0 + w1*V1 + w2*V2) + l * (w0*N0 + w1*N1 + w2*N2) - CO = 0
 *   w0 + w1 + w2 = 1
 *
 * The actual solution vector is [ w0, w1, l ], with w2 eliminated.
 */

//#define TRACE_TARGET_PROJECT

typedef struct TargetProjectTriData {
  const float **vtri_co;
  const float (*vtri_no)[3];
  const float *point_co;

  float n0_minus_n2[3], n1_minus_n2[3];
  float c0_minus_c2[3], c1_minus_c2[3];

  /* Current interpolated position and normal. */
  float co_interp[3], no_interp[3];
} TargetProjectTriData;

/* Computes the deviation of the equation system from goal. */
static void target_project_tri_deviation(void *userdata, const float x[3], float r_delta[3])
{
  TargetProjectTriData *data = (TargetProjectTriData *)userdata;

  const float w[3] = {x[0], x[1], 1.0f - x[0] - x[1]};
  interp_v3_v3v3v3(data->co_interp, data->vtri_co[0], data->vtri_co[1], data->vtri_co[2], w);
  interp_v3_v3v3v3(data->no_interp, data->vtri_no[0], data->vtri_no[1], data->vtri_no[2], w);

  madd_v3_v3v3fl(r_delta, data->co_interp, data->no_interp, x[2]);
  sub_v3_v3(r_delta, data->point_co);
}

/* Computes the Jacobian matrix of the equation system. */
static void target_project_tri_jacobian(void *userdata, const float x[3], float r_jacobian[3][3])
{
  TargetProjectTriData *data = (TargetProjectTriData *)userdata;

  madd_v3_v3v3fl(r_jacobian[0], data->c0_minus_c2, data->n0_minus_n2, x[2]);
  madd_v3_v3v3fl(r_jacobian[1], data->c1_minus_c2, data->n1_minus_n2, x[2]);

  copy_v3_v3(r_jacobian[2], data->vtri_no[2]);
  madd_v3_v3fl(r_jacobian[2], data->n0_minus_n2, x[0]);
  madd_v3_v3fl(r_jacobian[2], data->n1_minus_n2, x[1]);
}

/* Clamp barycentric weights to the triangle. */
static void target_project_tri_clamp(float x[3])
{
  if (x[0] < 0.0f) {
    x[0] = 0.0f;
  }
  if (x[1] < 0.0f) {
    x[1] = 0.0f;
  }
  if (x[0] + x[1] > 1.0f) {
    x[0] = x[0] / (x[0] + x[1]);
    x[1] = 1.0f - x[0];
  }
}

/* Correct the Newton's method step to keep the coordinates within the triangle. */
static bool target_project_tri_correct(void * /*userdata*/,
                                       const float x[3],
                                       float step[3],
                                       float x_next[3])
{
  /* Insignificant correction threshold */
  const float epsilon = 1e-5f;
  /* Dot product threshold for checking if step is 'clearly' pointing outside. */
  const float dir_epsilon = 0.5f;
  bool fixed = false, locked = false;

  /* The barycentric coordinate domain is a triangle bounded by
   * the X and Y axes, plus the x+y=1 diagonal. First, clamp the
   * movement against the diagonal. Note that step is subtracted. */
  float sum = x[0] + x[1];
  float sstep = -(step[0] + step[1]);

  if (sum + sstep > 1.0f) {
    float ldist = 1.0f - sum;

    /* If already at the boundary, slide along it. */
    if (ldist < epsilon * (float)M_SQRT2) {
      float step_len = len_v2(step);

      /* Abort if the solution is clearly outside the domain. */
      if (step_len > epsilon && sstep > step_len * dir_epsilon * (float)M_SQRT2) {
        return false;
      }

      /* Project the new position onto the diagonal. */
      add_v2_fl(step, (sum + sstep - 1.0f) * 0.5f);
      fixed = locked = true;
    }
    else {
      /* Scale a significant step down to arrive at the boundary. */
      mul_v3_fl(step, ldist / sstep);
      fixed = true;
    }
  }

  /* Weight 0 and 1 boundary checks - along axis. */
  for (int i = 0; i < 2; i++) {
    if (step[i] > x[i]) {
      /* If already at the boundary, slide along it. */
      if (x[i] < epsilon) {
        float step_len = len_v2(step);

        /* Abort if the solution is clearly outside the domain. */
        if (step_len > epsilon && (locked || step[i] > step_len * dir_epsilon)) {
          return false;
        }

        /* Reset precision errors to stay at the boundary. */
        step[i] = x[i];
        fixed = true;
      }
      else {
        /* Scale a significant step down to arrive at the boundary. */
        mul_v3_fl(step, x[i] / step[i]);
        fixed = true;
      }
    }
  }

  /* Recompute and clamp the new coordinates after step correction. */
  if (fixed) {
    sub_v3_v3v3(x_next, x, step);
    target_project_tri_clamp(x_next);
  }

  return true;
}

static bool target_project_solve_point_tri(const float *vtri_co[3],
                                           const float vtri_no[3][3],
                                           const float point_co[3],
                                           const float hit_co[3],
                                           float hit_dist_sq,
                                           float r_hit_co[3],
                                           float r_hit_no[3])
{
  float x[3], tmp[3];
  float dist = sqrtf(hit_dist_sq);
  float magnitude_estimate = dist + len_manhattan_v3(vtri_co[0]) + len_manhattan_v3(vtri_co[1]) +
                             len_manhattan_v3(vtri_co[2]);
  float epsilon = magnitude_estimate * 1.0e-6f;

  /* Initial solution vector: barycentric weights plus distance along normal. */
  interp_weights_tri_v3(x, UNPACK3(vtri_co), hit_co);

  interp_v3_v3v3v3(r_hit_no, UNPACK3(vtri_no), x);
  sub_v3_v3v3(tmp, point_co, hit_co);

  x[2] = (dot_v3v3(tmp, r_hit_no) < 0) ? -dist : dist;

  /* Solve the equations iteratively. */
  TargetProjectTriData tri_data = {
      vtri_co,
      vtri_no,
      point_co,
  };

  sub_v3_v3v3(tri_data.n0_minus_n2, vtri_no[0], vtri_no[2]);
  sub_v3_v3v3(tri_data.n1_minus_n2, vtri_no[1], vtri_no[2]);
  sub_v3_v3v3(tri_data.c0_minus_c2, vtri_co[0], vtri_co[2]);
  sub_v3_v3v3(tri_data.c1_minus_c2, vtri_co[1], vtri_co[2]);

  target_project_tri_clamp(x);

#ifdef TRACE_TARGET_PROJECT
  const bool trace = true;
#else
  const bool trace = false;
#endif

  bool ok = BLI_newton3d_solve(target_project_tri_deviation,
                               target_project_tri_jacobian,
                               target_project_tri_correct,
                               &tri_data,
                               epsilon,
                               20,
                               trace,
                               x,
                               x);

  if (ok) {
    copy_v3_v3(r_hit_co, tri_data.co_interp);
    copy_v3_v3(r_hit_no, tri_data.no_interp);

    return true;
  }

  return false;
}

static bool update_hit(BVHTreeNearest *nearest,
                       int index,
                       const float co[3],
                       const float hit_co[3],
                       const float hit_no[3])
{
  float dist_sq = len_squared_v3v3(hit_co, co);

  if (dist_sq < nearest->dist_sq) {
#ifdef TRACE_TARGET_PROJECT
    printf(
        "#=#=#> %d (%.3f,%.3f,%.3f) %g < %g\n", index, UNPACK3(hit_co), dist_sq, nearest->dist_sq);
#endif
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, hit_co);
    normalize_v3_v3(nearest->no, hit_no);
    return true;
  }

  return false;
}

/* Target normal projection BVH callback - based on mesh_looptri_nearest_point. */
ATTR_NO_OPT static void mesh_looptri_target_project(void *userdata,
                                                    int index,
                                                    const float co[3],
                                                    BVHTreeNearest *nearest)
{
  const BassReliefTreeData *tree = (BassReliefTreeData *)userdata;
  const BVHTreeFromMesh *data = &tree->treeData;
  const MLoopTri *lt = &data->looptri[index];
  const MLoop *loop[3] = {
      &data->loop[lt->tri[0]], &data->loop[lt->tri[1]], &data->loop[lt->tri[2]]};
  const float *vtri_co[3] = {data->vert_positions[loop[0]->v],
                             data->vert_positions[loop[1]->v],
                             data->vert_positions[loop[2]->v]};
  float raw_hit_co[3], hit_co[3], hit_no[3], dist_sq, vtri_no[3][3];

  /* First find the closest point and bail out if it's worse than the current solution. */
  closest_on_tri_to_point_v3(raw_hit_co, co, UNPACK3(vtri_co));
  dist_sq = len_squared_v3v3(co, raw_hit_co);

#ifdef TRACE_TARGET_PROJECT
  printf("TRIANGLE %d (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) %g %g\n",
         index,
         UNPACK3(vtri_co[0]),
         UNPACK3(vtri_co[1]),
         UNPACK3(vtri_co[2]),
         dist_sq,
         nearest->dist_sq);
#endif

  if (dist_sq >= nearest->dist_sq) {
    return;
  }

  /* Decode normals */
  copy_v3_v3(vtri_no[0], tree->vert_normals[loop[0]->v]);
  copy_v3_v3(vtri_no[1], tree->vert_normals[loop[1]->v]);
  copy_v3_v3(vtri_no[2], tree->vert_normals[loop[2]->v]);

  /* Solve the equations for the triangle */
  if (target_project_solve_point_tri(vtri_co, vtri_no, co, raw_hit_co, dist_sq, hit_co, hit_no)) {
    update_hit(nearest, index, co, hit_co, hit_no);
  }
}

/**
 * Compute a smooth normal of the target (if applicable) at the hit location.
 *
 * \param tree: information about the mesh
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_no: output in hit coordinate space; may be shared with inputs
 */
ATTR_NO_OPT void BKE_bassrelief_compute_smooth_normal(const struct BassReliefTreeData *tree,
                                                      const struct SpaceTransform *transform,
                                                      int looptri_idx,
                                                      const float hit_co[3],
                                                      const float hit_no[3],
                                                      float r_no[3])
{
  const BVHTreeFromMesh *treeData = &tree->treeData;
  const MLoopTri *tri = &treeData->looptri[looptri_idx];

  /* Interpolate smooth normals if enabled. */
  if ((tree->mpoly[tri->poly].flag & ME_SMOOTH) != 0) {
    const float *verts[] = {
        treeData->vert_positions[treeData->loop[tri->tri[0]].v],
        treeData->vert_positions[treeData->loop[tri->tri[1]].v],
        treeData->vert_positions[treeData->loop[tri->tri[2]].v],
    };
    float w[3], no[3][3], tmp_co[3];

    /* Custom and auto smooth split normals. */
    if (tree->clnors) {
      copy_v3_v3(no[0], tree->clnors[tri->tri[0]]);
      copy_v3_v3(no[1], tree->clnors[tri->tri[1]]);
      copy_v3_v3(no[2], tree->clnors[tri->tri[2]]);
    }
    /* Ordinary vertex normals. */
    else {
      copy_v3_v3(no[0], tree->vert_normals[treeData->loop[tri->tri[0]].v]);
      copy_v3_v3(no[1], tree->vert_normals[treeData->loop[tri->tri[1]].v]);
      copy_v3_v3(no[2], tree->vert_normals[treeData->loop[tri->tri[2]].v]);
    }

    /* Barycentric weights from hit point. */
    copy_v3_v3(tmp_co, hit_co);

    if (transform) {
      BLI_space_transform_apply(transform, tmp_co);
    }

    interp_weights_tri_v3(w, verts[0], verts[1], verts[2], tmp_co);

    /* Interpolate using weights. */
    interp_v3_v3v3v3(r_no, no[0], no[1], no[2], w);

    if (transform) {
      BLI_space_transform_invert_normal(transform, r_no);
    }
    else {
      normalize_v3(r_no);
    }
  }
  /* Use the polygon normal if flat. */
  else if (tree->pnors != nullptr) {
    copy_v3_v3(r_no, tree->pnors[tri->poly]);
  }
  /* Finally fallback to the looptri normal. */
  else {
    copy_v3_v3(r_no, hit_no);
  }
}

/* Helper for MOD_BASSRELIEF_INSIDE, MOD_BASSRELIEF_OUTSIDE and MOD_BASSRELIEF_OUTSIDE_SURFACE. */
static void shrinkwrap_snap_with_side(float r_point_co[3],
                                      const float point_co[3],
                                      const float hit_co[3],
                                      const float hit_no[3],
                                      float goal_dist,
                                      float forcesign,
                                      bool forcesnap)
{
  float delta[3];
  sub_v3_v3v3(delta, point_co, hit_co);

  float dist = len_v3(delta);

  /* If exactly on the surface, push out along normal */
  if (dist < FLT_EPSILON) {
    if (forcesnap || goal_dist > 0) {
      madd_v3_v3v3fl(r_point_co, hit_co, hit_no, goal_dist * forcesign);
    }
    else {
      copy_v3_v3(r_point_co, hit_co);
    }
  }
  /* Move to the correct side if needed */
  else {
    float dsign = signf(dot_v3v3(delta, hit_no));

    if (forcesign == 0.0f) {
      forcesign = dsign;
    }

    /* If on the wrong side or too close, move to correct */
    if (forcesnap || dsign * dist * forcesign < goal_dist) {
      mul_v3_fl(delta, dsign / dist);

      /* At very small distance, blend in the hit normal to stabilize math. */
      float dist_epsilon = (fabsf(goal_dist) + len_manhattan_v3(hit_co)) * 1e-4f;

      if (dist < dist_epsilon) {
#ifdef TRACE_TARGET_PROJECT
        printf("zero_factor %g = %g / %g\n", dist / dist_epsilon, dist, dist_epsilon);
#endif

        interp_v3_v3v3(delta, hit_no, delta, dist / dist_epsilon);
      }

      madd_v3_v3v3fl(r_point_co, hit_co, delta, goal_dist * forcesign);
    }
    else {
      copy_v3_v3(r_point_co, point_co);
    }
  }
}

/**
 * Apply the shrink to surface modes to the given original coordinates and nearest point.
 *
 * \param tree: mesh data for smooth normals
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_point_co: may be the same memory location as point_co, hit_co, or hit_no.
 */
static void bassrelief_snap_point_to_surface(const struct BassReliefTreeData *tree,
                                             const struct SpaceTransform *transform,
                                             int hit_idx,
                                             const float hit_co[3],
                                             const float hit_no[3],
                                             float goal_dist,
                                             const float point_co[3],
                                             float r_point_co[3])
{
  float tmp[3];

  /* Offsets along the normal */
  if (goal_dist != 0) {
    BKE_bassrelief_compute_smooth_normal(tree, transform, hit_idx, hit_co, hit_no, tmp);
    madd_v3_v3v3fl(r_point_co, hit_co, tmp, goal_dist);
  }
  else {
    copy_v3_v3(r_point_co, hit_co);
  }
}

/* Main shrinkwrap function */
void bassReliefModifier_deform(BassReliefModifierData *smd,
                               const ModifierEvalContext *ctx,
                               struct Scene *scene,
                               Object *ob,
                               Mesh *mesh,
                               const MDeformVert *dvert,
                               const int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts,
                               MPropCol *debugColors[MAX_BASSRELIEF_DEBUG_COLORS])
{

  DerivedMesh *ss_mesh = nullptr;
  BassReliefCalcData calc = NULL_BassReliefCalcData;

  calc.ropt = nullptr;
  calc.shrinkRatio = smd->rayShrinkRatio;

  if (debugColors) {
    for (int i = 0; i < MAX_BASSRELIEF_DEBUG_COLORS; i++) {
      calc.debugColors[i] = debugColors[i];
    }
  }
  else {
    for (int i = 0; i < MAX_BASSRELIEF_DEBUG_COLORS; i++) {
      calc.debugColors[i] = nullptr;
    }
  }

  /* remove loop dependencies on derived meshes (TODO should this be done elsewhere?) */
  if (smd->target == ob) {
    smd->target = nullptr;
  }

  /* Configure Shrinkwrap calc data */
  calc.smd = smd;
  calc.ob = ob;
  calc.numVerts = numVerts;
  calc.vertexCos = vertexCos;
  calc.dvert = dvert;
  calc.vgroup = defgrp_index;
  calc.invert_vgroup = (smd->shrinkOpts & MOD_BASSRELIEF_INVERT_VGROUP) != 0;

  calc.vertexNos = BKE_mesh_vertex_normals_ensure(mesh);

  if (mesh != nullptr) {
    /* Setup arrays to get vertexs positions, normals and deform weights */
    calc.vert_positions = BKE_mesh_vert_positions(mesh);
  }

  if (!calc.ropt && (smd->shrinkOpts & MOD_BASSRELIEF_OPTIMIZE)) {
    const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
    int totlooptri = BKE_mesh_runtime_looptri_len(mesh);

    calc.ropt = new ReliefOptimizer(vertexCos,
                                    calc.vertexNos,
                                    mesh->totvert,
                                    mesh->edges().data(),
                                    mesh->totedge,
                                    calc.debugColors,
                                    mlooptri,
                                    totlooptri,
                                    mesh->loops().data(),
                                    smd->detailScale,
                                    smd->boundSmoothFalloff,
                                    smd->boundSmoothSteps);
    calc.ropt->compress_ratio = smd->rayShrinkRatio;
  }

  if (smd->target && smd->target->type == OB_MESH) {
    BassReliefTreeData tree;

    if (BKE_bassrelief_init_tree(ctx->depsgraph, ob, smd->target, &tree, smd->keepDist)) {
      calc.trees.push_back(tree);
    }
  }

  if (smd->collection) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (smd->collection, col_ob) {
      BassReliefTreeData tree;

      if (col_ob->type == OB_MESH &&
          BKE_bassrelief_init_tree(ctx->depsgraph, ob, col_ob, &tree, smd->keepDist)) {
        calc.trees.push_back(tree);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  /*
  BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);
  */

  TIMEIT_BENCH(shrinkwrap_calc_normal_projection(&calc), deform_project);

  if (calc.ropt) {
    calc.ropt->solve(smd->optimizeSteps, calc.vertexCos);
    delete calc.ropt;
  }

  /* free memory */
  if (ss_mesh) {
    ss_mesh->release(ss_mesh);
  }

  /* free trees */
  for (BassReliefTreeData &tree : calc.trees) {
    BKE_bassrelief_free_tree(&tree);
  }
}
