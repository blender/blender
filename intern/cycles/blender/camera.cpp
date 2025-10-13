/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/camera.h"
#include "scene/bake.h"
#include "scene/osl.h"
#include "scene/scene.h"

#include "blender/sync.h"
#include "blender/util.h"

#include "util/log.h"

CCL_NAMESPACE_BEGIN

/* Blender Camera Intermediate: we first convert both the offline and 3d view
 * render camera to this, and from there convert to our native camera format. */

class BlenderCamera {
 public:
  explicit BlenderCamera(BL::RenderSettings &b_render)
  {
    full_width = render_width = render_resolution_x(b_render);
    full_height = render_height = render_resolution_y(b_render);
  };

  PointerRNA custom_props;
  string custom_bytecode;
  string custom_bytecode_hash;
  string custom_filepath;

  float nearclip = 1e-5f;
  float farclip = 1e5f;

  CameraType type = CAMERA_PERSPECTIVE;
  float ortho_scale = 1.0f;

  float lens = 50.0f;
  float shuttertime = 1.0f;
  MotionPosition motion_position = MOTION_POSITION_CENTER;
  array<float> shutter_curve;

  Camera::RollingShutterType rolling_shutter_type = Camera::ROLLING_SHUTTER_NONE;
  float rolling_shutter_duration = 0.1f;

  float aperturesize = 0.0f;
  uint apertureblades = 0;
  float aperturerotation = 0.0f;
  float focaldistance = 10.0f;

  float2 shift = zero_float2();
  float2 offset = zero_float2();
  float zoom = 1.0f;

  float2 pixelaspect = one_float2();

  float aperture_ratio = 1.0f;

  PanoramaType panorama_type = PANORAMA_EQUIRECTANGULAR;
  float fisheye_fov = M_PI_F;
  float fisheye_lens = 10.5f;
  float latitude_min = -M_PI_2_F;
  float latitude_max = M_PI_2_F;
  float longitude_min = -M_PI_F;
  float longitude_max = M_PI_F;
  bool use_spherical_stereo = false;
  float interocular_distance = 0.065f;
  float convergence_distance = 30.0f * 0.065f;
  bool use_pole_merge = false;
  float pole_merge_angle_from = (60.0f * M_PI_F / 180.0f);
  float pole_merge_angle_to = (75.0f * M_PI_F / 180.0f);

  float fisheye_polynomial_k0 = 0.0f;
  float fisheye_polynomial_k1 = 0.0f;
  float fisheye_polynomial_k2 = 0.0f;
  float fisheye_polynomial_k3 = 0.0f;
  float fisheye_polynomial_k4 = 0.0f;

  float central_cylindrical_range_u_min = -M_PI_F;
  float central_cylindrical_range_u_max = M_PI_F;
  float central_cylindrical_range_v_min = -1.0f;
  float central_cylindrical_range_v_max = 1.0f;
  float central_cylindrical_radius = 1.0f;

  enum { AUTO, HORIZONTAL, VERTICAL } sensor_fit = AUTO;
  float sensor_width = 36.0f;
  float sensor_height = 24.0f;

  int full_width = 0;
  int full_height = 0;

  int render_width = 0;
  int render_height = 0;

  BoundBox2D border = BoundBox2D();
  BoundBox2D viewport_camera_border = BoundBox2D();
  BoundBox2D pano_viewplane = BoundBox2D();
  float pano_aspectratio = 0.0f;

  float passepartout_alpha = 0.5f;

  Transform matrix = transform_identity();

  float offscreen_dicing_scale = 1.0f;

  int motion_steps = 0;
};

static float blender_camera_focal_distance(BL::RenderEngine &b_engine,
                                           BL::Object &b_ob,
                                           BL::Camera &b_camera,
                                           BlenderCamera *bcam)
{
  BL::Object b_dof_object = b_camera.dof().focus_object();

  if (!b_dof_object) {
    return b_camera.dof().focus_distance();
  }

  Transform dofmat = get_transform(b_dof_object.matrix_world());

  const string focus_subtarget = b_camera.dof().focus_subtarget();
  if (b_dof_object.pose() && !focus_subtarget.empty()) {
    BL::PoseBone b_bone = b_dof_object.pose().bones[focus_subtarget];
    if (b_bone) {
      dofmat = dofmat * get_transform(b_bone.matrix());
    }
  }

  /* for dof object, return distance along camera Z direction */
  BL::Array<float, 16> b_ob_matrix;
  b_engine.camera_model_matrix(b_ob, bcam->use_spherical_stereo, b_ob_matrix);
  const Transform obmat = transform_clear_scale(get_transform(b_ob_matrix));
  const float3 view_dir = normalize(transform_get_column(&obmat, 2));
  const float3 dof_dir = transform_get_column(&obmat, 3) - transform_get_column(&dofmat, 3);
  return fabsf(dot(view_dir, dof_dir));
}

static PanoramaType blender_panorama_type_to_cycles(const BL::Camera::panorama_type_enum type)
{
  switch (type) {
    case BL::Camera::panorama_type_EQUIRECTANGULAR:
      return PANORAMA_EQUIRECTANGULAR;
    case BL::Camera::panorama_type_EQUIANGULAR_CUBEMAP_FACE:
      return PANORAMA_EQUIANGULAR_CUBEMAP_FACE;
    case BL::Camera::panorama_type_MIRRORBALL:
      return PANORAMA_MIRRORBALL;
    case BL::Camera::panorama_type_FISHEYE_EQUIDISTANT:
      return PANORAMA_FISHEYE_EQUIDISTANT;
    case BL::Camera::panorama_type_FISHEYE_EQUISOLID:
      return PANORAMA_FISHEYE_EQUISOLID;
    case BL::Camera::panorama_type_FISHEYE_LENS_POLYNOMIAL:
      return PANORAMA_FISHEYE_LENS_POLYNOMIAL;
    case BL::Camera::panorama_type_CENTRAL_CYLINDRICAL:
      return PANORAMA_CENTRAL_CYLINDRICAL;
  }
  /* Could happen if loading a newer file that has an unsupported type. */
  return PANORAMA_FISHEYE_EQUISOLID;
}

static void blender_camera_from_object(BlenderCamera *bcam,
                                       BL::RenderEngine &b_engine,
                                       BL::Object &b_ob,
                                       BL::BlendData &b_data,
                                       bool skip_panorama = false)
{
  BL::ID b_ob_data = b_ob.data();

  if (b_ob_data.is_a(&RNA_Camera)) {
    BL::Camera b_camera(b_ob_data);

    bcam->nearclip = b_camera.clip_start();
    bcam->farclip = b_camera.clip_end();

    switch (b_camera.type()) {
      case BL::Camera::type_ORTHO:
        bcam->type = CAMERA_ORTHOGRAPHIC;
        break;
      case BL::Camera::type_CUSTOM:
        bcam->type = skip_panorama ? CAMERA_PERSPECTIVE : CAMERA_CUSTOM;
        break;
      case BL::Camera::type_PANO:
        bcam->type = skip_panorama ? CAMERA_PERSPECTIVE : CAMERA_PANORAMA;
        break;
      case BL::Camera::type_PERSP:
      default:
        bcam->type = CAMERA_PERSPECTIVE;
        break;
    }

    bcam->panorama_type = blender_panorama_type_to_cycles(b_camera.panorama_type());
    bcam->fisheye_fov = b_camera.fisheye_fov();
    bcam->fisheye_lens = b_camera.fisheye_lens();
    bcam->latitude_min = b_camera.latitude_min();
    bcam->latitude_max = b_camera.latitude_max();
    bcam->longitude_min = b_camera.longitude_min();
    bcam->longitude_max = b_camera.longitude_max();

    bcam->fisheye_polynomial_k0 = b_camera.fisheye_polynomial_k0();
    bcam->fisheye_polynomial_k1 = b_camera.fisheye_polynomial_k1();
    bcam->fisheye_polynomial_k2 = b_camera.fisheye_polynomial_k2();
    bcam->fisheye_polynomial_k3 = b_camera.fisheye_polynomial_k3();
    bcam->fisheye_polynomial_k4 = b_camera.fisheye_polynomial_k4();

    bcam->central_cylindrical_range_u_min = b_camera.central_cylindrical_range_u_min();
    bcam->central_cylindrical_range_u_max = b_camera.central_cylindrical_range_u_max();
    bcam->central_cylindrical_range_v_min = b_camera.central_cylindrical_range_v_min();
    bcam->central_cylindrical_range_v_max = b_camera.central_cylindrical_range_v_max();
    bcam->central_cylindrical_radius = b_camera.central_cylindrical_radius();

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

    bcam->passepartout_alpha = b_camera.show_passepartout() ? b_camera.passepartout_alpha() : 0.0f;

    if (b_camera.dof().use_dof()) {
      /* allow f/stop number to change aperture_size but still
       * give manual control over aperture radius */
      float fstop = b_camera.dof().aperture_fstop();
      fstop = max(fstop, 1e-5f);

      if (bcam->type == CAMERA_ORTHOGRAPHIC || bcam->type == CAMERA_CUSTOM) {
        bcam->aperturesize = 1.0f / (2.0f * fstop);
      }
      else {
        bcam->aperturesize = (bcam->lens * 1e-3f) / (2.0f * fstop);
      }

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

    if (b_camera.sensor_fit() == BL::Camera::sensor_fit_AUTO) {
      bcam->sensor_fit = BlenderCamera::AUTO;
    }
    else if (b_camera.sensor_fit() == BL::Camera::sensor_fit_HORIZONTAL) {
      bcam->sensor_fit = BlenderCamera::HORIZONTAL;
    }
    else {
      bcam->sensor_fit = BlenderCamera::VERTICAL;
    }

    if (bcam->type == CAMERA_CUSTOM) {
      bcam->custom_props = RNA_pointer_get(&b_camera.ptr, "cycles_custom");
      bcam->custom_bytecode_hash = b_camera.custom_bytecode_hash();
      if (!bcam->custom_bytecode_hash.empty()) {
        bcam->custom_bytecode = b_camera.custom_bytecode();
      }
      else {
        bcam->custom_filepath = blender_absolute_path(
            b_data, b_camera, b_camera.custom_filepath());
      }
    }
  }
  else if (b_ob_data.is_a(&RNA_Light)) {
    /* Can also look through spot light. */
    BL::SpotLight b_light(b_ob_data);
    const float lens = 16.0f / tanf(b_light.spot_size() * 0.5f);
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
    /* Note the blender camera points along the negative z-axis. */
    result = tfm * transform_scale(1.0f, 1.0f, -1.0f);
  }

  return transform_clear_scale(result);
}

static void blender_camera_viewplane(BlenderCamera *bcam,
                                     const int width,
                                     const int height,
                                     BoundBox2D &viewplane,
                                     float &aspectratio,
                                     float &sensor_size)
{
  /* dimensions */
  const float xratio = (float)width * bcam->pixelaspect.x;
  const float yratio = (float)height * bcam->pixelaspect.y;

  /* compute x/y aspect and ratio */
  float2 aspect;
  bool horizontal_fit;

  /* sensor fitting */
  if (bcam->sensor_fit == BlenderCamera::AUTO) {
    horizontal_fit = (xratio > yratio);
    sensor_size = bcam->sensor_width;
  }
  else if (bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
    horizontal_fit = true;
    sensor_size = bcam->sensor_width;
  }
  else {
    horizontal_fit = false;
    sensor_size = bcam->sensor_height;
  }

  if (horizontal_fit) {
    aspectratio = xratio / yratio;
    aspect = make_float2(aspectratio, 1.0f);
  }
  else {
    aspectratio = yratio / xratio;
    aspect = make_float2(1.0f, aspectratio);
  }

  /* modify aspect for orthographic scale */
  if (bcam->type == CAMERA_ORTHOGRAPHIC) {
    aspect *= bcam->ortho_scale / (aspectratio * 2.0f);
    aspectratio = bcam->ortho_scale / 2.0f;
  }

  if (bcam->type == CAMERA_PANORAMA || bcam->type == CAMERA_CUSTOM) {
    /* Account for camera shift. */
    float2 dv = bcam->shift;
    if (bcam->pano_aspectratio != 0.0f) {
      dv *= aspectratio / bcam->pano_aspectratio;
    }

    /* Set viewplane for panoramic or custom camera. */
    viewplane = bcam->pano_viewplane.offset(dv);
  }
  else {
    /* Account for camera shift and 3d camera view offset. */
    const float2 dv = 2.0f * (aspectratio * bcam->shift + bcam->offset * aspect * 2.0f);

    /* Set viewplane for perspective or orthographic camera. */
    viewplane = (BoundBox2D(aspect) * bcam->zoom).offset(dv);
  }
}

class BlenderCameraParamQuery : public OSLCameraParamQuery {
 public:
  BlenderCameraParamQuery(PointerRNA custom_props) : custom_props(custom_props) {}
  virtual ~BlenderCameraParamQuery() = default;

  bool get_float(ustring name, vector<float> &data) override
  {
    PropertyRNA *prop = get_prop(name);
    if (!prop) {
      return false;
    }
    if (RNA_property_array_check(prop)) {
      data.resize(RNA_property_array_length(&custom_props, prop));
      RNA_property_float_get_array(&custom_props, prop, data.data());
    }
    else {
      data.resize(1);
      data[0] = RNA_property_float_get(&custom_props, prop);
    }
    return true;
  }

  bool get_int(ustring name, vector<int> &data) override
  {
    PropertyRNA *prop = get_prop(name);
    if (!prop) {
      return false;
    }
    int array_len = 0;
    if (RNA_property_array_check(prop)) {
      array_len = RNA_property_array_length(&custom_props, prop);
    }

    /* OSL represents booleans as integers, but we represent them as boolean-type
     * properties in RNA, so convert here. */
    if (RNA_property_type(prop) == PROP_BOOLEAN) {
      if (array_len > 0) {
        /* Can't use std::vector<bool> here since it's a weird special case. */
        array<bool> bool_data(array_len);
        RNA_property_boolean_get_array(&custom_props, prop, bool_data.data());
        std::copy(bool_data.begin(), bool_data.end(), std::back_inserter(data));
      }
      else {
        data.push_back(RNA_property_boolean_get(&custom_props, prop));
      }
    }
    else if (RNA_property_type(prop) == PROP_ENUM) {
      const char *identifier = "";
      const int value = RNA_property_enum_get(&custom_props, prop);
      if (RNA_property_enum_identifier(nullptr, &custom_props, prop, value, &identifier)) {
        data.push_back(atoi(identifier));
      }
      else {
        data.push_back(value);
      }
    }
    else {
      if (array_len > 0) {
        data.resize(array_len);
        RNA_property_int_get_array(&custom_props, prop, data.data());
      }
      else {
        data.push_back(RNA_property_int_get(&custom_props, prop));
      }
    }
    return true;
  }

  bool get_string(ustring name, string &data) override
  {
    PropertyRNA *prop = get_prop(name);
    if (!prop) {
      return false;
    }
    data = RNA_property_string_get(&custom_props, prop);
    return true;
  }

 private:
  PointerRNA custom_props;

  PropertyRNA *get_prop(ustring param)
  {
    string name = string_printf("[\"%s\"]", param.c_str());
    return RNA_struct_find_property(&custom_props, name.c_str());
  }
};

static void blender_camera_sync(Camera *cam,
                                Scene *scene,
                                BlenderCamera *bcam,
                                const int width,
                                const int height,
                                const char *viewname,
                                PointerRNA *cscene)
{
  float aspectratio;
  float sensor_size;

  /* viewplane */
  BoundBox2D viewplane;
  blender_camera_viewplane(bcam, width, height, viewplane, aspectratio, sensor_size);

  cam->set_viewplane_left(viewplane.left);
  cam->set_viewplane_right(viewplane.right);
  cam->set_viewplane_top(viewplane.top);
  cam->set_viewplane_bottom(viewplane.bottom);

  cam->set_full_width(width);
  cam->set_full_height(height);

  /* Set panorama or custom sensor. */
  if ((bcam->type == CAMERA_PANORAMA &&
       (bcam->panorama_type == PANORAMA_FISHEYE_EQUISOLID ||
        bcam->panorama_type == PANORAMA_FISHEYE_LENS_POLYNOMIAL)) ||
      bcam->type == CAMERA_CUSTOM)
  {
    const float fit_xratio = (float)bcam->render_width * bcam->pixelaspect.x;
    const float fit_yratio = (float)bcam->render_height * bcam->pixelaspect.y;
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
      cam->set_sensorwidth(sensor_size);
      cam->set_sensorheight(sensor_size * fit_yratio / fit_xratio);
    }
    else {
      cam->set_sensorwidth(sensor_size * fit_xratio / fit_yratio);
      cam->set_sensorheight(sensor_size);
    }
  }

  /* Sync custom camera parameters. */
  if (scene != nullptr) {
    if (bcam->type == CAMERA_CUSTOM) {
      BlenderCameraParamQuery params(bcam->custom_props);
      cam->set_osl_camera(
          scene, params, bcam->custom_filepath, bcam->custom_bytecode_hash, bcam->custom_bytecode);
    }
    else {
      cam->clear_osl_camera(scene);
    }
  }

  /* clipping distances */
  cam->set_nearclip(bcam->nearclip);
  cam->set_farclip(bcam->farclip);

  /* type */
  cam->set_camera_type(bcam->type);

  /* panorama */
  cam->set_panorama_type(bcam->panorama_type);
  cam->set_fisheye_fov(bcam->fisheye_fov);
  cam->set_fisheye_lens(bcam->fisheye_lens);
  cam->set_latitude_min(bcam->latitude_min);
  cam->set_latitude_max(bcam->latitude_max);

  cam->set_fisheye_polynomial_k0(bcam->fisheye_polynomial_k0);
  cam->set_fisheye_polynomial_k1(bcam->fisheye_polynomial_k1);
  cam->set_fisheye_polynomial_k2(bcam->fisheye_polynomial_k2);
  cam->set_fisheye_polynomial_k3(bcam->fisheye_polynomial_k3);
  cam->set_fisheye_polynomial_k4(bcam->fisheye_polynomial_k4);

  cam->set_longitude_min(bcam->longitude_min);
  cam->set_longitude_max(bcam->longitude_max);

  cam->set_central_cylindrical_range_u_min(bcam->central_cylindrical_range_u_min);
  cam->set_central_cylindrical_range_u_max(bcam->central_cylindrical_range_u_max);
  cam->set_central_cylindrical_range_v_min(bcam->central_cylindrical_range_v_min /
                                           bcam->central_cylindrical_radius);
  cam->set_central_cylindrical_range_v_max(bcam->central_cylindrical_range_v_max /
                                           bcam->central_cylindrical_radius);

  /* panorama stereo */
  cam->set_interocular_distance(bcam->interocular_distance);
  cam->set_convergence_distance(bcam->convergence_distance);
  cam->set_use_spherical_stereo(bcam->use_spherical_stereo);

  if (cam->get_use_spherical_stereo()) {
    if (strcmp(viewname, "left") == 0) {
      cam->set_stereo_eye(Camera::STEREO_LEFT);
    }
    else if (strcmp(viewname, "right") == 0) {
      cam->set_stereo_eye(Camera::STEREO_RIGHT);
    }
    else {
      cam->set_stereo_eye(Camera::STEREO_NONE);
    }
  }

  cam->set_use_pole_merge(bcam->use_pole_merge);
  cam->set_pole_merge_angle_from(bcam->pole_merge_angle_from);
  cam->set_pole_merge_angle_to(bcam->pole_merge_angle_to);

  /* anamorphic lens bokeh */
  cam->set_aperture_ratio(bcam->aperture_ratio);

  /* perspective */
  cam->set_fov(2.0f * atanf((0.5f * sensor_size) / bcam->lens / aspectratio));
  cam->set_focaldistance(bcam->focaldistance);
  cam->set_aperturesize(bcam->aperturesize);
  cam->set_blades(bcam->apertureblades);
  cam->set_bladesrotation(bcam->aperturerotation);

  /* transform */
  cam->set_matrix(blender_camera_matrix(bcam->matrix, bcam->type, bcam->panorama_type));

  array<Transform> motion;
  motion.resize(bcam->motion_steps, cam->get_matrix());
  cam->set_motion(motion);
  cam->set_use_perspective_motion(false);

  cam->set_shuttertime(bcam->shuttertime);
  cam->set_fov_pre(cam->get_fov());
  cam->set_fov_post(cam->get_fov());
  cam->set_motion_position(bcam->motion_position);

  cam->set_rolling_shutter_type(bcam->rolling_shutter_type);
  cam->set_rolling_shutter_duration(bcam->rolling_shutter_duration);

  cam->set_shutter_curve(bcam->shutter_curve);

  /* border */
  cam->set_border_left(bcam->border.left);
  cam->set_border_right(bcam->border.right);
  cam->set_border_top(bcam->border.top);
  cam->set_border_bottom(bcam->border.bottom);

  cam->set_viewport_camera_border_left(bcam->viewport_camera_border.left);
  cam->set_viewport_camera_border_right(bcam->viewport_camera_border.right);
  cam->set_viewport_camera_border_top(bcam->viewport_camera_border.top);
  cam->set_viewport_camera_border_bottom(bcam->viewport_camera_border.bottom);

  bcam->offscreen_dicing_scale = RNA_float_get(cscene, "offscreen_dicing_scale");
  cam->set_offscreen_dicing_scale(bcam->offscreen_dicing_scale);
}

/* Sync Render Camera */

static MotionPosition blender_motion_blur_position_type_to_cycles(
    const BL::RenderSettings::motion_blur_position_enum type)
{
  switch (type) {
    case BL::RenderSettings::motion_blur_position_START:
      return MOTION_POSITION_START;
    case BL::RenderSettings::motion_blur_position_CENTER:
      return MOTION_POSITION_CENTER;
    case BL::RenderSettings::motion_blur_position_END:
      return MOTION_POSITION_END;
  }
  /* Could happen if loading a newer file that has an unsupported type. */
  return MOTION_POSITION_CENTER;
}

void BlenderSync::sync_camera(BL::RenderSettings &b_render,
                              const int width,
                              const int height,
                              const char *viewname)
{
  BlenderCamera bcam(b_render);

  /* pixel aspect */
  bcam.pixelaspect.x = b_render.pixel_aspect_x();
  bcam.pixelaspect.y = b_render.pixel_aspect_y();
  bcam.shuttertime = b_render.motion_blur_shutter();
  bcam.motion_position = blender_motion_blur_position_type_to_cycles(
      b_render.motion_blur_position());

  BL::CurveMapping b_shutter_curve(b_render.motion_blur_shutter_curve());
  curvemapping_to_array(b_shutter_curve, bcam.shutter_curve, RAMP_TABLE_SIZE);

  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
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
  BL::Object b_ob = get_camera_object(PointerRNA_NULL, PointerRNA_NULL);

  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob, b_data);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);
    scene->bake_manager->set_use_camera(b_render.bake().view_from() ==
                                        BL::BakeSettings::view_from_ACTIVE_CAMERA);
  }
  else {
    scene->bake_manager->set_use_camera(false);
  }

  /* sync */
  Camera *cam = scene->camera;
  blender_camera_sync(cam, scene, &bcam, width, height, viewname, &cscene);

  /* dicing camera */
  b_ob = BL::Object(RNA_pointer_get(&cscene, "dicing_camera"));
  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob, b_data);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);

    blender_camera_sync(scene->dicing_camera, nullptr, &bcam, width, height, viewname, &cscene);
  }
  else {
    *scene->dicing_camera = *cam;
  }
}

BL::Object BlenderSync::get_camera_object(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d)
{
  BL::Object b_camera_override = b_engine.camera_override();
  if (b_camera_override) {
    return b_camera_override;
  }

  if (b_v3d && b_rv3d && b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA &&
      b_v3d.use_local_camera())
  {
    return b_v3d.camera();
  }

  return b_scene.camera();
}

BL::Object BlenderSync::get_dicing_camera_object(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  BL::Object b_ob = BL::Object(RNA_pointer_get(&cscene, "dicing_camera"));
  if (b_ob) {
    return b_ob;
  }

  return get_camera_object(b_v3d, b_rv3d);
}

void BlenderSync::sync_camera_motion(BL::RenderSettings &b_render,
                                     BL::Object &b_ob,
                                     const int width,
                                     const int height,
                                     const float motion_time)
{
  if (!b_ob) {
    return;
  }

  Camera *cam = scene->camera;
  BL::Array<float, 16> b_ob_matrix;
  b_engine.camera_model_matrix(b_ob, cam->get_use_spherical_stereo(), b_ob_matrix);
  Transform tfm = get_transform(b_ob_matrix);
  tfm = blender_camera_matrix(tfm, cam->get_camera_type(), cam->get_panorama_type());

  if (motion_time == 0.0f) {
    /* When motion blur is not centered in frame, cam->matrix gets reset. */
    cam->set_matrix(tfm);
  }

  /* Set transform in motion array. */
  const int motion_step = cam->motion_step(motion_time);
  if (motion_step >= 0) {
    array<Transform> motion = cam->get_motion();
    motion[motion_step] = tfm;
    cam->set_motion(motion);
  }

  if (cam->get_camera_type() == CAMERA_PERSPECTIVE) {
    BlenderCamera bcam(b_render);

    /* TODO(sergey): Consider making it a part of BlenderCamera(). */
    bcam.pixelaspect.x = b_render.pixel_aspect_x();
    bcam.pixelaspect.y = b_render.pixel_aspect_y();

    blender_camera_from_object(&bcam, b_engine, b_ob, b_data);

    BoundBox2D viewplane;
    float aspectratio;
    float sensor_size;
    blender_camera_viewplane(&bcam, width, height, viewplane, aspectratio, sensor_size);
    /* TODO(sergey): De-duplicate calculation with camera sync. */
    const float fov = 2.0f * atanf((0.5f * sensor_size) / bcam.lens / aspectratio);
    if (fov != cam->get_fov()) {
      LOG_DEBUG << "Camera " << b_ob.name() << " FOV change detected.";
      if (motion_time == 0.0f) {
        cam->set_fov(fov);
      }
      else if (motion_time == -1.0f) {
        cam->set_fov_pre(fov);
        cam->set_use_perspective_motion(true);
      }
      else if (motion_time == 1.0f) {
        cam->set_fov_post(fov);
        cam->set_use_perspective_motion(true);
      }
    }
  }
}

/* Sync 3D View Camera */

static void blender_camera_view_subset(BL::RenderEngine &b_engine,
                                       BL::RenderSettings &b_render,
                                       BL::Scene &b_scene,
                                       BL::BlendData &b_data,
                                       BL::Object &b_ob,
                                       BL::SpaceView3D &b_v3d,
                                       BL::RegionView3D &b_rv3d,
                                       const int width,
                                       const int height,
                                       BoundBox2D &view_box,
                                       BoundBox2D &cam_box,
                                       float &view_aspect);

static void blender_camera_from_view(BlenderCamera *bcam,
                                     BL::RenderEngine &b_engine,
                                     BL::Scene &b_scene,
                                     BL::BlendData &b_data,
                                     BL::SpaceView3D &b_v3d,
                                     BL::RegionView3D &b_rv3d,
                                     const int width,
                                     const int height,
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
      blender_camera_from_object(bcam, b_engine, b_ob, b_data, skip_panorama);

      if (!skip_panorama && (bcam->type == CAMERA_PANORAMA || bcam->type == CAMERA_CUSTOM)) {
        /* in panorama or custom camera view, we map viewplane to camera border */
        BoundBox2D view_box;
        BoundBox2D cam_box;
        float view_aspect;

        BL::RenderSettings b_render_settings(b_scene.render());
        blender_camera_view_subset(b_engine,
                                   b_render_settings,
                                   b_scene,
                                   b_data,
                                   b_ob,
                                   b_v3d,
                                   b_rv3d,
                                   width,
                                   height,
                                   view_box,
                                   cam_box,
                                   view_aspect);

        bcam->pano_viewplane = view_box.make_relative_to(cam_box);
        bcam->pano_aspectratio = view_aspect;
      }
      else {
        /* magic zoom formula */
        bcam->zoom = b_rv3d.view_camera_zoom();
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
    if (bcam->sensor_fit == BlenderCamera::VERTICAL) {
      sensor_size = bcam->sensor_height;
    }
    else {
      sensor_size = bcam->sensor_width;
    }

    bcam->type = CAMERA_ORTHOGRAPHIC;
    bcam->ortho_scale = b_rv3d.view_distance() * sensor_size / b_v3d.lens();
  }

  bcam->zoom *= 2.0f;

  /* 3d view transform */
  bcam->matrix = transform_inverse(get_transform(b_rv3d.view_matrix()));

  /* dimensions */
  bcam->full_width = width;
  bcam->full_height = height;
}

static void blender_camera_view_subset(BL::RenderEngine &b_engine,
                                       BL::RenderSettings &b_render,
                                       BL::Scene &b_scene,
                                       BL::BlendData &b_data,
                                       BL::Object &b_ob,
                                       BL::SpaceView3D &b_v3d,
                                       BL::RegionView3D &b_rv3d,
                                       const int width,
                                       const int height,
                                       BoundBox2D &view_box,
                                       BoundBox2D &cam_box,
                                       float &view_aspect)
{
  BoundBox2D cam;
  BoundBox2D view;
  float cam_aspect;
  float sensor_size;

  /* Get viewport viewplane. */
  BlenderCamera view_bcam(b_render);
  blender_camera_from_view(
      &view_bcam, b_engine, b_scene, b_data, b_v3d, b_rv3d, width, height, true);

  blender_camera_viewplane(&view_bcam, width, height, view, view_aspect, sensor_size);

  /* Get camera viewplane. */
  BlenderCamera cam_bcam(b_render);
  blender_camera_from_object(&cam_bcam, b_engine, b_ob, b_data, true);

  /* Camera border is affect by aspect, viewport is not. */
  cam_bcam.pixelaspect.x = b_render.pixel_aspect_x();
  cam_bcam.pixelaspect.y = b_render.pixel_aspect_y();

  blender_camera_viewplane(
      &cam_bcam, cam_bcam.full_width, cam_bcam.full_height, cam, cam_aspect, sensor_size);

  /* Return */
  view_box = view * (1.0f / view_aspect);
  cam_box = cam * (1.0f / cam_aspect);
}

static void blender_camera_border_subset(BL::RenderEngine &b_engine,
                                         BL::RenderSettings &b_render,
                                         BL::Scene &b_scene,
                                         BL::BlendData &b_data,
                                         BL::SpaceView3D &b_v3d,
                                         BL::RegionView3D &b_rv3d,
                                         BL::Object &b_ob,
                                         const int width,
                                         const int height,
                                         const BoundBox2D &border,
                                         BoundBox2D *result)
{
  /* Determine camera viewport subset. */
  BoundBox2D view_box;
  BoundBox2D cam_box;
  float view_aspect;
  blender_camera_view_subset(b_engine,
                             b_render,
                             b_scene,
                             b_data,
                             b_ob,
                             b_v3d,
                             b_rv3d,
                             width,
                             height,
                             view_box,
                             cam_box,
                             view_aspect);

  /* Determine viewport subset matching given border. */
  cam_box = cam_box.make_relative_to(view_box);
  *result = cam_box.subset(border);
}

static void blender_camera_border(BlenderCamera *bcam,
                                  BL::RenderEngine &b_engine,
                                  BL::RenderSettings &b_render,
                                  BL::Scene &b_scene,
                                  BL::BlendData &b_data,
                                  BL::SpaceView3D &b_v3d,
                                  BL::RegionView3D &b_rv3d,
                                  const int width,
                                  const int height)
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

  if (!b_ob) {
    return;
  }

  /* Determine camera border inside the viewport. */
  const BoundBox2D full_border;
  blender_camera_border_subset(b_engine,
                               b_render,
                               b_scene,
                               b_data,
                               b_v3d,
                               b_rv3d,
                               b_ob,
                               width,
                               height,
                               full_border,
                               &bcam->viewport_camera_border);

  if (b_render.use_border()) {
    bcam->border.left = b_render.border_min_x();
    bcam->border.right = b_render.border_max_x();
    bcam->border.bottom = b_render.border_min_y();
    bcam->border.top = b_render.border_max_y();
  }
  else if (bcam->passepartout_alpha == 1.0f) {
    bcam->border = full_border;
  }
  else {
    return;
  }

  /* Determine viewport subset matching camera border. */
  blender_camera_border_subset(b_engine,
                               b_render,
                               b_scene,
                               b_data,
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
                            const int width,
                            const int height)
{
  BL::RenderSettings b_render_settings(b_scene.render());
  BlenderCamera bcam(b_render_settings);
  blender_camera_from_view(&bcam, b_engine, b_scene, b_data, b_v3d, b_rv3d, width, height);
  blender_camera_border(
      &bcam, b_engine, b_render_settings, b_scene, b_data, b_v3d, b_rv3d, width, height);
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  blender_camera_sync(scene->camera, scene, &bcam, width, height, "", &cscene);

  /* dicing camera */
  BL::Object b_ob = BL::Object(RNA_pointer_get(&cscene, "dicing_camera"));
  if (b_ob) {
    BL::Array<float, 16> b_ob_matrix;
    blender_camera_from_object(&bcam, b_engine, b_ob, b_data);
    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    bcam.matrix = get_transform(b_ob_matrix);

    blender_camera_sync(scene->dicing_camera, nullptr, &bcam, width, height, "", &cscene);
  }
  else {
    *scene->dicing_camera = *scene->camera;
  }
}

BufferParams BlenderSync::get_buffer_params(BL::SpaceView3D &b_v3d,
                                            BL::RegionView3D &b_rv3d,
                                            Camera *cam,
                                            const int width,
                                            const int height)
{
  BufferParams params;
  bool use_border = false;

  params.full_width = width;
  params.full_height = height;

  if (b_v3d && b_rv3d && b_rv3d.view_perspective() != BL::RegionView3D::view_perspective_CAMERA) {
    use_border = b_v3d.use_render_border();
  }
  else {
    /* the camera can always have a passepartout */
    use_border = true;
  }

  if (use_border) {
    /* border render */
    /* the viewport may offset the border outside the view */
    const BoundBox2D border = cam->border.clamp();
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

  params.window_width = params.width;
  params.window_height = params.height;

  return params;
}

CCL_NAMESPACE_END
