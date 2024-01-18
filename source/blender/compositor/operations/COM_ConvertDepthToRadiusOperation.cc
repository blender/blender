/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"

#include "DNA_camera_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_camera.h"

#include "COM_ConvertDepthToRadiusOperation.h"

namespace blender::compositor {

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);
  flags_.can_be_constant = true;
}

void ConvertDepthToRadiusOperation::init_execution()
{
  depth_input_operation_ = this->get_input_socket_reader(0);
  image_input_operation_ = this->get_input_socket_reader(1);

  f_stop = get_f_stop();
  focal_length = get_focal_length();
  max_radius = data_->maxblur;
  pixels_per_meter = compute_pixels_per_meter();
  distance_to_image_of_focus = compute_distance_to_image_of_focus();

  NodeBlurData blur_data;
  blur_data.sizex = compute_maximum_defocus_radius();
  blur_data.sizey = blur_data.sizex;
  blur_data.relative = false;
  blur_data.filtertype = R_FILTER_GAUSS;

  blur_x_operation_->set_data(&blur_data);
  blur_x_operation_->set_size(1.0f);
  blur_y_operation_->set_data(&blur_data);
  blur_y_operation_->set_size(1.0f);
}

/* Given a depth texture, compute the radius of the circle of confusion in pixels based on equation
 * (8) of the paper:
 *
 *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
 *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */
void ConvertDepthToRadiusOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float input_value[4];
  depth_input_operation_->read_sampled(input_value, x, y, sampler);
  const float depth = input_value[0];

  /* Compute `Vu` in equation (7). */
  const float distance_to_image_of_object = (focal_length * depth) / (depth - focal_length);

  /* Compute C in equation (8). Notice that the last multiplier was included in the absolute since
   * it is negative when the object distance is less than the focal length, as noted in equation
   * (7). */
  float diameter = abs((distance_to_image_of_object - distance_to_image_of_focus) *
                       (focal_length / (f_stop * distance_to_image_of_object)));

  /* The diameter is in meters, so multiply by the pixels per meter. */
  float radius = (diameter / 2.0f) * pixels_per_meter;

  output[0] = math::min(max_radius, radius);
}

void ConvertDepthToRadiusOperation::deinit_execution()
{
  depth_input_operation_ = nullptr;
}

/* Given a depth texture, compute the radius of the circle of confusion in pixels based on equation
 * (8) of the paper:
 *
 *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
 *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */
void ConvertDepthToRadiusOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float depth = *it.in(0);

    /* Compute `Vu` in equation (7). */
    const float distance_to_image_of_object = (focal_length * depth) / (depth - focal_length);

    /* Compute C in equation (8). Notice that the last multiplier was included in the absolute
     * since it is negative when the object distance is less than the focal length, as noted in
     * equation (7). */
    float diameter = abs((distance_to_image_of_object - distance_to_image_of_focus) *
                         (focal_length / (f_stop * distance_to_image_of_object)));

    /* The diameter is in meters, so multiply by the pixels per meter. */
    float radius = (diameter / 2.0f) * pixels_per_meter;

    *it.out = math::min(max_radius, radius);
  }
}

/* Computes the maximum possible defocus radius in pixels. */
float ConvertDepthToRadiusOperation::compute_maximum_defocus_radius() const
{
  const float maximum_diameter = compute_maximum_diameter_of_circle_of_confusion();
  const float pixels_per_meter = compute_pixels_per_meter();
  const float radius = (maximum_diameter / 2.0f) * pixels_per_meter;
  return math::min(radius, data_->maxblur);
}

/* Computes the diameter of the circle of confusion at infinity. This computes the limit in
 * figure (5) of the paper:
 *
 *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
 *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305.
 *
 * Notice that the diameter is asymmetric around the focus point, and we are computing the
 * limiting diameter at infinity, while another limiting diameter exist at zero distance from the
 * lens. This is a limitation of the implementation, as it assumes far defocusing only. */
float ConvertDepthToRadiusOperation::compute_maximum_diameter_of_circle_of_confusion() const
{
  const float f_stop = get_f_stop();
  const float focal_length = get_focal_length();
  const float distance_to_image_of_focus = compute_distance_to_image_of_focus();
  return math::abs((distance_to_image_of_focus / (f_stop * focal_length)) -
                   (focal_length / f_stop));
}

/* Computes the distance in meters to the image of the focus point across a lens of the specified
 * focal length. This computes `Vp` in equation (7) of the paper:
 *
 *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
 *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */
float ConvertDepthToRadiusOperation::compute_distance_to_image_of_focus() const
{
  const float focal_length = get_focal_length();
  const float focus_distance = compute_focus_distance();
  return (focal_length * focus_distance) / (focus_distance - focal_length);
}

/* Returns the focal length in meters. Fallback to 50 mm in case of an invalid camera. Ensure a
 * minimum of 1e-6. */
float ConvertDepthToRadiusOperation::get_focal_length() const
{
  const Camera *camera = get_camera();
  return camera ? math::max(1e-6f, camera->lens / 1000.0f) : 50.0f / 1000.0f;
}

/* Computes the distance to the point that is completely in focus. Default to 10 meters for null
 * camera. */
float ConvertDepthToRadiusOperation::compute_focus_distance() const
{
  const Object *camera_object = get_camera_object();
  if (!camera_object) {
    return 10.0f;
  }
  return BKE_camera_object_dof_distance(camera_object);
}

/* Computes the number of pixels per meter of the sensor size. This is essentially the resolution
 * over the sensor size, using the sensor fit axis. Fallback to DEFAULT_SENSOR_WIDTH in case of
 * an invalid camera. Note that the stored sensor size is in millimeter, so convert to meters. */
float ConvertDepthToRadiusOperation::compute_pixels_per_meter() const
{
  const int2 size = int2(image_input_operation_->get_width(),
                         image_input_operation_->get_height());
  const Camera *camera = get_camera();
  const float default_value = size.x / (DEFAULT_SENSOR_WIDTH / 1000.0f);
  if (!camera) {
    return default_value;
  }

  switch (camera->sensor_fit) {
    case CAMERA_SENSOR_FIT_HOR:
      return size.x / (camera->sensor_x / 1000.0f);
    case CAMERA_SENSOR_FIT_VERT:
      return size.y / (camera->sensor_y / 1000.0f);
    case CAMERA_SENSOR_FIT_AUTO: {
      return size.x > size.y ? size.x / (camera->sensor_x / 1000.0f) :
                               size.y / (camera->sensor_y / 1000.0f);
    }
    default:
      break;
  }

  return default_value;
}

/* Returns the f-stop number. Fallback to 1e-3 for zero f-stop. */
float ConvertDepthToRadiusOperation::get_f_stop() const
{
  return math::max(1e-3f, data_->fstop);
}

const Camera *ConvertDepthToRadiusOperation::get_camera() const
{
  const Object *camera_object = get_camera_object();
  if (!camera_object || camera_object->type != OB_CAMERA) {
    return nullptr;
  }

  return reinterpret_cast<Camera *>(camera_object->data);
}

const Object *ConvertDepthToRadiusOperation::get_camera_object() const
{
  return scene_->camera;
}

}  // namespace blender::compositor
