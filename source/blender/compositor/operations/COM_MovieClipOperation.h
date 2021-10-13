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

#include "BLI_listbase.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_movieclip_types.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

/**
 * Base class for movie clip
 */
class MovieClipBaseOperation : public MultiThreadedOperation {
 protected:
  MovieClip *movie_clip_;
  MovieClipUser *movie_clip_user_;
  ImBuf *movie_clip_buffer_;
  int movie_clipheight_;
  int movie_clipwidth_;
  int framenumber_;
  bool cache_frame_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 public:
  MovieClipBaseOperation();

  void init_execution() override;
  void deinit_execution() override;
  void set_movie_clip(MovieClip *image)
  {
    movie_clip_ = image;
  }
  void set_movie_clip_user(MovieClipUser *imageuser)
  {
    movie_clip_user_ = imageuser;
  }
  void set_cache_frame(bool value)
  {
    cache_frame_ = value;
  }

  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class MovieClipOperation : public MovieClipBaseOperation {
 public:
  MovieClipOperation();
};

class MovieClipAlphaOperation : public MovieClipBaseOperation {
 public:
  MovieClipAlphaOperation();
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
