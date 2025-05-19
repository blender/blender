/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* DX11 like EdgeDice implementation, with different tessellation factors for
 * each edge for watertight tessellation, with subpatch remapping to work with
 * DiagSplit. For more algorithm details, see the DiagSplit paper or the
 * ARB_tessellation_shader OpenGL extension, Section 2.X.2. */

#include "util/transform.h"
#include "util/types.h"

#include "subd/subpatch.h"

CCL_NAMESPACE_BEGIN

class Camera;
class Mesh;
class Patch;
class SubdAttributeInterpolation;
class DiagSplit;

struct SubdParams {
  Mesh *mesh = nullptr;
  bool ptex = false;

  int test_steps = 3;
  int split_threshold = 1;
  float dicing_rate = 1.0f;
  int max_level = 12;
  Camera *camera = nullptr;
  Transform objecttoworld = transform_identity();

  SubdParams(Mesh *mesh_, bool ptex_ = false) : mesh(mesh_), ptex(ptex_) {}
};

class EdgeDice {
 public:
  SubdParams params;
  SubdAttributeInterpolation &interpolation;
  int *mesh_triangles = nullptr;
  int *mesh_shader = nullptr;
  bool *mesh_smooth = nullptr;
  float3 *mesh_P = nullptr;
  float3 *mesh_N = nullptr;
  float *mesh_ptex_face_id = nullptr;
  float2 *mesh_ptex_uv = nullptr;

  explicit EdgeDice(const SubdParams &params,
                    const int num_verts,
                    const int num_triangles,
                    SubdAttributeInterpolation &interpolation);

  void dice(const DiagSplit &split);

 protected:
  void tri_dice(const SubPatch &sub);
  void quad_dice(const SubPatch &sub);

  void set_vertex(const SubPatch &sub, const int index, const float2 uv);
  void set_triangle(const SubPatch &sub,
                    const int triangle_index,
                    const int v0,
                    const int v1,
                    const int v2,
                    const float2 uv0,
                    const float2 uv1,
                    const float2 uv2);

  void add_grid_triangles_and_stitch(const SubPatch &sub, const int Mu, const int Mv);
  void add_triangle_strip(const SubPatch &sub, const int left_edge, const int right_edge);

  float3 eval_projected(const SubPatch &sub, const float2 uv);

  void tri_set_sides(const SubPatch &sub);
  void quad_set_sides(const SubPatch &sub);

  float quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d);
  float scale_factor(const SubPatch &sub, const int Mu, const int Mv);
};

CCL_NAMESPACE_END
