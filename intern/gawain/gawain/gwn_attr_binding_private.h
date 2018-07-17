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

/** \file blender/gpu/gwn_attr_binding_private.h
 *  \ingroup gpu
 *
 * Gawain vertex attribute binding
 */

#ifndef __GWN_ATTR_BINDING_PRIVATE_H__
#define __GWN_ATTR_BINDING_PRIVATE_H__

#include "gwn_vertex_format.h"
#include "gwn_shader_interface.h"

void AttribBinding_clear(Gwn_AttrBinding*);

void get_attrib_locations(const Gwn_VertFormat*, Gwn_AttrBinding*, const Gwn_ShaderInterface*);
unsigned read_attrib_location(const Gwn_AttrBinding*, unsigned a_idx);

#endif /* __GWN_ATTR_BINDING_PRIVATE_H__ */
