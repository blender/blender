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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): (mar-2001 nzc)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MESH_MAPPING_H__
#define __BKE_MESH_MAPPING_H__

/** \file BKE_mesh_mapping.h
 *  \ingroup bke
 */

struct MPoly;
struct MEdge;
struct MLoop;
struct MLoopUV;

/* map from uv vertex to face (for select linked, stitch, uv suburf) */

/* UvVertMap */
#define STD_UV_CONNECT_LIMIT  0.0001f

typedef struct UvVertMap {
	struct UvMapVert **vert;
	struct UvMapVert *buf;
} UvVertMap;

typedef struct UvMapVert {
	struct UvMapVert *next;
	unsigned int f;
	unsigned char tfindex, separate, flag;
} UvMapVert;

/* UvElement stores per uv information so that we can quickly access information for a uv.
 * it is actually an improved UvMapVert, including an island and a direct pointer to the face
 * to avoid initializing face arrays */
typedef struct UvElement {
	/* Next UvElement corresponding to same vertex */
	struct UvElement *next;
	/* Face the element belongs to */
	struct BMLoop *l;
	/* index in loop. */
	unsigned short tfindex;
	/* Whether this element is the first of coincident elements */
	unsigned char separate;
	/* general use flag */
	unsigned char flag;
	/* If generating element map with island sorting, this stores the island index */
	unsigned short island;
} UvElement;


/* UvElementMap is a container for UvElements of a mesh. It stores some UvElements belonging to the
 * same uv island in sequence and the number of uvs per island so it is possible to access all uvs
 * belonging to an island directly by iterating through the buffer.
 */
typedef struct UvElementMap {
	/* address UvElements by their vertex */
	struct UvElement **vert;
	/* UvElement Store */
	struct UvElement *buf;
	/* Total number of UVs in the layer. Useful to know */
	int totalUVs;
	/* Number of Islands in the mesh */
	int totalIslands;
	/* Stores the starting index in buf where each island begins */
	int *islandIndices;
} UvElementMap;

/* invalid island index is max short. If any one has the patience
 * to make that many islands, he can bite me :p */
#define INVALID_ISLAND 0xFFFF

/* Connectivity data */
typedef struct MeshElemMap {
	int *indices;
	int count;
} MeshElemMap;

/* mapping */
UvVertMap *BKE_mesh_uv_vert_map_create(
        struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv,
        unsigned int totpoly, unsigned int totvert, int selected, float *limit);
UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v);
void       BKE_mesh_uv_vert_map_free(UvVertMap *vmap);

void BKE_mesh_vert_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MPoly *mface, const struct MLoop *mloop,
        int totvert, int totface, int totloop);
void BKE_mesh_vert_edge_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, int totvert, int totedge);
void BKE_mesh_edge_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, const int totedge,
        const struct MPoly *mpoly, const int totpoly,
        const struct MLoop *mloop, const int totloop);
void BKE_mesh_origindex_map_create(
        MeshElemMap **r_map, int **r_mem,
        const int totorig,
        const int *final_origindex, const int totfinal);

/* smoothgroups */
int *BKE_mesh_calc_smoothgroups(
        const struct MEdge *medge, const int totedge,
        const struct MPoly *mpoly, const int totpoly,
        const struct MLoop *mloop, const int totloop,
        int *r_totgroup, const bool use_bitflags);

/* No good (portable) way to have exported inlined functions... */
#define BKE_MESH_TESSFACE_VINDEX_ORDER(_mf, _v)  (                          \
    (CHECK_TYPE_INLINE(_mf, MFace *),                                       \
     CHECK_TYPE_INLINE(&(_v), unsigned int *)),                             \
    ((_mf->v1 == _v) ? 0 :                                                  \
     (_mf->v2 == _v) ? 1 :                                                  \
     (_mf->v3 == _v) ? 2 :                                                  \
     (_mf->v4 && _mf->v4 == _v) ? 3 : -1)                                   \
    )

#endif  /* __BKE_MESH_MAPPING_H__ */
