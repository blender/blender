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
