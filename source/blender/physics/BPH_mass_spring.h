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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BPH_MASS_SPRING_H__
#define __BPH_MASS_SPRING_H__

int implicit_init (struct Object *ob, struct ClothModifierData *clmd );
int implicit_free (struct ClothModifierData *clmd );
int implicit_solver (struct Object *ob, float frame, struct ClothModifierData *clmd, struct ListBase *effectors );
void implicit_set_positions (struct ClothModifierData *clmd );

bool implicit_hair_volume_get_texture_data(struct Object *UNUSED(ob), struct ClothModifierData *clmd, struct ListBase *UNUSED(effectors), struct VoxelData *vd);

#endif
