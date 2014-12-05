/*
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

#ifndef __ED_VIEW3D_H__
#define __ED_VIEW3D_H__

/* ********* exports for space_view3d/ module ********** */
struct ARegion;
struct BMEdge;
struct BMFace;
struct BMVert;
struct BPoint;
struct Base;
struct BezTriple;
struct BoundBox;
struct EditBone;
struct ImBuf;
struct MVert;
struct Main;
struct MetaElem;
struct Nurb;
struct Object;
struct RegionView3D;
struct Scene;
struct ScrArea;
struct View3D;
struct ViewContext;
struct bContext;
struct bPoseChannel;
struct bScreen;
struct bglMats;
struct rctf;
struct rcti;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;

/* for derivedmesh drawing callbacks, for view3d_select, .... */
typedef struct ViewContext {
	struct Scene *scene;
	struct Object *obact;
	struct Object *obedit;
	struct ARegion *ar;
	struct View3D *v3d;
	struct RegionView3D *rv3d;
	struct BMEditMesh *em;
	int mval[2];
} ViewContext;

typedef struct ViewDepths {
	unsigned short w, h;
	short x, y; /* only for temp use for sub-rects, added to ar->winx/y */
	float *depths;
	double depth_range[2];
	
	bool damaged;
} ViewDepths;

float *ED_view3d_cursor3d_get(struct Scene *scene, struct View3D *v3d);
void   ED_view3d_cursor3d_position(struct bContext *C, float fp[3], const int mval[2]);
void   ED_view3d_cursor3d_update(struct bContext *C, const int mval[2]);

struct Camera *ED_view3d_camera_data_get(struct View3D *v3d, struct RegionView3D *rv3d);

void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], const float dist);
void ED_view3d_from_m4(float mat[4][4], float ofs[3], float quat[4], float *dist);

void ED_view3d_from_object(struct Object *ob, float ofs[3], float quat[4], float *dist, float *lens);
void ED_view3d_to_object(struct Object *ob, const float ofs[3], const float quat[4], const float dist);

void ED_view3d_lastview_store(struct RegionView3D *rv3d);

/* Depth buffer */
void  ED_view3d_depth_update(struct ARegion *ar);
float ED_view3d_depth_read_cached(struct ViewContext *vc, int x, int y);
void  ED_view3d_depth_tag_update(struct RegionView3D *rv3d);

/* Projection */
#define IS_CLIPPED        12000

/* return values for ED_view3d_project_...() */
typedef enum {
	V3D_PROJ_RET_OK   = 0,
	V3D_PROJ_RET_CLIP_NEAR = 1,  /* can't avoid this when in perspective mode, (can't avoid) */
	V3D_PROJ_RET_CLIP_ZERO = 2,  /* so close to zero we can't apply a perspective matrix usefully */
	V3D_PROJ_RET_CLIP_BB   = 3,  /* bounding box clip - RV3D_CLIPPING */
	V3D_PROJ_RET_CLIP_WIN  = 4,  /* outside window bounds */
	V3D_PROJ_RET_OVERFLOW  = 5   /* outside range (mainly for short), (can't avoid) */
} eV3DProjStatus;

/* some clipping tests are optional */
typedef enum {
	V3D_PROJ_TEST_NOP        = 0,
	V3D_PROJ_TEST_CLIP_BB    = (1 << 0),
	V3D_PROJ_TEST_CLIP_WIN   = (1 << 1),
	V3D_PROJ_TEST_CLIP_NEAR  = (1 << 2),
	V3D_PROJ_TEST_CLIP_ZERO  = (1 << 3)
} eV3DProjTest;

#define V3D_PROJ_TEST_CLIP_DEFAULT \
	(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR)
#define V3D_PROJ_TEST_ALL \
	(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR | V3D_PROJ_TEST_CLIP_ZERO)


/* view3d_iterators.c */

/* foreach iterators */
void meshobject_foreachScreenVert(
        struct ViewContext *vc,
        void (*func)(void *userData, struct MVert *eve, const float screen_co[2], int index),
        void *userData, const eV3DProjTest clip_flag);
void mesh_foreachScreenVert(
        struct ViewContext *vc,
        void (*func)(void *userData, struct BMVert *eve, const float screen_co[2], int index),
        void *userData, const eV3DProjTest clip_flag);
void mesh_foreachScreenEdge(
        struct ViewContext *vc,
        void (*func)(void *userData, struct BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2],
                     int index),
        void *userData, const eV3DProjTest clip_flag);
void mesh_foreachScreenFace(
        struct ViewContext *vc,
        void (*func)(void *userData, struct BMFace *efa, const float screen_co[2], int index),
        void *userData, const eV3DProjTest clip_flag);
void nurbs_foreachScreenVert(
        struct ViewContext *vc,
        void (*func)(void *userData, struct Nurb *nu, struct BPoint *bp, struct BezTriple *bezt,
                     int beztindex, const float screen_co[2]),
        void *userData, const eV3DProjTest clip_flag);
void mball_foreachScreenElem(
        struct ViewContext *vc,
        void (*func)(void *userData, struct MetaElem *ml, const float screen_co[2]),
        void *userData, const eV3DProjTest clip_flag);
void lattice_foreachScreenVert(
        struct ViewContext *vc,
        void (*func)(void *userData, struct BPoint *bp,
                     const float screen_co[2]),
        void *userData, const eV3DProjTest clip_flag);
void armature_foreachScreenBone(
        struct ViewContext *vc,
        void (*func)(void *userData, struct EditBone *ebone,
                     const float screen_co_a[2], const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag);
void pose_foreachScreenBone(
        struct ViewContext *vc,
        void (*func)(void *userData, struct bPoseChannel *pchan,
                     const float screen_co_a[2], const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag);
/* *** end iterators *** */


/* view3d_project.c */
void ED_view3d_project_float_v2_m4(const struct ARegion *ar, const float co[3], float r_co[2], float mat[4][4]);
void ED_view3d_project_float_v3_m4(const struct ARegion *ar, const float co[3], float r_co[3], float mat[4][4]);

eV3DProjStatus ED_view3d_project_base(const struct ARegion *ar, struct Base *base);

/* *** short *** */
eV3DProjStatus ED_view3d_project_short_ex(const struct ARegion *ar, float perspmat[4][4], const bool is_local,
                                          const float co[3], short r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_short_global(const struct ARegion *ar, const float co[3], short r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_short_object(const struct ARegion *ar, const float co[3], short r_co[2], const eV3DProjTest flag);

/* *** int *** */
eV3DProjStatus ED_view3d_project_int_ex(const struct ARegion *ar, float perspmat[4][4], const bool is_local,
                                        const float co[3], int r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_int_global(const struct ARegion *ar, const float co[3], int r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_int_object(const struct ARegion *ar, const float co[3], int r_co[2], const eV3DProjTest flag);

/* *** float *** */
eV3DProjStatus ED_view3d_project_float_ex(const struct ARegion *ar, float perspmat[4][4], const bool is_local,
                                          const float co[3], float r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_float_global(const struct ARegion *ar, const float co[3], float r_co[2], const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_float_object(const struct ARegion *ar, const float co[3], float r_co[2], const eV3DProjTest flag);

float ED_view3d_calc_zfac(const struct RegionView3D *rv3d, const float co[3], bool *r_flip);
bool ED_view3d_win_to_ray(const struct ARegion *ar, struct View3D *v3d, const float mval[2],
                          float ray_start[3], float ray_normal[3], const bool do_clip);
bool ED_view3d_win_to_ray_ex(const struct ARegion *ar, struct View3D *v3d, const float mval[2],
                             float r_ray_co[3], float r_ray_normal[3], float r_ray_start[3], bool do_clip);
void ED_view3d_global_to_vector(const struct RegionView3D *rv3d, const float coord[3], float vec[3]);
void ED_view3d_win_to_3d(const struct ARegion *ar, const float depth_pt[3], const float mval[2], float out[3]);
void ED_view3d_win_to_3d_int(const struct ARegion *ar, const float depth_pt[3], const int mval[2], float out[3]);
void ED_view3d_win_to_delta(const struct ARegion *ar, const float mval[2], float out[3], const float zfac);
void ED_view3d_win_to_vector(const struct ARegion *ar, const float mval[2], float out[3]);
bool ED_view3d_win_to_segment(const struct ARegion *ar, struct View3D *v3d, const float mval[2],
                              float r_ray_start[3], float r_ray_end[3], const bool do_clip);
void ED_view3d_ob_project_mat_get(const struct RegionView3D *v3d, struct Object *ob, float pmat[4][4]);
void ED_view3d_unproject(struct bglMats *mats, float out[3], const float x, const float y, const float z);

/* end */


void ED_view3d_dist_range_get(struct View3D *v3d,
                              float r_dist_range[2]);
bool ED_view3d_clip_range_get(struct View3D *v3d, struct RegionView3D *rv3d,
                              float *r_clipsta, float *r_clipend, const bool use_ortho_factor);
bool ED_view3d_viewplane_get(struct View3D *v3d, struct RegionView3D *rv3d, int winxi, int winyi,
                             struct rctf *r_viewplane, float *r_clipsta, float *r_clipend, float *r_pixsize);

void ED_view3d_polygon_offset(const struct RegionView3D *rv3d, const float dist);

void ED_view3d_calc_camera_border(struct Scene *scene, struct ARegion *ar,
                                  struct View3D *v3d, struct RegionView3D *rv3d,
                                  struct rctf *r_viewborder, const bool no_shift);
void ED_view3d_calc_camera_border_size(struct Scene *scene, struct ARegion *ar,
                                       struct View3D *v3d, struct RegionView3D *rv3d,
                                       float r_size[2]);
bool ED_view3d_calc_render_border(struct Scene *scene, struct View3D *v3d,
                                  struct ARegion *ar, struct rcti *rect);

void ED_view3d_clipping_calc(struct BoundBox *bb, float planes[4][4], struct bglMats *mats, const struct rcti *rect);
void ED_view3d_clipping_local(struct RegionView3D *rv3d, float mat[4][4]);
bool ED_view3d_clipping_test(struct RegionView3D *rv3d, const float co[3], const bool is_local);
void ED_view3d_clipping_set(struct RegionView3D *rv3d);
void ED_view3d_clipping_enable(void);
void ED_view3d_clipping_disable(void);

float ED_view3d_pixel_size(struct RegionView3D *rv3d, const float co[3]);

float ED_view3d_radius_to_persp_dist(const float angle, const float radius);
float ED_view3d_radius_to_ortho_dist(const float lens, const float radius);

void drawcircball(int mode, const float cent[3], float rad, float tmat[4][4]);

/* backbuffer select and draw support */
void view3d_validate_backbuf(struct ViewContext *vc);
struct ImBuf *view3d_read_backbuf(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
unsigned int view3d_sample_backbuf_rect(struct ViewContext *vc, const int mval[2], int size,
                                        unsigned int min, unsigned int max, float *dist, short strict,
                                        void *handle, bool (*indextest)(void *handle, unsigned int index));
unsigned int view3d_sample_backbuf(struct ViewContext *vc, int x, int y);

/* draws and does a 4x4 sample */
bool ED_view3d_autodist(struct Scene *scene, struct ARegion *ar, struct View3D *v3d,
                        const int mval[2], float mouse_worldloc[3],
                        const bool alphaoverride, const float fallback_depth_pt[3]);

/* only draw so ED_view3d_autodist_simple can be called many times after */
void ED_view3d_autodist_init(struct Scene *scene, struct ARegion *ar, struct View3D *v3d, int mode);
bool ED_view3d_autodist_simple(struct ARegion *ar, const int mval[2], float mouse_worldloc[3], int margin, float *force_depth);
bool ED_view3d_autodist_depth(struct ARegion *ar, const int mval[2], int margin, float *depth);
bool ED_view3d_autodist_depth_seg(struct ARegion *ar, const int mval_sta[2], const int mval_end[2], int margin, float *depth);

/* select */
#define MAXPICKBUF      10000
short view3d_opengl_select(struct ViewContext *vc, unsigned int *buffer, unsigned int bufsize, const struct rcti *input, bool do_nearest);

/* view3d_select.c */
float ED_view3d_select_dist_px(void);
void view3d_set_viewcontext(struct bContext *C, struct ViewContext *vc);
void view3d_operator_needs_opengl(const struct bContext *C);
void view3d_region_operator_needs_opengl(struct wmWindow *win, struct ARegion *ar);
void view3d_opengl_read_pixels(struct ARegion *ar, int x, int y, int w, int h, int format, int type, void *data);
void view3d_get_transformation(const struct ARegion *ar, struct RegionView3D *rv3d, struct Object *ob, struct bglMats *mats);

/* XXX should move to BLI_math */
bool edge_inside_circle(const float cent[2], float radius, const float screen_co_a[2], const float screen_co_b[2]);

/* get 3d region from context, also if mouse is in header or toolbar */
struct RegionView3D *ED_view3d_context_rv3d(struct bContext *C);
bool ED_view3d_context_user_region(struct bContext *C, struct View3D **r_v3d, struct ARegion **r_ar);
int ED_operator_rv3d_user_region_poll(struct bContext *C);

void ED_view3d_init_mats_rv3d(struct Object *ob, struct RegionView3D *rv3d);
void ED_view3d_init_mats_rv3d_gl(struct Object *ob, struct RegionView3D *rv3d);
#ifdef DEBUG
void ED_view3d_clear_mats_rv3d(struct RegionView3D *rv3d);
void ED_view3d_check_mats_rv3d(struct RegionView3D *rv3d);
#else
#  define ED_view3d_clear_mats_rv3d(rv3d) (void)(rv3d)
#  define ED_view3d_check_mats_rv3d(rv3d) (void)(rv3d)
#endif
int ED_view3d_scene_layer_set(int lay, const int *values, int *active);

bool ED_view3d_context_activate(struct bContext *C);
void ED_view3d_draw_offscreen_init(struct Scene *scene, struct View3D *v3d);
void ED_view3d_draw_offscreen(struct Scene *scene, struct View3D *v3d, struct ARegion *ar,
                              int winx, int winy, float viewmat[4][4], float winmat[4][4], bool do_bgpic, bool do_sky);

struct ImBuf *ED_view3d_draw_offscreen_imbuf(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, int sizex, int sizey, unsigned int flag,
                                             bool draw_background, int alpha_mode, char err_out[256]);
struct ImBuf *ED_view3d_draw_offscreen_imbuf_simple(struct Scene *scene, struct Object *camera, int width, int height, unsigned int flag, int drawtype,
                                                    bool use_solid_tex, bool use_gpencil, bool draw_background, int alpha_mode, char err_out[256]);
void ED_view3d_offscreen_sky_color_get(struct Scene *scene, float sky_color[3]);

struct Base *ED_view3d_give_base_under_cursor(struct bContext *C, const int mval[2]);
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar, bool do_clip);
void ED_view3d_update_viewmat(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, float viewmat[4][4], float winmat[4][4]);
bool ED_view3d_quat_from_axis_view(const char view, float quat[4]);
char ED_view3d_quat_to_axis_view(const float quat[4], const float epsilon);
char ED_view3d_lock_view_from_index(int index);
bool ED_view3d_lock(struct RegionView3D *rv3d);

uint64_t ED_view3d_datamask(struct Scene *scene, struct View3D *v3d);
uint64_t ED_view3d_screen_datamask(struct bScreen *screen);

bool ED_view3d_view_lock_check(struct View3D *v3d, struct RegionView3D *rv3d);

bool ED_view3d_offset_lock_check(struct View3D *v3d, struct RegionView3D *rv3d);

/* camera lock functions */
bool ED_view3d_camera_lock_check(struct View3D *v3d, struct RegionView3D *rv3d);
/* copy the camera to the view before starting a view transformation */
void ED_view3d_camera_lock_init_ex(struct View3D *v3d, struct RegionView3D *rv3d, const bool calc_dist);
void ED_view3d_camera_lock_init(struct View3D *v3d, struct RegionView3D *rv3d);
/* copy the view to the camera, return true if */
bool ED_view3d_camera_lock_sync(struct View3D *v3d, struct RegionView3D *rv3d);

bool ED_view3d_camera_autokey(
        struct Scene *scene, struct ID *id_key,
        struct bContext *C, const bool do_rotate, const bool do_translate);
bool ED_view3d_camera_lock_autokey(
        struct View3D *v3d, struct RegionView3D *rv3d,
        struct bContext *C, const bool do_rotate, const bool do_translate);

void ED_view3D_lock_clear(struct View3D *v3d);

struct BGpic *ED_view3D_background_image_new(struct View3D *v3d);
void ED_view3D_background_image_remove(struct View3D *v3d, struct BGpic *bgpic);
void ED_view3D_background_image_clear(struct View3D *v3d);

#define VIEW3D_MARGIN 1.4f
#define VIEW3D_DIST_FALLBACK 1.0f

float ED_view3d_offset_distance(float mat[4][4], const float ofs[3], const float dist_fallback);
void  ED_view3d_distance_set(struct RegionView3D *rv3d, const float dist);

float ED_scene_grid_scale(struct Scene *scene, const char **grid_unit);
float ED_view3d_grid_scale(struct Scene *scene, struct View3D *v3d, const char **grid_unit);

void ED_scene_draw_fps(struct Scene *scene, const struct rcti *rect);

/* view matrix properties utilities */
/* unused */
#if 0
void ED_view3d_operator_properties_viewmat(struct wmOperatorType *ot);
void ED_view3d_operator_properties_viewmat_set(struct bContext *C, struct wmOperator *op);
void ED_view3d_operator_properties_viewmat_get(struct wmOperator *op, int *winx, int *winy, float persmat[4][4]);
#endif

/* render */
void ED_view3d_shade_update(struct Main *bmain, struct Scene *scene, struct View3D *v3d, struct ScrArea *sa);

#endif /* __ED_VIEW3D_H__ */
