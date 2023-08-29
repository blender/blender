/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __COLORSPACE_H__
#define __COLORSPACE_H__

#include "util/map.h"
#include "util/param.h"

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
                                         const char *file_colorspace,
                                         const char *file_format,
                                         bool is_float);

  /* Test if colorspace is for non-color data. */
  static bool colorspace_is_data(ustring colorspace);

  /* Convert pixels in the specified colorspace to scene linear color for
   * rendering. Must be a colorspace returned from detect_known_colorspace. */
  template<typename T>
  static void to_scene_linear(
      ustring colorspace, T *pixels, size_t num_pixels, bool is_rgba, bool compress_as_srgb);

  /* Efficiently convert pixels to scene linear colorspace at render time,
   * for OSL where the image texture cache contains original pixels. The
   * handle is valid for the lifetime of the application. */
  static ColorSpaceProcessor *get_processor(ustring colorspace);
  static void to_scene_linear(ColorSpaceProcessor *processor, float *pixel, int channels);

  /* Clear memory when the application exits. Invalidates all processors. */
  static void free_memory();

  /* Create a fallback color space configuration.
   *
   * This may be useful to allow regression test to create a configuration which is considered
   * valid without knowing the actual configuration used by the final application. */
  static void init_fallback_config();

 private:
  static void is_builtin_colorspace(ustring colorspace, bool &is_no_op, bool &is_srgb);
};

CCL_NAMESPACE_END

#endif /* __COLORSPACE_H__ */
