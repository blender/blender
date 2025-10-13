/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <optional>

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_camera_types.h"
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_bpath.hh"
#include "BKE_camera.h"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.hh"

/* -------------------------------------------------------------------- */
/** \name Camera Data-Block
 * \{ */

static void camera_init_data(ID *id)
{
  Camera *cam = (Camera *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cam, id));

  MEMCPY_STRUCT_AFTER(cam, DNA_struct_default_get(Camera), id);
}

/**
 * Only copy internal data of Camera ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
static void camera_copy_data(Main * /*bmain*/,
                             std::optional<Library *> /*owner_library*/,
                             ID *id_dst,
                             const ID *id_src,
                             const int flag)
{
  Camera *cam_dst = (Camera *)id_dst;
  const Camera *cam_src = (const Camera *)id_src;

  /* We never handle user-count here for owned data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  BLI_listbase_clear(&cam_dst->bg_images);
  LISTBASE_FOREACH (CameraBGImage *, bgpic_src, &cam_src->bg_images) {
    CameraBGImage *bgpic_dst = BKE_camera_background_image_copy(bgpic_src, flag_subdata);
    BLI_addtail(&cam_dst->bg_images, bgpic_dst);
  }

  if (cam_src->custom_bytecode) {
    cam_dst->custom_bytecode = static_cast<char *>(MEM_dupallocN(cam_src->custom_bytecode));
  }
}

/** Free (or release) any data used by this camera (does not free the camera itself). */
static void camera_free_data(ID *id)
{
  Camera *cam = (Camera *)id;
  BLI_freelistN(&cam->bg_images);
  if (cam->custom_bytecode) {
    MEM_freeN(cam->custom_bytecode);
  }
}

static void camera_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Camera *camera = reinterpret_cast<Camera *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, camera->dof.focus_object, IDWALK_CB_NOP);
  LISTBASE_FOREACH (CameraBGImage *, bgpic, &camera->bg_images) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, bgpic->ima, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, bgpic->clip, IDWALK_CB_USER);
  }

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, camera->dof_ob, IDWALK_CB_NOP);
  }

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, camera->custom_shader, IDWALK_CB_USER);
}

static void camera_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Camera *camera = reinterpret_cast<Camera *>(id);

  if (camera->custom_filepath[0]) {
    BKE_bpath_foreach_path_fixed_process(
        bpath_data, camera->custom_filepath, sizeof(camera->custom_filepath));
  }
}

struct CameraCyclesCompatibilityData {
  IDProperty *idprop_prev = nullptr;
  IDProperty *idprop_temp = nullptr;
};

static CameraCyclesCompatibilityData camera_write_cycles_compatibility_data_create(ID *id)
{
  auto cycles_data_ensure = [](IDProperty *group) {
    IDProperty *prop = IDP_GetPropertyTypeFromGroup(group, "cycles", IDP_GROUP);
    if (prop) {
      return prop;
    }
    prop = blender::bke::idprop::create_group("cycles").release();
    IDP_AddToGroup(group, prop);
    return prop;
  };

  auto cycles_property_int_set = [](IDProperty *idprop, const char *name, int value) {
    if (IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_INT)) {
      IDP_int_set(prop, value);
    }
    else {
      IDP_AddToGroup(idprop, blender::bke::idprop::create(name, value).release());
    }
  };

  auto cycles_property_float_set = [](IDProperty *idprop, const char *name, float value) {
    if (IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_FLOAT)) {
      IDP_float_set(prop, value);
    }
    else {
      IDP_AddToGroup(idprop, blender::bke::idprop::create(name, value).release());
    }
  };

  /* For forward compatibility, still write panoramic properties as ID properties for
   * previous blender versions. */
  IDProperty *idprop_prev = IDP_ID_system_properties_get(id);
  /* Make a copy to avoid modifying the original. */
  IDProperty *idprop_temp = idprop_prev ? IDP_CopyProperty(idprop_prev) :
                                          IDP_ID_system_properties_ensure(id);

  Camera *cam = (Camera *)id;
  IDProperty *cycles_cam = cycles_data_ensure(idprop_temp);
  cycles_property_int_set(cycles_cam, "panorama_type", cam->panorama_type);
  cycles_property_float_set(cycles_cam, "fisheye_fov", cam->fisheye_fov);
  cycles_property_float_set(cycles_cam, "fisheye_lens", cam->fisheye_lens);
  cycles_property_float_set(cycles_cam, "latitude_min", cam->latitude_min);
  cycles_property_float_set(cycles_cam, "latitude_max", cam->latitude_max);
  cycles_property_float_set(cycles_cam, "longitude_min", cam->longitude_min);
  cycles_property_float_set(cycles_cam, "longitude_max", cam->longitude_max);
  cycles_property_float_set(cycles_cam, "fisheye_polynomial_k0", cam->fisheye_polynomial_k0);
  cycles_property_float_set(cycles_cam, "fisheye_polynomial_k1", cam->fisheye_polynomial_k1);
  cycles_property_float_set(cycles_cam, "fisheye_polynomial_k2", cam->fisheye_polynomial_k2);
  cycles_property_float_set(cycles_cam, "fisheye_polynomial_k3", cam->fisheye_polynomial_k3);
  cycles_property_float_set(cycles_cam, "fisheye_polynomial_k4", cam->fisheye_polynomial_k4);

  id->system_properties = idprop_temp;

  return {idprop_prev, idprop_temp};
}

static void camera_write_cycles_compatibility_data_clear(ID *id,
                                                         CameraCyclesCompatibilityData &data)
{
  id->system_properties = data.idprop_prev;
  data.idprop_prev = nullptr;

  if (data.idprop_temp) {
    IDP_FreeProperty(data.idprop_temp);
    data.idprop_temp = nullptr;
  }
}

static void camera_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  const bool is_undo = BLO_write_is_undo(writer);
  Camera *cam = (Camera *)id;

  CameraCyclesCompatibilityData cycles_data;
  if (!is_undo) {
    cycles_data = camera_write_cycles_compatibility_data_create(id);
  }

  /* write LibData */
  BLO_write_id_struct(writer, Camera, id_address, &cam->id);
  BKE_id_blend_write(writer, &cam->id);

  LISTBASE_FOREACH (CameraBGImage *, bgpic, &cam->bg_images) {
    BLO_write_struct(writer, CameraBGImage, bgpic);
  }

  if (!is_undo) {
    camera_write_cycles_compatibility_data_clear(id, cycles_data);
  }

  if (cam->custom_bytecode) {
    BLO_write_string(writer, cam->custom_bytecode);
  }
}

static void camera_blend_read_data(BlendDataReader *reader, ID *id)
{
  Camera *ca = (Camera *)id;

  BLO_read_struct_list(reader, CameraBGImage, &ca->bg_images);

  LISTBASE_FOREACH (CameraBGImage *, bgpic, &ca->bg_images) {
    bgpic->iuser.scene = nullptr;

    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(ca)) {
      bgpic->flag &= ~CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL;
    }
  }

  BLO_read_string(reader, &ca->custom_bytecode);
}

IDTypeInfo IDType_ID_CA = {
    /*id_code*/ Camera::id_type,
    /*id_filter*/ FILTER_ID_CA,
    /*dependencies_id_types*/ FILTER_ID_OB | FILTER_ID_IM,
    /*main_listbase_index*/ INDEX_ID_CA,
    /*struct_size*/ sizeof(Camera),
    /*name*/ "Camera",
    /*name_plural*/ N_("cameras"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_CAMERA,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ camera_init_data,
    /*copy_data*/ camera_copy_data,
    /*free_data*/ camera_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ camera_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ camera_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ camera_blend_write,
    /*blend_read_data*/ camera_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Usage
 * \{ */

Camera *BKE_camera_add(Main *bmain, const char *name)
{
  Camera *cam;

  cam = BKE_id_new<Camera>(bmain, name);

  return cam;
}

float BKE_camera_object_dof_distance(const Object *ob)
{
  const Camera *cam = (const Camera *)ob->data;
  if (ob->type != OB_CAMERA) {
    return 0.0f;
  }
  if (cam->dof.focus_object) {
    float view_dir[3], dof_dir[3];
    normalize_v3_v3(view_dir, ob->object_to_world().ptr()[2]);
    bPoseChannel *pchan = BKE_pose_channel_find_name(cam->dof.focus_object->pose,
                                                     cam->dof.focus_subtarget);
    if (pchan) {
      float posemat[4][4];
      mul_m4_m4m4(posemat, cam->dof.focus_object->object_to_world().ptr(), pchan->pose_mat);
      sub_v3_v3v3(dof_dir, ob->object_to_world().location(), posemat[3]);
    }
    else {
      sub_v3_v3v3(dof_dir,
                  ob->object_to_world().location(),
                  cam->dof.focus_object->object_to_world().location());
    }
    return fmax(fabsf(dot_v3v3(view_dir, dof_dir)), 1e-5f);
  }
  return fmax(cam->dof.focus_distance, 1e-5f);
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

    return CAMERA_SENSOR_FIT_VERT;
  }

  return sensor_fit;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Parameter Access
 * \{ */

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

void BKE_camera_params_from_object(CameraParams *params, const Object *cam_ob)
{
  if (!cam_ob) {
    return;
  }

  if (cam_ob->type == OB_CAMERA) {
    /* camera object */
    const Camera *cam = static_cast<const Camera *>(cam_ob->data);

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
  else if (cam_ob->type == OB_LAMP) {
    /* light object */
    Light *la = static_cast<Light *>(cam_ob->data);
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
                                   const Depsgraph *depsgraph,
                                   const View3D *v3d,
                                   const RegionView3D *rv3d)
{
  /* common */
  params->lens = v3d->lens;
  params->clip_start = v3d->clip_start;
  params->clip_end = v3d->clip_end;

  if (rv3d->persp == RV3D_CAMOB) {
    /* camera view */
    const Object *ob_camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
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
    /* Halve, otherwise too extreme low Z-buffer quality. */
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
    CameraParams *params, int winx, int winy, float aspx, float aspy)
{
  rctf viewplane;
  float pixsize, viewfac, sensor_size, dx, dy;
  int sensor_fit;

  params->ycor = aspy / aspx;

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
  sensor_fit = BKE_camera_sensor_fit(params->sensor_fit, aspx * winx, aspy * winy);

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
   * Fully centered, Z-buffer fills in jittered between `-.5` and `+.5`. */
  viewplane.xmin = -0.5f * float(winx);
  viewplane.ymin = -0.5f * params->ycor * float(winy);
  viewplane.xmax = 0.5f * float(winx);
  viewplane.ymax = 0.5f * params->ycor * float(winy);

  /* lens shift and offset */
  dx = params->shiftx * viewfac + winx * params->offsetx;
  dy = params->shifty * viewfac + winy * params->offsety;

  viewplane.xmin += dx;
  viewplane.ymin += dy;
  viewplane.xmax += dx;
  viewplane.ymax += dy;

  /* the window matrix is used for clipping, and not changed during OSA steps */
  /* using an offset of +0.5 here would give clip errors on edges */
  BLI_rctf_mul(&viewplane, pixsize);

  /* Used for rendering (offset by near-clip with perspective views), passed to RE_SetPixelSize.
   * For viewport drawing 'RegionView3D.pixsize'. */
  params->viewdx = pixsize;
  params->viewdy = params->ycor * pixsize;
  params->viewplane = viewplane;
}

void BKE_camera_params_crop_viewplane(rctf *viewplane, int winx, int winy, const rcti *region)
{
  float pix_size_x = BLI_rctf_size_x(viewplane) / winx;
  float pix_size_y = BLI_rctf_size_y(viewplane) / winy;

  viewplane->xmin += pix_size_x * region->xmin;
  viewplane->ymin += pix_size_y * region->ymin;

  viewplane->xmax = viewplane->xmin + pix_size_x * BLI_rcti_size_x(region);
  viewplane->ymax = viewplane->ymin + pix_size_y * BLI_rcti_size_y(region);
}

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera View Frame
 * \{ */

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

  /* aspect correction */
  if (scene) {
    float aspx = float(scene->r.xsch) * scene->r.xasp;
    float aspy = float(scene->r.ysch) * scene->r.yasp;
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
    /* Ensure the frame isn't behind the near clipping plane, #62814. */
    float fac = ((camera->clip_start + 0.1f) / -r_vec[0][2]) * scale[2];
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera View Frame Fit to Points
 * \{ */

#define CAMERA_VIEWFRAME_NUM_PLANES 4

#define Y_MIN 0
#define Y_MAX 1
#define Z_MIN 2
#define Z_MAX 3

struct CameraViewFrameData {
  float plane_tx[CAMERA_VIEWFRAME_NUM_PLANES][4]; /* 4 planes normalized */
  float dist_vals[CAMERA_VIEWFRAME_NUM_PLANES];   /* distance (signed) */
  float camera_no[3];
  float z_range[2];
  uint tot;

  bool do_zrange;

  /* Not used by callbacks... */
  float camera_rotmat[3][3];
};

static void camera_to_frame_view_cb(const float co[3], void *user_data)
{
  CameraViewFrameData *data = (CameraViewFrameData *)user_data;

  for (uint i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
    const float nd = plane_point_side_v3(data->plane_tx[i], co);
    CLAMP_MAX(data->dist_vals[i], nd);
  }

  if (data->do_zrange) {
    const float d = dot_v3v3(data->camera_no, co);
    CLAMP_MAX(data->z_range[0], d);
    CLAMP_MIN(data->z_range[1], d);
  }

  data->tot++;
}

static void camera_frame_fit_data_init(const Scene *scene,
                                       const Object *ob,
                                       const bool do_clip_dists,
                                       CameraParams *params,
                                       CameraViewFrameData *data)
{
  float camera_rotmat_transposed_inversed[4][4];

  /* setup parameters */
  BKE_camera_params_init(params);
  BKE_camera_params_from_object(params, ob);

  /* Compute matrix, view-plane, etc. */
  if (scene) {
    BKE_camera_params_compute_viewplane(
        params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
  }
  else {
    BKE_camera_params_compute_viewplane(params, 1, 1, 1.0f, 1.0f);
  }
  BKE_camera_params_compute_matrix(params);

  /* initialize callback data */
  copy_m3_m4(data->camera_rotmat, (float (*)[4])ob->object_to_world().ptr());
  normalize_m3(data->camera_rotmat);
  /* To transform a plane which is in its homogeneous representation (4d vector),
   * we need the inverse of the transpose of the transform matrix... */
  copy_m4_m3(camera_rotmat_transposed_inversed, data->camera_rotmat);
  transpose_m4(camera_rotmat_transposed_inversed);
  invert_m4(camera_rotmat_transposed_inversed);

  /* Extract frustum planes from projection matrix. */
  planes_from_projmat(params->winmat,
                      data->plane_tx[Y_MIN],
                      data->plane_tx[Y_MAX],
                      data->plane_tx[Z_MIN],
                      data->plane_tx[Z_MAX],
                      nullptr,
                      nullptr);

  /* Rotate planes and get normals from them */
  for (uint i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
    mul_m4_v4(camera_rotmat_transposed_inversed, data->plane_tx[i]);
    /* Normalize. */
    data->plane_tx[i][3] /= normalize_v3(data->plane_tx[i]);

    data->dist_vals[i] = FLT_MAX;
  }

  data->tot = 0;
  data->do_zrange = params->is_ortho || do_clip_dists;

  if (data->do_zrange) {
    /* We want (0, 0, -1) transformed by camera_rotmat, this is a quicker shortcut. */
    negate_v3_v3(data->camera_no, data->camera_rotmat[2]);
    data->z_range[0] = FLT_MAX;
    data->z_range[1] = -FLT_MAX;
  }
}

static bool camera_frame_fit_calc_from_data(CameraParams *params,
                                            CameraViewFrameData *data,
                                            float r_co[3],
                                            float *r_scale,
                                            float *r_clip_start,
                                            float *r_clip_end)
{
  float plane_tx[CAMERA_VIEWFRAME_NUM_PLANES][4];

  if (data->tot <= 1) {
    return false;
  }

  if (params->is_ortho) {
    const float *cam_axis_x = data->camera_rotmat[0];
    const float *cam_axis_y = data->camera_rotmat[1];
    const float *cam_axis_z = data->camera_rotmat[2];
    const float *dists = data->dist_vals;
    const float dist_span_y = dists[Y_MIN] + dists[Y_MAX];
    const float dist_span_z = dists[Z_MIN] + dists[Z_MAX];
    const float dist_mid_y = (dists[Y_MIN] - dists[Y_MAX]) * 0.5f;
    const float dist_mid_z = (dists[Z_MIN] - dists[Z_MAX]) * 0.5f;
    const float scale_diff = (dist_span_z < dist_span_y) ?
                                 (dist_span_z * (BLI_rctf_size_x(&params->viewplane) /
                                                 BLI_rctf_size_y(&params->viewplane))) :
                                 (dist_span_y * (BLI_rctf_size_y(&params->viewplane) /
                                                 BLI_rctf_size_x(&params->viewplane)));

    *r_scale = params->ortho_scale - scale_diff;

    zero_v3(r_co);
    madd_v3_v3fl(r_co, cam_axis_x, dist_mid_y + (params->shiftx * scale_diff));
    madd_v3_v3fl(r_co, cam_axis_y, dist_mid_z + (params->shifty * scale_diff));
    madd_v3_v3fl(r_co, cam_axis_z, -(data->z_range[0] - 1.0f - params->clip_start));
  }
  else {
    float plane_isect_1[3], plane_isect_1_no[3], plane_isect_1_other[3];
    float plane_isect_2[3], plane_isect_2_no[3], plane_isect_2_other[3];

    float plane_isect_pt_1[3], plane_isect_pt_2[3];

    /* apply the dist-from-plane's to the transformed plane points */
    for (int i = 0; i < CAMERA_VIEWFRAME_NUM_PLANES; i++) {
      float co[3];
      mul_v3_v3fl(co, data->plane_tx[i], data->dist_vals[i]);
      plane_from_point_normal_v3(plane_tx[i], co, data->plane_tx[i]);
    }

    if (!isect_plane_plane_v3(plane_tx[Y_MIN], plane_tx[Y_MAX], plane_isect_1, plane_isect_1_no) ||
        !isect_plane_plane_v3(plane_tx[Z_MIN], plane_tx[Z_MAX], plane_isect_2, plane_isect_2_no))
    {
      return false;
    }

    add_v3_v3v3(plane_isect_1_other, plane_isect_1, plane_isect_1_no);
    add_v3_v3v3(plane_isect_2_other, plane_isect_2, plane_isect_2_no);

    if (!isect_line_line_v3(plane_isect_1,
                            plane_isect_1_other,
                            plane_isect_2,
                            plane_isect_2_other,
                            plane_isect_pt_1,
                            plane_isect_pt_2))
    {
      return false;
    }

    float cam_plane_no[3];
    float plane_isect_delta[3];

    const float shift_fac = BKE_camera_sensor_size(
                                params->sensor_fit, params->sensor_x, params->sensor_y) /
                            params->lens;

    /* we want (0, 0, -1) transformed by camera_rotmat, this is a quicker shortcut. */
    negate_v3_v3(cam_plane_no, data->camera_rotmat[2]);

    sub_v3_v3v3(plane_isect_delta, plane_isect_pt_2, plane_isect_pt_1);
    const float plane_isect_delta_len = len_v3(plane_isect_delta);

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
  }

  if (r_clip_start && r_clip_end) {
    const float z_offs = dot_v3v3(r_co, data->camera_no);
    *r_clip_start = data->z_range[0] - z_offs;
    *r_clip_end = data->z_range[1] - z_offs;
  }
  return true;
}

#undef Y_MIN
#undef Y_MAX
#undef Z_MIN
#undef Z_MAX

bool BKE_camera_view_frame_fit_to_scene(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *camera_ob,
                                        float r_co[3],
                                        float *r_scale,
                                        float *r_clip_start,
                                        float *r_clip_end)
{
  CameraParams params;
  CameraViewFrameData data_cb;

  /* just in case */
  *r_scale = 1.0f;

  camera_frame_fit_data_init(scene, camera_ob, r_clip_start && r_clip_end, &params, &data_cb);

  /* run callback on all visible points */
  BKE_scene_foreach_display_point(depsgraph, camera_to_frame_view_cb, &data_cb);

  return camera_frame_fit_calc_from_data(
      &params, &data_cb, r_co, r_scale, r_clip_start, r_clip_end);
}

bool BKE_camera_view_frame_fit_to_coords(const Depsgraph *depsgraph,
                                         const float (*cos)[3],
                                         int num_cos,
                                         Object *camera_ob,
                                         float r_co[3],
                                         float *r_scale)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *camera_ob_eval = DEG_get_evaluated(depsgraph, camera_ob);
  CameraParams params;
  CameraViewFrameData data_cb;

  /* just in case */
  *r_scale = 1.0f;

  camera_frame_fit_data_init(scene_eval, camera_ob_eval, false, &params, &data_cb);

  /* run callback on all given coordinates */
  while (num_cos--) {
    camera_to_frame_view_cb(cos[num_cos], &data_cb);
  }

  return camera_frame_fit_calc_from_data(&params, &data_cb, r_co, r_scale, nullptr, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Multi-View Matrix
 * \{ */

static void camera_model_matrix(const Object *camera, float r_modelmat[4][4])
{
  copy_m4_m4(r_modelmat, camera->object_to_world().ptr());
}

static void camera_stereo3d_model_matrix(const Object *camera,
                                         const bool is_left,
                                         float r_modelmat[4][4])
{
  const Camera *data = (const Camera *)camera->data;
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

  float size[3];
  mat4_to_size(size, camera->object_to_world().ptr());
  size_to_mat4(sizemat, size);

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
      normalize_m4_m4(r_modelmat, camera->object_to_world().ptr());
      mul_m4_m4m4(r_modelmat, r_modelmat, toeinmat);

      /* scale back to the original size */
      mul_m4_m4m4(r_modelmat, r_modelmat, sizemat);
    }
    else { /* CAM_S3D_PIVOT_LEFT, CAM_S3D_PIVOT_RIGHT */
      /* rotate perpendicular to the interocular line */
      normalize_m4_m4(r_modelmat, camera->object_to_world().ptr());
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
    normalize_m4_m4(r_modelmat, camera->object_to_world().ptr());

    /* translate - no rotation in CAM_S3D_OFFAXIS, CAM_S3D_PARALLEL */
    translate_m4(r_modelmat, -interocular_distance * fac_signed, 0.0f, 0.0f);

    /* scale back to the original size */
    mul_m4_m4m4(r_modelmat, r_modelmat, sizemat);
  }
}

void BKE_camera_multiview_view_matrix(const RenderData *rd,
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

void BKE_camera_multiview_model_matrix(const RenderData *rd,
                                       const Object *camera,
                                       const char *viewname,
                                       float r_modelmat[4][4])
{
  BKE_camera_multiview_model_matrix_scaled(rd, camera, viewname, r_modelmat);
  normalize_m4(r_modelmat);
}

void BKE_camera_multiview_model_matrix_scaled(const RenderData *rd,
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

void BKE_camera_multiview_window_matrix(const RenderData *rd,
                                        const Object *camera,
                                        const char *viewname,
                                        float r_winmat[4][4])
{
  CameraParams params;

  /* Setup parameters */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, camera);
  BKE_camera_multiview_params(rd, &params, camera, viewname);

  /* Compute matrix, view-plane, etc. */
  BKE_camera_params_compute_viewplane(&params, rd->xsch, rd->ysch, rd->xasp, rd->yasp);
  BKE_camera_params_compute_matrix(&params);

  copy_m4_m4(r_winmat, params.winmat);
}

bool BKE_camera_multiview_spherical_stereo(const RenderData *rd, const Object *camera)
{
  const bool is_multiview = (rd && rd->scemode & R_MULTIVIEW) != 0;

  if (!is_multiview) {
    return false;
  }

  if (camera->type != OB_CAMERA) {
    return false;
  }

  const Camera *cam = static_cast<const Camera *>(camera->data);

  if ((rd->views_format == SCE_VIEWS_FORMAT_STEREO_3D) &&
      ELEM(cam->type, CAM_PANO, CAM_PERSP, CAM_CUSTOM) &&
      ((cam->stereo.flag & CAM_S3D_SPHERICAL) != 0))
  {
    return true;
  }

  return false;
}

static Object *camera_multiview_advanced(const Scene *scene, Object *camera, const char *suffix)
{
  char name[MAX_NAME];
  const char *camera_name = camera->id.name + 2;
  const int len_name = strlen(camera_name);
  int len_suffix_max = -1;

  name[0] = '\0';

  /* we need to take the better match, thus the len_suffix_max test */
  LISTBASE_FOREACH (const SceneRenderView *, srv, &scene->r.views) {
    const int len_suffix = strlen(srv->suffix);

    if ((len_suffix < len_suffix_max) || (len_name < len_suffix)) {
      continue;
    }

    if (STREQ(camera_name + (len_name - len_suffix), srv->suffix)) {
      SNPRINTF(name, "%.*s%s", (len_name - len_suffix), camera_name, suffix);
      len_suffix_max = len_suffix;
    }
  }

  if (name[0] != '\0') {
    Object *ob = BKE_scene_object_find_by_name(scene, name);
    if (ob != nullptr) {
      return ob;
    }
  }

  return camera;
}

Object *BKE_camera_multiview_render(const Scene *scene, Object *camera, const char *viewname)
{
  const bool is_multiview = (camera != nullptr) && (scene->r.scemode & R_MULTIVIEW) != 0;

  if (!is_multiview) {
    return camera;
  }
  if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    return camera;
  }
  /* SCE_VIEWS_FORMAT_MULTIVIEW */
  const char *suffix = BKE_scene_multiview_view_suffix_get(&scene->r, viewname);
  return camera_multiview_advanced(scene, camera, suffix);
}

static float camera_stereo3d_shift_x(const Object *camera, const char *viewname)
{
  const Camera *data = static_cast<const Camera *>(camera->data);
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

float BKE_camera_multiview_shift_x(const RenderData *rd,
                                   const Object *camera,
                                   const char *viewname)
{
  const bool is_multiview = (rd && rd->scemode & R_MULTIVIEW) != 0;
  const Camera *data = static_cast<const Camera *>(camera->data);

  BLI_assert(camera->type == OB_CAMERA);

  if (!is_multiview) {
    return data->shiftx;
  }
  if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
    return data->shiftx;
  }
  if (data->type == CAM_PANO) {
    return data->shiftx;
  }
  /* SCE_VIEWS_SETUP_BASIC */
  return camera_stereo3d_shift_x(camera, viewname);
}

void BKE_camera_multiview_params(const RenderData *rd,
                                 CameraParams *params,
                                 const Object *camera,
                                 const char *viewname)
{
  if (camera->type == OB_CAMERA) {
    params->shiftx = BKE_camera_multiview_shift_x(rd, camera, viewname);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Background Image
 * \{ */

CameraBGImage *BKE_camera_background_image_new(Camera *cam)
{
  CameraBGImage *bgpic = MEM_callocN<CameraBGImage>("Background Image");

  bgpic->scale = 1.0f;
  bgpic->alpha = 0.5f;
  bgpic->iuser.flag |= IMA_ANIM_ALWAYS;
  bgpic->flag |= CAM_BGIMG_FLAG_EXPANDED | CAM_BGIMG_FLAG_CAMERA_ASPECT |
                 CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL;

  BLI_addtail(&cam->bg_images, bgpic);

  return bgpic;
}

CameraBGImage *BKE_camera_background_image_copy(const CameraBGImage *bgpic_src, const int flag)
{
  CameraBGImage *bgpic_dst = static_cast<CameraBGImage *>(MEM_dupallocN(bgpic_src));

  bgpic_dst->next = bgpic_dst->prev = nullptr;

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)bgpic_dst->ima);
    id_us_plus((ID *)bgpic_dst->clip);
  }

  if ((flag & LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG) == 0) {
    bgpic_dst->flag |= CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL;
  }

  return bgpic_dst;
}

void BKE_camera_background_image_remove(Camera *cam, CameraBGImage *bgpic)
{
  BLI_remlink(&cam->bg_images, bgpic);

  MEM_freeN(bgpic);
}

void BKE_camera_background_image_clear(Camera *cam)
{
  CameraBGImage *bgpic = static_cast<CameraBGImage *>(cam->bg_images.first);

  while (bgpic) {
    CameraBGImage *next_bgpic = bgpic->next;

    BKE_camera_background_image_remove(cam, bgpic);

    bgpic = next_bgpic;
  }
}

/** \} */
