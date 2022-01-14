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
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

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
