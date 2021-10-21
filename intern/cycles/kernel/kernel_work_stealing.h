/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

/*
 * Utility functions for work stealing
 */

/* Map global work index to tile, pixel X/Y and sample. */
ccl_device_inline void get_work_pixel(ccl_global const KernelWorkTile *tile,
                                      uint global_work_index,
                                      ccl_private uint *x,
                                      ccl_private uint *y,
                                      ccl_private uint *sample)
{
#if 0
  /* Keep threads for the same sample together. */
  uint tile_pixels = tile->w * tile->h;
  uint sample_offset = global_work_index / tile_pixels;
  uint pixel_offset = global_work_index - sample_offset * tile_pixels;
#else
  /* Keeping threads for the same pixel together.
   * Appears to improve performance by a few % on CUDA and OptiX. */
  uint sample_offset = global_work_index % tile->num_samples;
  uint pixel_offset = global_work_index / tile->num_samples;
#endif

  uint y_offset = pixel_offset / tile->w;
  uint x_offset = pixel_offset - y_offset * tile->w;

  *x = tile->x + x_offset;
  *y = tile->y + y_offset;
  *sample = tile->start_sample + sample_offset;
}

CCL_NAMESPACE_END
