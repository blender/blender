/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __SUBD_DICE_H__
#define __SUBD_DICE_H__

/* DX11 like EdgeDice implementation, with different tessellation factors for
 * each edge for watertight tessellation, with subpatch remapping to work with
 * DiagSplit. For more algorithm details, see the DiagSplit paper or the
 * ARB_tessellation_shader OpenGL extension, Section 2.X.2. */

#include "util/types.h"
#include "util/vector.h"

#include "subd/subpatch.h"

CCL_NAMESPACE_BEGIN

class Camera;
class Mesh;
class Patch;

struct SubdParams {
  Mesh *mesh;
  bool ptex;

  int test_steps;
  int split_threshold;
  float dicing_rate;
  int max_level;
  Camera *camera;
  Transform objecttoworld;

  SubdParams(Mesh *mesh_, bool ptex_ = false)
  {
    mesh = mesh_;
    ptex = ptex_;

    test_steps = 3;
    split_threshold = 1;
    dicing_rate = 1.0f;
    max_level = 12;
    camera = NULL;
  }
};

/* EdgeDice Base */

class EdgeDice {
 public:
  SubdParams params;
  float3 *mesh_P;
  float3 *mesh_N;
  size_t vert_offset;
  size_t tri_offset;

  explicit EdgeDice(const SubdParams &params);

  void reserve(int num_verts, int num_triangles);

  void set_vert(Patch *patch, int index, float2 uv);
  void add_triangle(Patch *patch, int v0, int v1, int v2);

  void stitch_triangles(Subpatch &sub, int edge);
};

/* Quad EdgeDice */

class QuadDice : public EdgeDice {
 public:
  explicit QuadDice(const SubdParams &params);

  float3 eval_projected(Subpatch &sub, float u, float v);

  float2 map_uv(Subpatch &sub, float u, float v);
  void set_vert(Subpatch &sub, int index, float u, float v);

  void add_grid(Subpatch &sub, int Mu, int Mv, int offset);

  void set_side(Subpatch &sub, int edge);

  float quad_area(const float3 &a, const float3 &b, const float3 &c, const float3 &d);
  float scale_factor(Subpatch &sub, int Mu, int Mv);

  void dice(Subpatch &sub);
};

CCL_NAMESPACE_END

#endif /* __SUBD_DICE_H__ */
