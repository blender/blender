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

/** \file blender/gpu/gwn_context.h
 *  \ingroup gpu
 *
 * This interface allow Gawain to manage VAOs for mutiple context and threads.
 */

#ifndef __GWN_CONTEXT_H__
#define __GWN_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "gwn_common.h"
#include "gwn_batch.h"
#include "gwn_shader_interface.h"

typedef struct Gwn_Context Gwn_Context;

Gwn_Context* GWN_context_create(void);
void GWN_context_discard(Gwn_Context*);

void GWN_context_active_set(Gwn_Context*);
Gwn_Context* GWN_context_active_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __GWN_CONTEXT_H__ */
