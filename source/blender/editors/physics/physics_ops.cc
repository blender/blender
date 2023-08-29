/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#include <cstdlib>

#include "WM_api.hh"

#include "ED_physics.hh"

#include "physics_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

static void operatortypes_particle()
{
  WM_operatortype_append(PARTICLE_OT_select_all);
  WM_operatortype_append(PARTICLE_OT_select_roots);
  WM_operatortype_append(PARTICLE_OT_select_tips);
  WM_operatortype_append(PARTICLE_OT_select_random);
  WM_operatortype_append(PARTICLE_OT_select_linked);
  WM_operatortype_append(PARTICLE_OT_select_linked_pick);
  WM_operatortype_append(PARTICLE_OT_select_less);
  WM_operatortype_append(PARTICLE_OT_select_more);

  WM_operatortype_append(PARTICLE_OT_hide);
  WM_operatortype_append(PARTICLE_OT_reveal);

  WM_operatortype_append(PARTICLE_OT_rekey);
  WM_operatortype_append(PARTICLE_OT_subdivide);
  WM_operatortype_append(PARTICLE_OT_remove_doubles);
  WM_operatortype_append(PARTICLE_OT_weight_set);
  WM_operatortype_append(PARTICLE_OT_delete);
  WM_operatortype_append(PARTICLE_OT_mirror);

  WM_operatortype_append(PARTICLE_OT_brush_edit);

  WM_operatortype_append(PARTICLE_OT_shape_cut);

  WM_operatortype_append(PARTICLE_OT_particle_edit_toggle);
  WM_operatortype_append(PARTICLE_OT_edited_clear);

  WM_operatortype_append(PARTICLE_OT_unify_length);

  WM_operatortype_append(OBJECT_OT_particle_system_add);
  WM_operatortype_append(OBJECT_OT_particle_system_remove);

  WM_operatortype_append(PARTICLE_OT_new);
  WM_operatortype_append(PARTICLE_OT_new_target);
  WM_operatortype_append(PARTICLE_OT_target_remove);
  WM_operatortype_append(PARTICLE_OT_target_move_up);
  WM_operatortype_append(PARTICLE_OT_target_move_down);
  WM_operatortype_append(PARTICLE_OT_connect_hair);
  WM_operatortype_append(PARTICLE_OT_disconnect_hair);
  WM_operatortype_append(PARTICLE_OT_copy_particle_systems);
  WM_operatortype_append(PARTICLE_OT_duplicate_particle_system);

  WM_operatortype_append(PARTICLE_OT_dupliob_refresh);
  WM_operatortype_append(PARTICLE_OT_dupliob_copy);
  WM_operatortype_append(PARTICLE_OT_dupliob_remove);
  WM_operatortype_append(PARTICLE_OT_dupliob_move_up);
  WM_operatortype_append(PARTICLE_OT_dupliob_move_down);

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
  //  WM_operatortype_append(RIGIDBODY_OT_world_export);
}

static void keymap_particle(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Particle", 0, 0);
  keymap->poll = PE_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boids
 * \{ */

static void operatortypes_boids()
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fluid
 * \{ */

static void operatortypes_fluid()
{
  WM_operatortype_append(FLUID_OT_bake_all);
  WM_operatortype_append(FLUID_OT_free_all);
  WM_operatortype_append(FLUID_OT_bake_data);
  WM_operatortype_append(FLUID_OT_free_data);
  WM_operatortype_append(FLUID_OT_bake_noise);
  WM_operatortype_append(FLUID_OT_free_noise);
  WM_operatortype_append(FLUID_OT_bake_mesh);
  WM_operatortype_append(FLUID_OT_free_mesh);
  WM_operatortype_append(FLUID_OT_bake_particles);
  WM_operatortype_append(FLUID_OT_free_particles);
  WM_operatortype_append(FLUID_OT_bake_guides);
  WM_operatortype_append(FLUID_OT_free_guides);
  WM_operatortype_append(FLUID_OT_pause_bake);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cache
 * \{ */

static void operatortypes_pointcache()
{
  WM_operatortype_append(PTCACHE_OT_bake_all);
  WM_operatortype_append(PTCACHE_OT_free_bake_all);
  WM_operatortype_append(PTCACHE_OT_bake);
  WM_operatortype_append(PTCACHE_OT_free_bake);
  WM_operatortype_append(PTCACHE_OT_bake_from_cache);
  WM_operatortype_append(PTCACHE_OT_add);
  WM_operatortype_append(PTCACHE_OT_remove);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dynamic Paint
 * \{ */

static void operatortypes_dynamicpaint()
{
  WM_operatortype_append(DPAINT_OT_bake);
  WM_operatortype_append(DPAINT_OT_surface_slot_add);
  WM_operatortype_append(DPAINT_OT_surface_slot_remove);
  WM_operatortype_append(DPAINT_OT_type_toggle);
  WM_operatortype_append(DPAINT_OT_output_toggle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_physics()
{
  operatortypes_particle();
  operatortypes_boids();
  operatortypes_fluid();
  operatortypes_pointcache();
  operatortypes_dynamicpaint();
}

void ED_keymap_physics(wmKeyConfig *keyconf)
{
  keymap_particle(keyconf);
}

/** \} */
