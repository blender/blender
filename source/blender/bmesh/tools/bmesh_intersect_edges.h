/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool BM_mesh_intersect_edges(
    BMesh *bm, char hflag, float dist, bool split_faces, GHash *r_targetmap);

#ifdef __cplusplus
}
#endif
