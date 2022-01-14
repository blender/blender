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
 * \param use_snap_center: Snap verts onto the plane.
 * \param use_tag: Only bisect tagged edges and faces.
 * \param oflag_center: Operator flag, enabled for geometry on the axis (existing and created)
 */
void BM_mesh_bisect_plane(BMesh *bm,
                          const float plane[4],
                          bool use_snap_center,
                          bool use_tag,
                          short oflag_center,
                          short oflag_new,
                          float eps);
