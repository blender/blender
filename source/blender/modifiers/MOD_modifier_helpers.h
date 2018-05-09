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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file MOD_modifier_helpers.h
 *  \ingroup modifiers
 *
 * Modifier-specific functions that should be accessible from outside the modifiers module.
 */

#ifndef __MOD_MODIFIER_HELPERS_H__
#define __MOD_MODIFIER_HELPERS_H__

struct Object;
struct SurfaceDeformModifierData;

/* Defined in MOD_surfacedeform.c */
bool MOD_surfacedeform_bind(struct Object *ob, struct SurfaceDeformModifierData *smd);


#endif  /* __MOD_MODIFIER_HELPERS_H__ */
