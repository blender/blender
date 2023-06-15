/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#define MAX_CLIPPLANE_LEN 3

/* -------------------------------------------------------------------- */
/** \name Internal Data Types
 * \{ */

/** #SnapObjectContext.editmesh_caches */
struct SnapData_EditMesh {
  /* Verts, Edges. */
  BVHTree *bvhtree[2];
  bool cached[2];

  /* BVH tree from #BMEditMesh.looptris. */
  BVHTreeFromEditMesh treedata_editmesh;

  blender::bke::MeshRuntime *mesh_runtime;
  float min[3], max[3];

  void clear()
  {
    for (int i = 0; i < ARRAY_SIZE(this->bvhtree); i++) {
      if (!this->cached[i]) {
        BLI_bvhtree_free(this->bvhtree[i]);
      }
      this->bvhtree[i] = nullptr;
    }
    free_bvhtree_from_editmesh(&this->treedata_editmesh);
  }

  ~SnapData_EditMesh()
  {
    this->clear();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("SnapData_EditMesh")
#endif
};

/** \} */

struct SnapObjectContext {
  struct Scene *scene;

  blender::Map<const BMEditMesh *, std::unique_ptr<SnapData_EditMesh>> editmesh_caches;

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
    const ARegion *region;
    const View3D *v3d;

    eSnapMode snap_to_flag;
    SnapObjectParams params;

    float ray_start[3];
    float ray_dir[3];
    float mval[2];

    float init_co[3];
    float curr_co[3];

    float pmat[4][4];  /* perspective matrix */
    float win_size[2]; /* win x and y */
    float clip_plane[MAX_CLIPPLANE_LEN][4];
    int clip_plane_len;

    /* read/write */
    uint object_index;

    bool is_persp;
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
    ID *data;

    float ray_depth_max;
    float dist_px_sq;

    bool is_edit;
  } ret;
};

struct RayCastAll_Data {
  void *bvhdata;

  /* internal vars for adding depths */
  BVHTree_RayCastCallback raycast_callback;

  const float (*obmat)[4];
  const float (*timat)[3];

  float len_diff;
  float local_scale;

  Object *ob_eval;
  uint ob_uuid;

  /* output data */
  ListBase *hit_list;
  bool retval;
};

struct Nearest2dUserData;

using Nearest2DGetVertCoCallback = void (*)(const int index,
                                            const Nearest2dUserData *data,
                                            const float **r_co);
using Nearest2DGetEdgeVertsCallback = void (*)(const int index,
                                               const Nearest2dUserData *data,
                                               int r_v_index[2]);
using Nearest2DGetTriVertsCallback = void (*)(const int index,
                                              const Nearest2dUserData *data,
                                              int r_v_index[3]);
/* Equal the previous one */
using Nearest2DGetTriEdgesCallback = void (*)(const int index,
                                              const Nearest2dUserData *data,
                                              int r_e_index[3]);
using Nearest2DCopyVertNoCallback = void (*)(const int index,
                                             const Nearest2dUserData *data,
                                             float r_no[3]);

struct Nearest2dUserData {
  Nearest2DGetVertCoCallback get_vert_co;
  Nearest2DGetEdgeVertsCallback get_edge_verts_index;
  Nearest2DGetTriVertsCallback get_tri_verts_index;
  Nearest2DGetTriEdgesCallback get_tri_edges_index;
  Nearest2DCopyVertNoCallback copy_vert_no;

  union {
    struct {
      BMesh *bm;
    };
    struct {
      const blender::float3 *vert_positions;
      const blender::float3 *vert_normals;
      const blender::int2 *edges; /* only used for #BVHTreeFromMeshEdges */
      const int *corner_verts;
      const int *corner_edges;
      const MLoopTri *looptris;
    };
  };

  bool is_persp;
  bool use_backface_culling;
};

/* transform_snap_object.cc */

void raycast_all_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit);

bool raycast_tri_backface_culling_test(
    const float dir[3], const float v0[3], const float v1[3], const float v2[3], float no[3]);

bool snap_bound_box_check_dist(const float min[3],
                               const float max[3],
                               const float lpmat[4][4],
                               const float win_size[2],
                               const float mval[2],
                               float dist_px_sq);

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
                        const float (*obmat)[4],
                        const float init_co[3],
                        const float curr_co[3],
                        float *r_dist_sq,
                        float *r_loc,
                        float *r_no,
                        int *r_index);

/* transform_snap_object_editmesh.cc */

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               Object *ob_eval,
                               ID *id,
                               const float obmat[4][4],
                               eSnapMode snap_to_flag,
                               bool use_hide);

eSnapMode snap_polygon_editmesh(SnapObjectContext *sctx,
                                Object *ob_eval,
                                ID *id,
                                const float obmat[4][4],
                                eSnapMode snap_to_flag,
                                int polygon,
                                const float clip_planes_local[MAX_CLIPPLANE_LEN][4]);

void nearest2d_data_init_editmesh(struct BMEditMesh *em,
                                  bool is_persp,
                                  bool use_backface_culling,
                                  struct Nearest2dUserData *r_nearest2d);

/* transform_snap_object_mesh.cc */

eSnapMode snap_object_mesh(SnapObjectContext *sctx,
                           Object *ob_eval,
                           ID *id,
                           const float obmat[4][4],
                           eSnapMode snap_to_flag,
                           bool use_hide);

eSnapMode snap_polygon_mesh(SnapObjectContext *sctx,
                            Object *ob_eval,
                            ID *id,
                            const float obmat[4][4],
                            eSnapMode snap_to_flag,
                            int polygon,
                            const float clip_planes_local[MAX_CLIPPLANE_LEN][4]);

void nearest2d_data_init_mesh(const Mesh *mesh,
                              bool is_persp,
                              bool use_backface_culling,
                              Nearest2dUserData *r_nearest2d);
