/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;

typedef struct EditMeshData {
  /** when set, \a vertexNos, polyNos are lazy initialized */
  float (*vertexCos)[3];

  /** lazy initialize (when \a vertexCos is set) */
  float const (*vertexNos)[3];
  float const (*polyNos)[3];
  /** also lazy init but don't depend on \a vertexCos */
  const float (*polyCos)[3];
} EditMeshData;

void BKE_editmesh_cache_ensure_poly_normals(struct BMEditMesh *em, EditMeshData *emd);
void BKE_editmesh_cache_ensure_vert_normals(struct BMEditMesh *em, EditMeshData *emd);

void BKE_editmesh_cache_ensure_poly_centers(struct BMEditMesh *em, EditMeshData *emd);

bool BKE_editmesh_cache_calc_minmax(struct BMEditMesh *em,
                                    EditMeshData *emd,
                                    float min[3],
                                    float max[3]);

#ifdef __cplusplus
}
#endif
