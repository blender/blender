/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#pragma once

#include "ED_view3d.hh"

#ifdef __cplusplus
extern "C" {
#endif

/* internal exports only */

struct ARegion;
struct ARegionType;
struct BoundBox;
struct Depsgraph;
struct Object;
struct Scene;
struct ViewContext;
struct ViewLayer;
struct bContext;
struct bContextDataResult;
struct wmGizmoGroupType;
struct wmGizmoType;
struct wmKeyConfig;
struct wmOperatorType;

/* `view3d_header.cc` */

void VIEW3D_OT_toggle_matcap_flip(struct wmOperatorType *ot);

/* `view3d_context.cc` */

int view3d_context(const bContext *C, const char *member, bContextDataResult *result);

/* `view3d_ops.cc` */

void view3d_operatortypes(void);

/* `view3d_edit.cc` */

void VIEW3D_OT_zoom_camera_1_to_1(struct wmOperatorType *ot);
void VIEW3D_OT_view_lock_clear(struct wmOperatorType *ot);
void VIEW3D_OT_view_lock_to_active(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_camera(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_lock(struct wmOperatorType *ot);
void VIEW3D_OT_view_persportho(struct wmOperatorType *ot);
void VIEW3D_OT_navigate(struct wmOperatorType *ot);
void VIEW3D_OT_background_image_add(struct wmOperatorType *ot);
void VIEW3D_OT_background_image_remove(struct wmOperatorType *ot);
void VIEW3D_OT_drop_world(struct wmOperatorType *ot);
void VIEW3D_OT_clip_border(struct wmOperatorType *ot);
void VIEW3D_OT_cursor3d(struct wmOperatorType *ot);
void VIEW3D_OT_render_border(struct wmOperatorType *ot);
void VIEW3D_OT_clear_render_border(struct wmOperatorType *ot);
void VIEW3D_OT_toggle_shading(struct wmOperatorType *ot);
void VIEW3D_OT_toggle_xray(struct wmOperatorType *ot);

/* `view3d_draw.cc` */

void view3d_main_region_draw(const struct bContext *C, struct ARegion *region);
/**
 * Information drawn on top of the solid plates and composed data.
 */
void view3d_draw_region_info(const struct bContext *C, struct ARegion *region);

/* view3d_draw_legacy.c */

void ED_view3d_draw_select_loop(struct Depsgraph *depsgraph,
                                struct ViewContext *vc,
                                struct Scene *scene,
                                struct ViewLayer *view_layer,
                                struct View3D *v3d,
                                struct ARegion *region,
                                bool use_obedit_skip,
                                bool use_nearest);

void ED_view3d_draw_depth_loop(struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct ARegion *region,
                               View3D *v3d);

void view3d_depths_rect_create(struct ARegion *region, struct rcti *rect, struct ViewDepths *r_d);
/**
 * Utility function to find the closest Z value, use for auto-depth.
 */
float view3d_depth_near(struct ViewDepths *d);

/* view3d_select.cc */

void VIEW3D_OT_select(struct wmOperatorType *ot);
void VIEW3D_OT_select_circle(struct wmOperatorType *ot);
void VIEW3D_OT_select_box(struct wmOperatorType *ot);
void VIEW3D_OT_select_lasso(struct wmOperatorType *ot);
void VIEW3D_OT_select_menu(struct wmOperatorType *ot);
void VIEW3D_OT_bone_select_menu(struct wmOperatorType *ot);

/* `view3d_utils.cc` */

/**
 * For home, center etc.
 */
void view3d_boxview_copy(struct ScrArea *area, struct ARegion *region);
/**
 * Sync center/zoom view of region to others, for view transforms.
 */
void view3d_boxview_sync(struct ScrArea *area, struct ARegion *region);

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d,
                                const struct BoundBox *bb,
                                float obmat[4][4]);
bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const struct BoundBox *bb);

/* `view3d_view.cc` */

void VIEW3D_OT_camera_to_view(struct wmOperatorType *ot);
void VIEW3D_OT_camera_to_view_selected(struct wmOperatorType *ot);
void VIEW3D_OT_object_as_camera(struct wmOperatorType *ot);
void VIEW3D_OT_localview(struct wmOperatorType *ot);
void VIEW3D_OT_localview_remove_from(struct wmOperatorType *ot);

/**
 * \param rect: optional for picking (can be NULL).
 */
void view3d_winmatrix_set(struct Depsgraph *depsgraph,
                          struct ARegion *region,
                          const View3D *v3d,
                          const rcti *rect);
/**
 * Sets #RegionView3D.viewmat
 *
 * \param depsgraph: Depsgraph.
 * \param scene: Scene for camera and cursor location.
 * \param v3d: View 3D space data.
 * \param rv3d: 3D region which stores the final matrices.
 * \param rect_scale: Optional 2D scale argument,
 * Use when displaying a sub-region, eg: when #view3d_winmatrix_set takes a 'rect' argument.
 *
 * \note don't set windows active in here, is used by renderwin too.
 */
void view3d_viewmatrix_set(struct Depsgraph *depsgraph,
                           const struct Scene *scene,
                           const View3D *v3d,
                           RegionView3D *rv3d,
                           const float rect_scale[2]);

/* Called in `transform_ops.cc`, on each regeneration of key-maps. */

/* `view3d_placement.cc` */

void viewplace_modal_keymap(struct wmKeyConfig *keyconf);

/* `view3d_buttons.cc` */

void VIEW3D_OT_object_mode_pie_or_toggle(struct wmOperatorType *ot);
void view3d_buttons_register(struct ARegionType *art);

/* `view3d_camera_control.cc` */

/**
 * Creates a #View3DCameraControl handle and sets up
 * the view for first-person style navigation.
 */
struct View3DCameraControl *ED_view3d_cameracontrol_acquire(struct Depsgraph *depsgraph,
                                                            struct Scene *scene,
                                                            View3D *v3d,
                                                            RegionView3D *rv3d);
/**
 * Updates cameras from the `rv3d` values, optionally auto-keyframing.
 */
void ED_view3d_cameracontrol_update(struct View3DCameraControl *vctrl,
                                    bool use_autokey,
                                    struct bContext *C,
                                    bool do_rotate,
                                    bool do_translate);
/**
 * Release view control.
 *
 * \param restore: Sets the view state to the values that were set
 *                 before #ED_view3d_control_acquire was called.
 */
void ED_view3d_cameracontrol_release(struct View3DCameraControl *vctrl, bool restore);
/**
 * Returns the object which is being manipulated or NULL.
 */
struct Object *ED_view3d_cameracontrol_object_get(struct View3DCameraControl *vctrl);

/* `view3d_snap.cc` */

/**
 * Calculates the bounding box corners (min and max) for \a obedit.
 * The returned values are in global space.
 */
bool ED_view3d_minmax_verts(struct Object *obedit, float min[3], float max[3]);

void VIEW3D_OT_snap_selected_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_cursor(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_active(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_center(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_selected(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_active(struct wmOperatorType *ot);

/* `view3d_placement.cc` */

void VIEW3D_OT_interactive_add(struct wmOperatorType *ot);

/* space_view3d.cc */

extern const char *view3d_context_dir[]; /* doc access */

/* view3d_widgets.c */

void VIEW3D_GGT_light_spot(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_point(struct wmGizmoGroupType *gzgt);
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
void VIEW3D_GGT_tool_generic_handle_normal(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_tool_generic_handle_free(struct wmGizmoGroupType *gzgt);

void VIEW3D_GGT_ruler(struct wmGizmoGroupType *gzgt);
void VIEW3D_GT_ruler_item(struct wmGizmoType *gzt);
void VIEW3D_OT_ruler_add(struct wmOperatorType *ot);
void VIEW3D_OT_ruler_remove(struct wmOperatorType *ot);

void VIEW3D_GT_navigate_rotate(struct wmGizmoType *gzt);

void VIEW3D_GGT_placement(struct wmGizmoGroupType *gzgt);

/* workaround for trivial but noticeable camera bug caused by imprecision
 * between view border calculation in 2D/3D space, workaround for bug #28037.
 * without this define we get the old behavior which is to try and align them
 * both which _mostly_ works fine, but when the camera moves beyond ~1000 in
 * any direction it starts to fail */
#define VIEW3D_CAMERA_BORDER_HACK
#ifdef VIEW3D_CAMERA_BORDER_HACK
extern uchar view3d_camera_border_hack_col[3];
extern bool view3d_camera_border_hack_test;
#endif

#ifdef __cplusplus
}
#endif
