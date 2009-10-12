/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RENDER_INTERN_H
#define RENDER_INTERN_H

struct wmOperatorType;

/* render_shading.c */
void OBJECT_OT_material_slot_add(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_remove(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_assign(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_select(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_deselect(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_copy(struct wmOperatorType *ot);

void MATERIAL_OT_new(struct wmOperatorType *ot);
void TEXTURE_OT_new(struct wmOperatorType *ot);
void WORLD_OT_new(struct wmOperatorType *ot);

void SCENE_OT_render_layer_add(struct wmOperatorType *ot);
void SCENE_OT_render_layer_remove(struct wmOperatorType *ot);

void TEXTURE_OT_slot_move(struct wmOperatorType *ot);


#endif /* RENDER_INTERN_H */

