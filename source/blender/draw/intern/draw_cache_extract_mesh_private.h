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
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#pragma once

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_editmesh.h"

#include "draw_cache_extract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eMRExtractType {
  MR_EXTRACT_BMESH,
  MR_EXTRACT_MAPPED,
  MR_EXTRACT_MESH,
} eMRExtractType;

typedef struct MeshRenderData {
  eMRExtractType extract_type;

  int poly_len, edge_len, vert_len, loop_len;
  int edge_loose_len;
  int vert_loose_len;
  int loop_loose_len;
  int tri_len;
  int mat_len;

  bool use_hide;
  bool use_subsurf_fdots;
  bool use_final_mesh;

  /** Use for #MeshStatVis calculation which use world-space coords. */
  float obmat[4][4];

  const ToolSettings *toolsettings;
  /** Edit Mesh */
  BMEditMesh *edit_bmesh;
  BMesh *bm;
  EditMeshData *edit_data;

  /* For deformed edit-mesh data. */
  /* Use for #ME_WRAPPER_TYPE_BMESH. */
  const float (*bm_vert_coords)[3];
  const float (*bm_vert_normals)[3];
  const float (*bm_poly_normals)[3];
  const float (*bm_poly_centers)[3];

  int *v_origindex, *e_origindex, *p_origindex;
  int crease_ofs;
  int bweight_ofs;
  int freestyle_edge_ofs;
  int freestyle_face_ofs;
  /** Mesh */
  Mesh *me;
  const MVert *mvert;
  const MEdge *medge;
  const MLoop *mloop;
  const MPoly *mpoly;
  BMVert *eve_act;
  BMEdge *eed_act;
  BMFace *efa_act;
  BMFace *efa_act_uv;
  /* Data created on-demand (usually not for #BMesh based data). */
  MLoopTri *mlooptri;
  float (*loop_normals)[3];
  float (*poly_normals)[3];
  int *lverts, *ledges;
} MeshRenderData;

BLI_INLINE BMFace *bm_original_face_get(const MeshRenderData *mr, int idx)
{
  return ((mr->p_origindex != NULL) && (mr->p_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_face_at_index(mr->bm, mr->p_origindex[idx]) :
             NULL;
}

BLI_INLINE BMEdge *bm_original_edge_get(const MeshRenderData *mr, int idx)
{
  return ((mr->e_origindex != NULL) && (mr->e_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_edge_at_index(mr->bm, mr->e_origindex[idx]) :
             NULL;
}

BLI_INLINE BMVert *bm_original_vert_get(const MeshRenderData *mr, int idx)
{
  return ((mr->v_origindex != NULL) && (mr->v_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_vert_at_index(mr->bm, mr->v_origindex[idx]) :
             NULL;
}

BLI_INLINE const float *bm_vert_co_get(const MeshRenderData *mr, const BMVert *eve)
{
  const float(*vert_coords)[3] = mr->bm_vert_coords;
  if (vert_coords != NULL) {
    return vert_coords[BM_elem_index_get(eve)];
  }

  UNUSED_VARS(mr);
  return eve->co;
}

BLI_INLINE const float *bm_vert_no_get(const MeshRenderData *mr, const BMVert *eve)
{
  const float(*vert_normals)[3] = mr->bm_vert_normals;
  if (vert_normals != NULL) {
    return vert_normals[BM_elem_index_get(eve)];
  }

  UNUSED_VARS(mr);
  return eve->no;
}

BLI_INLINE const float *bm_face_no_get(const MeshRenderData *mr, const BMFace *efa)
{
  const float(*poly_normals)[3] = mr->bm_poly_normals;
  if (poly_normals != NULL) {
    return poly_normals[BM_elem_index_get(efa)];
  }

  UNUSED_VARS(mr);
  return efa->no;
}

/* TODO(jbakker): phase out batch iteration macros as they are only used once. */
/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract: Loop Triangles
 * \{ */

typedef struct ExtractTriBMesh_Params {
  BMLoop *(*looptris)[3];
  int tri_range[2];
} ExtractTriBMesh_Params;
typedef void(ExtractTriBMeshFn)(const MeshRenderData *mr,
                                BMLoop **elt,
                                const int elt_index,
                                void *data);

#define EXTRACT_TRIS_LOOPTRI_FOREACH_BM_BEGIN(elem_tri, index_tri, params) \
  CHECK_TYPE(params, const ExtractTriBMesh_Params *); \
  { \
    const int _tri_index_end = (params)->tri_range[1]; \
    BMLoop **elem_tri = (params)->looptris[(params)->tri_range[0]]; \
    for (int index_tri = (params)->tri_range[0]; index_tri < _tri_index_end; \
         index_tri += 1, elem_tri += 3)
#define EXTRACT_TRIS_LOOPTRI_FOREACH_BM_END }

typedef struct ExtractTriMesh_Params {
  const MLoopTri *mlooptri;
  int tri_range[2];
} ExtractTriMesh_Params;
typedef void(ExtractTriMeshFn)(const MeshRenderData *mr,
                               const MLoopTri *mlt,
                               const int elt_index,
                               void *data);

#define EXTRACT_TRIS_LOOPTRI_FOREACH_MESH_BEGIN(elem_tri, index_tri, params) \
  CHECK_TYPE(params, const ExtractTriMesh_Params *); \
  { \
    const int _tri_index_end = (params)->tri_range[1]; \
    const MLoopTri *elem_tri = &(params)->mlooptri[(params)->tri_range[0]]; \
    for (int index_tri = (params)->tri_range[0]; index_tri < _tri_index_end; \
         index_tri += 1, elem_tri += 1)
#define EXTRACT_TRIS_LOOPTRI_FOREACH_MESH_END }

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract: Polygons, Loops
 * \{ */

typedef struct ExtractPolyBMesh_Params {
  BMLoop *(*looptris)[3];
  int poly_range[2];
} ExtractPolyBMesh_Params;
typedef void(ExtractPolyBMeshFn)(const MeshRenderData *mr,
                                 BMFace *f,
                                 const int f_index,
                                 void *data);

#define EXTRACT_POLY_FOREACH_BM_BEGIN(elem_poly, index_poly, params, mr) \
  CHECK_TYPE(params, const ExtractPolyBMesh_Params *); \
  { \
    BLI_assert((mr->bm->elem_table_dirty & BM_FACE) == 0); \
    BMFace **_ftable = mr->bm->ftable; \
    const int _poly_index_end = (params)->poly_range[1]; \
    for (int index_poly = (params)->poly_range[0]; index_poly < _poly_index_end; \
         index_poly += 1) { \
      BMFace *elem_poly = _ftable[index_poly]; \
      (void)elem_poly;

#define EXTRACT_POLY_FOREACH_BM_END \
  } \
  }

/* Iterate over polygon and loop. */
#define EXTRACT_POLY_AND_LOOP_FOREACH_BM_BEGIN(elem_loop, index_loop, params, mr) \
  CHECK_TYPE(params, const ExtractPolyBMesh_Params *); \
  { \
    BLI_assert((mr->bm->elem_table_dirty & BM_FACE) == 0); \
    BMFace **_ftable = mr->bm->ftable; \
    const int _poly_index_end = (params)->poly_range[1]; \
    for (int index_poly = (params)->poly_range[0]; index_poly < _poly_index_end; \
         index_poly += 1) { \
      BMFace *elem_face = _ftable[index_poly]; \
      BMLoop *elem_loop, *l_first; \
      elem_loop = l_first = BM_FACE_FIRST_LOOP(elem_face); \
      do { \
        const int index_loop = BM_elem_index_get(elem_loop); \
        (void)index_loop; /* Quiet warning when unused. */

#define EXTRACT_POLY_AND_LOOP_FOREACH_BM_END(elem_loop) \
  } \
  while ((elem_loop = elem_loop->next) != l_first) \
    ; \
  } \
  }

typedef struct ExtractPolyMesh_Params {
  int poly_range[2];
} ExtractPolyMesh_Params;
typedef void(ExtractPolyMeshFn)(const MeshRenderData *mr,
                                const MPoly *mp,
                                const int mp_index,
                                void *data);

#define EXTRACT_POLY_FOREACH_MESH_BEGIN(elem_poly, index_poly, params, mr) \
  CHECK_TYPE(params, const ExtractPolyMesh_Params *); \
  { \
    const MPoly *_mpoly = mr->mpoly; \
    const int _poly_index_end = (params)->poly_range[1]; \
    for (int index_poly = (params)->poly_range[0]; index_poly < _poly_index_end; \
         index_poly += 1) { \
      const MPoly *elem_poly = &_mpoly[index_poly]; \
      (void)elem_poly;

#define EXTRACT_POLY_FOREACH_MESH_END \
  } \
  }

/* Iterate over polygon and loop. */
#define EXTRACT_POLY_AND_LOOP_FOREACH_MESH_BEGIN( \
    elem_poly, index_poly, elem_loop, index_loop, params, mr) \
  CHECK_TYPE(params, const ExtractPolyMesh_Params *); \
  { \
    const MPoly *_mpoly = mr->mpoly; \
    const MLoop *_mloop = mr->mloop; \
    const int _poly_index_end = (params)->poly_range[1]; \
    for (int index_poly = (params)->poly_range[0]; index_poly < _poly_index_end; \
         index_poly += 1) { \
      const MPoly *elem_poly = &_mpoly[index_poly]; \
      const int _index_end = elem_poly->loopstart + elem_poly->totloop; \
      for (int index_loop = elem_poly->loopstart; index_loop < _index_end; index_loop += 1) { \
        const MLoop *elem_loop = &_mloop[index_loop]; \
        (void)elem_loop;

#define EXTRACT_POLY_AND_LOOP_FOREACH_MESH_END \
  } \
  } \
  }

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract: Loose Edges
 * \{ */

typedef struct ExtractLEdgeBMesh_Params {
  const int *ledge;
  int ledge_range[2];
} ExtractLEdgeBMesh_Params;
typedef void(ExtractLEdgeBMeshFn)(const MeshRenderData *mr,
                                  BMEdge *eed,
                                  const int ledge_index,
                                  void *data);

#define EXTRACT_LEDGE_FOREACH_BM_BEGIN(elem_edge, index_ledge, params) \
  CHECK_TYPE(params, const ExtractLEdgeBMesh_Params *); \
  { \
    BLI_assert((mr->bm->elem_table_dirty & BM_EDGE) == 0); \
    BMEdge **_etable = mr->bm->etable; \
    const int *_ledge = (params)->ledge; \
    const int _ledge_index_end = (params)->ledge_range[1]; \
    for (int index_ledge = (params)->ledge_range[0]; index_ledge < _ledge_index_end; \
         index_ledge += 1) { \
      BMEdge *elem_edge = _etable[_ledge[index_ledge]]; \
      (void)elem_edge; /* Quiet warning when unused. */ \
      {
#define EXTRACT_LEDGE_FOREACH_BM_END \
  } \
  } \
  }

typedef struct ExtractLEdgeMesh_Params {
  const int *ledge;
  int ledge_range[2];
} ExtractLEdgeMesh_Params;
typedef void(ExtractLEdgeMeshFn)(const MeshRenderData *mr,
                                 const MEdge *med,
                                 const int ledge_index,
                                 void *data);

#define EXTRACT_LEDGE_FOREACH_MESH_BEGIN(elem_edge, index_ledge, params, mr) \
  CHECK_TYPE(params, const ExtractLEdgeMesh_Params *); \
  { \
    const MEdge *_medge = mr->medge; \
    const int *_ledge = (params)->ledge; \
    const int _ledge_index_end = (params)->ledge_range[1]; \
    for (int index_ledge = (params)->ledge_range[0]; index_ledge < _ledge_index_end; \
         index_ledge += 1) { \
      const MEdge *elem_edge = &_medge[_ledge[index_ledge]]; \
      (void)elem_edge; /* Quiet warning when unused. */ \
      {
#define EXTRACT_LEDGE_FOREACH_MESH_END \
  } \
  } \
  }

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract: Loose Vertices
 * \{ */

typedef struct ExtractLVertBMesh_Params {
  const int *lvert;
  int lvert_range[2];
} ExtractLVertBMesh_Params;
typedef void(ExtractLVertBMeshFn)(const MeshRenderData *mr,
                                  BMVert *eve,
                                  const int lvert_index,
                                  void *data);

#define EXTRACT_LVERT_FOREACH_BM_BEGIN(elem_vert, index_lvert, params) \
  CHECK_TYPE(params, const ExtractLVertBMesh_Params *); \
  { \
    BLI_assert((mr->bm->elem_table_dirty & BM_FACE) == 0); \
    BMVert **vtable = mr->bm->vtable; \
    const int *lverts = (params)->lvert; \
    const int _lvert_index_end = (params)->lvert_range[1]; \
    for (int index_lvert = (params)->lvert_range[0]; index_lvert < _lvert_index_end; \
         index_lvert += 1) { \
      BMVert *elem_vert = vtable[lverts[index_lvert]]; \
      (void)elem_vert; /* Quiet warning when unused. */ \
      {
#define EXTRACT_LVERT_FOREACH_BM_END \
  } \
  } \
  }

typedef struct ExtractLVertMesh_Params {
  const int *lvert;
  int lvert_range[2];
} ExtractLVertMesh_Params;
typedef void(ExtractLVertMeshFn)(const MeshRenderData *mr,
                                 const MVert *mv,
                                 const int lvert_index,
                                 void *data);

#define EXTRACT_LVERT_FOREACH_MESH_BEGIN(elem, index_lvert, params, mr) \
  CHECK_TYPE(params, const ExtractLVertMesh_Params *); \
  { \
    const MVert *mvert = mr->mvert; \
    const int *lverts = (params)->lvert; \
    const int _lvert_index_end = (params)->lvert_range[1]; \
    for (int index_lvert = (params)->lvert_range[0]; index_lvert < _lvert_index_end; \
         index_lvert += 1) { \
      const MVert *elem = &mvert[lverts[index_lvert]]; \
      (void)elem; /* Quiet warning when unused. */ \
      {
#define EXTRACT_LVERT_FOREACH_MESH_END \
  } \
  } \
  }

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract Struct
 * \{ */
/* TODO(jbakker): move parameters inside a struct. */
typedef void *(ExtractInitFn)(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buffer);
typedef void(ExtractFinishFn)(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buffer,
                              void *data);
typedef void *(ExtractTaskInitFn)(void *userdata);
typedef void(ExtractTaskFinishFn)(void *userdata, void *task_userdata);

typedef struct MeshExtract {
  /** Executed on main thread and return user data for iteration functions. */
  ExtractInitFn *init;
  /** Task local data. */
  ExtractTaskInitFn *task_init;
  /** Executed on one (or more if use_threading) worker thread(s). */
  ExtractTriBMeshFn *iter_looptri_bm;
  ExtractTriMeshFn *iter_looptri_mesh;
  ExtractPolyBMeshFn *iter_poly_bm;
  ExtractPolyMeshFn *iter_poly_mesh;
  ExtractLEdgeBMeshFn *iter_ledge_bm;
  ExtractLEdgeMeshFn *iter_ledge_mesh;
  ExtractLVertBMeshFn *iter_lvert_bm;
  ExtractLVertMeshFn *iter_lvert_mesh;
  /** Executed on one worker thread after all elements iterations. */
  ExtractTaskFinishFn *task_finish;
  ExtractFinishFn *finish;
  /** Used to request common data. */
  eMRDataType data_type;
  /** Used to know if the element callbacks are thread-safe and can be parallelized. */
  bool use_threading;
  /**
   * Offset in bytes of the buffer inside a MeshBufferCache instance. Points to a vertex or index
   * buffer.
   */
  size_t mesh_buffer_offset;
} MeshExtract;

/** \} */

/* draw_cache_extract_mesh_render_data.c */
MeshRenderData *mesh_render_data_create(Mesh *me,
                                        MeshBufferExtractionCache *cache,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool is_mode_active,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const ToolSettings *ts,
                                        const eMRIterType iter_type);
void mesh_render_data_free(MeshRenderData *mr);
void mesh_render_data_update_normals(MeshRenderData *mr, const eMRDataType data_flag);
void mesh_render_data_update_looptris(MeshRenderData *mr,
                                      const eMRIterType iter_type,
                                      const eMRDataType data_flag);

/* draw_cache_extract_mesh_extractors.c */
void *mesh_extract_buffer_get(const MeshExtract *extractor, MeshBufferCache *mbc);
eMRIterType mesh_extract_iter_type(const MeshExtract *ext);
const MeshExtract *mesh_extract_override_get(const MeshExtract *extractor,
                                             const bool do_hq_normals,
                                             const bool do_single_mat);

extern const MeshExtract extract_tris;
extern const MeshExtract extract_tris_single_mat;
extern const MeshExtract extract_lines;
extern const MeshExtract extract_lines_with_lines_loose;
extern const MeshExtract extract_lines_loose_only;
extern const MeshExtract extract_points;
extern const MeshExtract extract_fdots;
extern const MeshExtract extract_lines_paint_mask;
extern const MeshExtract extract_lines_adjacency;
extern const MeshExtract extract_edituv_tris;
extern const MeshExtract extract_edituv_lines;
extern const MeshExtract extract_edituv_points;
extern const MeshExtract extract_edituv_fdots;
extern const MeshExtract extract_pos_nor;
extern const MeshExtract extract_pos_nor_hq;
extern const MeshExtract extract_lnor_hq;
extern const MeshExtract extract_lnor;
extern const MeshExtract extract_uv;
extern const MeshExtract extract_tan;
extern const MeshExtract extract_tan_hq;
extern const MeshExtract extract_sculpt_data;
extern const MeshExtract extract_vcol;
extern const MeshExtract extract_orco;
extern const MeshExtract extract_edge_fac;
extern const MeshExtract extract_weights;
extern const MeshExtract extract_edit_data;
extern const MeshExtract extract_edituv_data;
extern const MeshExtract extract_edituv_stretch_area;
extern const MeshExtract extract_edituv_stretch_angle;
extern const MeshExtract extract_mesh_analysis;
extern const MeshExtract extract_fdots_pos;
extern const MeshExtract extract_fdots_nor;
extern const MeshExtract extract_fdots_nor_hq;
extern const MeshExtract extract_fdots_uv;
extern const MeshExtract extract_fdots_edituv_data;
extern const MeshExtract extract_skin_roots;
extern const MeshExtract extract_poly_idx;
extern const MeshExtract extract_edge_idx;
extern const MeshExtract extract_vert_idx;
extern const MeshExtract extract_fdot_idx;

#ifdef __cplusplus
}
#endif
