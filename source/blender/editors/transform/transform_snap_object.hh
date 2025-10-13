/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_scene_types.h"

#include "BLI_kdopbvh.hh"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "ED_transform_snap_object_context.hh"

#define MAX_CLIPPLANE_LEN 6

#define SNAP_TO_EDGE_ELEMENTS \
  (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_EDGE_ENDPOINT | SCE_SNAP_TO_EDGE_MIDPOINT | \
   SCE_SNAP_TO_EDGE_PERPENDICULAR)

struct BMEdge;
struct BMFace;
struct BMVert;
struct Depsgraph;
struct ID;
struct ListBase;
struct Object;
struct RegionView3D;
struct Scene;
struct View3D;

namespace blender::ed::transform {

struct SnapObjectContext {
  Scene *scene;

  struct SnapCache {
    virtual ~SnapCache() = default;
  };
  Map<const ID *, std::unique_ptr<SnapCache>> editmesh_caches;

  /* Filter data, returns true to check this value. */
  struct {
    struct {
      bool (*test_vert_fn)(BMVert *, void *user_data);
      bool (*test_edge_fn)(BMEdge *, void *user_data);
      bool (*test_face_fn)(BMFace *, void *user_data);
      void *user_data;
    } edit_mesh;
  } callbacks;

  struct {
    /* Compare with #RegionView3D::persmat to update. */
    float4x4 persmat;
    float4 planes[4];
    float size;
    bool use_init_co;
  } grid;

  struct {
    Depsgraph *depsgraph;
    const RegionView3D *rv3d;
    const View3D *v3d;

    eSnapMode snap_to_flag;
    SnapObjectParams params;

    float3 ray_start;
    float3 ray_dir;

    float3 init_co;
    float3 curr_co;

    float2 win_size; /* Win x and y. */
    float2 mval;

    Vector<float4, MAX_CLIPPLANE_LEN> clip_planes;
    float4 occlusion_plane;
    float4 occlusion_plane_in_front;

    /* Read/write. */
    uint object_index;
    /* List of #SnapObjectHitDepth (caller must free). */
    ListBase *hit_list;

    eSnapOcclusionTest occlusion_test_edit;

    bool has_occlusion_plane;
    bool has_occlusion_plane_in_front;
  } runtime;

  /* Output. */
  struct Output {
    /* Location of snapped point on target surface. */
    float3 loc;
    /* Normal of snapped point on target surface. */
    float3 no;
    /* Index of snapped element on target object (-1 when no valid index is found). */
    int index;
    /* Matrix of target object (may not be #Object.object_to_world with dupli-instances). */
    float4x4 obmat;
    /* Snapped object. */
    const Object *ob;
    /* Snapped data. */
    const ID *data;

    float ray_depth_max;
    float ray_depth_max_in_front;
    union {
      float dist_px_sq;
      float dist_nearest_sq;
    };
  } ret;
};

struct RayCastAll_Data {
  void *bvhdata;

  /* Internal vars for adding depths. */
  BVHTree_RayCastCallback raycast_callback;

  const float4x4 *obmat;

  float len_diff;
  float local_scale;

  uint ob_uuid;

  /* Output data. */
  ListBase *hit_list;
};

class SnapData {
 public:
  /* Read-only. */
  DistProjectedAABBPrecalc nearest_precalc;
  Vector<float4, MAX_CLIPPLANE_LEN + 1> clip_planes;
  float4x4 pmat_local;
  float4x4 obmat_;
  const bool is_persp;
  bool use_backface_culling;

  /* Read and write. */
  BVHTreeNearest nearest_point;

  /* Constructor. */
  SnapData(SnapObjectContext *sctx, const float4x4 &obmat = float4x4::identity());

  void clip_planes_enable(SnapObjectContext *sctx,
                          const Object *ob_eval,
                          bool skip_occlusion_plane = false);
  bool snap_boundbox(const float3 &min, const float3 &max);
  bool snap_point(const float3 &co, int index = -1);
  bool snap_edge(const float3 &va, const float3 &vb, int edge_index = -1);
  eSnapMode snap_edge_points_impl(SnapObjectContext *sctx, int edge_index, float dist_px_sq_orig);
  static void register_result(SnapObjectContext *sctx,
                              const Object *ob_eval,
                              const ID *id_eval,
                              const float4x4 &obmat,
                              BVHTreeNearest *r_nearest);
  void register_result(SnapObjectContext *sctx, const Object *ob_eval, const ID *id_eval);
  static void register_result_raycast(SnapObjectContext *sctx,
                                      const Object *ob_eval,
                                      const ID *id_eval,
                                      const float4x4 &obmat,
                                      const BVHTreeRayHit *hit,
                                      const bool is_in_front);

  virtual void get_vert_co(const int /*index*/, const float ** /*r_co*/) {};
  virtual void get_edge_verts_index(const int /*index*/, int /*r_v_index*/[2]) {};
  virtual void copy_vert_no(const int /*index*/, float /*r_no*/[3]) {};
};

/* `transform_snap_object.cc` */

void raycast_all_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit);

bool raycast_tri_backface_culling_test(
    const float dir[3], const float v0[3], const float v1[3], const float v2[3], float no[3]);

void cb_snap_vert(void *userdata,
                  int index,
                  const DistProjectedAABBPrecalc *precalc,
                  const float (*clip_plane)[4],
                  const int clip_plane_len,
                  BVHTreeNearest *nearest);

void cb_snap_edge(void *userdata,
                  int index,
                  const DistProjectedAABBPrecalc *precalc,
                  const float (*clip_plane)[4],
                  const int clip_plane_len,
                  BVHTreeNearest *nearest);

bool nearest_world_tree(SnapObjectContext *sctx,
                        const BVHTree *tree,
                        BVHTree_NearestPointCallback nearest_cb,
                        const float4x4 &obmat,
                        void *treedata,
                        BVHTreeNearest *r_nearest);

eSnapMode snap_object_center(SnapObjectContext *sctx,
                             const Object *ob_eval,
                             const float4x4 &obmat,
                             eSnapMode snap_to_flag);

/* `transform_snap_object_armature.cc` */

eSnapMode snapArmature(SnapObjectContext *sctx,
                       const Object *ob_eval,
                       const float4x4 &obmat,
                       bool is_object_active);

/* `transform_snap_object_camera.cc` */

eSnapMode snapCamera(SnapObjectContext *sctx,
                     const Object *object,
                     const float4x4 &obmat,
                     eSnapMode snap_to_flag);

/* `transform_snap_object_curve.cc` */

eSnapMode snapCurve(SnapObjectContext *sctx, const Object *ob_eval, const float4x4 &obmat);

/* `transform_snap_object_editmesh.cc` */

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               const Object *ob_eval,
                               const ID *id,
                               const float4x4 &obmat,
                               eSnapMode snap_to_flag,
                               bool use_hide);

/* `transform_snap_object_mesh.cc` */

eSnapMode snap_object_mesh(SnapObjectContext *sctx,
                           const Object *ob_eval,
                           const ID *id,
                           const float4x4 &obmat,
                           eSnapMode snap_to_flag,
                           bool skip_hidden,
                           bool is_editmesh = false);

eSnapMode snap_polygon_mesh(SnapObjectContext *sctx,
                            const Object *ob_eval,
                            const ID *id,
                            const float4x4 &obmat,
                            eSnapMode snap_to_flag,
                            int face_index);

eSnapMode snap_edge_points_mesh(SnapObjectContext *sctx,
                                const Object *ob_eval,
                                const ID *id,
                                const float4x4 &obmat,
                                float dist_px_sq_orig,
                                int edge_index);

}  // namespace blender::ed::transform
