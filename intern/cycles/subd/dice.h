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

/* EdgeDice Base */

class EdgeDice {
 public:
  SubdParams params;
  float3 *mesh_P = nullptr;
  float3 *mesh_N = nullptr;
  float *mesh_ptex_face_id = nullptr;
  float2 *mesh_ptex_uv = nullptr;

  explicit EdgeDice(const SubdParams &params);

  void reserve(const int num_verts, const int num_triangles);

 protected:
  void set_vert(const Patch *patch, const int index, const float2 uv);
  void add_triangle(const Patch *patch,
                    const int v0,
                    const int v1,
                    const int v2,
                    const float2 uv0,
                    const float2 uv1,
                    const float2 uv2);

  void stitch_triangles_to_inner_grid(SubPatch &sub, const int edge, const int Mu, const int Mv);
  void stitch_triangles_across(SubPatch &sub, const int left_edge, const int right_edge);
};

/* Quad EdgeDice */

class QuadDice : public EdgeDice {
 public:
  explicit QuadDice(const SubdParams &params);

  void dice(SubPatch &sub);

 protected:
  float3 eval_projected(SubPatch &sub, const float2 uv);

  void set_vert(SubPatch &sub, const int index, const float2 uv);

  void add_grid(SubPatch &sub, const int Mu, const int Mv, const int offset);

  void set_side(SubPatch &sub, const int edge);

  float quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d);
  float scale_factor(SubPatch &sub, const int Mu, const int Mv);
};

CCL_NAMESPACE_END
