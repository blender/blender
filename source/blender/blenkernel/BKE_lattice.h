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
 */

#ifndef __BKE_LATTICE_H__
#define __BKE_LATTICE_H__

/** \file BKE_lattice.h
 *  \ingroup bke
 *  \author Ton Roosendaal
 *  \since June 2001
 */

struct Lattice;
struct Object;
struct Scene;
struct DerivedMesh;
struct BPoint;
struct MDeformVert;

void resizelattice(struct Lattice *lt, int u, int v, int w, struct Object *ltOb);
struct Lattice *add_lattice(const char *name);
struct Lattice *BKE_lattice_copy(struct Lattice *lt);
void BKE_lattice_free(struct Lattice *lt);
void BKE_lattice_make_local(struct Lattice *lt);
void calc_lat_fudu(int flag, int res, float *fu, float *du);

void init_latt_deform(struct Object *oblatt, struct Object *ob);
void calc_latt_deform(struct Object *, float co[3], float weight);
void end_latt_deform(struct Object *);

int object_deform_mball(struct Object *ob, struct ListBase *dispbase);
void outside_lattice(struct Lattice *lt);

void curve_deform_verts(struct Scene *scene, struct Object *cuOb, struct Object *target,
                        struct DerivedMesh *dm, float (*vertexCos)[3],
                        int numVerts, const char *vgroup, short defaxis);
void curve_deform_vector(struct Scene *scene, struct Object *cuOb, struct Object *target,
                         float orco[3], float vec[3], float mat[][3], int no_rot_axis);

void lattice_deform_verts(struct Object *laOb, struct Object *target,
                          struct DerivedMesh *dm, float (*vertexCos)[3],
                          int numVerts, const char *vgroup, float influence);
void armature_deform_verts(struct Object *armOb, struct Object *target,
                           struct DerivedMesh *dm, float (*vertexCos)[3],
                           float (*defMats)[3][3], int numVerts, int deformflag,
                           float (*prevCos)[3], const char *defgrp_name);

float (*lattice_getVertexCos(struct Object *ob, int *numVerts_r))[3];
void lattice_applyVertexCos(struct Object *ob, float (*vertexCos)[3]);
void lattice_calc_modifiers(struct Scene *scene, struct Object *ob);

struct MDeformVert* lattice_get_deform_verts(struct Object *lattice);

#endif

