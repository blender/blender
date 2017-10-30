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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/draw/modes/edit_mesh_mode_intern.h
 *  \ingroup draw
 */

#ifndef __EDIT_MESH_MODE_INTERN_H__
#define __EDIT_MESH_MODE_INTERN_H__

struct ARegion;
struct Object;
struct UnitSettings;
struct View3D;

/* edit_mesh_mode_text.c */
void DRW_edit_mesh_mode_text_measure_stats(
        struct ARegion *ar, struct View3D *v3d,
        struct Object *ob, const UnitSettings *unit);

#endif /* __EDIT_MESH_MODE_INTERN_H__ */
