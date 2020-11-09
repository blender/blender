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

#include "COM_AntiAliasOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "RE_texture.h"

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
  if ((!PEQ(B, H)) && (!PEQ(D, F))) {
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
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_VALUE);
  this->m_valueReader = nullptr;
  this->setComplex(true);
}

void AntiAliasOperation::initExecution()
{
  this->m_valueReader = this->getInputSocketReader(0);
}

void AntiAliasOperation::executePixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const int buffer_width = input_buffer->getWidth(), buffer_height = input_buffer->getHeight();
  if (y < 0 || y >= buffer_height || x < 0 || x >= buffer_width) {
    output[0] = 0.0f;
  }
  else {
    const float *buffer = input_buffer->getBuffer();
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
                     &row_next[x + 1])) {
      /* Some rounding magic to so make weighting correct with the
       * original coefficients.
       */
      unsigned char result = ((3 * ninepix[0] + 5 * ninepix[1] + 3 * ninepix[2] + 5 * ninepix[3] +
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

void AntiAliasOperation::deinitExecution()
{
  this->m_valueReader = nullptr;
}

bool AntiAliasOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti imageInput;
  NodeOperation *operation = getInputOperation(0);
  imageInput.xmax = input->xmax + 1;
  imageInput.xmin = input->xmin - 1;
  imageInput.ymax = input->ymax + 1;
  imageInput.ymin = input->ymin - 1;
  return operation->determineDependingAreaOfInterest(&imageInput, readOperation, output);
}

void *AntiAliasOperation::initializeTileData(rcti *rect)
{
  return getInputOperation(0)->initializeTileData(rect);
}
