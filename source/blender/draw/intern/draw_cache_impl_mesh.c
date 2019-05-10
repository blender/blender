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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Mesh API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_math_bits.h"
#include "BLI_string.h"
#include "BLI_alloca.h"
#include "BLI_edgehash.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object_deform.h"

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

static void mesh_batch_cache_clear(Mesh *me);

/* Vertex Group Selection and display options */
typedef struct DRW_MeshWeightState {
  int defgroup_active;
  int defgroup_len;

  short flags;
  char alert_mode;

  /* Set of all selected bones for Multipaint. */
  bool *defgroup_sel; /* [defgroup_len] */
  int defgroup_sel_count;
} DRW_MeshWeightState;

typedef struct DRW_MeshCDMask {
  uint32_t uv : 8;
  uint32_t tan : 8;
  uint32_t vcol : 8;
  uint32_t orco : 1;
  uint32_t tan_orco : 1;
} DRW_MeshCDMask;

/* DRW_MeshWeightState.flags */
enum {
  DRW_MESH_WEIGHT_STATE_MULTIPAINT = (1 << 0),
  DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE = (1 << 1),
};

/* ---------------------------------------------------------------------- */
/** \name BMesh Inline Wrappers
 * \{ */

/**
 * Wrapper for #BM_vert_find_first_loop_visible
 * since most of the time this can be accessed directly without a function call.
 */
BLI_INLINE BMLoop *bm_vert_find_first_loop_visible_inline(BMVert *v)
{
  if (v->e) {
    BMLoop *l = v->e->l;
    if (l && !BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
      return l->v == v ? l : l->next;
    }
    return BM_vert_find_first_loop_visible(v);
  }
  return NULL;
}

BLI_INLINE BMLoop *bm_edge_find_first_loop_visible_inline(BMEdge *e)
{
  if (e->l) {
    BMLoop *l = e->l;
    if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
      return l;
    }
    return BM_edge_find_first_loop_visible(e);
  }
  return NULL;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (direct access to basic data).
 * \{ */

static int mesh_render_verts_len_get(Mesh *me)
{
  return me->edit_mesh ? me->edit_mesh->bm->totvert : me->totvert;
}

static int mesh_render_edges_len_get(Mesh *me)
{
  return me->edit_mesh ? me->edit_mesh->bm->totedge : me->totedge;
}

static int mesh_render_looptri_len_get(Mesh *me)
{
  return me->edit_mesh ? me->edit_mesh->tottri : poly_to_tri_count(me->totpoly, me->totloop);
}

static int mesh_render_polys_len_get(Mesh *me)
{
  return me->edit_mesh ? me->edit_mesh->bm->totface : me->totpoly;
}

static int mesh_render_mat_len_get(Mesh *me)
{
  return MAX2(1, me->totcol);
}

static int UNUSED_FUNCTION(mesh_render_loops_len_get)(Mesh *me)
{
  return me->edit_mesh ? me->edit_mesh->bm->totloop : me->totloop;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

typedef struct EdgeAdjacentPolys {
  int count;
  int face_index[2];
} EdgeAdjacentPolys;

typedef struct EdgeAdjacentVerts {
  int vert_index[2]; /* -1 if none */
} EdgeAdjacentVerts;

typedef struct EdgeDrawAttr {
  uchar v_flag;
  uchar e_flag;
  uchar crease;
  uchar bweight;
} EdgeDrawAttr;

typedef struct MeshRenderData {
  int types;

  int vert_len;
  int edge_len;
  int tri_len;
  int loop_len;
  int poly_len;
  int mat_len;
  int loose_vert_len;
  int loose_edge_len;

  /* Support for mapped mesh data. */
  struct {
    /* Must be set if we want to get mapped data. */
    bool use;
    bool supported;

    Mesh *me_cage;

    int vert_len;
    int edge_len;
    int tri_len;
    int loop_len;
    int poly_len;

    int *loose_verts;
    int loose_vert_len;

    int *loose_edges;
    int loose_edge_len;

    /* origindex layers */
    int *v_origindex;
    int *e_origindex;
    int *l_origindex;
    int *p_origindex;
  } mapped;

  BMEditMesh *edit_bmesh;
  struct EditMeshData *edit_data;
  const ToolSettings *toolsettings;

  Mesh *me;

  MVert *mvert;
  const MEdge *medge;
  const MLoop *mloop;
  const MPoly *mpoly;
  float (*orco)[3]; /* vertex coordinates normalized to bounding box */
  bool is_orco_allocated;
  MDeformVert *dvert;
  MLoopUV *mloopuv;
  MLoopCol *mloopcol;
  float (*loop_normals)[3];

  /* CustomData 'cd' cache for efficient access. */
  struct {
    struct {
      MLoopUV **uv;
      int uv_len;
      int uv_active;
      int uv_mask_active;

      MLoopCol **vcol;
      int vcol_len;
      int vcol_active;

      float (**tangent)[4];
      int tangent_len;
      int tangent_active;

      bool *auto_vcol;
    } layers;

    /* Custom-data offsets (only needed for BMesh access) */
    struct {
      int crease;
      int bweight;
      int *uv;
      int *vcol;
#ifdef WITH_FREESTYLE
      int freestyle_edge;
      int freestyle_face;
#endif
    } offset;

    struct {
      char (*auto_mix)[32];
      char (*uv)[32];
      char (*vcol)[32];
      char (*tangent)[32];
    } uuid;

    /* for certain cases we need an output loop-data storage (bmesh tangents) */
    struct {
      CustomData ldata;
      /* grr, special case variable (use in place of 'dm->tangent_mask') */
      short tangent_mask;
    } output;
  } cd;

  BMVert *eve_act;
  BMEdge *eed_act;
  BMFace *efa_act;
  BMFace *efa_act_uv;

  /* Data created on-demand (usually not for bmesh-based data). */
  EdgeAdjacentPolys *edges_adjacent_polys;
  MLoopTri *mlooptri;
  int *loose_edges;
  int *loose_verts;

  float (*poly_normals)[3];
  float *vert_weight;
  char (*vert_color)[3];
  GPUPackedNormal *poly_normals_pack;
  GPUPackedNormal *vert_normals_pack;
  bool *edge_select_bool;
  bool *edge_visible_bool;
} MeshRenderData;

typedef enum eMRDataType {
  MR_DATATYPE_VERT = 1 << 0,
  MR_DATATYPE_EDGE = 1 << 1,
  MR_DATATYPE_LOOPTRI = 1 << 2,
  MR_DATATYPE_LOOP = 1 << 3,
  MR_DATATYPE_POLY = 1 << 4,
  MR_DATATYPE_OVERLAY = 1 << 5,
  MR_DATATYPE_SHADING = 1 << 6,
  MR_DATATYPE_DVERT = 1 << 7,
  MR_DATATYPE_LOOPCOL = 1 << 8,
  MR_DATATYPE_LOOPUV = 1 << 9,
  MR_DATATYPE_LOOSE_VERT = 1 << 10,
  MR_DATATYPE_LOOSE_EDGE = 1 << 11,
  MR_DATATYPE_LOOP_NORMALS = 1 << 12,
} eMRDataType;

#define MR_DATATYPE_VERT_LOOP_POLY (MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP)
#define MR_DATATYPE_VERT_LOOP_TRI_POLY (MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_LOOPTRI)
#define MR_DATATYPE_LOOSE_VERT_EGDE (MR_DATATYPE_LOOSE_VERT | MR_DATATYPE_LOOSE_EDGE)

/**
 * These functions look like they would be slow but they will typically return true on the first
 * iteration. Only false when all attached elements are hidden.
 */
static bool bm_vert_has_visible_edge(const BMVert *v)
{
  const BMEdge *e_iter, *e_first;

  e_iter = e_first = v->e;
  do {
    if (!BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN)) {
      return true;
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
  return false;
}

static bool bm_edge_has_visible_face(const BMEdge *e)
{
  const BMLoop *l_iter, *l_first;
  l_iter = l_first = e->l;
  do {
    if (!BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
      return true;
    }
  } while ((l_iter = l_iter->radial_next) != l_first);
  return false;
}

BLI_INLINE bool bm_vert_is_loose_and_visible(const BMVert *v)
{
  return (!BM_elem_flag_test(v, BM_ELEM_HIDDEN) && (v->e == NULL || !bm_vert_has_visible_edge(v)));
}

BLI_INLINE bool bm_edge_is_loose_and_visible(const BMEdge *e)
{
  return (!BM_elem_flag_test(e, BM_ELEM_HIDDEN) && (e->l == NULL || !bm_edge_has_visible_face(e)));
}

/* Return true is all layers in _b_ are inside _a_. */
BLI_INLINE bool mesh_cd_layers_type_overlap(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return (*((uint32_t *)&a) & *((uint32_t *)&b)) == *((uint32_t *)&b);
}

BLI_INLINE bool mesh_cd_layers_type_equal(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return *((uint32_t *)&a) == *((uint32_t *)&b);
}

BLI_INLINE void mesh_cd_layers_type_merge(DRW_MeshCDMask *a, DRW_MeshCDMask b)
{
  atomic_fetch_and_or_uint32((uint32_t *)a, *(uint32_t *)&b);
}

BLI_INLINE void mesh_cd_layers_type_clear(DRW_MeshCDMask *a)
{
  *((uint32_t *)a) = 0;
}

static void mesh_cd_calc_active_uv_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const CustomData *cd_ldata = (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;

  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mask_uv_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const CustomData *cd_ldata = (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;

  int layer = CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_vcol_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const CustomData *cd_ldata = (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;

  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
  if (layer != -1) {
    cd_used->vcol |= (1 << layer);
  }
}

static DRW_MeshCDMask mesh_cd_calc_used_gpu_layers(const Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   int gpumat_array_len)
{
  const CustomData *cd_ldata = (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;

  /* See: DM_vertex_attributes_from_gpu for similar logic */
  DRW_MeshCDMask cd_used;
  mesh_cd_layers_type_clear(&cd_used);

  for (int i = 0; i < gpumat_array_len; i++) {
    GPUMaterial *gpumat = gpumat_array[i];
    if (gpumat) {
      GPUVertAttrLayers gpu_attrs;
      GPU_material_vertex_attrs(gpumat, &gpu_attrs);
      for (int j = 0; j < gpu_attrs.totlayer; j++) {
        const char *name = gpu_attrs.layer[j].name;
        int type = gpu_attrs.layer[j].type;
        int layer = -1;

        if (type == CD_AUTO_FROM_NAME) {
          /* We need to deduct what exact layer is used.
           *
           * We do it based on the specified name.
           */
          if (name[0] != '\0') {
            layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name);
            type = CD_MTFACE;

            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name);
              type = CD_MCOL;
            }
#if 0 /* Tangents are always from UV's - this will never happen. */
            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_TANGENT, name);
              type = CD_TANGENT;
            }
#endif
            if (layer == -1) {
              continue;
            }
          }
          else {
            /* Fall back to the UV layer, which matches old behavior. */
            type = CD_MTFACE;
          }
        }

        switch (type) {
          case CD_MTFACE: {
            if (layer == -1) {
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
                                          CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
            }
            if (layer != -1) {
              cd_used.uv |= (1 << layer);
            }
            break;
          }
          case CD_TANGENT: {
            if (layer == -1) {
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
                                          CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);

              /* Only fallback to orco (below) when we have no UV layers, see: T56545 */
              if (layer == -1 && name[0] != '\0') {
                layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
              }
            }
            if (layer != -1) {
              cd_used.tan |= (1 << layer);
            }
            else {
              /* no UV layers at all => requesting orco */
              cd_used.tan_orco = 1;
              cd_used.orco = 1;
            }
            break;
          }
          case CD_MCOL: {
            if (layer == -1) {
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name) :
                                          CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
            }
            if (layer != -1) {
              cd_used.vcol |= (1 << layer);
            }
            break;
          }
          case CD_ORCO: {
            cd_used.orco = 1;
            break;
          }
        }
      }
    }
  }
  return cd_used;
}

static void mesh_render_calc_normals_loop_and_poly(const Mesh *me,
                                                   const float split_angle,
                                                   MeshRenderData *rdata)
{
  BLI_assert((me->flag & ME_AUTOSMOOTH) != 0);

  int totloop = me->totloop;
  int totpoly = me->totpoly;
  float(*loop_normals)[3] = MEM_mallocN(sizeof(*loop_normals) * totloop, __func__);
  float(*poly_normals)[3] = MEM_mallocN(sizeof(*poly_normals) * totpoly, __func__);
  short(*clnors)[2] = CustomData_get_layer(&me->ldata, CD_CUSTOMLOOPNORMAL);

  BKE_mesh_calc_normals_poly(
      me->mvert, NULL, me->totvert, me->mloop, me->mpoly, totloop, totpoly, poly_normals, false);

  BKE_mesh_normals_loop_split(me->mvert,
                              me->totvert,
                              me->medge,
                              me->totedge,
                              me->mloop,
                              loop_normals,
                              totloop,
                              me->mpoly,
                              poly_normals,
                              totpoly,
                              true,
                              split_angle,
                              NULL,
                              clnors,
                              NULL);

  rdata->loop_len = totloop;
  rdata->poly_len = totpoly;
  rdata->loop_normals = loop_normals;
  rdata->poly_normals = poly_normals;
}

static void mesh_cd_extract_auto_layers_names_and_srgb(Mesh *me,
                                                       DRW_MeshCDMask cd_used,
                                                       char **r_auto_layers_names,
                                                       int **r_auto_layers_srgb,
                                                       int *r_auto_layers_len)
{
  const CustomData *cd_ldata = (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;

  int uv_len_used = count_bits_i(cd_used.uv);
  int vcol_len_used = count_bits_i(cd_used.vcol);
  int uv_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPUV);
  int vcol_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPCOL);

  uint auto_names_len = 32 * (uv_len_used + vcol_len_used);
  uint auto_ofs = 0;
  /* Allocate max, resize later. */
  char *auto_names = MEM_callocN(sizeof(char) * auto_names_len, __func__);
  int *auto_is_srgb = MEM_callocN(sizeof(int) * (uv_len_used + vcol_len_used), __func__);

  for (int i = 0; i < uv_len; i++) {
    if ((cd_used.uv & (1 << i)) != 0) {
      const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
      uint hash = BLI_ghashutil_strhash_p(name);
      /* +1 to include '\0' terminator. */
      auto_ofs += 1 + BLI_snprintf_rlen(
                          auto_names + auto_ofs, auto_names_len - auto_ofs, "ba%u", hash);
    }
  }

  uint auto_is_srgb_ofs = uv_len_used;
  for (int i = 0; i < vcol_len; i++) {
    if ((cd_used.vcol & (1 << i)) != 0) {
      const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
      /* We only do vcols that are not overridden by a uv layer with same name. */
      if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, name) == -1) {
        uint hash = BLI_ghashutil_strhash_p(name);
        /* +1 to include '\0' terminator. */
        auto_ofs += 1 + BLI_snprintf_rlen(
                            auto_names + auto_ofs, auto_names_len - auto_ofs, "ba%u", hash);
        auto_is_srgb[auto_is_srgb_ofs] = true;
        auto_is_srgb_ofs++;
      }
    }
  }

  auto_names = MEM_reallocN(auto_names, sizeof(char) * auto_ofs);
  auto_is_srgb = MEM_reallocN(auto_is_srgb, sizeof(int) * auto_is_srgb_ofs);

  /* WATCH: May have been referenced somewhere before freeing. */
  MEM_SAFE_FREE(*r_auto_layers_names);
  MEM_SAFE_FREE(*r_auto_layers_srgb);

  *r_auto_layers_names = auto_names;
  *r_auto_layers_srgb = auto_is_srgb;
  *r_auto_layers_len = auto_is_srgb_ofs;
}

/**
 * TODO(campbell): 'gpumat_array' may include materials linked to the object.
 * While not default, object materials should be supported.
 * Although this only impacts the data that's generated, not the materials that display.
 */
static MeshRenderData *mesh_render_data_create_ex(Mesh *me,
                                                  const int types,
                                                  const DRW_MeshCDMask *cd_used,
                                                  const ToolSettings *ts)
{
  MeshRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
  rdata->types = types;
  rdata->toolsettings = ts;
  rdata->mat_len = mesh_render_mat_len_get(me);

  CustomData_reset(&rdata->cd.output.ldata);

  const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;
  const float split_angle = is_auto_smooth ? me->smoothresh : (float)M_PI;

  if (me->edit_mesh) {
    BMEditMesh *embm = me->edit_mesh;
    BMesh *bm = embm->bm;

    rdata->edit_bmesh = embm;
    rdata->edit_data = me->runtime.edit_data;

    if (embm->mesh_eval_cage && (embm->mesh_eval_cage->runtime.is_original == false)) {
      Mesh *me_cage = embm->mesh_eval_cage;

      rdata->mapped.me_cage = me_cage;
      if (types & MR_DATATYPE_VERT) {
        rdata->mapped.vert_len = me_cage->totvert;
      }
      if (types & MR_DATATYPE_EDGE) {
        rdata->mapped.edge_len = me_cage->totedge;
      }
      if (types & MR_DATATYPE_LOOP) {
        rdata->mapped.loop_len = me_cage->totloop;
      }
      if (types & MR_DATATYPE_POLY) {
        rdata->mapped.poly_len = me_cage->totpoly;
      }
      if (types & MR_DATATYPE_LOOPTRI) {
        rdata->mapped.tri_len = poly_to_tri_count(me_cage->totpoly, me_cage->totloop);
      }
      if (types & MR_DATATYPE_LOOPUV) {
        rdata->mloopuv = CustomData_get_layer(&me_cage->ldata, CD_MLOOPUV);
      }

      rdata->mapped.v_origindex = CustomData_get_layer(&me_cage->vdata, CD_ORIGINDEX);
      rdata->mapped.e_origindex = CustomData_get_layer(&me_cage->edata, CD_ORIGINDEX);
      rdata->mapped.l_origindex = CustomData_get_layer(&me_cage->ldata, CD_ORIGINDEX);
      rdata->mapped.p_origindex = CustomData_get_layer(&me_cage->pdata, CD_ORIGINDEX);
      rdata->mapped.supported = (rdata->mapped.v_origindex || rdata->mapped.e_origindex ||
                                 rdata->mapped.p_origindex);
    }

    int bm_ensure_types = 0;
    if (types & MR_DATATYPE_VERT) {
      rdata->vert_len = bm->totvert;
      bm_ensure_types |= BM_VERT;
    }
    if (types & MR_DATATYPE_EDGE) {
      rdata->edge_len = bm->totedge;
      bm_ensure_types |= BM_EDGE;
    }
    if (types & MR_DATATYPE_LOOPTRI) {
      bm_ensure_types |= BM_LOOP;
    }
    if (types & MR_DATATYPE_LOOP) {
      rdata->loop_len = bm->totloop;
      bm_ensure_types |= BM_LOOP;
    }
    if (types & MR_DATATYPE_POLY) {
      rdata->poly_len = bm->totface;
      bm_ensure_types |= BM_FACE;
    }
    if (types & MR_DATATYPE_LOOP_NORMALS) {
      BLI_assert(types & MR_DATATYPE_LOOP);
      if (is_auto_smooth) {
        rdata->loop_normals = MEM_mallocN(sizeof(*rdata->loop_normals) * bm->totloop, __func__);
        int cd_loop_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
        BM_loops_calc_normal_vcos(bm,
                                  NULL,
                                  NULL,
                                  NULL,
                                  true,
                                  split_angle,
                                  rdata->loop_normals,
                                  NULL,
                                  NULL,
                                  cd_loop_clnors_offset,
                                  false);
      }
    }
    if (types & MR_DATATYPE_OVERLAY) {
      rdata->efa_act_uv = EDBM_uv_active_face_get(embm, false, false);
      rdata->efa_act = BM_mesh_active_face_get(bm, false, true);
      rdata->eed_act = BM_mesh_active_edge_get(bm);
      rdata->eve_act = BM_mesh_active_vert_get(bm);
      rdata->cd.offset.crease = CustomData_get_offset(&bm->edata, CD_CREASE);
      rdata->cd.offset.bweight = CustomData_get_offset(&bm->edata, CD_BWEIGHT);

#ifdef WITH_FREESTYLE
      rdata->cd.offset.freestyle_edge = CustomData_get_offset(&bm->edata, CD_FREESTYLE_EDGE);
      rdata->cd.offset.freestyle_face = CustomData_get_offset(&bm->pdata, CD_FREESTYLE_FACE);
#endif
    }
    if (types & (MR_DATATYPE_DVERT)) {
      bm_ensure_types |= BM_VERT;
    }
    if (rdata->edit_data != NULL) {
      bm_ensure_types |= BM_VERT;
    }

    BM_mesh_elem_index_ensure(bm, bm_ensure_types);
    BM_mesh_elem_table_ensure(bm, bm_ensure_types & ~BM_LOOP);

    if (types & MR_DATATYPE_LOOPTRI) {
      /* Edit mode ensures this is valid, no need to calculate. */
      BLI_assert((bm->totloop == 0) || (embm->looptris != NULL));
      int tottri = embm->tottri;
      MLoopTri *mlooptri = MEM_mallocN(sizeof(*rdata->mlooptri) * embm->tottri, __func__);
      for (int index = 0; index < tottri; index++) {
        BMLoop **bmtri = embm->looptris[index];
        MLoopTri *mtri = &mlooptri[index];
        mtri->tri[0] = BM_elem_index_get(bmtri[0]);
        mtri->tri[1] = BM_elem_index_get(bmtri[1]);
        mtri->tri[2] = BM_elem_index_get(bmtri[2]);
      }
      rdata->mlooptri = mlooptri;
      rdata->tri_len = tottri;
    }

    if (types & MR_DATATYPE_LOOSE_VERT) {
      BLI_assert(types & MR_DATATYPE_VERT);
      rdata->loose_vert_len = 0;

      {
        int *lverts = MEM_mallocN(rdata->vert_len * sizeof(int), __func__);
        BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
        for (int i = 0; i < bm->totvert; i++) {
          const BMVert *eve = BM_vert_at_index(bm, i);
          if (bm_vert_is_loose_and_visible(eve)) {
            lverts[rdata->loose_vert_len++] = i;
          }
        }
        rdata->loose_verts = MEM_reallocN(lverts, rdata->loose_vert_len * sizeof(int));
      }

      if (rdata->mapped.supported) {
        Mesh *me_cage = embm->mesh_eval_cage;
        rdata->mapped.loose_vert_len = 0;

        if (rdata->loose_vert_len) {
          int *lverts = MEM_mallocN(me_cage->totvert * sizeof(int), __func__);
          const int *v_origindex = rdata->mapped.v_origindex;
          for (int i = 0; i < me_cage->totvert; i++) {
            const int v_orig = v_origindex[i];
            if (v_orig != ORIGINDEX_NONE) {
              BMVert *eve = BM_vert_at_index(bm, v_orig);
              if (bm_vert_is_loose_and_visible(eve)) {
                lverts[rdata->mapped.loose_vert_len++] = i;
              }
            }
          }
          rdata->mapped.loose_verts = MEM_reallocN(lverts,
                                                   rdata->mapped.loose_vert_len * sizeof(int));
        }
      }
    }

    if (types & MR_DATATYPE_LOOSE_EDGE) {
      BLI_assert(types & MR_DATATYPE_EDGE);
      rdata->loose_edge_len = 0;

      {
        int *ledges = MEM_mallocN(rdata->edge_len * sizeof(int), __func__);
        BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
        for (int i = 0; i < bm->totedge; i++) {
          const BMEdge *eed = BM_edge_at_index(bm, i);
          if (bm_edge_is_loose_and_visible(eed)) {
            ledges[rdata->loose_edge_len++] = i;
          }
        }
        rdata->loose_edges = MEM_reallocN(ledges, rdata->loose_edge_len * sizeof(int));
      }

      if (rdata->mapped.supported) {
        Mesh *me_cage = embm->mesh_eval_cage;
        rdata->mapped.loose_edge_len = 0;

        if (rdata->loose_edge_len) {
          int *ledges = MEM_mallocN(me_cage->totedge * sizeof(int), __func__);
          const int *e_origindex = rdata->mapped.e_origindex;
          for (int i = 0; i < me_cage->totedge; i++) {
            const int e_orig = e_origindex[i];
            if (e_orig != ORIGINDEX_NONE) {
              BMEdge *eed = BM_edge_at_index(bm, e_orig);
              if (bm_edge_is_loose_and_visible(eed)) {
                ledges[rdata->mapped.loose_edge_len++] = i;
              }
            }
          }
          rdata->mapped.loose_edges = MEM_reallocN(ledges,
                                                   rdata->mapped.loose_edge_len * sizeof(int));
        }
      }
    }
  }
  else {
    rdata->me = me;

    if (types & (MR_DATATYPE_VERT)) {
      rdata->vert_len = me->totvert;
      rdata->mvert = CustomData_get_layer(&me->vdata, CD_MVERT);
    }
    if (types & (MR_DATATYPE_EDGE)) {
      rdata->edge_len = me->totedge;
      rdata->medge = CustomData_get_layer(&me->edata, CD_MEDGE);
    }
    if (types & MR_DATATYPE_LOOPTRI) {
      const int tri_len = rdata->tri_len = poly_to_tri_count(me->totpoly, me->totloop);
      MLoopTri *mlooptri = MEM_mallocN(sizeof(*mlooptri) * tri_len, __func__);
      BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, mlooptri);
      rdata->mlooptri = mlooptri;
    }
    if (types & MR_DATATYPE_LOOP) {
      rdata->loop_len = me->totloop;
      rdata->mloop = CustomData_get_layer(&me->ldata, CD_MLOOP);
    }
    if (types & MR_DATATYPE_LOOP_NORMALS) {
      BLI_assert(types & MR_DATATYPE_LOOP);
      if (is_auto_smooth) {
        mesh_render_calc_normals_loop_and_poly(me, split_angle, rdata);
      }
    }
    if (types & MR_DATATYPE_POLY) {
      rdata->poly_len = me->totpoly;
      rdata->mpoly = CustomData_get_layer(&me->pdata, CD_MPOLY);
    }
    if (types & MR_DATATYPE_DVERT) {
      rdata->vert_len = me->totvert;
      rdata->dvert = CustomData_get_layer(&me->vdata, CD_MDEFORMVERT);
    }
    if (types & MR_DATATYPE_LOOPCOL) {
      rdata->loop_len = me->totloop;
      rdata->mloopcol = CustomData_get_layer(&me->ldata, CD_MLOOPCOL);
    }
    if (types & MR_DATATYPE_LOOPUV) {
      rdata->loop_len = me->totloop;
      rdata->mloopuv = CustomData_get_layer(&me->ldata, CD_MLOOPUV);
    }
  }

  if (types & MR_DATATYPE_SHADING) {
    CustomData *cd_vdata, *cd_ldata;

    BLI_assert(cd_used != NULL);

    if (me->edit_mesh) {
      BMesh *bm = me->edit_mesh->bm;
      cd_vdata = &bm->vdata;
      cd_ldata = &bm->ldata;
    }
    else {
      cd_vdata = &me->vdata;
      cd_ldata = &me->ldata;
    }

    rdata->cd.layers.uv_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
    rdata->cd.layers.uv_mask_active = CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV);
    rdata->cd.layers.vcol_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
    rdata->cd.layers.tangent_active = rdata->cd.layers.uv_active;

#define CD_VALIDATE_ACTIVE_LAYER(active_index, used) \
  if ((active_index != -1) && (used & (1 << active_index)) == 0) { \
    active_index = -1; \
  } \
  ((void)0)

    CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.uv_active, cd_used->uv);
    CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.uv_mask_active, cd_used->uv);
    CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.tangent_active, cd_used->tan);
    CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.vcol_active, cd_used->vcol);

#undef CD_VALIDATE_ACTIVE_LAYER

    rdata->is_orco_allocated = false;
    if (cd_used->orco != 0) {
      rdata->orco = CustomData_get_layer(cd_vdata, CD_ORCO);
      /* If orco is not available compute it ourselves */
      if (!rdata->orco) {
        rdata->is_orco_allocated = true;
        if (me->edit_mesh) {
          BMesh *bm = me->edit_mesh->bm;
          rdata->orco = MEM_mallocN(sizeof(*rdata->orco) * rdata->vert_len, "orco mesh");
          BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
          for (int i = 0; i < bm->totvert; i++) {
            copy_v3_v3(rdata->orco[i], BM_vert_at_index(bm, i)->co);
          }
          BKE_mesh_orco_verts_transform(me, rdata->orco, rdata->vert_len, 0);
        }
        else {
          rdata->orco = MEM_mallocN(sizeof(*rdata->orco) * rdata->vert_len, "orco mesh");
          MVert *mvert = rdata->mvert;
          for (int a = 0; a < rdata->vert_len; a++, mvert++) {
            copy_v3_v3(rdata->orco[a], mvert->co);
          }
          BKE_mesh_orco_verts_transform(me, rdata->orco, rdata->vert_len, 0);
        }
      }
    }
    else {
      rdata->orco = NULL;
    }

    /* don't access mesh directly, instead use vars taken from BMesh or Mesh */
#define me DONT_USE_THIS
#ifdef me /* quiet warning */
#endif
    struct {
      uint uv_len;
      uint vcol_len;
    } cd_layers_src = {
        .uv_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPUV),
        .vcol_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPCOL),
    };

    rdata->cd.layers.uv_len = min_ii(cd_layers_src.uv_len, count_bits_i(cd_used->uv));
    rdata->cd.layers.tangent_len = count_bits_i(cd_used->tan) + cd_used->tan_orco;
    rdata->cd.layers.vcol_len = min_ii(cd_layers_src.vcol_len, count_bits_i(cd_used->vcol));

    rdata->cd.layers.uv = MEM_mallocN(sizeof(*rdata->cd.layers.uv) * rdata->cd.layers.uv_len,
                                      __func__);
    rdata->cd.layers.vcol = MEM_mallocN(sizeof(*rdata->cd.layers.vcol) * rdata->cd.layers.vcol_len,
                                        __func__);
    rdata->cd.layers.tangent = MEM_mallocN(
        sizeof(*rdata->cd.layers.tangent) * rdata->cd.layers.tangent_len, __func__);

    rdata->cd.uuid.uv = MEM_mallocN(sizeof(*rdata->cd.uuid.uv) * rdata->cd.layers.uv_len,
                                    __func__);
    rdata->cd.uuid.vcol = MEM_mallocN(sizeof(*rdata->cd.uuid.vcol) * rdata->cd.layers.vcol_len,
                                      __func__);
    rdata->cd.uuid.tangent = MEM_mallocN(
        sizeof(*rdata->cd.uuid.tangent) * rdata->cd.layers.tangent_len, __func__);

    rdata->cd.offset.uv = MEM_mallocN(sizeof(*rdata->cd.offset.uv) * rdata->cd.layers.uv_len,
                                      __func__);
    rdata->cd.offset.vcol = MEM_mallocN(sizeof(*rdata->cd.offset.vcol) * rdata->cd.layers.vcol_len,
                                        __func__);

    /* Allocate max */
    rdata->cd.layers.auto_vcol = MEM_callocN(
        sizeof(*rdata->cd.layers.auto_vcol) * rdata->cd.layers.vcol_len, __func__);
    rdata->cd.uuid.auto_mix = MEM_mallocN(
        sizeof(*rdata->cd.uuid.auto_mix) * (rdata->cd.layers.vcol_len + rdata->cd.layers.uv_len),
        __func__);

    /* XXX FIXME XXX */
    /* We use a hash to identify each data layer based on its name.
     * Gawain then search for this name in the current shader and bind if it exists.
     * NOTE : This is prone to hash collision.
     * One solution to hash collision would be to format the cd layer name
     * to a safe glsl var name, but without name clash.
     * NOTE 2 : Replicate changes to code_generate_vertex_new() in gpu_codegen.c */
    if (rdata->cd.layers.vcol_len != 0) {
      int act_vcol = rdata->cd.layers.vcol_active;
      for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.vcol_len; i_src++, i_dst++) {
        if ((cd_used->vcol & (1 << i_src)) == 0) {
          /* This is a non-used VCol slot. Skip. */
          i_dst--;
          if (rdata->cd.layers.vcol_active >= i_src) {
            act_vcol--;
          }
        }
        else {
          const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i_src);
          uint hash = BLI_ghashutil_strhash_p(name);
          BLI_snprintf(rdata->cd.uuid.vcol[i_dst], sizeof(*rdata->cd.uuid.vcol), "c%u", hash);
          rdata->cd.layers.vcol[i_dst] = CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i_src);
          if (rdata->edit_bmesh) {
            rdata->cd.offset.vcol[i_dst] = CustomData_get_n_offset(
                &rdata->edit_bmesh->bm->ldata, CD_MLOOPCOL, i_src);
          }

          /* Gather number of auto layers. */
          /* We only do vcols that are not overridden by uvs */
          if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, name) == -1) {
            BLI_snprintf(rdata->cd.uuid.auto_mix[rdata->cd.layers.uv_len + i_dst],
                         sizeof(*rdata->cd.uuid.auto_mix),
                         "a%u",
                         hash);
            rdata->cd.layers.auto_vcol[i_dst] = true;
          }
        }
      }
      if (rdata->cd.layers.vcol_active != -1) {
        /* Actual active Vcol slot inside vcol layers used for shading. */
        rdata->cd.layers.vcol_active = act_vcol;
      }
    }

    /* Start Fresh */
    CustomData_free_layers(cd_ldata, CD_TANGENT, rdata->loop_len);
    CustomData_free_layers(cd_ldata, CD_MLOOPTANGENT, rdata->loop_len);

    if (rdata->cd.layers.uv_len != 0) {
      int act_uv = rdata->cd.layers.uv_active;
      for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
        if ((cd_used->uv & (1 << i_src)) == 0) {
          /* This is a non-used UV slot. Skip. */
          i_dst--;
          if (rdata->cd.layers.uv_active >= i_src) {
            act_uv--;
          }
        }
        else {
          const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src);
          uint hash = BLI_ghashutil_strhash_p(name);

          BLI_snprintf(rdata->cd.uuid.uv[i_dst], sizeof(*rdata->cd.uuid.uv), "u%u", hash);
          rdata->cd.layers.uv[i_dst] = CustomData_get_layer_n(cd_ldata, CD_MLOOPUV, i_src);
          if (rdata->edit_bmesh) {
            rdata->cd.offset.uv[i_dst] = CustomData_get_n_offset(
                &rdata->edit_bmesh->bm->ldata, CD_MLOOPUV, i_src);
          }
          BLI_snprintf(
              rdata->cd.uuid.auto_mix[i_dst], sizeof(*rdata->cd.uuid.auto_mix), "a%u", hash);
        }
      }
      if (rdata->cd.layers.uv_active != -1) {
        /* Actual active UV slot inside uv layers used for shading. */
        rdata->cd.layers.uv_active = act_uv;
      }
    }

    if (rdata->cd.layers.tangent_len != 0) {

      /* -------------------------------------------------------------------- */
      /* Pre-calculate tangents into 'rdata->cd.output.ldata' */

      BLI_assert(!CustomData_has_layer(&rdata->cd.output.ldata, CD_TANGENT));

      /* Tangent Names */
      char tangent_names[MAX_MTFACE][MAX_NAME];
      for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
        if ((cd_used->tan & (1 << i_src)) == 0) {
          i_dst--;
        }
        else {
          BLI_strncpy(tangent_names[i_dst],
                      CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src),
                      MAX_NAME);
        }
      }

      /* If tangent from orco is requested, decrement tangent_len */
      int actual_tangent_len = (cd_used->tan_orco != 0) ? rdata->cd.layers.tangent_len - 1 :
                                                          rdata->cd.layers.tangent_len;
      if (rdata->edit_bmesh) {
        BMEditMesh *em = rdata->edit_bmesh;
        BMesh *bm = em->bm;

        if (is_auto_smooth && rdata->loop_normals == NULL) {
          /* Should we store the previous array of `loop_normals` in somewhere? */
          rdata->loop_len = bm->totloop;
          rdata->loop_normals = MEM_mallocN(sizeof(*rdata->loop_normals) * rdata->loop_len,
                                            __func__);
          BM_loops_calc_normal_vcos(
              bm, NULL, NULL, NULL, true, split_angle, rdata->loop_normals, NULL, NULL, -1, false);
        }

        bool calc_active_tangent = false;

        BKE_editmesh_loop_tangent_calc(em,
                                       calc_active_tangent,
                                       tangent_names,
                                       actual_tangent_len,
                                       rdata->poly_normals,
                                       rdata->loop_normals,
                                       rdata->orco,
                                       &rdata->cd.output.ldata,
                                       bm->totloop,
                                       &rdata->cd.output.tangent_mask);
      }
      else {
#undef me

        if (is_auto_smooth && rdata->loop_normals == NULL) {
          /* Should we store the previous array of `loop_normals` in CustomData? */
          mesh_render_calc_normals_loop_and_poly(me, split_angle, rdata);
        }

        bool calc_active_tangent = false;

        BKE_mesh_calc_loop_tangent_ex(me->mvert,
                                      me->mpoly,
                                      me->totpoly,
                                      me->mloop,
                                      rdata->mlooptri,
                                      rdata->tri_len,
                                      cd_ldata,
                                      calc_active_tangent,
                                      tangent_names,
                                      actual_tangent_len,
                                      rdata->poly_normals,
                                      rdata->loop_normals,
                                      rdata->orco,
                                      &rdata->cd.output.ldata,
                                      me->totloop,
                                      &rdata->cd.output.tangent_mask);

        /* If we store tangents in the mesh, set temporary. */
#if 0
        CustomData_set_layer_flag(cd_ldata, CD_TANGENT, CD_FLAG_TEMPORARY);
#endif

#define me DONT_USE_THIS
#ifdef me /* quiet warning */
#endif
      }

      /* End tangent calculation */
      /* -------------------------------------------------------------------- */

      BLI_assert(CustomData_number_of_layers(&rdata->cd.output.ldata, CD_TANGENT) ==
                 rdata->cd.layers.tangent_len);

      int i_dst = 0;
      for (int i_src = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
        if ((cd_used->tan & (1 << i_src)) == 0) {
          i_dst--;
          if (rdata->cd.layers.tangent_active >= i_src) {
            rdata->cd.layers.tangent_active--;
          }
        }
        else {
          const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src);
          uint hash = BLI_ghashutil_strhash_p(name);

          BLI_snprintf(
              rdata->cd.uuid.tangent[i_dst], sizeof(*rdata->cd.uuid.tangent), "t%u", hash);

          /* Done adding tangents. */

          /* note: BKE_editmesh_loop_tangent_calc calculates 'CD_TANGENT',
           * not 'CD_MLOOPTANGENT' (as done below). It's OK, they're compatible. */

          /* note: normally we'd use 'i_src' here, but 'i_dst' is in sync with 'rdata->cd.output'
           */
          rdata->cd.layers.tangent[i_dst] = CustomData_get_layer_n(
              &rdata->cd.output.ldata, CD_TANGENT, i_dst);
          if (rdata->tri_len != 0) {
            BLI_assert(rdata->cd.layers.tangent[i_dst] != NULL);
          }
        }
      }
      if (cd_used->tan_orco != 0) {
        const char *name = CustomData_get_layer_name(&rdata->cd.output.ldata, CD_TANGENT, i_dst);
        uint hash = BLI_ghashutil_strhash_p(name);
        BLI_snprintf(rdata->cd.uuid.tangent[i_dst], sizeof(*rdata->cd.uuid.tangent), "t%u", hash);

        rdata->cd.layers.tangent[i_dst] = CustomData_get_layer_n(
            &rdata->cd.output.ldata, CD_TANGENT, i_dst);
      }
    }

#undef me
  }

  return rdata;
}

/* Warning replace mesh pointer. */
#define MBC_GET_FINAL_MESH(me) \
  /* Hack to show the final result. */ \
  const bool _use_em_final = ((me)->edit_mesh && (me)->edit_mesh->mesh_eval_final && \
                              ((me)->edit_mesh->mesh_eval_final->runtime.is_original == false)); \
  Mesh _me_fake; \
  if (_use_em_final) { \
    _me_fake = *(me)->edit_mesh->mesh_eval_final; \
    _me_fake.mat = (me)->mat; \
    _me_fake.totcol = (me)->totcol; \
    (me) = &_me_fake; \
  } \
  ((void)0)

static void mesh_render_data_free(MeshRenderData *rdata)
{
  if (rdata->is_orco_allocated) {
    MEM_SAFE_FREE(rdata->orco);
  }
  MEM_SAFE_FREE(rdata->cd.offset.uv);
  MEM_SAFE_FREE(rdata->cd.offset.vcol);
  MEM_SAFE_FREE(rdata->cd.uuid.auto_mix);
  MEM_SAFE_FREE(rdata->cd.uuid.uv);
  MEM_SAFE_FREE(rdata->cd.uuid.vcol);
  MEM_SAFE_FREE(rdata->cd.uuid.tangent);
  MEM_SAFE_FREE(rdata->cd.layers.uv);
  MEM_SAFE_FREE(rdata->cd.layers.vcol);
  MEM_SAFE_FREE(rdata->cd.layers.tangent);
  MEM_SAFE_FREE(rdata->cd.layers.auto_vcol);
  MEM_SAFE_FREE(rdata->loose_verts);
  MEM_SAFE_FREE(rdata->loose_edges);
  MEM_SAFE_FREE(rdata->edges_adjacent_polys);
  MEM_SAFE_FREE(rdata->mlooptri);
  MEM_SAFE_FREE(rdata->loop_normals);
  MEM_SAFE_FREE(rdata->poly_normals);
  MEM_SAFE_FREE(rdata->poly_normals_pack);
  MEM_SAFE_FREE(rdata->vert_normals_pack);
  MEM_SAFE_FREE(rdata->vert_weight);
  MEM_SAFE_FREE(rdata->edge_select_bool);
  MEM_SAFE_FREE(rdata->edge_visible_bool);
  MEM_SAFE_FREE(rdata->vert_color);

  MEM_SAFE_FREE(rdata->mapped.loose_verts);
  MEM_SAFE_FREE(rdata->mapped.loose_edges);

  CustomData_free(&rdata->cd.output.ldata, rdata->loop_len);

  MEM_freeN(rdata);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Accessor Functions
 * \{ */

static const char *mesh_render_data_uv_auto_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
  BLI_assert(rdata->types & MR_DATATYPE_SHADING);
  return rdata->cd.uuid.auto_mix[layer];
}

static const char *mesh_render_data_vcol_auto_layer_uuid_get(const MeshRenderData *rdata,
                                                             int layer)
{
  BLI_assert(rdata->types & MR_DATATYPE_SHADING);
  return rdata->cd.uuid.auto_mix[rdata->cd.layers.uv_len + layer];
}

static const char *mesh_render_data_uv_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
  BLI_assert(rdata->types & MR_DATATYPE_SHADING);
  return rdata->cd.uuid.uv[layer];
}

static const char *mesh_render_data_vcol_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
  BLI_assert(rdata->types & MR_DATATYPE_SHADING);
  return rdata->cd.uuid.vcol[layer];
}

static const char *mesh_render_data_tangent_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
  BLI_assert(rdata->types & MR_DATATYPE_SHADING);
  return rdata->cd.uuid.tangent[layer];
}

static int UNUSED_FUNCTION(mesh_render_data_verts_len_get)(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_VERT);
  return rdata->vert_len;
}
static int mesh_render_data_verts_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_VERT);
  return ((rdata->mapped.use == false) ? rdata->vert_len : rdata->mapped.vert_len);
}

static int UNUSED_FUNCTION(mesh_render_data_loose_verts_len_get)(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOSE_VERT);
  return rdata->loose_vert_len;
}
static int mesh_render_data_loose_verts_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOSE_VERT);
  return ((rdata->mapped.use == false) ? rdata->loose_vert_len : rdata->mapped.loose_vert_len);
}

static int mesh_render_data_edges_len_get(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_EDGE);
  return rdata->edge_len;
}
static int mesh_render_data_edges_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_EDGE);
  return ((rdata->mapped.use == false) ? rdata->edge_len : rdata->mapped.edge_len);
}

static int UNUSED_FUNCTION(mesh_render_data_loose_edges_len_get)(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOSE_EDGE);
  return rdata->loose_edge_len;
}
static int mesh_render_data_loose_edges_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOSE_EDGE);
  return ((rdata->mapped.use == false) ? rdata->loose_edge_len : rdata->mapped.loose_edge_len);
}

static int mesh_render_data_looptri_len_get(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
  return rdata->tri_len;
}
static int mesh_render_data_looptri_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
  return ((rdata->mapped.use == false) ? rdata->tri_len : rdata->mapped.tri_len);
}

static int UNUSED_FUNCTION(mesh_render_data_mat_len_get)(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_POLY);
  return rdata->mat_len;
}

static int mesh_render_data_loops_len_get(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOP);
  return rdata->loop_len;
}

static int mesh_render_data_loops_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_LOOP);
  return ((rdata->mapped.use == false) ? rdata->loop_len : rdata->mapped.loop_len);
}

static int mesh_render_data_polys_len_get(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_POLY);
  return rdata->poly_len;
}
static int mesh_render_data_polys_len_get_maybe_mapped(const MeshRenderData *rdata)
{
  BLI_assert(rdata->types & MR_DATATYPE_POLY);
  return ((rdata->mapped.use == false) ? rdata->poly_len : rdata->mapped.poly_len);
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Internal Cache (Lazy Initialization)
 * \{ */

/** Ensure #MeshRenderData.poly_normals_pack */
static void mesh_render_data_ensure_poly_normals_pack(MeshRenderData *rdata)
{
  GPUPackedNormal *pnors_pack = rdata->poly_normals_pack;
  if (pnors_pack == NULL) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter fiter;
      BMFace *efa;
      int i;

      pnors_pack = rdata->poly_normals_pack = MEM_mallocN(sizeof(*pnors_pack) * rdata->poly_len,
                                                          __func__);
      if (rdata->edit_data && rdata->edit_data->vertexCos != NULL) {
        BKE_editmesh_cache_ensure_poly_normals(rdata->edit_bmesh, rdata->edit_data);
        const float(*pnors)[3] = rdata->edit_data->polyNos;
        for (i = 0; i < bm->totface; i++) {
          pnors_pack[i] = GPU_normal_convert_i10_v3(pnors[i]);
        }
      }
      else {
        BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
          pnors_pack[i] = GPU_normal_convert_i10_v3(efa->no);
        }
      }
    }
    else {
      float(*pnors)[3] = rdata->poly_normals;

      if (!pnors) {
        pnors = rdata->poly_normals = MEM_mallocN(sizeof(*pnors) * rdata->poly_len, __func__);
        BKE_mesh_calc_normals_poly(rdata->mvert,
                                   NULL,
                                   rdata->vert_len,
                                   rdata->mloop,
                                   rdata->mpoly,
                                   rdata->loop_len,
                                   rdata->poly_len,
                                   pnors,
                                   true);
      }

      pnors_pack = rdata->poly_normals_pack = MEM_mallocN(sizeof(*pnors_pack) * rdata->poly_len,
                                                          __func__);
      for (int i = 0; i < rdata->poly_len; i++) {
        pnors_pack[i] = GPU_normal_convert_i10_v3(pnors[i]);
      }
    }
  }
}

/** Ensure #MeshRenderData.vert_normals_pack */
static void mesh_render_data_ensure_vert_normals_pack(MeshRenderData *rdata)
{
  GPUPackedNormal *vnors_pack = rdata->vert_normals_pack;
  if (vnors_pack == NULL) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter viter;
      BMVert *eve;
      int i;

      vnors_pack = rdata->vert_normals_pack = MEM_mallocN(sizeof(*vnors_pack) * rdata->vert_len,
                                                          __func__);
      BM_ITER_MESH_INDEX (eve, &viter, bm, BM_VERT, i) {
        vnors_pack[i] = GPU_normal_convert_i10_v3(eve->no);
      }
    }
    else {
      /* data from mesh used directly */
      BLI_assert(0);
    }
  }
}

/** Ensure #MeshRenderData.vert_color */
static void UNUSED_FUNCTION(mesh_render_data_ensure_vert_color)(MeshRenderData *rdata)
{
  char(*vcol)[3] = rdata->vert_color;
  if (vcol == NULL) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      const int cd_loop_color_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPCOL);
      if (cd_loop_color_offset == -1) {
        goto fallback;
      }

      vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

      BMIter fiter;
      BMFace *efa;
      int i = 0;

      BM_ITER_MESH (efa, &fiter, bm, BM_FACES_OF_MESH) {
        BMLoop *l_iter, *l_first;
        l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
        do {
          const MLoopCol *lcol = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_color_offset);
          vcol[i][0] = lcol->r;
          vcol[i][1] = lcol->g;
          vcol[i][2] = lcol->b;
          i += 1;
        } while ((l_iter = l_iter->next) != l_first);
      }
      BLI_assert(i == rdata->loop_len);
    }
    else {
      if (rdata->mloopcol == NULL) {
        goto fallback;
      }

      vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

      for (int i = 0; i < rdata->loop_len; i++) {
        vcol[i][0] = rdata->mloopcol[i].r;
        vcol[i][1] = rdata->mloopcol[i].g;
        vcol[i][2] = rdata->mloopcol[i].b;
      }
    }
  }
  return;

fallback:
  vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

  for (int i = 0; i < rdata->loop_len; i++) {
    vcol[i][0] = 255;
    vcol[i][1] = 255;
    vcol[i][2] = 255;
  }
}

static float evaluate_vertex_weight(const MDeformVert *dvert, const DRW_MeshWeightState *wstate)
{
  float input = 0.0f;
  bool show_alert_color = false;

  if (wstate->flags & DRW_MESH_WEIGHT_STATE_MULTIPAINT) {
    /* Multi-Paint feature */
    input = BKE_defvert_multipaint_collective_weight(
        dvert,
        wstate->defgroup_len,
        wstate->defgroup_sel,
        wstate->defgroup_sel_count,
        (wstate->flags & DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE) != 0);

    /* make it black if the selected groups have no weight on a vertex */
    if (input == 0.0f) {
      show_alert_color = true;
    }
  }
  else {
    /* default, non tricky behavior */
    input = defvert_find_weight(dvert, wstate->defgroup_active);

    if (input == 0.0f) {
      switch (wstate->alert_mode) {
        case OB_DRAW_GROUPUSER_ACTIVE:
          show_alert_color = true;
          break;

        case OB_DRAW_GROUPUSER_ALL:
          show_alert_color = defvert_is_weight_zero(dvert, wstate->defgroup_len);
          break;
      }
    }
  }

  if (show_alert_color) {
    return -1.0f;
  }
  else {
    CLAMP(input, 0.0f, 1.0f);
    return input;
  }
}

/** Ensure #MeshRenderData.vert_weight */
static void mesh_render_data_ensure_vert_weight(MeshRenderData *rdata,
                                                const struct DRW_MeshWeightState *wstate)
{
  float *vweight = rdata->vert_weight;
  if (vweight == NULL) {
    if (wstate->defgroup_active == -1) {
      goto fallback;
    }

    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
      if (cd_dvert_offset == -1) {
        goto fallback;
      }

      BMIter viter;
      BMVert *eve;
      int i;

      vweight = rdata->vert_weight = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
      BM_ITER_MESH_INDEX (eve, &viter, bm, BM_VERT, i) {
        const MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
        vweight[i] = evaluate_vertex_weight(dvert, wstate);
      }
    }
    else {
      if (rdata->dvert == NULL) {
        goto fallback;
      }

      vweight = rdata->vert_weight = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
      for (int i = 0; i < rdata->vert_len; i++) {
        vweight[i] = evaluate_vertex_weight(&rdata->dvert[i], wstate);
      }
    }
  }
  return;

fallback:
  vweight = rdata->vert_weight = MEM_callocN(sizeof(*vweight) * rdata->vert_len, __func__);

  if ((wstate->defgroup_active < 0) && (wstate->defgroup_len > 0)) {
    copy_vn_fl(vweight, rdata->vert_len, -2.0f);
  }
  else if (wstate->alert_mode != OB_DRAW_GROUPUSER_NONE) {
    copy_vn_fl(vweight, rdata->vert_len, -1.0f);
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Internal Cache Generation
 * \{ */

static uchar mesh_render_data_face_flag(MeshRenderData *rdata, const BMFace *efa, const int cd_ofs)
{
  uchar fflag = 0;

  if (efa == rdata->efa_act) {
    fflag |= VFLAG_FACE_ACTIVE;
  }
  if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    fflag |= VFLAG_FACE_SELECTED;
  }

  if (efa == rdata->efa_act_uv) {
    fflag |= VFLAG_FACE_UV_ACTIVE;
  }
  if ((cd_ofs != -1) && uvedit_face_select_test_ex(rdata->toolsettings, (BMFace *)efa, cd_ofs)) {
    fflag |= VFLAG_FACE_UV_SELECT;
  }

#ifdef WITH_FREESTYLE
  if (rdata->cd.offset.freestyle_face != -1) {
    const FreestyleFace *ffa = BM_ELEM_CD_GET_VOID_P(efa, rdata->cd.offset.freestyle_face);
    if (ffa->flag & FREESTYLE_FACE_MARK) {
      fflag |= VFLAG_FACE_FREESTYLE;
    }
  }
#endif

  return fflag;
}

static void mesh_render_data_edge_flag(const MeshRenderData *rdata,
                                       const BMEdge *eed,
                                       EdgeDrawAttr *eattr)
{
  const ToolSettings *ts = rdata->toolsettings;
  const bool is_vertex_select_mode = (ts != NULL) && (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool is_face_only_select_mode = (ts != NULL) && (ts->selectmode == SCE_SELECT_FACE);

  if (eed == rdata->eed_act) {
    eattr->e_flag |= VFLAG_EDGE_ACTIVE;
  }
  if (!is_vertex_select_mode && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
  }
  if (is_vertex_select_mode && BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) &&
      BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
  if (BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
    eattr->e_flag |= VFLAG_EDGE_SEAM;
  }
  if (!BM_elem_flag_test(eed, BM_ELEM_SMOOTH)) {
    eattr->e_flag |= VFLAG_EDGE_SHARP;
  }

  /* Use active edge color for active face edges because
   * specular highlights make it hard to see T55456#510873.
   *
   * This isn't ideal since it can't be used when mixing edge/face modes
   * but it's still better then not being able to see the active face. */
  if (is_face_only_select_mode) {
    if (rdata->efa_act != NULL) {
      if (BM_edge_in_face(eed, rdata->efa_act)) {
        eattr->e_flag |= VFLAG_EDGE_ACTIVE;
      }
    }
  }

  /* Use a byte for value range */
  if (rdata->cd.offset.crease != -1) {
    float crease = BM_ELEM_CD_GET_FLOAT(eed, rdata->cd.offset.crease);
    if (crease > 0) {
      eattr->crease = (uchar)(crease * 255.0f);
    }
  }
  /* Use a byte for value range */
  if (rdata->cd.offset.bweight != -1) {
    float bweight = BM_ELEM_CD_GET_FLOAT(eed, rdata->cd.offset.bweight);
    if (bweight > 0) {
      eattr->bweight = (uchar)(bweight * 255.0f);
    }
  }
#ifdef WITH_FREESTYLE
  if (rdata->cd.offset.freestyle_edge != -1) {
    const FreestyleEdge *fed = BM_ELEM_CD_GET_VOID_P(eed, rdata->cd.offset.freestyle_edge);
    if (fed->flag & FREESTYLE_EDGE_MARK) {
      eattr->e_flag |= VFLAG_EDGE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_loop_flag(MeshRenderData *rdata,
                                       BMLoop *loop,
                                       const int cd_ofs,
                                       EdgeDrawAttr *eattr)
{
  if (cd_ofs == -1) {
    return;
  }
  MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, cd_ofs);
  if (luv != NULL && (luv->flag & MLOOPUV_PINNED)) {
    eattr->v_flag |= VFLAG_VERT_UV_PINNED;
  }
  if (uvedit_uv_select_test_ex(rdata->toolsettings, loop, cd_ofs)) {
    eattr->v_flag |= VFLAG_VERT_UV_SELECT;
  }
  if (uvedit_edge_select_test_ex(rdata->toolsettings, loop, cd_ofs)) {
    eattr->v_flag |= VFLAG_EDGE_UV_SELECT;
  }
}

static void mesh_render_data_vert_flag(MeshRenderData *rdata,
                                       const BMVert *eve,
                                       EdgeDrawAttr *eattr)
{
  if (eve == rdata->eve_act) {
    eattr->e_flag |= VFLAG_VERT_ACTIVE;
  }
  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
}

static bool add_edit_facedot(MeshRenderData *rdata,
                             GPUVertBuf *vbo,
                             const uint fdot_pos_id,
                             const uint fdot_nor_flag_id,
                             const int poly,
                             const int base_vert_idx)
{
  BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));
  float pnor[3], center[3];
  int facedot_flag;
  if (rdata->edit_bmesh) {
    BMEditMesh *em = rdata->edit_bmesh;
    const BMFace *efa = BM_face_at_index(em->bm, poly);
    if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      return false;
    }
    if (rdata->edit_data && rdata->edit_data->vertexCos) {
      copy_v3_v3(center, rdata->edit_data->polyCos[poly]);
      copy_v3_v3(pnor, rdata->edit_data->polyNos[poly]);
    }
    else {
      BM_face_calc_center_median(efa, center);
      copy_v3_v3(pnor, efa->no);
    }
    facedot_flag = BM_elem_flag_test(efa, BM_ELEM_SELECT) ? ((efa == em->bm->act_face) ? -1 : 1) :
                                                            0;
  }
  else {
    MVert *mvert = rdata->mvert;
    const MPoly *mpoly = rdata->mpoly + poly;
    const MLoop *mloop = rdata->mloop + mpoly->loopstart;

    BKE_mesh_calc_poly_center(mpoly, mloop, mvert, center);
    BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, pnor);
    /* No selection if not in edit mode. */
    facedot_flag = 0;
  }

  GPUPackedNormal nor = GPU_normal_convert_i10_v3(pnor);
  nor.w = facedot_flag;
  GPU_vertbuf_attr_set(vbo, fdot_nor_flag_id, base_vert_idx, &nor);
  GPU_vertbuf_attr_set(vbo, fdot_pos_id, base_vert_idx, center);

  return true;
}
static bool add_edit_facedot_mapped(MeshRenderData *rdata,
                                    GPUVertBuf *vbo,
                                    const uint fdot_pos_id,
                                    const uint fdot_nor_flag_id,
                                    const int poly,
                                    const int base_vert_idx)
{
  BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));
  float pnor[3], center[3];
  const int *p_origindex = rdata->mapped.p_origindex;
  const int p_orig = p_origindex[poly];
  if (p_orig == ORIGINDEX_NONE) {
    return false;
  }
  BMEditMesh *em = rdata->edit_bmesh;
  const BMFace *efa = BM_face_at_index(em->bm, p_orig);
  if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    return false;
  }

  Mesh *me_cage = em->mesh_eval_cage;
  const MVert *mvert = me_cage->mvert;
  const MLoop *mloop = me_cage->mloop;
  const MPoly *mpoly = me_cage->mpoly;

  const MPoly *mp = mpoly + poly;
  const MLoop *ml = mloop + mp->loopstart;

  BKE_mesh_calc_poly_center(mp, ml, mvert, center);
  BKE_mesh_calc_poly_normal(mp, ml, mvert, pnor);

  GPUPackedNormal nor = GPU_normal_convert_i10_v3(pnor);
  nor.w = BM_elem_flag_test(efa, BM_ELEM_SELECT) ? ((efa == em->bm->act_face) ? -1 : 1) : 0;
  GPU_vertbuf_attr_set(vbo, fdot_nor_flag_id, base_vert_idx, &nor);
  GPU_vertbuf_attr_set(vbo, fdot_pos_id, base_vert_idx, center);

  return true;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Vertex Group Selection
 * \{ */

/** Reset the selection structure, deallocating heap memory as appropriate. */
static void drw_mesh_weight_state_clear(struct DRW_MeshWeightState *wstate)
{
  MEM_SAFE_FREE(wstate->defgroup_sel);

  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = -1;
}

/** Copy selection data from one structure to another, including heap memory. */
static void drw_mesh_weight_state_copy(struct DRW_MeshWeightState *wstate_dst,
                                       const struct DRW_MeshWeightState *wstate_src)
{
  MEM_SAFE_FREE(wstate_dst->defgroup_sel);

  memcpy(wstate_dst, wstate_src, sizeof(*wstate_dst));

  if (wstate_src->defgroup_sel) {
    wstate_dst->defgroup_sel = MEM_dupallocN(wstate_src->defgroup_sel);
  }
}

/** Compare two selection structures. */
static bool drw_mesh_weight_state_compare(const struct DRW_MeshWeightState *a,
                                          const struct DRW_MeshWeightState *b)
{
  return a->defgroup_active == b->defgroup_active && a->defgroup_len == b->defgroup_len &&
         a->flags == b->flags && a->alert_mode == b->alert_mode &&
         a->defgroup_sel_count == b->defgroup_sel_count &&
         ((!a->defgroup_sel && !b->defgroup_sel) ||
          (a->defgroup_sel && b->defgroup_sel &&
           memcmp(a->defgroup_sel, b->defgroup_sel, a->defgroup_len * sizeof(bool)) == 0));
}

static void drw_mesh_weight_state_extract(Object *ob,
                                          Mesh *me,
                                          const ToolSettings *ts,
                                          bool paint_mode,
                                          struct DRW_MeshWeightState *wstate)
{
  /* Extract complete vertex weight group selection state and mode flags. */
  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = ob->actdef - 1;
  wstate->defgroup_len = BLI_listbase_count(&ob->defbase);

  wstate->alert_mode = ts->weightuser;

  if (paint_mode && ts->multipaint) {
    /* Multipaint needs to know all selected bones, not just the active group.
     * This is actually a relatively expensive operation, but caching would be difficult. */
    wstate->defgroup_sel = BKE_object_defgroup_selected_get(
        ob, wstate->defgroup_len, &wstate->defgroup_sel_count);

    if (wstate->defgroup_sel_count > 1) {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_MULTIPAINT |
                       (ts->auto_normalize ? DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE : 0);

      if (me->editflag & ME_EDIT_MIRROR_X) {
        BKE_object_defgroup_mirror_selection(ob,
                                             wstate->defgroup_len,
                                             wstate->defgroup_sel,
                                             wstate->defgroup_sel,
                                             &wstate->defgroup_sel_count);
      }
    }
    /* With only one selected bone Multipaint reverts to regular mode. */
    else {
      wstate->defgroup_sel_count = 0;
      MEM_SAFE_FREE(wstate->defgroup_sel);
    }
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh GPUBatch Cache
 * \{ */

typedef enum DRWBatchFlag {
  MBC_SURFACE = (1 << 0),
  MBC_SURFACE_WEIGHTS = (1 << 1),
  MBC_EDIT_TRIANGLES = (1 << 2),
  MBC_EDIT_VERTICES = (1 << 3),
  MBC_EDIT_EDGES = (1 << 4),
  MBC_EDIT_LNOR = (1 << 5),
  MBC_EDIT_FACEDOTS = (1 << 6),
  MBC_EDIT_MESH_ANALYSIS = (1 << 7),
  MBC_EDITUV_FACES_STRECH_AREA = (1 << 8),
  MBC_EDITUV_FACES_STRECH_ANGLE = (1 << 9),
  MBC_EDITUV_FACES = (1 << 10),
  MBC_EDITUV_EDGES = (1 << 11),
  MBC_EDITUV_VERTS = (1 << 12),
  MBC_EDITUV_FACEDOTS = (1 << 13),
  MBC_EDIT_SELECTION_VERTS = (1 << 14),
  MBC_EDIT_SELECTION_EDGES = (1 << 15),
  MBC_EDIT_SELECTION_FACES = (1 << 16),
  MBC_EDIT_SELECTION_FACEDOTS = (1 << 17),
  MBC_ALL_VERTS = (1 << 18),
  MBC_ALL_EDGES = (1 << 19),
  MBC_LOOSE_EDGES = (1 << 20),
  MBC_EDGE_DETECTION = (1 << 21),
  MBC_WIRE_EDGES = (1 << 22),
  MBC_WIRE_LOOPS = (1 << 23),
  MBC_WIRE_LOOPS_UVS = (1 << 24),
  MBC_SURF_PER_MAT = (1 << 25),
} DRWBatchFlag;

#define MBC_EDITUV \
  (MBC_EDITUV_FACES_STRECH_AREA | MBC_EDITUV_FACES_STRECH_ANGLE | MBC_EDITUV_FACES | \
   MBC_EDITUV_EDGES | MBC_EDITUV_VERTS | MBC_EDITUV_FACEDOTS)

typedef struct MeshBatchCache {
  /* In order buffers: All verts only specified once
   * or once per loop. To be used with a GPUIndexBuf. */
  struct {
    /* Vertex data. */
    GPUVertBuf *pos_nor;
    GPUVertBuf *weights;
    /* Loop data. */
    GPUVertBuf *loop_pos_nor;
    GPUVertBuf *loop_uv_tan;
    GPUVertBuf *loop_vcol;
    GPUVertBuf *loop_edge_fac;
    GPUVertBuf *loop_orco;
  } ordered;

  /* Edit Mesh Data:
   * Edit cage can be different from final mesh so vertex count
   * might differ. */
  struct {
    /* TODO(fclem): Reuse ordered.loop_pos_nor and maybe even
     * ordered.loop_uv_tan when cage match final mesh. */
    GPUVertBuf *loop_pos_nor;
    GPUVertBuf *loop_data;
    GPUVertBuf *loop_lnor;
    GPUVertBuf *facedots_pos_nor_data;
    GPUVertBuf *loop_mesh_analysis;
    /* UV data without modifier applied.
     * Vertex count is always the one of the cage. */
    GPUVertBuf *loop_uv;
    GPUVertBuf *loop_uv_data;
    GPUVertBuf *loop_stretch_angle;
    GPUVertBuf *loop_stretch_area;
    GPUVertBuf *facedots_uv;
    GPUVertBuf *facedots_uv_data;
    /* Selection */
    GPUVertBuf *loop_vert_idx;
    GPUVertBuf *loop_edge_idx;
    GPUVertBuf *loop_face_idx;
    GPUVertBuf *facedots_idx;
  } edit;

  /* Index Buffers:
   * Only need to be updated when topology changes. */
  struct {
    /* Indices to verts. */
    GPUIndexBuf *surf_tris;
    GPUIndexBuf *edges_lines;
    GPUIndexBuf *edges_adj_lines;
    GPUIndexBuf *loose_edges_lines;
    /* Indices to vloops. */
    GPUIndexBuf *loops_tris;
    GPUIndexBuf *loops_lines;
    GPUIndexBuf *loops_line_strips;
    /* Edit mode. */
    GPUIndexBuf *edit_loops_points; /* verts */
    GPUIndexBuf *edit_loops_lines;  /* edges */
    GPUIndexBuf *edit_loops_tris;   /* faces */
    /* Edit UVs */
    GPUIndexBuf *edituv_loops_points;      /* verts */
    GPUIndexBuf *edituv_loops_line_strips; /* edges */
    GPUIndexBuf *edituv_loops_tri_fans;    /* faces */
  } ibo;

  struct {
    /* Surfaces / Render */
    GPUBatch *surface;
    GPUBatch *surface_weights;
    /* Edit mode */
    GPUBatch *edit_triangles;
    GPUBatch *edit_vertices;
    GPUBatch *edit_edges;
    GPUBatch *edit_lnor;
    GPUBatch *edit_facedots;
    GPUBatch *edit_mesh_analysis;
    /* Edit UVs */
    GPUBatch *edituv_faces_strech_area;
    GPUBatch *edituv_faces_strech_angle;
    GPUBatch *edituv_faces;
    GPUBatch *edituv_edges;
    GPUBatch *edituv_verts;
    GPUBatch *edituv_facedots;
    /* Edit selection */
    GPUBatch *edit_selection_verts;
    GPUBatch *edit_selection_edges;
    GPUBatch *edit_selection_faces;
    GPUBatch *edit_selection_facedots;
    /* Common display / Other */
    GPUBatch *all_verts;
    GPUBatch *all_edges;
    GPUBatch *loose_edges;
    GPUBatch *edge_detection;
    GPUBatch *wire_edges;     /* Individual edges with face normals. */
    GPUBatch *wire_loops;     /* Loops around faces. */
    GPUBatch *wire_loops_uvs; /* Same as wire_loops but only has uvs. */
  } batch;

  GPUIndexBuf **surf_per_mat_tris;
  GPUBatch **surf_per_mat;

  /* arrays of bool uniform names (and value) that will be use to
   * set srgb conversion for auto attributes.*/
  char *auto_layer_names;
  int *auto_layer_is_srgb;
  int auto_layer_len;

  DRWBatchFlag batch_requested;
  DRWBatchFlag batch_ready;

  /* settings to determine if cache is invalid */
  int edge_len;
  int tri_len;
  int poly_len;
  int vert_len;
  int mat_len;
  bool is_dirty; /* Instantly invalidates cache, skipping mesh check */
  bool is_editmode;
  bool is_uvsyncsel;

  struct DRW_MeshWeightState weight_state;

  DRW_MeshCDMask cd_used, cd_needed, cd_used_over_time;

  int lastmatch;

  /* Valid only if edge_detection is up to date. */
  bool is_manifold;
} MeshBatchCache;

BLI_INLINE void mesh_batch_cache_add_request(MeshBatchCache *cache, DRWBatchFlag new_flag)
{
  atomic_fetch_and_or_uint32((uint32_t *)(&cache->batch_requested), *(uint32_t *)&new_flag);
}

/* GPUBatch cache management. */

static bool mesh_batch_cache_valid(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (cache == NULL) {
    return false;
  }

  if (cache->is_editmode != (me->edit_mesh != NULL)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  return true;
}

static void mesh_batch_cache_init(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (!cache) {
    cache = me->runtime.batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_editmode = me->edit_mesh != NULL;

  if (cache->is_editmode == false) {
    cache->edge_len = mesh_render_edges_len_get(me);
    cache->tri_len = mesh_render_looptri_len_get(me);
    cache->poly_len = mesh_render_polys_len_get(me);
    cache->vert_len = mesh_render_verts_len_get(me);
  }

  cache->mat_len = mesh_render_mat_len_get(me);
  cache->surf_per_mat_tris = MEM_callocN(sizeof(*cache->surf_per_mat_tris) * cache->mat_len,
                                         __func__);
  cache->surf_per_mat = MEM_callocN(sizeof(*cache->surf_per_mat) * cache->mat_len, __func__);

  cache->is_dirty = false;
  cache->batch_ready = 0;
  cache->batch_requested = 0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_validate(Mesh *me)
{
  if (!mesh_batch_cache_valid(me)) {
    mesh_batch_cache_clear(me);
    mesh_batch_cache_init(me);
  }
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
  return me->runtime.batch_cache;
}

static void mesh_batch_cache_check_vertex_group(MeshBatchCache *cache,
                                                const struct DRW_MeshWeightState *wstate)
{
  if (!drw_mesh_weight_state_compare(&cache->weight_state, wstate)) {
    GPU_BATCH_CLEAR_SAFE(cache->batch.surface_weights);
    GPU_VERTBUF_DISCARD_SAFE(cache->ordered.weights);

    cache->batch_ready &= ~MBC_SURFACE_WEIGHTS;

    drw_mesh_weight_state_clear(&cache->weight_state);
  }
}

static void mesh_batch_cache_discard_shaded_tri(MeshBatchCache *cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_pos_nor);
  GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_uv_tan);
  GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_vcol);
  GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_orco);

  if (cache->surf_per_mat_tris) {
    for (int i = 0; i < cache->mat_len; i++) {
      GPU_INDEXBUF_DISCARD_SAFE(cache->surf_per_mat_tris[i]);
    }
  }
  MEM_SAFE_FREE(cache->surf_per_mat_tris);
  if (cache->surf_per_mat) {
    for (int i = 0; i < cache->mat_len; i++) {
      GPU_BATCH_DISCARD_SAFE(cache->surf_per_mat[i]);
    }
  }
  MEM_SAFE_FREE(cache->surf_per_mat);

  cache->batch_ready &= ~MBC_SURF_PER_MAT;

  MEM_SAFE_FREE(cache->auto_layer_names);
  MEM_SAFE_FREE(cache->auto_layer_is_srgb);

  mesh_cd_layers_type_clear(&cache->cd_used);

  cache->mat_len = 0;
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache *cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_stretch_angle);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_stretch_area);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_uv);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_uv_data);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.facedots_uv);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit.facedots_uv_data);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_tri_fans);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_line_strips);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_points);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_strech_area);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_strech_angle);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_edges);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_verts);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_facedots);

  cache->batch_ready &= ~MBC_EDITUV;
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, int mode)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (cache == NULL) {
    return;
  }
  switch (mode) {
    case BKE_MESH_BATCH_DIRTY_SELECT:
      GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_data);
      GPU_VERTBUF_DISCARD_SAFE(cache->edit.facedots_pos_nor_data);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_triangles);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_vertices);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_facedots);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_mesh_analysis);
      cache->batch_ready &= ~(MBC_EDIT_TRIANGLES | MBC_EDIT_VERTICES | MBC_EDIT_EDGES |
                              MBC_EDIT_FACEDOTS | MBC_EDIT_MESH_ANALYSIS);
      /* Because visible UVs depends on edit mode selection, discard everything. */
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_SELECT_PAINT:
      /* Paint mode selection flag is packed inside the nor attrib.
       * Note that it can be slow if auto smooth is enabled. (see T63946) */
      GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_pos_nor);
      GPU_BATCH_DISCARD_SAFE(cache->batch.surface);
      GPU_BATCH_DISCARD_SAFE(cache->batch.wire_loops);
      if (cache->surf_per_mat) {
        for (int i = 0; i < cache->mat_len; i++) {
          GPU_BATCH_DISCARD_SAFE(cache->surf_per_mat[i]);
        }
      }
      cache->batch_ready &= ~(MBC_SURFACE | MBC_WIRE_LOOPS | MBC_SURF_PER_MAT);
      break;
    case BKE_MESH_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    case BKE_MESH_BATCH_DIRTY_SHADING:
      mesh_batch_cache_discard_shaded_tri(cache);
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_ALL:
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT:
      GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_uv_data);
      GPU_VERTBUF_DISCARD_SAFE(cache->edit.facedots_uv_data);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_strech_area);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_strech_angle);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_verts);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_facedots);
      cache->batch_ready &= ~MBC_EDITUV;
      break;
    default:
      BLI_assert(0);
  }
}

static void mesh_batch_cache_clear(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (!cache) {
    return;
  }

  for (int i = 0; i < sizeof(cache->ordered) / sizeof(void *); ++i) {
    GPUVertBuf **vbo = (GPUVertBuf **)&cache->ordered;
    GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
  }
  for (int i = 0; i < sizeof(cache->edit) / sizeof(void *); ++i) {
    GPUVertBuf **vbo = (GPUVertBuf **)&cache->edit;
    GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
  }
  for (int i = 0; i < sizeof(cache->ibo) / sizeof(void *); ++i) {
    GPUIndexBuf **ibo = (GPUIndexBuf **)&cache->ibo;
    GPU_INDEXBUF_DISCARD_SAFE(ibo[i]);
  }
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); ++i) {
    GPUBatch **batch = (GPUBatch **)&cache->batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }

  mesh_batch_cache_discard_shaded_tri(cache);

  mesh_batch_cache_discard_uvedit(cache);

  cache->batch_ready = 0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_free(Mesh *me)
{
  mesh_batch_cache_clear(me);
  MEM_SAFE_FREE(me->runtime.batch_cache);
}

/* GPUBatch cache usage. */

static void mesh_create_edit_vertex_loops(MeshRenderData *rdata,
                                          GPUVertBuf *vbo_pos_nor,
                                          GPUVertBuf *vbo_lnor,
                                          GPUVertBuf *vbo_uv,
                                          GPUVertBuf *vbo_data,
                                          GPUVertBuf *vbo_verts,
                                          GPUVertBuf *vbo_edges,
                                          GPUVertBuf *vbo_faces)
{
#if 0
  const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int edge_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);
#endif
  const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
  const int lvert_len = mesh_render_data_loose_verts_len_get_maybe_mapped(rdata);
  const int ledge_len = mesh_render_data_loose_edges_len_get_maybe_mapped(rdata);
  const int loop_len = mesh_render_data_loops_len_get_maybe_mapped(rdata);
  const int tot_loop_len = loop_len + ledge_len * 2 + lvert_len;
  float(*lnors)[3] = rdata->loop_normals;
  uchar fflag;

  /* Static formats */
  static struct {
    GPUVertFormat sel_id, pos_nor, lnor, flag, uv;
  } format = {{0}};
  static struct {
    uint sel_id, pos, nor, lnor, data, uvs;
  } attr_id;
  if (format.sel_id.attr_len == 0) {
    attr_id.sel_id = GPU_vertformat_attr_add(
        &format.sel_id, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
    attr_id.pos = GPU_vertformat_attr_add(
        &format.pos_nor, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format.pos_nor, "vnor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.lnor = GPU_vertformat_attr_add(
        &format.lnor, "lnor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.data = GPU_vertformat_attr_add(&format.flag, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    attr_id.uvs = GPU_vertformat_attr_add(&format.uv, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format.uv, "pos");
    GPU_vertformat_alias_add(&format.flag, "flag");
  }

  GPUVertBufRaw raw_verts, raw_edges, raw_faces, raw_pos, raw_nor, raw_lnor, raw_uv, raw_data;
  if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor)) {
    GPU_vertbuf_init_with_format(vbo_pos_nor, &format.pos_nor);
    GPU_vertbuf_data_alloc(vbo_pos_nor, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.pos, &raw_pos);
    GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.nor, &raw_nor);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_lnor)) {
    GPU_vertbuf_init_with_format(vbo_lnor, &format.lnor);
    GPU_vertbuf_data_alloc(vbo_lnor, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_lnor, attr_id.lnor, &raw_lnor);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_data)) {
    GPU_vertbuf_init_with_format(vbo_data, &format.flag);
    GPU_vertbuf_data_alloc(vbo_data, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_data, attr_id.data, &raw_data);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_uv)) {
    GPU_vertbuf_init_with_format(vbo_uv, &format.uv);
    GPU_vertbuf_data_alloc(vbo_uv, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_uv, attr_id.uvs, &raw_uv);
  }
  /* Select Idx */
  if (DRW_TEST_ASSIGN_VBO(vbo_verts)) {
    GPU_vertbuf_init_with_format(vbo_verts, &format.sel_id);
    GPU_vertbuf_data_alloc(vbo_verts, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_verts, attr_id.sel_id, &raw_verts);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_edges)) {
    GPU_vertbuf_init_with_format(vbo_edges, &format.sel_id);
    GPU_vertbuf_data_alloc(vbo_edges, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_edges, attr_id.sel_id, &raw_edges);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_faces)) {
    GPU_vertbuf_init_with_format(vbo_faces, &format.sel_id);
    GPU_vertbuf_data_alloc(vbo_faces, tot_loop_len);
    GPU_vertbuf_attr_get_raw_data(vbo_faces, attr_id.sel_id, &raw_faces);
  }

  if (rdata->edit_bmesh && rdata->mapped.use == false) {
    BMesh *bm = rdata->edit_bmesh->bm;
    BMIter iter_efa, iter_loop, iter_vert;
    BMFace *efa;
    BMEdge *eed;
    BMVert *eve;
    BMLoop *loop;
    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    /* Face Loops */
    BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
      int fidx = BM_elem_index_get(efa);
      if (vbo_data) {
        fflag = mesh_render_data_face_flag(rdata, efa, cd_loop_uv_offset);
      }
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        if (vbo_pos_nor) {
          GPUPackedNormal *vnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor);
          *vnor = GPU_normal_convert_i10_v3(loop->v->no);
          copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), loop->v->co);
        }
        if (vbo_lnor) {
          const float *nor = (lnors) ? lnors[BM_elem_index_get(loop)] : efa->no;
          GPUPackedNormal *lnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_lnor);
          *lnor = GPU_normal_convert_i10_v3(nor);
        }
        if (vbo_data) {
          EdgeDrawAttr eattr = {.v_flag = fflag};
          mesh_render_data_edge_flag(rdata, loop->e, &eattr);
          mesh_render_data_vert_flag(rdata, loop->v, &eattr);
          mesh_render_data_loop_flag(rdata, loop, cd_loop_uv_offset, &eattr);
          memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
        }
        if (vbo_uv) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, cd_loop_uv_offset);
          copy_v2_v2(GPU_vertbuf_raw_step(&raw_uv), luv->uv);
        }
        /* Select Idx */
        if (vbo_verts) {
          int vidx = BM_elem_index_get(loop->v);
          *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
        }
        if (vbo_edges) {
          int eidx = BM_elem_index_get(loop->e);
          *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
        }
        if (vbo_faces) {
          *((uint *)GPU_vertbuf_raw_step(&raw_faces)) = fidx;
        }
      }
    }
    /* Loose edges */
    for (int e = 0; e < ledge_len; e++) {
      eed = BM_edge_at_index(bm, rdata->loose_edges[e]);
      BM_ITER_ELEM (eve, &iter_vert, eed, BM_VERTS_OF_EDGE) {
        if (vbo_pos_nor) {
          GPUPackedNormal *vnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor);
          *vnor = GPU_normal_convert_i10_v3(eve->no);
          copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), eve->co);
        }
        if (vbo_data) {
          EdgeDrawAttr eattr = {0};
          mesh_render_data_edge_flag(rdata, eed, &eattr);
          mesh_render_data_vert_flag(rdata, eve, &eattr);
          memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
        }
        if (vbo_lnor) {
          memset(GPU_vertbuf_raw_step(&raw_lnor), 0, sizeof(GPUPackedNormal));
        }
        /* Select Idx */
        if (vbo_verts) {
          int vidx = BM_elem_index_get(eve);
          *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
        }
        if (vbo_edges) {
          int eidx = BM_elem_index_get(eed);
          *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
        }
      }
    }
    /* Loose verts */
    for (int e = 0; e < lvert_len; e++) {
      eve = BM_vert_at_index(bm, rdata->loose_verts[e]);
      if (vbo_pos_nor) {
        GPUPackedNormal *vnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor);
        *vnor = GPU_normal_convert_i10_v3(eve->no);
        copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), eve->co);
      }
      if (vbo_lnor) {
        memset(GPU_vertbuf_raw_step(&raw_lnor), 0, sizeof(GPUPackedNormal));
      }
      if (vbo_data) {
        EdgeDrawAttr eattr = {0};
        mesh_render_data_vert_flag(rdata, eve, &eattr);
        memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
      }
      /* Select Idx */
      if (vbo_verts) {
        int vidx = BM_elem_index_get(eve);
        *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
      }
    }
  }
  else if (rdata->mapped.use == true) {
    BMesh *bm = rdata->edit_bmesh->bm;
    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    const MPoly *mpoly = rdata->mapped.me_cage->mpoly;
    const MEdge *medge = rdata->mapped.me_cage->medge;
    const MVert *mvert = rdata->mapped.me_cage->mvert;
    const MLoop *mloop = rdata->mapped.me_cage->mloop;

    const int *v_origindex = rdata->mapped.v_origindex;
    const int *e_origindex = rdata->mapped.e_origindex;
    const int *p_origindex = rdata->mapped.p_origindex;

    /* Face Loops */
    for (int poly = 0; poly < poly_len; poly++, mpoly++) {
      const MLoop *l = &mloop[mpoly->loopstart];
      int fidx = p_origindex[poly];
      BMFace *efa = NULL;
      if (vbo_data) {
        fflag = 0;
        if (fidx != ORIGINDEX_NONE) {
          efa = BM_face_at_index(bm, fidx);
          fflag = mesh_render_data_face_flag(rdata, efa, cd_loop_uv_offset);
        }
      }
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        if (vbo_pos_nor) {
          copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert[l->v].co);
        }
        if (vbo_lnor || vbo_pos_nor) {
          GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[l->v].no);
          if (vbo_pos_nor) {
            *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor) = vnor;
          }
          if (vbo_lnor) {
            /* Mapped does not support lnors yet. */
            *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_lnor) = vnor;
          }
        }
        if (vbo_data) {
          EdgeDrawAttr eattr = {.v_flag = fflag};
          int vidx = v_origindex[l->v];
          int eidx = e_origindex[l->e];
          if (vidx != ORIGINDEX_NONE) {
            BMVert *eve = BM_vert_at_index(bm, vidx);
            mesh_render_data_vert_flag(rdata, eve, &eattr);
          }
          if (eidx != ORIGINDEX_NONE) {
            BMEdge *eed = BM_edge_at_index(bm, eidx);
            mesh_render_data_edge_flag(rdata, eed, &eattr);
            if (efa) {
              BMLoop *loop = BM_face_edge_share_loop(efa, eed);
              if (loop) {
                mesh_render_data_loop_flag(rdata, loop, cd_loop_uv_offset, &eattr);
              }
            }
          }
          memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
        }
        if (vbo_uv) {
          MLoopUV *luv = &rdata->mloopuv[mpoly->loopstart + i];
          copy_v2_v2(GPU_vertbuf_raw_step(&raw_uv), luv->uv);
        }
        /* Select Idx */
        if (vbo_verts) {
          int vidx = v_origindex[l->v];
          *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
        }
        if (vbo_edges) {
          int eidx = e_origindex[l->e];
          *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
        }
        if (vbo_faces) {
          *((uint *)GPU_vertbuf_raw_step(&raw_faces)) = fidx;
        }
      }
    }
    /* Loose edges */
    for (int j = 0; j < ledge_len; j++) {
      const int e = rdata->mapped.loose_edges[j];
      for (int i = 0; i < 2; ++i) {
        int v = (i == 0) ? medge[e].v1 : medge[e].v2;
        if (vbo_pos_nor) {
          GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[v].no);
          *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor) = vnor;
          copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert[v].co);
        }
        if (vbo_lnor) {
          memset(GPU_vertbuf_raw_step(&raw_lnor), 0, sizeof(GPUPackedNormal));
        }
        if (vbo_data) {
          EdgeDrawAttr eattr = {0};
          int vidx = v_origindex[v];
          int eidx = e_origindex[e];
          if (vidx != ORIGINDEX_NONE) {
            BMVert *eve = BM_vert_at_index(bm, vidx);
            mesh_render_data_vert_flag(rdata, eve, &eattr);
          }
          if (eidx != ORIGINDEX_NONE) {
            BMEdge *eed = BM_edge_at_index(bm, eidx);
            mesh_render_data_edge_flag(rdata, eed, &eattr);
          }
          memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
        }
        /* Select Idx */
        if (vbo_verts) {
          int vidx = v_origindex[v];
          *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
        }
        if (vbo_edges) {
          int eidx = e_origindex[e];
          *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
        }
      }
    }
    /* Loose verts */
    for (int i = 0; i < lvert_len; i++) {
      const int v = rdata->mapped.loose_verts[i];
      if (vbo_pos_nor) {
        GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[v].no);
        *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor) = vnor;
        copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert[v].co);
      }
      if (vbo_lnor) {
        memset(GPU_vertbuf_raw_step(&raw_lnor), 0, sizeof(GPUPackedNormal));
      }
      if (vbo_data) {
        EdgeDrawAttr eattr = {0};
        int vidx = v_origindex[v];
        if (vidx != ORIGINDEX_NONE) {
          BMVert *eve = BM_vert_at_index(bm, vidx);
          mesh_render_data_vert_flag(rdata, eve, &eattr);
        }
        memcpy(GPU_vertbuf_raw_step(&raw_data), &eattr, sizeof(EdgeDrawAttr));
      }
      /* Select Idx */
      if (vbo_verts) {
        int vidx = v_origindex[v];
        *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
      }
    }
  }
  else {
    const MPoly *mpoly = rdata->mpoly;
    const MVert *mvert = rdata->mvert;
    const MLoop *mloop = rdata->mloop;

    const int *v_origindex = CustomData_get_layer(&rdata->me->vdata, CD_ORIGINDEX);
    const int *e_origindex = CustomData_get_layer(&rdata->me->edata, CD_ORIGINDEX);
    const int *p_origindex = CustomData_get_layer(&rdata->me->pdata, CD_ORIGINDEX);

    /* Face Loops */
    for (int poly = 0; poly < poly_len; poly++, mpoly++) {
      const MLoop *l = &mloop[mpoly->loopstart];
      int fidx = p_origindex ? p_origindex[poly] : poly;
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        if (vbo_pos_nor) {
          copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert[l->v].co);
        }
        if (vbo_lnor || vbo_pos_nor) {
          GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[l->v].no);
          if (vbo_pos_nor) {
            *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_nor) = vnor;
          }
          if (vbo_lnor) {
            /* Mapped does not support lnors yet. */
            *(GPUPackedNormal *)GPU_vertbuf_raw_step(&raw_lnor) = vnor;
          }
        }
        if (vbo_uv) {
          MLoopUV *luv = &rdata->mloopuv[mpoly->loopstart + i];
          copy_v2_v2(GPU_vertbuf_raw_step(&raw_uv), luv->uv);
        }
        /* Select Idx */
        if (vbo_verts) {
          int vidx = v_origindex ? v_origindex[l->v] : l->v;
          *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
        }
        if (vbo_edges) {
          int eidx = e_origindex ? e_origindex[l->e] : l->e;
          *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
        }
        if (vbo_faces) {
          *((uint *)GPU_vertbuf_raw_step(&raw_faces)) = fidx;
        }
      }
    }
    /* TODO(fclem): Until we find a way to detect
     * loose verts easily outside of edit mode, this
     * will remain disabled. */
#if 0
    /* Loose edges */
    for (int e = 0; e < edge_len; e++, medge++) {
      int eidx = e_origindex[e];
      if (eidx != ORIGINDEX_NONE && (medge->flag & ME_LOOSEEDGE)) {
        for (int i = 0; i < 2; ++i) {
          int vidx = (i == 0) ? medge->v1 : medge->v2;
          if (vbo_pos) {
            copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert[vidx].co);
          }
          if (vbo_verts) {
            *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
          }
          if (vbo_edges) {
            *((uint *)GPU_vertbuf_raw_step(&raw_edges)) = eidx;
          }
        }
      }
    }
    /* Loose verts */
    for (int v = 0; v < vert_len; v++, mvert++) {
      int vidx = v_origindex[v];
      if (vidx != ORIGINDEX_NONE) {
        MVert *eve = BM_vert_at_index(bm, vidx);
        if (eve->e == NULL) {
          if (vbo_pos) {
            copy_v3_v3(GPU_vertbuf_raw_step(&raw_pos), mvert->co);
          }
          if (vbo_verts) {
            *((uint *)GPU_vertbuf_raw_step(&raw_verts)) = vidx;
          }
        }
      }
    }
#endif
  }
  /* Don't resize */
}

/* TODO: We could use gl_PrimitiveID as index instead of using another VBO. */
static void mesh_create_edit_facedots_select_id(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);

  static GPUVertFormat format = {0};
  static struct {
    uint idx;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.idx = GPU_vertformat_attr_add(&format, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }

  GPUVertBufRaw idx_step;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, poly_len);
  GPU_vertbuf_attr_get_raw_data(vbo, attr_id.idx, &idx_step);

  /* Keep in sync with mesh_create_edit_facedots(). */
  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      for (int poly = 0; poly < poly_len; poly++) {
        const BMFace *efa = BM_face_at_index(rdata->edit_bmesh->bm, poly);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          *((uint *)GPU_vertbuf_raw_step(&idx_step)) = poly;
        }
      }
    }
    else {
      for (int poly = 0; poly < poly_len; poly++) {
        *((uint *)GPU_vertbuf_raw_step(&idx_step)) = poly;
      }
    }
  }
  else {
    const int *p_origindex = rdata->mapped.p_origindex;
    for (int poly = 0; poly < poly_len; poly++) {
      const int p_orig = p_origindex[poly];
      if (p_orig != ORIGINDEX_NONE) {
        const BMFace *efa = BM_face_at_index(rdata->edit_bmesh->bm, p_orig);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          *((uint *)GPU_vertbuf_raw_step(&idx_step)) = poly;
        }
      }
    }
  }

  /* Resize & Finish */
  int facedot_len_used = GPU_vertbuf_raw_used(&idx_step);
  if (facedot_len_used != poly_len) {
    GPU_vertbuf_data_resize(vbo, facedot_len_used);
  }
}

static void mesh_create_pos_and_nor(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  static GPUVertFormat format = {0};
  static struct {
    uint pos, nor;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter;
      BMVert *eve;
      uint i;

      mesh_render_data_ensure_vert_normals_pack(rdata);
      GPUPackedNormal *vnor = rdata->vert_normals_pack;

      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        GPU_vertbuf_attr_set(vbo, attr_id.pos, i, eve->co);
        GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor[i]);
      }
      BLI_assert(i == vbo_len_capacity);
    }
    else {
      for (int i = 0; i < vbo_len_capacity; i++) {
        const MVert *mv = &rdata->mvert[i];
        GPUPackedNormal vnor_pack = GPU_normal_convert_i10_s3(mv->no);
        vnor_pack.w = (mv->flag & ME_HIDE) ? -1 : ((mv->flag & SELECT) ? 1 : 0);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, i, rdata->mvert[i].co);
        GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor_pack);
      }
    }
  }
  else {
    const MVert *mvert = rdata->mapped.me_cage->mvert;
    const int *v_origindex = rdata->mapped.v_origindex;
    for (int i = 0; i < vbo_len_capacity; i++) {
      const int v_orig = v_origindex[i];
      if (v_orig != ORIGINDEX_NONE) {
        const MVert *mv = &mvert[i];
        GPUPackedNormal vnor_pack = GPU_normal_convert_i10_s3(mv->no);
        vnor_pack.w = (mv->flag & ME_HIDE) ? -1 : ((mv->flag & SELECT) ? 1 : 0);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, i, mv->co);
        GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor_pack);
      }
    }
  }
}

static void mesh_create_weights(MeshRenderData *rdata,
                                GPUVertBuf *vbo,
                                DRW_MeshWeightState *wstate)
{
  static GPUVertFormat format = {0};
  static struct {
    uint weight;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.weight = GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);

  mesh_render_data_ensure_vert_weight(rdata, wstate);
  const float *vert_weight = rdata->vert_weight;

  GPU_vertbuf_init_with_format(vbo, &format);
  /* Meh, another allocation / copy for no benefit.
   * Needed because rdata->vert_weight is freed afterwards and
   * GPU module don't have a GPU_vertbuf_data_from_memory or similar. */
  /* TODO get rid of the extra allocation/copy. */
  GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
  GPU_vertbuf_attr_fill(vbo, attr_id.weight, vert_weight);
}

static float mesh_loop_edge_factor_get(const float f_no[3],
                                       const float v_co[3],
                                       const float v_no[3],
                                       const float v_next_co[3])
{
  float enor[3], evec[3];
  sub_v3_v3v3(evec, v_next_co, v_co);
  cross_v3_v3v3(enor, v_no, evec);
  normalize_v3(enor);
  float d = fabsf(dot_v3v3(enor, f_no));
  /* Rescale to the slider range. */
  d *= (1.0f / 0.065f);
  CLAMP(d, 0.0f, 1.0f);
  return d;
}

static void vertbuf_raw_step_u8(GPUVertBufRaw *wd_step, const uchar wiredata)
{
  *((uchar *)GPU_vertbuf_raw_step(wd_step)) = wiredata;
}

static void vertbuf_raw_step_u8_to_f32(GPUVertBufRaw *wd_step, const uchar wiredata)
{
  *((float *)GPU_vertbuf_raw_step(wd_step)) = wiredata / 255.0f;
}

static void mesh_create_loop_edge_fac(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  static GPUVertFormat format = {0};
  static struct {
    uint wd;
  } attr_id;
  static union {
    float f;
    uchar u;
  } data;
  static void (*vertbuf_raw_step)(GPUVertBufRaw *, const uchar);
  if (format.attr_len == 0) {
    if (!GPU_crappy_amd_driver()) {
      /* Some AMD drivers strangely crash with a vbo with this format. */
      attr_id.wd = GPU_vertformat_attr_add(
          &format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
      vertbuf_raw_step = vertbuf_raw_step_u8;
      data.u = UCHAR_MAX;
    }
    else {
      attr_id.wd = GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
      vertbuf_raw_step = vertbuf_raw_step_u8_to_f32;
      data.f = 1.0f;
    }
  }
  const int poly_len = mesh_render_data_polys_len_get(rdata);
  const int loop_len = mesh_render_data_loops_len_get(rdata);
  const int edge_len = mesh_render_data_edges_len_get(rdata);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loop_len);

  GPUVertBufRaw wd_step;
  GPU_vertbuf_attr_get_raw_data(vbo, attr_id.wd, &wd_step);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter_efa, iter_loop;
      BMFace *efa;
      BMLoop *loop;
      uint f;

      BM_ITER_MESH_INDEX (efa, &iter_efa, bm, BM_FACES_OF_MESH, f) {
        BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
          float ratio = mesh_loop_edge_factor_get(
              efa->no, loop->v->co, loop->v->no, loop->next->v->co);
          vertbuf_raw_step(&wd_step, ratio * 255);
        }
      }
      BLI_assert(GPU_vertbuf_raw_used(&wd_step) == loop_len);
    }
    else {
      const MVert *mvert = rdata->mvert;
      const MPoly *mpoly = rdata->mpoly;
      const MLoop *mloop = rdata->mloop;
      MEdge *medge = (MEdge *)rdata->medge;
      bool use_edge_render = false;

      /* TODO(fclem) We don't need them to be packed. But we need rdata->poly_normals */
      mesh_render_data_ensure_poly_normals_pack(rdata);

      /* Reset flag */
      for (int edge = 0; edge < edge_len; ++edge) {
        /* NOTE: not thread safe. */
        medge[edge].flag &= ~ME_EDGE_TMP_TAG;

        /* HACK(fclem) Feels like a hack. Detecting the need for edge render. */
        if ((medge[edge].flag & ME_EDGERENDER) == 0) {
          use_edge_render = true;
        }
      }

      for (int a = 0; a < poly_len; a++, mpoly++) {
        const float *fnor = rdata->poly_normals[a];
        for (int b = 0; b < mpoly->totloop; b++) {
          const MLoop *ml1 = &mloop[mpoly->loopstart + b];
          const MLoop *ml2 = &mloop[mpoly->loopstart + (b + 1) % mpoly->totloop];

          /* Will only work for edges that have an odd number of faces connected. */
          MEdge *ed = (MEdge *)rdata->medge + ml1->e;
          ed->flag ^= ME_EDGE_TMP_TAG;

          if (use_edge_render) {
            vertbuf_raw_step(&wd_step, (ed->flag & ME_EDGERENDER) ? 255 : 0);
          }
          else {
            float vnor_f[3];
            normal_short_to_float_v3(vnor_f, mvert[ml1->v].no);
            float ratio = mesh_loop_edge_factor_get(
                fnor, mvert[ml1->v].co, vnor_f, mvert[ml2->v].co);
            vertbuf_raw_step(&wd_step, ratio * 253 + 1);
          }
        }
      }
      /* Gather non-manifold edges. */
      for (int l = 0; l < loop_len; l++, mloop++) {
        MEdge *ed = (MEdge *)rdata->medge + mloop->e;
        if (ed->flag & ME_EDGE_TMP_TAG) {
          GPU_vertbuf_attr_set(vbo, attr_id.wd, l, &data);
        }
      }

      BLI_assert(loop_len == GPU_vertbuf_raw_used(&wd_step));
    }
  }
  else {
    BLI_assert(0);
  }
}

static void mesh_create_loop_pos_and_nor(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  /* TODO deduplicate format creation*/
  static GPUVertFormat format = {0};
  static struct {
    uint pos, nor;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
  const int poly_len = mesh_render_data_polys_len_get(rdata);
  const int loop_len = mesh_render_data_loops_len_get(rdata);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loop_len);

  GPUVertBufRaw pos_step, nor_step;
  GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);
  GPU_vertbuf_attr_get_raw_data(vbo, attr_id.nor, &nor_step);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      const GPUPackedNormal *vnor, *pnor;
      const float(*lnors)[3] = rdata->loop_normals;
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter_efa, iter_loop;
      BMFace *efa;
      BMLoop *loop;
      uint f;

      if (rdata->loop_normals == NULL) {
        mesh_render_data_ensure_poly_normals_pack(rdata);
        mesh_render_data_ensure_vert_normals_pack(rdata);
        vnor = rdata->vert_normals_pack;
        pnor = rdata->poly_normals_pack;
      }

      BM_ITER_MESH_INDEX (efa, &iter_efa, bm, BM_FACES_OF_MESH, f) {
        const bool face_smooth = BM_elem_flag_test(efa, BM_ELEM_SMOOTH);

        BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
          BLI_assert(GPU_vertbuf_raw_used(&pos_step) == BM_elem_index_get(loop));
          copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), loop->v->co);

          if (lnors) {
            GPUPackedNormal plnor = GPU_normal_convert_i10_v3(lnors[BM_elem_index_get(loop)]);
            *((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = plnor;
          }
          else if (!face_smooth) {
            *((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = pnor[f];
          }
          else {
            *((GPUPackedNormal *)GPU_vertbuf_raw_step(
                &nor_step)) = vnor[BM_elem_index_get(loop->v)];
          }
        }
      }
      BLI_assert(GPU_vertbuf_raw_used(&pos_step) == loop_len);
    }
    else {
      const MVert *mvert = rdata->mvert;
      const MPoly *mpoly = rdata->mpoly;

      if (rdata->loop_normals == NULL) {
        mesh_render_data_ensure_poly_normals_pack(rdata);
      }

      for (int a = 0; a < poly_len; a++, mpoly++) {
        const MLoop *mloop = rdata->mloop + mpoly->loopstart;
        const float(*lnors)[3] = (rdata->loop_normals) ? &rdata->loop_normals[mpoly->loopstart] :
                                                         NULL;
        const GPUPackedNormal *fnor = (mpoly->flag & ME_SMOOTH) ? NULL :
                                                                  &rdata->poly_normals_pack[a];
        const int hide_select_flag = (mpoly->flag & ME_HIDE) ?
                                         -1 :
                                         ((mpoly->flag & ME_FACE_SEL) ? 1 : 0);
        for (int b = 0; b < mpoly->totloop; b++, mloop++) {
          copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop->v].co);
          GPUPackedNormal *pnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step);
          if (lnors) {
            *pnor = GPU_normal_convert_i10_v3(lnors[b]);
          }
          else if (fnor) {
            *pnor = *fnor;
          }
          else {
            *pnor = GPU_normal_convert_i10_s3(mvert[mloop->v].no);
          }
          pnor->w = hide_select_flag;
        }
      }

      BLI_assert(loop_len == GPU_vertbuf_raw_used(&pos_step));
    }
  }
  else {
    const int *p_origindex = rdata->mapped.p_origindex;
    const MVert *mvert = rdata->mvert;
    const MPoly *mpoly = rdata->mpoly;

    if (rdata->loop_normals == NULL) {
      mesh_render_data_ensure_poly_normals_pack(rdata);
    }

    for (int a = 0; a < poly_len; a++, mpoly++) {
      const MLoop *mloop = rdata->mloop + mpoly->loopstart;
      const float(*lnors)[3] = (rdata->loop_normals) ? &rdata->loop_normals[mpoly->loopstart] :
                                                       NULL;
      const GPUPackedNormal *fnor = (mpoly->flag & ME_SMOOTH) ? NULL :
                                                                &rdata->poly_normals_pack[a];
      if (p_origindex[a] == ORIGINDEX_NONE) {
        continue;
      }
      for (int b = 0; b < mpoly->totloop; b++, mloop++) {
        copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop->v].co);
        GPUPackedNormal *pnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step);
        if (lnors) {
          *pnor = GPU_normal_convert_i10_v3(lnors[b]);
        }
        else if (fnor) {
          *pnor = *fnor;
        }
        else {
          *pnor = GPU_normal_convert_i10_s3(mvert[mloop->v].no);
        }
      }
    }
  }

  int vbo_len_used = GPU_vertbuf_raw_used(&pos_step);
  if (vbo_len_used < loop_len) {
    GPU_vertbuf_data_resize(vbo, vbo_len_used);
  }
}

static void mesh_create_loop_orco(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  const uint loops_len = mesh_render_data_loops_len_get(rdata);

  /* initialize vertex format */
  GPUVertFormat format = {0};
  GPUVertBufRaw vbo_step;

  /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex attribs.
   * This is a substential waste of Vram and should be done another way. Unfortunately,
   * at the time of writting, I did not found any other "non disruptive" alternative. */
  uint attr_id = GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loops_len);
  GPU_vertbuf_attr_get_raw_data(vbo, attr_id, &vbo_step);

  if (rdata->edit_bmesh) {
    BMesh *bm = rdata->edit_bmesh->bm;
    BMIter iter_efa, iter_loop;
    BMFace *efa;
    BMLoop *loop;

    BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        float *data = (float *)GPU_vertbuf_raw_step(&vbo_step);
        copy_v3_v3(data, rdata->orco[BM_elem_index_get(loop->v)]);
        data[3] = 0.0; /* Tag as not a generic attrib */
      }
    }
  }
  else {
    for (uint l = 0; l < loops_len; l++) {
      float *data = (float *)GPU_vertbuf_raw_step(&vbo_step);
      copy_v3_v3(data, rdata->orco[rdata->mloop[l].v]);
      data[3] = 0.0; /* Tag as not a generic attrib */
    }
  }
}

static void mesh_create_loop_uv_and_tan(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  const uint loops_len = mesh_render_data_loops_len_get(rdata);
  const uint uv_len = rdata->cd.layers.uv_len;
  const uint tangent_len = rdata->cd.layers.tangent_len;
  const uint layers_combined_len = uv_len + tangent_len;

  GPUVertBufRaw *layers_combined_step = BLI_array_alloca(layers_combined_step,
                                                         layers_combined_len);
  GPUVertBufRaw *uv_step = layers_combined_step;
  GPUVertBufRaw *tangent_step = uv_step + uv_len;

  uint *layers_combined_id = BLI_array_alloca(layers_combined_id, layers_combined_len);
  uint *uv_id = layers_combined_id;
  uint *tangent_id = uv_id + uv_len;

  /* initialize vertex format */
  GPUVertFormat format = {0};

  for (uint i = 0; i < uv_len; i++) {
    const char *attr_name = mesh_render_data_uv_layer_uuid_get(rdata, i);
#if 0 /* these are clamped. Maybe use them as an option in the future */
    uv_id[i] = GPU_vertformat_attr_add(
        &format, attr_name, GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
#else
    uv_id[i] = GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
#endif
    /* Auto Name */
    attr_name = mesh_render_data_uv_auto_layer_uuid_get(rdata, i);
    GPU_vertformat_alias_add(&format, attr_name);

    if (i == rdata->cd.layers.uv_active) {
      GPU_vertformat_alias_add(&format, "u");
    }
    if (i == rdata->cd.layers.uv_mask_active) {
      GPU_vertformat_alias_add(&format, "mu");
    }
  }

  for (uint i = 0; i < tangent_len; i++) {
    const char *attr_name = mesh_render_data_tangent_layer_uuid_get(rdata, i);
#ifdef USE_COMP_MESH_DATA
    tangent_id[i] = GPU_vertformat_attr_add(
        &format, attr_name, GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
#else
    tangent_id[i] = GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
#endif
    if (i == rdata->cd.layers.tangent_active) {
      GPU_vertformat_alias_add(&format, "t");
    }
  }

  /* HACK: Create a dummy attribute in case there is no valid UV/tangent layer. */
  if (layers_combined_len == 0) {
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loops_len);

  for (uint i = 0; i < uv_len; i++) {
    GPU_vertbuf_attr_get_raw_data(vbo, uv_id[i], &uv_step[i]);
  }
  for (uint i = 0; i < tangent_len; i++) {
    GPU_vertbuf_attr_get_raw_data(vbo, tangent_id[i], &tangent_step[i]);
  }

  if (rdata->edit_bmesh) {
    BMesh *bm = rdata->edit_bmesh->bm;
    BMIter iter_efa, iter_loop;
    BMFace *efa;
    BMLoop *loop;

    BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        /* UVs */
        for (uint j = 0; j < uv_len; j++) {
          const uint layer_offset = rdata->cd.offset.uv[j];
          const float *elem = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(loop, layer_offset))->uv;
          copy_v2_v2(GPU_vertbuf_raw_step(&uv_step[j]), elem);
        }
        /* TANGENTs */
        for (uint j = 0; j < tangent_len; j++) {
          float(*layer_data)[4] = rdata->cd.layers.tangent[j];
          const float *elem = layer_data[BM_elem_index_get(loop)];
#ifdef USE_COMP_MESH_DATA
          normal_float_to_short_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#else
          copy_v4_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#endif
        }
      }
    }
  }
  else {
    for (uint loop = 0; loop < loops_len; loop++) {
      /* UVs */
      for (uint j = 0; j < uv_len; j++) {
        const MLoopUV *layer_data = rdata->cd.layers.uv[j];
        const float *elem = layer_data[loop].uv;
        copy_v2_v2(GPU_vertbuf_raw_step(&uv_step[j]), elem);
      }
      /* TANGENTs */
      for (uint j = 0; j < tangent_len; j++) {
        float(*layer_data)[4] = rdata->cd.layers.tangent[j];
        const float *elem = layer_data[loop];
#ifdef USE_COMP_MESH_DATA
        normal_float_to_short_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#else
        copy_v4_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#endif
      }
    }
  }

#ifndef NDEBUG
  /* Check all layers are write aligned. */
  if (layers_combined_len > 0) {
    int vbo_len_used = GPU_vertbuf_raw_used(&layers_combined_step[0]);
    for (uint i = 0; i < layers_combined_len; i++) {
      BLI_assert(vbo_len_used == GPU_vertbuf_raw_used(&layers_combined_step[i]));
    }
  }
#endif

#undef USE_COMP_MESH_DATA
}

static void mesh_create_loop_vcol(MeshRenderData *rdata, GPUVertBuf *vbo)
{
  const uint loops_len = mesh_render_data_loops_len_get(rdata);
  const uint vcol_len = rdata->cd.layers.vcol_len;

  GPUVertBufRaw *vcol_step = BLI_array_alloca(vcol_step, vcol_len);
  uint *vcol_id = BLI_array_alloca(vcol_id, vcol_len);

  /* initialize vertex format */
  GPUVertFormat format = {0};

  for (uint i = 0; i < vcol_len; i++) {
    const char *attr_name = mesh_render_data_vcol_layer_uuid_get(rdata, i);
    vcol_id[i] = GPU_vertformat_attr_add(
        &format, attr_name, GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    /* Auto layer */
    if (rdata->cd.layers.auto_vcol[i]) {
      attr_name = mesh_render_data_vcol_auto_layer_uuid_get(rdata, i);
      GPU_vertformat_alias_add(&format, attr_name);
    }
    if (i == rdata->cd.layers.vcol_active) {
      GPU_vertformat_alias_add(&format, "c");
    }
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loops_len);

  for (uint i = 0; i < vcol_len; i++) {
    GPU_vertbuf_attr_get_raw_data(vbo, vcol_id[i], &vcol_step[i]);
  }

  if (rdata->edit_bmesh) {
    BMesh *bm = rdata->edit_bmesh->bm;
    BMIter iter_efa, iter_loop;
    BMFace *efa;
    BMLoop *loop;

    BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        for (uint j = 0; j < vcol_len; j++) {
          const uint layer_offset = rdata->cd.offset.vcol[j];
          const uchar *elem = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(loop, layer_offset))->r;
          copy_v3_v3_uchar(GPU_vertbuf_raw_step(&vcol_step[j]), elem);
        }
      }
    }
  }
  else {
    for (uint loop = 0; loop < loops_len; loop++) {
      for (uint j = 0; j < vcol_len; j++) {
        const MLoopCol *layer_data = rdata->cd.layers.vcol[j];
        const uchar *elem = &layer_data[loop].r;
        copy_v3_v3_uchar(GPU_vertbuf_raw_step(&vcol_step[j]), elem);
      }
    }
  }

#ifndef NDEBUG
  /* Check all layers are write aligned. */
  if (vcol_len > 0) {
    int vbo_len_used = GPU_vertbuf_raw_used(&vcol_step[0]);
    for (uint i = 0; i < vcol_len; i++) {
      BLI_assert(vbo_len_used == GPU_vertbuf_raw_used(&vcol_step[i]));
    }
  }
#endif

#undef USE_COMP_MESH_DATA
}

static void mesh_create_edit_facedots(MeshRenderData *rdata, GPUVertBuf *vbo_facedots_pos_nor_data)
{
  const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
  const int verts_facedot_len = poly_len;
  int facedot_len_used = 0;

  static struct {
    uint fdot_pos, fdot_nor_flag;
  } attr_id;
  static GPUVertFormat facedot_format = {0};
  if (facedot_format.attr_len == 0) {
    attr_id.fdot_pos = GPU_vertformat_attr_add(
        &facedot_format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.fdot_nor_flag = GPU_vertformat_attr_add(
        &facedot_format, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  if (DRW_TEST_ASSIGN_VBO(vbo_facedots_pos_nor_data)) {
    GPU_vertbuf_init_with_format(vbo_facedots_pos_nor_data, &facedot_format);
    GPU_vertbuf_data_alloc(vbo_facedots_pos_nor_data, verts_facedot_len);
    /* TODO(fclem): Maybe move data generation to mesh_render_data_create() */
    if (rdata->edit_bmesh) {
      if (rdata->edit_data && rdata->edit_data->vertexCos != NULL) {
        BKE_editmesh_cache_ensure_poly_normals(rdata->edit_bmesh, rdata->edit_data);
        BKE_editmesh_cache_ensure_poly_centers(rdata->edit_bmesh, rdata->edit_data);
      }
    }
  }

  if (rdata->mapped.use == false) {
    for (int i = 0; i < poly_len; i++) {
      if (add_edit_facedot(rdata,
                           vbo_facedots_pos_nor_data,
                           attr_id.fdot_pos,
                           attr_id.fdot_nor_flag,
                           i,
                           facedot_len_used)) {
        facedot_len_used += 1;
      }
    }
  }
  else {
#if 0 /* TODO(fclem): Mapped facedots are not following the original face. */
    Mesh *me_cage = rdata->mapped.me_cage;
    const MVert *mvert = me_cage->mvert;
    const MEdge *medge = me_cage->medge;
    const int *e_origindex = rdata->mapped.e_origindex;
    const int *v_origindex = rdata->mapped.v_origindex;
#endif
    for (int i = 0; i < poly_len; i++) {
      if (add_edit_facedot_mapped(rdata,
                                  vbo_facedots_pos_nor_data,
                                  attr_id.fdot_pos,
                                  attr_id.fdot_nor_flag,
                                  i,
                                  facedot_len_used)) {
        facedot_len_used += 1;
      }
    }
  }

  /* Resize & Finish */
  if (facedot_len_used != verts_facedot_len) {
    if (vbo_facedots_pos_nor_data != NULL) {
      GPU_vertbuf_data_resize(vbo_facedots_pos_nor_data, facedot_len_used);
    }
  }
}

static void mesh_create_edit_mesh_analysis(MeshRenderData *rdata, GPUVertBuf *vbo_mesh_analysis)
{
  const MeshStatVis *mesh_stat_vis = &rdata->toolsettings->statvis;

  int mesh_analysis_len_used = 0;

  const uint loops_len = mesh_render_data_loops_len_get(rdata);
  BMesh *bm = rdata->edit_bmesh->bm;
  BMIter iter_efa, iter_loop;
  BMFace *efa;
  BMLoop *loop;

  static struct {
    uint weight;
  } attr_id;
  static GPUVertFormat mesh_analysis_format = {0};
  if (mesh_analysis_format.attr_len == 0) {
    attr_id.weight = GPU_vertformat_attr_add(
        &mesh_analysis_format, "weight_color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  /* TODO(jbakker): Maybe move data generation to mesh_render_data_create() */
  BKE_editmesh_statvis_calc(rdata->edit_bmesh, rdata->edit_data, mesh_stat_vis);

  if (DRW_TEST_ASSIGN_VBO(vbo_mesh_analysis)) {
    GPU_vertbuf_init_with_format(vbo_mesh_analysis, &mesh_analysis_format);
    GPU_vertbuf_data_alloc(vbo_mesh_analysis, loops_len);
  }

  const bool is_vertex_data = mesh_stat_vis->type == SCE_STATVIS_SHARP;
  if (is_vertex_data) {
    BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        uint vertex_index = BM_elem_index_get(loop->v);
        GPU_vertbuf_attr_set(vbo_mesh_analysis,
                             attr_id.weight,
                             mesh_analysis_len_used,
                             &rdata->edit_bmesh->derivedVertColor[vertex_index]);
        mesh_analysis_len_used += 1;
      }
    }
  }
  else {
    uint face_index;
    BM_ITER_MESH_INDEX (efa, &iter_efa, bm, BM_FACES_OF_MESH, face_index) {
      BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
        GPU_vertbuf_attr_set(vbo_mesh_analysis,
                             attr_id.weight,
                             mesh_analysis_len_used,
                             &rdata->edit_bmesh->derivedFaceColor[face_index]);
        mesh_analysis_len_used += 1;
      }
    }
  }

  // Free temp data in edit bmesh
  BKE_editmesh_color_free(rdata->edit_bmesh);

  /* Resize & Finish */
  if (mesh_analysis_len_used != loops_len) {
    if (vbo_mesh_analysis != NULL) {
      GPU_vertbuf_data_resize(vbo_mesh_analysis, mesh_analysis_len_used);
    }
  }
}
/* Indices */

#define NO_EDGE INT_MAX
static void mesh_create_edges_adjacency_lines(MeshRenderData *rdata,
                                              GPUIndexBuf *ibo,
                                              bool *r_is_manifold,
                                              const bool use_hide)
{
  const MLoopTri *mlooptri;
  const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);

  *r_is_manifold = true;

  /* Allocate max but only used indices are sent to GPU. */
  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, tri_len * 3, vert_len);

  if (rdata->mapped.use) {
    Mesh *me_cage = rdata->mapped.me_cage;
    mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
  }
  else {
    mlooptri = rdata->mlooptri;
  }

  EdgeHash *eh = BLI_edgehash_new_ex(__func__, tri_len * 3);
  /* Create edges for each pair of triangles sharing an edge. */
  for (int i = 0; i < tri_len; i++) {
    for (int e = 0; e < 3; e++) {
      uint v0, v1, v2;
      if (rdata->mapped.use) {
        const MLoop *mloop = rdata->mloop;
        const MLoopTri *mlt = mlooptri + i;
        const int p_orig = rdata->mapped.p_origindex[mlt->poly];
        if (p_orig != ORIGINDEX_NONE) {
          BMesh *bm = rdata->edit_bmesh->bm;
          BMFace *efa = BM_face_at_index(bm, p_orig);
          /* Assume 'use_hide' */
          if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
            break;
          }
        }
        v0 = mloop[mlt->tri[e]].v;
        v1 = mloop[mlt->tri[(e + 1) % 3]].v;
        v2 = mloop[mlt->tri[(e + 2) % 3]].v;
      }
      else if (rdata->edit_bmesh) {
        const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
        if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
          break;
        }
        v0 = BM_elem_index_get(bm_looptri[e]->v);
        v1 = BM_elem_index_get(bm_looptri[(e + 1) % 3]->v);
        v2 = BM_elem_index_get(bm_looptri[(e + 2) % 3]->v);
      }
      else {
        const MLoop *mloop = rdata->mloop;
        const MLoopTri *mlt = mlooptri + i;
        const MPoly *mp = &rdata->mpoly[mlt->poly];
        if (use_hide && (mp->flag & ME_HIDE)) {
          break;
        }
        v0 = mloop[mlt->tri[e]].v;
        v1 = mloop[mlt->tri[(e + 1) % 3]].v;
        v2 = mloop[mlt->tri[(e + 2) % 3]].v;
      }
      bool inv_indices = (v1 > v2);
      void **pval;
      bool value_is_init = BLI_edgehash_ensure_p(eh, v1, v2, &pval);
      int v_data = POINTER_AS_INT(*pval);
      if (!value_is_init || v_data == NO_EDGE) {
        /* Save the winding order inside the sign bit. Because the
         * edgehash sort the keys and we need to compare winding later. */
        int value = (int)v0 + 1; /* Int 0 bm_looptricannot be signed */
        *pval = POINTER_FROM_INT((inv_indices) ? -value : value);
      }
      else {
        /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
        *pval = POINTER_FROM_INT(NO_EDGE);
        bool inv_opposite = (v_data < 0);
        uint v_opposite = (uint)abs(v_data) - 1;

        if (inv_opposite == inv_indices) {
          /* Don't share edge if triangles have non matching winding. */
          GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v0);
          GPU_indexbuf_add_line_adj_verts(&elb, v_opposite, v1, v2, v_opposite);
          *r_is_manifold = false;
        }
        else {
          GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v_opposite);
        }
      }
    }
  }
  /* Create edges for remaning non manifold edges. */
  EdgeHashIterator *ehi;
  for (ehi = BLI_edgehashIterator_new(eh); BLI_edgehashIterator_isDone(ehi) == false;
       BLI_edgehashIterator_step(ehi)) {
    uint v1, v2;
    int v_data = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
    if (v_data == NO_EDGE) {
      continue;
    }
    BLI_edgehashIterator_getKey(ehi, &v1, &v2);
    uint v0 = (uint)abs(v_data) - 1;
    if (v_data < 0) { /* inv_opposite  */
      SWAP(uint, v1, v2);
    }
    GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v0);
    *r_is_manifold = false;
  }
  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(eh, NULL);

  GPU_indexbuf_build_in_place(&elb, ibo);
}
#undef NO_EDGE

static void mesh_create_edges_lines(MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
  const int verts_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int edges_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edges_len, verts_len);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter;
      BMEdge *eed;

      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        /* use_hide always for edit-mode */
        if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
          continue;
        }
        GPU_indexbuf_add_line_verts(&elb, BM_elem_index_get(eed->v1), BM_elem_index_get(eed->v2));
      }
    }
    else {
      const MEdge *ed = rdata->medge;
      for (int i = 0; i < edges_len; i++, ed++) {
        if ((ed->flag & ME_EDGERENDER) == 0) {
          continue;
        }
        if (!(use_hide && (ed->flag & ME_HIDE))) {
          GPU_indexbuf_add_line_verts(&elb, ed->v1, ed->v2);
        }
      }
    }
  }
  else {
    BMesh *bm = rdata->edit_bmesh->bm;
    const MEdge *edge = rdata->medge;
    for (int i = 0; i < edges_len; i++, edge++) {
      const int p_orig = rdata->mapped.e_origindex[i];
      if (p_orig != ORIGINDEX_NONE) {
        BMEdge *eed = BM_edge_at_index(bm, p_orig);
        if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
          GPU_indexbuf_add_line_verts(&elb, edge->v1, edge->v2);
        }
      }
    }
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_surf_tris(MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
  const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int tri_len = mesh_render_data_looptri_len_get(rdata);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len * 3);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      for (int i = 0; i < tri_len; i++) {
        const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
        const BMFace *bm_face = bm_looptri[0]->f;
        /* use_hide always for edit-mode */
        if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
          continue;
        }
        GPU_indexbuf_add_tri_verts(&elb,
                                   BM_elem_index_get(bm_looptri[0]->v),
                                   BM_elem_index_get(bm_looptri[1]->v),
                                   BM_elem_index_get(bm_looptri[2]->v));
      }
    }
    else {
      const MLoop *loops = rdata->mloop;
      for (int i = 0; i < tri_len; i++) {
        const MLoopTri *mlt = &rdata->mlooptri[i];
        const MPoly *mp = &rdata->mpoly[mlt->poly];
        if (use_hide && (mp->flag & ME_HIDE)) {
          continue;
        }
        GPU_indexbuf_add_tri_verts(
            &elb, loops[mlt->tri[0]].v, loops[mlt->tri[1]].v, loops[mlt->tri[2]].v);
      }
    }
  }
  else {
    /* Note: mapped doesn't support lnors yet. */
    BMesh *bm = rdata->edit_bmesh->bm;
    Mesh *me_cage = rdata->mapped.me_cage;

    const MLoop *loops = rdata->mloop;
    const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
    for (int i = 0; i < tri_len; i++) {
      const MLoopTri *mlt = &mlooptri[i];
      const int p_orig = rdata->mapped.p_origindex[mlt->poly];
      if (p_orig != ORIGINDEX_NONE) {
        /* Assume 'use_hide' */
        BMFace *efa = BM_face_at_index(bm, p_orig);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          GPU_indexbuf_add_tri_verts(
              &elb, loops[mlt->tri[0]].v, loops[mlt->tri[1]].v, loops[mlt->tri[2]].v);
        }
      }
    }
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loops_lines(MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
  const int edge_len = mesh_render_data_edges_len_get(rdata);
  const int loop_len = mesh_render_data_loops_len_get(rdata);
  const int poly_len = mesh_render_data_polys_len_get(rdata);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, loop_len);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter;
      BMEdge *bm_edge;

      BM_ITER_MESH (bm_edge, &iter, bm, BM_EDGES_OF_MESH) {
        /* use_hide always for edit-mode */
        if (!BM_elem_flag_test(bm_edge, BM_ELEM_HIDDEN) && bm_edge->l != NULL) {
          BMLoop *bm_loop1 = bm_vert_find_first_loop_visible_inline(bm_edge->v1);
          BMLoop *bm_loop2 = bm_vert_find_first_loop_visible_inline(bm_edge->v2);
          int v1 = BM_elem_index_get(bm_loop1);
          int v2 = BM_elem_index_get(bm_loop2);
          if (v1 > v2) {
            SWAP(int, v1, v2);
          }
          GPU_indexbuf_add_line_verts(&elb, v1, v2);
        }
      }
    }
    else {
      MLoop *mloop = (MLoop *)rdata->mloop;
      MEdge *medge = (MEdge *)rdata->medge;

      /* Reset flag */
      for (int edge = 0; edge < edge_len; ++edge) {
        /* NOTE: not thread safe. */
        medge[edge].flag &= ~ME_EDGE_TMP_TAG;
      }

      for (int poly = 0; poly < poly_len; poly++) {
        const MPoly *mp = &rdata->mpoly[poly];
        if (!(use_hide && (mp->flag & ME_HIDE))) {
          for (int j = 0; j < mp->totloop; j++) {
            MEdge *ed = (MEdge *)rdata->medge + mloop[mp->loopstart + j].e;
            if ((ed->flag & ME_EDGE_TMP_TAG) == 0) {
              ed->flag |= ME_EDGE_TMP_TAG;
              int v1 = mp->loopstart + j;
              int v2 = mp->loopstart + (j + 1) % mp->totloop;
              GPU_indexbuf_add_line_verts(&elb, v1, v2);
            }
          }
        }
      }
    }
  }
  else {
    /* Implement ... eventually if needed. */
    BLI_assert(0);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loops_line_strips(MeshRenderData *rdata,
                                          GPUIndexBuf *ibo,
                                          const bool use_hide)
{
  const int loop_len = mesh_render_data_loops_len_get(rdata);
  const int poly_len = mesh_render_data_polys_len_get(rdata);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, loop_len + poly_len * 2, loop_len, true);

  uint v_index = 0;
  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter iter;
      BMFace *bm_face;

      BM_ITER_MESH (bm_face, &iter, bm, BM_FACES_OF_MESH) {
        /* use_hide always for edit-mode */
        if (!BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
          for (int i = 0; i < bm_face->len; i++) {
            GPU_indexbuf_add_generic_vert(&elb, v_index + i);
          }
          /* Finish loop and restart primitive. */
          GPU_indexbuf_add_generic_vert(&elb, v_index);
          GPU_indexbuf_add_primitive_restart(&elb);
        }
        v_index += bm_face->len;
      }
    }
    else {
      for (int poly = 0; poly < poly_len; poly++) {
        const MPoly *mp = &rdata->mpoly[poly];
        if (!(use_hide && (mp->flag & ME_HIDE))) {
          const int loopend = mp->loopstart + mp->totloop;
          for (int j = mp->loopstart; j < loopend; j++) {
            GPU_indexbuf_add_generic_vert(&elb, j);
          }
          /* Finish loop and restart primitive. */
          GPU_indexbuf_add_generic_vert(&elb, mp->loopstart);
          GPU_indexbuf_add_primitive_restart(&elb);
        }
        v_index += mp->totloop;
      }
    }
  }
  else {
    /* Implement ... eventually if needed. */
    BLI_assert(0);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loose_edges_lines(MeshRenderData *rdata,
                                          GPUIndexBuf *ibo,
                                          const bool use_hide)
{
  const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int edge_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);

  /* Alloc max (edge_len) and upload only needed range. */
  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, vert_len);

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      /* No need to support since edit mesh already draw them.
       * But some engines may want them ... */
      BMesh *bm = rdata->edit_bmesh->bm;
      BMIter eiter;
      BMEdge *eed;
      BM_ITER_MESH (eed, &eiter, bm, BM_EDGES_OF_MESH) {
        if (bm_edge_is_loose_and_visible(eed)) {
          GPU_indexbuf_add_line_verts(
              &elb, BM_elem_index_get(eed->v1), BM_elem_index_get(eed->v2));
        }
      }
    }
    else {
      for (int i = 0; i < edge_len; i++) {
        const MEdge *medge = &rdata->medge[i];
        if ((medge->flag & ME_LOOSEEDGE) && !(use_hide && (medge->flag & ME_HIDE))) {
          GPU_indexbuf_add_line_verts(&elb, medge->v1, medge->v2);
        }
      }
    }
  }
  else {
    /* Hidden checks are already done when creating the loose edge list. */
    Mesh *me_cage = rdata->mapped.me_cage;
    for (int i_iter = 0; i_iter < rdata->mapped.loose_edge_len; i_iter++) {
      const int i = rdata->mapped.loose_edges[i_iter];
      const MEdge *medge = &me_cage->medge[i];
      GPU_indexbuf_add_line_verts(&elb, medge->v1, medge->v2);
    }
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loops_tris(MeshRenderData *rdata,
                                   GPUIndexBuf **ibo,
                                   int ibo_len,
                                   const bool use_hide)
{
  const int loop_len = mesh_render_data_loops_len_get(rdata);
  const int tri_len = mesh_render_data_looptri_len_get(rdata);

  GPUIndexBufBuilder *elb = BLI_array_alloca(elb, ibo_len);

  for (int i = 0; i < ibo_len; ++i) {
    /* TODO alloc minmum necessary. */
    GPU_indexbuf_init(&elb[i], GPU_PRIM_TRIS, tri_len, loop_len * 3);
  }

  if (rdata->mapped.use == false) {
    if (rdata->edit_bmesh) {
      for (int i = 0; i < tri_len; i++) {
        const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
        const BMFace *bm_face = bm_looptri[0]->f;
        /* use_hide always for edit-mode */
        if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
          continue;
        }
        int mat = min_ii(ibo_len - 1, bm_face->mat_nr);
        GPU_indexbuf_add_tri_verts(&elb[mat],
                                   BM_elem_index_get(bm_looptri[0]),
                                   BM_elem_index_get(bm_looptri[1]),
                                   BM_elem_index_get(bm_looptri[2]));
      }
    }
    else {
      for (int i = 0; i < tri_len; i++) {
        const MLoopTri *mlt = &rdata->mlooptri[i];
        const MPoly *mp = &rdata->mpoly[mlt->poly];
        if (use_hide && (mp->flag & ME_HIDE)) {
          continue;
        }
        int mat = min_ii(ibo_len - 1, mp->mat_nr);
        GPU_indexbuf_add_tri_verts(&elb[mat], mlt->tri[0], mlt->tri[1], mlt->tri[2]);
      }
    }
  }
  else {
    /* Note: mapped doesn't support lnors yet. */
    BMesh *bm = rdata->edit_bmesh->bm;
    Mesh *me_cage = rdata->mapped.me_cage;

    const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
    for (int i = 0; i < tri_len; i++) {
      const MLoopTri *mlt = &mlooptri[i];
      const int p_orig = rdata->mapped.p_origindex[mlt->poly];
      if (p_orig != ORIGINDEX_NONE) {
        /* Assume 'use_hide' */
        BMFace *efa = BM_face_at_index(bm, p_orig);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          int mat = min_ii(ibo_len - 1, efa->mat_nr);
          GPU_indexbuf_add_tri_verts(&elb[mat], mlt->tri[0], mlt->tri[1], mlt->tri[2]);
        }
      }
    }
  }

  for (int i = 0; i < ibo_len; ++i) {
    GPU_indexbuf_build_in_place(&elb[i], ibo[i]);
  }
}

/* Warning! this function is not thread safe!
 * It writes to MEdge->flag with ME_EDGE_TMP_TAG. */
static void mesh_create_edit_loops_points_lines(MeshRenderData *rdata,
                                                GPUIndexBuf *ibo_verts,
                                                GPUIndexBuf *ibo_edges)
{
  BMIter iter;
  int i;

  const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
  const int edge_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);
  const int loop_len = mesh_render_data_loops_len_get_maybe_mapped(rdata);
  const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
  const int lvert_len = mesh_render_data_loose_verts_len_get_maybe_mapped(rdata);
  const int ledge_len = mesh_render_data_loose_edges_len_get_maybe_mapped(rdata);
  const int tot_loop_len = loop_len + ledge_len * 2 + lvert_len;

  GPUIndexBufBuilder elb_vert, elb_edge;
  if (DRW_TEST_ASSIGN_IBO(ibo_edges)) {
    GPU_indexbuf_init(&elb_edge, GPU_PRIM_LINES, edge_len, tot_loop_len);
  }
  if (DRW_TEST_ASSIGN_IBO(ibo_verts)) {
    GPU_indexbuf_init(&elb_vert, GPU_PRIM_POINTS, tot_loop_len, tot_loop_len);
  }

  int loop_idx = 0;
  if (rdata->edit_bmesh && (rdata->mapped.use == false)) {
    BMesh *bm = rdata->edit_bmesh->bm;
    /* Edges not loose. */
    if (ibo_edges) {
      BMEdge *eed;
      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
          BMLoop *l = bm_edge_find_first_loop_visible_inline(eed);
          if (l != NULL) {
            int v1 = BM_elem_index_get(eed->l);
            int v2 = BM_elem_index_get(eed->l->next);
            GPU_indexbuf_add_line_verts(&elb_edge, v1, v2);
          }
        }
      }
    }
    /* Face Loops */
    if (ibo_verts) {
      BMVert *eve;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          BMLoop *l = bm_vert_find_first_loop_visible_inline(eve);
          if (l != NULL) {
            int v = BM_elem_index_get(l);
            GPU_indexbuf_add_generic_vert(&elb_vert, v);
          }
        }
      }
    }
    loop_idx = loop_len;
    /* Loose edges */
    for (i = 0; i < ledge_len; ++i) {
      if (ibo_verts) {
        GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + 0);
        GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + 1);
      }
      if (ibo_edges) {
        GPU_indexbuf_add_line_verts(&elb_edge, loop_idx + 0, loop_idx + 1);
      }
      loop_idx += 2;
    }
    /* Loose verts */
    if (ibo_verts) {
      for (i = 0; i < lvert_len; ++i) {
        GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx);
        loop_idx += 1;
      }
    }
  }
  else if (rdata->mapped.use) {
    const MPoly *mpoly = rdata->mapped.me_cage->mpoly;
    MVert *mvert = rdata->mapped.me_cage->mvert;
    MEdge *medge = rdata->mapped.me_cage->medge;
    BMesh *bm = rdata->edit_bmesh->bm;

    const int *v_origindex = rdata->mapped.v_origindex;
    const int *e_origindex = rdata->mapped.e_origindex;
    const int *p_origindex = rdata->mapped.p_origindex;

    /* Reset flag */
    for (int edge = 0; edge < edge_len; ++edge) {
      /* NOTE: not thread safe. */
      medge[edge].flag &= ~ME_EDGE_TMP_TAG;
    }
    for (int vert = 0; vert < vert_len; ++vert) {
      /* NOTE: not thread safe. */
      mvert[vert].flag &= ~ME_VERT_TMP_TAG;
    }

    /* Face Loops */
    for (int poly = 0; poly < poly_len; poly++, mpoly++) {
      int fidx = p_origindex[poly];
      if (fidx != ORIGINDEX_NONE) {
        BMFace *efa = BM_face_at_index(bm, fidx);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          const MLoop *mloop = &rdata->mapped.me_cage->mloop[mpoly->loopstart];
          for (i = 0; i < mpoly->totloop; ++i, ++mloop) {
            if (ibo_verts && (v_origindex[mloop->v] != ORIGINDEX_NONE) &&
                (mvert[mloop->v].flag & ME_VERT_TMP_TAG) == 0) {
              mvert[mloop->v].flag |= ME_VERT_TMP_TAG;
              GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + i);
            }
            if (ibo_edges && (e_origindex[mloop->e] != ORIGINDEX_NONE) &&
                ((medge[mloop->e].flag & ME_EDGE_TMP_TAG) == 0)) {
              medge[mloop->e].flag |= ME_EDGE_TMP_TAG;
              int v1 = loop_idx + i;
              int v2 = loop_idx + ((i + 1) % mpoly->totloop);
              GPU_indexbuf_add_line_verts(&elb_edge, v1, v2);
            }
          }
        }
      }
      loop_idx += mpoly->totloop;
    }
    /* Loose edges */
    for (i = 0; i < ledge_len; ++i) {
      int eidx = e_origindex[rdata->mapped.loose_edges[i]];
      if (eidx != ORIGINDEX_NONE) {
        if (ibo_verts) {
          const MEdge *ed = &medge[rdata->mapped.loose_edges[i]];
          if (v_origindex[ed->v1] != ORIGINDEX_NONE) {
            GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + 0);
          }
          if (v_origindex[ed->v2] != ORIGINDEX_NONE) {
            GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + 1);
          }
        }
        if (ibo_edges) {
          GPU_indexbuf_add_line_verts(&elb_edge, loop_idx + 0, loop_idx + 1);
        }
      }
      loop_idx += 2;
    }
    /* Loose verts */
    if (ibo_verts) {
      for (i = 0; i < lvert_len; ++i) {
        int vidx = v_origindex[rdata->mapped.loose_verts[i]];
        if (vidx != ORIGINDEX_NONE) {
          GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx);
        }
        loop_idx += 1;
      }
    }
  }
  else {
    const MPoly *mpoly = rdata->mpoly;

    /* Face Loops */
    for (int poly = 0; poly < poly_len; poly++, mpoly++) {
      if ((mpoly->flag & ME_HIDE) == 0) {
        for (i = 0; i < mpoly->totloop; ++i) {
          if (ibo_verts) {
            GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + i);
          }
          if (ibo_edges) {
            int v1 = loop_idx + i;
            int v2 = loop_idx + ((i + 1) % mpoly->totloop);
            GPU_indexbuf_add_line_verts(&elb_edge, v1, v2);
          }
        }
      }
      loop_idx += mpoly->totloop;
    }
    /* TODO(fclem): Until we find a way to detect
     * loose verts easily outside of edit mode, this
     * will remain disabled. */
#if 0
    /* Loose edges */
    for (int e = 0; e < edge_len; e++, medge++) {
      if (medge->flag & ME_LOOSEEDGE) {
        int eidx = e_origindex[e];
        if (eidx != ORIGINDEX_NONE) {
          if ((medge->flag & ME_HIDE) == 0) {
            for (int j = 0; j < 2; ++j) {
              if (ibo_verts) {
                GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx + j);
              }
              if (ibo_edges) {
                GPU_indexbuf_add_generic_vert(&elb_edge, loop_idx + j);
              }
            }
          }
        }
        loop_idx += 2;
      }
    }
    /* Loose verts */
    for (int v = 0; v < vert_len; v++, mvert++) {
      int vidx = v_origindex[v];
      if (vidx != ORIGINDEX_NONE) {
        if ((mvert->flag & ME_HIDE) == 0) {
          if (ibo_verts) {
            GPU_indexbuf_add_generic_vert(&elb_vert, loop_idx);
          }
          if (ibo_edges) {
            GPU_indexbuf_add_generic_vert(&elb_edge, loop_idx);
          }
        }
        loop_idx += 1;
      }
    }
#endif
  }

  if (ibo_verts) {
    GPU_indexbuf_build_in_place(&elb_vert, ibo_verts);
  }
  if (ibo_edges) {
    GPU_indexbuf_build_in_place(&elb_edge, ibo_edges);
  }
}

static void mesh_create_edit_loops_tris(MeshRenderData *rdata, GPUIndexBuf *ibo)
{
  const int loop_len = mesh_render_data_loops_len_get_maybe_mapped(rdata);
  const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);

  GPUIndexBufBuilder elb;
  /* TODO alloc minmum necessary. */
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, loop_len * 3);

  if (rdata->edit_bmesh && (rdata->mapped.use == false)) {
    for (int i = 0; i < tri_len; i++) {
      const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
      const BMFace *bm_face = bm_looptri[0]->f;
      /* use_hide always for edit-mode */
      if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
        continue;
      }
      GPU_indexbuf_add_tri_verts(&elb,
                                 BM_elem_index_get(bm_looptri[0]),
                                 BM_elem_index_get(bm_looptri[1]),
                                 BM_elem_index_get(bm_looptri[2]));
    }
  }
  else if (rdata->mapped.use == true) {
    BMesh *bm = rdata->edit_bmesh->bm;
    Mesh *me_cage = rdata->mapped.me_cage;

    const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
    for (int i = 0; i < tri_len; i++) {
      const MLoopTri *mlt = &mlooptri[i];
      const int p_orig = rdata->mapped.p_origindex[mlt->poly];
      if (p_orig != ORIGINDEX_NONE) {
        /* Assume 'use_hide' */
        BMFace *efa = BM_face_at_index(bm, p_orig);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          GPU_indexbuf_add_tri_verts(&elb, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
        }
      }
    }
  }
  else {
    const MLoopTri *mlt = rdata->mlooptri;
    for (int i = 0; i < tri_len; i++, mlt++) {
      const MPoly *mpoly = &rdata->mpoly[mlt->poly];
      /* Assume 'use_hide' */
      if ((mpoly->flag & ME_HIDE) == 0) {
        GPU_indexbuf_add_tri_verts(&elb, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
      }
    }
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void texpaint_request_active_uv(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_uv_layer(me, &cd_needed);

  BLI_assert(cd_needed.uv != 0 &&
             "No uv layer available in texpaint, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(me, &cd_needed);
  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

static void texpaint_request_active_vcol(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_vcol_layer(me, &cd_needed);

  BLI_assert(cd_needed.vcol != 0 &&
             "No vcol layer available in vertpaint, but batches requested anyway!");

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

GPUBatch *DRW_mesh_batch_cache_get_all_verts(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_ALL_VERTS);
  return DRW_batch_request(&cache->batch.all_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_all_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_ALL_EDGES);
  return DRW_batch_request(&cache->batch.all_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_LOOSE_EDGES);
  return DRW_batch_request(&cache->batch.loose_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_weights(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE_WEIGHTS);
  return DRW_batch_request(&cache->batch.surface_weights);
}

GPUBatch *DRW_mesh_batch_cache_get_edge_detection(Mesh *me, bool *r_is_manifold)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDGE_DETECTION);
  /* Even if is_manifold is not correct (not updated),
   * the default (not manifold) is just the worst case. */
  if (r_is_manifold) {
    *r_is_manifold = cache->is_manifold;
  }
  return DRW_batch_request(&cache->batch.edge_detection);
}

GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_EDGES);
  return DRW_batch_request(&cache->batch.wire_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_MESH_ANALYSIS);
  return DRW_batch_request(&cache->batch.edit_mesh_analysis);
}

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len,
                                                   char **auto_layer_names,
                                                   int **auto_layer_is_srgb,
                                                   int *auto_layer_count)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(me, gpumat_array, gpumat_array_len);

  BLI_assert(gpumat_array_len == cache->mat_len);

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);

  if (!mesh_cd_layers_type_overlap(cache->cd_used, cd_needed)) {
    mesh_cd_extract_auto_layers_names_and_srgb(me,
                                               cache->cd_needed,
                                               &cache->auto_layer_names,
                                               &cache->auto_layer_is_srgb,
                                               &cache->auto_layer_len);
  }

  mesh_batch_cache_add_request(cache, MBC_SURF_PER_MAT);

  if (auto_layer_names) {
    *auto_layer_names = cache->auto_layer_names;
    *auto_layer_is_srgb = cache->auto_layer_is_srgb;
    *auto_layer_count = cache->auto_layer_len;
  }
  for (int i = 0; i < cache->mat_len; ++i) {
    DRW_batch_request(&cache->surf_per_mat[i]);
  }
  return cache->surf_per_mat;
}

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SURF_PER_MAT);
  texpaint_request_active_uv(cache, me);
  for (int i = 0; i < cache->mat_len; ++i) {
    DRW_batch_request(&cache->surf_per_mat[i]);
  }
  return cache->surf_per_mat;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_vcol(cache, me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_TRIANGLES);
  return DRW_batch_request(&cache->batch.edit_triangles);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_EDGES);
  return DRW_batch_request(&cache->batch.edit_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_VERTICES);
  return DRW_batch_request(&cache->batch.edit_vertices);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_lnors(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_LNOR);
  return DRW_batch_request(&cache->batch.edit_lnor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_FACEDOTS);
  return DRW_batch_request(&cache->batch.edit_facedots);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode selection API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_FACES);
  return DRW_batch_request(&cache->batch.edit_selection_faces);
}

GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_FACEDOTS);
  return DRW_batch_request(&cache->batch.edit_selection_facedots);
}

GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_EDGES);
  return DRW_batch_request(&cache->batch.edit_selection_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_VERTS);
  return DRW_batch_request(&cache->batch.edit_selection_verts);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name UV Image editor API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_strech_area(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRECH_AREA);
  return DRW_batch_request(&cache->batch.edituv_faces_strech_area);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_strech_angle(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRECH_ANGLE);
  return DRW_batch_request(&cache->batch.edituv_faces_strech_angle);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES);
  return DRW_batch_request(&cache->batch.edituv_faces);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_EDGES);
  return DRW_batch_request(&cache->batch.edituv_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_VERTS);
  return DRW_batch_request(&cache->batch.edituv_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACEDOTS);
  return DRW_batch_request(&cache->batch.edituv_facedots);
}

GPUBatch *DRW_mesh_batch_cache_get_uv_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS_UVS);
  return DRW_batch_request(&cache->batch.wire_loops_uvs);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS);
  return DRW_batch_request(&cache->batch.wire_loops);
}

/* Compute 3D & 2D areas and their sum. */
BLI_INLINE void edit_uv_preprocess_stretch_area(BMFace *efa,
                                                const int cd_loop_uv_offset,
                                                uint fidx,
                                                float *totarea,
                                                float *totuvarea,
                                                float (*faces_areas)[2])
{
  faces_areas[fidx][0] = BM_face_calc_area(efa);
  faces_areas[fidx][1] = BM_face_calc_area_uv(efa, cd_loop_uv_offset);

  *totarea += faces_areas[fidx][0];
  *totuvarea += faces_areas[fidx][1];
}

BLI_INLINE float edit_uv_get_stretch_area(float area, float uvarea)
{
  if (area < FLT_EPSILON || uvarea < FLT_EPSILON) {
    return 1.0f;
  }
  else if (area > uvarea) {
    return 1.0f - (uvarea / area);
  }
  else {
    return 1.0f - (area / uvarea);
  }
}

/* Compute face's normalized contour vectors. */
BLI_INLINE void edit_uv_preprocess_stretch_angle(float (*auv)[2],
                                                 float (*av)[3],
                                                 const int cd_loop_uv_offset,
                                                 BMFace *efa)
{
  BMLoop *l;
  BMIter liter;
  int i;
  BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);

    sub_v2_v2v2(auv[i], luv_prev->uv, luv->uv);
    normalize_v2(auv[i]);

    sub_v3_v3v3(av[i], l->prev->v->co, l->v->co);
    normalize_v3(av[i]);
  }
}

#if 0 /* here for reference, this is done in shader now. */
BLI_INLINE float edit_uv_get_loop_stretch_angle(const float auv0[2],
                                                const float auv1[2],
                                                const float av0[3],
                                                const float av1[3])
{
  float uvang = angle_normalized_v2v2(auv0, auv1);
  float ang = angle_normalized_v3v3(av0, av1);
  float stretch = fabsf(uvang - ang) / (float)M_PI;
  return 1.0f - pow2f(1.0f - stretch);
}
#endif

static struct EditUVFormatIndex {
  uint area, angle, uv_adj, flag, fdots_uvs, fdots_flag;
} uv_attr_id = {0};

static void uvedit_fill_buffer_data(MeshRenderData *rdata,
                                    GPUVertBuf *vbo_area,
                                    GPUVertBuf *vbo_angle,
                                    GPUVertBuf *vbo_fdots_pos,
                                    GPUVertBuf *vbo_fdots_data,
                                    GPUIndexBufBuilder *elb_vert,
                                    GPUIndexBufBuilder *elb_edge,
                                    GPUIndexBufBuilder *elb_face)
{
  BMesh *bm = rdata->edit_bmesh->bm;
  BMIter iter, liter;
  BMFace *efa;
  uint vidx, fidx, fdot_idx, i;
  const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
  float(*faces_areas)[2] = NULL;
  float totarea = 0.0f, totuvarea = 0.0f;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  BLI_buffer_declare_static(vec3f, vec3_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_buffer_declare_static(vec2f, vec2_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

  if (vbo_area) {
    faces_areas = MEM_mallocN(sizeof(float) * 2 * bm->totface, "EDITUV faces areas");
  }

  /* Preprocess */
  fidx = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    /* Tag hidden faces */
    BM_elem_flag_set(efa, BM_ELEM_TAG, uvedit_face_visible_nolocal_ex(rdata->toolsettings, efa));

    if (vbo_area && BM_elem_flag_test(efa, BM_ELEM_TAG)) {
      edit_uv_preprocess_stretch_area(
          efa, cd_loop_uv_offset, fidx++, &totarea, &totuvarea, faces_areas);
    }
  }

  vidx = 0;
  fidx = 0;
  fdot_idx = 0;
  if (rdata->mapped.use == false && rdata->edit_bmesh) {
    BMLoop *l;
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      const bool face_visible = BM_elem_flag_test(efa, BM_ELEM_TAG);
      const int efa_len = efa->len;
      float fdot[2] = {0.0f, 0.0f};
      float(*av)[3], (*auv)[2];
      ushort area_stretch;

      /* Face preprocess */
      if (vbo_area) {
        area_stretch = edit_uv_get_stretch_area(faces_areas[fidx][0] / totarea,
                                                faces_areas[fidx][1] / totuvarea) *
                       65534.0f;
      }
      if (vbo_angle) {
        av = (float(*)[3])BLI_buffer_reinit_data(&vec3_buf, vec3f, efa_len);
        auv = (float(*)[2])BLI_buffer_reinit_data(&vec2_buf, vec2f, efa_len);
        edit_uv_preprocess_stretch_angle(auv, av, cd_loop_uv_offset, efa);
      }

      /* Skip hidden faces. */
      if (face_visible) {
        if (elb_face) {
          for (i = 0; i < efa->len; ++i) {
            GPU_indexbuf_add_generic_vert(elb_face, vidx + i);
          }
        }
        if (elb_vert) {
          for (i = 0; i < efa->len; ++i) {
            GPU_indexbuf_add_generic_vert(elb_vert, vidx + i);
          }
        }
        if (elb_edge) {
          for (i = 0; i < efa->len; ++i) {
            GPU_indexbuf_add_line_verts(elb_edge, vidx + i, vidx + (i + 1) % efa->len);
          }
        }
      }

      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
        MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (vbo_area) {
          GPU_vertbuf_attr_set(vbo_area, uv_attr_id.area, vidx, &area_stretch);
        }
        if (vbo_angle) {
          int i_next = (i + 1) % efa_len;
          short suv[4];
          /* Send uvs to the shader and let it compute the aspect corrected angle. */
          normal_float_to_short_v2(&suv[0], auv[i]);
          normal_float_to_short_v2(&suv[2], auv[i_next]);
          GPU_vertbuf_attr_set(vbo_angle, uv_attr_id.uv_adj, vidx, suv);
          /* Compute 3D angle here */
          short angle = 32767.0f * angle_normalized_v3v3(av[i], av[i_next]) / (float)M_PI;
          GPU_vertbuf_attr_set(vbo_angle, uv_attr_id.angle, vidx, &angle);
        }
        if (vbo_fdots_pos) {
          add_v2_v2(fdot, luv->uv);
        }
        vidx++;
      }

      if (elb_face && face_visible) {
        GPU_indexbuf_add_generic_vert(elb_face, vidx - efa->len);
        GPU_indexbuf_add_primitive_restart(elb_face);
      }
      if (vbo_fdots_pos && face_visible) {
        mul_v2_fl(fdot, 1.0f / (float)efa->len);
        GPU_vertbuf_attr_set(vbo_fdots_pos, uv_attr_id.fdots_uvs, fdot_idx, fdot);
      }
      if (vbo_fdots_data && face_visible) {
        uchar face_flag = mesh_render_data_face_flag(rdata, efa, cd_loop_uv_offset);
        GPU_vertbuf_attr_set(vbo_fdots_data, uv_attr_id.fdots_flag, fdot_idx, &face_flag);
      }
      fdot_idx += face_visible ? 1 : 0;
      fidx++;
    }
  }
  else {
    const MPoly *mpoly = rdata->mapped.me_cage->mpoly;
    // const MEdge *medge = rdata->mapped.me_cage->medge;
    // const MVert *mvert = rdata->mapped.me_cage->mvert;
    const MLoop *mloop = rdata->mapped.me_cage->mloop;

    const int *v_origindex = rdata->mapped.v_origindex;
    const int *e_origindex = rdata->mapped.e_origindex;
    const int *p_origindex = rdata->mapped.p_origindex;

    /* Face Loops */
    for (int poly = 0; poly < poly_len; poly++, mpoly++) {
      float fdot[2] = {0.0f, 0.0f};
      const MLoop *l = &mloop[mpoly->loopstart];
      int fidx_ori = p_origindex[poly];
      efa = (fidx_ori != ORIGINDEX_NONE) ? BM_face_at_index(bm, fidx_ori) : NULL;
      const bool face_visible = efa != NULL && BM_elem_flag_test(efa, BM_ELEM_TAG);
      if (efa && vbo_fdots_data) {
        uchar face_flag = mesh_render_data_face_flag(rdata, efa, cd_loop_uv_offset);
        GPU_vertbuf_attr_set(vbo_fdots_data, uv_attr_id.fdots_flag, fdot_idx, &face_flag);
      }
      /* Skip hidden faces. */
      if (face_visible) {
        if (elb_face) {
          for (i = 0; i < mpoly->totloop; ++i) {
            GPU_indexbuf_add_generic_vert(elb_face, vidx + i);
          }
          GPU_indexbuf_add_generic_vert(elb_face, vidx);
          GPU_indexbuf_add_primitive_restart(elb_face);
        }
        if (elb_edge && e_origindex[l[i].e] != ORIGINDEX_NONE) {
          for (i = 0; i < mpoly->totloop; ++i) {
            GPU_indexbuf_add_line_verts(elb_edge, vidx + i, vidx + (i + 1) % mpoly->totloop);
          }
        }
        if (elb_vert && v_origindex[l[i].v] != ORIGINDEX_NONE) {
          for (i = 0; i < mpoly->totloop; ++i) {
            GPU_indexbuf_add_generic_vert(elb_vert, vidx + i);
          }
        }
      }
      for (i = 0; i < mpoly->totloop; i++, l++) {
        /* TODO support stretch. */
        if (vbo_fdots_pos) {
          MLoopUV *luv = &rdata->mloopuv[mpoly->loopstart + i];
          add_v2_v2(fdot, luv->uv);
        }
        vidx++;
      }
      if (vbo_fdots_pos && face_visible) {
        mul_v2_fl(fdot, 1.0f / mpoly->totloop);
        GPU_vertbuf_attr_set(vbo_fdots_pos, uv_attr_id.fdots_uvs, fdot_idx, fdot);
      }
      fidx++;
      fdot_idx += face_visible ? 1 : 0;
    }
  }

  if (faces_areas) {
    MEM_freeN(faces_areas);
  }

  BLI_buffer_free(&vec3_buf);
  BLI_buffer_free(&vec2_buf);

  if (fdot_idx < poly_len) {
    if (vbo_fdots_pos) {
      GPU_vertbuf_data_resize(vbo_fdots_pos, fdot_idx);
    }
    if (vbo_fdots_data) {
      GPU_vertbuf_data_resize(vbo_fdots_data, fdot_idx);
    }
  }
}

static void mesh_create_uvedit_buffers(MeshRenderData *rdata,
                                       GPUVertBuf *vbo_area,
                                       GPUVertBuf *vbo_angle,
                                       GPUVertBuf *vbo_fdots_pos,
                                       GPUVertBuf *vbo_fdots_data,
                                       GPUIndexBuf *ibo_vert,
                                       GPUIndexBuf *ibo_edge,
                                       GPUIndexBuf *ibo_face)
{
  static GPUVertFormat format_area = {0};
  static GPUVertFormat format_angle = {0};
  static GPUVertFormat format_fdots_pos = {0};
  static GPUVertFormat format_fdots_flag = {0};

  if (format_area.attr_len == 0) {
    uv_attr_id.area = GPU_vertformat_attr_add(
        &format_area, "stretch", GPU_COMP_U16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    uv_attr_id.angle = GPU_vertformat_attr_add(
        &format_angle, "angle", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    uv_attr_id.uv_adj = GPU_vertformat_attr_add(
        &format_angle, "uv_adj", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

    uv_attr_id.fdots_flag = GPU_vertformat_attr_add(
        &format_fdots_flag, "flag", GPU_COMP_U8, 1, GPU_FETCH_INT);
    uv_attr_id.fdots_uvs = GPU_vertformat_attr_add(
        &format_fdots_pos, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format_fdots_pos, "pos");
  }

  const int loop_len = mesh_render_data_loops_len_get_maybe_mapped(rdata);
  const int face_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
  const int idx_len = loop_len + face_len * 2;

  if (DRW_TEST_ASSIGN_VBO(vbo_area)) {
    GPU_vertbuf_init_with_format(vbo_area, &format_area);
    GPU_vertbuf_data_alloc(vbo_area, loop_len);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_angle)) {
    GPU_vertbuf_init_with_format(vbo_angle, &format_angle);
    GPU_vertbuf_data_alloc(vbo_angle, loop_len);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_fdots_pos)) {
    GPU_vertbuf_init_with_format(vbo_fdots_pos, &format_fdots_pos);
    GPU_vertbuf_data_alloc(vbo_fdots_pos, face_len);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_fdots_data)) {
    GPU_vertbuf_init_with_format(vbo_fdots_data, &format_fdots_flag);
    GPU_vertbuf_data_alloc(vbo_fdots_data, face_len);
  }

  GPUIndexBufBuilder elb_vert, elb_edge, elb_face;
  if (DRW_TEST_ASSIGN_IBO(ibo_vert)) {
    GPU_indexbuf_init_ex(&elb_vert, GPU_PRIM_POINTS, loop_len, loop_len, false);
  }
  if (DRW_TEST_ASSIGN_IBO(ibo_edge)) {
    GPU_indexbuf_init_ex(&elb_edge, GPU_PRIM_LINES, loop_len * 2, loop_len, false);
  }
  if (DRW_TEST_ASSIGN_IBO(ibo_face)) {
    GPU_indexbuf_init_ex(&elb_face, GPU_PRIM_TRI_FAN, idx_len, loop_len, true);
  }

  uvedit_fill_buffer_data(rdata,
                          vbo_area,
                          vbo_angle,
                          vbo_fdots_pos,
                          vbo_fdots_data,
                          ibo_vert ? &elb_vert : NULL,
                          ibo_edge ? &elb_edge : NULL,
                          ibo_face ? &elb_face : NULL);

  if (ibo_vert) {
    GPU_indexbuf_build_in_place(&elb_vert, ibo_vert);
  }

  if (ibo_edge) {
    GPU_indexbuf_build_in_place(&elb_edge, ibo_edge);
  }

  if (ibo_face) {
    GPU_indexbuf_build_in_place(&elb_face, ibo_face);
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

/* Thread safety need to be assured by caller. Don't call this during drawing.
 * Note: For now this only free the shading batches / vbo if any cd layers is
 * not needed anymore. */
void DRW_mesh_batch_cache_free_old(Mesh *me, int ctime)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (cache == NULL) {
    return;
  }

  if (mesh_cd_layers_type_equal(cache->cd_used_over_time, cache->cd_used)) {
    cache->lastmatch = ctime;
  }

  if (ctime - cache->lastmatch > U.vbotimeout) {
    mesh_batch_cache_discard_shaded_tri(cache);
  }

  mesh_cd_layers_type_clear(&cache->cd_used_over_time);
}

/* Can be called for any surface type. Mesh *me is the final mesh. */
void DRW_mesh_batch_cache_create_requested(
    Object *ob, Mesh *me, const ToolSettings *ts, const bool is_paint_mode, const bool use_hide)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  /* Early out */
  if (cache->batch_requested == 0) {
#ifdef DEBUG
    goto check;
#endif
    return;
  }

  if (cache->batch_requested & MBC_SURFACE_WEIGHTS) {
    /* Check vertex weights. */
    if ((cache->batch.surface_weights != NULL) && (ts != NULL)) {
      struct DRW_MeshWeightState wstate;
      BLI_assert(ob->type == OB_MESH);
      drw_mesh_weight_state_extract(ob, me, ts, is_paint_mode, &wstate);
      mesh_batch_cache_check_vertex_group(cache, &wstate);
      drw_mesh_weight_state_copy(&cache->weight_state, &wstate);
      drw_mesh_weight_state_clear(&wstate);
    }
  }

  if (cache->batch_requested & (MBC_SURFACE | MBC_SURF_PER_MAT | MBC_WIRE_LOOPS_UVS)) {
    /* Optimization : Only create orco layer if mesh is deformed. */
    if (cache->cd_needed.orco != 0) {
      CustomData *cd_vdata = (me->edit_mesh) ? &me->edit_mesh->bm->vdata : &me->vdata;
      if (CustomData_get_layer(cd_vdata, CD_ORCO) != NULL && ob->modifiers.first != NULL) {
        /* Orco layer is needed. */
      }
      else if (cache->cd_needed.tan_orco == 0) {
        /* Skip orco calculation if not needed by tangent generation.
         */
        cache->cd_needed.orco = 0;
      }
    }

    /* Verify that all surface batches have needed attribute layers.
     */
    /* TODO(fclem): We could be a bit smarter here and only do it per
     * material. */
    bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_used, cache->cd_needed);
    if (cd_overlap == false) {
      if ((cache->cd_used.uv & cache->cd_needed.uv) != cache->cd_needed.uv ||
          (cache->cd_used.tan & cache->cd_needed.tan) != cache->cd_needed.tan ||
          cache->cd_used.tan_orco != cache->cd_needed.tan_orco) {
        GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_uv_tan);
      }
      if (cache->cd_used.orco != cache->cd_needed.orco) {
        GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_orco);
      }
      if ((cache->cd_used.vcol & cache->cd_needed.vcol) != cache->cd_needed.vcol) {
        GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_vcol);
      }
      /* We can't discard batches at this point as they have been
       * referenced for drawing. Just clear them in place. */
      for (int i = 0; i < cache->mat_len; ++i) {
        GPU_BATCH_CLEAR_SAFE(cache->surf_per_mat[i]);
      }
      GPU_BATCH_CLEAR_SAFE(cache->batch.surface);
      cache->batch_ready &= ~(MBC_SURFACE | MBC_SURF_PER_MAT);

      mesh_cd_layers_type_merge(&cache->cd_used, cache->cd_needed);
    }
    mesh_cd_layers_type_merge(&cache->cd_used_over_time, cache->cd_needed);
    mesh_cd_layers_type_clear(&cache->cd_needed);
  }

  if (cache->batch_requested & MBC_EDITUV) {
    /* Discard UV batches if sync_selection changes */
    if (ts != NULL) {
      const bool is_uvsyncsel = (ts->uv_flag & UV_SYNC_SELECTION);
      if (cache->is_uvsyncsel != is_uvsyncsel) {
        cache->is_uvsyncsel = is_uvsyncsel;
        GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_uv_data);
        GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_stretch_angle);
        GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_stretch_area);
        GPU_VERTBUF_DISCARD_SAFE(cache->edit.loop_uv);
        GPU_VERTBUF_DISCARD_SAFE(cache->edit.facedots_uv);
        GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_tri_fans);
        GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_line_strips);
        GPU_INDEXBUF_DISCARD_SAFE(cache->ibo.edituv_loops_points);
        /* We only clear the batches as they may already have been
         * referenced. */
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces_strech_area);
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces_strech_angle);
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces);
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_edges);
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_verts);
        GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_facedots);
        cache->batch_ready &= ~MBC_EDITUV;
      }
    }
  }

  /* Second chance to early out */
  if ((cache->batch_requested & ~cache->batch_ready) == 0) {
#ifdef DEBUG
    goto check;
#endif
    return;
  }

  cache->batch_ready |= cache->batch_requested;
  cache->batch_requested = 0;

  /* Init batches and request VBOs & IBOs */
  if (DRW_batch_requested(cache->batch.surface, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface, &cache->ibo.loops_tris);
    DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_pos_nor);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_uv_tan);
    }
    if (cache->cd_used.vcol != 0) {
      DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_vcol);
    }
  }
  if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.all_verts, &cache->ordered.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.all_edges, &cache->ibo.edges_lines);
    DRW_vbo_request(cache->batch.all_edges, &cache->ordered.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.loose_edges, &cache->ibo.loose_edges_lines);
    DRW_vbo_request(cache->batch.loose_edges, &cache->ordered.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &cache->ibo.edges_adj_lines);
    DRW_vbo_request(cache->batch.edge_detection, &cache->ordered.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_weights, &cache->ibo.surf_tris);
    DRW_vbo_request(cache->batch.surface_weights, &cache->ordered.pos_nor);
    DRW_vbo_request(cache->batch.surface_weights, &cache->ordered.weights);
  }
  if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache->batch.wire_loops, &cache->ibo.loops_line_strips);
    DRW_vbo_request(cache->batch.wire_loops, &cache->ordered.loop_pos_nor);
  }
  if (DRW_batch_requested(cache->batch.wire_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_edges, &cache->ibo.loops_lines);
    DRW_vbo_request(cache->batch.wire_edges, &cache->ordered.loop_pos_nor);
    DRW_vbo_request(cache->batch.wire_edges, &cache->ordered.loop_edge_fac);
  }
  if (DRW_batch_requested(cache->batch.wire_loops_uvs, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache->batch.wire_loops_uvs, &cache->ibo.loops_line_strips);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.wire_loops_uvs, &cache->ordered.loop_uv_tan);
    }
  }

  /* Edit Mesh */
  if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_triangles, &cache->ibo.edit_loops_tris);
    DRW_vbo_request(cache->batch.edit_triangles, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_triangles, &cache->edit.loop_data);
  }
  if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vertices, &cache->ibo.edit_loops_points);
    DRW_vbo_request(cache->batch.edit_vertices, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_vertices, &cache->edit.loop_data);
  }
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &cache->ibo.edit_loops_lines);
    DRW_vbo_request(cache->batch.edit_edges, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_edges, &cache->edit.loop_data);
  }
  if (DRW_batch_requested(cache->batch.edit_lnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_lnor, &cache->ibo.edit_loops_tris);
    DRW_vbo_request(cache->batch.edit_lnor, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_lnor, &cache->edit.loop_lnor);
  }
  if (DRW_batch_requested(cache->batch.edit_facedots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_facedots, &cache->edit.facedots_pos_nor_data);
  }

  /* Mesh Analysis */
  if (DRW_batch_requested(cache->batch.edit_mesh_analysis, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_mesh_analysis, &cache->ibo.edit_loops_tris);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &cache->edit.loop_mesh_analysis);
  }

  /* Edit UV */
  if (DRW_batch_requested(cache->batch.edituv_faces, GPU_PRIM_TRI_FAN)) {
    DRW_ibo_request(cache->batch.edituv_faces, &cache->ibo.edituv_loops_tri_fans);
    DRW_vbo_request(cache->batch.edituv_faces, &cache->edit.loop_uv);
    DRW_vbo_request(cache->batch.edituv_faces, &cache->edit.loop_uv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_faces_strech_area, GPU_PRIM_TRI_FAN)) {
    DRW_ibo_request(cache->batch.edituv_faces_strech_area, &cache->ibo.edituv_loops_tri_fans);
    DRW_vbo_request(cache->batch.edituv_faces_strech_area, &cache->edit.loop_uv);
    DRW_vbo_request(cache->batch.edituv_faces_strech_area, &cache->edit.loop_uv_data);
    DRW_vbo_request(cache->batch.edituv_faces_strech_area, &cache->edit.loop_stretch_area);
  }
  if (DRW_batch_requested(cache->batch.edituv_faces_strech_angle, GPU_PRIM_TRI_FAN)) {
    DRW_ibo_request(cache->batch.edituv_faces_strech_angle, &cache->ibo.edituv_loops_tri_fans);
    DRW_vbo_request(cache->batch.edituv_faces_strech_angle, &cache->edit.loop_uv);
    DRW_vbo_request(cache->batch.edituv_faces_strech_angle, &cache->edit.loop_uv_data);
    DRW_vbo_request(cache->batch.edituv_faces_strech_angle, &cache->edit.loop_stretch_angle);
  }
  if (DRW_batch_requested(cache->batch.edituv_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edituv_edges, &cache->ibo.edituv_loops_line_strips);
    DRW_vbo_request(cache->batch.edituv_edges, &cache->edit.loop_uv);
    DRW_vbo_request(cache->batch.edituv_edges, &cache->edit.loop_uv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_verts, &cache->ibo.edituv_loops_points);
    DRW_vbo_request(cache->batch.edituv_verts, &cache->edit.loop_uv);
    DRW_vbo_request(cache->batch.edituv_verts, &cache->edit.loop_uv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_facedots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edituv_facedots, &cache->edit.facedots_uv);
    DRW_vbo_request(cache->batch.edituv_facedots, &cache->edit.facedots_uv_data);
  }

  /* Selection */
  /* TODO reuse ordered.loop_pos_nor if possible. */
  if (DRW_batch_requested(cache->batch.edit_selection_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_verts, &cache->ibo.edit_loops_points);
    DRW_vbo_request(cache->batch.edit_selection_verts, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_verts, &cache->edit.loop_vert_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_selection_edges, &cache->ibo.edit_loops_lines);
    DRW_vbo_request(cache->batch.edit_selection_edges, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_edges, &cache->edit.loop_edge_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_selection_faces, &cache->ibo.edit_loops_tris);
    DRW_vbo_request(cache->batch.edit_selection_faces, &cache->edit.loop_pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_faces, &cache->edit.loop_face_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_facedots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_selection_facedots, &cache->edit.facedots_pos_nor_data);
    DRW_vbo_request(cache->batch.edit_selection_facedots, &cache->edit.facedots_idx);
  }

  /* Per Material */
  for (int i = 0; i < cache->mat_len; ++i) {
    if (DRW_batch_requested(cache->surf_per_mat[i], GPU_PRIM_TRIS)) {
      if (cache->mat_len > 1) {
        DRW_ibo_request(cache->surf_per_mat[i], &cache->surf_per_mat_tris[i]);
      }
      else {
        DRW_ibo_request(cache->surf_per_mat[i], &cache->ibo.loops_tris);
      }
      DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_pos_nor);
      if ((cache->cd_used.uv != 0) || (cache->cd_used.tan != 0) ||
          (cache->cd_used.tan_orco != 0)) {
        DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_uv_tan);
      }
      if (cache->cd_used.vcol != 0) {
        DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_vcol);
      }
      if (cache->cd_used.orco != 0) {
        DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_orco);
      }
    }
  }

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("-- %s %s --\n", __func__, ob->id.name + 2);
#endif

  /* Generate MeshRenderData flags */
  eMRDataType mr_flag = 0, mr_edit_flag = 0;
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.pos_nor, MR_DATATYPE_VERT /* A comment to wrap the line ;) */);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.weights, MR_DATATYPE_VERT | MR_DATATYPE_DVERT);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.loop_pos_nor, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_LOOP_NORMALS);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.loop_uv_tan, MR_DATATYPE_VERT_LOOP_TRI_POLY | MR_DATATYPE_SHADING);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.loop_orco, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_SHADING);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.loop_vcol, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_SHADING);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.loop_edge_fac, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_EDGE);

  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.surf_tris, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_LOOPTRI);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.loops_tris, MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.loops_lines, MR_DATATYPE_LOOP | MR_DATATYPE_EDGE | MR_DATATYPE_POLY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.loops_line_strips, MR_DATATYPE_LOOP | MR_DATATYPE_POLY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.edges_lines, MR_DATATYPE_VERT | MR_DATATYPE_EDGE);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.edges_adj_lines, MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_LOOPTRI);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_flag, cache->ibo.loose_edges_lines, MR_DATATYPE_VERT | MR_DATATYPE_EDGE);
  for (int i = 0; i < cache->mat_len; ++i) {
    int combined_flag = MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI;
    DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->surf_per_mat_tris[i], combined_flag);
  }

  int combined_edit_flag = MR_DATATYPE_VERT_LOOP_POLY | MR_DATATYPE_EDGE |
                           MR_DATATYPE_LOOSE_VERT_EGDE;
  int combined_edit_with_lnor_flag = combined_edit_flag | MR_DATATYPE_LOOP_NORMALS;
  int combined_edituv_flag = combined_edit_flag | MR_DATATYPE_LOOPUV;
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_pos_nor, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_lnor, combined_edit_with_lnor_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_data, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_uv_data, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_uv, combined_edituv_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_stretch_angle, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_stretch_area, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.loop_mesh_analysis, MR_DATATYPE_VERT_LOOP_POLY);

  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.loop_vert_idx, combined_edit_flag);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.loop_edge_idx, combined_edit_flag);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.loop_face_idx, combined_edit_flag);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.facedots_idx, MR_DATATYPE_POLY);

  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.facedots_pos_nor_data, MR_DATATYPE_POLY | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.facedots_uv, combined_edituv_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_edit_flag, cache->edit.facedots_uv_data, combined_edit_flag | MR_DATATYPE_OVERLAY);

  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edituv_loops_points, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edituv_loops_line_strips, combined_edit_flag | MR_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edituv_loops_tri_fans, combined_edit_flag | MR_DATATYPE_OVERLAY);

  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edit_loops_points, combined_edit_flag | MR_DATATYPE_LOOPTRI);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edit_loops_lines, combined_edit_flag | MR_DATATYPE_LOOPTRI);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(
      mr_edit_flag, cache->ibo.edit_loops_tris, combined_edit_flag | MR_DATATYPE_LOOPTRI);

  Mesh *me_original = me;
  MBC_GET_FINAL_MESH(me);

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("  mr_flag %u, mr_edit_flag %u\n\n", mr_flag, mr_edit_flag);
#endif

  if (me_original == me) {
    mr_flag |= mr_edit_flag;
  }

  MeshRenderData *rdata = NULL;

  if (mr_flag != 0) {
    rdata = mesh_render_data_create_ex(me, mr_flag, &cache->cd_used, ts);
  }

  /* Generate VBOs */
  if (DRW_vbo_requested(cache->ordered.pos_nor)) {
    mesh_create_pos_and_nor(rdata, cache->ordered.pos_nor);
  }
  if (DRW_vbo_requested(cache->ordered.weights)) {
    mesh_create_weights(rdata, cache->ordered.weights, &cache->weight_state);
  }
  if (DRW_vbo_requested(cache->ordered.loop_pos_nor)) {
    mesh_create_loop_pos_and_nor(rdata, cache->ordered.loop_pos_nor);
  }
  if (DRW_vbo_requested(cache->ordered.loop_edge_fac)) {
    mesh_create_loop_edge_fac(rdata, cache->ordered.loop_edge_fac);
  }
  if (DRW_vbo_requested(cache->ordered.loop_uv_tan)) {
    mesh_create_loop_uv_and_tan(rdata, cache->ordered.loop_uv_tan);
  }
  if (DRW_vbo_requested(cache->ordered.loop_orco)) {
    mesh_create_loop_orco(rdata, cache->ordered.loop_orco);
  }
  if (DRW_vbo_requested(cache->ordered.loop_vcol)) {
    mesh_create_loop_vcol(rdata, cache->ordered.loop_vcol);
  }
  if (DRW_ibo_requested(cache->ibo.edges_lines)) {
    mesh_create_edges_lines(rdata, cache->ibo.edges_lines, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.edges_adj_lines)) {
    mesh_create_edges_adjacency_lines(
        rdata, cache->ibo.edges_adj_lines, &cache->is_manifold, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.loose_edges_lines)) {
    mesh_create_loose_edges_lines(rdata, cache->ibo.loose_edges_lines, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.surf_tris)) {
    mesh_create_surf_tris(rdata, cache->ibo.surf_tris, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.loops_lines)) {
    mesh_create_loops_lines(rdata, cache->ibo.loops_lines, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.loops_line_strips)) {
    mesh_create_loops_line_strips(rdata, cache->ibo.loops_line_strips, use_hide);
  }
  if (DRW_ibo_requested(cache->ibo.loops_tris)) {
    mesh_create_loops_tris(rdata, &cache->ibo.loops_tris, 1, use_hide);
  }
  if (DRW_ibo_requested(cache->surf_per_mat_tris[0])) {
    mesh_create_loops_tris(rdata, cache->surf_per_mat_tris, cache->mat_len, use_hide);
  }

  /* Use original Mesh* to have the correct edit cage. */
  if (me_original != me && mr_edit_flag != 0) {
    if (rdata) {
      mesh_render_data_free(rdata);
    }
    rdata = mesh_render_data_create_ex(me_original, mr_edit_flag, NULL, ts);
  }

  if (rdata && rdata->mapped.supported) {
    rdata->mapped.use = true;
  }

  if (DRW_vbo_requested(cache->edit.loop_pos_nor) || DRW_vbo_requested(cache->edit.loop_lnor) ||
      DRW_vbo_requested(cache->edit.loop_data) || DRW_vbo_requested(cache->edit.loop_vert_idx) ||
      DRW_vbo_requested(cache->edit.loop_edge_idx) ||
      DRW_vbo_requested(cache->edit.loop_face_idx)) {
    mesh_create_edit_vertex_loops(rdata,
                                  cache->edit.loop_pos_nor,
                                  cache->edit.loop_lnor,
                                  NULL,
                                  cache->edit.loop_data,
                                  cache->edit.loop_vert_idx,
                                  cache->edit.loop_edge_idx,
                                  cache->edit.loop_face_idx);
  }
  if (DRW_vbo_requested(cache->edit.facedots_pos_nor_data)) {
    mesh_create_edit_facedots(rdata, cache->edit.facedots_pos_nor_data);
  }
  if (DRW_vbo_requested(cache->edit.facedots_idx)) {
    mesh_create_edit_facedots_select_id(rdata, cache->edit.facedots_idx);
  }
  if (DRW_ibo_requested(cache->ibo.edit_loops_points) ||
      DRW_ibo_requested(cache->ibo.edit_loops_lines)) {
    mesh_create_edit_loops_points_lines(
        rdata, cache->ibo.edit_loops_points, cache->ibo.edit_loops_lines);
  }
  if (DRW_ibo_requested(cache->ibo.edit_loops_tris)) {
    mesh_create_edit_loops_tris(rdata, cache->ibo.edit_loops_tris);
  }
  if (DRW_vbo_requested(cache->edit.loop_mesh_analysis)) {
    mesh_create_edit_mesh_analysis(rdata, cache->edit.loop_mesh_analysis);
  }

  /* UV editor */
  /**
   * TODO: The code and data structure is ready to support modified UV display
   * but the selection code for UVs needs to support it first. So for now, only
   * display the cage in all cases.
   */
  if (rdata && rdata->mapped.supported) {
    rdata->mapped.use = false;
  }

  if (DRW_vbo_requested(cache->edit.loop_uv_data) || DRW_vbo_requested(cache->edit.loop_uv)) {
    mesh_create_edit_vertex_loops(
        rdata, NULL, NULL, cache->edit.loop_uv, cache->edit.loop_uv_data, NULL, NULL, NULL);
  }
  if (DRW_vbo_requested(cache->edit.loop_stretch_angle) ||
      DRW_vbo_requested(cache->edit.loop_stretch_area) ||
      DRW_vbo_requested(cache->edit.facedots_uv) ||
      DRW_vbo_requested(cache->edit.facedots_uv_data) ||
      DRW_ibo_requested(cache->ibo.edituv_loops_points) ||
      DRW_ibo_requested(cache->ibo.edituv_loops_line_strips) ||
      DRW_ibo_requested(cache->ibo.edituv_loops_tri_fans)) {
    mesh_create_uvedit_buffers(rdata,
                               cache->edit.loop_stretch_area,
                               cache->edit.loop_stretch_angle,
                               cache->edit.facedots_uv,
                               cache->edit.facedots_uv_data,
                               cache->ibo.edituv_loops_points,
                               cache->ibo.edituv_loops_line_strips,
                               cache->ibo.edituv_loops_tri_fans);
  }

  if (rdata) {
    mesh_render_data_free(rdata);
  }

#ifdef DEBUG
check:
  /* Make sure all requested batches have been setup. */
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); ++i) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
  }
#endif
}

/** \} */
