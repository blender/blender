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

/** \file
 * \ingroup bmesh
 *
 * Triangulate.
 */

#pragma once

void BM_mesh_triangulate(BMesh *bm,
                         const int quad_method,
                         const int ngon_method,
                         const int min_vertices,
                         const bool tag_only,
                         BMOperator *op,
                         BMOpSlot *slot_facemap_out,
                         BMOpSlot *slot_doubles_out);
