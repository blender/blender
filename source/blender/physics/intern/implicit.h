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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BPH_IMPLICIT_H__
#define __BPH_IMPLICIT_H__

/** \file implicit.h
 *  \ingroup bph
 */

#include "stdio.h"

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define IMPLICIT_SOLVER_EIGEN
#define IMPLICIT_SOLVER_BLENDER

#define CLOTH_ROOT_FRAME /* enable use of root frame coordinate transform */

#define CLOTH_FORCE_GRAVITY
#define CLOTH_FORCE_DRAG
#define CLOTH_FORCE_SPRING_STRUCTURAL
#define CLOTH_FORCE_SPRING_BEND
#define CLOTH_FORCE_SPRING_GOAL
#define CLOTH_FORCE_EFFECTORS

//#define IMPLICIT_PRINT_SOLVER_INPUT_OUTPUT

//#define IMPLICIT_ENABLE_EIGEN_DEBUG

struct Implicit_Data;

BLI_INLINE void implicit_print_matrix_elem(float v)
{
    printf("%-8.3f", v);
}

void BPH_mass_spring_set_root_motion(struct Implicit_Data *data, int index, const float loc[3], const float vel[3], float rot[3][3], const float angvel[3]);
void BPH_mass_spring_set_motion_state(struct Implicit_Data *data, int index, const float x[3], const float v[3]);
void BPH_mass_spring_set_vertex_mass(struct Implicit_Data *data, int index, float mass);
int BPH_mass_spring_init_spring(struct Implicit_Data *data, int index, int v1, int v2);

#ifdef __cplusplus
}
#endif

#endif
