/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup openexr
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void imb_initopenexr(void);
void imb_exitopenexr(void);

/**
 * Test presence of OpenEXR file.
 * \param mem: pointer to loaded OpenEXR bit-stream.
 */
bool imb_is_a_openexr(const unsigned char *mem, size_t size);

bool imb_save_openexr(struct ImBuf *ibuf, const char *filepath, int flags);

struct ImBuf *imb_load_openexr(const unsigned char *mem, size_t size, int flags, char *colorspace);

struct ImBuf *imb_load_filepath_thumbnail_openexr(const char *filepath,
                                                  int flags,
                                                  size_t max_thumb_size,
                                                  char colorspace[],
                                                  size_t *r_width,
                                                  size_t *r_height);

#ifdef __cplusplus
}
#endif
