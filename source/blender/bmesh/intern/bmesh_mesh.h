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
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_MESH_H__
#define __BMESH_MESH_H__

/** \file blender/bmesh/intern/bmesh_mesh.h
 *  \ingroup bmesh
 */

struct BMAllocTemplate;
struct MLoopNorSpaceArray;

void   BM_mesh_elem_toolflags_ensure(BMesh *bm);
void   BM_mesh_elem_toolflags_clear(BMesh *bm);

struct BMeshCreateParams {
	uint use_toolflags : 1;
};

BMesh *BM_mesh_create(
        const struct BMAllocTemplate *allocsize,
        const struct BMeshCreateParams *params);

void   BM_mesh_free(BMesh *bm);
void   BM_mesh_data_free(BMesh *bm);
void   BM_mesh_clear(BMesh *bm);

void BM_mesh_normals_update(BMesh *bm);
void BM_verts_calc_normal_vcos(BMesh *bm, const float (*fnos)[3], const float (*vcos)[3], float (*vnos)[3]);
void BM_loops_calc_normal_vcos(
        BMesh *bm, const float(*vcos)[3], const float(*vnos)[3], const float(*pnos)[3],
        const bool use_split_normals, const float split_angle, float(*r_lnos)[3],
        struct MLoopNorSpaceArray *r_lnors_spacearr, short (*clnors_data)[2],
        const int cd_loop_clnors_offset);

bool BM_loop_check_cyclic_smooth_fan(BMLoop *l_curr);


void BM_edges_sharp_from_angle_set(BMesh *bm, const float split_angle);

void bmesh_edit_begin(BMesh *bm, const BMOpTypeFlag type_flag);
void bmesh_edit_end(BMesh *bm, const BMOpTypeFlag type_flag);

void BM_mesh_elem_index_ensure(BMesh *bm, const char hflag);
void BM_mesh_elem_index_validate(
        BMesh *bm, const char *location, const char *func,
        const char *msg_a, const char *msg_b);

void BM_mesh_toolflags_set(BMesh *bm, bool use_toolflags);

#ifndef NDEBUG
bool BM_mesh_elem_table_check(BMesh *bm);
#endif

void           BM_mesh_elem_table_ensure(BMesh *bm, const char htype);
void           BM_mesh_elem_table_init(BMesh *bm, const char htype);
void           BM_mesh_elem_table_free(BMesh *bm, const char htype);

BLI_INLINE BMVert *BM_vert_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totvert));
	BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
	return bm->vtable[index];
}
BLI_INLINE BMEdge *BM_edge_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totedge));
	BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
	return bm->etable[index];
}
BLI_INLINE BMFace *BM_face_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totface));
	BLI_assert((bm->elem_table_dirty & BM_FACE) == 0);
	return bm->ftable[index];
}

BMVert *BM_vert_at_index_find(BMesh *bm, const int index);
BMEdge *BM_edge_at_index_find(BMesh *bm, const int index);
BMFace *BM_face_at_index_find(BMesh *bm, const int index);

BMVert *BM_vert_at_index_find_or_table(BMesh *bm, const int index);
BMEdge *BM_edge_at_index_find_or_table(BMesh *bm, const int index);
BMFace *BM_face_at_index_find_or_table(BMesh *bm, const int index);

// XXX

int  BM_mesh_elem_count(BMesh *bm, const char htype);

void BM_mesh_remap(
        BMesh *bm,
        const uint *vert_idx,
        const uint *edge_idx,
        const uint *face_idx);

void BM_mesh_rebuild(
        BMesh *bm, const struct BMeshCreateParams *params,
        struct BLI_mempool *vpool, struct BLI_mempool *epool, struct BLI_mempool *lpool, struct BLI_mempool *fpool);

typedef struct BMAllocTemplate {
	int totvert, totedge, totloop, totface;
} BMAllocTemplate;

extern const BMAllocTemplate bm_mesh_allocsize_default;
extern const BMAllocTemplate bm_mesh_chunksize_default;

#define BMALLOC_TEMPLATE_FROM_BM(bm) { (CHECK_TYPE_INLINE(bm, BMesh *), \
	(bm)->totvert), (bm)->totedge, (bm)->totloop, (bm)->totface}
#define BMALLOC_TEMPLATE_FROM_ME(me) { (CHECK_TYPE_INLINE(me, Mesh *), \
	(me)->totvert), (me)->totedge, (me)->totloop, (me)->totpoly}

#define _VA_BMALLOC_TEMPLATE_FROM_DM_1(dm) { \
	(CHECK_TYPE_INLINE(dm, DerivedMesh *), \
	(dm)->getNumVerts(dm)),		\
	(dm)->getNumEdges(dm),		\
	(dm)->getNumLoops(dm),		\
	(dm)->getNumPolys(dm),		\
}
#define _VA_BMALLOC_TEMPLATE_FROM_DM_2(dm_a, dm_b) { \
	(CHECK_TYPE_INLINE(dm_a, DerivedMesh *), \
	 CHECK_TYPE_INLINE(dm_b, DerivedMesh *), \
	(dm_a)->getNumVerts(dm_a)) + (dm_b)->getNumVerts(dm_b),	\
	(dm_a)->getNumEdges(dm_a)  + (dm_b)->getNumEdges(dm_b),	\
	(dm_a)->getNumLoops(dm_a)  + (dm_b)->getNumLoops(dm_b),	\
	(dm_a)->getNumPolys(dm_a)  + (dm_b)->getNumPolys(dm_b),	\
}

#define BMALLOC_TEMPLATE_FROM_DM(...) VA_NARGS_CALL_OVERLOAD(_VA_BMALLOC_TEMPLATE_FROM_DM_, __VA_ARGS__)

#endif /* __BMESH_MESH_H__ */
