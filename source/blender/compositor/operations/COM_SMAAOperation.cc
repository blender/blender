/*
 * Copyright 2017, Blender Foundation.
 *
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
 * Contributor: IRIE Shinsuke
 */

#include "COM_SMAAOperation.h"
#include "BLI_math.h"
#include "COM_SMAAAreaTexture.h"

extern "C" {
#include "IMB_colormanagement.h"
}

namespace blender::compositor {

/*
 * An implementation of Enhanced Subpixel Morphological Antialiasing (SMAA)
 *
 * The algorithm was proposed by:
 *   Jorge Jimenez, Jose I. Echevarria, Tiago Sousa, Diego Gutierrez
 *
 *   http://www.iryoku.com/smaa/
 *
 * This file is based on smaa-cpp:
 *
 *   https://github.com/iRi-E/smaa-cpp
 *
 * Currently only SMAA 1x mode is provided, so the operation will be done
 * with no spatial multisampling nor temporal supersampling.
 *
 * Note: This program assumes the screen coordinates are DirectX style, so
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

static inline void sample(SocketReader *reader, int x, int y, float color[4])
{
  if (x < 0 || x >= reader->getWidth() || y < 0 || y >= reader->getHeight()) {
    color[0] = color[1] = color[2] = color[3] = 0.0;
    return;
  }

  reader->read(color, x, y, nullptr);
}

static void sample_bilinear_vertical(
    SocketReader *reader, int x, int y, float yoffset, float color[4])
{
  float iy = floorf(yoffset);
  float fy = yoffset - iy;
  y += (int)iy;

  float color00[4], color01[4];

  sample(reader, x + 0, y + 0, color00);
  sample(reader, x + 0, y + 1, color01);

  color[0] = interpf(color01[0], color00[0], fy);
  color[1] = interpf(color01[1], color00[1], fy);
  color[2] = interpf(color01[2], color00[2], fy);
  color[3] = interpf(color01[3], color00[3], fy);
}

static void sample_bilinear_horizontal(
    SocketReader *reader, int x, int y, float xoffset, float color[4])
{
  float ix = floorf(xoffset);
  float fx = xoffset - ix;
  x += (int)ix;

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
  float x = (float)(SMAA_AREATEX_MAX_DISTANCE * e1) + sqrtf((float)d1);
  float y = (float)(SMAA_AREATEX_MAX_DISTANCE * e2) + sqrtf((float)d2);

  float ix = floorf(x), iy = floorf(y);
  float fx = x - ix, fy = y - iy;
  int X = (int)ix, Y = (int)iy;

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
  this->addInputSocket(DataType::Color); /* image */
  this->addInputSocket(DataType::Value); /* depth, material ID, etc. */
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;
  this->m_imageReader = nullptr;
  this->m_valueReader = nullptr;
  this->m_threshold = 0.1f;
  this->m_contrast_limit = 2.0f;
}

void SMAAEdgeDetectionOperation::initExecution()
{
  this->m_imageReader = this->getInputSocketReader(0);
  this->m_valueReader = this->getInputSocketReader(1);
}

void SMAAEdgeDetectionOperation::deinitExecution()
{
  this->m_imageReader = nullptr;
  this->m_valueReader = nullptr;
}

void SMAAEdgeDetectionOperation::setThreshold(float threshold)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 0 and 0.5 */
  m_threshold = scalenorm(0, 0.5, threshold);
}

void SMAAEdgeDetectionOperation::setLocalContrastAdaptationFactor(float factor)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 1 and 10 */
  m_contrast_limit = scalenorm(1, 10, factor);
}

bool SMAAEdgeDetectionOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
  newInput.xmax = input->xmax + 1;
  newInput.xmin = input->xmin - 2;
  newInput.ymax = input->ymax + 1;
  newInput.ymin = input->ymin - 2;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void SMAAEdgeDetectionOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  float color[4];

  /* Calculate luma deltas: */
  sample(m_imageReader, x, y, color);
  float L = IMB_colormanagement_get_luminance(color);
  sample(m_imageReader, x - 1, y, color);
  float Lleft = IMB_colormanagement_get_luminance(color);
  sample(m_imageReader, x, y - 1, color);
  float Ltop = IMB_colormanagement_get_luminance(color);
  float Dleft = fabsf(L - Lleft);
  float Dtop = fabsf(L - Ltop);

  /* We do the usual threshold: */
  output[0] = (x > 0 && Dleft >= m_threshold) ? 1.0f : 0.0f;
  output[1] = (y > 0 && Dtop >= m_threshold) ? 1.0f : 0.0f;
  output[2] = 0.0f;
  output[3] = 1.0f;

  /* Then discard if there is no edge: */
  if (is_zero_v2(output)) {
    return;
  }

  /* Calculate right and bottom deltas: */
  sample(m_imageReader, x + 1, y, color);
  float Lright = IMB_colormanagement_get_luminance(color);
  sample(m_imageReader, x, y + 1, color);
  float Lbottom = IMB_colormanagement_get_luminance(color);
  float Dright = fabsf(L - Lright);
  float Dbottom = fabsf(L - Lbottom);

  /* Calculate the maximum delta in the direct neighborhood: */
  float maxDelta = fmaxf(fmaxf(Dleft, Dright), fmaxf(Dtop, Dbottom));

  /* Calculate luma used for both left and top edges: */
  sample(m_imageReader, x - 1, y - 1, color);
  float Llefttop = IMB_colormanagement_get_luminance(color);

  /* Left edge */
  if (output[0] != 0.0f) {
    /* Calculate deltas around the left pixel: */
    sample(m_imageReader, x - 2, y, color);
    float Lleftleft = IMB_colormanagement_get_luminance(color);
    sample(m_imageReader, x - 1, y + 1, color);
    float Lleftbottom = IMB_colormanagement_get_luminance(color);
    float Dleftleft = fabsf(Lleft - Lleftleft);
    float Dlefttop = fabsf(Lleft - Llefttop);
    float Dleftbottom = fabsf(Lleft - Lleftbottom);

    /* Calculate the final maximum delta: */
    maxDelta = fmaxf(maxDelta, fmaxf(Dleftleft, fmaxf(Dlefttop, Dleftbottom)));

    /* Local contrast adaptation: */
    if (maxDelta > m_contrast_limit * Dleft) {
      output[0] = 0.0f;
    }
  }

  /* Top edge */
  if (output[1] != 0.0f) {
    /* Calculate top-top delta: */
    sample(m_imageReader, x, y - 2, color);
    float Ltoptop = IMB_colormanagement_get_luminance(color);
    sample(m_imageReader, x + 1, y - 1, color);
    float Ltopright = IMB_colormanagement_get_luminance(color);
    float Dtoptop = fabsf(Ltop - Ltoptop);
    float Dtopleft = fabsf(Ltop - Llefttop);
    float Dtopright = fabsf(Ltop - Ltopright);

    /* Calculate the final maximum delta: */
    maxDelta = fmaxf(maxDelta, fmaxf(Dtoptop, fmaxf(Dtopleft, Dtopright)));

    /* Local contrast adaptation: */
    if (maxDelta > m_contrast_limit * Dtop) {
      output[1] = 0.0f;
    }
  }
}

/*-----------------------------------------------------------------------------*/
/* Blending Weight Calculation (Second Pass) */
/*-----------------------------------------------------------------------------*/

SMAABlendingWeightCalculationOperation::SMAABlendingWeightCalculationOperation()
{
  this->addInputSocket(DataType::Color); /* edges */
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;
  this->m_imageReader = nullptr;
  this->m_corner_rounding = 25;
}

void *SMAABlendingWeightCalculationOperation::initializeTileData(rcti *rect)
{
  return getInputOperation(0)->initializeTileData(rect);
}

void SMAABlendingWeightCalculationOperation::initExecution()
{
  this->m_imageReader = this->getInputSocketReader(0);
}

void SMAABlendingWeightCalculationOperation::setCornerRounding(float rounding)
{
  /* UI values are between 0 and 1 for simplicity but algorithm expects values between 0 and 100 */
  m_corner_rounding = static_cast<int>(scalenorm(0, 100, rounding));
}

void SMAABlendingWeightCalculationOperation::executePixel(float output[4],
                                                          int x,
                                                          int y,
                                                          void * /*data*/)
{
  float edges[4], c[4];

  zero_v4(output);
  sample(m_imageReader, x, y, edges);

  /* Edge at north */
  if (edges[1] > 0.0f) {
    /* Diagonals have both north and west edges, so calculating weights for them */
    /* in one of the boundaries is enough. */
    calculateDiagWeights(x, y, edges, output);

    /* We give priority to diagonals, so if we find a diagonal we skip  */
    /* horizontal/vertical processing. */
    if (!is_zero_v2(output)) {
      return;
    }

    /* Find the distance to the left and the right: */
    int left = searchXLeft(x, y);
    int right = searchXRight(x, y);
    int d1 = x - left, d2 = right - x;

    /* Fetch the left and right crossing edges: */
    int e1 = 0, e2 = 0;
    sample(m_imageReader, left, y - 1, c);
    if (c[0] > 0.0) {
      e1 += 1;
    }
    sample(m_imageReader, left, y, c);
    if (c[0] > 0.0) {
      e1 += 2;
    }
    sample(m_imageReader, right + 1, y - 1, c);
    if (c[0] > 0.0) {
      e2 += 1;
    }
    sample(m_imageReader, right + 1, y, c);
    if (c[0] > 0.0) {
      e2 += 2;
    }

    /* Ok, we know how this pattern looks like, now it is time for getting */
    /* the actual area: */
    area(d1, d2, e1, e2, output); /* R, G */

    /* Fix corners: */
    if (m_corner_rounding) {
      detectHorizontalCornerPattern(output, left, right, y, d1, d2);
    }
  }

  /* Edge at west */
  if (edges[0] > 0.0f) {
    /* Did we already do diagonal search for this west edge from the left neighboring pixel? */
    if (isVerticalSearchUnneeded(x, y)) {
      return;
    }

    /* Find the distance to the top and the bottom: */
    int top = searchYUp(x, y);
    int bottom = searchYDown(x, y);
    int d1 = y - top, d2 = bottom - y;

    /* Fetch the top and bottom crossing edges: */
    int e1 = 0, e2 = 0;
    sample(m_imageReader, x - 1, top, c);
    if (c[1] > 0.0) {
      e1 += 1;
    }
    sample(m_imageReader, x, top, c);
    if (c[1] > 0.0) {
      e1 += 2;
    }
    sample(m_imageReader, x - 1, bottom + 1, c);
    if (c[1] > 0.0) {
      e2 += 1;
    }
    sample(m_imageReader, x, bottom + 1, c);
    if (c[1] > 0.0) {
      e2 += 2;
    }

    /* Get the area for this direction: */
    area(d1, d2, e1, e2, output + 2); /* B, A */

    /* Fix corners: */
    if (m_corner_rounding) {
      detectVerticalCornerPattern(output + 2, x, top, bottom, d1, d2);
    }
  }
}

void SMAABlendingWeightCalculationOperation::deinitExecution()
{
  this->m_imageReader = nullptr;
}

bool SMAABlendingWeightCalculationOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;

  newInput.xmax = input->xmax + fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  newInput.xmin = input->xmin -
                  fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG + 1);
  newInput.ymax = input->ymax + fmax(SMAA_MAX_SEARCH_STEPS, SMAA_MAX_SEARCH_STEPS_DIAG);
  newInput.ymin = input->ymin -
                  fmax(fmax(SMAA_MAX_SEARCH_STEPS - 1, 1), SMAA_MAX_SEARCH_STEPS_DIAG);

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

/*-----------------------------------------------------------------------------*/
/* Diagonal Search Functions */

/**
 * These functions allows to perform diagonal pattern searches.
 */
int SMAABlendingWeightCalculationOperation::searchDiag1(int x, int y, int dir, bool *found)
{
  float e[4];
  int end = x + SMAA_MAX_SEARCH_STEPS_DIAG * dir;
  *found = false;

  while (x != end) {
    x += dir;
    y -= dir;
    sample(m_imageReader, x, y, e);
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

int SMAABlendingWeightCalculationOperation::searchDiag2(int x, int y, int dir, bool *found)
{
  float e[4];
  int end = x + SMAA_MAX_SEARCH_STEPS_DIAG * dir;
  *found = false;

  while (x != end) {
    x += dir;
    y += dir;
    sample(m_imageReader, x, y, e);
    if (e[1] == 0.0f) {
      *found = true;
      break;
    }
    sample(m_imageReader, x + 1, y, e);
    if (e[0] == 0.0f) {
      *found = true;
      return (dir > 0) ? x : x - dir;
    }
  }

  return x - dir;
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
void SMAABlendingWeightCalculationOperation::calculateDiagWeights(int x,
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
    d1 = x - searchDiag1(x, y, -1, &d1_found);
  }
  else {
    d1 = 0;
    d1_found = true;
  }
  d2 = searchDiag1(x, y, 1, &d2_found) - x;

  if (d1 + d2 > 2) { /* d1 + d2 + 1 > 3 */
    int e1 = 0, e2 = 0;

    if (d1_found) {
      /* Fetch the crossing edges: */
      int left = x - d1, bottom = y + d1;

      sample(m_imageReader, left - 1, bottom, c);
      if (c[1] > 0.0) {
        e1 += 2;
      }
      sample(m_imageReader, left, bottom, c);
      if (c[0] > 0.0) {
        e1 += 1;
      }
    }

    if (d2_found) {
      /* Fetch the crossing edges: */
      int right = x + d2, top = y - d2;

      sample(m_imageReader, right + 1, top, c);
      if (c[1] > 0.0) {
        e2 += 2;
      }
      sample(m_imageReader, right + 1, top - 1, c);
      if (c[0] > 0.0) {
        e2 += 1;
      }
    }

    /* Fetch the areas for this line: */
    area_diag(d1, d2, e1, e2, weights);
  }

  /* Search for the line ends: */
  d1 = x - searchDiag2(x, y, -1, &d1_found);
  sample(m_imageReader, x + 1, y, e);
  if (e[0] > 0.0f) {
    d2 = searchDiag2(x, y, 1, &d2_found) - x;
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

      sample(m_imageReader, left - 1, top, c);
      if (c[1] > 0.0) {
        e1 += 2;
      }
      sample(m_imageReader, left, top - 1, c);
      if (c[0] > 0.0) {
        e1 += 1;
      }
    }

    if (d2_found) {
      /* Fetch the crossing edges: */
      int right = x + d2, bottom = y + d2;

      sample(m_imageReader, right + 1, bottom, c);
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

bool SMAABlendingWeightCalculationOperation::isVerticalSearchUnneeded(int x, int y)
{
  int d1, d2;
  bool found;
  float e[4];

  if (SMAA_MAX_SEARCH_STEPS_DIAG <= 0) {
    return false;
  }

  /* Search for the line ends: */
  sample(m_imageReader, x - 1, y, e);
  if (e[1] > 0.0f) {
    d1 = x - searchDiag2(x - 1, y, -1, &found);
  }
  else {
    d1 = 0;
  }
  d2 = searchDiag2(x - 1, y, 1, &found) - x;

  return (d1 + d2 > 2); /* d1 + d2 + 1 > 3 */
}

/*-----------------------------------------------------------------------------*/
/* Horizontal/Vertical Search Functions */

int SMAABlendingWeightCalculationOperation::searchXLeft(int x, int y)
{
  int end = x - SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (x > end) {
    sample(m_imageReader, x, y, e);
    if (e[1] == 0.0f) { /* Is the edge not activated? */
      break;
    }
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return x;
    }
    sample(m_imageReader, x, y - 1, e);
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return x;
    }
    x--;
  }

  return x + 1;
}

int SMAABlendingWeightCalculationOperation::searchXRight(int x, int y)
{
  int end = x + SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (x < end) {
    x++;
    sample(m_imageReader, x, y, e);
    if (e[1] == 0.0f || /* Is the edge not activated? */
        e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
    sample(m_imageReader, x, y - 1, e);
    if (e[0] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
  }

  return x - 1;
}

int SMAABlendingWeightCalculationOperation::searchYUp(int x, int y)
{
  int end = y - SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (y > end) {
    sample(m_imageReader, x, y, e);
    if (e[0] == 0.0f) { /* Is the edge not activated? */
      break;
    }
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return y;
    }
    sample(m_imageReader, x - 1, y, e);
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      return y;
    }
    y--;
  }

  return y + 1;
}

int SMAABlendingWeightCalculationOperation::searchYDown(int x, int y)
{
  int end = y + SMAA_MAX_SEARCH_STEPS;
  float e[4];

  while (y < end) {
    y++;
    sample(m_imageReader, x, y, e);
    if (e[0] == 0.0f || /* Is the edge not activated? */
        e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
    sample(m_imageReader, x - 1, y, e);
    if (e[1] != 0.0f) { /* Or is there a crossing edge that breaks the line? */
      break;
    }
  }

  return y - 1;
}

/*-----------------------------------------------------------------------------*/
/* Corner Detection Functions */

void SMAABlendingWeightCalculationOperation::detectHorizontalCornerPattern(
    float weights[2], int left, int right, int y, int d1, int d2)
{
  float factor[2] = {1.0f, 1.0f};
  float rounding = m_corner_rounding / 100.0f;
  float e[4];

  /* Reduce blending for pixels in the center of a line. */
  rounding *= (d1 == d2) ? 0.5f : 1.0f;

  /* Near the left corner */
  if (d1 <= d2) {
    sample(m_imageReader, left, y + 1, e);
    factor[0] -= rounding * e[0];
    sample(m_imageReader, left, y - 2, e);
    factor[1] -= rounding * e[0];
  }
  /* Near the right corner */
  if (d1 >= d2) {
    sample(m_imageReader, right + 1, y + 1, e);
    factor[0] -= rounding * e[0];
    sample(m_imageReader, right + 1, y - 2, e);
    factor[1] -= rounding * e[0];
  }

  weights[0] *= CLAMPIS(factor[0], 0.0f, 1.0f);
  weights[1] *= CLAMPIS(factor[1], 0.0f, 1.0f);
}

void SMAABlendingWeightCalculationOperation::detectVerticalCornerPattern(
    float weights[2], int x, int top, int bottom, int d1, int d2)
{
  float factor[2] = {1.0f, 1.0f};
  float rounding = m_corner_rounding / 100.0f;
  float e[4];

  /* Reduce blending for pixels in the center of a line. */
  rounding *= (d1 == d2) ? 0.5f : 1.0f;

  /* Near the top corner */
  if (d1 <= d2) {
    sample(m_imageReader, x + 1, top, e);
    factor[0] -= rounding * e[1];
    sample(m_imageReader, x - 2, top, e);
    factor[1] -= rounding * e[1];
  }
  /* Near the bottom corner */
  if (d1 >= d2) {
    sample(m_imageReader, x + 1, bottom + 1, e);
    factor[0] -= rounding * e[1];
    sample(m_imageReader, x - 2, bottom + 1, e);
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
  this->addInputSocket(DataType::Color); /* image */
  this->addInputSocket(DataType::Color); /* blend */
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;
  this->m_image1Reader = nullptr;
  this->m_image2Reader = nullptr;
}

void *SMAANeighborhoodBlendingOperation::initializeTileData(rcti *rect)
{
  return getInputOperation(0)->initializeTileData(rect);
}

void SMAANeighborhoodBlendingOperation::initExecution()
{
  this->m_image1Reader = this->getInputSocketReader(0);
  this->m_image2Reader = this->getInputSocketReader(1);
}

void SMAANeighborhoodBlendingOperation::executePixel(float output[4],
                                                     int x,
                                                     int y,
                                                     void * /*data*/)
{
  float w[4];

  /* Fetch the blending weights for current pixel: */
  sample(m_image2Reader, x, y, w);
  float left = w[2], top = w[0];
  sample(m_image2Reader, x + 1, y, w);
  float right = w[3];
  sample(m_image2Reader, x, y + 1, w);
  float bottom = w[1];

  /* Is there any blending weight with a value greater than 0.0? */
  if (right + bottom + left + top < 1e-5f) {
    sample(m_image1Reader, x, y, output);
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
  samplefunc(m_image1Reader, x, y, offset1, color1);
  samplefunc(m_image1Reader, x, y, offset2, color2);

  mul_v4_v4fl(output, color1, weight1);
  madd_v4_v4fl(output, color2, weight2);
}

void SMAANeighborhoodBlendingOperation::deinitExecution()
{
  this->m_image1Reader = nullptr;
  this->m_image2Reader = nullptr;
}

bool SMAANeighborhoodBlendingOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;

  newInput.xmax = input->xmax + 1;
  newInput.xmin = input->xmin - 1;
  newInput.ymax = input->ymax + 1;
  newInput.ymin = input->ymin - 1;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

}  // namespace blender::compositor
