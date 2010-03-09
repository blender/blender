/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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

#ifndef ED_PHYSICS_INTERN_H
#define ED_PHYSICS_INTERN_H

struct wmOperatorType;

/* particle_edit.c */
void PARTICLE_OT_select_all(struct wmOperatorType *ot);
void PARTICLE_OT_select_first(struct wmOperatorType *ot);
void PARTICLE_OT_select_last(struct wmOperatorType *ot);
void PARTICLE_OT_select_linked(struct wmOperatorType *ot);
void PARTICLE_OT_select_less(struct wmOperatorType *ot);
void PARTICLE_OT_select_more(struct wmOperatorType *ot);
void PARTICLE_OT_select_inverse(struct wmOperatorType *ot);

void PARTICLE_OT_hide(struct wmOperatorType *ot);
void PARTICLE_OT_reveal(struct wmOperatorType *ot);

void PARTICLE_OT_rekey(struct wmOperatorType *ot);
void PARTICLE_OT_subdivide(struct wmOperatorType *ot);
void PARTICLE_OT_remove_doubles(struct wmOperatorType *ot);
void PARTICLE_OT_weight_set(struct wmOperatorType *ot);
void PARTICLE_OT_delete(struct wmOperatorType *ot);
void PARTICLE_OT_mirror(struct wmOperatorType *ot);

void PARTICLE_OT_brush_edit(struct wmOperatorType *ot);
void PARTICLE_OT_brush_radial_control(struct wmOperatorType *ot);

void PARTICLE_OT_particle_edit_toggle(struct wmOperatorType *ot);
void PARTICLE_OT_edited_clear(struct wmOperatorType *ot);

/* particle_object.c */
void OBJECT_OT_particle_system_add(struct wmOperatorType *ot);
void OBJECT_OT_particle_system_remove(struct wmOperatorType *ot);

void PARTICLE_OT_new(struct wmOperatorType *ot);
void PARTICLE_OT_new_target(struct wmOperatorType *ot);
void PARTICLE_OT_target_remove(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_up(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_down(struct wmOperatorType *ot);
void PARTICLE_OT_connect_hair(struct wmOperatorType *ot);
void PARTICLE_OT_disconnect_hair(struct wmOperatorType *ot);

void PARTICLE_OT_dupliob_copy(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_remove(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_up(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_down(struct wmOperatorType *ot);

/* particle_boids.c */
void BOID_OT_rule_add(struct wmOperatorType *ot);
void BOID_OT_rule_del(struct wmOperatorType *ot);
void BOID_OT_rule_move_up(struct wmOperatorType *ot);
void BOID_OT_rule_move_down(struct wmOperatorType *ot);

void BOID_OT_state_add(struct wmOperatorType *ot);
void BOID_OT_state_del(struct wmOperatorType *ot);
void BOID_OT_state_move_up(struct wmOperatorType *ot);
void BOID_OT_state_move_down(struct wmOperatorType *ot);

/* physics_fluid.c */
void FLUID_OT_bake(struct wmOperatorType *ot);

/* physics_pointcache.c */
void PTCACHE_OT_bake_all(struct wmOperatorType *ot);
void PTCACHE_OT_free_bake_all(struct wmOperatorType *ot);
void PTCACHE_OT_bake(struct wmOperatorType *ot);
void PTCACHE_OT_free_bake(struct wmOperatorType *ot);
void PTCACHE_OT_bake_from_cache(struct wmOperatorType *ot);
void PTCACHE_OT_add(struct wmOperatorType *ot);
void PTCACHE_OT_remove(struct wmOperatorType *ot);

#endif /* ED_PHYSICS_INTERN_H */

