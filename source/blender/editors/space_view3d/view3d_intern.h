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

struct BoundBox;
struct Object;
struct DerivedMesh;
struct wmOperatorType;
struct EditVert;
struct EditEdge;
struct EditFace;
struct Nurb;
struct BezTriple;
struct BPoint;
struct bContext;
struct wmWindowManager;

typedef struct ViewDepths {
	unsigned short w, h;
	float *depths;
	double depth_range[2];
	
	char damaged;
} ViewDepths;

/* drawing flags: */
#define DRAW_PICKING	1
#define DRAW_CONSTCOLOR	2
#define DRAW_SCENESET	4

#define V3D_XRAY	1
#define V3D_TRANSP	2

#define V3D_SELECT_MOUSE	1

/* project short */
#define IS_CLIPPED        12000

/* view3d_header.c */
void view3d_header_buttons(const struct bContext *C, ARegion *ar);

/* view3d_ops.c */
void view3d_operatortypes(void);
void view3d_keymap(struct wmWindowManager *wm);

/* view3d_edit.c */
void VIEW3D_OT_viewzoom(struct wmOperatorType *ot);
void VIEW3D_OT_viewmove(struct wmOperatorType *ot);
void VIEW3D_OT_viewrotate(struct wmOperatorType *ot);
void VIEW3D_OT_viewhome(struct wmOperatorType *ot);
void VIEW3D_OT_viewnumpad(struct wmOperatorType *ot);
void VIEW3D_OT_viewcenter(struct wmOperatorType *ot);
void VIEW3D_OT_clipping(struct wmOperatorType *ot);
void VIEW3D_OT_cursor3d(struct wmOperatorType *ot);
void VIEW3D_OT_render_border(struct wmOperatorType *ot);

/* drawobject.c */
void draw_object(Scene *scene, ARegion *ar, View3D *v3d, Base *base, int flag);
int draw_glsl_material(Scene *scene, Object *ob, View3D *v3d, int dt);
void drawcircball(int mode, float *cent, float rad, float tmat[][4]);
void draw_object_instance(Scene *scene, View3D *v3d, Object *ob, int dt, int outline);
void drawaxes(float size, int flag, char drawtype);
void mesh_foreachScreenVert(ARegion *ar, View3D *v3d, void (*func)(void *userData, struct EditVert *eve, int x, int y, int index), void *userData, int clipVerts);
void mesh_foreachScreenEdge(ARegion *ar, View3D *v3d, void (*func)(void *userData, struct EditEdge *eed, int x0, int y0, int x1, int y1, int index), void *userData, int clipVerts);
void mesh_foreachScreenFace(ARegion *ar, View3D *v3d, void (*func)(void *userData, struct EditFace *efa, int x, int y, int index), void *userData);
void nurbs_foreachScreenVert(ARegion *ar, View3D *v3d, void (*func)(void *userData, struct Nurb *nu, struct BPoint *bp, struct BezTriple *bezt, int beztindex, int x, int y), void *userData);
void lattice_foreachScreenVert(void (*func)(void *userData, struct BPoint *bp, int x, int y), void *userData);

/* drawarmature.c */
int draw_armature(Scene *scene, View3D *v3d, Base *base, int dt, int flag);

/* drawmesh.c */
void draw_mesh_textured(Scene *scene, View3D *v3d, Object *ob, struct DerivedMesh *dm, int faceselect);

/* view3d_draw.c */
void drawview3dspace(Scene *scene, ARegion *ar, View3D *v3d);
void draw_depth(Scene *scene, ARegion *ar, View3D *v3d, int (* func)(void *));
int view3d_test_clipping(View3D *v3d, float *vec);
void view3d_clr_clipping(void);
void view3d_set_clipping(View3D *v3d);
void add_view3d_after(View3D *v3d, Base *base, int type, int flag);
void make_axis_color(char *col, char *col2, char axis);
void calc_viewborder(Scene *scene, ARegion *ar, View3D *v3d, rctf *viewborder_r);

void circf(float x, float y, float rad);
void circ(float x, float y, float rad);
void view3d_update_depths(ARegion *ar, View3D *v3d);

/* view3d_select.c */
void VIEW3D_OT_select(struct wmOperatorType *ot);
void VIEW3D_OT_select_extend(struct wmOperatorType *ot);
void VIEW3D_OT_circle_select(struct wmOperatorType *ot);
void VIEW3D_OT_borderselect(struct wmOperatorType *ot);

/* view3d_view.c */
void VIEW3D_OT_smoothview(struct wmOperatorType *ot);

void view3d_operator_needs_opengl(const struct bContext *C);

int boundbox_clip(View3D *v3d, float obmat[][4], struct BoundBox *bb);

void view3d_project_short_clip(ARegion *ar, View3D *v3d, float *vec, short *adr, float projmat[4][4], float wmat[4][4]);
void view3d_project_short_noclip(ARegion *ar, float *vec, short *adr, float mat[4][4]);
void view3d_project_float(ARegion *a, float *vec, float *adr, float mat[4][4]);
void view3d_get_object_project_mat(View3D *v3d, struct Object *ob, float pmat[4][4], float vmat[4][4]);
void view3d_project_float(ARegion *ar, float *vec, float *adr, float mat[4][4]);

int get_view3d_viewplane(View3D *v3d, int winxi, int winyi, rctf *viewplane, float *clipsta, float *clipend, float *pixsize);
void view_settings_from_ob(Object *ob, float *ofs, float *quat, float *dist, float *lens);
void obmat_to_viewmat(View3D *v3d, Object *ob, short smooth);

short view3d_opengl_select(Scene *scene, ARegion *ar, View3D *v3d, unsigned int *buffer, unsigned int bufsize, rcti *input);
void initlocalview(Scene *scene, ARegion *ar, View3D *v3d);
void restore_localviewdata(View3D *vd);
void endlocalview(Scene *scene, ScrArea *sa);

void centerview(ARegion *ar, View3D *v3d);

void view3d_align_axis_to_vector(View3D *v3d, int axisidx, float vec[3]);

void smooth_view(struct bContext *C, Object *, Object *, float *ofs, float *quat, float *dist, float *lens);

void setwinmatrixview3d(View3D *v3d, int winx, int winy, rctf *rect);	/* rect: for picking */
void setviewmatrixview3d(View3D *v3d);

#endif /* ED_VIEW3D_INTERN_H */

