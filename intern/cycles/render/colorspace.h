/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __COLORSPACE_H__
#define __COLORSPACE_H__

#include "util/util_map.h"
#include "util/util_param.h"

CCL_NAMESPACE_BEGIN

extern ustring u_colorspace_auto;
extern ustring u_colorspace_raw;
extern ustring u_colorspace_srgb;

class ColorSpaceProcessor;

class ColorSpaceManager {
 public:
  /* Convert used specified colorspace to a colorspace that we are able to
   * convert to and from. If the colorspace is u_colorspace_auto, we auto
   * detect a colospace. */
  static ustring detect_known_colorspace(ustring colorspace,
                                         const char *file_format,
                                         bool is_float);

  /* Test if colorspace is for non-color data. */
  static bool colorspace_is_data(ustring colorspace);

  /* Convert pixels in the specified colorspace to scene linear color for
   * rendering. Must be a colorspace returned from detect_known_colorspace. */
  template<typename T>
  static void to_scene_linear(ustring colorspace,
                              T *pixels,
                              size_t width,
                              size_t height,
                              size_t depth,
                              bool compress_as_srgb);

  /* Efficiently convert pixels to scene linear colorspace at render time,
   * for OSL where the image texture cache contains original pixels. The
   * handle is valid for the lifetime of the application. */
  static ColorSpaceProcessor *get_processor(ustring colorspace);
  static void to_scene_linear(ColorSpaceProcessor *processor, float *pixel, int channels);

  /* Clear memory when the application exits. Invalidates all processors. */
  static void free_memory();

 private:
  static void is_builtin_colorspace(ustring colorspace, bool &is_no_op, bool &is_srgb);
};

CCL_NAMESPACE_END

#endif /* __COLORSPACE_H__ */
