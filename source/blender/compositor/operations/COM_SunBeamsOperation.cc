/* This program is free software; you can redistribute it and/or
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
 * Copyright 2014, Blender Foundation.
 */

#include "MEM_guardedalloc.h"

#include "COM_SunBeamsOperation.h"

namespace blender::compositor {

SunBeamsOperation::SunBeamsOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);

  flags_.complex = true;
}

void SunBeamsOperation::calc_rays_common_data()
{
  /* convert to pixels */
  source_px_[0] = data_.source[0] * this->get_width();
  source_px_[1] = data_.source[1] * this->get_height();
  ray_length_px_ = data_.ray_length * MAX2(this->get_width(), this->get_height());
}

void SunBeamsOperation::init_execution()
{
  calc_rays_common_data();
}

/**
 * Defines a line accumulator for a specific sector,
 * given by the four matrix entries that rotate from buffer space into the sector
 *
 * (x,y) is used to designate buffer space coordinates
 * (u,v) is used to designate sector space coordinates
 *
 * For a target point (x,y) the sector should be chosen such that
 *   `u >= v >= 0`
 * This removes the need to handle all sorts of special cases.
 *
 * Template parameters:
 * \param fxu: buffer increment in x for sector `u + 1`.
 * \param fxv: buffer increment in x for sector `v + 1`.
 * \param fyu: buffer increment in y for sector `u + 1`.
 * \param fyv: buffer increment in y for sector `v + 1`.
 */
template<int fxu, int fxv, int fyu, int fyv> struct BufferLineAccumulator {

  /* utility functions implementing the matrix transform to/from sector space */

  static inline void buffer_to_sector(const float source[2], int x, int y, int &u, int &v)
  {
    int x0 = (int)source[0];
    int y0 = (int)source[1];
    x -= x0;
    y -= y0;
    u = x * fxu + y * fyu;
    v = x * fxv + y * fyv;
  }

  static inline void buffer_to_sector(const float source[2], float x, float y, float &u, float &v)
  {
    int x0 = (int)source[0];
    int y0 = (int)source[1];
    x -= (float)x0;
    y -= (float)y0;
    u = x * fxu + y * fyu;
    v = x * fxv + y * fyv;
  }

  static inline void sector_to_buffer(const float source[2], int u, int v, int &x, int &y)
  {
    int x0 = (int)source[0];
    int y0 = (int)source[1];
    x = x0 + u * fxu + v * fxv;
    y = y0 + u * fyu + v * fyv;
  }

  static inline void sector_to_buffer(const float source[2], float u, float v, float &x, float &y)
  {
    int x0 = (int)source[0];
    int y0 = (int)source[1];
    x = (float)x0 + u * fxu + v * fxv;
    y = (float)y0 + u * fyu + v * fyv;
  }

  /**
   * Set up the initial buffer pointer and calculate necessary variables for looping.
   *
   * Note that sector space is centered around the "source" point while the loop starts
   * at dist_min from the target pt. This way the loop can be canceled as soon as it runs
   * out of the buffer rect, because no pixels further along the line can contribute.
   *
   * \param x, y: Start location in the buffer
   * \param num: Total steps in the loop
   * \param v, dv: Vertical offset in sector space, for line offset perpendicular to the loop axis
   */
  static float *init_buffer_iterator(MemoryBuffer *input,
                                     const float source[2],
                                     const float co[2],
                                     float dist_min,
                                     float dist_max,
                                     int &x,
                                     int &y,
                                     int &num,
                                     float &v,
                                     float &dv,
                                     float &falloff_factor)
  {
    float pu, pv;
    buffer_to_sector(source, co[0], co[1], pu, pv);

    /* line angle */
    float tan_phi = pv / pu;
    float dr = sqrtf(tan_phi * tan_phi + 1.0f);
    float cos_phi = 1.0f / dr;

    /* clamp u range to avoid influence of pixels "behind" the source */
    float umin = max_ff(pu - cos_phi * dist_min, 0.0f);
    float umax = max_ff(pu - cos_phi * dist_max, 0.0f);
    v = umin * tan_phi;
    dv = tan_phi;

    int start = (int)floorf(umax);
    int end = (int)ceilf(umin);
    num = end - start;

    sector_to_buffer(source, end, (int)ceilf(v), x, y);

    falloff_factor = dist_max > dist_min ? dr / (float)(dist_max - dist_min) : 0.0f;

    float *iter = input->get_buffer() + input->get_coords_offset(x, y);
    return iter;
  }

  /**
   * Perform the actual accumulation along a ray segment from source to pt.
   * Only pixels within dist_min..dist_max contribute.
   *
   * The loop runs backwards(!) over the primary sector space axis u, i.e. increasing distance to
   * pt. After each step it decrements v by dv < 1, adding a buffer shift when necessary.
   */
  static void eval(MemoryBuffer *input,
                   float output[4],
                   const float co[2],
                   const float source[2],
                   float dist_min,
                   float dist_max)
  {
    const rcti &rect = input->get_rect();
    int x, y, num;
    float v, dv;
    float falloff_factor;
    float border[4];

    zero_v4(output);

    if ((int)(co[0] - source[0]) == 0 && (int)(co[1] - source[1]) == 0) {
      copy_v4_v4(output, input->get_elem(source[0], source[1]));
      return;
    }

    /* Initialize the iteration variables. */
    float *buffer = init_buffer_iterator(
        input, source, co, dist_min, dist_max, x, y, num, v, dv, falloff_factor);
    zero_v3(border);
    border[3] = 1.0f;

    /* v_local keeps track of when to decrement v (see below) */
    float v_local = v - floorf(v);

    for (int i = 0; i < num; i++) {
      float weight = 1.0f - (float)i * falloff_factor;
      weight *= weight;

      /* range check, use last valid color when running beyond the image border */
      if (x >= rect.xmin && x < rect.xmax && y >= rect.ymin && y < rect.ymax) {
        madd_v4_v4fl(output, buffer, buffer[3] * weight);
        /* use as border color in case subsequent pixels are out of bounds */
        copy_v4_v4(border, buffer);
      }
      else {
        madd_v4_v4fl(output, border, border[3] * weight);
      }

      /* TODO: implement proper filtering here, see
       * https://en.wikipedia.org/wiki/Lanczos_resampling
       * https://en.wikipedia.org/wiki/Sinc_function
       *
       * using lanczos with x = distance from the line segment,
       * normalized to a == 0.5f, could give a good result
       *
       * for now just divide equally at the end ...
       */

      /* decrement u */
      x -= fxu;
      y -= fyu;
      buffer -= fxu * input->elem_stride + fyu * input->row_stride;

      /* decrement v (in steps of dv < 1) */
      v_local -= dv;
      if (v_local < 0.0f) {
        v_local += 1.0f;

        x -= fxv;
        y -= fyv;
        buffer -= fxv * input->elem_stride + fyv * input->row_stride;
      }
    }

    /* normalize */
    if (num > 0) {
      mul_v4_fl(output, 1.0f / (float)num);
    }
  }
};

/**
 * Dispatch function which selects an appropriate accumulator based on the sector of the target
 * point, relative to the source.
 *
 * The BufferLineAccumulator defines the actual loop over the buffer, with an efficient inner loop
 * due to using compile time constants instead of a local matrix variable defining the sector
 * space.
 */
static void accumulate_line(MemoryBuffer *input,
                            float output[4],
                            const float co[2],
                            const float source[2],
                            float dist_min,
                            float dist_max)
{
  /* coordinates relative to source */
  float pt_ofs[2] = {co[0] - source[0], co[1] - source[1]};

  /* The source sectors are defined like so:
   *
   *   \ 3 | 2 /
   *    \  |  /
   *   4 \ | / 1
   *      \|/
   *  -----------
   *      /|\
   *   5 / | \ 8
   *    /  |  \
   *   / 6 | 7 \
   *
   * The template arguments encode the transformation into "sector space",
   * by means of rotation/mirroring matrix elements.
   */

  if (fabsf(pt_ofs[1]) > fabsf(pt_ofs[0])) {
    if (pt_ofs[0] > 0.0f) {
      if (pt_ofs[1] > 0.0f) {
        /* 2 */
        BufferLineAccumulator<0, 1, 1, 0>::eval(input, output, co, source, dist_min, dist_max);
      }
      else {
        /* 7 */
        BufferLineAccumulator<0, 1, -1, 0>::eval(input, output, co, source, dist_min, dist_max);
      }
    }
    else {
      if (pt_ofs[1] > 0.0f) {
        /* 3 */
        BufferLineAccumulator<0, -1, 1, 0>::eval(input, output, co, source, dist_min, dist_max);
      }
      else {
        /* 6 */
        BufferLineAccumulator<0, -1, -1, 0>::eval(input, output, co, source, dist_min, dist_max);
      }
    }
  }
  else {
    if (pt_ofs[0] > 0.0f) {
      if (pt_ofs[1] > 0.0f) {
        /* 1 */
        BufferLineAccumulator<1, 0, 0, 1>::eval(input, output, co, source, dist_min, dist_max);
      }
      else {
        /* 8 */
        BufferLineAccumulator<1, 0, 0, -1>::eval(input, output, co, source, dist_min, dist_max);
      }
    }
    else {
      if (pt_ofs[1] > 0.0f) {
        /* 4 */
        BufferLineAccumulator<-1, 0, 0, 1>::eval(input, output, co, source, dist_min, dist_max);
      }
      else {
        /* 5 */
        BufferLineAccumulator<-1, 0, 0, -1>::eval(input, output, co, source, dist_min, dist_max);
      }
    }
  }
}

void *SunBeamsOperation::initialize_tile_data(rcti * /*rect*/)
{
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  return buffer;
}

void SunBeamsOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const float co[2] = {(float)x, (float)y};

  accumulate_line((MemoryBuffer *)data, output, co, source_px_, 0.0f, ray_length_px_);
}

static void calc_ray_shift(rcti *rect, float x, float y, const float source[2], float ray_length)
{
  float co[2] = {(float)x, (float)y};
  float dir[2], dist;

  /* move (x,y) vector toward the source by ray_length distance */
  sub_v2_v2v2(dir, co, source);
  dist = normalize_v2(dir);
  mul_v2_fl(dir, min_ff(dist, ray_length));
  sub_v2_v2(co, dir);

  int ico[2] = {(int)co[0], (int)co[1]};
  BLI_rcti_do_minmax_v(rect, ico);
}

bool SunBeamsOperation::determine_depending_area_of_interest(rcti *input,
                                                             ReadBufferOperation *read_operation,
                                                             rcti *output)
{
  /* Enlarges the rect by moving each corner toward the source.
   * This is the maximum distance that pixels can influence each other
   * and gives a rect that contains all possible accumulated pixels.
   */
  rcti rect = *input;
  calc_ray_shift(&rect, input->xmin, input->ymin, source_px_, ray_length_px_);
  calc_ray_shift(&rect, input->xmin, input->ymax, source_px_, ray_length_px_);
  calc_ray_shift(&rect, input->xmax, input->ymin, source_px_, ray_length_px_);
  calc_ray_shift(&rect, input->xmax, input->ymax, source_px_, ray_length_px_);

  return NodeOperation::determine_depending_area_of_interest(&rect, read_operation, output);
}

void SunBeamsOperation::get_area_of_interest(const int input_idx,
                                             const rcti &output_area,
                                             rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS(input_idx);
  calc_rays_common_data();

  r_input_area = output_area;
  /* Enlarges the rect by moving each corner toward the source.
   * This is the maximum distance that pixels can influence each other
   * and gives a rect that contains all possible accumulated pixels. */
  calc_ray_shift(&r_input_area, output_area.xmin, output_area.ymin, source_px_, ray_length_px_);
  calc_ray_shift(&r_input_area, output_area.xmin, output_area.ymax, source_px_, ray_length_px_);
  calc_ray_shift(&r_input_area, output_area.xmax, output_area.ymin, source_px_, ray_length_px_);
  calc_ray_shift(&r_input_area, output_area.xmax, output_area.ymax, source_px_, ray_length_px_);
}

void SunBeamsOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];
  float coords[2];
  for (int y = area.ymin; y < area.ymax; y++) {
    coords[1] = y;
    float *out_elem = output->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      coords[0] = x;
      accumulate_line(input, out_elem, coords, source_px_, 0.0f, ray_length_px_);
      out_elem += output->elem_stride;
    }
  }
}

}  // namespace blender::compositor
