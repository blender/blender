/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup openimageio
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

bool imb_is_a_photoshop(const unsigned char *mem, size_t size);

int imb_save_photoshop(struct ImBuf *ibuf, const char *name, int flags);

struct ImBuf *imb_load_photoshop(const char *name, int flags, char *colorspace);

int OIIO_getVersionHex(void);

#ifdef __cplusplus
}

#endif
