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

/** \file blender/gpu/GPU_context.h
 *  \ingroup gpu
 *
 * This interface allow GPU to manage VAOs for mutiple context and threads.
 */

#ifndef __GPU_CONTEXT_H__
#define __GPU_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "GPU_common.h"
#include "GPU_batch.h"
#include "GPU_shader_interface.h"

typedef struct GPUContext GPUContext;

GPUContext *GPU_context_create(void);
void GPU_context_discard(GPUContext *);

void GPU_context_active_set(GPUContext *);
GPUContext *GPU_context_active_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_CONTEXT_H__ */
