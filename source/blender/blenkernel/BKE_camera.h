/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_vec_types.h"

/** \file
 * \ingroup bke
 * \brief Camera data-block and utility functions.
 */

struct Camera;
struct Depsgraph;
struct Main;
struct Object;
struct RegionView3D;
struct RenderData;
struct Scene;
struct View3D;

/* Camera Data-block */

struct Camera *BKE_camera_add(struct Main *bmain, const char *name);

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
struct CameraParams {
  /* lens */
  bool is_ortho = false;
  float lens = 0.0f;
  float ortho_scale = 1.0f;
  float zoom = 1.0f;

  float shiftx = 0.0f;
  float shifty = 0.0f;
  float offsetx = 0.0f;
  float offsety = 0.0f;

  /* sensor */
  float sensor_x = 0.0f;
  float sensor_y = 0.0f;
  int sensor_fit = 0;

  /* clipping */
  float clip_start = 0.1f;
  float clip_end = 100.0f;

  /* computed viewplane */
  float ycor = 0.0f;
  float viewdx = 0.0f;
  float viewdy = 0.0f;
  rctf viewplane;

  /* computed matrix */
  float winmat[4][4] = {};
};

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
 * Crop `viewplane` given the current resolution and a pixel region inside the view plane.
 */
void BKE_camera_params_crop_viewplane(rctf *viewplane, int winx, int winy, const rcti *region);
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
 * `BKE_lib_id.hh`.
 */
struct CameraBGImage *BKE_camera_background_image_copy(const struct CameraBGImage *bgpic_src,
                                                       int flag);
void BKE_camera_background_image_remove(struct Camera *cam, struct CameraBGImage *bgpic);
void BKE_camera_background_image_clear(struct Camera *cam);
