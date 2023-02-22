/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation */

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
void SIM_mass_spring_set_implicit_vertex_mass(struct Implicit_Data *data, int index, float mass);

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
