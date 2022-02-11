/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;
struct EditMeshData;

void BKE_editmesh_cache_ensure_poly_normals(struct BMEditMesh *em, struct EditMeshData *emd);
void BKE_editmesh_cache_ensure_vert_normals(struct BMEditMesh *em, struct EditMeshData *emd);

void BKE_editmesh_cache_ensure_poly_centers(struct BMEditMesh *em, struct EditMeshData *emd);

bool BKE_editmesh_cache_calc_minmax(struct BMEditMesh *em,
                                    struct EditMeshData *emd,
                                    float min[3],
                                    float max[3]);

#ifdef __cplusplus
}
#endif
