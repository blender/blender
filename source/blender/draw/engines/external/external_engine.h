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
 * Copyright 2017, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

extern DrawEngineType draw_engine_external_type;
extern RenderEngineType DRW_engine_viewport_external_type;

/* Check whether an external engine is to be used to draw content of an image editor.
 * If the drawing is possible, the render engine is "acquired" so that it is not freed by the
 * render engine for until drawing is finished.
 *
 * NOTE: Released by the draw engine when it is done drawing. */
bool DRW_engine_external_acquire_for_image_editor(void);
