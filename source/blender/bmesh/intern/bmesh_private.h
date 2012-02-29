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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_PRIVATE_H__
#define __BMESH_PRIVATE_H__

/** \file blender/bmesh/intern/bmesh_private.h
 *  \ingroup bmesh
 *
 *  Private function prototypes for bmesh public API.
 *  This file is a grab-bag of functions from various
 *  parts of the bmesh internals.
 */

/* returns positive nonzero on error */
int bmesh_elem_check(BMesh *bm, void *element, const char htype);

#define BM_CHECK_ELEMENT(bm, el)                                              \
    if (bmesh_elem_check(bm, el, ((BMHeader *)el)->htype)) {                  \
        printf("check_element failure, with code %i on line %i in file\n"     \
        "    \"%s\"\n\n",                                                     \
        bmesh_elem_check(bm, el, ((BMHeader *)el)->htype),                    \
        __LINE__, __FILE__);                                                  \
    }

#define BM_DISK_EDGE_LINK_GET(e, v)  (                                        \
	((v) == ((BMEdge *)(e))->v1) ?                                            \
		&((e)->v1_disk_link) :                                                \
		&((e)->v2_disk_link)                                                  \
    )

int bmesh_radial_length(BMLoop *l);
int bmesh_disk_count(BMVert *v);

/* NOTE: ensure different parts of the API do not conflict
 * on using these internal flags!*/
#define _FLAG_JF	1 /* join faces */
#define _FLAG_MF	2 /* make face */

#define BM_ELEM_API_FLAG_ENABLE(element, f)  ((element)->oflags[0].pflag |=  (f))
#define BM_ELEM_API_FLAG_DISABLE(element, f) ((element)->oflags[0].pflag &= ~(f))
#define BM_ELEM_API_FLAG_TEST(element, f)    ((element)->oflags[0].pflag &   (f))

void bmesh_face_normal_update(BMesh *bm, BMFace *f, float no[3],
                              float (*projectverts)[3]);
void bmesh_face_normal_update_vertex_cos(BMesh *bm, BMFace *f, float no[3],
                                         float (*projectverts)[3], float (*vertexCos)[3]);

void compute_poly_plane(float (*verts)[3], int nverts);
void poly_rotate_plane(const float normal[3], float (*verts)[3], const int nverts);

/* include the rest of our private declarations */
#include "bmesh_structure.h"
#include "bmesh_operators_private.h"

#endif /* __BMESH_PRIVATE_H__ */
