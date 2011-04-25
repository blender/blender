/*
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_view3d.h
 *  \ingroup editors
 */

#ifndef ED_VIEW3D_H
#define ED_VIEW3D_H

/* ********* exports for space_view3d/ module ********** */
struct ARegion;
struct bContext;
struct BezTriple;
struct bglMats;
struct BoundBox;
struct BPoint;
struct EditEdge;
struct EditFace;
struct EditVert;
struct ImBuf;
struct Main;
struct Nurb;
struct Object;
struct rcti;
struct RegionView3D;
struct Scene;
struct View3D;
struct ViewContext;
struct wmWindow;


/* for derivedmesh drawing callbacks, for view3d_select, .... */
typedef struct ViewContext {
	struct Scene *scene;
	struct Object *obact;
	struct Object *obedit;
	struct ARegion *ar;
	struct View3D *v3d;
	struct RegionView3D *rv3d;
	struct EditMesh *em;
	short mval[2];
} ViewContext;

typedef struct ViewDepths {
	unsigned short w, h;
	short x, y; /* only for temp use for sub-rects, added to ar->winx/y */
	float *depths;
	double depth_range[2];
	
	char damaged;
} ViewDepths;


float *give_cursor(struct Scene *scene, struct View3D *v3d);

int initgrabz(struct RegionView3D *rv3d, float x, float y, float z);
void window_to_3d(struct ARegion *ar, float out[3], short mx, short my);
void window_to_3d_delta(struct ARegion *ar, float out[3], short mx, short my);
void window_to_3d_vector(struct ARegion *ar, float out[3], short mx, short my);
void view3d_unproject(struct bglMats *mats, float out[3], const short x, const short y, const float z);

/* Depth buffer */
void view3d_update_depths(struct ARegion *ar);
float read_cached_depth(struct ViewContext *vc, int x, int y);
void request_depth_update(struct RegionView3D *rv3d);

/* Projection */
#define IS_CLIPPED        12000

void view3d_calculate_clipping(struct BoundBox *bb, float planes[4][4], struct bglMats *mats, struct rcti *rect);

void project_short(struct ARegion *ar, const float vec[3], short adr[2]);
void project_short_noclip(struct ARegion *ar, const float vec[3], short adr[2]);

void project_int(struct ARegion *ar, const float vec[3], int adr[2]);
void project_int_noclip(struct ARegion *ar, const float vec[3], int adr[2]);

void project_float(struct ARegion *ar, const float vec[3], float adr[2]);
void project_float_noclip(struct ARegion *ar, const float vec[3], float adr[2]);

void viewvector(struct RegionView3D *rv3d, float coord[3], float vec[3]);

void viewline(struct ARegion *ar, struct View3D *v3d, float mval[2], float ray_start[3], float ray_end[3]);
void viewray(struct ARegion *ar, struct View3D *v3d, float mval[2], float ray_start[3], float ray_normal[3]);

int get_view3d_cliprange(struct View3D *v3d, struct RegionView3D *rv3d, float *clipsta, float *clipend);
int get_view3d_viewplane(struct View3D *v3d, struct RegionView3D *rv3d, int winxi, int winyi, struct rctf *viewplane, float *clipsta, float *clipend, float *pixsize);
int get_view3d_ortho(struct View3D *v3d, struct RegionView3D *rv3d);
void view3d_get_object_project_mat(struct RegionView3D *v3d, struct Object *ob, float pmat[4][4]);
void view3d_project_float(struct ARegion *a, const float vec[3], float adr[2], float mat[4][4]);
void view3d_calc_camera_border(struct Scene *scene, struct ARegion *ar, struct RegionView3D *rv3d, struct View3D *v3d, struct rctf *viewborder_r, short do_shift);

/* drawobject.c iterators */
void mesh_foreachScreenVert(struct ViewContext *vc, void (*func)(void *userData, struct EditVert *eve, int x, int y, int index), void *userData, int clipVerts);
void mesh_foreachScreenEdge(struct ViewContext *vc, void (*func)(void *userData, struct EditEdge *eed, int x0, int y0, int x1, int y1, int index), void *userData, int clipVerts);
void mesh_foreachScreenFace(struct ViewContext *vc, void (*func)(void *userData, struct EditFace *efa, int x, int y, int index), void *userData);
void nurbs_foreachScreenVert(struct ViewContext *vc, void (*func)(void *userData, struct Nurb *nu, struct BPoint *bp, struct BezTriple *bezt, int beztindex, int x, int y), void *userData);
void lattice_foreachScreenVert(struct ViewContext *vc, void (*func)(void *userData, struct BPoint *bp, int x, int y), void *userData);

void ED_view3d_local_clipping(struct RegionView3D *rv3d, float mat[][4]);
int view3d_test_clipping(struct RegionView3D *rv3d, const float vec[3], const int local);
void view3d_align_axis_to_vector(struct View3D *v3d, struct RegionView3D *rv3d, int axisidx, float vec[3]);
float view3d_pixel_size(struct RegionView3D *rv3d, const float co[3]);

void drawcircball(int mode, const float cent[3], float rad, float tmat[][4]);

/* backbuffer select and draw support */
void view3d_validate_backbuf(struct ViewContext *vc);
struct ImBuf *view3d_read_backbuf(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
unsigned int view3d_sample_backbuf_rect(struct ViewContext *vc, const short mval[2], int size, unsigned int min, unsigned int max, int *dist, short strict,
										void *handle, unsigned int (*indextest)(void *handle, unsigned int index));
unsigned int view3d_sample_backbuf(struct ViewContext *vc, int x, int y);

/* draws and does a 4x4 sample */
int view_autodist(struct Scene *scene, struct ARegion *ar, struct View3D *v3d, const short mval[2], float mouse_worldloc[3]);

/* only draw so view_autodist_simple can be called many times after */
int view_autodist_init(struct Scene *scene, struct ARegion *ar, struct View3D *v3d, int mode);
int view_autodist_simple(struct ARegion *ar, const short mval[2], float mouse_worldloc[3], int margin, float *force_depth);
int view_autodist_depth(struct ARegion *ar, const short mval[2], int margin, float *depth);
int view_autodist_depth_segment(struct ARegion *ar, const short mval_sta[2], const short mval_end[2], int margin, float *depth);

/* select */
#define MAXPICKBUF      10000
short view3d_opengl_select(struct ViewContext *vc, unsigned int *buffer, unsigned int bufsize, rcti *input);

void view3d_set_viewcontext(struct bContext *C, struct ViewContext *vc);
void view3d_operator_needs_opengl(const struct bContext *C);
void view3d_region_operator_needs_opengl(struct wmWindow *win, struct ARegion *ar);
int view3d_get_view_aligned_coordinate(struct ViewContext *vc, float fp[3], const short mval[2], const short do_fallback);
void view3d_get_transformation(struct ARegion *ar, struct RegionView3D *rv3d, struct Object *ob, struct bglMats *mats);

/* XXX should move to BLI_math */
int edge_inside_circle(short centx, short centy, short rad, short x1, short y1, short x2, short y2);
int lasso_inside(short mcords[][2], short moves, short sx, short sy);
int lasso_inside_edge(short mcords[][2], short moves, int x0, int y0, int x1, int y1);

/* get 3d region from context, also if mouse is in header or toolbar */
struct RegionView3D *ED_view3d_context_rv3d(struct bContext *C);

void ED_view3d_init_mats_rv3d(struct Object *ob, struct RegionView3D *rv3d);

int ED_view3d_scene_layer_set(int lay, const int *values, int *active);

int ED_view3d_context_activate(struct bContext *C);
void ED_view3d_draw_offscreen(struct Scene *scene, struct View3D *v3d, struct ARegion *ar,
	int winx, int winy, float viewmat[][4], float winmat[][4]);

struct ImBuf *ED_view3d_draw_offscreen_imbuf(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, int sizex, int sizey, unsigned int flag, char err_out[256]);
struct ImBuf *ED_view3d_draw_offscreen_imbuf_simple(Scene *scene, int width, int height, unsigned int flag, int drawtype, char err_out[256]);


Base *ED_view3d_give_base_under_cursor(struct bContext *C, const short mval[2]);
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar, short do_clip);
int ED_view3d_lock(struct RegionView3D *rv3d);

unsigned int ED_view3d_datamask(struct Scene *scene, struct View3D *v3d);
unsigned int ED_viewedit_datamask(struct bScreen *screen);


/* assigning view matrix */
void view3d_settings_from_mat(float mat[][4], float *ofs, float *quat, float *dist);
void view3d_settings_from_ob(struct Object *ob, float *ofs, float *quat, float *dist, float *lens);
int view3d_is_ortho(struct View3D *v3d, struct RegionView3D *rv3d);

#endif /* ED_VIEW3D_H */

