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

  MovieClip *movie_clip_;
  int framenumber_;
  TriangulationData *cached_triangulation_;
  char tracking_object_[64];

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  TriangulationData *build_voronoi_triangulation();

 public:
  KeyingScreenOperation();

  void init_execution() override;
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  void deinitialize_tile_data(rcti *rect, void *data) override;

  void set_movie_clip(MovieClip *clip)
  {
    movie_clip_ = clip;
  }
  void set_tracking_object(const char *object)
  {
    BLI_strncpy(tracking_object_, object, sizeof(tracking_object_));
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }

  void execute_pixel(float output[4], int x, int y, void *data) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  TileData *triangulate(const rcti *rect);
};

}  // namespace blender::compositor
