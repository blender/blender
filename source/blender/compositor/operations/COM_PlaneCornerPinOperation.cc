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

#include "COM_PlaneCornerPinOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

#include "BKE_node.h"

static bool check_corners(float corners[4][2])
{
  int i, next, prev;
  float cross = 0.0f;

  for (i = 0; i < 4; i++) {
    float v1[2], v2[2], cur_cross;

    next = (i + 1) % 4;
    prev = (4 + i - 1) % 4;

    sub_v2_v2v2(v1, corners[i], corners[prev]);
    sub_v2_v2v2(v2, corners[next], corners[i]);

    cur_cross = cross_v2v2(v1, v2);
    if (fabsf(cur_cross) <= FLT_EPSILON) {
      return false;
    }

    if (cross == 0.0f) {
      cross = cur_cross;
    }
    else if (cross * cur_cross < 0.0f) {
      return false;
    }
  }

  return true;
}

static void readCornersFromSockets(rcti *rect, SocketReader *readers[4], float corners[4][2])
{
  for (int i = 0; i < 4; i++) {
    float result[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    readers[i]->readSampled(result, rect->xmin, rect->ymin, COM_PS_NEAREST);
    corners[i][0] = result[0];
    corners[i][1] = result[1];
  }

  /* convexity check:
   * concave corners need to be prevented, otherwise
   * BKE_tracking_homography_between_two_quads will freeze
   */
  if (!check_corners(corners)) {
    /* simply revert to default corners
     * there could be a more elegant solution,
     * this prevents freezing at least.
     */
    corners[0][0] = 0.0f;
    corners[0][1] = 0.0f;
    corners[1][0] = 1.0f;
    corners[1][1] = 0.0f;
    corners[2][0] = 1.0f;
    corners[2][1] = 1.0f;
    corners[3][0] = 0.0f;
    corners[3][1] = 1.0f;
  }
}

/* ******** PlaneCornerPinMaskOperation ******** */

PlaneCornerPinMaskOperation::PlaneCornerPinMaskOperation() : m_corners_ready(false)
{
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);

  /* XXX this is stupid: we need to make this "complex",
   * so we can use the initializeTileData function
   * to read corners from input sockets ...
   */
  setComplex(true);
}

void PlaneCornerPinMaskOperation::initExecution()
{
  PlaneDistortMaskOperation::initExecution();

  initMutex();
}

void PlaneCornerPinMaskOperation::deinitExecution()
{
  PlaneDistortMaskOperation::deinitExecution();

  deinitMutex();
}

void *PlaneCornerPinMaskOperation::initializeTileData(rcti *rect)
{
  void *data = PlaneDistortMaskOperation::initializeTileData(rect);

  /* get corner values once, by reading inputs at (0,0)
   * XXX this assumes invariable values (no image inputs),
   * we don't have a nice generic system for that yet
   */
  lockMutex();
  if (!m_corners_ready) {
    SocketReader *readers[4] = {
        getInputSocketReader(0),
        getInputSocketReader(1),
        getInputSocketReader(2),
        getInputSocketReader(3),
    };
    float corners[4][2];
    readCornersFromSockets(rect, readers, corners);
    calculateCorners(corners, true, 0);

    m_corners_ready = true;
  }
  unlockMutex();

  return data;
}

void PlaneCornerPinMaskOperation::determineResolution(unsigned int resolution[2],
                                                      unsigned int preferredResolution[2])
{
  resolution[0] = preferredResolution[0];
  resolution[1] = preferredResolution[1];
}

/* ******** PlaneCornerPinWarpImageOperation ******** */

PlaneCornerPinWarpImageOperation::PlaneCornerPinWarpImageOperation() : m_corners_ready(false)
{
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);
  addInputSocket(COM_DT_VECTOR);
}

void PlaneCornerPinWarpImageOperation::initExecution()
{
  PlaneDistortWarpImageOperation::initExecution();

  initMutex();
}

void PlaneCornerPinWarpImageOperation::deinitExecution()
{
  PlaneDistortWarpImageOperation::deinitExecution();

  deinitMutex();
}

void *PlaneCornerPinWarpImageOperation::initializeTileData(rcti *rect)
{
  void *data = PlaneDistortWarpImageOperation::initializeTileData(rect);

  /* get corner values once, by reading inputs at (0,0)
   * XXX this assumes invariable values (no image inputs),
   * we don't have a nice generic system for that yet
   */
  lockMutex();
  if (!m_corners_ready) {
    /* corner sockets start at index 1 */
    SocketReader *readers[4] = {
        getInputSocketReader(1),
        getInputSocketReader(2),
        getInputSocketReader(3),
        getInputSocketReader(4),
    };
    float corners[4][2];
    readCornersFromSockets(rect, readers, corners);
    calculateCorners(corners, true, 0);

    m_corners_ready = true;
  }
  unlockMutex();

  return data;
}

bool PlaneCornerPinWarpImageOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  for (int i = 0; i < 4; i++) {
    if (getInputOperation(i + 1)->determineDependingAreaOfInterest(input, readOperation, output)) {
      return true;
    }
  }

  /* XXX this is bad, but unavoidable with the current design:
   * we don't know the actual corners and matrix at this point,
   * so all we can do is get the full input image
   */
  output->xmin = 0;
  output->ymin = 0;
  output->xmax = getInputOperation(0)->getWidth();
  output->ymax = getInputOperation(0)->getHeight();
  return true;
#if 0
  return PlaneDistortWarpImageOperation::determineDependingAreaOfInterest(
      input, readOperation, output);
#endif
}
