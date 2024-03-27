/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#pragma once

struct Depsgraph;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct PointCache;
struct Scene;
struct wmOperatorType;

/* `particle_edit.cc` */

void PARTICLE_OT_select_all(wmOperatorType *ot);
void PARTICLE_OT_select_roots(wmOperatorType *ot);
void PARTICLE_OT_select_tips(wmOperatorType *ot);
void PARTICLE_OT_select_random(wmOperatorType *ot);
void PARTICLE_OT_select_linked(wmOperatorType *ot);
void PARTICLE_OT_select_linked_pick(wmOperatorType *ot);
void PARTICLE_OT_select_less(wmOperatorType *ot);
void PARTICLE_OT_select_more(wmOperatorType *ot);

void PARTICLE_OT_hide(wmOperatorType *ot);
void PARTICLE_OT_reveal(wmOperatorType *ot);

void PARTICLE_OT_rekey(wmOperatorType *ot);
void PARTICLE_OT_subdivide(wmOperatorType *ot);
void PARTICLE_OT_remove_doubles(wmOperatorType *ot);
void PARTICLE_OT_weight_set(wmOperatorType *ot);
void PARTICLE_OT_delete(wmOperatorType *ot);
void PARTICLE_OT_mirror(wmOperatorType *ot);

void PARTICLE_OT_brush_edit(wmOperatorType *ot);

void PARTICLE_OT_shape_cut(wmOperatorType *ot);

void PARTICLE_OT_particle_edit_toggle(wmOperatorType *ot);
void PARTICLE_OT_edited_clear(wmOperatorType *ot);

void PARTICLE_OT_unify_length(wmOperatorType *ot);

/**
 * Initialize needed data for bake edit.
 */
void PE_create_particle_edit(
    Depsgraph *depsgraph, Scene *scene, Object *ob, PointCache *cache, ParticleSystem *psys);
/**
 * Set current distances to be kept between neighboring keys.
 */
void recalc_lengths(PTCacheEdit *edit);
/**
 * Calculate a tree for finding nearest emitter's vertices.
 */
void recalc_emitter_field(Depsgraph *depsgraph, Object *ob, ParticleSystem *psys);
void update_world_cos(Object *ob, PTCacheEdit *edit);

/* `particle_object.cc` */

void OBJECT_OT_particle_system_add(wmOperatorType *ot);
void OBJECT_OT_particle_system_remove(wmOperatorType *ot);

void PARTICLE_OT_new(wmOperatorType *ot);
void PARTICLE_OT_new_target(wmOperatorType *ot);
void PARTICLE_OT_target_remove(wmOperatorType *ot);
void PARTICLE_OT_target_move_up(wmOperatorType *ot);
void PARTICLE_OT_target_move_down(wmOperatorType *ot);
void PARTICLE_OT_connect_hair(wmOperatorType *ot);
void PARTICLE_OT_disconnect_hair(wmOperatorType *ot);
void PARTICLE_OT_copy_particle_systems(wmOperatorType *ot);
void PARTICLE_OT_duplicate_particle_system(wmOperatorType *ot);

void PARTICLE_OT_dupliob_copy(wmOperatorType *ot);
void PARTICLE_OT_dupliob_remove(wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_up(wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_down(wmOperatorType *ot);
void PARTICLE_OT_dupliob_refresh(wmOperatorType *ot);

/* `particle_boids.cc` */

void BOID_OT_rule_add(wmOperatorType *ot);
void BOID_OT_rule_del(wmOperatorType *ot);
void BOID_OT_rule_move_up(wmOperatorType *ot);
void BOID_OT_rule_move_down(wmOperatorType *ot);

void BOID_OT_state_add(wmOperatorType *ot);
void BOID_OT_state_del(wmOperatorType *ot);
void BOID_OT_state_move_up(wmOperatorType *ot);
void BOID_OT_state_move_down(wmOperatorType *ot);

/* `physics_fluid.cc` */

void FLUID_OT_bake_all(wmOperatorType *ot);
void FLUID_OT_free_all(wmOperatorType *ot);
void FLUID_OT_bake_data(wmOperatorType *ot);
void FLUID_OT_free_data(wmOperatorType *ot);
void FLUID_OT_bake_noise(wmOperatorType *ot);
void FLUID_OT_free_noise(wmOperatorType *ot);
void FLUID_OT_bake_mesh(wmOperatorType *ot);
void FLUID_OT_free_mesh(wmOperatorType *ot);
void FLUID_OT_bake_particles(wmOperatorType *ot);
void FLUID_OT_free_particles(wmOperatorType *ot);
void FLUID_OT_bake_guides(wmOperatorType *ot);
void FLUID_OT_free_guides(wmOperatorType *ot);
void FLUID_OT_pause_bake(wmOperatorType *ot);

/* dynamicpaint.cc */

void DPAINT_OT_bake(wmOperatorType *ot);
/**
 * Add surface slot.
 */
void DPAINT_OT_surface_slot_add(wmOperatorType *ot);
/**
 * Remove surface slot.
 */
void DPAINT_OT_surface_slot_remove(wmOperatorType *ot);
void DPAINT_OT_type_toggle(wmOperatorType *ot);
void DPAINT_OT_output_toggle(wmOperatorType *ot);

/* `physics_pointcache.cc` */

void PTCACHE_OT_bake_all(wmOperatorType *ot);
void PTCACHE_OT_free_bake_all(wmOperatorType *ot);
void PTCACHE_OT_bake(wmOperatorType *ot);
void PTCACHE_OT_free_bake(wmOperatorType *ot);
void PTCACHE_OT_bake_from_cache(wmOperatorType *ot);
void PTCACHE_OT_add(wmOperatorType *ot);
void PTCACHE_OT_remove(wmOperatorType *ot);

/* `rigidbody_object.cc` */

void RIGIDBODY_OT_object_add(wmOperatorType *ot);
void RIGIDBODY_OT_object_remove(wmOperatorType *ot);

void RIGIDBODY_OT_objects_add(wmOperatorType *ot);
void RIGIDBODY_OT_objects_remove(wmOperatorType *ot);

void RIGIDBODY_OT_shape_change(wmOperatorType *ot);
void RIGIDBODY_OT_mass_calculate(wmOperatorType *ot);

/* `rigidbody_constraint.cc` */

void RIGIDBODY_OT_constraint_add(wmOperatorType *ot);
void RIGIDBODY_OT_constraint_remove(wmOperatorType *ot);

/* `rigidbody_world.cc` */

void RIGIDBODY_OT_world_add(wmOperatorType *ot);
void RIGIDBODY_OT_world_remove(wmOperatorType *ot);
void RIGIDBODY_OT_world_export(wmOperatorType *ot);
