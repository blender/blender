/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/GPU_vertex_array_id.h
 *  \ingroup gpu
 *
 * Manage GL vertex array IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from a thread that is bound
 *   to the context that will be used for drawing with
 *   this vao.
 * - free can be called from any thread
 */

#ifndef __GPU_VERTEX_ARRAY_ID_H__
#define __GPU_VERTEX_ARRAY_ID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "GPU_common.h"
#include "GPU_context.h"

GLuint GPU_vao_default(void);
GLuint GPU_vao_alloc(void);
void GPU_vao_free(GLuint vao_id, GPUContext*);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_VERTEX_ARRAY_ID_H__ */
