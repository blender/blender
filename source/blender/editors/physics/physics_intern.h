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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/physics_intern.h
 *  \ingroup edphys
 */


#ifndef __PHYSICS_INTERN_H__
#define __PHYSICS_INTERN_H__

struct wmOperatorType;

/* physics_fluid.c */
void FLUID_OT_bake(struct wmOperatorType *ot);

/* dynamicpaint.c */
void DPAINT_OT_bake(struct wmOperatorType *ot);
void DPAINT_OT_surface_slot_add(struct wmOperatorType *ot);
void DPAINT_OT_surface_slot_remove(struct wmOperatorType *ot);
void DPAINT_OT_type_toggle(struct wmOperatorType *ot);
void DPAINT_OT_output_toggle(struct wmOperatorType *ot);

/* rigidbody_object.c */
void RIGIDBODY_OT_object_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_object_remove(struct wmOperatorType *ot);

void RIGIDBODY_OT_objects_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_objects_remove(struct wmOperatorType *ot);

void RIGIDBODY_OT_shape_change(struct wmOperatorType *ot);
void RIGIDBODY_OT_mass_calculate(struct wmOperatorType *ot);

/* rigidbody_constraint.c */
void RIGIDBODY_OT_constraint_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_constraint_remove(struct wmOperatorType *ot);

/*rigidbody_world.c */
void RIGIDBODY_OT_world_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_world_remove(struct wmOperatorType *ot);
void RIGIDBODY_OT_world_export(struct wmOperatorType *ot);

#endif /* __PHYSICS_INTERN_H__ */
