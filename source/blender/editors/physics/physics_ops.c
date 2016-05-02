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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/physics_ops.c
 *  \ingroup edphys
 */

#include <stdlib.h>

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_physics.h"
#include "ED_object.h"

#include "physics_intern.h" // own include


/***************************** particles ***********************************/

static void operatortypes_rigidbody(void)
{
	WM_operatortype_append(RIGIDBODY_OT_object_add);
	WM_operatortype_append(RIGIDBODY_OT_object_remove);

	WM_operatortype_append(RIGIDBODY_OT_objects_add);
	WM_operatortype_append(RIGIDBODY_OT_objects_remove);

	WM_operatortype_append(RIGIDBODY_OT_shape_change);
	WM_operatortype_append(RIGIDBODY_OT_mass_calculate);

	WM_operatortype_append(RIGIDBODY_OT_constraint_add);
	WM_operatortype_append(RIGIDBODY_OT_constraint_remove);

	WM_operatortype_append(RIGIDBODY_OT_world_add);
	WM_operatortype_append(RIGIDBODY_OT_world_remove);
//	WM_operatortype_append(RIGIDBODY_OT_world_export);
}

/********************************* fluid ***********************************/

static void operatortypes_fluid(void)
{
	WM_operatortype_append(FLUID_OT_bake);
}

/********************************* dynamic paint ***********************************/

static void operatortypes_dynamicpaint(void)
{
	WM_operatortype_append(DPAINT_OT_bake);
	WM_operatortype_append(DPAINT_OT_surface_slot_add);
	WM_operatortype_append(DPAINT_OT_surface_slot_remove);
	WM_operatortype_append(DPAINT_OT_type_toggle);
	WM_operatortype_append(DPAINT_OT_output_toggle);
}

/****************************** general ************************************/

void ED_operatortypes_physics(void)
{
	operatortypes_rigidbody();
	operatortypes_fluid();
	operatortypes_dynamicpaint();
}

void ED_keymap_physics(wmKeyConfig *UNUSED(keyconf))
{
}
