/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_node_types.h"

struct RNG;

namespace blender::compositor {

class ScreenLensDistortionOperation : public MultiThreadedOperation {
 private:
  struct RNG *rng_;

  bool fit_;
  bool jitter_;

  float dispersion_;
  float distortion_;
  bool dispersion_const_;
  bool distortion_const_;
  bool variables_ready_;
  float k_[3];
  float k4_[3];
  float dk4_[3];
  float maxk_;
  float sc_, cx_, cy_;

 public:
  ScreenLensDistortionOperation();

  void init_data() override;

  void init_execution() override;
  void deinit_execution() override;

  void set_fit(bool fit)
  {
    fit_ = fit;
  }
  void set_jitter(bool jitter)
  {
    jitter_ = jitter;
  }

  /** Set constant distortion value */
  void set_distortion(float distortion);
  /** Set constant dispersion value */
  void set_dispersion(float dispersion);

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void determineUV(float result[6], float x, float y) const;
  void update_variables(float distortion, float dispersion);

  void get_uv(const float xy[2], float uv[2]) const;
  void distort_uv(const float uv[2], float t, float xy[2]) const;
  bool get_delta(float r_sq, float k4, const float uv[2], float delta[2]) const;
  void accumulate(const MemoryBuffer *buffer,
                  int a,
                  int b,
                  float r_sq,
                  const float uv[2],
                  const float delta[3][2],
                  float sum[4],
                  int count[3]) const;
};

}  // namespace blender::compositor
