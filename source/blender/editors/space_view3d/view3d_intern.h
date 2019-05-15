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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 */

#ifndef __VIEW3D_INTERN_H__
#define __VIEW3D_INTERN_H__

#include "ED_view3d.h"

/* internal exports only */

struct ARegion;
struct ARegionType;
struct Base;
struct BoundBox;
struct Depsgraph;
struct GPUBatch;
struct Mesh;
struct Object;
struct SmokeDomainSettings;
struct ViewLayer;
struct bAnimVizSettings;
struct bContext;
struct bMotionPath;
struct bPoseChannel;
struct wmGizmoGroupType;
struct wmGizmoType;
struct wmKeyConfig;
struct wmOperatorType;
struct wmWindowManager;

/* drawing flags: */
enum {
  DRAW_PICKING = (1 << 0),
  DRAW_CONSTCOLOR = (1 << 1),
  DRAW_SCENESET = (1 << 2),
};

/* view3d_header.c */
void VIEW3D_OT_toggle_matcap_flip(struct wmOperatorType *ot);

/* view3d_ops.c */
void view3d_operatortypes(void);

/* view3d_edit.c */
void VIEW3D_OT_zoom(struct wmOperatorType *ot);
void VIEW3D_OT_dolly(struct wmOperatorType *ot);
void VIEW3D_OT_zoom_camera_1_to_1(struct wmOperatorType *ot);
void VIEW3D_OT_move(struct wmOperatorType *ot);
void VIEW3D_OT_rotate(struct wmOperatorType *ot);
#ifdef WITH_INPUT_NDOF
void VIEW3D_OT_ndof_orbit(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_orbit_zoom(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_pan(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_all(struct wmOperatorType *ot);
#endif /* WITH_INPUT_NDOF */
void VIEW3D_OT_view_all(struct wmOperatorType *ot);
void VIEW3D_OT_view_axis(struct wmOperatorType *ot);
void VIEW3D_OT_view_camera(struct wmOperatorType *ot);
void VIEW3D_OT_view_selected(struct wmOperatorType *ot);
void VIEW3D_OT_view_lock_clear(struct wmOperatorType *ot);
void VIEW3D_OT_view_lock_to_active(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_cursor(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_pick(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_camera(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_lock(struct wmOperatorType *ot);
void VIEW3D_OT_view_pan(struct wmOperatorType *ot);
void VIEW3D_OT_view_persportho(struct wmOperatorType *ot);
void VIEW3D_OT_navigate(struct wmOperatorType *ot);
void VIEW3D_OT_background_image_add(struct wmOperatorType *ot);
void VIEW3D_OT_background_image_remove(struct wmOperatorType *ot);
void VIEW3D_OT_view_orbit(struct wmOperatorType *ot);
void VIEW3D_OT_view_roll(struct wmOperatorType *ot);
void VIEW3D_OT_clip_border(struct wmOperatorType *ot);
void VIEW3D_OT_cursor3d(struct wmOperatorType *ot);
void VIEW3D_OT_render_border(struct wmOperatorType *ot);
void VIEW3D_OT_clear_render_border(struct wmOperatorType *ot);
void VIEW3D_OT_zoom_border(struct wmOperatorType *ot);
void VIEW3D_OT_toggle_shading(struct wmOperatorType *ot);
void VIEW3D_OT_toggle_xray(struct wmOperatorType *ot);

void view3d_boxview_copy(struct ScrArea *sa, struct ARegion *ar);
void view3d_boxview_sync(struct ScrArea *sa, struct ARegion *ar);

void view3d_orbit_apply_dyn_ofs(float r_ofs[3],
                                const float ofs_old[3],
                                const float viewquat_old[4],
                                const float viewquat_new[4],
                                const float dyn_ofs[3]);

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;

void view3d_ndof_fly(const struct wmNDOFMotionData *ndof,
                     struct View3D *v3d,
                     struct RegionView3D *rv3d,
                     const bool use_precision,
                     const short protectflag,
                     bool *r_has_translate,
                     bool *r_has_rotate);
#endif /* WITH_INPUT_NDOF */

/* view3d_fly.c */
void view3d_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_fly(struct wmOperatorType *ot);

/* view3d_walk.c */
void VIEW3D_OT_walk(struct wmOperatorType *ot);

/* drawobject.c */
int view3d_effective_drawtype(const struct View3D *v3d);

/* view3d_draw.c */
void view3d_main_region_draw(const struct bContext *C, struct ARegion *ar);
void view3d_draw_region_info(const struct bContext *C, struct ARegion *ar);

void ED_view3d_draw_depth(struct Depsgraph *depsgraph,
                          struct ARegion *ar,
                          View3D *v3d,
                          bool alphaoverride);

/* view3d_draw_legacy.c */
void ED_view3d_draw_depth_gpencil(struct Depsgraph *depsgraph,
                                  Scene *scene,
                                  struct ARegion *ar,
                                  View3D *v3d);

void ED_view3d_draw_select_loop(struct Depsgraph *depsgraph,
                                ViewContext *vc,
                                Scene *scene,
                                struct ViewLayer *view_layer,
                                View3D *v3d,
                                struct ARegion *ar,
                                bool use_obedit_skip,
                                bool use_nearest);

void ED_view3d_draw_depth_loop(struct Depsgraph *depsgraph,
                               Scene *scene,
                               struct ARegion *ar,
                               View3D *v3d);

void view3d_update_depths_rect(struct ARegion *ar, struct ViewDepths *d, struct rcti *rect);
float view3d_depth_near(struct ViewDepths *d);

/* view3d_select.c */
void VIEW3D_OT_select(struct wmOperatorType *ot);
void VIEW3D_OT_select_circle(struct wmOperatorType *ot);
void VIEW3D_OT_select_box(struct wmOperatorType *ot);
void VIEW3D_OT_select_lasso(struct wmOperatorType *ot);
void VIEW3D_OT_select_menu(struct wmOperatorType *ot);

/* view3d_view.c */
void VIEW3D_OT_smoothview(struct wmOperatorType *ot);
void VIEW3D_OT_camera_to_view(struct wmOperatorType *ot);
void VIEW3D_OT_camera_to_view_selected(struct wmOperatorType *ot);
void VIEW3D_OT_object_as_camera(struct wmOperatorType *ot);
void VIEW3D_OT_localview(struct wmOperatorType *ot);
void VIEW3D_OT_localview_remove_from(struct wmOperatorType *ot);

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d,
                                const struct BoundBox *bb,
                                float obmat[4][4]);
bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const struct BoundBox *bb);

typedef struct V3D_SmoothParams {
  struct Object *camera_old, *camera;
  const float *ofs, *quat, *dist, *lens;
  /* alternate rotation center (ofs = must be NULL) */
  const float *dyn_ofs;
} V3D_SmoothParams;

void ED_view3d_smooth_view_ex(const struct Depsgraph *depsgraph,
                              struct wmWindowManager *wm,
                              struct wmWindow *win,
                              struct ScrArea *sa,
                              struct View3D *v3d,
                              struct ARegion *ar,
                              const int smooth_viewtx,
                              const V3D_SmoothParams *sview);

void ED_view3d_smooth_view(struct bContext *C,
                           struct View3D *v3d,
                           struct ARegion *ar,
                           const int smooth_viewtx,
                           const V3D_SmoothParams *sview);

void ED_view3d_smooth_view_force_finish(struct bContext *C,
                                        struct View3D *v3d,
                                        struct ARegion *ar);

void view3d_winmatrix_set(struct Depsgraph *depsgraph,
                          struct ARegion *ar,
                          const View3D *v3d,
                          const rcti *rect);
void view3d_viewmatrix_set(struct Depsgraph *depsgraph,
                           Scene *scene,
                           const View3D *v3d,
                           RegionView3D *rv3d,
                           const float rect_scale[2]);

void fly_modal_keymap(struct wmKeyConfig *keyconf);
void walk_modal_keymap(struct wmKeyConfig *keyconf);
void viewrotate_modal_keymap(struct wmKeyConfig *keyconf);
void viewmove_modal_keymap(struct wmKeyConfig *keyconf);
void viewzoom_modal_keymap(struct wmKeyConfig *keyconf);
void viewdolly_modal_keymap(struct wmKeyConfig *keyconf);

/* view3d_buttons.c */
void VIEW3D_OT_object_mode_pie_or_toggle(struct wmOperatorType *ot);
void view3d_buttons_register(struct ARegionType *art);

/* view3d_camera_control.c */
struct View3DCameraControl *ED_view3d_cameracontrol_acquire(struct Depsgraph *depsgraph,
                                                            Scene *scene,
                                                            View3D *v3d,
                                                            RegionView3D *rv3d,
                                                            const bool use_parent_root);
void ED_view3d_cameracontrol_update(struct View3DCameraControl *vctrl,
                                    const bool use_autokey,
                                    struct bContext *C,
                                    const bool do_rotate,
                                    const bool do_translate);
void ED_view3d_cameracontrol_release(struct View3DCameraControl *vctrl, const bool restore);
struct Object *ED_view3d_cameracontrol_object_get(struct View3DCameraControl *vctrl);

/* view3d_toolbar.c */
void VIEW3D_OT_toolshelf(struct wmOperatorType *ot);

/* view3d_snap.c */
bool ED_view3d_minmax_verts(struct Object *obedit, float min[3], float max[3]);

void VIEW3D_OT_snap_selected_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_cursor(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_active(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_center(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_selected(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_active(struct wmOperatorType *ot);

/* space_view3d.c */
extern const char *view3d_context_dir[]; /* doc access */

/* view3d_widgets.c */
void VIEW3D_GGT_light_spot(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_area(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_target(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_camera(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_camera_view(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_force_field(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_empty_image(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_armature_spline(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_navigate(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_mesh_preselect_elem(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_mesh_preselect_edgering(struct wmGizmoGroupType *gzgt);

void VIEW3D_GGT_ruler(struct wmGizmoGroupType *gzgt);
void VIEW3D_GT_ruler_item(struct wmGizmoType *gzt);
void VIEW3D_OT_ruler_add(struct wmOperatorType *ot);
void VIEW3D_OT_ruler_remove(struct wmOperatorType *ot);

void VIEW3D_GT_navigate_rotate(struct wmGizmoType *gzt);

/* workaround for trivial but noticeable camera bug caused by imprecision
 * between view border calculation in 2D/3D space, workaround for bug [#28037].
 * without this define we get the old behavior which is to try and align them
 * both which _mostly_ works fine, but when the camera moves beyond ~1000 in
 * any direction it starts to fail */
#define VIEW3D_CAMERA_BORDER_HACK
#ifdef VIEW3D_CAMERA_BORDER_HACK
extern uchar view3d_camera_border_hack_col[3];
extern bool view3d_camera_border_hack_test;
#endif

#endif /* __VIEW3D_INTERN_H__ */
