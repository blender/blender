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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <stddef.h>

#include "DNA_camera_types.h"
#include "DNA_light_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_ID.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

/****************************** Camera Datablock *****************************/

void BKE_camera_init(Camera *cam)
{
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cam, id));

  cam->lens = 50.0f;
  cam->sensor_x = DEFAULT_SENSOR_WIDTH;
  cam->sensor_y = DEFAULT_SENSOR_HEIGHT;
  cam->clip_start = 0.1f;
  cam->clip_end = 1000.0f;
  cam->drawsize = 1.0f;
  cam->ortho_scale = 6.0;
  cam->flag |= CAM_SHOWPASSEPARTOUT;
  cam->passepartalpha = 0.5f;

  cam->dof.aperture_fstop = 2.8f;
  cam->dof.aperture_ratio = 1.0f;
  cam->dof.focus_distance = 10.0f;

  /* stereoscopy 3d */
  cam->stereo.interocular_distance = 0.065f;
  cam->stereo.convergence_distance = 30.f * 0.065f;
  cam->stereo.pole_merge_angle_from = DEG2RADF(60.0f);
  cam->stereo.pole_merge_angle_to = DEG2RADF(75.0f);
}

void *BKE_camera_add(Main *bmain, const char *name)
{
  Camera *cam;

  cam = BKE_libblock_alloc(bmain, ID_CA, name, 0);

  BKE_camera_init(cam);

  return cam;
}

/**
 * Only copy internal data of Camera ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_camera_copy_data(Main *UNUSED(bmain),
                          Camera *cam_dst,
                          const Camera *cam_src,
                          const int UNUSED(flag))
{
  BLI_duplicatelist(&cam_dst->bg_images, &cam_src->bg_images);
}

Camera *BKE_camera_copy(Main *bmain, const Camera *cam)
{
  Camera *cam_copy;
  BKE_id_copy(bmain, &cam->id, (ID **)&cam_copy);
  return cam_copy;
}

void BKE_camera_make_local(Main *bmain, Camera *cam, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &cam->id, true, lib_local);
}

/** Free (or release) any data used by this camera (does not free the camera itself). */
void BKE_camera_free(Camera *ca)
{
  BLI_freelistN(&ca->bg_images);

  BKE_animdata_free((ID *)ca, false);
}

/******************************** Camera Usage *******************************/

/* get the camera's dof value, takes the dof object into account */
float BKE_camera_object_dof_distance(Object *ob)
{
  Camera *cam = (Camera *)ob->data;
  if (ob->type != OB_CAMERA) {
    return 0.0f;
  }
  if (cam->dof.focus_object) {
    float view_dir[3], dof_dir[3];
    normalize_v3_v3(view_dir, ob->obmat[2]);
    sub_v3_v3v3(dof_dir, ob->obmat[3], cam->dof.focus_object->obmat[3]);
    return fabsf(dot_v3v3(view_dir, dof_dir));
  }
  return cam->dof.focus_distance;
}

float BKE_camera_sensor_size(int sensor_fit, float sensor_x, float sensor_y)
{
  /* sensor size used to fit to. for auto, sensor_x is both x and y. */
  if (sensor_fit == CAMERA_SENSOR_FIT_VERT) {
    return sensor_y;
  }

  return sensor_x;
}

int BKE_camera_sensor_fit(int sensor_fit, float sizex, float sizey)
{
  if (sensor_fit == CAMERA_SENSOR_FIT_AUTO) {
    if (sizex >= sizey) {
      return CAMERA_SENSOR_FIT_HOR;
    }
    else {
      return CAMERA_SENSOR_FIT_VERT;
    }
  }

  return sensor_fit;
}

/******************************** Camera Params *******************************/

void BKE_camera_params_init(CameraParams *params)
{
  memset(params, 0, sizeof(CameraParams));

  /* defaults */
  params->sensor_x = DEFAULT_SENSOR_WIDTH;
  params->sensor_y = DEFAULT_SENSOR_HEIGHT;
  params->sensor_fit = CAMERA_SENSOR_FIT_AUTO;

  params->zoom = 1.0f;

  /* fallback for non camera objects */
  params->clip_start = 0.1f;
  params->clip_end = 100.0f;
}

void BKE_camera_params_from_object(CameraParams *params, const Object *ob)
{
  if (!ob) {
    return;
  }

  if (ob->type == OB_CAMERA) {
    /* camera object */
    Camera *cam = ob->data;

    if (cam->type == CAM_ORTHO) {
      params->is_ortho = true;
    }
    params->lens = cam->lens;
    params->ortho_scale = cam->ortho_scale;

    params->shiftx = cam->shiftx;
    params->shifty = cam->shifty;

    params->sensor_x = cam->sensor_x;
    params->sensor_y = cam->sensor_y;
    params->sensor_fit = cam->sensor_fit;

    params->clip_start = cam->clip_start;
    params->clip_end = cam->clip_end;
  }
  else if (ob->type == OB_LAMP) {
    /* light object */
    Light *la = ob->data;
    params->lens = 16.0f / tanf(la->spotsize * 0.5f);
    if (params->lens == 0.0f) {
      params->lens = 35.0f;
    }
  }
  else {
    params->lens = 35.0f;
  }
}

void BKE_camera_params_from_view3d(CameraParams *params,
                                   Depsgraph *depsgraph,
                                   const View3D *v3d,
                                   const RegionView3D *rv3d)
{
  /* common */
  params->lens = v3d->lens;
  params->clip_start = v3d->clip_start;
  params->clip_end = v3d->clip_end;

  if (rv3d->persp == RV3D_CAMOB) {
    /* camera view */
    const Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
    BKE_camera_params_from_object(params, ob_camera_eval);

    params->zoom = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);

    params->offsetx = 2.0f * rv3d->camdx * params->zoom;
    params->offsety = 2.0f * rv3d->camdy * params->zoom;

    params->shiftx *= params->zoom;
    params->shifty *= params->zoom;

    params->zoom = CAMERA_PARAM_ZOOM_INIT_CAMOB / params->zoom;
  }
  else if (rv3d->persp == RV3D_ORTHO) {
    /* orthographic view */
    float sensor_size = BKE_camera_sensor_size(
        params->sensor_fit, params->sensor_x, params->sensor_y);
    /* Halve, otherwise too extreme low zbuffer quality. */
    params->clip_end *= 0.5f;
    params->clip_start = -params->clip_end;

    params->is_ortho = true;
    /* make sure any changes to this match ED_view3d_radius_to_dist_ortho() */
    params->ortho_scale = rv3d->dist * sensor_size / v3d->lens;
    params->zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
  }
  else {
    /* perspective view */
    params->zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
  }
}

void BKE_camera_params_compute_viewplane(
    CameraParams *params, int winx, int winy, float xasp, float yasp)
{
  rctf viewplane;
  float pixsize, viewfac, sensor_size, dx, dy;
  int sensor_fit;

  params->ycor = yasp / xasp;

  if (params->is_ortho) {
    /* orthographic camera */
    /* scale == 1.0 means exact 1 to 1 mapping */
    pixsize = params->ortho_scale;
  }
  else {
    /* perspective camera */
    sensor_size = BKE_camera_sensor_size(params->sensor_fit, params->sensor_x, params->sensor_y);
    pixsize = (sensor_size * params->clip_start) / params->lens;
  }

  /* determine sensor fit */
  sensor_fit = BKE_camera_sensor_fit(params->sensor_fit, xasp * winx, yasp * winy);

  if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
    viewfac = winx;
  }
  else {
    viewfac = params->ycor * winy;
  }

  pixsize /= viewfac;

  /* extra zoom factor */
  pixsize *= params->zoom;

  /* compute view plane:
   * fully centered, zbuffer fills in jittered between -.5 and +.5 */
  viewplane.xmin = -0.5f * (float)winx;
  viewplane.ymin = -0.5f * params->ycor * (float)winy;
  viewplane.xmax = 0.5f * (float)winx;
  viewplane.ymax = 0.5f * params->ycor * (float)winy;

  /* lens shift and offset */
  dx = params->shiftx * viewfac + winx * params->offsetx;
  dy = params->shifty * viewfac + winy * params->offsety;

  viewplane.xmin += dx;
  viewplane.ymin += dy;
  viewplane.xmax += dx;
  viewplane.ymax += dy;

  /* the window matrix is used for clipping, and not changed during OSA steps */
  /* using an offset of +0.5 here would give clip errors on edges */
  viewplane.xmin *= pixsize;
  viewplane.xmax *= pixsize;
  viewplane.ymin *= pixsize;
  viewplane.ymax *= pixsize;

  /* Used for rendering (offset by near-clip with perspective views), passed to RE_SetPixelSize.
   * For viewport drawing 'RegionView3D.pixsize'. */
  params->viewdx = pixsize;
  params->viewdy = params->ycor * pixsize;
  params->viewplane = viewplane;
}

/* viewplane is assumed to be already computed */
void BKE_camera_params_compute_matrix(CameraParams *params)
{
  rctf viewplane = params->viewplane;

  /* compute projection matrix */
  if (params->is_ortho) {
    orthographic_m4(params->winmat,
                    viewplane.xmin,
                    viewplane.xmax,
                    viewplane.ymin,
                    viewplane.ymax,
                    params->clip_start,
                    params->clip_end);
  }
  else {
    perspective_m4(params->winmat,
                   viewplane.xmin,
                   viewplane.xmax,
                   viewplane.ymin,
                   viewplane.ymax,
                   params->clip_start,
                   params->clip_end);
  }
}

/***************************** Camera View Frame *****************************/

void BKE_camera_view_frame_ex(const Scene *scene,
                              const Camera *camera,
                              const float drawsize,
                              const bool do_clip,
                              const float scale[3],
                              float r_asp[2],
                              float r_shift[2],
                              float *r_drawsize,
                              float r_vec[4][3])
{
  float facx, facy;
  float depth;

  /* aspect correcton */
  if (scene) {
    float aspx = (float)scene->r.xsch * scene->r.xasp;
    float aspy = (float)scene->r.ysch * scene->r.yasp;
    int sensor_fit = BKE_camera_sensor_fit(camera->sensor_fit, aspx, aspy);

    if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
      r_asp[0] = 1.0;
      r_asp[1] = aspy / aspx;
    }
    else {
      r_asp[0] = aspx / aspy;
      r_asp[1] = 1.0;
    }
  }
  else {
    r_asp[0] = 1.0f;
    r_asp[1] = 1.0f;
  }

  if (camera->type == CAM_ORTHO) {
    facx = 0.5f * camera->ortho_scale * r_asp[0] * scale[0];
    facy = 0.5f * camera->ortho_scale * r_asp[1] * scale[1];
    r_shift[0] = camera->shiftx * camera->ortho_scale * scale[0];
    r_shift[1] = camera->shifty * camera->ortho_scale * scale[1];
    depth = -drawsize * scale[2];

    *r_drawsize = 0.5f * camera->ortho_scale;
  }
  else {
    /* that way it's always visible - clip_start+0.1 */
    float fac, scale_x, scale_y;
    float half_sensor = 0.5f * ((camera->sensor_fit == CAMERA_SENSOR_FIT_VERT) ?
                                    (camera->sensor_y) :
                                    (camera->sensor_x));

    /* fixed size, variable depth (stays a reasonable size in the 3D view) */
    *r_drawsize = (drawsize / 2.0f) / ((scale[0] + scale[1] + scale[2]) / 3.0f);
    depth = *r_drawsize * camera->lens / (-half_sensor) * scale[2];
    fac = *r_drawsize;
    scale_x = scale[0];
    scale_y = scale[1];

    facx = fac * r_asp[0] * scale_x;
    facy = fac * r_asp[1] * scale_y;
    r_shift[0] = camera->shiftx * fac * 2.0f * scale_x;
    r_shift[1] = camera->shifty * fac * 2.0f * scale_y;
  }

  r_vec[0][0] = r_shift[0] + facx;
  r_vec[0][1] = r_shift[1] + facy;
  r_vec[0][2] = depth;
  r_vec[1][0] = r_shift[0] + facx;
  r_vec[1][1] = r_shift[1] - facy;
  r_vec[1][2] = depth;
  r_vec[2][0] = r_shift[0] - facx;
  r_vec[2][1] = r_shift[1] - facy;
  r_vec[2][2] = depth;
  r_vec[3][0] = r_shift[0] - facx;
  r_vec[3][1] = r_shift[1] + facy;
  r_vec[3][2] = depth;

  if (do_clip) {
    /* Ensure the frame isn't behind the near clipping plane, T62814. */
    float fac = (camera->clip_start + 0.1f) / -r_vec[0][2];
    for (uint i = 0; i < 4; i++) {
      if (camera->type == CAM_ORTHO) {
        r_vec[i][2] *= fac;
      }
      else {
        mul_v3_fl(r_vec[i], fac);
      }
    }
  }
}

void BKE_camera_view_frame(const Scene *scene, const Camera *camera, float r_vec[4][3])
{
  float dummy_asp[2];
  float dummy_shift[2];
  float dummy_drawsize;
  const float dummy_scale[3] = {1.0f, 1.0f, 1.0f};

  BKE_camera_view_frame_ex(
      scene, camera, 1.0, false, dummy_scale, dummy_asp, dummy_shift, &dummy_drawsize, r_vec);
}

#define CAMERA_VIEWFRAME_NUM_PLANES 4

typedef struct CameraViewFrameData {
  float plane_tx[CAMERA_VIEWFRAME_NUM_PLANES][4]; /* 4 planes */
  float normal_tx[CAMERA_VIEWFRAME_NUM_PLANES][3];
  float dist_vals_sq[CAMERA_VIEWFRAME_NUM_PLANES]; /* distance squared (signed) */
  unsigned int tot;

  /* Ortho camera only. */
  bool is_ortho;
  float camera_no[3];
  float dist_to_cam;

  /* Not used by callbacks... */
  float camera_rotmat[3][3];
} CameraViewFrameData;

static void camera_to_frame_view_cb(const float co[3], void *user_data)
{
  CameraViewFrameData *data = (CameraViewFrameData *)user_data;
  unsigned int i;

  for (i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
    const float nd = dist_signed_squared_to_plane_v3(co, data->plane_tx[i]);
    CLAMP_MAX(data->dist_vals_sq[i], nd);
  }

  if (data->is_ortho) {
    const float d = dot_v3v3(data->camera_no, co);
    CLAMP_MAX(data->dist_to_cam, d);
  }

  data->tot++;
}

static void camera_frame_fit_data_init(const Scene *scene,
                                       const Object *ob,
                                       CameraParams *params,
                                       CameraViewFrameData *data)
{
  float camera_rotmat_transposed_inversed[4][4];
  unsigned int i;

  /* setup parameters */
  BKE_camera_params_init(params);
  BKE_camera_params_from_object(params, ob);

  /* compute matrix, viewplane, .. */
  if (scene) {
    BKE_camera_params_compute_viewplane(
        params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
  }
  else {
    BKE_camera_params_compute_viewplane(params, 1, 1, 1.0f, 1.0f);
  }
  BKE_camera_params_compute_matrix(params);

  /* initialize callback data */
  copy_m3_m4(data->camera_rotmat, (float(*)[4])ob->obmat);
  normalize_m3(data->camera_rotmat);
  /* To transform a plane which is in its homogeneous representation (4d vector),
   * we need the inverse of the transpose of the transform matrix... */
  copy_m4_m3(camera_rotmat_transposed_inversed, data->camera_rotmat);
  transpose_m4(camera_rotmat_transposed_inversed);
  invert_m4(camera_rotmat_transposed_inversed);

  /* Extract frustum planes from projection matrix. */
  planes_from_projmat(
      params->winmat,
      /*   left              right                 top              bottom        near  far */
      data->plane_tx[2],
      data->plane_tx[0],
      data->plane_tx[3],
      data->plane_tx[1],
      NULL,
      NULL);

  /* Rotate planes and get normals from them */
  for (i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
    mul_m4_v4(camera_rotmat_transposed_inversed, data->plane_tx[i]);
    normalize_v3_v3(data->normal_tx[i], data->plane_tx[i]);
  }

  copy_v4_fl(data->dist_vals_sq, FLT_MAX);
  data->tot = 0;
  data->is_ortho = params->is_ortho;
  if (params->is_ortho) {
    /* we want (0, 0, -1) transformed by camera_rotmat, this is a quicker shortcut. */
    negate_v3_v3(data->camera_no, data->camera_rotmat[2]);
    data->dist_to_cam = FLT_MAX;
  }
}

static bool camera_frame_fit_calc_from_data(CameraParams *params,
                                            CameraViewFrameData *data,
                                            float r_co[3],
                                            float *r_scale)
{
  float plane_tx[CAMERA_VIEWFRAME_NUM_PLANES][4];
  unsigned int i;

  if (data->tot <= 1) {
    return false;
  }

  if (params->is_ortho) {
    const float *cam_axis_x = data->camera_rotmat[0];
    const float *cam_axis_y = data->camera_rotmat[1];
    const float *cam_axis_z = data->camera_rotmat[2];
    float dists[CAMERA_VIEWFRAME_NUM_PLANES];
    float scale_diff;

    /* apply the dist-from-plane's to the transformed plane points */
    for (i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
      dists[i] = sqrtf_signed(data->dist_vals_sq[i]);
    }

    if ((dists[0] + dists[2]) > (dists[1] + dists[3])) {
      scale_diff = (dists[1] + dists[3]) *
                   (BLI_rctf_size_x(&params->viewplane) / BLI_rctf_size_y(&params->viewplane));
    }
    else {
      scale_diff = (dists[0] + dists[2]) *
                   (BLI_rctf_size_y(&params->viewplane) / BLI_rctf_size_x(&params->viewplane));
    }
    *r_scale = params->ortho_scale - scale_diff;

    zero_v3(r_co);
    madd_v3_v3fl(r_co, cam_axis_x, (dists[2] - dists[0]) * 0.5f + params->shiftx * scale_diff);
    madd_v3_v3fl(r_co, cam_axis_y, (dists[1] - dists[3]) * 0.5f + params->shifty * scale_diff);
    madd_v3_v3fl(r_co, cam_axis_z, -(data->dist_to_cam - 1.0f - params->clip_start));

    return true;
  }
  else {
    float plane_isect_1[3], plane_isect_1_no[3], plane_isect_1_other[3];
    float plane_isect_2[3], plane_isect_2_no[3], plane_isect_2_other[3];

    float plane_isect_pt_1[3], plane_isect_pt_2[3];

    /* apply the dist-from-plane's to the transformed plane points */
    for (i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
      float co[3];
      mul_v3_v3fl(co, data->normal_tx[i], sqrtf_signed(data->dist_vals_sq[i]));
      plane_from_point_normal_v3(plane_tx[i], co, data->normal_tx[i]);
    }

    if ((!isect_plane_plane_v3(plane_tx[0], plane_tx[2], plane_isect_1, plane_isect_1_no)) ||
        (!isect_plane_plane_v3(plane_tx[1], plane_tx[3], plane_isect_2, plane_isect_2_no))) {
      return false;
    }

    add_v3_v3v3(plane_isect_1_other, plane_isect_1, plane_isect_1_no);
    add_v3_v3v3(plane_isect_2_other, plane_isect_2, plane_isect_2_no);

    if (isect_line_line_v3(plane_isect_1,
                           plane_isect_1_other,
                           plane_isect_2,
                           plane_isect_2_other,
                           plane_isect_pt_1,
                           plane_isect_pt_2) != 0) {
      float cam_plane_no[3];
      float plane_isect_delta[3];
      float plane_isect_delta_len;

      float shift_fac = BKE_camera_sensor_size(
                            params->sensor_fit, params->sensor_x, params->sensor_y) /
                        params->lens;

      /* we want (0, 0, -1) transformed by camera_rotmat, this is a quicker shortcut. */
      negate_v3_v3(cam_plane_no, data->camera_rotmat[2]);

      sub_v3_v3v3(plane_isect_delta, plane_isect_pt_2, plane_isect_pt_1);
      plane_isect_delta_len = len_v3(plane_isect_delta);

      if (dot_v3v3(plane_isect_delta, cam_plane_no) > 0.0f) {
        copy_v3_v3(r_co, plane_isect_pt_1);

        /* offset shift */
        normalize_v3(plane_isect_1_no);
        madd_v3_v3fl(r_co, plane_isect_1_no, params->shifty * plane_isect_delta_len * shift_fac);
      }
      else {
        copy_v3_v3(r_co, plane_isect_pt_2);

        /* offset shift */
        normalize_v3(plane_isect_2_no);
        madd_v3_v3fl(r_co, plane_isect_2_no, params->shiftx * plane_isect_delta_len * shift_fac);
      }

      return true;
    }
  }

  return false;
}

/* don't move the camera, just yield the fit location */
/* r_scale only valid/useful for ortho cameras */
bool BKE_camera_view_frame_fit_to_scene(
    Depsgraph *depsgraph, Scene *scene, Object *camera_ob, float r_co[3], float *r_scale)
{
  CameraParams params;
  CameraViewFrameData data_cb;

  /* just in case */
  *r_scale = 1.0f;

  camera_frame_fit_data_init(scene, camera_ob, &params, &data_cb);

  /* run callback on all visible points */
  BKE_scene_foreach_display_point(depsgraph, camera_to_frame_view_cb, &data_cb);

  return camera_frame_fit_calc_from_data(&params, &data_cb, r_co, r_scale);
}

bool BKE_camera_view_frame_fit_to_coords(const Depsgraph *depsgraph,
                                         const float (*cos)[3],
                                         int num_cos,
                                         Object *camera_ob,
                                         float r_co[3],
                                         float *r_scale)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *camera_ob_eval = DEG_get_evaluated_object(depsgraph, camera_ob);
  CameraParams params;
  CameraViewFrameData data_cb;

  /* just in case */
  *r_scale = 1.0f;

  camera_frame_fit_data_init(scene_eval, camera_ob_eval, &params, &data_cb);

  /* run callback on all given coordinates */
  while (num_cos--) {
    camera_to_frame_view_cb(cos[num_cos], &data_cb);
  }

  return camera_frame_fit_calc_from_data(&params, &data_cb, r_co, r_scale);
}

/******************* multiview matrix functions ***********************/

static void camera_model_matrix(const Object *camera, float r_modelmat[4][4])
{
  copy_m4_m4(r_modelmat, camera->obmat);
}

static void camera_stereo3d_model_matrix(const Object *camera,
                                         const bool is_left,
                                         float r_modelmat[4][4])
{
  Camera *data = (Camera *)camera->data;
  float interocular_distance, convergence_distance;
  short convergence_mode, pivot;
  float sizemat[4][4];

  float fac = 1.0f;
  float fac_signed;

  interocular_distance = data->stereo.interocular_distance;
  convergence_distance = data->stereo.convergence_distance;
  convergence_mode = data->stereo.convergence_mode;
  pivot = data->stereo.pivot;

  if (((pivot == CAM_S3D_PIVOT_LEFT) && is_left) || ((pivot == CAM_S3D_PIVOT_RIGHT) && !is_left)) {
    camera_model_matrix(camera, r_modelmat);
    return;
  }
  else {
    float size[3];
    mat4_to_size(size, camera->obmat);
    size_to_mat4(sizemat, size);
  }

  if (pivot == CAM_S3D_PIVOT_CENTER) {
    fac = 0.5f;
  }

  fac_signed = is_left ? fac : -fac;

  /* rotation */
  if (convergence_mode == CAM_S3D_TOE) {
    float angle;
    float angle_sin, angle_cos;
    float toeinmat[4][4];
    float rotmat[4][4];

    unit_m4(rotmat);

    if (pivot == CAM_S3D_PIVOT_CENTER) {
      fac = -fac;
      fac_signed = -fac_signed;
    }

    angle = atanf((interocular_distance * 0.5f) / convergence_distance) / fac;

    angle_cos = cosf(angle * fac_signed);
    angle_sin = sinf(angle * fac_signed);

    rotmat[0][0] = angle_cos;
    rotmat[2][0] = -angle_sin;
    rotmat[0][2] = angle_sin;
    rotmat[2][2] = angle_cos;

    if (pivot == CAM_S3D_PIVOT_CENTER) {
      /* set the rotation */
      copy_m4_m4(toeinmat, rotmat);
      /* set the translation */
      toeinmat[3][0] = interocular_distance * fac_signed;

      /* transform */
      normalize_m4_m4(r_modelmat, camera->obmat);
      mul_m4_m4m4(r_modelmat, r_modelmat, toeinmat);

      /* scale back to the original size */
      mul_m4_m4m4(r_modelmat, r_modelmat, sizemat);
    }
    else { /* CAM_S3D_PIVOT_LEFT, CAM_S3D_PIVOT_RIGHT */
      /* rotate perpendicular to the interocular line */
      normalize_m4_m4(r_modelmat, camera->obmat);
      mul_m4_m4m4(r_modelmat, r_modelmat, rotmat);

      /* translate along the interocular line */
      unit_m4(toeinmat);
      toeinmat[3][0] = -interocular_distance * fac_signed;
      mul_m4_m4m4(r_modelmat, r_modelmat, toeinmat);

      /* rotate to toe-in angle */
      mul_m4_m4m4(r_modelmat, r_modelmat, rotmat);

      /* scale back to the original size */
      mul_m4_m4m4(r_modelmat, r_modelmat, sizemat);
    }
  }
  else {
    normalize_m4_m4(r_modelmat, camera->obmat);

    /* translate - no rotation in CAM_S3D_OFFAXIS, CAM_S3D_PARALLEL */
    translate_m4(r_modelmat, -interocular_distance * fac_signed, 0.0f, 0.0f);

    /* scale back to the original size */
    mul_m4_m4m4(r_modelmat, r_modelmat, sizemat);
  }
}

/* the view matrix is used by the viewport drawing, it is basically the inverted model matrix */
void BKE_camera_multiview_view_matrix(RenderData *rd,
                                      const Object *camera,
                                      const bool is_left,
                                      float r_viewmat[4][4])
{
  BKE_camera_multiview_model_matrix(
      rd, camera, is_left ? STEREO_LEFT_NAME : STEREO_RIGHT_NAME, r_viewmat);
  invert_m4(r_viewmat);
}

/* left is the default */
static bool camera_is_left(const char *viewname)
{
  if (viewname && viewname[0] != '\0') {
    return !STREQ(viewname, STEREO_RIGHT_NAME);
  }
  return true;
}

void BKE_camera_multiview_model_matrix(RenderData *rd,
                                       const Object *camera,
                                       const char *viewname,
                                       float r_modelmat[4][4])
{
  BKE_camera_multiview_model_matrix_scaled(rd, camera, viewname, r_modelmat);
  normalize_m4(r_modelmat);
}

void BKE_camera_multiview_model_matrix_scaled(RenderData *rd,
                                              const Object *camera,
                                              const char *viewname,
                                              float r_modelmat[4][4])
{
  const bool is_multiview = (rd && rd->scemode & R_MULTIVIEW) != 0;

  if (!is_multiview) {
    camera_model_matrix(camera, r_modelmat);
  }
  else if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
    camera_model_matrix(camera, r_modelmat);
  }
  else { /* SCE_VIEWS_SETUP_BASIC */
    const bool is_left = camera_is_left(viewname);
    camera_stereo3d_model_matrix(camera, is_left, r_modelmat);
  }
}

void BKE_camera_multiview_window_matrix(RenderData *rd,
                                        const Object *camera,
                                        const char *viewname,
                                        float r_winmat[4][4])
{
  CameraParams params;

  /* Setup parameters */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, camera);
  BKE_camera_multiview_params(rd, &params, camera, viewname);

  /* Compute matrix, viewplane, .. */
  BKE_camera_params_compute_viewplane(&params, rd->xsch, rd->ysch, rd->xasp, rd->yasp);
  BKE_camera_params_compute_matrix(&params);

  copy_m4_m4(r_winmat, params.winmat);
}

bool BKE_camera_multiview_spherical_stereo(RenderData *rd, const Object *camera)
{
  Camera *cam;
  const bool is_multiview = (rd && rd->scemode & R_MULTIVIEW) != 0;

  if (!is_multiview) {
    return false;
  }

  if (camera->type != OB_CAMERA) {
    return false;
  }
  else {
    cam = camera->data;
  }

  if ((rd->views_format == SCE_VIEWS_FORMAT_STEREO_3D) && ELEM(cam->type, CAM_PANO, CAM_PERSP) &&
      ((cam->stereo.flag & CAM_S3D_SPHERICAL) != 0)) {
    return true;
  }

  return false;
}

static Object *camera_multiview_advanced(Scene *scene, Object *camera, const char *suffix)
{
  SceneRenderView *srv;
  char name[MAX_NAME];
  const char *camera_name = camera->id.name + 2;
  const int len_name = strlen(camera_name);
  int len_suffix_max = -1;

  name[0] = '\0';

  /* we need to take the better match, thus the len_suffix_max test */
  for (srv = scene->r.views.first; srv; srv = srv->next) {
    const int len_suffix = strlen(srv->suffix);

    if ((len_suffix < len_suffix_max) || (len_name < len_suffix)) {
      continue;
    }

    if (STREQ(camera_name + (len_name - len_suffix), srv->suffix)) {
      BLI_snprintf(name, sizeof(name), "%.*s%s", (len_name - len_suffix), camera_name, suffix);
      len_suffix_max = len_suffix;
    }
  }

  if (name[0] != '\0') {
    Object *ob = BKE_scene_object_find_by_name(scene, name);
    if (ob != NULL) {
      return ob;
    }
  }

  return camera;
}

/* returns the camera to be used for render */
Object *BKE_camera_multiview_render(Scene *scene, Object *camera, const char *viewname)
{
  const bool is_multiview = (camera != NULL) && (scene->r.scemode & R_MULTIVIEW) != 0;

  if (!is_multiview) {
    return camera;
  }
  else if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    return camera;
  }
  else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
    const char *suffix = BKE_scene_multiview_view_suffix_get(&scene->r, viewname);
    return camera_multiview_advanced(scene, camera, suffix);
  }
}

static float camera_stereo3d_shift_x(const Object *camera, const char *viewname)
{
  Camera *data = camera->data;
  float shift = data->shiftx;
  float interocular_distance, convergence_distance;
  short convergence_mode, pivot;
  bool is_left = true;

  float fac = 1.0f;
  float fac_signed;

  if (viewname && viewname[0]) {
    is_left = STREQ(viewname, STEREO_LEFT_NAME);
  }

  interocular_distance = data->stereo.interocular_distance;
  convergence_distance = data->stereo.convergence_distance;
  convergence_mode = data->stereo.convergence_mode;
  pivot = data->stereo.pivot;

  if (convergence_mode != CAM_S3D_OFFAXIS) {
    return shift;
  }

  if (((pivot == CAM_S3D_PIVOT_LEFT) && is_left) || ((pivot == CAM_S3D_PIVOT_RIGHT) && !is_left)) {
    return shift;
  }

  if (pivot == CAM_S3D_PIVOT_CENTER) {
    fac = 0.5f;
  }

  fac_signed = is_left ? fac : -fac;
  shift += ((interocular_distance / data->sensor_x) * (data->lens / convergence_distance)) *
           fac_signed;

  return shift;
}

float BKE_camera_multiview_shift_x(RenderData *rd, const Object *camera, const char *viewname)
{
  const bool is_multiview = (rd && rd->scemode & R_MULTIVIEW) != 0;
  Camera *data = camera->data;

  BLI_assert(camera->type == OB_CAMERA);

  if (!is_multiview) {
    return data->shiftx;
  }
  else if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
    return data->shiftx;
  }
  else { /* SCE_VIEWS_SETUP_BASIC */
    return camera_stereo3d_shift_x(camera, viewname);
  }
}

void BKE_camera_multiview_params(RenderData *rd,
                                 CameraParams *params,
                                 const Object *camera,
                                 const char *viewname)
{
  if (camera->type == OB_CAMERA) {
    params->shiftx = BKE_camera_multiview_shift_x(rd, camera, viewname);
  }
}

CameraBGImage *BKE_camera_background_image_new(Camera *cam)
{
  CameraBGImage *bgpic = MEM_callocN(sizeof(CameraBGImage), "Background Image");

  bgpic->scale = 1.0f;
  bgpic->alpha = 0.5f;
  bgpic->iuser.ok = 1;
  bgpic->iuser.flag |= IMA_ANIM_ALWAYS;
  bgpic->flag |= CAM_BGIMG_FLAG_EXPANDED;

  BLI_addtail(&cam->bg_images, bgpic);

  return bgpic;
}

void BKE_camera_background_image_remove(Camera *cam, CameraBGImage *bgpic)
{
  BLI_remlink(&cam->bg_images, bgpic);

  MEM_freeN(bgpic);
}

void BKE_camera_background_image_clear(Camera *cam)
{
  CameraBGImage *bgpic = cam->bg_images.first;

  while (bgpic) {
    CameraBGImage *next_bgpic = bgpic->next;

    BKE_camera_background_image_remove(cam, bgpic);

    bgpic = next_bgpic;
  }
}
