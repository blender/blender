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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#ifndef __BKE_CRAZYSPACE_H__
#define __BKE_CRAZYSPACE_H__

#ifdef __cplusplus
extern "C" {
#endif
struct BMEditMesh;
struct Depsgraph;
struct Mesh;
struct Object;
struct Scene;

/* crazyspace.c */
float (*BKE_crazyspace_get_mapped_editverts(struct Depsgraph *depsgraph,
                                            struct Object *obedit))[3];
void BKE_crazyspace_set_quats_editmesh(struct BMEditMesh *em,
                                       float (*origcos)[3],
                                       float (*mappedcos)[3],
                                       float (*quats)[4],
                                       const bool use_select);
void BKE_crazyspace_set_quats_mesh(struct Mesh *me,
                                   float (*origcos)[3],
                                   float (*mappedcos)[3],
                                   float (*quats)[4]);
int BKE_crazyspace_get_first_deform_matrices_editbmesh(struct Depsgraph *depsgraph,
                                                       struct Scene *,
                                                       struct Object *,
                                                       struct BMEditMesh *em,
                                                       float (**deformmats)[3][3],
                                                       float (**deformcos)[3]);
int BKE_sculpt_get_first_deform_matrices(struct Depsgraph *depsgraph,
                                         struct Scene *scene,
                                         struct Object *ob,
                                         float (**deformmats)[3][3],
                                         float (**deformcos)[3]);
void BKE_crazyspace_build_sculpt(struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob,
                                 float (**deformmats)[3][3],
                                 float (**deformcos)[3]);

#ifdef __cplusplus
}
#endif

#endif
