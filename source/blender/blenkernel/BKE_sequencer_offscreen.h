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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct GPUOffScreen;

enum eDrawType;
enum eImBufFlags;

typedef struct ImBuf *(*SequencerDrawView)(struct Depsgraph *depsgraph,
                                           struct Scene *scene,
                                           struct View3DShading *shading_override,
                                           enum eDrawType drawtype,
                                           struct Object *camera,
                                           int width,
                                           int height,
                                           enum eImBufFlags flag,
                                           eV3DOffscreenDrawFlag draw_flags,
                                           int alpha_mode,
                                           const char *viewname,
                                           struct GPUOffScreen *ofs,
                                           char err_out[256]);
extern SequencerDrawView sequencer_view3d_fn;

#ifdef __cplusplus
}
#endif
