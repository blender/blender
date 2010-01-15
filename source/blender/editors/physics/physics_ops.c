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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_physics.h"
#include "ED_object.h"

#include "physics_intern.h" // own include

/***************************** particles ***********************************/

static void operatortypes_particle(void)
{
	WM_operatortype_append(PARTICLE_OT_select_all);
	WM_operatortype_append(PARTICLE_OT_select_first);
	WM_operatortype_append(PARTICLE_OT_select_last);
	WM_operatortype_append(PARTICLE_OT_select_linked);
	WM_operatortype_append(PARTICLE_OT_select_less);
	WM_operatortype_append(PARTICLE_OT_select_more);
	WM_operatortype_append(PARTICLE_OT_select_inverse);

	WM_operatortype_append(PARTICLE_OT_hide);
	WM_operatortype_append(PARTICLE_OT_reveal);

	WM_operatortype_append(PARTICLE_OT_rekey);
	WM_operatortype_append(PARTICLE_OT_subdivide);
	WM_operatortype_append(PARTICLE_OT_remove_doubles);
	WM_operatortype_append(PARTICLE_OT_weight_set);	
	WM_operatortype_append(PARTICLE_OT_delete);
	WM_operatortype_append(PARTICLE_OT_mirror);

	WM_operatortype_append(PARTICLE_OT_brush_edit);
	WM_operatortype_append(PARTICLE_OT_brush_radial_control);

	WM_operatortype_append(PARTICLE_OT_particle_edit_toggle);
	WM_operatortype_append(PARTICLE_OT_edited_clear);


	WM_operatortype_append(OBJECT_OT_particle_system_add);
	WM_operatortype_append(OBJECT_OT_particle_system_remove);

	WM_operatortype_append(PARTICLE_OT_new);
	WM_operatortype_append(PARTICLE_OT_new_target);
	WM_operatortype_append(PARTICLE_OT_target_remove);
	WM_operatortype_append(PARTICLE_OT_target_move_up);
	WM_operatortype_append(PARTICLE_OT_target_move_down);
	WM_operatortype_append(PARTICLE_OT_connect_hair);
	WM_operatortype_append(PARTICLE_OT_disconnect_hair);

	WM_operatortype_append(PARTICLE_OT_dupliob_copy);
	WM_operatortype_append(PARTICLE_OT_dupliob_remove);
	WM_operatortype_append(PARTICLE_OT_dupliob_move_up);
	WM_operatortype_append(PARTICLE_OT_dupliob_move_down);
}

static void keymap_particle(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	keymap= WM_keymap_find(keyconf, "Particle", 0, 0);
	keymap->poll= PE_poll;
	
	WM_keymap_add_item(keymap, "PARTICLE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "PARTICLE_OT_select_linked", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "deselect", 1);
	WM_keymap_add_item(keymap, "PARTICLE_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "PARTICLE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "PARTICLE_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "PARTICLE_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);

	WM_keymap_add_item(keymap, "PARTICLE_OT_brush_edit", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PARTICLE_OT_brush_edit", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "PARTICLE_OT_brush_radial_control", FKEY, KM_PRESS, 0, 0)->ptr, "mode", WM_RADIALCONTROL_SIZE);
	RNA_enum_set(WM_keymap_add_item(keymap, "PARTICLE_OT_brush_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", WM_RADIALCONTROL_STRENGTH);

	WM_keymap_add_menu(keymap, "VIEW3D_MT_particle_specials", WKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "PARTICLE_OT_weight_set", KKEY, KM_PRESS, KM_SHIFT, 0);

	ED_object_generic_keymap(keyconf, keymap, 1);
}

/******************************* boids *************************************/

static void operatortypes_boids(void)
{
	WM_operatortype_append(BOID_OT_rule_add);
	WM_operatortype_append(BOID_OT_rule_del);
	WM_operatortype_append(BOID_OT_rule_move_up);
	WM_operatortype_append(BOID_OT_rule_move_down);

	WM_operatortype_append(BOID_OT_state_add);
	WM_operatortype_append(BOID_OT_state_del);
	WM_operatortype_append(BOID_OT_state_move_up);
	WM_operatortype_append(BOID_OT_state_move_down);
}

/********************************* fluid ***********************************/

static void operatortypes_fluid(void)
{
	WM_operatortype_append(FLUID_OT_bake);
}

/**************************** point cache **********************************/

static void operatortypes_pointcache(void)
{
	WM_operatortype_append(PTCACHE_OT_bake_all);
	WM_operatortype_append(PTCACHE_OT_free_bake_all);
	WM_operatortype_append(PTCACHE_OT_bake);
	WM_operatortype_append(PTCACHE_OT_free_bake);
	WM_operatortype_append(PTCACHE_OT_bake_from_cache);
	WM_operatortype_append(PTCACHE_OT_add);
	WM_operatortype_append(PTCACHE_OT_remove);
}

//static void keymap_pointcache(wmWindowManager *wm)
//{
//	wmKeyMap *keymap= WM_keymap_find(wm, "Pointcache", 0, 0);
//	
//	WM_keymap_add_item(keymap, "PHYSICS_OT_bake_all", AKEY, KM_PRESS, 0, 0);
//	WM_keymap_add_item(keymap, "PHYSICS_OT_free_all", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
//	WM_keymap_add_item(keymap, "PHYSICS_OT_bake_particle_system", PADMINUS, KM_PRESS, KM_CTRL, 0);
//	WM_keymap_add_item(keymap, "PHYSICS_OT_free_particle_system", LKEY, KM_PRESS, 0, 0);
//}

/****************************** general ************************************/

void ED_operatortypes_physics(void)
{
	operatortypes_particle();
	operatortypes_boids();
	operatortypes_fluid();
	operatortypes_pointcache();
}

void ED_keymap_physics(wmKeyConfig *keyconf)
{
	keymap_particle(keyconf);
	//keymap_pointcache(keyconf);
}



