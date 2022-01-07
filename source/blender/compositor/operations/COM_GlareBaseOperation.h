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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_SingleThreadedOperation.h"
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

class GlareBaseOperation : public SingleThreadedOperation {
 private:
  /**
   * \brief Cached reference to the input_program
   */
  SocketReader *input_program_;

  /**
   * \brief settings of the glare node.
   */
  NodeGlare *settings_;

  bool is_output_rendered_;

 public:
  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_glare_settings(NodeGlare *settings)
  {
    settings_ = settings;
  }
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) final;

 protected:
  GlareBaseOperation();

  virtual void generate_glare(float *data, MemoryBuffer *input_tile, NodeGlare *settings) = 0;

  MemoryBuffer *create_memory_buffer(rcti *rect) override;
};

}  // namespace blender::compositor
