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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup sim
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ClothModifierData;
struct Depsgraph;
struct Implicit_Data;
struct ListBase;
struct Object;

typedef enum eMassSpringSolverStatus {
  SIM_SOLVER_SUCCESS = (1 << 0),
  SIM_SOLVER_NUMERICAL_ISSUE = (1 << 1),
  SIM_SOLVER_NO_CONVERGENCE = (1 << 2),
  SIM_SOLVER_INVALID_INPUT = (1 << 3),
} eMassSpringSolverStatus;

struct Implicit_Data *SIM_mass_spring_solver_create(int numverts, int numsprings);
void SIM_mass_spring_solver_free(struct Implicit_Data *id);
int SIM_mass_spring_solver_numvert(struct Implicit_Data *id);

int SIM_cloth_solver_init(struct Object *ob, struct ClothModifierData *clmd);
void SIM_cloth_solver_free(struct ClothModifierData *clmd);
int SIM_cloth_solve(struct Depsgraph *depsgraph,
                    struct Object *ob,
                    float frame,
                    struct ClothModifierData *clmd,
                    struct ListBase *effectors);
void SIM_cloth_solver_set_positions(struct ClothModifierData *clmd);
void SIM_cloth_solver_set_volume(struct ClothModifierData *clmd);

#ifdef __cplusplus
}
#endif
