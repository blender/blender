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

void BMO_mesh_delete_oflag_tagged(BMesh *bm, short oflag, char htype);
void BM_mesh_delete_hflag_tagged(BMesh *bm, char hflag, char htype);

/**
 * \warning oflag applies to different types in some contexts,
 * not just the type being removed.
 */
void BMO_mesh_delete_oflag_context(BMesh *bm, short oflag, int type);
/**
 * \warning oflag applies to different types in some contexts,
 * not just the type being removed.
 */
void BM_mesh_delete_hflag_context(BMesh *bm, char hflag, int type);
