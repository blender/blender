/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"

struct BMEditMesh;

namespace blender::bke {

struct EditMeshData {
  /** when set, \a vertexNos, faceNos are lazy initialized */
  Array<float3> vertexCos;

  /** lazy initialize (when \a vertexCos is set) */
  Array<float3> vertexNos;
  Array<float3> faceNos;
  /** also lazy init but don't depend on \a vertexCos */
  Array<float3> faceCos;
};

}  // namespace blender::bke

void BKE_editmesh_cache_ensure_face_normals(BMEditMesh *em, blender::bke::EditMeshData *emd);
void BKE_editmesh_cache_ensure_vert_normals(BMEditMesh *em, blender::bke::EditMeshData *emd);

void BKE_editmesh_cache_ensure_face_centers(BMEditMesh *em, blender::bke::EditMeshData *emd);

bool BKE_editmesh_cache_calc_minmax(BMEditMesh *em,
                                    blender::bke::EditMeshData *emd,
                                    float min[3],
                                    float max[3]);
