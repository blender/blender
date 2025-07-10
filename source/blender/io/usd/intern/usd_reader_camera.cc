/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#include "usd_reader_camera.hh"
#include "usd_armature_utils.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "BLI_math_base.h"

#include "BKE_camera.h"
#include "BKE_fcurve.hh"
#include "BKE_object.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include <pxr/usd/usdGeom/camera.h>

#include <array>
#include <optional>

namespace blender::io::usd {

namespace {

template<typename T> struct SampleData {
  float frame;
  T value;
};

template<typename T> struct AttributeData {
  std::optional<T> initial_value = std::nullopt;
  Vector<SampleData<T>> samples;

  void reset()
  {
    initial_value = std::nullopt;
    samples.clear();
  }
};

template<typename T>
bool read_attribute_values(const pxr::UsdAttribute &attr,
                           const pxr::UsdTimeCode initial_time,
                           AttributeData<T> &data)
{
  data.reset(); /* Clear any prior data. */

  T value{};
  if (attr.Get(&value, initial_time)) {
    data.initial_value = value;
  }
  else {
    data.initial_value = std::nullopt;
  }

  if (attr.ValueMightBeTimeVarying()) {
    std::vector<double> times;
    attr.GetTimeSamples(&times);

    data.samples.resize(times.size());
    for (int64_t i = 0; i < times.size(); i++) {
      data.samples[i].frame = float(times[i]);
      attr.Get(&data.samples[i].value, times[i]);
    }
  }

  return data.initial_value.has_value() || !data.samples.is_empty();
}

void read_aperture_data(Camera *camera,
                        const pxr::UsdAttribute &usd_horiz_aperture,
                        const pxr::UsdAttribute &usd_vert_aperture,
                        const pxr::UsdAttribute &usd_horiz_offset,
                        const pxr::UsdAttribute &usd_vert_offset,
                        const pxr::UsdTimeCode initial_time,
                        const double tenth_unit_to_millimeters,
                        animrig::Channelbag &channelbag)
{
  /* If the Aperture values are changing, that effects the sensor_fit and shift_x|y values as
   * well. We need to put animation data on all of them. */
  if (usd_horiz_aperture.ValueMightBeTimeVarying() || usd_vert_aperture.ValueMightBeTimeVarying())
  {
    std::vector<double> times;
    pxr::UsdAttribute::GetUnionedTimeSamples(
        {usd_horiz_aperture, usd_vert_aperture, usd_horiz_offset, usd_vert_offset}, &times);

    std::array<FCurve *, 5> curves = {
        create_fcurve(channelbag, {"sensor_width", 0}, times.size()),
        create_fcurve(channelbag, {"sensor_height", 0}, times.size()),
        create_fcurve(channelbag, {"sensor_fit", 0}, times.size()),
        create_fcurve(channelbag, {"shift_x", 0}, times.size()),
        create_fcurve(channelbag, {"shift_y", 0}, times.size())};

    for (int64_t i = 0; i < times.size(); i++) {
      const double time = times[i];

      float horiz_aperture, vert_aperture;
      float shift_x, shift_y;
      usd_horiz_aperture.Get(&horiz_aperture, time);
      usd_vert_aperture.Get(&vert_aperture, time);
      usd_horiz_offset.Get(&shift_x, time);
      usd_vert_offset.Get(&shift_y, time);

      const float sensor_x = horiz_aperture * tenth_unit_to_millimeters;
      const float sensor_y = vert_aperture * tenth_unit_to_millimeters;
      const char sensor_fit = horiz_aperture >= vert_aperture ? CAMERA_SENSOR_FIT_HOR :
                                                                CAMERA_SENSOR_FIT_VERT;

      const float sensor_size = sensor_x >= sensor_y ? sensor_x : sensor_y;
      shift_x = (shift_x * tenth_unit_to_millimeters) / sensor_size;
      shift_y = (shift_y * tenth_unit_to_millimeters) / sensor_size;

      set_fcurve_sample(curves[0], i, float(time), sensor_x);
      set_fcurve_sample(curves[1], i, float(time), sensor_y);
      set_fcurve_sample(curves[2], i, float(time), sensor_fit);
      set_fcurve_sample(curves[3], i, float(time), shift_x);
      set_fcurve_sample(curves[4], i, float(time), shift_y);
    }
  }
  else if (usd_horiz_offset.ValueMightBeTimeVarying() || usd_vert_offset.ValueMightBeTimeVarying())
  {
    /* Only the shift_x|y values are changing. Load in the initial values for aperture and
     * sensor_fit and use those when setting the shift_x|y curves. */
    float horiz_aperture, vert_aperture;
    usd_horiz_aperture.Get(&horiz_aperture, initial_time);
    usd_vert_aperture.Get(&vert_aperture, initial_time);

    camera->sensor_x = horiz_aperture * tenth_unit_to_millimeters;
    camera->sensor_y = vert_aperture * tenth_unit_to_millimeters;
    camera->sensor_fit = camera->sensor_x >= camera->sensor_y ? CAMERA_SENSOR_FIT_HOR :
                                                                CAMERA_SENSOR_FIT_VERT;
    const float sensor_size = camera->sensor_x >= camera->sensor_y ? camera->sensor_x :
                                                                     camera->sensor_y;

    std::vector<double> times;
    if (usd_horiz_offset.GetTimeSamples(&times)) {
      FCurve *fcu = create_fcurve(channelbag, {"shift_x", 0}, times.size());
      for (int64_t i = 0; i < times.size(); i++) {
        const double time = times[i];
        float shift;
        usd_horiz_offset.Get(&shift, time);

        shift = (shift * tenth_unit_to_millimeters) / sensor_size;
        set_fcurve_sample(fcu, i, float(time), shift);
      }
    }

    if (usd_vert_offset.GetTimeSamples(&times)) {
      FCurve *fcu = create_fcurve(channelbag, {"shift_y", 0}, times.size());
      for (int64_t i = 0; i < times.size(); i++) {
        const double time = times[i];
        float shift;
        usd_vert_offset.Get(&shift, time);

        shift = (shift * tenth_unit_to_millimeters) / sensor_size;
        set_fcurve_sample(fcu, i, float(time), shift);
      }
    }
  }
  else {
    /* No animation data. */
    float horiz_aperture, vert_aperture;
    float shift_x, shift_y;
    usd_horiz_aperture.Get(&horiz_aperture, initial_time);
    usd_vert_aperture.Get(&vert_aperture, initial_time);
    usd_horiz_offset.Get(&shift_x, initial_time);
    usd_vert_offset.Get(&shift_y, initial_time);

    camera->sensor_x = horiz_aperture * tenth_unit_to_millimeters;
    camera->sensor_y = vert_aperture * tenth_unit_to_millimeters;
    camera->sensor_fit = camera->sensor_x >= camera->sensor_y ? CAMERA_SENSOR_FIT_HOR :
                                                                CAMERA_SENSOR_FIT_VERT;
    const float sensor_size = camera->sensor_x >= camera->sensor_y ? camera->sensor_x :
                                                                     camera->sensor_y;
    camera->shiftx = (shift_x * tenth_unit_to_millimeters) / sensor_size;
    camera->shifty = (shift_y * tenth_unit_to_millimeters) / sensor_size;
  }
}

}  // namespace

void USDCameraReader::create_object(Main *bmain)
{
  Camera *camera = BKE_camera_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_CAMERA, name_.c_str());
  object_->data = camera;
}

void USDCameraReader::read_object_data(Main *bmain, const pxr::UsdTimeCode time)
{
  pxr::UsdAttribute usd_focal_length = cam_prim_.GetFocalLengthAttr();
  pxr::UsdAttribute usd_focus_dist = cam_prim_.GetFocusDistanceAttr();
  pxr::UsdAttribute usd_fstop = cam_prim_.GetFStopAttr();
  pxr::UsdAttribute usd_clipping_range = cam_prim_.GetClippingRangeAttr();
  pxr::UsdAttribute usd_horiz_aperture = cam_prim_.GetHorizontalApertureAttr();
  pxr::UsdAttribute usd_vert_aperture = cam_prim_.GetVerticalApertureAttr();
  pxr::UsdAttribute usd_horiz_offset = cam_prim_.GetHorizontalApertureOffsetAttr();
  pxr::UsdAttribute usd_vert_offset = cam_prim_.GetVerticalApertureOffsetAttr();

  /* If any of the camera attributes are time varying, then prepare the animation data. */
  const bool is_time_varying = usd_focal_length.ValueMightBeTimeVarying() ||
                               usd_focus_dist.ValueMightBeTimeVarying() ||
                               usd_fstop.ValueMightBeTimeVarying() ||
                               usd_clipping_range.ValueMightBeTimeVarying() ||
                               usd_horiz_aperture.ValueMightBeTimeVarying() ||
                               usd_vert_aperture.ValueMightBeTimeVarying() ||
                               usd_horiz_offset.ValueMightBeTimeVarying() ||
                               usd_vert_offset.ValueMightBeTimeVarying();

  Camera *camera = (Camera *)object_->data;

  bAction *action = nullptr;
  if (is_time_varying) {
    action = blender::animrig::id_action_ensure(bmain, &camera->id);
  }

  animrig::Channelbag empty{};
  animrig::Channelbag &channelbag = is_time_varying ?
                                        animrig::action_channelbag_ensure(*action, camera->id) :
                                        empty;

  /*
   * In USD, some camera properties are in tenths of a world unit.
   * https://graphics.pixar.com/usd/release/api/class_usd_geom_camera.html#UsdGeom_CameraUnits
   *
   * tenth_unit_to_meters  = stage_meters_per_unit / 10
   * tenth_unit_to_millimeters = 1000 * tenth_unit_to_meters
   *                           = 100 * stage_meters_per_unit
   */
  const double tenth_unit_to_millimeters = 100.0 * settings_->stage_meters_per_unit;
  auto scale_default = [](std::optional<float> input, double scale, float default_value) {
    return input.has_value() ? input.value() * scale : default_value;
  };

  AttributeData<float> data;
  if (read_attribute_values(usd_focal_length, time, data)) {
    camera->lens = scale_default(data.initial_value, tenth_unit_to_millimeters, camera->lens);

    if (!data.samples.is_empty()) {
      FCurve *fcu = create_fcurve(channelbag, {"lens", 0}, data.samples.size());
      for (int64_t i = 0; i < data.samples.size(); i++) {
        const SampleData<float> &sample = data.samples[i];
        set_fcurve_sample(fcu, i, sample.frame, sample.value * tenth_unit_to_millimeters);
      }
    }
  }

  if (read_attribute_values(usd_focus_dist, time, data)) {
    camera->dof.focus_distance = scale_default(
        data.initial_value, this->settings_->scene_scale, camera->dof.focus_distance);

    if (!data.samples.is_empty()) {
      FCurve *fcu = create_fcurve(channelbag, {"dof.focus_distance", 0}, data.samples.size());
      for (int64_t i = 0; i < data.samples.size(); i++) {
        const SampleData<float> &sample = data.samples[i];
        set_fcurve_sample(fcu, i, sample.frame, sample.value * this->settings_->scene_scale);
      }
    }
  }

  if (read_attribute_values(usd_fstop, time, data)) {
    camera->dof.aperture_fstop = scale_default(data.initial_value, 1, camera->dof.aperture_fstop);

    if (!data.samples.is_empty()) {
      FCurve *fcu = create_fcurve(channelbag, {"dof.aperture_fstop", 0}, data.samples.size());
      for (int64_t i = 0; i < data.samples.size(); i++) {
        const SampleData<float> &sample = data.samples[i];
        set_fcurve_sample(fcu, i, sample.frame, sample.value);
      }
    }
  }

  AttributeData<pxr::GfVec2f> clip_data;
  if (read_attribute_values(usd_clipping_range, time, clip_data)) {
    auto clamp_clip = [this](pxr::GfVec2f value) {
      /* Clamp the value for clip-start, matching the range defined in RNA. */
      return pxr::GfVec2f(max_ff(1e-6f, value[0] * settings_->scene_scale),
                          value[1] * settings_->scene_scale);
    };

    pxr::GfVec2f clip_range = clip_data.initial_value.has_value() ?
                                  clamp_clip(clip_data.initial_value.value()) :
                                  pxr::GfVec2f(camera->clip_start, camera->clip_end);
    camera->clip_start = clip_range[0];
    camera->clip_end = clip_range[1];

    if (!clip_data.samples.is_empty()) {
      std::array<FCurve *, 2> curves = {
          create_fcurve(channelbag, {"clip_start", 0}, clip_data.samples.size()),
          create_fcurve(channelbag, {"clip_end", 0}, clip_data.samples.size())};

      for (int64_t i = 0; i < clip_data.samples.size(); i++) {
        const SampleData<pxr::GfVec2f> &sample = clip_data.samples[i];
        clip_range = clamp_clip(sample.value);
        set_fcurve_sample(curves[0], i, sample.frame, clip_range[0]);
        set_fcurve_sample(curves[1], i, sample.frame, clip_range[1]);
      }
    }
  }

  /* Aperture data impacts sensor size, sensor fit, and shift values simultaneously. */
  read_aperture_data(camera,
                     usd_horiz_aperture,
                     usd_vert_aperture,
                     usd_horiz_offset,
                     usd_vert_offset,
                     time,
                     tenth_unit_to_millimeters,
                     channelbag);

  /* USD Orthographic cameras have very limited support. Support a basic, non-animated, translation
   * between USD and Blender. */
  pxr::TfToken projection;
  cam_prim_.GetProjectionAttr().Get(&projection, time);
  camera->type = (projection.GetString() == "perspective") ? CAM_PERSP : CAM_ORTHO;
  if (camera->type == CAM_ORTHO) {
    float horiz_aperture, vert_aperture;
    usd_horiz_aperture.Get(&horiz_aperture, time);
    usd_vert_aperture.Get(&vert_aperture, time);
    camera->ortho_scale = max_ff(vert_aperture, horiz_aperture);
  }

  /* Enable depth of field when needed. */
  const bool requires_dof = usd_focus_dist.IsAuthored() || usd_fstop.IsAuthored();
  camera->dof.flag |= requires_dof ? CAM_DOF_ENABLED : 0;

  /* Recalculate any animation curve handles. */
  for (FCurve *fcu : channelbag.fcurves()) {
    BKE_fcurve_handles_recalc(fcu);
  }

  USDXformReader::read_object_data(bmain, time);
}

}  // namespace blender::io::usd
