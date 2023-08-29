/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Take a face-region and return a list of matching face-regions.
 *
 * \param faces_region: A single, contiguous face-region.
 * \return A list of matching null-terminated face-region arrays.
 */
int BM_mesh_region_match(BMesh *bm,
                         BMFace **faces_region,
                         uint faces_region_len,
                         ListBase *r_face_regions);

#ifdef __cplusplus
}
#endif
