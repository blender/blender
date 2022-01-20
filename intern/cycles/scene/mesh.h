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

#include "bvh/params.h"
#include "scene/attribute.h"
#include "scene/geometry.h"
#include "scene/shader.h"

#include "util/array.h"
#include "util/boundbox.h"
#include "util/list.h"
#include "util/map.h"
#include "util/param.h"
#include "util/set.h"
#include "util/types.h"
#include "util/vector.h"

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
 protected:
  Mesh(const NodeType *node_type_, Type geom_type_);

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

  SubdEdgeCrease get_subd_crease(size_t i) const
  {
    SubdEdgeCrease s;
    s.v[0] = subd_creases_edge[i * 2];
    s.v[1] = subd_creases_edge[i * 2 + 1];
    s.crease = subd_creases_weight[i];
    return s;
  }

  bool need_tesselation();

  enum SubdivisionType {
    SUBDIVISION_NONE,
    SUBDIVISION_LINEAR,
    SUBDIVISION_CATMULL_CLARK,
  };

  NODE_SOCKET_API(SubdivisionType, subdivision_type)

  /* Mesh Data */
  NODE_SOCKET_API_ARRAY(array<int>, triangles)
  NODE_SOCKET_API_ARRAY(array<float3>, verts)
  NODE_SOCKET_API_ARRAY(array<int>, shader)
  NODE_SOCKET_API_ARRAY(array<bool>, smooth)

  /* used for storing patch info for subd triangles, only allocated if there are patches */
  NODE_SOCKET_API_ARRAY(array<int>, triangle_patch) /* must be < 0 for non subd triangles */
  NODE_SOCKET_API_ARRAY(array<float2>, vert_patch_uv)

  /* SubdFaces */
  NODE_SOCKET_API_ARRAY(array<int>, subd_start_corner)
  NODE_SOCKET_API_ARRAY(array<int>, subd_num_corners)
  NODE_SOCKET_API_ARRAY(array<int>, subd_shader)
  NODE_SOCKET_API_ARRAY(array<bool>, subd_smooth)
  NODE_SOCKET_API_ARRAY(array<int>, subd_ptex_offset)

  NODE_SOCKET_API_ARRAY(array<int>, subd_face_corners)
  NODE_SOCKET_API(int, num_ngons)

  NODE_SOCKET_API_ARRAY(array<int>, subd_creases_edge)
  NODE_SOCKET_API_ARRAY(array<float>, subd_creases_weight)

  NODE_SOCKET_API_ARRAY(array<int>, subd_vert_creases)
  NODE_SOCKET_API_ARRAY(array<float>, subd_vert_creases_weight)

  /* Subdivisions parameters */
  NODE_SOCKET_API(float, subd_dicing_rate)
  NODE_SOCKET_API(int, subd_max_level)
  NODE_SOCKET_API(Transform, subd_objecttoworld)

  AttributeSet subd_attributes;

 private:
  PackedPatchTable *patch_table;
  /* BVH */
  size_t vert_offset;

  size_t patch_offset;
  size_t patch_table_offset;
  size_t face_offset;
  size_t corner_offset;

  size_t num_subd_verts;
  size_t num_subd_faces;

  unordered_map<int, int> vert_to_stitching_key_map; /* real vert index -> stitching index */
  unordered_multimap<int, int>
      vert_stitching_map; /* stitching index -> multiple real vert indices */

  friend class BVH2;
  friend class BVHBuild;
  friend class BVHSpatialSplit;
  friend class DiagSplit;
  friend class EdgeDice;
  friend class GeometryManager;
  friend class ObjectManager;

  SubdParams *subd_params = nullptr;

 public:
  /* Functions */
  Mesh();
  ~Mesh();

  void resize_mesh(int numverts, int numfaces);
  void reserve_mesh(int numverts, int numfaces);
  void resize_subd_faces(int numfaces, int num_ngons, int numcorners);
  void reserve_subd_faces(int numfaces, int num_ngons, int numcorners);
  void reserve_subd_creases(size_t num_creases);
  void clear_non_sockets();
  void clear(bool preserve_shaders = false) override;
  void add_vertex(float3 P);
  void add_vertex_slow(float3 P);
  void add_triangle(int v0, int v1, int v2, int shader, bool smooth);
  void add_subd_face(int *corners, int num_corners, int shader_, bool smooth_);
  void add_edge_crease(int v0, int v1, float weight);
  void add_vertex_crease(int v, float weight);

  void copy_center_to_motion_step(const int motion_step);

  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;
  void add_face_normals();
  void add_vertex_normals();
  void add_undisplaced();

  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  void pack_shaders(Scene *scene, uint *shader);
  void pack_normals(packed_float3 *vnormal);
  void pack_verts(packed_float3 *tri_verts,
                  uint4 *tri_vindex,
                  uint *tri_patch,
                  float2 *tri_patch_uv);
  void pack_patches(uint *patch_data);

  PrimitiveType primitive_type() const override;

  void tessellate(DiagSplit *split);

  SubdFace get_subd_face(size_t index) const;

  SubdParams *get_subd_params();

  size_t get_num_subd_faces() const
  {
    return num_subd_faces;
  }

  void set_num_subd_faces(size_t num_subd_faces_)
  {
    num_subd_faces = num_subd_faces_;
  }

  size_t get_num_subd_verts()
  {
    return num_subd_verts;
  }

 protected:
  void clear(bool preserve_shaders, bool preserve_voxel_data);
};

CCL_NAMESPACE_END

#endif /* __MESH_H__ */
