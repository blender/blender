/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Camera data-block and utility functions.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct Camera;
struct Depsgraph;
struct Main;
struct Object;
struct RegionView3D;
struct RenderData;
struct Scene;
struct View3D;
struct rctf;

/* Camera Data-block */

void *BKE_camera_add(struct Main *bmain, const char *name);

/* Camera Usage */

/**
 * Get the camera's DOF value, takes the DOF object into account.
 */
float BKE_camera_object_dof_distance(const struct Object *ob);

int BKE_camera_sensor_fit(int sensor_fit, float sizex, float sizey);
float BKE_camera_sensor_size(int sensor_fit, float sensor_x, float sensor_y);

/**
 * Camera Parameters:
 *
 * Intermediate struct for storing camera parameters from various sources,
 * to unify computation of view-plane, window matrix, ... etc.
 */
typedef struct CameraParams {
  /* lens */
  bool is_ortho;
  float lens;
  float ortho_scale;
  float zoom;

  float shiftx;
  float shifty;
  float offsetx;
  float offsety;

  /* sensor */
  float sensor_x;
  float sensor_y;
  int sensor_fit;

  /* clipping */
  float clip_start;
  float clip_end;

  /* computed viewplane */
  float ycor;
  float viewdx;
  float viewdy;
  rctf viewplane;

  /* computed matrix */
  float winmat[4][4];
} CameraParams;

/* Values for CameraParams.zoom, need to be taken into account for some operations. */
#define CAMERA_PARAM_ZOOM_INIT_CAMOB 1.0f
#define CAMERA_PARAM_ZOOM_INIT_PERSP 2.0f

void BKE_camera_params_init(CameraParams *params);
void BKE_camera_params_from_object(CameraParams *params, const struct Object *cam_ob);
void BKE_camera_params_from_view3d(CameraParams *params,
                                   const struct Depsgraph *depsgraph,
                                   const struct View3D *v3d,
                                   const struct RegionView3D *rv3d);

void BKE_camera_params_compute_viewplane(
    CameraParams *params, int winx, int winy, float aspx, float aspy);
/**
 * View-plane is assumed to be already computed.
 */
void BKE_camera_params_compute_matrix(CameraParams *params);

/* Camera View Frame */

void BKE_camera_view_frame_ex(const struct Scene *scene,
                              const struct Camera *camera,
                              float drawsize,
                              bool do_clip,
                              const float scale[3],
                              float r_asp[2],
                              float r_shift[2],
                              float *r_drawsize,
                              float r_vec[4][3]);
void BKE_camera_view_frame(const struct Scene *scene,
                           const struct Camera *camera,
                           float r_vec[4][3]);

/**
 * \param r_scale: only valid/useful for orthographic cameras.
 *
 * \note Don't move the camera, just yield the fit location.
 */
bool BKE_camera_view_frame_fit_to_scene(struct Depsgraph *depsgraph,
                                        const struct Scene *scene,
                                        struct Object *camera_ob,
                                        float r_co[3],
                                        float *r_scale,
                                        float *r_clip_start,
                                        float *r_clip_end);
bool BKE_camera_view_frame_fit_to_coords(const struct Depsgraph *depsgraph,
                                         const float (*cos)[3],
                                         int num_cos,
                                         struct Object *camera_ob,
                                         float r_co[3],
                                         float *r_scale);

/* Camera multi-view API */

/**
 * Returns the camera to be used for render.
 */
struct Object *BKE_camera_multiview_render(const struct Scene *scene,
                                           struct Object *camera,
                                           const char *viewname);
/**
 * The view matrix is used by the viewport drawing, it is basically the inverted model matrix.
 */
void BKE_camera_multiview_view_matrix(const struct RenderData *rd,
                                      const struct Object *camera,
                                      bool is_left,
                                      float r_viewmat[4][4]);
void BKE_camera_multiview_model_matrix(const struct RenderData *rd,
                                       const struct Object *camera,
                                       const char *viewname,
                                       float r_modelmat[4][4]);
void BKE_camera_multiview_model_matrix_scaled(const struct RenderData *rd,
                                              const struct Object *camera,
                                              const char *viewname,
                                              float r_modelmat[4][4]);
void BKE_camera_multiview_window_matrix(const struct RenderData *rd,
                                        const struct Object *camera,
                                        const char *viewname,
                                        float r_winmat[4][4]);
float BKE_camera_multiview_shift_x(const struct RenderData *rd,
                                   const struct Object *camera,
                                   const char *viewname);
void BKE_camera_multiview_params(const struct RenderData *rd,
                                 struct CameraParams *params,
                                 const struct Object *camera,
                                 const char *viewname);
bool BKE_camera_multiview_spherical_stereo(const struct RenderData *rd,
                                           const struct Object *camera);

/* Camera background image API */

struct CameraBGImage *BKE_camera_background_image_new(struct Camera *cam);

/**
 * Duplicate a background image, in a ID management compatible way.
 *
 * \param copy_flag: The usual ID copying flags, see `LIB_ID_CREATE_`/`LIB_ID_COPY_` enums in
 * `BKE_lib_id.h`.
 */
struct CameraBGImage *BKE_camera_background_image_copy(struct CameraBGImage *bgpic_src,
                                                       const int copy_flag);
void BKE_camera_background_image_remove(struct Camera *cam, struct CameraBGImage *bgpic);
void BKE_camera_background_image_clear(struct Camera *cam);

#ifdef __cplusplus
}
#endif
