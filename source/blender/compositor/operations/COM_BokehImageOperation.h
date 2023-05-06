/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * \brief The #BokehImageOperation class is an operation that creates an image useful to mimic the
 * internals of a camera.
 *
 * features:
 *  - number of flaps
 *  - angle offset of the flaps
 *  - rounding of the flaps (also used to make a circular lens)
 *  - simulate catadioptric
 *  - simulate lens-shift
 *
 * Per pixel the algorithm determines the edge of the bokeh on the same line as the center of the
 * image and the pixel is evaluating.
 *
 * The edge is detected by finding the closest point on the direct line between the two nearest
 * flap-corners. this edge is interpolated with a full circle. Result of this edge detection is
 * stored as the distance between the center of the image and the edge.
 *
 * catadioptric lenses are simulated to interpolate between the center of the image and the
 * distance of the edge. We now have three distances:
 * - Distance between the center of the image and the pixel to be evaluated.
 * - Distance between the center of the image and the outer-edge.
 * - Distance between the center of the image and the inner-edge.
 *
 * With a simple compare it can be detected if the evaluated pixel is between the outer and inner
 * edge.
 */
class BokehImageOperation : public MultiThreadedOperation {
 private:
  /**
   * \brief Settings of the bokeh image
   */
  const NodeBokehImage *data_;

  /**
   * \brief precalculate center of the image
   */
  float center_[2];

  /**
   * \brief 1.0-rounding
   */
  float inverse_rounding_;

  /**
   * \brief distance of a full circle lens
   */
  float circular_distance_;

  /**
   * \brief radius when the first flap starts
   */
  float flap_rad_;

  /**
   * \brief radians of a single flap
   */
  float flap_rad_add_;

  /**
   * \brief should the data_ field by deleted when this operation is finished
   */
  bool delete_data_;

  /**
   * \brief determine the coordinate of a flap corner.
   *
   * \param r: result in bokeh-image space are stored [x,y]
   * \param flap_number: the flap number to calculate
   * \param distance: the lens distance is used to simulate lens shifts
   */
  void detemine_start_point_of_flap(float r[2], int flap_number, float distance);

  /**
   * \brief Determine if a coordinate is inside the bokeh image
   *
   * \param distance: the distance that will be used.
   * This parameter is modified a bit to mimic lens shifts.
   * \param x: the x coordinate of the pixel to evaluate
   * \param y: the y coordinate of the pixel to evaluate
   * \return float range 0..1 0 is completely outside
   */
  float is_inside_bokeh(float distance, float x, float y);

 public:
  BokehImageOperation();

  /**
   * \brief The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * \brief Initialize the execution
   */
  void init_execution() override;

  /**
   * \brief De-initialize the execution
   */
  void deinit_execution() override;

  /**
   * \brief determine the resolution of this operation. currently fixed at [COM_BLUR_BOKEH_PIXELS,
   * COM_BLUR_BOKEH_PIXELS] \param resolution: \param preferred_resolution:
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  /**
   * \brief set the node data
   * \param data:
   */
  void set_data(const NodeBokehImage *data)
  {
    data_ = data;
  }

  /**
   * \brief delete_data_on_finish
   *
   * There are cases that the compositor uses this operation on its own (see defocus node)
   * the delete_data_on_finish must only be called when the data has been created by the
   *compositor. It should not be called when the data has been created by the node-editor/user.
   */
  void delete_data_on_finish()
  {
    delete_data_ = true;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
