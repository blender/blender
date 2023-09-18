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

#ifdef __cplusplus
extern "C" {
#endif

/* `particle_edit.cc` */

void PARTICLE_OT_select_all(struct wmOperatorType *ot);
void PARTICLE_OT_select_roots(struct wmOperatorType *ot);
void PARTICLE_OT_select_tips(struct wmOperatorType *ot);
void PARTICLE_OT_select_random(struct wmOperatorType *ot);
void PARTICLE_OT_select_linked(struct wmOperatorType *ot);
void PARTICLE_OT_select_linked_pick(struct wmOperatorType *ot);
void PARTICLE_OT_select_less(struct wmOperatorType *ot);
void PARTICLE_OT_select_more(struct wmOperatorType *ot);

void PARTICLE_OT_hide(struct wmOperatorType *ot);
void PARTICLE_OT_reveal(struct wmOperatorType *ot);

void PARTICLE_OT_rekey(struct wmOperatorType *ot);
void PARTICLE_OT_subdivide(struct wmOperatorType *ot);
void PARTICLE_OT_remove_doubles(struct wmOperatorType *ot);
void PARTICLE_OT_weight_set(struct wmOperatorType *ot);
void PARTICLE_OT_delete(struct wmOperatorType *ot);
void PARTICLE_OT_mirror(struct wmOperatorType *ot);

void PARTICLE_OT_brush_edit(struct wmOperatorType *ot);

void PARTICLE_OT_shape_cut(struct wmOperatorType *ot);

void PARTICLE_OT_particle_edit_toggle(struct wmOperatorType *ot);
void PARTICLE_OT_edited_clear(struct wmOperatorType *ot);

void PARTICLE_OT_unify_length(struct wmOperatorType *ot);

/**
 * Initialize needed data for bake edit.
 */
void PE_create_particle_edit(struct Depsgraph *depsgraph,
                             struct Scene *scene,
                             struct Object *ob,
                             struct PointCache *cache,
                             struct ParticleSystem *psys);
/**
 * Set current distances to be kept between neighboring keys.
 */
void recalc_lengths(struct PTCacheEdit *edit);
/**
 * Calculate a tree for finding nearest emitter's vertices.
 */
void recalc_emitter_field(struct Depsgraph *depsgraph,
                          struct Object *ob,
                          struct ParticleSystem *psys);
void update_world_cos(struct Object *ob, struct PTCacheEdit *edit);

/* `particle_object.cc` */

void OBJECT_OT_particle_system_add(struct wmOperatorType *ot);
void OBJECT_OT_particle_system_remove(struct wmOperatorType *ot);

void PARTICLE_OT_new(struct wmOperatorType *ot);
void PARTICLE_OT_new_target(struct wmOperatorType *ot);
void PARTICLE_OT_target_remove(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_up(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_down(struct wmOperatorType *ot);
void PARTICLE_OT_connect_hair(struct wmOperatorType *ot);
void PARTICLE_OT_disconnect_hair(struct wmOperatorType *ot);
void PARTICLE_OT_copy_particle_systems(struct wmOperatorType *ot);
void PARTICLE_OT_duplicate_particle_system(struct wmOperatorType *ot);

void PARTICLE_OT_dupliob_copy(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_remove(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_up(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_move_down(struct wmOperatorType *ot);
void PARTICLE_OT_dupliob_refresh(struct wmOperatorType *ot);

/* `particle_boids.cc` */

void BOID_OT_rule_add(struct wmOperatorType *ot);
void BOID_OT_rule_del(struct wmOperatorType *ot);
void BOID_OT_rule_move_up(struct wmOperatorType *ot);
void BOID_OT_rule_move_down(struct wmOperatorType *ot);

void BOID_OT_state_add(struct wmOperatorType *ot);
void BOID_OT_state_del(struct wmOperatorType *ot);
void BOID_OT_state_move_up(struct wmOperatorType *ot);
void BOID_OT_state_move_down(struct wmOperatorType *ot);

/* `physics_fluid.cc` */

void FLUID_OT_bake_all(struct wmOperatorType *ot);
void FLUID_OT_free_all(struct wmOperatorType *ot);
void FLUID_OT_bake_data(struct wmOperatorType *ot);
void FLUID_OT_free_data(struct wmOperatorType *ot);
void FLUID_OT_bake_noise(struct wmOperatorType *ot);
void FLUID_OT_free_noise(struct wmOperatorType *ot);
void FLUID_OT_bake_mesh(struct wmOperatorType *ot);
void FLUID_OT_free_mesh(struct wmOperatorType *ot);
void FLUID_OT_bake_particles(struct wmOperatorType *ot);
void FLUID_OT_free_particles(struct wmOperatorType *ot);
void FLUID_OT_bake_guides(struct wmOperatorType *ot);
void FLUID_OT_free_guides(struct wmOperatorType *ot);
void FLUID_OT_pause_bake(struct wmOperatorType *ot);

/* dynamicpaint.cc */

void DPAINT_OT_bake(struct wmOperatorType *ot);
/**
 * Add surface slot.
 */
void DPAINT_OT_surface_slot_add(struct wmOperatorType *ot);
/**
 * Remove surface slot.
 */
void DPAINT_OT_surface_slot_remove(struct wmOperatorType *ot);
void DPAINT_OT_type_toggle(struct wmOperatorType *ot);
void DPAINT_OT_output_toggle(struct wmOperatorType *ot);

/* `physics_pointcache.cc` */

void PTCACHE_OT_bake_all(struct wmOperatorType *ot);
void PTCACHE_OT_free_bake_all(struct wmOperatorType *ot);
void PTCACHE_OT_bake(struct wmOperatorType *ot);
void PTCACHE_OT_free_bake(struct wmOperatorType *ot);
void PTCACHE_OT_bake_from_cache(struct wmOperatorType *ot);
void PTCACHE_OT_add(struct wmOperatorType *ot);
void PTCACHE_OT_remove(struct wmOperatorType *ot);

/* `rigidbody_object.cc` */

void RIGIDBODY_OT_object_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_object_remove(struct wmOperatorType *ot);

void RIGIDBODY_OT_objects_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_objects_remove(struct wmOperatorType *ot);

void RIGIDBODY_OT_shape_change(struct wmOperatorType *ot);
void RIGIDBODY_OT_mass_calculate(struct wmOperatorType *ot);

/* `rigidbody_constraint.cc` */

void RIGIDBODY_OT_constraint_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_constraint_remove(struct wmOperatorType *ot);

/* `rigidbody_world.cc` */

void RIGIDBODY_OT_world_add(struct wmOperatorType *ot);
void RIGIDBODY_OT_world_remove(struct wmOperatorType *ot);
void RIGIDBODY_OT_world_export(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
