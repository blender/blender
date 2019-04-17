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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_ccg.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"

typedef struct CCGMaterialFromMeshData {
  const Mesh *mesh;
} CCGMaterialFromMeshData;

static DMFlagMat subdiv_ccg_material_flags_eval(
    SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator, const int coarse_face_index)
{
  CCGMaterialFromMeshData *data = (CCGMaterialFromMeshData *)material_flags_evaluator->user_data;
  const Mesh *mesh = data->mesh;
  BLI_assert(coarse_face_index < mesh->totpoly);
  const MPoly *mpoly = mesh->mpoly;
  const MPoly *poly = &mpoly[coarse_face_index];
  DMFlagMat material_flags;
  material_flags.flag = poly->flag;
  material_flags.mat_nr = poly->mat_nr;
  return material_flags;
}

static void subdiv_ccg_material_flags_free(
    SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator)
{
  MEM_freeN(material_flags_evaluator->user_data);
}

void BKE_subdiv_ccg_material_flags_init_from_mesh(
    SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator, const Mesh *mesh)
{
  CCGMaterialFromMeshData *data = MEM_mallocN(sizeof(CCGMaterialFromMeshData),
                                              "ccg material eval");
  data->mesh = mesh;
  material_flags_evaluator->eval_material_flags = subdiv_ccg_material_flags_eval;
  material_flags_evaluator->free = subdiv_ccg_material_flags_free;
  material_flags_evaluator->user_data = data;
}
