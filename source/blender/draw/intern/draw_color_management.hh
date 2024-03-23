/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

struct GPUViewport;

/**
 * Draw texture to frame-buffer without any color transforms.
 */
void DRW_transform_none(struct GPUTexture *tex);
void DRW_viewport_colormanagement_set(struct GPUViewport *viewport);
