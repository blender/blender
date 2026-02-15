/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

namespace blender {

struct Image;

/* Create python module _cycles used by addon. */
void *CCL_python_module_init(void);

void CCL_log_init(void);

/* Texture cache generation. */

bool CCL_has_texture_cache(const Image *image,
                           const char *filepath,
                           const char *texture_cache_directory = "");

bool CCL_generate_texture_cache(const Image *image,
                                const char *filepath,
                                const char *texture_cache_directory = "");

}  // namespace blender

#ifdef __cplusplus
}
#endif
