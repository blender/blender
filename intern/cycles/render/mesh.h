/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MESH_H__
#define __MESH_H__

#include "graph/node.h"

#include "bvh/bvh_params.h"
#include "render/attribute.h"
#include "render/geometry.h"
#include "render/shader.h"

#include "util/util_array.h"
#include "util/util_boundbox.h"
#include "util/util_list.h"
#include "util/util_map.h"
#include "util/util_param.h"
#include "util/util_set.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Attribute;
class BVH;
class Device;
class DeviceScene;
class Mesh;
class Progress;
class RenderStats;
class Scene;
class SceneParams;
class AttributeRequest;
struct SubdParams;
class DiagSplit;
struct PackedPatchTable;

/* Mesh */

class Mesh : public Geometry {
 public:
  NODE_DECLARE

  /* Mesh Triangle */
  struct Triangle {
    int v[3];

    void bounds_grow(const float3 *verts, BoundBox &bounds) const;

    void motion_verts(const float3 *verts,
                      const float3 *vert_steps,
                      size_t num_verts,
                      size_t num_steps,
                      float time,
                      float3 r_verts[3]) const;

    void verts_for_step(const float3 *verts,
                        const float3 *vert_steps,
                        size_t num_verts,
                        size_t num_steps,
                        size_t step,
                        float3 r_verts[3]) const;

    float3 compute_normal(const float3 *verts) const;

    bool valid(const float3 *verts) const;
  };

  Triangle get_triangle(size_t i) const
  {
    Triangle tri = {{triangles[i * 3 + 0], triangles[i * 3 + 1], triangles[i * 3 + 2]}};
    return tri;
  }

  size_t num_triangles() const
  {
    return triangles.size() / 3;
  }

  /* Mesh SubdFace */
  struct SubdFace {
    int start_corner;
    int num_corners;
    int shader;
    bool smooth;
    int ptex_offset;

    bool is_quad()
    {
      return num_corners == 4;
    }
    float3 normal(const Mesh *mesh) const;
    int num_ptex_faces() const
    {
      return num_corners == 4 ? 1 : num_corners;
    }
  };

  struct SubdEdgeCrease {
    int v[2];
    float crease;
  };

  enum SubdivisionType {
    SUBDIVISION_NONE,
    SUBDIVISION_LINEAR,
    SUBDIVISION_CATMULL_CLARK,
  };

  SubdivisionType subdivision_type;

  /* Mesh Data */
  array<int> triangles;
  array<float3> verts;
  array<int> shader;
  array<bool> smooth;

  /* used for storing patch info for subd triangles, only allocated if there are patches */
  array<int> triangle_patch; /* must be < 0 for non subd triangles */
  array<float2> vert_patch_uv;

  float volume_clipping;
  float volume_step_size;
  bool volume_object_space;

  array<SubdFace> subd_faces;
  array<int> subd_face_corners;
  int num_ngons;

  array<SubdEdgeCrease> subd_creases;

  SubdParams *subd_params;

  AttributeSet subd_attributes;

  PackedPatchTable *patch_table;

  /* BVH */
  size_t vert_offset;

  size_t patch_offset;
  size_t patch_table_offset;
  size_t face_offset;
  size_t corner_offset;

  size_t num_subd_verts;

 private:
  unordered_map<int, int> vert_to_stitching_key_map; /* real vert index -> stitching index */
  unordered_multimap<int, int>
      vert_stitching_map; /* stitching index -> multiple real vert indices */
  friend class DiagSplit;
  friend class GeometryManager;

 public:
  /* Functions */
  Mesh();
  ~Mesh();

  void resize_mesh(int numverts, int numfaces);
  void reserve_mesh(int numverts, int numfaces);
  void resize_subd_faces(int numfaces, int num_ngons, int numcorners);
  void reserve_subd_faces(int numfaces, int num_ngons, int numcorners);
  void clear(bool preserve_voxel_data);
  void clear() override;
  void add_vertex(float3 P);
  void add_vertex_slow(float3 P);
  void add_triangle(int v0, int v1, int v2, int shader, bool smooth);
  void add_subd_face(int *corners, int num_corners, int shader_, bool smooth_);

  void copy_center_to_motion_step(const int motion_step);

  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;
  void add_face_normals();
  void add_vertex_normals();
  void add_undisplaced();

  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  void pack_shaders(Scene *scene, uint *shader);
  void pack_normals(float4 *vnormal);
  void pack_verts(const vector<uint> &tri_prim_index,
                  uint4 *tri_vindex,
                  uint *tri_patch,
                  float2 *tri_patch_uv,
                  size_t vert_offset,
                  size_t tri_offset);
  void pack_patches(uint *patch_data, uint vert_offset, uint face_offset, uint corner_offset);

  void tessellate(DiagSplit *split);
};

CCL_NAMESPACE_END

#endif /* __MESH_H__ */
