/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
 * \param use_verts: Use flagged verts instead of edges.
 * \param tag_only: Only split tagged edges.
 * \param copy_select: Copy selection history.
 */
void BM_mesh_edgesplit(BMesh *bm, bool use_verts, bool tag_only, bool copy_select);

#ifdef __cplusplus
}
#endif
