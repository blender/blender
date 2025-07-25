/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh tangents.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_mesh_tangent.hh"
#include "BKE_report.hh"

#include "mikktspace.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* -------------------------------------------------------------------- */
/** \name Mesh Tangent Calculations (Single Layer)
 * \{ */

namespace blender::bke::mesh {

struct BKEMeshToTangent {
  uint GetNumFaces()
  {
    return uint(faces.size());
  }

  uint GetNumVerticesOfFace(const uint face_num)
  {
    return uint(faces[face_num].size());
  }

  mikk::float3 GetPosition(const uint face_num, const uint vert_num)
  {
    const uint loop_idx = uint(faces[face_num].start()) + vert_num;
    return mikk::float3(positions[corner_verts[loop_idx]]);
  }

  mikk::float3 GetTexCoord(const uint face_num, const uint vert_num)
  {
    const float *uv = luvs[uint(faces[face_num].start()) + vert_num];
    return mikk::float3(uv[0], uv[1], 1.0f);
  }

  mikk::float3 GetNormal(const uint face_num, const uint vert_num)
  {
    return mikk::float3(corner_normals[uint(faces[face_num].start()) + vert_num]);
  }

  void SetTangentSpace(const uint face_num, const uint vert_num, mikk::float3 T, bool orientation)
  {
    float *p_res = tangents[uint(faces[face_num].start()) + vert_num];
    copy_v4_fl4(p_res, T.x, T.y, T.z, orientation ? 1.0f : -1.0f);
  }

  bool has_uv() const
  {
    return true;
  }

  OffsetIndices<int> faces;     /* faces */
  Span<int> corner_verts;       /* faces vertices */
  Span<float3> positions;       /* vertices */
  Span<float2> luvs;            /* texture coordinates */
  Span<float3> corner_normals;  /* loops' normals */
  MutableSpan<float4> tangents; /* output tangents */
};

void calc_uv_tangent_tris_quads(const Span<float3> vert_positions,
                                const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const Span<float3> corner_normals,
                                const Span<float2> uv_map,
                                MutableSpan<float4> results,
                                ReportList *reports)
{
  BKEMeshToTangent mesh_to_tangent;
  mesh_to_tangent.faces = faces;
  mesh_to_tangent.corner_verts = corner_verts;
  mesh_to_tangent.positions = vert_positions;
  mesh_to_tangent.luvs = uv_map;
  mesh_to_tangent.corner_normals = corner_normals;
  mesh_to_tangent.tangents = results;

  mikk::Mikktspace<BKEMeshToTangent> mikk(mesh_to_tangent);

  /* First check we do have a tris/quads only mesh. */
  for (const int64_t i : faces.index_range()) {
    if (faces[i].size() > 4) {
      BKE_report(
          reports, RPT_ERROR, "Tangent space can only be computed for tris/quads, aborting");
      return;
    }
  }

  mikk.genTangSpace();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Tangent Calculations (All Layers)
 * \{ */

/* Necessary complexity to handle corner_tris as quads for correct tangents. */
#define USE_TRI_DETECT_QUADS

struct SGLSLMeshToTangent {
  uint GetNumFaces()
  {
#ifdef USE_TRI_DETECT_QUADS
    return uint(num_face_as_quad_map);
#else
    return uint(numTessFaces);
#endif
  }

  uint GetNumVerticesOfFace(const uint face_num)
  {
#ifdef USE_TRI_DETECT_QUADS
    if (face_as_quad_map) {
      const int face_index = tri_faces[face_as_quad_map[face_num]];
      if (faces[face_index].size() == 4) {
        return 4;
      }
    }
    return 3;
#else
    UNUSED_VARS(pContext, face_num);
    return 3;
#endif
  }

  uint GetLoop(const uint face_num, const uint vert_num, int3 &tri, int &face_index)
  {
#ifdef USE_TRI_DETECT_QUADS
    if (face_as_quad_map) {
      tri = corner_tris[face_as_quad_map[face_num]];
      face_index = tri_faces[face_as_quad_map[face_num]];
      if (faces[face_index].size() == 4) {
        return uint(faces[face_index][vert_num]);
      }
      /* fall through to regular triangle */
    }
    else {
      tri = corner_tris[face_num];
      face_index = tri_faces[face_num];
    }
#else
    tri = &corner_tris[face_num];
#endif

    /* Safe to suppress since the way `face_as_quad_map` is used
     * prevents out-of-bounds reads on the 4th component of the `int3`. */
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

    return uint(tri[int(vert_num)]);

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  }

  mikk::float3 GetPosition(const uint face_num, const uint vert_num)
  {
    int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    return mikk::float3(positions[corner_verts[loop_index]]);
  }

  mikk::float3 GetTexCoord(const uint face_num, const uint vert_num)
  {
    int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    if (has_uv()) {
      const float2 &uv = mloopuv[loop_index];
      return mikk::float3(uv[0], uv[1], 1.0f);
    }
    const float *l_orco = orco[corner_verts[loop_index]];
    float u, v;
    map_to_sphere(&u, &v, l_orco[0], l_orco[1], l_orco[2]);
    return mikk::float3(u, v, 1.0f);
  }

  mikk::float3 GetNormal(const uint face_num, const uint vert_num)
  {
    int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    if (!corner_normals.is_empty()) {
      return mikk::float3(corner_normals[loop_index]);
    }
    if (!sharp_faces.is_empty() && sharp_faces[face_index]) { /* flat */
      if (!face_normals.is_empty()) {
        return mikk::float3(face_normals[face_index]);
      }
#ifdef USE_TRI_DETECT_QUADS
      const blender::IndexRange face = faces[face_index];
      float normal[3];
      if (face.size() == 4) {
        normal_quad_v3(normal,
                       positions[corner_verts[face[0]]],
                       positions[corner_verts[face[1]]],
                       positions[corner_verts[face[2]]],
                       positions[corner_verts[face[3]]]);
      }
      else
#endif
      {
        normal_tri_v3(normal,
                      positions[corner_verts[tri[0]]],
                      positions[corner_verts[tri[1]]],
                      positions[corner_verts[tri[2]]]);
      }
      return mikk::float3(normal);
    }
    return mikk::float3(vert_normals[corner_verts[loop_index]]);
  }

  void SetTangentSpace(const uint face_num, const uint vert_num, mikk::float3 T, bool orientation)
  {
    int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);

    copy_v4_fl4(tangent[loop_index], T.x, T.y, T.z, orientation ? 1.0f : -1.0f);
  }

  bool has_uv() const
  {
    return !mloopuv.is_empty();
  }

  Span<float3> face_normals;
  Span<float3> corner_normals;
  const int3 *corner_tris;
  const int *tri_faces;
  Span<float2> mloopuv; /* texture coordinates */
  OffsetIndices<int> faces;
  const int *corner_verts; /* indices */
  Span<float3> positions;  /* vertex coordinates */
  Span<float3> vert_normals;
  Span<float3> orco;
  float (*tangent)[4]; /* destination */
  Span<bool> sharp_faces;
  int numTessFaces;

#ifdef USE_TRI_DETECT_QUADS
  /* map from 'fake' face index to corner_tris,
   * quads will point to the first corner_tris of the quad */
  const int *face_as_quad_map;
  int num_face_as_quad_map;
#endif
};

static void calc_face_as_quad_map(const OffsetIndices<int> &faces,
                                  const Span<int3> &corner_tris,
                                  const Span<int> &corner_tri_faces,
                                  int &num_face_as_quad_map,
                                  int *&face_as_quad_map)
{
#ifdef USE_TRI_DETECT_QUADS
  if (corner_tris.size() != faces.size()) {
    /* Over allocate, since we don't know how many ngon or quads we have. */

    /* Map fake face index to corner_tris. */
    face_as_quad_map = MEM_malloc_arrayN<int>(size_t(corner_tris.size()), __func__);
    int k, j;
    for (k = 0, j = 0; j < int(corner_tris.size()); k++, j++) {
      face_as_quad_map[k] = j;
      /* step over all quads */
      if (faces[corner_tri_faces[j]].size() == 4) {
        j++; /* Skips the next corner_tri. */
      }
    }
    num_face_as_quad_map = k;
  }
  else {
    num_face_as_quad_map = int(corner_tris.size());
  }
#else
  num_face_as_quad_map = 0;
  face_as_quad_map = nullptr;
#endif
}

Array<Array<float4>> calc_uv_tangents(const Span<float3> vert_positions,
                                      const OffsetIndices<int> faces,
                                      const Span<int> corner_verts,
                                      const Span<int3> corner_tris,
                                      const Span<int> corner_tri_faces,
                                      const Span<bool> sharp_faces,
                                      const Span<float3> vert_normals,
                                      const Span<float3> face_normals,
                                      const Span<float3> corner_normals,
                                      const Span<Span<float2>> uv_maps)
{
  if (corner_tris.is_empty()) {
    return {};
  }

  int num_face_as_quad_map;
  int *face_as_quad_map = nullptr;
  calc_face_as_quad_map(
      faces, corner_tris, corner_tri_faces, num_face_as_quad_map, face_as_quad_map);

  Array<Array<float4>> results(uv_maps.size());
  threading::parallel_for(uv_maps.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t i : range) {
      SGLSLMeshToTangent mesh2tangent{};
      mesh2tangent.numTessFaces = int(corner_tris.size());
      mesh2tangent.face_as_quad_map = face_as_quad_map;
      mesh2tangent.num_face_as_quad_map = num_face_as_quad_map;
      mesh2tangent.positions = vert_positions;
      mesh2tangent.vert_normals = vert_normals;
      mesh2tangent.faces = faces;
      mesh2tangent.corner_verts = corner_verts.data();
      mesh2tangent.corner_tris = corner_tris.data();
      mesh2tangent.tri_faces = corner_tri_faces.data();
      mesh2tangent.sharp_faces = sharp_faces;
      /* NOTE: we assume we do have tessellated loop normals at this point
       * (in case it is object-enabled), have to check this is valid. */
      mesh2tangent.corner_normals = corner_normals;
      mesh2tangent.face_normals = face_normals;
      mesh2tangent.mloopuv = uv_maps[i];

      results[i].reinitialize(corner_verts.size());
      mesh2tangent.tangent = reinterpret_cast<float(*)[4]>(results[i].data());

      mikk::Mikktspace<SGLSLMeshToTangent> mikk(mesh2tangent);
      mikk.genTangSpace();
    }
  });

  MEM_SAFE_FREE(face_as_quad_map);

  return results;
}

Array<float4> calc_orco_tangents(const Span<float3> vert_positions,
                                 const OffsetIndices<int> faces,
                                 const Span<int> corner_verts,
                                 const Span<int3> corner_tris,
                                 const Span<int> corner_tri_faces,
                                 const Span<bool> sharp_faces,
                                 const Span<float3> vert_normals,
                                 const Span<float3> face_normals,
                                 const Span<float3> corner_normals,
                                 const Span<float3> vert_orco)
{
  if (corner_tris.is_empty()) {
    return {};
  }

  int num_face_as_quad_map;
  int *face_as_quad_map = nullptr;
  calc_face_as_quad_map(
      faces, corner_tris, corner_tri_faces, num_face_as_quad_map, face_as_quad_map);

  Array<float4> results(corner_verts.size());
  SGLSLMeshToTangent mesh2tangent{};
  mesh2tangent.numTessFaces = int(corner_tris.size());
  mesh2tangent.face_as_quad_map = face_as_quad_map;
  mesh2tangent.num_face_as_quad_map = num_face_as_quad_map;
  mesh2tangent.positions = vert_positions;
  mesh2tangent.vert_normals = vert_normals;
  mesh2tangent.faces = faces;
  mesh2tangent.corner_verts = corner_verts.data();
  mesh2tangent.corner_tris = corner_tris.data();
  mesh2tangent.tri_faces = corner_tri_faces.data();
  mesh2tangent.sharp_faces = sharp_faces;
  /* NOTE: we assume we do have tessellated loop normals at this point
   * (in case it is object-enabled), have to check this is valid. */
  mesh2tangent.corner_normals = corner_normals;
  mesh2tangent.face_normals = face_normals;
  mesh2tangent.orco = vert_orco;

  mesh2tangent.tangent = reinterpret_cast<float(*)[4]>(results.data());

  mikk::Mikktspace<SGLSLMeshToTangent> mikk(mesh2tangent);
  mikk.genTangSpace();

  MEM_SAFE_FREE(face_as_quad_map);

  return results;
}

}  // namespace blender::bke::mesh

/** \} */
