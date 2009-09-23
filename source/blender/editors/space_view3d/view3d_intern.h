/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_VIEW3D_INTERN_H
#define ED_VIEW3D_INTERN_H

#include "ED_view3d.h"

/* internal exports only */

struct bScreen;
struct ARegion;
struct BoundBox;
struct Object;
struct DerivedMesh;
struct wmOperatorType;
struct bContext;
struct wmWindowManager;
struct EditMesh;
struct ViewContext;
struct ARegionType;

#define BL_NEAR_CLIP 0.001

/* drawing flags: */
#define DRAW_PICKING	1
#define DRAW_CONSTCOLOR	2
#define DRAW_SCENESET	4

#define V3D_XRAY	1
#define V3D_TRANSP	2

#define V3D_SELECT_MOUSE	1

/* view3d_header.c */
void view3d_header_buttons(const struct bContext *C, struct ARegion *ar);
void VIEW3D_OT_layers(struct wmOperatorType *ot);

/* view3d_ops.c */
void view3d_operatortypes(void);
void view3d_keymap(struct wmWindowManager *wm);

/* view3d_edit.c */
void VIEW3D_OT_zoom(struct wmOperatorType *ot);
void VIEW3D_OT_viewmove(struct wmOperatorType *ot);
void VIEW3D_OT_viewrotate(struct wmOperatorType *ot);
void VIEW3D_OT_view_all(struct wmOperatorType *ot);
void VIEW3D_OT_viewnumpad(struct wmOperatorType *ot);
void VIEW3D_OT_view_center(struct wmOperatorType *ot);
void VIEW3D_OT_view_pan(struct wmOperatorType *ot);
void VIEW3D_OT_view_persportho(struct wmOperatorType *ot);
void VIEW3D_OT_view_orbit(struct wmOperatorType *ot);
void VIEW3D_OT_clip_border(struct wmOperatorType *ot);
void VIEW3D_OT_cursor3d(struct wmOperatorType *ot);
void VIEW3D_OT_manipulator(struct wmOperatorType *ot);
void VIEW3D_OT_render_border(struct wmOperatorType *ot);
void VIEW3D_OT_zoom_border(struct wmOperatorType *ot);
void VIEW3D_OT_drawtype(struct wmOperatorType *ot);

void view3d_boxview_copy(ScrArea *sa, ARegion *ar);

/* drawobject.c */
void draw_object(Scene *scene, struct ARegion *ar, View3D *v3d, Base *base, int flag);
int draw_glsl_material(Scene *scene, Object *ob, View3D *v3d, int dt);
void draw_object_instance(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, int dt, int outline);
void draw_object_backbufsel(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob);
void drawaxes(float size, int flag, char drawtype);

void view3d_cached_text_draw_begin(void);
void view3d_cached_text_draw_add(float x, float y, float z, char *str, short xoffs);
void view3d_cached_text_draw_end(View3D *v3d, ARegion *ar, int depth_write, float mat[][4]);

/* drawarmature.c */
int draw_armature(Scene *scene, View3D *v3d, ARegion *ar, Base *base, int dt, int flag);

/* drawmesh.c */
void draw_mesh_textured(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, struct DerivedMesh *dm, int faceselect);

/* view3d_draw.c */
void view3d_main_area_draw(const struct bContext *C, struct ARegion *ar);
void draw_depth(Scene *scene, struct ARegion *ar, View3D *v3d, int (* func)(void *));
void view3d_clr_clipping(void);
void view3d_set_clipping(RegionView3D *rv3d);
void add_view3d_after(View3D *v3d, Base *base, int type, int flag);
void calc_viewborder(Scene *scene, struct ARegion *ar, View3D *v3d, rctf *viewborder_r);

void circf(float x, float y, float rad);
void circ(float x, float y, float rad);
void view3d_update_depths(struct ARegion *ar, View3D *v3d);

/* view3d_select.c */
void VIEW3D_OT_select(struct wmOperatorType *ot);
void VIEW3D_OT_select_extend(struct wmOperatorType *ot);
void VIEW3D_OT_select_circle(struct wmOperatorType *ot);
void VIEW3D_OT_select_border(struct wmOperatorType *ot);
void VIEW3D_OT_select_lasso(struct wmOperatorType *ot);

/* view3d_view.c */
void VIEW3D_OT_smoothview(struct wmOperatorType *ot);
void VIEW3D_OT_setcameratoview(struct wmOperatorType *ot);
void VIEW3D_OT_localview(struct wmOperatorType *ot);
void VIEW3D_OT_game_start(struct wmOperatorType *ot);
void VIEW3D_OT_fly(struct wmOperatorType *ot);


int boundbox_clip(RegionView3D *rv3d, float obmat[][4], struct BoundBox *bb);

void centerview(struct ARegion *ar, View3D *v3d);

void smooth_view(struct bContext *C, Object *, Object *, float *ofs, float *quat, float *dist, float *lens);

void setwinmatrixview3d(ARegion *ar, View3D *v3d, rctf *rect);	/* rect: for picking */
void setviewmatrixview3d(Scene *scene, View3D *v3d, RegionView3D *rv3d);

void fly_modal_keymap(struct wmWindowManager *wm);

/* view3d_buttons.c */
void VIEW3D_OT_properties(struct wmOperatorType *ot);
void view3d_buttons_register(struct ARegionType *art);

/* view3d_toolbar.c */
void VIEW3D_OT_toolbar(struct wmOperatorType *ot);
void view3d_toolbar_register(struct ARegionType *art);
void view3d_tool_props_register(struct ARegionType *art);

/* view3d_snap.c */
int minmax_verts(Object *obedit, float *min, float *max);

void VIEW3D_OT_snap_selected_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_cursor(struct wmOperatorType *ot);
void VIEW3D_OT_snap_selected_to_center(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_grid(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_selected(struct wmOperatorType *ot);
void VIEW3D_OT_snap_cursor_to_active(struct wmOperatorType *ot);
void VIEW3D_OT_snap_menu(struct wmOperatorType *ot);

/* space_view3d.c */
ARegion *view3d_has_buttons_region(ScrArea *sa);
ARegion *view3d_has_tools_region(ScrArea *sa);

/* draw_volume.c */
void draw_volume(struct Scene *scene, struct ARegion *ar, struct View3D *v3d, struct Base *base, struct GPUTexture *tex, float *min, float *max, int res[3], float dx, struct GPUTexture *tex_shadow);


#endif /* ED_VIEW3D_INTERN_H */

