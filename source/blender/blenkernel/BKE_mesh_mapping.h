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

struct MVert;
struct MEdge;
struct MPoly;
struct MLoop;
struct MLoopUV;
struct MLoopTri;

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
	unsigned int island;
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

#define INVALID_ISLAND ((unsigned int)-1)

/* Connectivity data */
typedef struct MeshElemMap {
	int *indices;
	int count;
} MeshElemMap;

/* mapping */
UvVertMap *BKE_mesh_uv_vert_map_create(
        struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv,
        unsigned int totpoly, unsigned int totvert,
        const float limit[2], const bool selected, const bool use_winding);
UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v);
void       BKE_mesh_uv_vert_map_free(UvVertMap *vmap);

void BKE_mesh_vert_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MPoly *mface, const struct MLoop *mloop,
        int totvert, int totface, int totloop);
void BKE_mesh_vert_loop_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MPoly *mface, const struct MLoop *mloop,
        int totvert, int totface, int totloop);
void BKE_mesh_vert_looptri_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MVert *mvert, const int totvert,
        const struct MLoopTri *mlooptri, const int totlooptri,
        const struct MLoop *mloop, const int totloop);
void BKE_mesh_vert_edge_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, int totvert, int totedge);
void BKE_mesh_vert_edge_vert_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, int totvert, int totedge);
void BKE_mesh_edge_loop_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, const int totedge,
        const struct MPoly *mpoly, const int totpoly,
        const struct MLoop *mloop, const int totloop);
void BKE_mesh_edge_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const struct MEdge *medge, const int totedge,
        const struct MPoly *mpoly, const int totpoly,
        const struct MLoop *mloop, const int totloop);
void BKE_mesh_origindex_map_create(
        MeshElemMap **r_map, int **r_mem,
        const int totorig,
        const int *final_origindex, const int totfinal);
void BKE_mesh_origindex_map_create_looptri(
        MeshElemMap **r_map, int **r_mem,
        const struct MPoly *mpoly, const int mpoly_num,
        const struct MLoopTri *looptri, const int looptri_num);

/* islands */

/* Loop islands data helpers. */
enum {
	MISLAND_TYPE_NONE = 0,
	MISLAND_TYPE_VERT = 1,
	MISLAND_TYPE_EDGE = 2,
	MISLAND_TYPE_POLY = 3,
	MISLAND_TYPE_LOOP = 4,
};

typedef struct MeshIslandStore {
	short item_type;      /* MISLAND_TYPE_... */
	short island_type;    /* MISLAND_TYPE_... */
	short innercut_type;  /* MISLAND_TYPE_... */

	int  items_to_islands_num;
	int *items_to_islands;  /* map the item to the island index */

	int                  islands_num;
	size_t               islands_num_alloc;
	struct MeshElemMap **islands;    /* Array of pointers, one item per island. */
	struct MeshElemMap **innercuts;  /* Array of pointers, one item per island. */

	struct MemArena *mem;  /* Memory arena, internal use only. */
} MeshIslandStore;

void BKE_mesh_loop_islands_init(
        MeshIslandStore *island_store,
        const short item_type, const int item_num, const short island_type, const short innercut_type);
void BKE_mesh_loop_islands_clear(MeshIslandStore *island_store);
void BKE_mesh_loop_islands_free(MeshIslandStore *island_store);
void BKE_mesh_loop_islands_add(
        MeshIslandStore *islands, const int item_num, int *item_indices,
        const int num_island_items, int *island_item_indices,
        const int num_innercut_items, int *innercut_item_indices);

typedef bool (*MeshRemapIslandsCalc)(
        struct MVert *verts, const int totvert,
        struct MEdge *edges, const int totedge,
        struct MPoly *polys, const int totpoly,
        struct MLoop *loops, const int totloop,
        struct MeshIslandStore *r_island_store);

/* Above vert/UV mapping stuff does not do what we need here, but does things we do not need here.
 * So better keep them separated for now, I think.
 */
bool BKE_mesh_calc_islands_loop_poly_edgeseam(
        struct MVert *verts, const int totvert,
        struct MEdge *edges, const int totedge,
        struct MPoly *polys, const int totpoly,
        struct MLoop *loops, const int totloop,
        MeshIslandStore *r_island_store);

bool BKE_mesh_calc_islands_loop_poly_uvmap(
        struct MVert *verts, const int totvert,
        struct MEdge *edges, const int totedge,
        struct MPoly *polys, const int totpoly,
        struct MLoop *loops, const int totloop,
        const struct MLoopUV *luvs,
        MeshIslandStore *r_island_store);

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

/* use on looptri vertex values */
#define BKE_MESH_TESSTRI_VINDEX_ORDER(_tri, _v)  (                          \
    (CHECK_TYPE_ANY(_tri, unsigned int *, int *, int[3],                    \
                          const unsigned int *, const int *, const int[3]), \
     CHECK_TYPE_ANY(_v, unsigned int, const unsigned int, int, const int)), \
    (((_tri)[0] == _v) ? 0 :                                                \
     ((_tri)[1] == _v) ? 1 :                                                \
     ((_tri)[2] == _v) ? 2 : -1)                                            \
    )

#endif  /* __BKE_MESH_MAPPING_H__ */
