/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__
#define __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__

struct BMEdge;
struct BMFace;
struct BMVert;

struct ARegion;
struct Depsgraph;
struct ListBase;
struct Main;
struct Object;
struct Scene;
struct View3D;
struct ViewLayer;
struct bContext;

/* transform_snap_object.c */

/* ED_transform_snap_object_*** API */

typedef enum eSnapSelect {
  SNAP_ALL = 0,
  SNAP_NOT_SELECTED = 1,
  SNAP_NOT_ACTIVE = 2,
} eSnapSelect;

/** used for storing multiple hits */
struct SnapObjectHitDepth {
  struct SnapObjectHitDepth *next, *prev;

  float depth;
  float co[3];
  float no[3];
  int index;

  struct Object *ob;
  float obmat[4][4];

  /* needed to tell which ray-cast this was part of,
   * the same object may be part of many ray-casts when dupli's are used. */
  unsigned int ob_uuid;
};

/** parameters that define which objects will be used to snap. */
struct SnapObjectParams {
  /* special context sensitive handling for the active or selected object */
  char snap_select;
  /* use editmode cage */
  unsigned int use_object_edit_cage : 1;
  /* snap to the closest element, use when using more than one snap type */
  unsigned int use_occlusion_test : 1;
};

typedef struct SnapObjectContext SnapObjectContext;
SnapObjectContext *ED_transform_snap_object_context_create(struct Main *bmain,
                                                           struct Scene *scene,
                                                           struct Depsgraph *depsgraph,
                                                           int flag);
SnapObjectContext *ED_transform_snap_object_context_create_view3d(struct Main *bmain,
                                                                  struct Scene *scene,
                                                                  struct Depsgraph *depsgraph,
                                                                  int flag,
                                                                  /* extra args for view3d */
                                                                  const struct ARegion *ar,
                                                                  const struct View3D *v3d);
void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx);

/* callbacks to filter how snap works */
void ED_transform_snap_object_context_set_editmesh_callbacks(
    SnapObjectContext *sctx,
    bool (*test_vert_fn)(struct BMVert *, void *user_data),
    bool (*test_edge_fn)(struct BMEdge *, void *user_data),
    bool (*test_face_fn)(struct BMFace *, void *user_data),
    void *user_data);

bool ED_transform_snap_object_project_ray_ex(struct SnapObjectContext *sctx,
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
                                          const struct SnapObjectParams *params,
                                          const float ray_origin[3],
                                          const float ray_direction[3],
                                          float *ray_depth,
                                          float r_co[3],
                                          float r_no[3]);

bool ED_transform_snap_object_project_ray_all(SnapObjectContext *sctx,
                                              const struct SnapObjectParams *params,
                                              const float ray_start[3],
                                              const float ray_normal[3],
                                              float ray_depth,
                                              bool sort,
                                              struct ListBase *r_hit_list);

short ED_transform_snap_object_project_view3d_ex(struct SnapObjectContext *sctx,
                                                const unsigned short snap_to,
                                                const struct SnapObjectParams *params,
                                                const float mval[2],
                                                float *dist_px,
                                                float r_loc[3],
                                                float r_no[3],
                                                int *r_index,
                                                struct Object **r_ob,
                                                float r_obmat[4][4]);
bool ED_transform_snap_object_project_view3d(struct SnapObjectContext *sctx,
                                             const unsigned short snap_to,
                                             const struct SnapObjectParams *params,
                                             const float mval[2],
                                             float *dist_px,
                                             /* return args */
                                             float r_loc[3],
                                             float r_no[3]);

bool ED_transform_snap_object_project_all_view3d_ex(SnapObjectContext *sctx,
                                                    const struct SnapObjectParams *params,
                                                    const float mval[2],
                                                    float ray_depth,
                                                    bool sort,
                                                    ListBase *r_hit_list);

#endif /* __ED_TRANSFORM_SNAP_OBJECT_CONTEXT_H__ */
