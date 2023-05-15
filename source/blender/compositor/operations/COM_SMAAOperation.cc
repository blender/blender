/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

#include "COM_SMAAOperation.h"
#include "BKE_node.hh"
#include "COM_SMAAAreaTexture.h"

extern "C" {
#include "IMB_colormanagement.h"
}

namespace blender::compositor {

/*
 * An implementation of Enhanced Sub-pixel Morphological Anti-aliasing (SMAA)
 *
 * The algorithm was proposed by:
 *   Jorge Jimenez, Jose I. Echevarria, Tiago Sousa, Diego Gutierrez
 *
 *   http://www.iryoku.com/smaa/
 *
 * This file is based on SMAA-CPP:
 *
 *   https://github.com/i_ri-E/smaa-cpp
 *
 * Currently only SMAA 1x mode is provided, so the operation will be done
 * with no spatial multi-sampling nor temporal super-sampling.
 *
 * NOTE: This program assumes the screen coordinates are DirectX style, so
 * the vertical direction is upside-down. "top" and "bottom" actually mean
 * bottom and top, respectively.
 */

/*-----------------------------------------------------------------------------*/
/* Non-Configurable Defines */

#define SMAA_AREATEX_SIZE 80
#define SMAA_AREATEX_MAX_DISTANCE 20
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_MAX_SEARCH_STEPS 362 /* 362 - 1 = 19^2 */
#define SMAA_MAX_SEARCH_STEPS_DIAG 19

/*-----------------------------------------------------------------------------*/
/* Internal Functions to Sample Pixel Color from Image */

/* TODO(manzanilla): to be removed with tiled implementation. Replace it with
 * #buffer->read_elem_checked. */
static inline void sample(SocketReader *reader, int x, int y, float color[4])
{
  if (x < 0 || x >= reader->get_width() || y < 0 || y >= reader->get_height()) {
    color[0] = color[1] = color[2] = color[3] = 0.0;
    return;
  }

  reader->read(color, x, y, nullptr);
}

static inline void sample(MemoryBuffer *reader, int x, int y, float color[4])
{
  reader->read_elem_checked(x, y, color);
}

template<typename T>
static void sample_bilinear_vertical(T *reader, int x, int y, float yoffset, float color[4])
{
  float iy = floorf(yoffset);
  float fy = yoffset - iy;
  y += int(iy);

  float color00[4], color01[4];

  sample(reader, x + 0, y + 0, color00);
  sample(reader, x + 0, y + 1, color01);

  color[0] = interpf(color01[0], color00[0], fy);
  color[1] = interpf(color01[1], color00[1], fy);
  color[2] = interpf(color01[2], color00[2], fy);
  color[3] = interpf(color01[3], color00[3], fy);
}

template<typename T>
static void sample_bilinear_horizontal(T *reader, int x, int y, float xoffset, float color[4])
{
  float ix = floorf(xoffset);
  float fx = xoffset - ix;
  x += int(ix);

  float color00[4], color10[4];

  sample(reader, x + 0, y + 0, color00);
  sample(reader, x + 1, y + 0, color10);

  color[0] = interpf(color10[0], color00[0], fx);
  color[1] = interpf(color10[1], color00[1], fx);
  color[2] = interpf(color10[2], color00[2], fx);
  color[3] = interpf(color10[3], color00[3], fx);
}

/*-----------------------------------------------------------------------------*/
/* Internal Functions to Sample Blending Weights from AreaTex */

static inline const float *areatex_sample_internal(const float *areatex, int x, int y)
{
  return &areatex[(CLAMPIS(x, 0, SMAA_AREATEX_SIZE - 1) +
                   CLAMPIS(y, 0, SMAA_AREATEX_SIZE - 1) * SMAA_AREATEX_SIZE) *
                  2];
}

/**
 * We have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
static void area(int d1, int d2, int e1, int e2, float weights[2])
{
  /* The areas texture is compressed  quadratically: */
  float x = float(SMAA_AREATEX_MAX_DISTANCE * e1) + sqrtf(float(d1));
  float y = float(SMAA_AREATEX_MAX_DISTANCE * e2) + sqrtf(float(d2));

  float ix = floorf(x), iy = floorf(y);
  float fx = x - ix, fy = y - iy;
  int X = int(ix), Y = int(iy);

  const float *weights00 = areatex_sample_internal(areatex, X + 0, Y + 0);
  const float *weights10 = areatex_sample_internal(areatex, X + 1, Y + 0);
  const float *weights01 = areatex_sample_internal(areatex, X + 0, Y + 1);
  const float *weights11 = areatex_sample_internal(areatex, X + 1, Y + 1);

  weights[0] = interpf(
      interpf(weights11[0], weights01[0], fx), interpf(weights10[0], weights00[0], fx), fy);
  weights[1] = interpf(
      interpf(weights11[1], weights01[1], fx), interpf(weights10[1], weights00[1], fx), fy);
}

/**
 * Similar to area(), this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
static void area_diag(int d1, int d2, int e1, int e2, float weights[2])
{
  int x = SMAA_AREATEX_MAX_DISTANCE_DIAG * e1 + d1;
  int y = SMAA_AREATEX_MAX_DISTANCE_DIAG * e2 + d2;

  const float *w = areatex_sample_internal(areatex_diag, x, y);
  copy_v2_v2(weights, w);
}

/*-----------------------------------------------------------------------------*/
/* Edge Detection (First Pass) */
/*-----------------------------------------------------------------------------*/

SMAAEdgeDetectionOperation::SMAAEdgeDetectionOperation()
{
  this->add_input_socket(DataType::Color); /* image */
  this->add_input_socket(DataType::Value); /* Depth, material ID, etc. TODO: currently unused. */
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  image_reader_ = nullptr;
  value_reader_ = nullptr;
  this->set_threshold(CMP_DEFAULT_SMAA_THRESHOLD);
  this->set_local_contrast_adaptation_factor(CMP_DEFAULT_SMAA_CONTRAST_LIMIT);
}

void SMAAEdgeDetectionOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
  value_reader_ = this->get_input_socket_reader(1);
}

void SMAAEdgeDetectionOperation::deinit_execution()
{
  image_reader_ = nullptr;
  value_reader_ = nullptr;
}

void SMAAEdgeDetectionOperation::set_threshold(float threshold)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 0 and 0.5 */
  threshold_ = scalenorm(0, 0.5, threshold);
}

void SMAAEdgeDetectionOperation::set_local_contrast_adaptation_factor(float factor)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 1 and 10 */
  contrast_limit_ = scalenorm(1, 10, factor);
}

bool SMAAEdgeDetectionOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  new_input.xmax = input->xmax + 1;
  new_input.xmin = input->xmin - 2;
  new_input.ymax = input->ymax + 1;
  new_input.ymin = input->ymin - 2;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void SMAAEdgeDetectionOperation::get_area_of_interest(const int /*input_idx*/,
                                                      const rcti &output_area,
                                                      rcti &r_input_area)
{
  r_input_area.xmax = output_area.xmax + 1;
  r_input_area.xmin = output_area.xmin - 2;
  r_input_area.ymax = output_area.ymax + 1;
  r_input_area.ymin = output_area.ymin - 2;
}

void SMAAEdgeDetectionOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  float color[4];

  /* Calculate luma deltas: */
  sample(image_reader_, x, y, color);
  float L = IMB_colormanagement_get_luminance(color);
  sample(image_reader_, x - 1, y, color);
  float Lleft = IMB_colormanagement_get_luminance(color);
  sample(image_reader_, x, y - 1, color);
  float Ltop = IMB_colormanagement_get_luminance(color);
  float Dleft = fabsf(L - Lleft);
  float Dtop = fabsf(L - Ltop);

  /* We do the usual threshold: */
  output[0] = (x > 0 && Dleft >= threshold_) ? 1.0f : 0.0f;
  output[1] = (y > 0 && Dtop >= threshold_) ? 1.0f : 0.0f;
  output[2] = 0.0f;
  output[3] = 1.0f;

  /* Then discard if there is no edge: */
  if (is_zero_v2(output)) {
    return;
  }

  /* Calculate right and bottom deltas: */
  sample(image_reader_, x + 1, y, color);
  float Lright = IMB_colormanagement_get_luminance(color);
  sample(image_reader_, x, y + 1, color);
  float Lbottom = IMB_colormanagement_get_luminance(color);
  float Dright = fabsf(L - Lright);
  float Dbottom = fabsf(L - Lbottom);

  /* Calculate the maximum delta in the direct neighborhood: */
  float max_delta = fmaxf(fmaxf(Dleft, Dright), fmaxf(Dtop, Dbottom));

  /* Calculate luma used for both left and top edges: */
  sample(image_reader_, x - 1, y - 1, color);
  float Llefttop = IMB_colormanagement_get_luminance(color);

  /* Left edge */
  if (output[0] != 0.0f) {
    /* Calculate deltas around the left pixel: */
    sample(image_reader_, x - 2, y, color);
    float Lleftleft = IMB_colormanagement_get_luminance(color);
    sample(image_reader_, x - 1, y + 1, color);
    float Lleftbottom = IMB_colormanagement_get_luminance(color);
    float Dleftleft = fabsf(Lleft - Lleftleft);
    float Dlefttop = fabsf(Lleft - Llefttop);
    float Dleftbottom = fabsf(Lleft - Lleftbottom);

    /* Calculate the final maximum delta: */
    max_delta = fmaxf(max_delta, fmaxf(Dleftleft, fmaxf(Dlefttop, Dleftbottom)));

    /* Local contrast adaptation: */
    if (max_delta > contrast_limit_ * Dleft) {
      output[0] = 0.0f;
    }
  }

  /* Top edge */
  if (output[1] != 0.0f) {
    /* Calculate top-top delta: */
    sample(image_reader_, x, y - 2, color);
    float Ltoptop = IMB_colormanagement_get_luminance(color);
    sample(image_reader_, x + 1, y - 1, color);
    float Ltopright = IMB_colormanagement_get_luminance(color);
    float Dtoptop = fabsf(Ltop - Ltoptop);
    float Dtopleft = fabsf(Ltop - Llefttop);
    float Dtopright = fabsf(Ltop - Ltopright);

    /* Calculate the final maximum delta: */
    max_delta = fmaxf(max_delta, fmaxf(Dtoptop, fmaxf(Dtopleft, Dtopright)));

    /* Local contrast adaptation: */
    if (max_delta > contrast_limit_ * Dtop) {
      output[1] = 0.0f;
    }
  }
}

void SMAAEdgeDetectionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *image = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    const int x = it.x;
    const int y = it.y;

    /* Calculate luma deltas: */
    image->read_elem_checked(x, y, color);
    const float L = IMB_colormanagement_get_luminance(color);
    image->read_elem_checked(x - 1, y, color);
    const float Lleft = IMB_colormanagement_get_luminance(color);
    image->read_elem_checked(x, y - 1, color);
    const float Ltop = IMB_colormanagement_get_luminance(color);
    const float Dleft = fabsf(L - Lleft);
    const float Dtop = fabsf(L - Ltop);

    /* We do the usual threshold: */
    it.out[0] = (x > 0 && Dleft >= threshold_) ? 1.0f : 0.0f;
    it.out[1] = (y > 0 && Dtop >= threshold_) ? 1.0f : 0.0f;
    it.out[2] = 0.0f;
    it.out[3] = 1.0f;

    /* Then discard if there is no edge: */
    if (is_zero_v2(it.out)) {
      continue;
    }

    /* Calculate right and bottom deltas: */
    image->read_elem_checked(x + 1, y, color);
    const float Lright = IMB_colormanagement_get_luminance(color);
    image->read_elem_checked(x, y + 1, color);
    const float Lbottom = IMB_colormanagement_get_luminance(color);
    const float Dright = fabsf(L - Lright);
    const float Dbottom = fabsf(L - Lbottom);

    /* Calculate the maximum delta in the direct neighborhood: */
    float max_delta = fmaxf(fmaxf(Dleft, Dright), fmaxf(Dtop, Dbottom));

    /* Calculate luma used for both left and top edges: */
    image->read_elem_checked(x - 1, y - 1, color);
    const float Llefttop = IMB_colormanagement_get_luminance(color);

    /* Left edge */
    if (it.out[0] != 0.0f) {
      /* Calculate deltas around the left pixel: */
      image->read_elem_checked(x - 2, y, color);
      const float Lleftleft = IMB_colormanagement_get_luminance(color);
      image->read_elem_checked(x - 1, y + 1, color);
      const float Lleftbottom = IMB_colormanagement_get_luminance(color);
      const float Dleftleft = fabsf(Lleft - Lleftleft);
      const float Dlefttop = fabsf(Lleft - Llefttop);
      const float Dleftbottom = fabsf(Lleft - Lleftbottom);

      /* Calculate the final maximum delta: */
      max_delta = fmaxf(max_delta, fmaxf(Dleftleft, fmaxf(Dlefttop, Dleftbottom)));

      /* Local contrast adaptation: */
      if (max_delta > contrast_limit_ * Dleft) {
        it.out[0] = 0.0f;
      }
    }

    /* Top edge */
    if (it.out[1] != 0.0f) {
      /* Calculate top-top delta: */
      image->read_elem_checked(x, y - 2, color);
      const float Ltoptop = IMB_colormanagement_get_luminance(color);
      image->read_elem_checked(x + 1, y - 1, color);
      const float Ltopright = IMB_colormanagement_get_luminance(color);
      const float Dtoptop = fabsf(Ltop - Ltoptop);
      const float Dtopleft = fabsf(Ltop - Llefttop);
      const float Dtopright = fabsf(Ltop - Ltopright);

      /* Calculate the final maximum delta: */
      max_delta = fmaxf(max_delta, fmaxf(Dtoptop, fmaxf(Dtopleft, Dtopright)));

      /* Local contrast adaptation: */
      if (max_delta > contrast_limit_ * Dtop) {
        it.out[1] = 0.0f;
      }
    }
  }
}

/*-----------------------------------------------------------------------------*/
/* Blending Weight Calculation (Second Pass) */
/*-----------------------------------------------------------------------------*/

SMAABlendingWeightCalculationOperation::SMAABlendingWeightCalculationOperation()
{
  this->add_input_socket(DataType::Color); /* edges */
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  image_reader_ = nullptr;
  this->set_corner_rounding(CMP_DEFAULT_SMAA_CORNER_ROUNDING);
}

void *SMAABlendingWeightCalculationOperation::initialize_tile_data(rcti *rect)
{
  return get_input_operation(0)->initialize_tile_data(rect);
}

void SMAABlendingWeightCalculationOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
  if (execution_model_ == eExecutionModel::Tiled) {
    sample_image_fn_ = [=](int x, int y, float *out) { sample(image_reader_, x, y, out); };
  }
}

void SMAABlendingWeightCalculationOperation::set_corner_rounding(float rounding)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 0 and 100 */
  corner_rounding_ = int(scalenorm(0, 100, rounding));
}

void SMAABlendingWeightCalculationOperation::execute_pixel(float output[4],
                                                           int x,
                                                           int y,
                                                           void * /*data*/)
{
  float edges[4], c[4];

  zero_v4(output);
  sample(image_reader_, x, y, edges);

  /* Edge at north */
  if (edges[1] > 0.0f) {
    /* Diagonals have both north and west edges, so calculating weights for them */
    /* in one of the boundaries is enough. */
    calculate_diag_weights(x, y, edges, output);

    /* We give priority to diagonals, so if we find a diagonal we skip. */
    /* horizontal/vertical processing. */
    if (!is_zero_v2(output)) {
      return;
    }

    /* Find the distance to the left and the right: */
    int left = search_xleft(x, y);
    int right = search_xright(x, y);
    int d1 = x - left, d2 = right - x;

    /* Fetch the left and right crossing edges: */
    int e1 = 0, e2 = 0;
    sample(image_reader_, left, y - 1, c);
    if (c[0] > 0.0) {
      e1 += 1;
    }
    sample(image_reader_, left, y, c);
    if (c[0] > 0.0) {
      e1 += 2;
    }
    sample(image_reader_, right + 1, y - 1, c);
    if (c[0] > 0.0) {
      e2 += 1;
    }
    sample(image_reader_, right + 1, y, c);
    if (c[0] > 0.0) {
      e2 += 2;
    }

    /* Ok, we know how this pattern looks like, now it is time for getting */
    /* the actual area: */
    area(d1, d2, e1, e2, output); /* R, G */

    /* Fix corners: */
    if (corner_rounding_) {
      detect_horizontal_corner_pattern(output, left, right, y, d1, d2);
    }
  }

  /* Edge at west */
  if (edges[0] > 0.0f) {
    /* Did we already do diagonal search for this west edge from the left neighboring pixel? */
    if (is_vertical_search_unneeded(x, y)) {
      return;
    }

    /* Find the distance to the top and the bottom: */
    int top = search_yup(x, y);
    int bottom = search_ydown(x, y);
    int d1 = y - top, d2 = bottom - y;

    /* Fetch the top and bottom crossing edges: */
    int e1 = 0, e2 = 0;
    sample(image_reader_, x - 1, top, c);
    if (c[1] > 0.0) {
      e1 += 1;
    }
    sample(image_reader_, x, top, c);
    if (c[1] > 0.0) {
      e1 += 2;
    }
    sample(image_reader_, x - 1, bottom + 1, c);
    if (c[1] > 0.0) {
      e2 += 1;
    }
    sample(image_reader_, x, bottom + 1, c);
    if (c[1] > 0.0) {
      e2 += 2;
    }

    /* Get the area for this direction: */
    area(d1, d2, e1, e2, output + 2); /* B, A */

    /* Fix corners: */
    if (corner_rounding_) {
      detect_vertical_corner_pattern(output + 2, x, top, bottom, d1, d2);
    }
  }
}

void SMAABlendingWeightCalculationOperation::update_memory_buffer_started(
    MemoryBuffer * /*output*/, const rcti & /*out_area*/, Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *image = inputs[0];
  sample_image_fn_ = [=](int x, int y, float *out) { image->read_elem_checked(x, y, out); };
}

void SMAABlendingWeightCalculationOperation::update_memory_buffer_partial(
    MemoryBuffer *output, const rcti &out_area, Span<MemoryBuffer *> /*inputs*/)
{
  for (BuffersIterator<float> it = output->iterate_with({}, out_area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    zero_v4(it.out);

    float edges[4];
    sample_image_fn_(x, y, edges);

    /* Edge at north */
    float c[4];
    if (edges[1] > 0.0f) {
      /* Diagonals have both north and west edges, so calculating weights for them */
      /* in one of the boundaries is enough. */
      calculate_diag_weights(x, y, edges, it.out);

      /* We give priority to diagonals, so if we find a diagonal we skip. */
      /* horizontal/vertical processing. */
      if (!is_zero_v2(it.out)) {
        continue;
      }

      /* Find the distance to the left and the right: */
      int left = search_xleft(x, y);
      int right = search_xright(x, y);
      int d1 = x - left, d2 = right - x;

      /* Fetch the left and right crossing edges: */
      int e1 = 0, e2 = 0;
      sample_image_fn_(left, y - 1, c);
      if (c[0] > 0.0) {
        e1 += 1;
      }
      sample_image_fn_(left, y, c);
      if (c[0] > 0.0) {
        e1 += 2;
      }
      sample_image_fn_(right + 1, y - 1, c);
      if (c[0] > 0.0) {
        e2 += 1;
      }
      sample_image_fn_(right + 1, y, c);
      if (c[0] > 0.0) {
        e2 += 2;
      }

      /* Ok, we know how this pattern looks like, now it is time for getting */
      /* the actual area: */
      area(d1, d2, e1, e2, it.out); /* R, G */

      /* Fix corners: */
      if (corner_rounding_) {
        detect_horizontal_corner_pattern(it.out, left, right, y, d1, d2);
      }
    }

    /* Edge at west */
    if (edges[0] > 0.0f) {
      /* Did we already do diagonal search for this west edge from the left neighboring pixel? */
      if (is_vertical_search_unneeded(x, y)) {
        continue;
      }

      /* Find the distance to the top and the bottom: */
      int top = search_yup(x, y);
      int bottom = search_ydown(x, y);
      int d1 = y - top, d2 = bottom - y;

      /* Fetch the top and bottom crossing edges: */
      int e1 = 0, e2 = 0;
      sample_image_fn_(x - 1, top, c);
      if (c[1] > 0.0) {
        e1 += 1;
      }
      sample_image_fn_(x, top, c);
      if (c[1] > 0.0) {
        e1 += 2;
      }
      sample_image_fn_(x - 1, bottom + 1, c);
      if (c[1] > 0.0) {
        e2 += 1;
      }
      sample_image_fn_(x, bottom + 1, c);
      if (c[1] > 0.0) {
        e2 += 2;
      }

      /* Get the area for this direction: */
      area(d1, d2, e1, e2, it.out + 2); /* B, A */

      /* Fix corners: */
      if (corner_rounding_) {
        detect_vertical_corner_pattern(it.out + 2, x, top, bottom, d1, d2);
      }
    }
  }
}

void SMAABlendingWeightCalculationOperation::deinit_execution()
{
  image_reader_ = nullptr;
}

bool SMAABlendingWeightCalculationOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = input->xmax + fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  new_input.xmin = input->xmin -
                   fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  new_input.ymax = input->ymax + fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG);
  new_input.ymin = input->ymin -
                   fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG);

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void SMAABlendingWeightCalculationOperation::get_area_of_interest(const int /*input_idx*/,
                                                                  const rcti &output_area,
                                                                  rcti &r_input_area)
{
  r_input_area.xmax = output_area.xmax +
                      fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  r_input_area.xmin = output_area.xmin -
                      fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  r_input_area.ymax = output_area.ymax + fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG);
  r_input_area.ymin = output_area.ymin -
                      fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG);
}

/*-----------------------------------------------------------------------------*/
/* Diagonal Search Functions */

int SMAABlendingWeightCalculationOperation::search_diag1(int x, int y, int dir, bool *found)
{
  float e[4];
  int end = x + SMAA_MAX_SEARCH_STEPS_DIAG * dir;
  *found = false;

  while (x != end) {
    x += dir;
    y -= dir;
    sample_image_fn_(x, y, e);
    if (e[1] == 0.0f) {
      *found = true;
      break;
    }
    if (e[0] == 0.0f) {
      *found = true;
      return (dir < 0) ? x : x - dir;
    }
  }

  return x - dir;
}

int SMAABlendingWeightCalculationOperation::search_diag2(int x, int y, int dir, bool *found)
{
  float e[4];
  int end = x + SMAA_MAX_SEARCH_STEPS_DIAG * dir;
  *found = false;

  while (x != end) {
    x += dir;
    y += dir;
    sample_image_fn_(x, y, e);
    if (e[1] == 0.0f) {
      *found = true;
      break;
    }
    sample_image_fn_(x + 1, y, e);
    if (e[0] == 0.0f) {
      *found = true;
      return (dir > 0) ? x : x - dir;
    }
  }

  return x - dir;
}

void SMAABlendingWeightCalculationOperation::calculate_diag_weights(int x,
                                                                    int y,
                                                                    const float edges[2],
                                                                    float weights[2])
{
  int d1, d2;
  bool d1_found, d2_found;
  float e[4], c[4];

  zero_v2(weights);

  if (SMAA_MAX_SEARCH_STEPS_DIAG <= 0) {
    return;
  }

  /* Search for the line ends: */
  if (edges[0] > 0.0f) {
    d1 = x - search_diag1(x, y, -1, &d1_found);
  }
  else {
    d1 = 0;
    d1_found = true;
  }
  d2 = search_diag1(x, y, 1, &d2_found) - x;

  if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
    int e1 = 0, e2 = 0;

    if (d1_found) {
      /* Fetch the crossing edges: */
      int left = x - d1, bottom = y + d1;

      sample_image_fn_(left - 1, bottom, c);
      if (c[1] > 0.0) {
        e1 += 2;
      }
      sample_image_fn_(left, bottom, c);
      if (c[0] > 0.0) {
        e1 += 1;
      }
    }

    if (d2_found) {
      /* Fetch the crossing edges: */
      int right = x + d2, top = y - d2;

      sample_image_fn_(right + 1, top, c);
      if (c[1] > 0.0) {
        e2 += 2;
      }
      sample_image_fn_(right + 1, top - 1, c);
      if (c[0] > 0.0) {
        e2 += 1;
      }
    }

    /* Fetch the areas for this line: */
    area_diag(d1, d2, e1, e2, weights);
  }

  /* Search for the line ends: */
  d1 = x - search_diag2(x, y, -1, &d1_found);
  sample_image_fn_(x + 1, y, e);
  if (e[0] > 0.0f) {
    d2 = search_diag2(x, y, 1, &d2_found) - x;
  }
  else {
    d2 = 0;
    d2_found = true;
  }

  if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
    int e1 = 0, e2 = 0;

    if (d1_found) {
      /* Fetch the crossing edges: */
      int left = x - d1, top = y - d1;

      sample_image_fn_(left - 1, top, c);
      if (c[1] > 0.0) {
        e1 += 2;
      }
      sample_image_fn_(left, top - 1, c);
      if (c[0] > 0.0) {
        e1 += 1;
      }
    }

    if (d2_found) {
      /* Fetch the crossing edges: */
      int right = x + d2, bottom = y + d2;

      sample_image_fn_(right + 1, bottom, c);
      if (c[1] > 0.0) {
        e2 += 2;
      }
      if (c[0] > 0.0) {
        e2 += 1;
      }
    }

    /* Fetch the areas for this line: */
    float w[2];
    area_diag(d1, d2, e1, e2, w);
    weights[0] += w[1];
    weights[1] += w[0];
  }
}

bool SMAABlendingWeightCalculationOperation::is_vertical_search_unneeded(int x, int y)
{
  int d1, d2;
  bool found;
  float e[4];

  if (SMAA_MAX_SEARCH_STEPS_DIAG <= 0) {
    return false;
  }

  /* Search for the line ends: */
  sample_image_fn_(x - 1, y, e);
  if (e[1] > 0.0f) {
    d1 = x - search_diag2(x - 1, y, -1, &found);
  }
  else {
    d1 = 0;
  }
  d2 = search_diag2(x - 1, y, 1, &found) - x;

  return (d1 + d2 > 2); /* d1 + d2 + 1 > 3 */
}

/*-----------------------------------------------------------------------------*/
/* Horizontal/Vertical Search Functions */

int SMAABlendingWeightCalculationOperation::search_xleft(int x, int y)
{
  int end = x - SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (x > end) {
    sample_image_fn_(x, y, e);
    if (e[1] == 0.0f) { /* Is the edge not activated? */
      break;
    }
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return x;
    }
    sample_image_fn_(x, y - 1, e);
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return x;
    }
    x--;
  }

  return x + 1;
}

int SMAABlendingWeightCalculationOperation::search_xright(int x, int y)
{
  int end = x + SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (x < end) {
    x++;
    sample_image_fn_(x, y, e);
    if (e[1] == 0.0f || /* Is the edge not activated? */
        e[0] != 0.0f)   /* Or is there a crossing edge that breaks the line? */
    {
      break;
    }
    sample_image_fn_(x, y - 1, e);
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
  }

  return x - 1;
}

int SMAABlendingWeightCalculationOperation::search_yup(int x, int y)
{
  int end = y - SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (y > end) {
    sample_image_fn_(x, y, e);
    if (e[0] == 0.0f) { /* Is the edge not activated? */
      break;
    }
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return y;
    }
    sample_image_fn_(x - 1, y, e);
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return y;
    }
    y--;
  }

  return y + 1;
}

int SMAABlendingWeightCalculationOperation::search_ydown(int x, int y)
{
  int end = y + SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (y < end) {
    y++;
    sample_image_fn_(x, y, e);
    if (e[0] == 0.0f || /* Is the edge not activated? */
        e[1] != 0.0f)   /* Or is there a crossing edge that breaks the line? */
    {
      break;
    }
    sample_image_fn_(x - 1, y, e);
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
  }

  return y - 1;
}

/*-----------------------------------------------------------------------------*/
/* Corner Detection Functions */

void SMAABlendingWeightCalculationOperation::detect_horizontal_corner_pattern(
    float weights[2], int left, int right, int y, int d1, int d2)
{
  float factor[2] = {1.0f, 1.0f};
  float rounding = corner_rounding_ / 100.0f;
  float e[4];

  /* Reduce blending for pixels in the center of a line. */
  rounding *= (d1 == d2) ? 0.5f : 1.0f;

  /* Near the left corner */
  if (d1 <= d2) {
    sample_image_fn_(left, y + 1, e);
    factor[0] -= rounding * e[0];
    sample_image_fn_(left, y - 2, e);
    factor[1] -= rounding * e[0];
  }
  /* Near the right corner */
  if (d1 >= d2) {
    sample_image_fn_(right + 1, y + 1, e);
    factor[0] -= rounding * e[0];
    sample_image_fn_(right + 1, y - 2, e);
    factor[1] -= rounding * e[0];
  }

  weights[0] *= CLAMPIS(factor[0], 0.0f, 1.0f);
  weights[1] *= CLAMPIS(factor[1], 0.0f, 1.0f);
}

void SMAABlendingWeightCalculationOperation::detect_vertical_corner_pattern(
    float weights[2], int x, int top, int bottom, int d1, int d2)
{
  float factor[2] = {1.0f, 1.0f};
  float rounding = corner_rounding_ / 100.0f;
  float e[4];

  /* Reduce blending for pixels in the center of a line. */
  rounding *= (d1 == d2) ? 0.5f : 1.0f;

  /* Near the top corner */
  if (d1 <= d2) {
    sample_image_fn_(x + 1, top, e);
    factor[0] -= rounding * e[1];
    sample_image_fn_(x - 2, top, e);
    factor[1] -= rounding * e[1];
  }
  /* Near the bottom corner */
  if (d1 >= d2) {
    sample_image_fn_(x + 1, bottom + 1, e);
    factor[0] -= rounding * e[1];
    sample_image_fn_(x - 2, bottom + 1, e);
    factor[1] -= rounding * e[1];
  }

  weights[0] *= CLAMPIS(factor[0], 0.0f, 1.0f);
  weights[1] *= CLAMPIS(factor[1], 0.0f, 1.0f);
}

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending (Third Pass) */
/*-----------------------------------------------------------------------------*/

SMAANeighborhoodBlendingOperation::SMAANeighborhoodBlendingOperation()
{
  this->add_input_socket(DataType::Color); /* image */
  this->add_input_socket(DataType::Color); /* blend */
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  image1Reader_ = nullptr;
  image2Reader_ = nullptr;
}

void *SMAANeighborhoodBlendingOperation::initialize_tile_data(rcti *rect)
{
  return get_input_operation(0)->initialize_tile_data(rect);
}

void SMAANeighborhoodBlendingOperation::init_execution()
{
  image1Reader_ = this->get_input_socket_reader(0);
  image2Reader_ = this->get_input_socket_reader(1);
}

void SMAANeighborhoodBlendingOperation::execute_pixel(float output[4],
                                                      int x,
                                                      int y,
                                                      void * /*data*/)
{
  float w[4];

  /* Fetch the blending weights for current pixel: */
  sample(image2Reader_, x, y, w);
  float left = w[2], top = w[0];
  sample(image2Reader_, x + 1, y, w);
  float right = w[3];
  sample(image2Reader_, x, y + 1, w);
  float bottom = w[1];

  /* Is there any blending weight with a value greater than 0.0? */
  if (right + bottom + left + top < 1e-5f) {
    sample(image1Reader_, x, y, output);
    return;
  }

  /* Calculate the blending offsets: */
  void (*samplefunc)(SocketReader * reader, int x, int y, float xoffset, float color[4]);
  float offset1, offset2, weight1, weight2, color1[4], color2[4];

  if (fmaxf(right, left) > fmaxf(bottom, top)) { /* max(horizontal) > max(vertical) */
    samplefunc = sample_bilinear_horizontal;
    offset1 = right;
    offset2 = -left;
    weight1 = right / (right + left);
    weight2 = left / (right + left);
  }
  else {
    samplefunc = sample_bilinear_vertical;
    offset1 = bottom;
    offset2 = -top;
    weight1 = bottom / (bottom + top);
    weight2 = top / (bottom + top);
  }

  /* We exploit bilinear filtering to mix current pixel with the chosen neighbor: */
  samplefunc(image1Reader_, x, y, offset1, color1);
  samplefunc(image1Reader_, x, y, offset2, color2);

  mul_v4_v4fl(output, color1, weight1);
  madd_v4_v4fl(output, color2, weight2);
}

void SMAANeighborhoodBlendingOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                     const rcti &out_area,
                                                                     Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image1 = inputs[0];
  MemoryBuffer *image2 = inputs[1];
  for (BuffersIterator<float> it = output->iterate_with({}, out_area); !it.is_end(); ++it) {
    const float x = it.x;
    const float y = it.y;
    float w[4];

    /* Fetch the blending weights for current pixel: */
    image2->read_elem_checked(x, y, w);
    const float left = w[2], top = w[0];
    image2->read_elem_checked(x + 1, y, w);
    const float right = w[3];
    image2->read_elem_checked(x, y + 1, w);
    const float bottom = w[1];

    /* Is there any blending weight with a value greater than 0.0? */
    if (right + bottom + left + top < 1e-5f) {
      image1->read_elem_checked(x, y, it.out);
      continue;
    }

    /* Calculate the blending offsets: */
    void (*sample_fn)(MemoryBuffer * reader, int x, int y, float xoffset, float color[4]);
    float offset1, offset2, weight1, weight2, color1[4], color2[4];

    if (fmaxf(right, left) > fmaxf(bottom, top)) { /* `max(horizontal) > max(vertical)` */
      sample_fn = sample_bilinear_horizontal;
      offset1 = right;
      offset2 = -left;
      weight1 = right / (right + left);
      weight2 = left / (right + left);
    }
    else {
      sample_fn = sample_bilinear_vertical;
      offset1 = bottom;
      offset2 = -top;
      weight1 = bottom / (bottom + top);
      weight2 = top / (bottom + top);
    }

    /* We exploit bilinear filtering to mix current pixel with the chosen neighbor: */
    sample_fn(image1, x, y, offset1, color1);
    sample_fn(image1, x, y, offset2, color2);

    mul_v4_v4fl(it.out, color1, weight1);
    madd_v4_v4fl(it.out, color2, weight2);
  }
}

void SMAANeighborhoodBlendingOperation::deinit_execution()
{
  image1Reader_ = nullptr;
  image2Reader_ = nullptr;
}

bool SMAANeighborhoodBlendingOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = input->xmax + 1;
  new_input.xmin = input->xmin - 1;
  new_input.ymax = input->ymax + 1;
  new_input.ymin = input->ymin - 1;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void SMAANeighborhoodBlendingOperation::get_area_of_interest(const int /*input_idx*/,
                                                             const rcti &output_area,
                                                             rcti &r_input_area)
{
  r_input_area = output_area;
  expand_area_for_sampler(r_input_area, PixelSampler::Bilinear);
}

}  // namespace blender::compositor
