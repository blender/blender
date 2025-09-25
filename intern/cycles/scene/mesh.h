/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"

#include "scene/attribute.h"
#include "scene/geometry.h"
#include "scene/shader.h"

#include "subd/dice.h"

#include "util/array.h"
#include "util/boundbox.h"
#include "util/param.h"
#include "util/set.h"
#include "util/types.h"
#include "util/unique_ptr.h"

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
                      const size_t num_verts,
                      const size_t num_steps,
                      const float time,
                      float3 r_verts[3]) const;

    void verts_for_step(const float3 *verts,
                        const float3 *vert_steps,
                        const size_t num_verts,
                        const size_t num_steps,
                        const size_t step,
                        float3 r_verts[3]) const;

    float3 compute_normal(const float3 *verts) const;

    bool valid(const float3 *verts) const;
  };

  Triangle get_triangle(const size_t i) const
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

  SubdEdgeCrease get_subd_crease(const size_t i) const
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

  enum SubdivisionBoundaryInterpolation {
    SUBDIVISION_BOUNDARY_NONE,
    SUBDIVISION_BOUNDARY_EDGE_ONLY,
    SUBDIVISION_BOUNDARY_EDGE_AND_CORNER,
  };

  enum SubdivisionFVarInterpolation {
    SUBDIVISION_FVAR_LINEAR_NONE,
    SUBDIVISION_FVAR_LINEAR_CORNERS_ONLY,
    SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS1,
    SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS2,
    SUBDIVISION_FVAR_LINEAR_BOUNDARIES,
    SUBDIVISION_FVAR_LINEAR_ALL,
  };

  enum SubdivisionAdaptiveSpace {
    SUBDIVISION_ADAPTIVE_SPACE_PIXEL,
    SUBDIVISION_ADAPTIVE_SPACE_OBJECT,
  };

  NODE_SOCKET_API(SubdivisionType, subdivision_type)
  NODE_SOCKET_API(SubdivisionBoundaryInterpolation, subdivision_boundary_interpolation)
  NODE_SOCKET_API(SubdivisionFVarInterpolation, subdivision_fvar_interpolation)

  /* Mesh Data */
  NODE_SOCKET_API_ARRAY(array<int>, triangles)
  NODE_SOCKET_API_ARRAY(array<float3>, verts)
  NODE_SOCKET_API_ARRAY(array<int>, shader)
  NODE_SOCKET_API_ARRAY(array<bool>, smooth)

  /* SubdFaces */
  NODE_SOCKET_API_ARRAY(array<int>, subd_start_corner)
  NODE_SOCKET_API_ARRAY(array<int>, subd_num_corners)
  NODE_SOCKET_API_ARRAY(array<int>, subd_shader)
  NODE_SOCKET_API_ARRAY(array<bool>, subd_smooth)
  NODE_SOCKET_API_ARRAY(array<int>, subd_ptex_offset)

  NODE_SOCKET_API_ARRAY(array<int>, subd_face_corners)

  NODE_SOCKET_API_ARRAY(array<int>, subd_creases_edge)
  NODE_SOCKET_API_ARRAY(array<float>, subd_creases_weight)

  NODE_SOCKET_API_ARRAY(array<int>, subd_vert_creases)
  NODE_SOCKET_API_ARRAY(array<float>, subd_vert_creases_weight)

  /* Subdivisions parameters */
  NODE_SOCKET_API(SubdivisionAdaptiveSpace, subd_adaptive_space)
  NODE_SOCKET_API(float, subd_dicing_rate)
  NODE_SOCKET_API(int, subd_max_level)
  NODE_SOCKET_API(Transform, subd_objecttoworld)

  AttributeSet subd_attributes;

  /* BVH */
  size_t vert_offset;

  size_t face_offset;
  size_t corner_offset;

 private:
  size_t num_subd_added_verts;
  size_t num_subd_faces;

  friend class BVH2;
  friend class BVHBuild;
  friend class BVHSpatialSplit;
  friend class DiagSplit;
  friend class EdgeDice;
  friend class GeometryManager;
  friend class ObjectManager;

  unique_ptr<SubdParams> subd_params;

 public:
  /* Functions */
  Mesh();

  void resize_mesh(const int numverts, const int numtris);
  void reserve_mesh(const int numverts, const int numtris);
  void resize_subd_faces(const int numfaces, const int numcorners);
  void reserve_subd_faces(const int numfaces, const int numcorners);
  void reserve_subd_creases(const size_t num_creases);
  void clear_non_sockets();
  void clear(bool preserve_shaders = false) override;
  void add_vertex(const float3 P);
  void add_vertex_slow(const float3 P);
  void add_triangle(const int v0, const int v1, const int v2, const int shader, bool smooth);
  void add_subd_face(const int *corners, const int num_corners, const int shader_, bool smooth_);
  void add_edge_crease(const int v0, const int v1, const float weight);
  void add_vertex_crease(const int v, const float weight);

  void copy_center_to_motion_step(const int motion_step);

  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;
  void add_vertex_normals();
  void add_undisplaced(Scene *scene);
  void update_generated(Scene *scene);
  void update_tangents(Scene *scene, bool undisplaced);

  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  void pack_shaders(Scene *scene, uint *shader);
  void pack_normals(packed_float3 *vnormal);
  void pack_verts(packed_float3 *tri_verts, packed_uint3 *tri_vindex);

  bool has_motion_blur() const override;
  PrimitiveType primitive_type() const override;

  void tessellate(SubdParams &params);

  SubdFace get_subd_face(const size_t index) const;
  size_t get_num_subd_faces() const
  {
    return num_subd_faces;
  }
  void set_num_subd_faces(const size_t num_subd_faces_)
  {
    num_subd_faces = num_subd_faces_;
  }
  size_t get_num_subd_base_verts() const
  {
    return verts.size() - num_subd_added_verts;
  }

 protected:
  void clear(bool preserve_shaders, bool preserve_voxel_data);
};

CCL_NAMESPACE_END
