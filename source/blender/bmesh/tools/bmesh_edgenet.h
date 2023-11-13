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
 * Fill in faces from an edgenet made up of boundary and wire edges.
 *
 * \note New faces currently don't have their normals calculated and are flipped randomly.
 *       The caller needs to flip faces correctly.
 *
 * \param bm: The mesh to operate on.
 * \param use_edge_tag: Only fill tagged edges.
 */
void BM_mesh_edgenet(BMesh *bm, bool use_edge_tag, bool use_new_face_tag);

#ifdef __cplusplus
}
#endif
