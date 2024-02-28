/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/* Utility functions used by glare, tone-map and lens distortion. */
/* Some macros for color handling. */
typedef float fRGB[4];

/* TODO: replace with BLI_math_vector. */
/* multiply c2 by color rgb, rgb as separate arguments */
#define fRGB_rgbmult(c, r, g, b) \
  { \
    c[0] *= (r); \
    c[1] *= (g); \
    c[2] *= (b); \
  } \
  (void)0

class GlareBaseOperation : public NodeOperation {
 private:
  /**
   * \brief settings of the glare node.
   */
  const NodeGlare *settings_;

  bool is_output_rendered_;

 public:
  void set_glare_settings(const NodeGlare *settings)
  {
    settings_ = settings;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) final;

 protected:
  GlareBaseOperation();

  virtual void generate_glare(float *data,
                              MemoryBuffer *input_tile,
                              const NodeGlare *settings) = 0;
};

}  // namespace blender::compositor
