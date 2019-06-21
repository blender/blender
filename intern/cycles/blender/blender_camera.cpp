/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/camera.h"
#include "render/scene.h"

#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

/* Blender Camera Intermediate: we first convert both the offline and 3d view
 * render camera to this, and from there convert to our native camera format. */

struct BlenderCamera {
  float nearclip;
  float farclip;

  CameraType type;
  float ortho_scale;

  float lens;
  float shuttertime;
  Camera::MotionPosition motion_position;
  array<float> shutter_curve;

  Camera::RollingShutterType rolling_shutter_type;
  float rolling_shutter_duration;

  float aperturesize;
  uint apertureblades;
  float aperturerotation;
  float focaldistance;

  float2 shift;
  float2 offset;
  float zoom;

  float2 pixelaspect;

  float aperture_ratio;

  PanoramaType panorama_type;
  float fisheye_fov;
  float fisheye_lens;
  float latitude_min;
  float latitude_max;
  float longitude_min;
  float longitude_max;
  bool use_spherical_stereo;
  float interocular_distance;
  float convergence_distance;
  bool use_pole_merge;
  float pole_merge_angle_from;
  float pole_merge_angle_to;

  enum { AUTO, HORIZONTAL, VERTICAL } sensor_fit;
  float sensor_width;
  float sensor_height;

  int full_width;
  int full_height;

  BoundBox2D border;
  BoundBox2D pano_viewplane;
  BoundBox2D viewport_camera_border;

  Transform matrix;

  float offscreen_dicing_scale;

  int motion_steps;
};

static void blender_camera_init(BlenderCamera *bcam, BL::RenderSettings &b_render)
{
  memset((void *)bcam, 0, sizeof(BlenderCamera));

  bcam->nearclip = 1e-5f;
  bcam->farclip = 1e5f;

  bcam->type = CAMERA_PERSPECTIVE;
  bcam->ortho_scale = 1.0f;

  bcam->lens = 50.0f;
  bcam->shuttertime = 1.0f;

  bcam->rolling_shutter_type = Camera::ROLLING_SHUTTER_NONE;
  bcam->rolling_shutter_duration = 0.1f;

  bcam->aperturesize = 0.0f;
  bcam->apertureblades = 0;
  bcam->aperturerotation = 0.0f;
  bcam->focaldistance = 10.0f;

  bcam->zoom = 1.0f;
  bcam->pixelaspect = make_float2(1.0f, 1.0f);
  bcam->aperture_ratio = 1.0f;

  bcam->sensor_width = 36.0f;
  bcam->sensor_height = 24.0f;
  bcam->sensor_fit = BlenderCamera::AUTO;
  bcam->motion_position = Camera::MOTION_POSITION_CENTER;
  bcam->border.right = 1.0f;
  bcam->border.top = 1.0f;
  bcam->pano_viewplane.right = 1.0f;
  bcam->pano_viewplane.top = 1.0f;
  bcam->viewport_camera_border.right = 1.0f;
  bcam->viewport_camera_border.top = 1.0f;
  bcam->offscreen_dicing_scale = 1.0f;
  bcam->matrix = transform_identity();

  /* render resolution */
  bcam->full_width = render_resolution_x(b_render);
  bcam->full_height = render_resolution_y(b_render);
}

static float blender_camera_focal_distance(BL::RenderEngine &b_engine,
                                           BL::Object &b_ob,
                                           BL::Camera &b_camera,
                                           BlenderCamera *bcam)
{
  BL::Object b_dof_object = b_camera.dof().focus_object();

  if (!b_dof_object)
    return b_camera.dof().focus_distance();

  /* for dof object, return distance along camera Z direction */
  BL::Array<float, 16> b_ob_matrix;
  b_engine.camera_model_matrix(b_ob, bcam->use_spherical_stereo, b_ob_matrix);
  Transform obmat = transform_clear_scale(get_transform(b_ob_matrix));
  Transform dofmat = get_transform(b_dof_object.matrix_world());
  float3 view_dir = normalize(transform_get_column(&obmat, 2));
  float3 dof_dir = transform_get_column(&obmat, 3) - transform_get_column(&dofmat, 3);
  return fabsf(dot(view_dir, dof_dir));
}

static void blender_camera_from_object(BlenderCamera *bcam,
                                       BL::RenderEngine &b_engine,
                                       BL::Object &b_ob,
                                       bool skip_panorama = false)
{
  BL::ID b_ob_data = b_ob.data();

  if (b_ob_data.is_a(&RNA_Camera)) {
    BL::Camera b_camera(b_ob_data);
    PointerRNA ccamera = RNA_pointer_get(&b_camera.ptr, "cycles");

    bcam->nearclip = b_camera.clip_start();
    bcam->farclip = b_camera.clip_end();

    switch (b_camera.type()) {
      case BL::Camera::type_ORTHO:
        bcam->type = CAMERA_ORTHOGRAPHIC;
        break;
      case BL::Camera::type_PANO:
        if (!skip_panorama)
          bcam->type = CAMERA_PANORAMA;
        else
          bcam->type = CAMERA_PERSPECTIVE;
        break;
      case BL::Camera::type_PERSP:
      default:
        bcam->type = CAMERA_PERSPECTIVE;
        break;
    }

    bcam->panorama_type = (PanoramaType)get_enum(
        ccamera, "panorama_type", PANORAMA_NUM_TYPES, PANORAMA_EQUIRECTANGULAR);

    bcam->fisheye_fov = RNA_float_get(&ccamera, "fisheye_fov");
    bcam->fisheye_lens = RNA_float_get(&ccamera, "fisheye_lens");
    bcam->latitude_min = RNA_float_get(&ccamera, "latitude_min");
    bcam->latitude_max = RNA_float_get(&ccamera, "latitude_max");
    bcam->longitude_min = RNA_float_get(&ccamera, "longitude_min");
    bcam->longitude_max = RNA_float_get(&ccamera, "longitude_max");

    bcam->interocular_distance = b_camera.stereo().interocular_distance();
    if (b_camera.stereo().convergence_mode() == BL::CameraStereoData::convergence_mode_PARALLEL) {
      bcam->convergence_distance = FLT_MAX;
    }
    else {
      bcam->convergence_distance = b_camera.stereo().convergence_distance();
    }
    bcam->use_spherical_stereo = b_engine.use_spherical_stereo(b_ob);

    bcam->use_pole_merge = b_camera.stereo().use_pole_merge();
    bcam->pole_merge_angle_from = b_camera.stereo().pole_merge_angle_from();
    bcam->pole_merge_angle_to = b_camera.stereo().pole_merge_angle_to();

    bcam->ortho_scale = b_camera.ortho_scale();

    bcam->lens = b_camera.lens();

    if (b_camera.dof().use_dof()) {
      /* allow f/stop number to change aperture_size but still
       * give manual control over aperture radius */
      float fstop = b_camera.dof().aperture_fstop();
      fstop = max(fstop, 1e-5f);

      if (bcam->type == CAMERA_ORTHOGRAPHIC)
        bcam->aperturesize = 1.0f / (2.0f * fstop);
      else
        bcam->aperturesize = (bcam->lens * 1e-3f) / (2.0f * fstop);

      bcam->apertureblades = b_camera.dof().aperture_blades();
      bcam->aperturerotation = b_camera.dof().aperture_rotation();
      bcam->focaldistance = blender_camera_focal_distance(b_engine, b_ob, b_camera, bcam);
      bcam->aperture_ratio = b_camera.dof().aperture_ratio();
    }
    else {
      /* DOF is turned of for the camera. */
      bcam->aperturesize = 0.0f;
      bcam->apertureblades = 0;
      bcam->aperturerotation = 0.0f;
      bcam->focaldistance = 0.0f;
      bcam->aperture_ratio = 1.0f;
    }

    bcam->shift.x = b_engine.camera_shift_x(b_ob, bcam->use_spherical_stereo);
    bcam->shift.y = b_camera.shift_y();

    bcam->sensor_width = b_camera.sensor_width();
    bcam->sensor_height = b_camera.sensor_height();

    if (b_camera.sensor_fit() == BL::Camera::sensor_fit_AUTO)
      bcam->sensor_fit = BlenderCamera::AUTO;
    else if (b_camera.sensor_fit() == BL::Camera::sensor_fit_HORIZONTAL)
      bcam->sensor_fit = BlenderCamera::HORIZONTAL;
    else
      bcam->sensor_fit = BlenderCamera::VERTICAL;
  }
  else if (b_ob_data.is_a(&RNA_Light)) {
    /* Can also look through spot light. */
    BL::SpotLight b_light(b_ob_data);
    float lens = 16.0f / tanf(b_light.spot_size() * 0.5f);
    if (lens > 0.0f) {
      bcam->lens = lens;
    }
  }

  bcam->motion_steps = object_motion_steps(b_ob, b_ob);
}

static Transform blender_camera_matrix(const Transform &tfm,
                                       const CameraType type,
                                       const PanoramaType panorama_type)
{
  Transform result;

  if (type == CAMERA_PANORAMA) {
    if (panorama_type == PANORAMA_MIRRORBALL) {
      /* Mirror ball camera is looking into the negative Y direction
       * which matches texture mirror ball mapping.
       */
      result = tfm * make_transform(
                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    }
    else {
      /* Make it so environment camera needs to be pointed in the direction
       * of the positive x-axis to match an environment texture, this way
       * it is looking at the center of the texture
       */
      result = tfm * make_transform(
                         0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
    }
  }
  else {
    /* note the blender camera points along the negative z-axis */
    result = tfm * transform_scale(1.0f, 1.0f, -1.0f);
  }

  return transform_clear_scale(result);
}

static void blender_camera_viewplane(BlenderCamera *bcam,
                                     int width,
                                     int height,
                                     BoundBox2D *viewplane,
                                     float *aspectratio,
                                     float *sensor_size)
{
  /* dimensions */
  float xratio = (float)width * bcam->pixelaspect.x;
  float yratio = (float)height * bcam->pixelaspect.y;

  /* compute x/y aspect and ratio */
  float xaspect, yaspect;
  bool horizontal_fit;

  /* sensor fitting */
  if (bcam->sensor_fit == BlenderCamera::AUTO) {
    horizontal_fit = (xratio > yratio);
    if (sensor_size != NULL) {
      *sensor_size = bcam->sensor_width;
    }
  }
  else if (bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
    horizontal_fit = true;
    if (sensor_size != NULL) {
      *sensor_size = bcam->sensor_width;
    }
  }
  else {
    horizontal_fit = false;
    if (sensor_size != NULL) {
      *sensor_size = bcam->sensor_height;
    }
  }

  if (horizontal_fit) {
    if (aspectratio != NULL) {
      *aspectratio = xratio / yratio;
    }
    xaspect = *aspectratio;
    yaspect = 1.0f;
  }
  else {
    if (aspectratio != NULL) {
      *aspectratio = yratio / xratio;
    }
    xaspect = 1.0f;
    yaspect = *aspectratio;
  }

  /* modify aspect for orthographic scale */
  if (bcam->type == CAMERA_ORTHOGRAPHIC) {
    xaspect = xaspect * bcam->ortho_scale / (*aspectratio * 2.0f);
    yaspect = yaspect * bcam->ortho_scale / (*aspectratio * 2.0f);
    if (aspectratio != NULL) {
      *aspectratio = bcam->ortho_scale / 2.0f;
    }
  }

  if (bcam->type == CAMERA_PANORAMA) {
    /* set viewplane */
    if (viewplane != NULL) {
      *viewplane = bcam->pano_viewplane;
    }
  }
  else {
    /* set viewplane */
    if (viewplane != NULL) {
      viewplane->left = -xaspect;
      viewplane->right = xaspect;
      viewplane->bottom = -yaspect;
      viewplane->top = yaspect;

      /* zoom for 3d camera view */
      *viewplane = (*viewplane) * bcam->zoom;

      /* modify viewplane with camera shift and 3d camera view offset */
      float dx = 2.0f * (*aspectratio * bcam->shift.x + bcam->offset.x * xaspect * 2.0f);
      float dy = 2.0f * (*aspectratio * bcam->shift.y + bcam->offset.y * yaspect * 2.0f);

      viewplane->left += dx;
      viewplane->right += dx;
      viewplane->bottom += dy;
      viewplane->top += dy;
    }
  }
}

static void blender_camera_sync(Camera *cam,
                                BlenderCamera *bcam,
                                int width,
                                int height,
                                const char *viewname,
                                PointerRNA *cscene)
{
  /* copy camera to compare later */
  Camera prevcam = *cam;
  float aspectratio, sensor_size;

  /* viewplane */
  blender_camera_viewplane(bcam, width, height, &cam->viewplane, &aspectratio, &sensor_size);

  cam->width = bcam->full_width;
  cam->height = bcam->full_height;

  cam->full_width = width;
  cam->full_height = height;

  /* panorama sensor */
  if (bcam->type == CAMERA_PANORAMA && bcam->panorama_type == PANORAMA_FISHEYE_EQUISOLID) {
    float fit_xratio = (float)bcam->full_width * bcam->pixelaspect.x;
    float fit_yratio = (float)bcam->full_height * bcam->pixelaspect.y;
    bool horizontal_fit;
    float sensor_size;

    if (bcam->sensor_fit == BlenderCamera::AUTO) {
      horizontal_fit = (fit_xratio > fit_yratio);
      sensor_size = bcam->sensor_width;
    }
    else if (bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
      horizontal_fit = true;
      sensor_size = bcam->sensor_width;
    }
    else { /* vertical */
      horizontal_fit = false;
      sensor_size = bcam->sensor_height;
    }

    if (horizontal_fit) {
      cam->sensorwidth = sensor_size;
      cam->sensorheight = sensor_size * fit_yratio / fit_xratio;
    }
    else {
      cam->sensorwidth = sensor_size * fit_xratio / fit_yratio;
      cam->sensorheight = sensor_size;
    }
  }

  /* clipping distances */
  cam->nearclip = bcam->nearclip;
  cam->farclip = bcam->farclip;

  /* type */
  cam->type = bcam->type;

  /* panorama */
  cam->panorama_type = bcam->panorama_type;
  cam->fisheye_fov = bcam->fisheye_fov;
  cam->fisheye_lens = bcam->fisheye_lens;
  cam->latitude_min = bcam->latitude_min;
  cam->latitude_max = bcam->latitude_max;

  cam->longitude_min = bcam->longitude_min;
  cam->longitude_max = bcam->longitude_max;

  /* panorama stereo */
  cam->interocular_distance = bcam->interocular_distance;
  cam->convergence_distance = bcam->convergence_distance;
  cam->use_spherical_stereo = bcam->use_spherical_stereo;

  if (cam->use_spherical_stereo) {
    if (strcmp(viewname, "left") == 0)
      cam->stereo_eye = Camera::STEREO_LEFT;
    else if (strcmp(viewname, "right") == 0)
      cam->stereo_eye = Camera::STEREO_RIGHT;
    else
      cam->stereo_eye = Camera::STEREO_NONE;
  }

  cam->use_pole_merge = bcam->use_pole_merge;
  cam->pole_merge_angle_from = bcam->pole_merge_angle_from;
  cam->pole_merge_angle_to = bcam->pole_merge_angle_to;

  /* anamorphic lens bokeh */
  cam->aperture_ratio = bcam->aperture_ratio;

  /* perspective */
  cam->fov = 2.0f * atanf((0.5f * sensor_size) / bcam->lens / aspectratio);
  cam->focaldistance = bcam->focaldistance;
  cam->aperturesize = bcam->aperturesize;
  cam->blades = bcam->apertureblades;
  cam->bladesrotation = bcam->aperturerotation;

  /* transform */
  cam->matrix = blender_camera_matrix(bcam->matrix, bcam->type, bcam->panorama_type);
  cam->motion.clear();
  cam->motion.resize(bcam->motion_steps, cam->matrix);
  cam->use_perspective_motion = false;
  cam->shuttertime = bcam->shuttertime;
  cam->fov_pre = cam->fov;
  cam->fov_post = cam->fov;
  cam->motion_position = bcam->motion_position;

  cam->rolling_shutter_type = bcam->rolling_shutter_type;
  cam->rolling_shutter_duration = bcam->rolling_shutter_duration;

  cam->shutter_curve = bcam->shutter_curve;

  /* border */
  cam->border = bcam->border;
  cam->viewport_camera_border = bcam->viewport_camera_border;

  bcam->offscreen_dicing_scale = RNA_float_get(cscene, "offscreen_dicing_scale");
  cam->offscreen_dicing_scale = bcam->offscreen_dicing_scale;

  /* set update flag */
  if (cam->modified(prevcam))
    cam->tag_update();
}

/* Sync Render Camera */

void BlenderSync::sync_camera(BL::RenderSettings &b_render,
                              BL::Object &b_override,
                              int width,
                              int height,
                              const char *viewname)
{
  BlenderCamera bcam;
  blender_camera_init(&bcam, b_render);

  /* pixel aspect */
  bcam.pixelaspect.x = b_render.pixel_aspect_x();
  bcam.pixelaspect.y = b_render.pixel_aspect_y();
  bcam.shuttertime = b_render.motion_blur_shutter();

  BL::CurveMapping b_shutter_curve(b_render.motion_blur_shutter_curve());
  curvemapping_to_array(b_shutter_curve, bcam.shutter_curve, RAMP_TABLE_SIZE);

  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  bcam.motion_position = (Camera::MotionPosition)get_enum(cscene,
                                                          "motion_blur_position",
                                                          Camera::MOTION_NUM_POSITIONS,
                                                          Camera::MOTION_POSITION_CENTER);
  bcam.rolling_shutter_type = (Camera::RollingShutterType)get_enum(
      cscene,
      "rolling_shutter_type",
      Camera::ROLLING_SHUTTER_NUM_TYPES,
      Camera::ROLLING_SHUTTER_NONE);
  bcam.rolling_shutter_duration = RNA_float_get(&cscene, "rolling_shutter_duration");

  /* border */
  if (b_render.use_border()) {
    bcam.border.left = b_render.border_min_x();
    bcam.border.right = b_render.border_max_x();
    bcam.border.bottom = b_render.border_min_y();
    bcam.border.top = b_render.border_max_y();
  }

  /* camera object */
  BL::Object b_ob = b_scene.camera();

  if (b_override)
    b_ob = b_override;

  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);
  }

  /* sync */
  Camera *cam = scene->camera;
  blender_camera_sync(cam, &bcam, width, height, viewname, &cscene);

  /* dicing camera */
  b_ob = BL::Object(RNA_pointer_get(&cscene, "dicing_camera"));
  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);

    blender_camera_sync(scene->dicing_camera, &bcam, width, height, viewname, &cscene);
  }
  else {
    *scene->dicing_camera = *cam;
  }
}

void BlenderSync::sync_camera_motion(
    BL::RenderSettings &b_render, BL::Object &b_ob, int width, int height, float motion_time)
{
  if (!b_ob)
    return;

  Camera *cam = scene->camera;
  BL::Array<float, 16> b_ob_matrix;
  b_engine.camera_model_matrix(b_ob, cam->use_spherical_stereo, b_ob_matrix);
  Transform tfm = get_transform(b_ob_matrix);
  tfm = blender_camera_matrix(tfm, cam->type, cam->panorama_type);

  if (motion_time == 0.0f) {
    /* When motion blur is not centered in frame, cam->matrix gets reset. */
    cam->matrix = tfm;
  }

  /* Set transform in motion array. */
  int motion_step = cam->motion_step(motion_time);
  if (motion_step >= 0) {
    cam->motion[motion_step] = tfm;
  }

  if (cam->type == CAMERA_PERSPECTIVE) {
    BlenderCamera bcam;
    float aspectratio, sensor_size;
    blender_camera_init(&bcam, b_render);

    /* TODO(sergey): Consider making it a part of blender_camera_init(). */
    bcam.pixelaspect.x = b_render.pixel_aspect_x();
    bcam.pixelaspect.y = b_render.pixel_aspect_y();

    blender_camera_from_object(&bcam, b_engine, b_ob);
    blender_camera_viewplane(&bcam, width, height, NULL, &aspectratio, &sensor_size);
    /* TODO(sergey): De-duplicate calculation with camera sync. */
    float fov = 2.0f * atanf((0.5f * sensor_size) / bcam.lens / aspectratio);
    if (fov != cam->fov) {
      VLOG(1) << "Camera " << b_ob.name() << " FOV change detected.";
      if (motion_time == 0.0f) {
        cam->fov = fov;
      }
      else if (motion_time == -1.0f) {
        cam->fov_pre = fov;
        cam->use_perspective_motion = true;
      }
      else if (motion_time == 1.0f) {
        cam->fov_post = fov;
        cam->use_perspective_motion = true;
      }
    }
  }
}

/* Sync 3D View Camera */

static void blender_camera_view_subset(BL::RenderEngine &b_engine,
                                       BL::RenderSettings &b_render,
                                       BL::Scene &b_scene,
                                       BL::Object &b_ob,
                                       BL::SpaceView3D &b_v3d,
                                       BL::RegionView3D &b_rv3d,
                                       int width,
                                       int height,
                                       BoundBox2D *view_box,
                                       BoundBox2D *cam_box);

static void blender_camera_from_view(BlenderCamera *bcam,
                                     BL::RenderEngine &b_engine,
                                     BL::Scene &b_scene,
                                     BL::SpaceView3D &b_v3d,
                                     BL::RegionView3D &b_rv3d,
                                     int width,
                                     int height,
                                     bool skip_panorama = false)
{
  /* 3d view parameters */
  bcam->nearclip = b_v3d.clip_start();
  bcam->farclip = b_v3d.clip_end();
  bcam->lens = b_v3d.lens();
  bcam->shuttertime = b_scene.render().motion_blur_shutter();

  BL::CurveMapping b_shutter_curve(b_scene.render().motion_blur_shutter_curve());
  curvemapping_to_array(b_shutter_curve, bcam->shutter_curve, RAMP_TABLE_SIZE);

  if (b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA) {
    /* camera view */
    BL::Object b_ob = (b_v3d.use_local_camera()) ? b_v3d.camera() : b_scene.camera();

    if (b_ob) {
      blender_camera_from_object(bcam, b_engine, b_ob, skip_panorama);

      if (!skip_panorama && bcam->type == CAMERA_PANORAMA) {
        /* in panorama camera view, we map viewplane to camera border */
        BoundBox2D view_box, cam_box;

        BL::RenderSettings b_render_settings(b_scene.render());
        blender_camera_view_subset(b_engine,
                                   b_render_settings,
                                   b_scene,
                                   b_ob,
                                   b_v3d,
                                   b_rv3d,
                                   width,
                                   height,
                                   &view_box,
                                   &cam_box);

        bcam->pano_viewplane = view_box.make_relative_to(cam_box);
      }
      else {
        /* magic zoom formula */
        bcam->zoom = (float)b_rv3d.view_camera_zoom();
        bcam->zoom = (1.41421f + bcam->zoom / 50.0f);
        bcam->zoom *= bcam->zoom;
        bcam->zoom = 2.0f / bcam->zoom;

        /* offset */
        bcam->offset = get_float2(b_rv3d.view_camera_offset());
      }
    }
  }
  else if (b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_ORTHO) {
    /* orthographic view */
    bcam->farclip *= 0.5f;
    bcam->nearclip = -bcam->farclip;

    float sensor_size;
    if (bcam->sensor_fit == BlenderCamera::VERTICAL)
      sensor_size = bcam->sensor_height;
    else
      sensor_size = bcam->sensor_width;

    bcam->type = CAMERA_ORTHOGRAPHIC;
    bcam->ortho_scale = b_rv3d.view_distance() * sensor_size / b_v3d.lens();
  }

  bcam->zoom *= 2.0f;

  /* 3d view transform */
  bcam->matrix = transform_inverse(get_transform(b_rv3d.view_matrix()));
}

static void blender_camera_view_subset(BL::RenderEngine &b_engine,
                                       BL::RenderSettings &b_render,
                                       BL::Scene &b_scene,
                                       BL::Object &b_ob,
                                       BL::SpaceView3D &b_v3d,
                                       BL::RegionView3D &b_rv3d,
                                       int width,
                                       int height,
                                       BoundBox2D *view_box,
                                       BoundBox2D *cam_box)
{
  BoundBox2D cam, view;
  float view_aspect, cam_aspect, sensor_size;

  /* get viewport viewplane */
  BlenderCamera view_bcam;
  blender_camera_init(&view_bcam, b_render);
  blender_camera_from_view(&view_bcam, b_engine, b_scene, b_v3d, b_rv3d, width, height, true);

  blender_camera_viewplane(&view_bcam, width, height, &view, &view_aspect, &sensor_size);

  /* get camera viewplane */
  BlenderCamera cam_bcam;
  blender_camera_init(&cam_bcam, b_render);
  blender_camera_from_object(&cam_bcam, b_engine, b_ob, true);

  blender_camera_viewplane(
      &cam_bcam, cam_bcam.full_width, cam_bcam.full_height, &cam, &cam_aspect, &sensor_size);

  /* return */
  *view_box = view * (1.0f / view_aspect);
  *cam_box = cam * (1.0f / cam_aspect);
}

static void blender_camera_border_subset(BL::RenderEngine &b_engine,
                                         BL::RenderSettings &b_render,
                                         BL::Scene &b_scene,
                                         BL::SpaceView3D &b_v3d,
                                         BL::RegionView3D &b_rv3d,
                                         BL::Object &b_ob,
                                         int width,
                                         int height,
                                         const BoundBox2D &border,
                                         BoundBox2D *result)
{
  /* Determine camera viewport subset. */
  BoundBox2D view_box, cam_box;
  blender_camera_view_subset(
      b_engine, b_render, b_scene, b_ob, b_v3d, b_rv3d, width, height, &view_box, &cam_box);

  /* Determine viewport subset matching given border. */
  cam_box = cam_box.make_relative_to(view_box);
  *result = cam_box.subset(border);
}

static void blender_camera_border(BlenderCamera *bcam,
                                  BL::RenderEngine &b_engine,
                                  BL::RenderSettings &b_render,
                                  BL::Scene &b_scene,
                                  BL::SpaceView3D &b_v3d,
                                  BL::RegionView3D &b_rv3d,
                                  int width,
                                  int height)
{
  bool is_camera_view;

  /* camera view? */
  is_camera_view = b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA;

  if (!is_camera_view) {
    /* for non-camera view check whether render border is enabled for viewport
     * and if so use border from 3d viewport
     * assume viewport has got correctly clamped border already
     */
    if (b_v3d.use_render_border()) {
      bcam->border.left = b_v3d.render_border_min_x();
      bcam->border.right = b_v3d.render_border_max_x();
      bcam->border.bottom = b_v3d.render_border_min_y();
      bcam->border.top = b_v3d.render_border_max_y();
    }
    return;
  }

  BL::Object b_ob = (b_v3d.use_local_camera()) ? b_v3d.camera() : b_scene.camera();

  if (!b_ob)
    return;

  /* Determine camera border inside the viewport. */
  BoundBox2D full_border;
  blender_camera_border_subset(b_engine,
                               b_render,
                               b_scene,
                               b_v3d,
                               b_rv3d,
                               b_ob,
                               width,
                               height,
                               full_border,
                               &bcam->viewport_camera_border);

  if (!b_render.use_border()) {
    return;
  }

  bcam->border.left = b_render.border_min_x();
  bcam->border.right = b_render.border_max_x();
  bcam->border.bottom = b_render.border_min_y();
  bcam->border.top = b_render.border_max_y();

  /* Determine viewport subset matching camera border. */
  blender_camera_border_subset(b_engine,
                               b_render,
                               b_scene,
                               b_v3d,
                               b_rv3d,
                               b_ob,
                               width,
                               height,
                               bcam->border,
                               &bcam->border);
  bcam->border = bcam->border.clamp();
}

void BlenderSync::sync_view(BL::SpaceView3D &b_v3d,
                            BL::RegionView3D &b_rv3d,
                            int width,
                            int height)
{
  BlenderCamera bcam;
  BL::RenderSettings b_render_settings(b_scene.render());
  blender_camera_init(&bcam, b_render_settings);
  blender_camera_from_view(&bcam, b_engine, b_scene, b_v3d, b_rv3d, width, height);
  blender_camera_border(&bcam, b_engine, b_render_settings, b_scene, b_v3d, b_rv3d, width, height);
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  blender_camera_sync(scene->camera, &bcam, width, height, "", &cscene);

  /* dicing camera */
  BL::Object b_ob = BL::Object(RNA_pointer_get(&cscene, "dicing_camera"));
  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);

    blender_camera_sync(scene->dicing_camera, &bcam, width, height, "", &cscene);
  }
  else {
    *scene->dicing_camera = *scene->camera;
  }
}

BufferParams BlenderSync::get_buffer_params(BL::RenderSettings &b_render,
                                            BL::SpaceView3D &b_v3d,
                                            BL::RegionView3D &b_rv3d,
                                            Camera *cam,
                                            int width,
                                            int height)
{
  BufferParams params;
  bool use_border = false;

  params.full_width = width;
  params.full_height = height;

  if (b_v3d && b_rv3d && b_rv3d.view_perspective() != BL::RegionView3D::view_perspective_CAMERA)
    use_border = b_v3d.use_render_border();
  else
    use_border = b_render.use_border();

  if (use_border) {
    /* border render */
    /* the viewport may offset the border outside the view */
    BoundBox2D border = cam->border.clamp();
    params.full_x = (int)(border.left * (float)width);
    params.full_y = (int)(border.bottom * (float)height);
    params.width = (int)(border.right * (float)width) - params.full_x;
    params.height = (int)(border.top * (float)height) - params.full_y;

    /* survive in case border goes out of view or becomes too small */
    params.width = max(params.width, 1);
    params.height = max(params.height, 1);
  }
  else {
    params.width = width;
    params.height = height;
  }

  return params;
}

CCL_NAMESPACE_END
