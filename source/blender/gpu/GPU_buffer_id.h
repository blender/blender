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

/** \file blender/gpu/GPU_buffer_id.h
 *  \ingroup gpu
 *
 * GPU buffer IDs
 */

#ifndef __GPU_BUFFER_ID_H__
#define __GPU_BUFFER_ID_H__

/* Manage GL buffer IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from main thread
 * - free can be called from any thread */

#ifdef __cplusplus
extern "C" {
#endif

#include "GPU_common.h"

GLuint GPU_buf_id_alloc(void);
void GPU_buf_id_free(GLuint buffer_id);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_BUFFER_ID_H__ */
