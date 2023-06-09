/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_mesh.hh"
#include "BKE_subdiv_ccg.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

struct CCGMaterialFromMeshData {
  const Mesh *mesh;
  const bool *sharp_faces;
  const int *material_indices;
};

static DMFlagMat subdiv_ccg_material_flags_eval(
    SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator, const int coarse_face_index)
{
  CCGMaterialFromMeshData *data = (CCGMaterialFromMeshData *)material_flags_evaluator->user_data;
  BLI_assert(coarse_face_index < data->mesh->totpoly);
  DMFlagMat material_flags;
  material_flags.sharp = data->sharp_faces && data->sharp_faces[coarse_face_index];
  material_flags.mat_nr = data->material_indices ? data->material_indices[coarse_face_index] : 0;
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
  CCGMaterialFromMeshData *data = static_cast<CCGMaterialFromMeshData *>(
      MEM_mallocN(sizeof(CCGMaterialFromMeshData), __func__));
  data->mesh = mesh;
  data->material_indices = (const int *)CustomData_get_layer_named(
      &mesh->pdata, CD_PROP_INT32, "material_index");
  data->sharp_faces = (const bool *)CustomData_get_layer_named(
      &mesh->pdata, CD_PROP_BOOL, "sharp_face");
  material_flags_evaluator->eval_material_flags = subdiv_ccg_material_flags_eval;
  material_flags_evaluator->free = subdiv_ccg_material_flags_free;
  material_flags_evaluator->user_data = data;
}
