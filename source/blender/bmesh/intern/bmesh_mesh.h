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

#ifndef __BMESH_MESH_H__
#define __BMESH_MESH_H__

/** \file
 * \ingroup bmesh
 */

struct BMAllocTemplate;
struct BMLoopNorEditDataArray;
struct MLoopNorSpaceArray;

void BM_mesh_elem_toolflags_ensure(BMesh *bm);
void BM_mesh_elem_toolflags_clear(BMesh *bm);

struct BMeshCreateParams {
  uint use_toolflags : 1;
};

BMesh *BM_mesh_create(const struct BMAllocTemplate *allocsize,
                      const struct BMeshCreateParams *params);

void BM_mesh_free(BMesh *bm);
void BM_mesh_data_free(BMesh *bm);
void BM_mesh_clear(BMesh *bm);

void BM_mesh_normals_update(BMesh *bm);
void BM_verts_calc_normal_vcos(BMesh *bm,
                               const float (*fnos)[3],
                               const float (*vcos)[3],
                               float (*vnos)[3]);
void BM_loops_calc_normal_vcos(BMesh *bm,
                               const float (*vcos)[3],
                               const float (*vnos)[3],
                               const float (*pnos)[3],
                               const bool use_split_normals,
                               const float split_angle,
                               float (*r_lnos)[3],
                               struct MLoopNorSpaceArray *r_lnors_spacearr,
                               short (*clnors_data)[2],
                               const int cd_loop_clnors_offset,
                               const bool do_rebuild);

bool BM_loop_check_cyclic_smooth_fan(BMLoop *l_curr);
void BM_lnorspacearr_store(BMesh *bm, float (*r_lnors)[3]);
void BM_lnorspace_invalidate(BMesh *bm, const bool do_invalidate_all);
void BM_lnorspace_rebuild(BMesh *bm, bool preserve_clnor);
void BM_lnorspace_update(BMesh *bm);
void BM_normals_loops_edges_tag(BMesh *bm, const bool do_edges);
#ifndef NDEBUG
void BM_lnorspace_err(BMesh *bm);
#endif

/* Loop Generics */
struct BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init(BMesh *bm,
                                                                  const bool do_all_loops_of_vert);
void BM_loop_normal_editdata_array_free(struct BMLoopNorEditDataArray *lnors_ed_arr);
int BM_total_loop_select(BMesh *bm);

void BM_edges_sharp_from_angle_set(BMesh *bm, const float split_angle);

void bmesh_edit_begin(BMesh *bm, const BMOpTypeFlag type_flag);
void bmesh_edit_end(BMesh *bm, const BMOpTypeFlag type_flag);

void BM_mesh_elem_index_ensure_ex(BMesh *bm, const char htype, int elem_offset[4]);
void BM_mesh_elem_index_ensure(BMesh *bm, const char hflag);
void BM_mesh_elem_index_validate(
    BMesh *bm, const char *location, const char *func, const char *msg_a, const char *msg_b);

void BM_mesh_toolflags_set(BMesh *bm, bool use_toolflags);

#ifndef NDEBUG
bool BM_mesh_elem_table_check(BMesh *bm);
#endif

void BM_mesh_elem_table_ensure(BMesh *bm, const char htype);
void BM_mesh_elem_table_init(BMesh *bm, const char htype);
void BM_mesh_elem_table_free(BMesh *bm, const char htype);

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

int BM_mesh_elem_count(BMesh *bm, const char htype);

void BM_mesh_remap(BMesh *bm, const uint *vert_idx, const uint *edge_idx, const uint *face_idx);

void BM_mesh_rebuild(BMesh *bm,
                     const struct BMeshCreateParams *params,
                     struct BLI_mempool *vpool,
                     struct BLI_mempool *epool,
                     struct BLI_mempool *lpool,
                     struct BLI_mempool *fpool);

typedef struct BMAllocTemplate {
  int totvert, totedge, totloop, totface;
} BMAllocTemplate;

extern const BMAllocTemplate bm_mesh_allocsize_default;
extern const BMAllocTemplate bm_mesh_chunksize_default;

#define BMALLOC_TEMPLATE_FROM_BM(bm) \
  { \
    (CHECK_TYPE_INLINE(bm, BMesh *), (bm)->totvert), (bm)->totedge, (bm)->totloop, (bm)->totface \
  }

#define _VA_BMALLOC_TEMPLATE_FROM_ME_1(me) \
  { \
    (CHECK_TYPE_INLINE(me, Mesh *), (me)->totvert), (me)->totedge, (me)->totloop, (me)->totpoly, \
  }
#define _VA_BMALLOC_TEMPLATE_FROM_ME_2(me_a, me_b) \
  { \
    (CHECK_TYPE_INLINE(me_a, Mesh *), \
     CHECK_TYPE_INLINE(me_b, Mesh *), \
     (me_a)->totvert + (me_b)->totvert), \
        (me_a)->totedge + (me_b)->totedge, (me_a)->totloop + (me_b)->totloop, \
        (me_a)->totpoly + (me_b)->totpoly, \
  }
#define BMALLOC_TEMPLATE_FROM_ME(...) \
  VA_NARGS_CALL_OVERLOAD(_VA_BMALLOC_TEMPLATE_FROM_ME_, __VA_ARGS__)

#endif /* __BMESH_MESH_H__ */
