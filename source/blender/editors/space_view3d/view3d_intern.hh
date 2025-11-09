/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#pragma once

#include "ED_view3d.hh"

/* internal exports only */

struct ARegion;
struct ARegionType;
struct BoundBox;
struct Depsgraph;
struct Object;
struct Scene;
struct bContext;
struct bContextDataResult;
struct View3DCameraControl;
struct wmGizmoGroupType;
struct wmGizmoType;
struct wmKeyConfig;
struct wmOperatorType;

/* `view3d_header.cc` */

void VIEW3D_OT_toggle_matcap_flip(wmOperatorType *ot);

/* `view3d_context.cc` */

int view3d_context(const bContext *C, const char *member, bContextDataResult *result);

/* `view3d_ops.cc` */

void view3d_operatortypes();

/* `view3d_edit.cc` */

void VIEW3D_OT_zoom_camera_1_to_1(wmOperatorType *ot);
void VIEW3D_OT_view_lock_clear(wmOperatorType *ot);
void VIEW3D_OT_view_lock_to_active(wmOperatorType *ot);
void VIEW3D_OT_view_center_camera(wmOperatorType *ot);
void VIEW3D_OT_view_center_lock(wmOperatorType *ot);
void VIEW3D_OT_view_persportho(wmOperatorType *ot);
void VIEW3D_OT_navigate(wmOperatorType *ot);
void VIEW3D_OT_camera_background_image_add(wmOperatorType *ot);
void VIEW3D_OT_camera_background_image_remove(wmOperatorType *ot);
void VIEW3D_OT_drop_world(wmOperatorType *ot);
void VIEW3D_OT_clip_border(wmOperatorType *ot);
void VIEW3D_OT_cursor3d(wmOperatorType *ot);
void VIEW3D_OT_render_border(wmOperatorType *ot);
void VIEW3D_OT_clear_render_border(wmOperatorType *ot);
void VIEW3D_OT_toggle_shading(wmOperatorType *ot);
void VIEW3D_OT_toggle_xray(wmOperatorType *ot);

/* `view3d_draw.cc` */

void view3d_main_region_draw(const bContext *C, ARegion *region);
/**
 * Information drawn on top of the solid plates and composed data.
 */
void view3d_draw_region_info(const bContext *C, ARegion *region);

void view3d_depths_rect_create(ARegion *region, rcti *rect, ViewDepths *r_d);
/**
 * Utility function to find the closest Z value, use for auto-depth.
 *
 * \param r_xy: When non-null, set this to the region relative position of the hit.
 */
float view3d_depth_near_ex(ViewDepths *d, int r_xy[2]);
float view3d_depth_near(ViewDepths *d);

/* view3d_dropboxes.cc */

void view3d_dropboxes();

/* view3d_select.cc */

void VIEW3D_OT_select(wmOperatorType *ot);
void VIEW3D_OT_select_circle(wmOperatorType *ot);
void VIEW3D_OT_select_box(wmOperatorType *ot);
void VIEW3D_OT_select_lasso(wmOperatorType *ot);
void VIEW3D_OT_select_menu(wmOperatorType *ot);
void VIEW3D_OT_bone_select_menu(wmOperatorType *ot);

/* `view3d_utils.cc` */

/**
 * For home, center etc.
 */
void view3d_boxview_copy(ScrArea *area, ARegion *region);
/**
 * Sync center/zoom view of region to others, for view transforms.
 */
void view3d_boxview_sync(ScrArea *area, ARegion *region);

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d, const BoundBox *bb, float obmat[4][4]);
bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const BoundBox *bb);

/* `view3d_view.cc` */

void VIEW3D_OT_camera_to_view(wmOperatorType *ot);
void VIEW3D_OT_camera_to_view_selected(wmOperatorType *ot);
void VIEW3D_OT_object_as_camera(wmOperatorType *ot);
void VIEW3D_OT_localview(wmOperatorType *ot);
void VIEW3D_OT_localview_remove_from(wmOperatorType *ot);

/**
 * \param rect: optional for picking (can be NULL).
 */
void view3d_winmatrix_set(const Depsgraph *depsgraph,
                          ARegion *region,
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
void view3d_viewmatrix_set(const Depsgraph *depsgraph,
                           const Scene *scene,
                           const View3D *v3d,
                           RegionView3D *rv3d,
                           const float rect_scale[2]);

/* Called in `transform_ops.cc`, on each regeneration of key-maps. */

/* `view3d_placement.cc` */

void viewplace_modal_keymap(wmKeyConfig *keyconf);

/* `view3d_buttons.cc` */

void VIEW3D_OT_object_mode_pie_or_toggle(wmOperatorType *ot);
void view3d_buttons_register(ARegionType *art);

/* `view3d_camera_control.cc` */

/**
 * Creates a #View3DCameraControl handle and sets up
 * the view for first-person style navigation.
 */
View3DCameraControl *ED_view3d_cameracontrol_acquire(Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     View3D *v3d,
                                                     RegionView3D *rv3d);
/**
 * Updates cameras from the `rv3d` values, optionally auto-keyframing.
 */
void ED_view3d_cameracontrol_update(
    View3DCameraControl *vctrl, bool use_autokey, bContext *C, bool do_rotate, bool do_translate);
/**
 * Release view control.
 *
 * \param restore: Sets the view state to the values that were set
 *                 before #ED_view3d_control_acquire was called.
 */
void ED_view3d_cameracontrol_release(View3DCameraControl *vctrl, bool restore);
/**
 * Returns the object which is being manipulated or NULL.
 */
Object *ED_view3d_cameracontrol_object_get(View3DCameraControl *vctrl);

/* `view3d_snap.cc` */

/**
 * Calculates the bounding box corners (min and max) for \a obedit.
 * The returned values are in global space.
 */
bool ED_view3d_minmax_verts(const Scene *scene, Object *obedit, float min[3], float max[3]);

void VIEW3D_OT_snap_selected_to_grid(wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_cursor(wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_active(wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_grid(wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_center(wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_selected(wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_active(wmOperatorType *ot);

/* `view3d_placement.cc` */

void VIEW3D_OT_interactive_add(wmOperatorType *ot);

/* space_view3d.cc */

extern "C" const char *view3d_context_dir[]; /* doc access */

/* view3d_widgets.c */

void VIEW3D_GGT_light_spot(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_point(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_area(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_light_target(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_camera(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_camera_view(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_force_field(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_empty_image(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_armature_spline(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_navigate(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_mesh_preselect_elem(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_mesh_preselect_edgering(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_tool_generic_handle_normal(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_tool_generic_handle_free(wmGizmoGroupType *gzgt);
void VIEW3D_GGT_geometry_nodes(struct wmGizmoGroupType *gzgt);

void VIEW3D_GGT_ruler(wmGizmoGroupType *gzgt);
void VIEW3D_GT_ruler_item(wmGizmoType *gzt);
void VIEW3D_OT_ruler_add(wmOperatorType *ot);
void VIEW3D_OT_ruler_remove(wmOperatorType *ot);

void VIEW3D_GT_navigate_rotate(wmGizmoType *gzt);

void VIEW3D_GGT_placement(wmGizmoGroupType *gzgt);

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

/* view3d_navigate_smoothview.cc */
void view3d_smooth_free(RegionView3D *rv3d);
