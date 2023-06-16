/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Dynamic remesher for PBVH
 */

#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "BLI_math.h"
#include "BLI_math_vector_types.hh"

#define DYNTOPO_CD_INTERP
#define DYNTOPO_DYNAMIC_TESS

namespace blender::bke::dyntopo {

float dist_to_tri_sphere_simple(float p[3], float v1[3], float v2[3], float v3[3], float n[3]);

struct BrushTester {
  bool is_sphere_or_tube;

  virtual ~BrushTester() {}

  virtual bool vert_in_range(BMVert * /*v*/)
  {
    return true;
  }
  virtual bool tri_in_range(BMVert * /*tri*/[3], float * /*no*/)
  {
    return true;
  }
};

struct BrushSphere : public BrushTester {
  BrushSphere(float3 center, float radius)
      : center_(center), radius_(radius), radius_squared_(radius * radius)
  {
    is_sphere_or_tube = true;
  }

  bool vert_in_range(BMVert *v) override
  {
    return len_squared_v3v3(center_, v->co) <= radius_squared_;
  }
  bool tri_in_range(BMVert *tri[3], float *no) override
  {
    /* Check if triangle intersects the sphere */
    float dis = dist_to_tri_sphere_simple((float *)center_,
                                          (float *)tri[0]->co,
                                          (float *)tri[1]->co,
                                          (float *)tri[2]->co,
                                          (float *)no);
    return dis <= radius_squared_;
  }

  inline const float3 &center()
  {
    return center_;
  }
  inline float radius_squared()
  {
    return radius_squared_;
  }

  inline float radius()
  {
    return radius_;
  }

 protected:
  float3 center_;
  float radius_, radius_squared_;
};

struct BrushTube : public BrushSphere {
  BrushTube(float3 center, float3 view_normal, float radius)
      : BrushSphere(center, radius), view_normal_(view_normal)
  {
    project_plane_normalized_v3_v3v3(center_proj_, center_, view_normal_);
  }

  bool vert_in_range(BMVert *v) override
  {
    float c[3];

    project_plane_normalized_v3_v3v3(c, v->co, view_normal_);

    return len_squared_v3v3(center_proj_, c) <= radius_squared_;
  }

  bool tri_in_range(BMVert *tri[3], float * /*no*/) override
  {
    float c[3];
    float tri_proj[3][3];

    project_plane_normalized_v3_v3v3(tri_proj[0], tri[0]->co, view_normal_);
    project_plane_normalized_v3_v3v3(tri_proj[1], tri[1]->co, view_normal_);
    project_plane_normalized_v3_v3v3(tri_proj[2], tri[2]->co, view_normal_);

    closest_on_tri_to_point_v3(c, center_proj_, tri_proj[0], tri_proj[1], tri_proj[2]);

    /* Check if triangle intersects the sphere */
    return len_squared_v3v3(center_proj_, c) <= radius_squared_;
  }

 private:
  float3 center_proj_, view_normal_;
};

struct BrushNoRadius : public BrushTester {
  BrushNoRadius()
  {
    is_sphere_or_tube = false;
  }
};

typedef float (*DyntopoMaskCB)(PBVHVertRef vertex, void *userdata);

enum PBVHTopologyUpdateMode {
  PBVH_None = 0,
  PBVH_Subdivide = 1 << 0,
  PBVH_Collapse = 1 << 1,
  PBVH_Cleanup = 1 << 2,  // dissolve verts surrounded by either 3 or 4 triangles then triangulate
  PBVH_LocalSubdivide = 1 << 3,
  PBVH_LocalCollapse = 1 << 4
};
ENUM_OPERATORS(PBVHTopologyUpdateMode, PBVH_LocalCollapse);

void detail_size_set(PBVH *pbvh, float detail_size, float detail_range);

bool remesh_topology(blender::bke::dyntopo::BrushTester *brush_tester,
                     struct Object *ob,
                     PBVH *pbvh,
                     PBVHTopologyUpdateMode mode,
                     bool use_frontface,
                     blender::float3 view_normal,
                     bool updatePBVH,
                     DyntopoMaskCB mask_cb,
                     void *mask_cb_data,
                     float quality);

bool remesh_topology_nodes(blender::bke::dyntopo::BrushTester *tester,
                           struct Object *ob,
                           PBVH *pbvh,
                           bool (*searchcb)(PBVHNode *node, void *data),
                           void (*undopush)(PBVHNode *node, void *data),
                           void *searchdata,
                           PBVHTopologyUpdateMode mode,
                           bool use_frontface,
                           blender::float3 view_normal,
                           bool updatePBVH,
                           DyntopoMaskCB mask_cb,
                           void *mask_cb_data,
                           float quality);

void after_stroke(PBVH *pbvh, bool force_balance);
}  // namespace blender::bke::dyntopo
