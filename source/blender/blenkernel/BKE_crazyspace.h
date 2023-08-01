/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;
struct Depsgraph;
struct Mesh;
struct Object;
struct ReportList;
struct Scene;

/* `crazyspace.cc` */

/**
 * Disable subdivision-surface temporal, get mapped coordinates, and enable it.
 */
float (*BKE_crazyspace_get_mapped_editverts(struct Depsgraph *depsgraph,
                                            struct Object *obedit))[3];
void BKE_crazyspace_set_quats_editmesh(struct BMEditMesh *em,
                                       float (*origcos)[3],
                                       float (*mappedcos)[3],
                                       float (*quats)[4],
                                       bool use_select);
void BKE_crazyspace_set_quats_mesh(struct Mesh *me,
                                   float (*origcos)[3],
                                   float (*mappedcos)[3],
                                   float (*quats)[4]);
/**
 * Returns an array of deform matrices for crazy-space correction,
 * and the number of modifiers left.
 */
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

/* -------------------------------------------------------------------- */
/** \name Crazy-Space API
 * \{ */

void BKE_crazyspace_api_eval(struct Depsgraph *depsgraph,
                             struct Scene *scene,
                             struct Object *object,
                             struct ReportList *reports);

void BKE_crazyspace_api_displacement_to_deformed(struct Object *object,
                                                 struct ReportList *reports,
                                                 int vertex_index,
                                                 const float displacement[3],
                                                 float r_displacement_deformed[3]);

void BKE_crazyspace_api_displacement_to_original(struct Object *object,
                                                 struct ReportList *reports,
                                                 int vertex_index,
                                                 const float displacement_deformed[3],
                                                 float r_displacement[3]);

void BKE_crazyspace_api_eval_clear(struct Object *object);

/** \} */

#ifdef __cplusplus
}
#endif
