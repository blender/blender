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

#include "BKE_customdata.h"
#include "BKE_editmesh.h"

#include "draw_cache_extract.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DRWSubdivCache;

#define MIN_RANGE_LEN 1024

/* ---------------------------------------------------------------------- */
/** \name Mesh Render Data
 * \{ */

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

  struct {
    int *tri_first_index;
    int *mat_tri_len;
    int visible_tri_len;
  } poly_sorted;
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

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract Struct
 * \{ */

/* TODO(jbakker): move parameters inside a struct. */

typedef void(ExtractTriBMeshFn)(const MeshRenderData *mr, BMLoop **elt, int elt_index, void *data);
typedef void(ExtractTriMeshFn)(const MeshRenderData *mr,
                               const MLoopTri *mlt,
                               int elt_index,
                               void *data);
typedef void(ExtractPolyBMeshFn)(const MeshRenderData *mr,
                                 const BMFace *f,
                                 int f_index,
                                 void *data);
typedef void(ExtractPolyMeshFn)(const MeshRenderData *mr,
                                const MPoly *mp,
                                int mp_index,
                                void *data);
typedef void(ExtractLEdgeBMeshFn)(const MeshRenderData *mr,
                                  const BMEdge *eed,
                                  int ledge_index,
                                  void *data);
typedef void(ExtractLEdgeMeshFn)(const MeshRenderData *mr,
                                 const MEdge *med,
                                 int ledge_index,
                                 void *data);
typedef void(ExtractLVertBMeshFn)(const MeshRenderData *mr,
                                  const BMVert *eve,
                                  int lvert_index,
                                  void *data);
typedef void(ExtractLVertMeshFn)(const MeshRenderData *mr,
                                 const MVert *mv,
                                 int lvert_index,
                                 void *data);
typedef void(ExtractLooseGeomSubdivFn)(const struct DRWSubdivCache *subdiv_cache,
                                       const MeshRenderData *mr,
                                       const MeshExtractLooseGeom *loose_geom,
                                       void *buffer,
                                       void *data);
typedef void(ExtractInitFn)(const MeshRenderData *mr,
                            struct MeshBatchCache *cache,
                            void *buffer,
                            void *r_data);
typedef void(ExtractFinishFn)(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buffer,
                              void *data);
typedef void(ExtractTaskReduceFn)(void *userdata, void *task_userdata);

typedef void(ExtractInitSubdivFn)(const struct DRWSubdivCache *subdiv_cache,
                                  const MeshRenderData *mr,
                                  struct MeshBatchCache *cache,
                                  void *buf,
                                  void *data);
typedef void(ExtractIterSubdivFn)(const struct DRWSubdivCache *subdiv_cache,
                                  const MeshRenderData *mr,
                                  void *data);
typedef void(ExtractFinishSubdivFn)(const struct DRWSubdivCache *subdiv_cache,
                                    const MeshRenderData *mr,
                                    struct MeshBatchCache *cache,
                                    void *buf,
                                    void *data);

typedef struct MeshExtract {
  /** Executed on main thread and return user data for iteration functions. */
  ExtractInitFn *init;
  /** Executed on one (or more if use_threading) worker thread(s). */
  ExtractTriBMeshFn *iter_looptri_bm;
  ExtractTriMeshFn *iter_looptri_mesh;
  ExtractPolyBMeshFn *iter_poly_bm;
  ExtractPolyMeshFn *iter_poly_mesh;
  ExtractLEdgeBMeshFn *iter_ledge_bm;
  ExtractLEdgeMeshFn *iter_ledge_mesh;
  ExtractLVertBMeshFn *iter_lvert_bm;
  ExtractLVertMeshFn *iter_lvert_mesh;
  ExtractLooseGeomSubdivFn *iter_loose_geom_subdiv;
  /** Executed on one worker thread after all elements iterations. */
  ExtractTaskReduceFn *task_reduce;
  ExtractFinishFn *finish;
  /** Executed on main thread for subdivision evaluation. */
  ExtractInitSubdivFn *init_subdiv;
  ExtractIterSubdivFn *iter_subdiv;
  ExtractFinishSubdivFn *finish_subdiv;
  /** Used to request common data. */
  eMRDataType data_type;
  size_t data_size;
  /** Used to know if the element callbacks are thread-safe and can be parallelized. */
  bool use_threading;
  /**
   * Offset in bytes of the buffer inside a MeshBufferList instance. Points to a vertex or index
   * buffer.
   */
  size_t mesh_buffer_offset;
} MeshExtract;

/** \} */

/* draw_cache_extract_mesh_render_data.c */

/**
 * \param is_mode_active: When true, use the modifiers from the edit-data,
 * otherwise don't use modifiers as they are not from this object.
 */
MeshRenderData *mesh_render_data_create(Mesh *me,
                                        bool is_editmode,
                                        bool is_paint_mode,
                                        bool is_mode_active,
                                        const float obmat[4][4],
                                        bool do_final,
                                        bool do_uvedit,
                                        const ToolSettings *ts);
void mesh_render_data_free(MeshRenderData *mr);
void mesh_render_data_update_normals(MeshRenderData *mr, eMRDataType data_flag);
void mesh_render_data_update_loose_geom(MeshRenderData *mr,
                                        MeshBufferCache *cache,
                                        eMRIterType iter_type,
                                        eMRDataType data_flag);
void mesh_render_data_update_polys_sorted(MeshRenderData *mr,
                                          MeshBufferCache *cache,
                                          eMRDataType data_flag);
/**
 * Part of the creation of the #MeshRenderData that happens in a thread.
 */
void mesh_render_data_update_looptris(MeshRenderData *mr,
                                      eMRIterType iter_type,
                                      eMRDataType data_flag);

/* draw_cache_extract_mesh_extractors.c */
typedef struct EditLoopData {
  uchar v_flag;
  uchar e_flag;
  uchar crease;
  uchar bweight;
} EditLoopData;

void *mesh_extract_buffer_get(const MeshExtract *extractor, MeshBufferList *mbuflist);
eMRIterType mesh_extract_iter_type(const MeshExtract *ext);
const MeshExtract *mesh_extract_override_get(const MeshExtract *extractor,
                                             bool do_hq_normals,
                                             bool do_single_mat);
void mesh_render_data_face_flag(const MeshRenderData *mr,
                                const BMFace *efa,
                                int cd_ofs,
                                EditLoopData *eattr);
void mesh_render_data_loop_flag(const MeshRenderData *mr,
                                BMLoop *l,
                                int cd_ofs,
                                EditLoopData *eattr);
void mesh_render_data_loop_edge_flag(const MeshRenderData *mr,
                                     BMLoop *l,
                                     int cd_ofs,
                                     EditLoopData *eattr);

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
extern const MeshExtract extract_attr[GPU_MAX_ATTR];

#ifdef __cplusplus
}
#endif
