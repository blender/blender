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

/** \file blender/gpu/gwn_batch_private.h
 *  \ingroup gpu
 *
 * Gawain geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#ifndef __GWN_BATCH_PRIVATE_H__
#define __GWN_BATCH_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_batch.h"
#include "gwn_context.h"
#include "gwn_shader_interface.h"

void gwn_batch_remove_interface_ref(Gwn_Batch*, const Gwn_ShaderInterface*);
void gwn_batch_vao_cache_clear(Gwn_Batch*);

void gwn_context_add_batch(Gwn_Context*, Gwn_Batch*);
void gwn_context_remove_batch(Gwn_Context*, Gwn_Batch*);

#ifdef __cplusplus
}
#endif

#endif /* __GWN_BATCH_PRIVATE_H__ */
