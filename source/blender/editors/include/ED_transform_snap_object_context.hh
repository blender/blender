/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_scene_types.h"

//#define DEBUG_SNAP_TIME

struct BMEdge;
struct BMFace;
struct BMVert;

struct ARegion;
struct Depsgraph;
struct ListBase;
struct Object;
struct Scene;
struct View3D;

/* transform_snap_object.cc */

/* ED_transform_snap_object_*** API */

typedef enum eSnapEditType {
  SNAP_GEOM_FINAL = 0,
  SNAP_GEOM_CAGE = 1,
  SNAP_GEOM_EDIT = 2, /* Bmesh for mesh-type. */
} eSnapEditType;

/** used for storing multiple hits */
struct SnapObjectHitDepth {
  struct SnapObjectHitDepth *next, *prev;

  float depth;
  float co[3];

  /* needed to tell which ray-cast this was part of,
   * the same object may be part of many ray-casts when dupli's are used. */
  unsigned int ob_uuid;
};

/** parameters that define which objects will be used to snap. */
struct SnapObjectParams {
  /* Special context sensitive handling for the active or selected object. */
  eSnapTargetOP snap_target_select;
  /* Geometry for snapping in edit mode. */
  eSnapEditType edit_mode_type;
  /* Break nearest face snapping into steps to improve transformations across U-shaped targets. */
  short face_nearest_steps;
  /* snap to the closest element, use when using more than one snap type */
  bool use_occlusion_test : 1;
  /* exclude back facing geometry from snapping */
  bool use_backface_culling : 1;
  /* Enable to force nearest face snapping to snap to target the source was initially near. */
  bool keep_on_same_target : 1;
};

typedef struct SnapObjectContext SnapObjectContext;
SnapObjectContext *ED_transform_snap_object_context_create(struct Scene *scene, int flag);
void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx);

/* callbacks to filter how snap works */
void ED_transform_snap_object_context_set_editmesh_callbacks(
    SnapObjectContext *sctx,
    bool (*test_vert_fn)(struct BMVert *, void *user_data),
    bool (*test_edge_fn)(struct BMEdge *, void *user_data),
    bool (*test_face_fn)(struct BMFace *, void *user_data),
    void *user_data);

bool ED_transform_snap_object_project_ray_ex(struct SnapObjectContext *sctx,
                                             struct Depsgraph *depsgraph,
                                             const View3D *v3d,
                                             const struct SnapObjectParams *params,
                                             const float ray_start[3],
                                             const float ray_normal[3],
                                             float *ray_depth,
                                             /* return args */
                                             float r_loc[3],
                                             float r_no[3],
                                             int *r_index,
                                             struct Object **r_ob,
                                             float r_obmat[4][4]);
bool ED_transform_snap_object_project_ray(SnapObjectContext *sctx,
                                          struct Depsgraph *depsgraph,
                                          const View3D *v3d,
                                          const struct SnapObjectParams *params,
                                          const float ray_origin[3],
                                          const float ray_direction[3],
                                          float *ray_depth,
                                          float r_co[3],
                                          float r_no[3]);

/**
 * Fill in a list of all hits.
 *
 * \param ray_depth: Only depths in this range are considered, -1.0 for maximum.
 * \param sort: Optionally sort the hits by depth.
 * \param r_hit_list: List of #SnapObjectHitDepth (caller must free).
 */
bool ED_transform_snap_object_project_ray_all(SnapObjectContext *sctx,
                                              struct Depsgraph *depsgraph,
                                              const View3D *v3d,
                                              const struct SnapObjectParams *params,
                                              const float ray_start[3],
                                              const float ray_normal[3],
                                              float ray_depth,
                                              bool sort,
                                              struct ListBase *r_hit_list);

/**
 * Perform snapping.
 *
 * Given a 2D region value, snap to vert/edge/face/grid.
 *
 * \param sctx: Snap context.
 * \param snap_to: Target elements to snap source to.
 * \param params: Addition snapping options.
 * \param init_co: Initial world-space coordinate of source (optional).
 * \param mval: Current transformed screen-space coordinate or mouse position (optional).
 * \param prev_co: Current transformed world-space coordinate of source (optional).
 * \param dist_px: Maximum distance to snap (in pixels).
 * \param r_loc: Snapped world-space coordinate.
 * \param r_no: Snapped world-space normal (optional).
 * \param r_index: Index of snapped-to target element (optional).
 * \param r_ob: Snapped-to target object (optional).
 * \param r_obmat: Matrix of snapped-to target object (optional).
 * \param r_face_nor: World-space normal of snapped-to target face (optional).
 * \return Snapped-to element, #eSnapMode.
 */
eSnapMode ED_transform_snap_object_project_view3d_ex(struct SnapObjectContext *sctx,
                                                     struct Depsgraph *depsgraph,
                                                     const ARegion *region,
                                                     const View3D *v3d,
                                                     const eSnapMode snap_to,
                                                     const struct SnapObjectParams *params,
                                                     const float init_co[3],
                                                     const float mval[2],
                                                     const float prev_co[3],
                                                     float *dist_px,
                                                     float r_loc[3],
                                                     float r_no[3],
                                                     int *r_index,
                                                     struct Object **r_ob,
                                                     float r_obmat[4][4],
                                                     float r_face_nor[3]);
/**
 * Convenience function for performing snapping.
 *
 * Given a 2D region value, snap to vert/edge/face.
 *
 * \param sctx: Snap context.
 * \param snap_to: Target elements to snap source to.
 * \param params: Addition snapping options.
 * \param init_co: Initial world-space coordinate of source (optional).
 * \param mval: Current transformed screen-space coordinate or mouse position (optional).
 * \param prev_co: Current transformed world-space coordinate of source (optional).
 * \param dist_px: Maximum distance to snap (in pixels).
 * \param r_loc: Snapped world-space coordinate.
 * \param r_no: Snapped world-space normal (optional).
 * \return Snapped-to element, #eSnapMode.
 */
eSnapMode ED_transform_snap_object_project_view3d(struct SnapObjectContext *sctx,
                                                  struct Depsgraph *depsgraph,
                                                  const ARegion *region,
                                                  const View3D *v3d,
                                                  const eSnapMode snap_to,
                                                  const struct SnapObjectParams *params,
                                                  const float init_co[3],
                                                  const float mval[2],
                                                  const float prev_co[3],
                                                  float *dist_px,
                                                  /* return args */
                                                  float r_loc[3],
                                                  float r_no[3]);

/**
 * see: #ED_transform_snap_object_project_ray_all
 */
bool ED_transform_snap_object_project_all_view3d_ex(SnapObjectContext *sctx,
                                                    struct Depsgraph *depsgraph,
                                                    const ARegion *region,
                                                    const View3D *v3d,
                                                    const struct SnapObjectParams *params,
                                                    const float mval[2],
                                                    float ray_depth,
                                                    bool sort,
                                                    ListBase *r_hit_list);

#ifdef DEBUG_SNAP_TIME
void ED_transform_snap_object_time_average_print(void);
#else
#  define ED_transform_snap_object_time_average_print() void(0)
#endif
