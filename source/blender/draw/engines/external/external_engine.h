/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern DrawEngineType draw_engine_external_type;
extern RenderEngineType DRW_engine_viewport_external_type;

/* Check whether an external engine is to be used to draw content of an image editor.
 * If the drawing is possible, the render engine is "acquired" so that it is not freed by the
 * render engine for until drawing is finished.
 *
 * NOTE: Released by the draw engine when it is done drawing. */
bool DRW_engine_external_acquire_for_image_editor(void);

#ifdef __cplusplus
}
#endif
