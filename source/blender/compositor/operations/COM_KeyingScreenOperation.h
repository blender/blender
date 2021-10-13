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
 * Copyright 2012, Blender Foundation.
 */

#pragma once

#include <string.h>

#include "COM_MultiThreadedOperation.h"

#include "DNA_movieclip_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLI_voronoi_2d.h"

namespace blender::compositor {

/**
 * Class with implementation of green screen gradient rasterization
 */
class KeyingScreenOperation : public MultiThreadedOperation {
 protected:
  typedef struct TriangulationData {
    VoronoiTriangulationPoint *triangulated_points;
    int (*triangles)[3];
    int triangulated_points_total, triangles_total;
    rcti *triangles_AABB;
  } TriangulationData;

  /* TODO(manzanilla): rename to #TrianguledArea on removing tiled implementation. */
  typedef struct TileData {
    int *triangles;
    int triangles_total;
  } TileData;

  MovieClip *m_movieClip;
  int m_framenumber;
  TriangulationData *m_cachedTriangulation;
  char m_trackingObject[64];

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  TriangulationData *buildVoronoiTriangulation();

 public:
  KeyingScreenOperation();

  void initExecution() override;
  void deinitExecution() override;

  void *initializeTileData(rcti *rect) override;
  void deinitializeTileData(rcti *rect, void *data) override;

  void setMovieClip(MovieClip *clip)
  {
    m_movieClip = clip;
  }
  void setTrackingObject(const char *object)
  {
    BLI_strncpy(m_trackingObject, object, sizeof(m_trackingObject));
  }
  void setFramenumber(int framenumber)
  {
    m_framenumber = framenumber;
  }

  void executePixel(float output[4], int x, int y, void *data) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  TileData *triangulate(const rcti *rect);
};

}  // namespace blender::compositor
