/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUViewport;

/**
 * Draw texture to frame-buffer without any color transforms.
 */
void DRW_transform_none(struct GPUTexture *tex);
void DRW_viewport_colormanagement_set(struct GPUViewport *viewport);

#ifdef __cplusplus
}
#endif
