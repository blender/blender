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
 * \ingroup editors
 */

#ifndef __ED_VIEW3D_H__
#define __ED_VIEW3D_H__

/* ********* exports for space_view3d/ module ********** */
struct ARegion;
struct BMEdge;
struct BMElem;
struct BMFace;
struct BMVert;
struct BPoint;
struct Base;
struct BezTriple;
struct BoundBox;
struct Camera;
struct CustomData_MeshMasks;
struct Depsgraph;
struct EditBone;
struct GPUFX;
struct GPUFXSettings;
struct GPUOffScreen;
struct GPUViewport;
struct ID;
struct ImBuf;
struct MVert;
struct Main;
struct MetaElem;
struct Nurb;
struct Object;
struct RV3DMatrixStore;
struct RegionView3D;
struct RenderEngineType;
struct Scene;
struct ScrArea;
struct View3D;
struct View3DShading;
struct ViewContext;
struct ViewLayer;
struct WorkSpace;
struct bContext;
struct bPoseChannel;
struct bScreen;
struct rctf;
struct rcti;
struct wmGizmo;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;
struct wmWindowManager;

enum eGPUFXFlags;

/* for derivedmesh drawing callbacks, for view3d_select, .... */
typedef struct ViewContext {
  struct bContext *C;
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ViewLayer *view_layer;
  struct Object *obact;
  struct Object *obedit;
  struct ARegion *ar;
  struct View3D *v3d;
  struct wmWindow *win;
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

/* Rotate 3D cursor on placement. */
enum eV3DCursorOrient {
  V3D_CURSOR_ORIENT_NONE = 0,
  V3D_CURSOR_ORIENT_VIEW,
  V3D_CURSOR_ORIENT_XFORM,
  V3D_CURSOR_ORIENT_GEOM,
};

void ED_view3d_background_color_get(const struct Scene *scene,
                                    const struct View3D *v3d,
                                    float r_color[3]);
void ED_view3d_cursor3d_position(struct bContext *C,
                                 const int mval[2],
                                 const bool use_depth,
                                 float cursor_co[3]);
void ED_view3d_cursor3d_position_rotation(struct bContext *C,
                                          const int mval[2],
                                          const bool use_depth,
                                          enum eV3DCursorOrient orientation,
                                          float cursor_co[3],
                                          float cursor_quat[4]);
void ED_view3d_cursor3d_update(struct bContext *C,
                               const int mval[2],
                               const bool use_depth,
                               enum eV3DCursorOrient orientation);

struct Camera *ED_view3d_camera_data_get(struct View3D *v3d, struct RegionView3D *rv3d);

void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], const float dist);
void ED_view3d_from_m4(const float mat[4][4], float ofs[3], float quat[4], float *dist);

void ED_view3d_from_object(
    const struct Object *ob, float ofs[3], float quat[4], float *dist, float *lens);
void ED_view3d_to_object(const struct Depsgraph *depsgraph,
                         struct Object *ob,
                         const float ofs[3],
                         const float quat[4],
                         const float dist);

void ED_view3d_lastview_store(struct RegionView3D *rv3d);

/* Depth buffer */
void ED_view3d_depth_update(struct ARegion *ar);
float ED_view3d_depth_read_cached(const struct ViewContext *vc, const int mval[2]);
bool ED_view3d_depth_read_cached_normal(const ViewContext *vc,
                                        const int mval[2],
                                        float r_normal[3]);
bool ED_view3d_depth_unproject(const struct ARegion *ar,
                               const int mval[2],
                               const double depth,
                               float r_location_world[3]);
void ED_view3d_depth_tag_update(struct RegionView3D *rv3d);

/* Projection */
#define IS_CLIPPED 12000

/* return values for ED_view3d_project_...() */
typedef enum {
  V3D_PROJ_RET_OK = 0,
  /** can't avoid this when in perspective mode, (can't avoid) */
  V3D_PROJ_RET_CLIP_NEAR = 1,
  /** so close to zero we can't apply a perspective matrix usefully */
  V3D_PROJ_RET_CLIP_ZERO = 2,
  /** bounding box clip - RV3D_CLIPPING */
  V3D_PROJ_RET_CLIP_BB = 3,
  /** outside window bounds */
  V3D_PROJ_RET_CLIP_WIN = 4,
  /** outside range (mainly for short), (can't avoid) */
  V3D_PROJ_RET_OVERFLOW = 5,
} eV3DProjStatus;

/* some clipping tests are optional */
typedef enum {
  V3D_PROJ_TEST_NOP = 0,
  V3D_PROJ_TEST_CLIP_BB = (1 << 0),
  V3D_PROJ_TEST_CLIP_WIN = (1 << 1),
  V3D_PROJ_TEST_CLIP_NEAR = (1 << 2),
  V3D_PROJ_TEST_CLIP_ZERO = (1 << 3),
} eV3DProjTest;

#define V3D_PROJ_TEST_CLIP_DEFAULT \
  (V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR)
#define V3D_PROJ_TEST_ALL \
  (V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR | \
   V3D_PROJ_TEST_CLIP_ZERO)

/* view3d_iterators.c */

/* foreach iterators */
void meshobject_foreachScreenVert(
    struct ViewContext *vc,
    void (*func)(void *userData, struct MVert *eve, const float screen_co[2], int index),
    void *userData,
    const eV3DProjTest clip_flag);
void mesh_foreachScreenVert(
    struct ViewContext *vc,
    void (*func)(void *userData, struct BMVert *eve, const float screen_co[2], int index),
    void *userData,
    const eV3DProjTest clip_flag);
void mesh_foreachScreenEdge(struct ViewContext *vc,
                            void (*func)(void *userData,
                                         struct BMEdge *eed,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2],
                                         int index),
                            void *userData,
                            const eV3DProjTest clip_flag);
void mesh_foreachScreenFace(
    struct ViewContext *vc,
    void (*func)(void *userData, struct BMFace *efa, const float screen_co[2], int index),
    void *userData,
    const eV3DProjTest clip_flag);
void nurbs_foreachScreenVert(struct ViewContext *vc,
                             void (*func)(void *userData,
                                          struct Nurb *nu,
                                          struct BPoint *bp,
                                          struct BezTriple *bezt,
                                          int beztindex,
                                          const float screen_co[2]),
                             void *userData,
                             const eV3DProjTest clip_flag);
void mball_foreachScreenElem(struct ViewContext *vc,
                             void (*func)(void *userData,
                                          struct MetaElem *ml,
                                          const float screen_co[2]),
                             void *userData,
                             const eV3DProjTest clip_flag);
void lattice_foreachScreenVert(struct ViewContext *vc,
                               void (*func)(void *userData,
                                            struct BPoint *bp,
                                            const float screen_co[2]),
                               void *userData,
                               const eV3DProjTest clip_flag);
void armature_foreachScreenBone(struct ViewContext *vc,
                                void (*func)(void *userData,
                                             struct EditBone *ebone,
                                             const float screen_co_a[2],
                                             const float screen_co_b[2]),
                                void *userData,
                                const eV3DProjTest clip_flag);
void pose_foreachScreenBone(struct ViewContext *vc,
                            void (*func)(void *userData,
                                         struct bPoseChannel *pchan,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2]),
                            void *userData,
                            const eV3DProjTest clip_flag);
/* *** end iterators *** */

/* view3d_project.c */
void ED_view3d_project_float_v2_m4(const struct ARegion *ar,
                                   const float co[3],
                                   float r_co[2],
                                   float mat[4][4]);
void ED_view3d_project_float_v3_m4(const struct ARegion *ar,
                                   const float co[3],
                                   float r_co[3],
                                   float mat[4][4]);

eV3DProjStatus ED_view3d_project_base(const struct ARegion *ar, struct Base *base);

/* *** short *** */
eV3DProjStatus ED_view3d_project_short_ex(const struct ARegion *ar,
                                          float perspmat[4][4],
                                          const bool is_local,
                                          const float co[3],
                                          short r_co[2],
                                          const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_short_global(const struct ARegion *ar,
                                              const float co[3],
                                              short r_co[2],
                                              const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_short_object(const struct ARegion *ar,
                                              const float co[3],
                                              short r_co[2],
                                              const eV3DProjTest flag);

/* *** int *** */
eV3DProjStatus ED_view3d_project_int_ex(const struct ARegion *ar,
                                        float perspmat[4][4],
                                        const bool is_local,
                                        const float co[3],
                                        int r_co[2],
                                        const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_int_global(const struct ARegion *ar,
                                            const float co[3],
                                            int r_co[2],
                                            const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_int_object(const struct ARegion *ar,
                                            const float co[3],
                                            int r_co[2],
                                            const eV3DProjTest flag);

/* *** float *** */
eV3DProjStatus ED_view3d_project_float_ex(const struct ARegion *ar,
                                          float perspmat[4][4],
                                          const bool is_local,
                                          const float co[3],
                                          float r_co[2],
                                          const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_float_global(const struct ARegion *ar,
                                              const float co[3],
                                              float r_co[2],
                                              const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_float_object(const struct ARegion *ar,
                                              const float co[3],
                                              float r_co[2],
                                              const eV3DProjTest flag);

float ED_view3d_pixel_size(const struct RegionView3D *rv3d, const float co[3]);
float ED_view3d_pixel_size_no_ui_scale(const struct RegionView3D *rv3d, const float co[3]);

float ED_view3d_calc_zfac(const struct RegionView3D *rv3d, const float co[3], bool *r_flip);
bool ED_view3d_clip_segment(const struct RegionView3D *rv3d, float ray_start[3], float ray_end[3]);
bool ED_view3d_win_to_ray_clipped(struct Depsgraph *depsgraph,
                                  const struct ARegion *ar,
                                  const struct View3D *v3d,
                                  const float mval[2],
                                  float ray_start[3],
                                  float ray_normal[3],
                                  const bool do_clip);
bool ED_view3d_win_to_ray_clipped_ex(struct Depsgraph *depsgraph,
                                     const struct ARegion *ar,
                                     const struct View3D *v3d,
                                     const float mval[2],
                                     float r_ray_co[3],
                                     float r_ray_normal[3],
                                     float r_ray_start[3],
                                     bool do_clip);
void ED_view3d_win_to_ray(const struct ARegion *ar,
                          const float mval[2],
                          float r_ray_start[3],
                          float r_ray_normal[3]);
void ED_view3d_global_to_vector(const struct RegionView3D *rv3d,
                                const float coord[3],
                                float vec[3]);
void ED_view3d_win_to_3d(const struct View3D *v3d,
                         const struct ARegion *ar,
                         const float depth_pt[3],
                         const float mval[2],
                         float r_out[3]);
void ED_view3d_win_to_3d_int(const struct View3D *v3d,
                             const struct ARegion *ar,
                             const float depth_pt[3],
                             const int mval[2],
                             float r_out[3]);
bool ED_view3d_win_to_3d_on_plane(const struct ARegion *ar,
                                  const float plane[4],
                                  const float mval[2],
                                  const bool do_clip,
                                  float r_out[3]);
bool ED_view3d_win_to_3d_on_plane_int(const struct ARegion *ar,
                                      const float plane[4],
                                      const int mval[2],
                                      const bool do_clip,
                                      float r_out[3]);
void ED_view3d_win_to_delta(const struct ARegion *ar,
                            const float mval[2],
                            float out[3],
                            const float zfac);
void ED_view3d_win_to_origin(const struct ARegion *ar, const float mval[2], float out[3]);
void ED_view3d_win_to_vector(const struct ARegion *ar, const float mval[2], float out[3]);
bool ED_view3d_win_to_segment_clipped(struct Depsgraph *depsgraph,
                                      const struct ARegion *ar,
                                      struct View3D *v3d,
                                      const float mval[2],
                                      float r_ray_start[3],
                                      float r_ray_end[3],
                                      const bool do_clip);
void ED_view3d_ob_project_mat_get(const struct RegionView3D *v3d,
                                  struct Object *ob,
                                  float pmat[4][4]);
void ED_view3d_ob_project_mat_get_from_obmat(const struct RegionView3D *rv3d,
                                             float obmat[4][4],
                                             float pmat[4][4]);

void ED_view3d_project(const struct ARegion *ar, const float world[3], float region[3]);
bool ED_view3d_unproject(
    const struct ARegion *ar, float regionx, float regiony, float regionz, float world[3]);

/* end */

void ED_view3d_dist_range_get(const struct View3D *v3d, float r_dist_range[2]);
bool ED_view3d_clip_range_get(struct Depsgraph *depsgraph,
                              const struct View3D *v3d,
                              const struct RegionView3D *rv3d,
                              float *r_clipsta,
                              float *r_clipend,
                              const bool use_ortho_factor);
bool ED_view3d_viewplane_get(struct Depsgraph *depsgraph,
                             const struct View3D *v3d,
                             const struct RegionView3D *rv3d,
                             int winxi,
                             int winyi,
                             struct rctf *r_viewplane,
                             float *r_clipsta,
                             float *r_clipend,
                             float *r_pixsize);

void ED_view3d_polygon_offset(const struct RegionView3D *rv3d, const float dist);

void ED_view3d_calc_camera_border(const struct Scene *scene,
                                  struct Depsgraph *depsgraph,
                                  const struct ARegion *ar,
                                  const struct View3D *v3d,
                                  const struct RegionView3D *rv3d,
                                  struct rctf *r_viewborder,
                                  const bool no_shift);
void ED_view3d_calc_camera_border_size(const struct Scene *scene,
                                       struct Depsgraph *depsgraph,
                                       const struct ARegion *ar,
                                       const struct View3D *v3d,
                                       const struct RegionView3D *rv3d,
                                       float r_size[2]);
bool ED_view3d_calc_render_border(const struct Scene *scene,
                                  struct Depsgraph *depsgraph,
                                  struct View3D *v3d,
                                  struct ARegion *ar,
                                  struct rcti *rect);

void ED_view3d_clipping_calc_from_boundbox(float clip[6][4],
                                           const struct BoundBox *clipbb,
                                           const bool is_flip);
void ED_view3d_clipping_calc(struct BoundBox *bb,
                             float planes[4][4],
                             const struct ARegion *ar,
                             const struct Object *ob,
                             const struct rcti *rect);
void ED_view3d_clipping_local(struct RegionView3D *rv3d, float mat[4][4]);
bool ED_view3d_clipping_test(const struct RegionView3D *rv3d,
                             const float co[3],
                             const bool is_local);
void ED_view3d_clipping_set(struct RegionView3D *rv3d);
void ED_view3d_clipping_enable(void);
void ED_view3d_clipping_disable(void);

float ED_view3d_radius_to_dist_persp(const float angle, const float radius);
float ED_view3d_radius_to_dist_ortho(const float lens, const float radius);
float ED_view3d_radius_to_dist(const struct View3D *v3d,
                               const struct ARegion *ar,
                               const struct Depsgraph *depsgraph,
                               const char persp,
                               const bool use_aspect,
                               const float radius);

void imm_drawcircball(const float cent[3], float rad, const float tmat[4][4], unsigned pos);

/* backbuffer select and draw support */
void ED_view3d_backbuf_depth_validate(struct ViewContext *vc);
int ED_view3d_backbuf_sample_size_clamp(struct ARegion *ar, const float dist);

void ED_view3d_select_id_validate(struct ViewContext *vc);
void ED_view3d_select_id_validate_with_select_mode(struct ViewContext *vc, short select_mode);

uint ED_view3d_select_id_sample(struct ViewContext *vc, int x, int y);
uint *ED_view3d_select_id_read(int xmin, int ymin, int xmax, int ymax, uint *r_buf_len);
uint *ED_view3d_select_id_read_rect(const struct rcti *rect, uint *r_buf_len);
uint ED_view3d_select_id_read_nearest(
    struct ViewContext *vc, const int mval[2], const uint min, const uint max, uint *r_dist);

bool ED_view3d_autodist(struct Depsgraph *depsgraph,
                        struct ARegion *ar,
                        struct View3D *v3d,
                        const int mval[2],
                        float mouse_worldloc[3],
                        const bool alphaoverride,
                        const float fallback_depth_pt[3]);

/* only draw so ED_view3d_autodist_simple can be called many times after */
void ED_view3d_autodist_init(struct Depsgraph *depsgraph,
                             struct ARegion *ar,
                             struct View3D *v3d,
                             int mode);
bool ED_view3d_autodist_simple(struct ARegion *ar,
                               const int mval[2],
                               float mouse_worldloc[3],
                               int margin,
                               float *force_depth);
bool ED_view3d_autodist_depth(struct ARegion *ar, const int mval[2], int margin, float *depth);
bool ED_view3d_autodist_depth_seg(
    struct ARegion *ar, const int mval_sta[2], const int mval_end[2], int margin, float *depth);

/* select */
#define MAXPICKELEMS 2500
#define MAXPICKBUF (4 * MAXPICKELEMS)

typedef enum {
  /* all elements in the region, ignore depth */
  VIEW3D_SELECT_ALL = 0,
  /* pick also depth sorts (only for small regions!) */
  VIEW3D_SELECT_PICK_ALL = 1,
  /* sorts and only returns visible objects (only for small regions!) */
  VIEW3D_SELECT_PICK_NEAREST = 2,
} eV3DSelectMode;

typedef enum {
  /** Don't exclude anything. */
  VIEW3D_SELECT_FILTER_NOP = 0,
  /** Don't select objects outside the current mode. */
  VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK = 1,
  /** A version of #VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK that allows pose-bone selection. */
  VIEW3D_SELECT_FILTER_WPAINT_POSE_MODE_LOCK = 2,
} eV3DSelectObjectFilter;

eV3DSelectObjectFilter ED_view3d_select_filter_from_mode(const struct Scene *scene,
                                                         const struct Object *obact);

void view3d_opengl_select_cache_begin(void);
void view3d_opengl_select_cache_end(void);

int view3d_opengl_select(struct ViewContext *vc,
                         unsigned int *buffer,
                         unsigned int bufsize,
                         const struct rcti *input,
                         eV3DSelectMode select_mode,
                         eV3DSelectObjectFilter select_filter);

/* view3d_select.c */
float ED_view3d_select_dist_px(void);
void ED_view3d_viewcontext_init(struct bContext *C, struct ViewContext *vc);
void ED_view3d_viewcontext_init_object(struct ViewContext *vc, struct Object *obact);
void view3d_operator_needs_opengl(const struct bContext *C);
void view3d_region_operator_needs_opengl(struct wmWindow *win, struct ARegion *ar);

/* XXX should move to BLI_math */
bool edge_inside_circle(const float cent[2],
                        float radius,
                        const float screen_co_a[2],
                        const float screen_co_b[2]);

/* get 3d region from context, also if mouse is in header or toolbar */
struct RegionView3D *ED_view3d_context_rv3d(struct bContext *C);
bool ED_view3d_context_user_region(struct bContext *C,
                                   struct View3D **r_v3d,
                                   struct ARegion **r_ar);
bool ED_operator_rv3d_user_region_poll(struct bContext *C);

void ED_view3d_init_mats_rv3d(struct Object *ob, struct RegionView3D *rv3d);
void ED_view3d_init_mats_rv3d_gl(struct Object *ob, struct RegionView3D *rv3d);
#ifdef DEBUG
void ED_view3d_clear_mats_rv3d(struct RegionView3D *rv3d);
void ED_view3d_check_mats_rv3d(struct RegionView3D *rv3d);
#else
#  define ED_view3d_clear_mats_rv3d(rv3d) (void)(rv3d)
#  define ED_view3d_check_mats_rv3d(rv3d) (void)(rv3d)
#endif

struct RV3DMatrixStore *ED_view3d_mats_rv3d_backup(struct RegionView3D *rv3d);
void ED_view3d_mats_rv3d_restore(struct RegionView3D *rv3d, struct RV3DMatrixStore *rv3dmat);

void ED_draw_object_facemap(struct Depsgraph *depsgraph,
                            struct Object *ob,
                            const float col[4],
                            const int facemap);

struct RenderEngineType *ED_view3d_engine_type(struct Scene *scene, int drawtype);

bool ED_view3d_context_activate(struct bContext *C);
void ED_view3d_draw_offscreen(struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              int drawtype,
                              struct View3D *v3d,
                              struct ARegion *ar,
                              int winx,
                              int winy,
                              float viewmat[4][4],
                              float winmat[4][4],
                              bool do_sky,
                              bool is_persp,
                              const char *viewname,
                              const bool do_color_managment,
                              struct GPUOffScreen *ofs,
                              struct GPUViewport *viewport);
void ED_view3d_draw_setup_view(struct wmWindow *win,
                               struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct ARegion *ar,
                               struct View3D *v3d,
                               float viewmat[4][4],
                               float winmat[4][4],
                               const struct rcti *rect);

struct ImBuf *ED_view3d_draw_offscreen_imbuf(struct Depsgraph *depsgraph,
                                             struct Scene *scene,
                                             int drawtype,
                                             struct View3D *v3d,
                                             struct ARegion *ar,
                                             int sizex,
                                             int sizey,
                                             unsigned int flag,
                                             unsigned int draw_flags,
                                             int alpha_mode,
                                             int samples,
                                             const char *viewname,
                                             struct GPUOffScreen *ofs,
                                             char err_out[256]);
struct ImBuf *ED_view3d_draw_offscreen_imbuf_simple(struct Depsgraph *depsgraph,
                                                    struct Scene *scene,
                                                    struct View3DShading *shading_override,
                                                    int drawtype,
                                                    struct Object *camera,
                                                    int width,
                                                    int height,
                                                    unsigned int flag,
                                                    unsigned int draw_flags,
                                                    int alpha_mode,
                                                    int samples,
                                                    const char *viewname,
                                                    struct GPUOffScreen *ofs,
                                                    char err_out[256]);

struct Base *ED_view3d_give_base_under_cursor(struct bContext *C, const int mval[2]);
struct Object *ED_view3d_give_object_under_cursor(struct bContext *C, const int mval[2]);
bool ED_view3d_is_object_under_cursor(struct bContext *C, const int mval[2]);
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar, bool do_clip);
void ED_view3d_update_viewmat(struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct View3D *v3d,
                              struct ARegion *ar,
                              float viewmat[4][4],
                              float winmat[4][4],
                              const struct rcti *rect);
bool ED_view3d_quat_from_axis_view(const char view, float quat[4]);
char ED_view3d_quat_to_axis_view(const float quat[4], const float epsilon);
char ED_view3d_lock_view_from_index(int index);
char ED_view3d_axis_view_opposite(char view);
bool ED_view3d_lock(struct RegionView3D *rv3d);

void ED_view3d_datamask(const struct bContext *C,
                        const struct Scene *scene,
                        const struct View3D *v3d,
                        struct CustomData_MeshMasks *r_cddata_masks);
void ED_view3d_screen_datamask(const struct bContext *C,
                               const struct Scene *scene,
                               const struct bScreen *screen,
                               struct CustomData_MeshMasks *r_cddata_masks);

bool ED_view3d_offset_lock_check(const struct View3D *v3d, const struct RegionView3D *rv3d);
void ED_view3d_persp_switch_from_camera(const struct Depsgraph *depsgraph,
                                        struct View3D *v3d,
                                        struct RegionView3D *rv3d,
                                        const char persp);
bool ED_view3d_persp_ensure(const struct Depsgraph *depsgraph,
                            struct View3D *v3d,
                            struct ARegion *ar);

/* camera lock functions */
bool ED_view3d_camera_lock_check(const struct View3D *v3d, const struct RegionView3D *rv3d);
/* copy the camera to the view before starting a view transformation */
void ED_view3d_camera_lock_init_ex(const struct Depsgraph *depsgraph,
                                   struct View3D *v3d,
                                   struct RegionView3D *rv3d,
                                   const bool calc_dist);
void ED_view3d_camera_lock_init(const struct Depsgraph *depsgraph,
                                struct View3D *v3d,
                                struct RegionView3D *rv3d);
/* copy the view to the camera, return true if */
bool ED_view3d_camera_lock_sync(const struct Depsgraph *depsgraph,
                                struct View3D *v3d,
                                struct RegionView3D *rv3d);

bool ED_view3d_camera_autokey(struct Scene *scene,
                              struct ID *id_key,
                              struct bContext *C,
                              const bool do_rotate,
                              const bool do_translate);
bool ED_view3d_camera_lock_autokey(struct View3D *v3d,
                                   struct RegionView3D *rv3d,
                                   struct bContext *C,
                                   const bool do_rotate,
                                   const bool do_translate);

void ED_view3d_lock_clear(struct View3D *v3d);

#define VIEW3D_MARGIN 1.4f
#define VIEW3D_DIST_FALLBACK 1.0f

float ED_view3d_offset_distance(float mat[4][4], const float ofs[3], const float dist_fallback);
void ED_view3d_distance_set(struct RegionView3D *rv3d, const float dist);

float ED_scene_grid_scale(struct Scene *scene, const char **grid_unit);
float ED_view3d_grid_scale(struct Scene *scene, struct View3D *v3d, const char **grid_unit);
float ED_view3d_grid_view_scale(struct Scene *scene,
                                struct View3D *v3d,
                                struct RegionView3D *rv3d,
                                const char **grid_unit);

void ED_scene_draw_fps(struct Scene *scene, int xoffset, int *yoffset);

/* view matrix properties utilities */
/* unused */
#if 0
void ED_view3d_operator_properties_viewmat(struct wmOperatorType *ot);
void ED_view3d_operator_properties_viewmat_set(struct bContext *C, struct wmOperator *op);
void ED_view3d_operator_properties_viewmat_get(struct wmOperator *op,
                                               int *winx,
                                               int *winy,
                                               float persmat[4][4]);
#endif

/* render */
void ED_view3d_stop_render_preview(struct wmWindowManager *wm, struct ARegion *ar);
void ED_view3d_shade_update(struct Main *bmain, struct View3D *v3d, struct ScrArea *sa);

#define XRAY_ALPHA(v3d) \
  (((v3d)->shading.type == OB_WIRE) ? (v3d)->shading.xray_alpha_wire : (v3d)->shading.xray_alpha)
#define XRAY_FLAG(v3d) \
  (((v3d)->shading.type == OB_WIRE) ? V3D_SHADING_XRAY_BONE : V3D_SHADING_XRAY)
#define XRAY_FLAG_ENABLED(v3d) (((v3d)->shading.flag & XRAY_FLAG(v3d)) != 0)
#define XRAY_ENABLED(v3d) (XRAY_FLAG_ENABLED(v3d) && (XRAY_ALPHA(v3d) < 1.0f))
#define XRAY_ACTIVE(v3d) (XRAY_ENABLED(v3d) && ((v3d)->shading.type < OB_MATERIAL))

/* view3d_draw_legacy.c */
/* Try avoid using these more move out of legacy. */
void ED_view3d_draw_bgpic_test(struct Scene *scene,
                               struct Depsgraph *depsgraph,
                               struct ARegion *ar,
                               struct View3D *v3d,
                               const bool do_foreground,
                               const bool do_camera_frame);

/* view3d_gizmo_preselect_type.c */
void ED_view3d_gizmo_mesh_preselect_get_active(struct bContext *C,
                                               struct wmGizmo *gz,
                                               struct Base **r_base,
                                               struct BMElem **r_ele);

/* space_view3d.c */
void ED_view3d_buttons_region_layout_ex(const struct bContext *C,
                                        struct ARegion *ar,
                                        const char *category_override);

#endif /* __ED_VIEW3D_H__ */
