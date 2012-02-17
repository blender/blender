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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * BIF_meshlaplacian.h: Algorithms using the mesh laplacian.
 */

/** \file blender/editors/armature/meshlaplacian.h
 *  \ingroup edarmature
 */


#ifndef __MESHLAPLACIAN_H__
#define __MESHLAPLACIAN_H__

//#define RIGID_DEFORM

struct Scene;
struct Object;
struct Mesh;
struct bDeformGroup;
struct MeshDeformModifierData;

#ifdef RIGID_DEFORM
struct EditMesh;
#endif

/* Laplacian System */

struct LaplacianSystem;
typedef struct LaplacianSystem LaplacianSystem;

LaplacianSystem *laplacian_construct_begin(int totvert, int totface, int lsq);

void laplacian_add_vertex(LaplacianSystem *sys, float *co, int pinned);
void laplacian_add_triangle(LaplacianSystem *sys, int v1, int v2, int v3);

void laplacian_construct_end(LaplacianSystem *sys);
void laplacian_delete(LaplacianSystem *sys);

void laplacian_begin_solve(LaplacianSystem *sys, int index);
void laplacian_add_right_hand_side(LaplacianSystem *sys, int v, float value);
int laplacian_system_solve(LaplacianSystem *sys);
float laplacian_system_get_solution(int v);

/* Heat Weighting */

void heat_bone_weighting(struct Object *ob, struct Mesh *me, float (*verts)[3],
	int numbones, struct bDeformGroup **dgrouplist,
	struct bDeformGroup **dgroupflip, float (*root)[3], float (*tip)[3],
	int *selected, const char **error);

#ifdef RIGID_DEFORM
/* As-Rigid-As-Possible Deformation */

void rigid_deform_begin(struct EditMesh *em);
void rigid_deform_iteration(void);
void rigid_deform_end(int cancel);
#endif

/* Harmonic Coordinates */

/* mesh_deform_bind(...) defined in ED_armature.h */

#endif

