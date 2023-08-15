/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * BIF_meshlaplacian.h: Algorithms using the mesh laplacian.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//#define RIGID_DEFORM

struct Mesh;
struct Object;
struct bDeformGroup;

#ifdef RIGID_DEFORM
struct EditMesh;
#endif

/* Laplacian System */

struct LaplacianSystem;
typedef struct LaplacianSystem LaplacianSystem;

void laplacian_add_vertex(LaplacianSystem *sys, float *co, int pinned);
void laplacian_add_triangle(LaplacianSystem *sys, int v1, int v2, int v3);

void laplacian_begin_solve(LaplacianSystem *sys, int index);
void laplacian_add_right_hand_side(LaplacianSystem *sys, int v, float value);
int laplacian_system_solve(LaplacianSystem *sys);
float laplacian_system_get_solution(LaplacianSystem *sys, int v);

/* Heat Weighting */

void heat_bone_weighting(struct Object *ob,
                         struct Mesh *me,
                         float (*verts)[3],
                         int numbones,
                         struct bDeformGroup **dgrouplist,
                         struct bDeformGroup **dgroupflip,
                         float (*root)[3],
                         float (*tip)[3],
                         const int *selected,
                         const char **error_str);

#ifdef RIGID_DEFORM
/* As-Rigid-As-Possible Deformation */

void rigid_deform_begin(struct EditMesh *em);
void rigid_deform_iteration(void);
void rigid_deform_end(int cancel);
#endif

#ifdef __cplusplus
}
#endif

/* Harmonic Coordinates */

/* ED_mesh_deform_bind_callback(...) defined in ED_armature.hh */
