/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "DNA_customdata_types.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_tangent.hh"
#include "BKE_mesh.hh"

#include "MEM_guardedalloc.h"

/* interface */
#include "mikktspace.hh"

using blender::Array;
using blender::float3;
using blender::float4;
using blender::Span;
using blender::StringRef;

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
    if (has_uv()) {
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

  bool has_uv()
  {
    return cd_loop_uv_offset != -1;
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

static void calc_face_as_quad_map(
    BMEditMesh *&em, BMesh *&bm, int &totface, int &num_face_as_quad_map, int *&face_as_quad_map)
{
#ifdef USE_LOOPTRI_DETECT_QUADS
  if (em->looptris.size() != bm->totface) {
    /* Over allocate, since we don't know how many ngon or quads we have. */

    /* map fake face index to looptri */
    face_as_quad_map = MEM_malloc_arrayN<int>(size_t(totface), __func__);
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
#else
  num_face_as_quad_map = totface;
  face_as_quad_map = nullptr;
#endif
}

Array<Array<float4>> BKE_editmesh_uv_tangents_calc(BMEditMesh *em,
                                                   const Span<float3> face_normals,
                                                   const Span<float3> corner_normals,
                                                   const Span<StringRef> uv_names)
{
  using namespace blender;
  if (em->looptris.is_empty()) {
    return {};
  }

  BMesh *bm = em->bm;

  int totface = em->looptris.size();
  int num_face_as_quad_map;
  int *face_as_quad_map = nullptr;
  calc_face_as_quad_map(em, bm, totface, num_face_as_quad_map, face_as_quad_map);

  Array<Array<float4>> result(uv_names.size());

  /* needed for indexing loop-tangents */
  int htype_index = BM_LOOP;
  if (!face_normals.is_empty()) {
    /* needed for face normal lookups */
    htype_index |= BM_FACE;
  }
  BM_mesh_elem_index_ensure(bm, htype_index);

  threading::parallel_for(uv_names.index_range(), 1, [&](const IndexRange range) {
    for (const int n : range) {
      SGLSLEditMeshToTangent mesh2tangent{};
      mesh2tangent.numTessFaces = em->looptris.size();
      mesh2tangent.face_as_quad_map = face_as_quad_map;
      mesh2tangent.num_face_as_quad_map = num_face_as_quad_map;
      mesh2tangent.face_normals = face_normals;
      /* NOTE: we assume we do have tessellated loop normals at this point
       * (in case it is object-enabled), have to check this is valid. */
      mesh2tangent.corner_normals = corner_normals;
      mesh2tangent.cd_loop_uv_offset = CustomData_get_offset_named(
          &bm->ldata, CD_PROP_FLOAT2, uv_names[n]);
      BLI_assert(mesh2tangent.cd_loop_uv_offset != -1);

      mesh2tangent.looptris = em->looptris;
      result[n].reinitialize(bm->totloop);
      mesh2tangent.tangent = reinterpret_cast<float (*)[4]>(result[n].data());

      mikk::Mikktspace<SGLSLEditMeshToTangent> mikk(mesh2tangent);
      mikk.genTangSpace();
    }
  });

  MEM_SAFE_FREE(face_as_quad_map);

  return result;
}

Array<float4> BKE_editmesh_orco_tangents_calc(BMEditMesh *em,
                                              const Span<float3> face_normals,
                                              const Span<float3> corner_normals,
                                              const Span<float3> vert_orco)
{
  if (em->looptris.is_empty()) {
    return {};
  }

  BMesh *bm = em->bm;

  int totface = em->looptris.size();
  int num_face_as_quad_map;
  int *face_as_quad_map = nullptr;
  calc_face_as_quad_map(em, bm, totface, num_face_as_quad_map, face_as_quad_map);

  Array<float4> result(bm->totloop);

  /* needed for indexing loop-tangents */
  int htype_index = BM_LOOP;
  /* needed for orco lookups */
  htype_index |= BM_VERT;
  if (!face_normals.is_empty()) {
    /* needed for face normal lookups */
    htype_index |= BM_FACE;
  }
  BM_mesh_elem_index_ensure(bm, htype_index);

  SGLSLEditMeshToTangent mesh2tangent{};
  mesh2tangent.numTessFaces = em->looptris.size();
  mesh2tangent.face_as_quad_map = face_as_quad_map;
  mesh2tangent.num_face_as_quad_map = num_face_as_quad_map;
  mesh2tangent.face_normals = face_normals;
  /* NOTE: we assume we do have tessellated loop normals at this point
   * (in case it is object-enabled), have to check this is valid. */
  mesh2tangent.corner_normals = corner_normals;
  mesh2tangent.cd_loop_uv_offset = -1;
  mesh2tangent.orco = vert_orco;

  mesh2tangent.looptris = em->looptris;
  mesh2tangent.tangent = reinterpret_cast<float (*)[4]>(result.data());
  mikk::Mikktspace<SGLSLEditMeshToTangent> mikk(mesh2tangent);
  mikk.genTangSpace();

  MEM_SAFE_FREE(face_as_quad_map);

  return result;
}

/** \} */
