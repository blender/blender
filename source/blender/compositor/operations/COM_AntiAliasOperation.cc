/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_AntiAliasOperation.h"

namespace blender::compositor {

/* An implementation of the Scale3X edge-extrapolation algorithm.
 *
 * Code from GIMP plugin, based on code from Adam D. Moss <adam@gimp.org>
 * licensed by the MIT license.
 */
static int extrapolate9(float *E0,
                        float *E1,
                        float *E2,
                        float *E3,
                        float *E4,
                        float *E5,
                        float *E6,
                        float *E7,
                        float *E8,
                        const float *A,
                        const float *B,
                        const float *C,
                        const float *D,
                        const float *E,
                        const float *F,
                        const float *G,
                        const float *H,
                        const float *I)
{
#define PEQ(X, Y) (fabsf(*X - *Y) < 1e-3f)
#define PCPY(DST, SRC) \
  do { \
    *DST = *SRC; \
  } while (0)
  if (!PEQ(B, H) && !PEQ(D, F)) {
    if (PEQ(D, B)) {
      PCPY(E0, D);
    }
    else {
      PCPY(E0, E);
    }
    if ((PEQ(D, B) && !PEQ(E, C)) || (PEQ(B, F) && !PEQ(E, A))) {
      PCPY(E1, B);
    }
    else {
      PCPY(E1, E);
    }
    if (PEQ(B, F)) {
      PCPY(E2, F);
    }
    else {
      PCPY(E2, E);
    }
    if ((PEQ(D, B) && !PEQ(E, G)) || (PEQ(D, H) && !PEQ(E, A))) {
      PCPY(E3, D);
    }
    else {
      PCPY(E3, E);
    }
    PCPY(E4, E);
    if ((PEQ(B, F) && !PEQ(E, I)) || (PEQ(H, F) && !PEQ(E, C))) {
      PCPY(E5, F);
    }
    else {
      PCPY(E5, E);
    }
    if (PEQ(D, H)) {
      PCPY(E6, D);
    }
    else {
      PCPY(E6, E);
    }
    if ((PEQ(D, H) && !PEQ(E, I)) || (PEQ(H, F) && !PEQ(E, G))) {
      PCPY(E7, H);
    }
    else {
      PCPY(E7, E);
    }
    if (PEQ(H, F)) {
      PCPY(E8, F);
    }
    else {
      PCPY(E8, E);
    }
    return 1;
  }

  return 0;

#undef PEQ
#undef PCPY
}

AntiAliasOperation::AntiAliasOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  value_reader_ = nullptr;
  flags_.complex = true;
}

void AntiAliasOperation::init_execution()
{
  value_reader_ = this->get_input_socket_reader(0);
}

void AntiAliasOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const int buffer_width = input_buffer->get_width(), buffer_height = input_buffer->get_height();
  if (y < 0 || y >= buffer_height || x < 0 || x >= buffer_width) {
    output[0] = 0.0f;
  }
  else {
    const float *buffer = input_buffer->get_buffer();
    const float *row_curr = &buffer[y * buffer_width];
    if (x == 0 || x == buffer_width - 1 || y == 0 || y == buffer_height - 1) {
      output[0] = row_curr[x];
      return;
    }
    const float *row_prev = &buffer[(y - 1) * buffer_width],
                *row_next = &buffer[(y + 1) * buffer_width];
    float ninepix[9];
    if (extrapolate9(&ninepix[0],
                     &ninepix[1],
                     &ninepix[2],
                     &ninepix[3],
                     &ninepix[4],
                     &ninepix[5],
                     &ninepix[6],
                     &ninepix[7],
                     &ninepix[8],
                     &row_prev[x - 1],
                     &row_prev[x],
                     &row_prev[x + 1],
                     &row_curr[x - 1],
                     &row_curr[x],
                     &row_curr[x + 1],
                     &row_next[x - 1],
                     &row_next[x],
                     &row_next[x + 1]))
    {
      /* Some rounding magic to so make weighting correct with the
       * original coefficients.
       */
      uchar result = ((3 * ninepix[0] + 5 * ninepix[1] + 3 * ninepix[2] + 5 * ninepix[3] +
                       6 * ninepix[4] + 5 * ninepix[5] + 3 * ninepix[6] + 5 * ninepix[7] +
                       3 * ninepix[8]) *
                          255.0f +
                      19.0f) /
                     38.0f;
      output[0] = result / 255.0f;
    }
    else {
      output[0] = row_curr[x];
    }
  }
}

void AntiAliasOperation::deinit_execution()
{
  value_reader_ = nullptr;
}

bool AntiAliasOperation::determine_depending_area_of_interest(rcti *input,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti image_input;
  NodeOperation *operation = get_input_operation(0);
  image_input.xmax = input->xmax + 1;
  image_input.xmin = input->xmin - 1;
  image_input.ymax = input->ymax + 1;
  image_input.ymin = input->ymin - 1;
  return operation->determine_depending_area_of_interest(&image_input, read_operation, output);
}

void *AntiAliasOperation::initialize_tile_data(rcti *rect)
{
  return get_input_operation(0)->initialize_tile_data(rect);
}

void AntiAliasOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmax = output_area.xmax + 1;
  r_input_area.xmin = output_area.xmin - 1;
  r_input_area.ymax = output_area.ymax + 1;
  r_input_area.ymin = output_area.ymin - 1;
}

void AntiAliasOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const rcti &input_area = input->get_rect();
  float ninepix[9];
  for (int y = area.ymin; y < area.ymax; y++) {
    float *out = output->get_elem(area.xmin, y);
    const float *row_curr = input->get_elem(area.xmin, y);
    const float *row_prev = row_curr - input->row_stride;
    const float *row_next = row_curr + input->row_stride;
    int x_offset = 0;
    for (int x = area.xmin; x < area.xmax;
         x++, out += output->elem_stride, x_offset += input->elem_stride)
    {
      if (x == input_area.xmin || x == input_area.xmax - 1 || y == input_area.xmin ||
          y == input_area.ymax - 1)
      {
        out[0] = row_curr[x_offset];
        continue;
      }

      if (extrapolate9(&ninepix[0],
                       &ninepix[1],
                       &ninepix[2],
                       &ninepix[3],
                       &ninepix[4],
                       &ninepix[5],
                       &ninepix[6],
                       &ninepix[7],
                       &ninepix[8],
                       &row_prev[x_offset - input->elem_stride],
                       &row_prev[x_offset],
                       &row_prev[x_offset + input->elem_stride],
                       &row_curr[x_offset - input->elem_stride],
                       &row_curr[x_offset],
                       &row_curr[x_offset + input->elem_stride],
                       &row_next[x_offset - input->elem_stride],
                       &row_next[x_offset],
                       &row_next[x_offset + input->elem_stride]))
      {
        /* Some rounding magic to make weighting correct with the
         * original coefficients. */
        uchar result = ((3 * ninepix[0] + 5 * ninepix[1] + 3 * ninepix[2] + 5 * ninepix[3] +
                         6 * ninepix[4] + 5 * ninepix[5] + 3 * ninepix[6] + 5 * ninepix[7] +
                         3 * ninepix[8]) *
                            255.0f +
                        19.0f) /
                       38.0f;
        out[0] = result / 255.0f;
      }
      else {
        out[0] = row_curr[x_offset];
      }
    }
  }
}

}  // namespace blender::compositor
