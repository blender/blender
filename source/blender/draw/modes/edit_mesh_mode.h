/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/modes/edit_mesh_mode.h
 *  \ingroup draw
 */

#ifndef __EDIT_MESH_MODE_H__
#define __EDIT_MESH_MODE_H__

struct Object;

void EDIT_MESH_init(void);

void EDIT_MESH_cache_init(void);
void EDIT_MESH_cache_populate(struct Object *ob);
void EDIT_MESH_cache_finish(void);

void EDIT_MESH_draw(void);

void EDIT_MESH_engine_free(void);

#endif /* __EDIT_MESH_MODE_H__ */