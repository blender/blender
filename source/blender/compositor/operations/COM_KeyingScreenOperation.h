/* SPDX-FileCopyrightText: 2012 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
