/*
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
 * The Original Code is Copyright (C) 2009 by Janne Karhu.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BoidSettings;
struct BoidState;
struct Object;
struct ParticleData;
struct ParticleSettings;
struct ParticleSimulationData;
struct RNG;

typedef struct BoidBrainData {
  struct ParticleSimulationData *sim;
  struct ParticleSettings *part;
  float timestep, cfra, dfra;
  float wanted_co[3], wanted_speed;

  /* Goal stuff */
  struct Object *goal_ob;
  float goal_co[3];
  float goal_nor[3];
  float goal_priority;

  struct RNG *rng;
} BoidBrainData;

void boids_precalc_rules(struct ParticleSettings *part, float cfra);
void boid_brain(BoidBrainData *bbd, int p, struct ParticleData *pa);
void boid_body(BoidBrainData *bbd, struct ParticleData *pa);
void boid_default_settings(struct BoidSettings *boids);
struct BoidRule *boid_new_rule(int type);
struct BoidState *boid_new_state(struct BoidSettings *boids);
struct BoidState *boid_duplicate_state(struct BoidSettings *boids, struct BoidState *state);
void boid_free_settings(struct BoidSettings *boids);
struct BoidSettings *boid_copy_settings(const struct BoidSettings *boids);
struct BoidState *boid_get_current_state(struct BoidSettings *boids);

#ifdef __cplusplus
}
#endif
