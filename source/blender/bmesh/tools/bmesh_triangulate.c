/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_triangulate.c
 *  \ingroup bmesh
 *
 * Triangulate.
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_triangulate.h"  /* own include */

void BM_mesh_triangulate(BMesh *bm, const bool use_beauty, const bool tag_only)
{
	BMIter iter;
	BMFace *face;

	if (tag_only == false) {
		BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
			if (face->len > 3) {
				BM_face_triangulate(bm, face, NULL, use_beauty, false);
			}
		}
	}
	else {
		BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(face, BM_ELEM_TAG)) {
				if (face->len > 3) {
					BM_face_triangulate(bm, face, NULL, use_beauty, true);
				}
			}
		}
	}
}
