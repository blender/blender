/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#define MAX_CLIPPLANE_LEN 3

#define SNAP_TO_EDGE_ELEMENTS \
  (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_EDGE_ENDPOINT | SCE_SNAP_TO_EDGE_MIDPOINT | \
   SCE_SNAP_TO_EDGE_PERPENDICULAR)

struct SnapCache_EditMesh;

struct SnapObjectContext {
  struct Scene *scene;

  blender::Map<const BMEditMesh *, std::unique_ptr<SnapCache_EditMesh>> editmesh_caches;

  /* Filter data, returns true to check this value */
  struct {
    struct {
      bool (*test_vert_fn)(BMVert *, void *user_data);
      bool (*test_edge_fn)(BMEdge *, void *user_data);
      bool (*test_face_fn)(BMFace *, void *user_data);
      void *user_data;
    } edit_mesh;
  } callbacks;

  struct {
    Depsgraph *depsgraph;
    const RegionView3D *rv3d;
    const View3D *v3d;

    eSnapMode snap_to_flag;
    SnapObjectParams params;

    float ray_start[3];
    float ray_dir[3];
    float mval[2];

    float init_co[3];
    float curr_co[3];

    float win_size[2]; /* win x and y */
    float clip_plane[MAX_CLIPPLANE_LEN][4];
    int clip_plane_len;

    /* read/write */
    uint object_index;

    bool has_occlusion_plane; /* Ignore plane of occlusion in curves. */
    bool use_occlusion_test_edit;
  } runtime;

  /* Output. */
  struct {
    /* Location of snapped point on target surface. */
    float loc[3];
    /* Normal of snapped point on target surface. */
    float no[3];
    /* Index of snapped element on target object (-1 when no valid index is found). */
    int index;
    /* Matrix of target object (may not be #Object.object_to_world with dupli-instances). */
    float obmat[4][4];
    /* List of #SnapObjectHitDepth (caller must free). */
    ListBase *hit_list;
    /* Snapped object. */
    Object *ob;
    /* Snapped data. */
    const ID *data;

    float ray_depth_max;
    float dist_px_sq;
  } ret;
};

struct RayCastAll_Data {
  void *bvhdata;

  /* internal vars for adding depths */
  BVHTree_RayCastCallback raycast_callback;

  const float (*obmat)[4];

  float len_diff;
  float local_scale;

  uint ob_uuid;

  /* output data */
  ListBase *hit_list;
};

class SnapData {
 public:
  /* Read-only. */
  DistProjectedAABBPrecalc nearest_precalc;
  blender::Vector<blender::float4, MAX_CLIPPLANE_LEN> clip_planes;
  blender::float4x4 pmat_local;
  blender::float4x4 obmat_;
  const bool is_persp;
  const bool use_backface_culling;

  /* Read and write. */
  BVHTreeNearest nearest_point;

 public:
  /* Constructor. */
  SnapData(SnapObjectContext *sctx,
           const blender::float4x4 &obmat = blender::float4x4::identity());

  void clip_planes_enable(SnapObjectContext *sctx, bool skip_occlusion_plane = false);
  bool snap_boundbox(const blender::float3 &min, const blender::float3 &max);
  bool snap_point(const blender::float3 &co, int index = -1);
  bool snap_edge(const blender::float3 &va, const blender::float3 &vb, int edge_index = -1);
  eSnapMode snap_edge_points_impl(SnapObjectContext *sctx, int edge_index, float dist_px_sq_orig);
  void register_result(SnapObjectContext *sctx, Object *ob_eval, const ID *id_eval);

  virtual void get_vert_co(const int /*index*/, const float ** /*r_co*/){};
  virtual void get_edge_verts_index(const int /*index*/, int /*r_v_index*/[2]){};
  virtual void get_tri_verts_index(const int /*index*/, int /*r_v_index*/[3]){};
  virtual void get_tri_edges_index(const int /*index*/, int /*r_e_index*/[3]){};
  virtual void copy_vert_no(const int /*index*/, float /*r_no*/[3]){};
};

/* transform_snap_object.cc */

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
                        BVHTree *tree,
                        BVHTree_NearestPointCallback nearest_cb,
                        void *treedata,
                        const float (*obmat)[4]);

eSnapMode snap_object_center(SnapObjectContext *sctx,
                             Object *ob_eval,
                             const float obmat[4][4],
                             eSnapMode snap_to_flag);

/* transform_snap_object_armature.cc */

eSnapMode snapArmature(SnapObjectContext *sctx,
                       Object *ob_eval,
                       const float obmat[4][4],
                       bool is_object_active);

/* transform_snap_object_camera.cc */

eSnapMode snapCamera(SnapObjectContext *sctx,
                     Object *object,
                     const float obmat[4][4],
                     eSnapMode snap_to_flag);

/* transform_snap_object_curve.cc */

eSnapMode snapCurve(SnapObjectContext *sctx, Object *ob_eval, const float obmat[4][4]);

/* transform_snap_object_editmesh.cc */

struct SnapCache_EditMesh {
  /* Loose Verts, Edges. */
  BVHTree *bvhtree[2];
  bool cached[2];

  /* BVH tree from #BMEditMesh.looptris. */
  BVHTreeFromEditMesh treedata_editmesh;

  blender::bke::MeshRuntime *mesh_runtime;
  float min[3], max[3];

  void clear();

  ~SnapCache_EditMesh()
  {
    this->clear();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("SnapData_EditMesh")
#endif
};

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               Object *ob_eval,
                               const ID *id,
                               const float obmat[4][4],
                               eSnapMode snap_to_flag,
                               bool use_hide);

eSnapMode snap_polygon_editmesh(SnapObjectContext *sctx,
                                Object *ob_eval,
                                const ID *id,
                                const float obmat[4][4],
                                eSnapMode snap_to_flag,
                                int polygon);

eSnapMode snap_edge_points_editmesh(SnapObjectContext *sctx,
                                    Object *ob_eval,
                                    const ID *id,
                                    const float obmat[4][4],
                                    float dist_px_sq_orig,
                                    int edge);

/* transform_snap_object_mesh.cc */

eSnapMode snap_object_mesh(SnapObjectContext *sctx,
                           Object *ob_eval,
                           const ID *id,
                           const float obmat[4][4],
                           eSnapMode snap_to_flag,
                           bool use_hide);

eSnapMode snap_polygon_mesh(SnapObjectContext *sctx,
                            Object *ob_eval,
                            const ID *id,
                            const float obmat[4][4],
                            eSnapMode snap_to_flag,
                            int polygon);

eSnapMode snap_edge_points_mesh(SnapObjectContext *sctx,
                                Object *ob_eval,
                                const ID *id,
                                const float obmat[4][4],
                                float dist_px_sq_orig,
                                int edge);
