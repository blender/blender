/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "DNA_customdata_types.h"
#include "DNA_defs.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_tangent.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh" /* for utility functions */

#include "MEM_guardedalloc.h"

/* interface */
#include "mikktspace.hh"

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Tangent Space Calculation
 * \{ */

/* Necessary complexity to handle looptris as quads for correct tangents. */
#define USE_LOOPTRI_DETECT_QUADS

struct SGLSLEditMeshToTangent {
  uint GetNumFaces()
  {
#ifdef USE_LOOPTRI_DETECT_QUADS
    return uint(num_face_as_quad_map);
#else
    return uint(numTessFaces);
#endif
  }

  uint GetNumVerticesOfFace(const uint face_num)
  {
#ifdef USE_LOOPTRI_DETECT_QUADS
    if (face_as_quad_map) {
      if (looptris[face_as_quad_map[face_num]][0]->f->len == 4) {
        return 4;
      }
    }
    return 3;
#else
    UNUSED_VARS(pContext, face_num);
    return 3;
#endif
  }

  const BMLoop *GetLoop(const uint face_num, uint vert_index)
  {
    // BLI_assert(vert_index >= 0 && vert_index < 4);
    BMLoop *const *ltri;
    const BMLoop *l;

#ifdef USE_LOOPTRI_DETECT_QUADS
    if (face_as_quad_map) {
      ltri = looptris[face_as_quad_map[face_num]].data();
      if (ltri[0]->f->len == 4) {
        l = BM_FACE_FIRST_LOOP(ltri[0]->f);
        while (vert_index--) {
          l = l->next;
        }
        return l;
      }
      /* fall through to regular triangle */
    }
    else {
      ltri = looptris[face_num].data();
    }
#else
    ltri = looptris[face_num].data();
#endif
    return ltri[vert_index];
  }

  mikk::float3 GetPosition(const uint face_num, const uint vert_index)
  {
    const BMLoop *l = GetLoop(face_num, vert_index);
    return mikk::float3(l->v->co);
  }

  mikk::float3 GetTexCoord(const uint face_num, const uint vert_index)
  {
    const BMLoop *l = GetLoop(face_num, vert_index);
    if (cd_loop_uv_offset != -1) {
      const float *uv = (const float *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      return mikk::float3(uv[0], uv[1], 1.0f);
    }
    const float *orco_p = orco[BM_elem_index_get(l->v)];
    float u, v;
    map_to_sphere(&u, &v, orco_p[0], orco_p[1], orco_p[2]);
    return mikk::float3(u, v, 1.0f);
  }

  mikk::float3 GetNormal(const uint face_num, const uint vert_index)
  {
    const BMLoop *l = GetLoop(face_num, vert_index);
    if (!corner_normals.is_empty()) {
      return mikk::float3(corner_normals[BM_elem_index_get(l)]);
    }
    if (BM_elem_flag_test(l->f, BM_ELEM_SMOOTH) == 0) { /* flat */
      if (!face_normals.is_empty()) {
        return mikk::float3(face_normals[BM_elem_index_get(l->f)]);
      }
      return mikk::float3(l->f->no);
    }
    return mikk::float3(l->v->no);
  }

  void SetTangentSpace(const uint face_num,
                       const uint vert_index,
                       mikk::float3 T,
                       bool orientation)
  {
    const BMLoop *l = GetLoop(face_num, vert_index);
    float *p_res = tangent[BM_elem_index_get(l)];
    copy_v4_fl4(p_res, T.x, T.y, T.z, orientation ? 1.0f : -1.0f);
  }

  Span<float3> face_normals;
  Span<float3> corner_normals;
  Span<std::array<BMLoop *, 3>> looptris;
  int cd_loop_uv_offset; /* texture coordinates */
  Span<float3> orco;
  float (*tangent)[4]; /* destination */
  int numTessFaces;

#ifdef USE_LOOPTRI_DETECT_QUADS
  /* map from 'fake' face index to looptris,
   * quads will point to the first looptri of the quad */
  const int *face_as_quad_map;
  int num_face_as_quad_map;
#endif
};

static void emDM_calc_loop_tangents_thread(TaskPool *__restrict /*pool*/, void *taskdata)
{
  SGLSLEditMeshToTangent *mesh_data = static_cast<SGLSLEditMeshToTangent *>(taskdata);

  mikk::Mikktspace<SGLSLEditMeshToTangent> mikk(*mesh_data);
  mikk.genTangSpace();
}

void BKE_editmesh_loop_tangent_calc(BMEditMesh *em,
                                    bool calc_active_tangent,
                                    const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                    int tangent_names_len,
                                    const Span<float3> face_normals,
                                    const Span<float3> corner_normals,
                                    const Span<float3> vert_orco,
                                    /* result */
                                    CustomData *loopdata_out,
                                    const uint loopdata_out_len,
                                    short *tangent_mask_curr_p)
{
  BMesh *bm = em->bm;

  int act_uv_n = -1;
  int ren_uv_n = -1;
  bool calc_act = false;
  bool calc_ren = false;
  char act_uv_name[MAX_NAME];
  char ren_uv_name[MAX_NAME];
  short tangent_mask = 0;
  short tangent_mask_curr = *tangent_mask_curr_p;

  BKE_mesh_calc_loop_tangent_step_0(&bm->ldata,
                                    calc_active_tangent,
                                    tangent_names,
                                    tangent_names_len,
                                    &calc_act,
                                    &calc_ren,
                                    &act_uv_n,
                                    &ren_uv_n,
                                    act_uv_name,
                                    ren_uv_name,
                                    &tangent_mask);

  if ((tangent_mask_curr | tangent_mask) != tangent_mask_curr) {
    for (int i = 0; i < tangent_names_len; i++) {
      if (tangent_names[i][0]) {
        BKE_mesh_add_loop_tangent_named_layer_for_uv(
            &bm->ldata, loopdata_out, int(loopdata_out_len), tangent_names[i]);
      }
    }
    if ((tangent_mask & DM_TANGENT_MASK_ORCO) &&
        CustomData_get_named_layer_index(loopdata_out, CD_TANGENT, "") == -1)
    {
      CustomData_add_layer_named(
          loopdata_out, CD_TANGENT, CD_SET_DEFAULT, int(loopdata_out_len), "");
    }
    if (calc_act && act_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          &bm->ldata, loopdata_out, int(loopdata_out_len), act_uv_name);
    }
    if (calc_ren && ren_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          &bm->ldata, loopdata_out, int(loopdata_out_len), ren_uv_name);
    }
    int totface = em->looptris.size();
#ifdef USE_LOOPTRI_DETECT_QUADS
    int num_face_as_quad_map;
    int *face_as_quad_map = nullptr;

    /* map faces to quads */
    if (em->looptris.size() != bm->totface) {
      /* Over allocate, since we don't know how many ngon or quads we have. */

      /* map fake face index to looptri */
      face_as_quad_map = static_cast<int *>(MEM_mallocN(sizeof(int) * totface, __func__));
      int i, j;
      for (i = 0, j = 0; j < totface; i++, j++) {
        face_as_quad_map[i] = j;
        /* step over all quads */
        if (em->looptris[j][0]->f->len == 4) {
          j++; /* Skips the next looptri. */
        }
      }
      num_face_as_quad_map = i;
    }
    else {
      num_face_as_quad_map = totface;
    }
#endif
    /* Calculation */
    if (em->looptris.size() != 0) {
      TaskPool *task_pool;
      task_pool = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);

      tangent_mask_curr = 0;
      /* Calculate tangent layers */
      SGLSLEditMeshToTangent data_array[MAX_MTFACE];
      int index = 0;
      int n = 0;
      CustomData_update_typemap(loopdata_out);
      const int tangent_layer_num = CustomData_number_of_layers(loopdata_out, CD_TANGENT);
      for (n = 0; n < tangent_layer_num; n++) {
        index = CustomData_get_layer_index_n(loopdata_out, CD_TANGENT, n);
        BLI_assert(n < MAX_MTFACE);
        SGLSLEditMeshToTangent *mesh2tangent = &data_array[n];
        mesh2tangent->numTessFaces = em->looptris.size();
#ifdef USE_LOOPTRI_DETECT_QUADS
        mesh2tangent->face_as_quad_map = face_as_quad_map;
        mesh2tangent->num_face_as_quad_map = num_face_as_quad_map;
#endif
        mesh2tangent->face_normals = face_normals;
        /* NOTE: we assume we do have tessellated loop normals at this point
         * (in case it is object-enabled), have to check this is valid. */
        mesh2tangent->corner_normals = corner_normals;
        mesh2tangent->cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_PROP_FLOAT2, n);

        /* needed for indexing loop-tangents */
        int htype_index = BM_LOOP;
        if (mesh2tangent->cd_loop_uv_offset == -1) {
          mesh2tangent->orco = vert_orco;
          if (mesh2tangent->orco.is_empty()) {
            continue;
          }
          /* needed for orco lookups */
          htype_index |= BM_VERT;
          tangent_mask_curr |= DM_TANGENT_MASK_ORCO;
        }
        else {
          /* Fill the resulting tangent_mask */
          int uv_ind = CustomData_get_named_layer_index(
              &bm->ldata, CD_PROP_FLOAT2, loopdata_out->layers[index].name);
          int uv_start = CustomData_get_layer_index(&bm->ldata, CD_PROP_FLOAT2);
          BLI_assert(uv_ind != -1 && uv_start != -1);
          BLI_assert(uv_ind - uv_start < MAX_MTFACE);
          tangent_mask_curr |= 1 << (uv_ind - uv_start);
        }
        if (!mesh2tangent->face_normals.is_empty()) {
          /* needed for face normal lookups */
          htype_index |= BM_FACE;
        }
        BM_mesh_elem_index_ensure(bm, htype_index);

        mesh2tangent->looptris = em->looptris;
        mesh2tangent->tangent = static_cast<float(*)[4]>(loopdata_out->layers[index].data);

        BLI_task_pool_push(
            task_pool, emDM_calc_loop_tangents_thread, mesh2tangent, false, nullptr);
      }

      BLI_assert(tangent_mask_curr == tangent_mask);
      BLI_task_pool_work_and_wait(task_pool);
      BLI_task_pool_free(task_pool);
    }
    else {
      tangent_mask_curr = tangent_mask;
    }
#ifdef USE_LOOPTRI_DETECT_QUADS
    if (face_as_quad_map) {
      MEM_freeN(face_as_quad_map);
    }
#  undef USE_LOOPTRI_DETECT_QUADS
#endif
  }

  *tangent_mask_curr_p = tangent_mask_curr;

  int act_uv_index = CustomData_get_layer_index_n(&bm->ldata, CD_PROP_FLOAT2, act_uv_n);
  if (act_uv_index >= 0) {
    int tan_index = CustomData_get_named_layer_index(
        loopdata_out, CD_TANGENT, bm->ldata.layers[act_uv_index].name);
    CustomData_set_layer_active_index(loopdata_out, CD_TANGENT, tan_index);
  } /* else tangent has been built from orco */

  /* Update render layer index */
  int ren_uv_index = CustomData_get_layer_index_n(&bm->ldata, CD_PROP_FLOAT2, ren_uv_n);
  if (ren_uv_index >= 0) {
    int tan_index = CustomData_get_named_layer_index(
        loopdata_out, CD_TANGENT, bm->ldata.layers[ren_uv_index].name);
    CustomData_set_layer_render_index(loopdata_out, CD_TANGENT, tan_index);
  } /* else tangent has been built from orco */
}

/** \} */
