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

struct CurveProfile;
struct MDeformVert;

/**
 * - Currently only bevels BM_ELEM_TAG'd verts and edges.
 *
 * - Newly created faces, edges, and verts are BM_ELEM_TAG'd too,
 *   the caller needs to ensure these are cleared before calling
 *   if its going to use this tag.
 *
 * - If limit_offset is set, adjusts offset down if necessary
 *   to avoid geometry collisions.
 *
 * \warning all tagged edges _must_ be manifold.
 */
void BM_mesh_bevel(BMesh *bm,
                   float offset,
                   int offset_type,
                   int profile_type,
                   int segments,
                   float profile,
                   bool affect_type,
                   bool use_weights,
                   bool limit_offset,
                   const struct MDeformVert *dvert,
                   int vertex_group,
                   int mat,
                   bool loop_slide,
                   bool mark_seam,
                   bool mark_sharp,
                   bool harden_normals,
                   int face_strength_mode,
                   int miter_outer,
                   int miter_inner,
                   float spread,
                   float smoothresh,
                   const struct CurveProfile *custom_profile,
                   int vmesh_method);

#ifdef __cplusplus
}
#endif
