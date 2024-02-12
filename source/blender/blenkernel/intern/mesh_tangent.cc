/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh tangents.
 */

#include <climits>

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh"
#include "BKE_report.hh"

#include "BLI_strict_flags.h"

#include "mikktspace.hh"

using blender::float2;

/* -------------------------------------------------------------------- */
/** \name Mesh Tangent Calculations (Single Layer)
 * \{ */

struct BKEMeshToTangent {
  uint GetNumFaces()
  {
    return uint(num_faces);
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

  blender::OffsetIndices<int> faces; /* faces */
  const int *corner_verts;           /* faces vertices */
  const float (*positions)[3];       /* vertices */
  const float (*luvs)[2];            /* texture coordinates */
  const float (*corner_normals)[3];  /* loops' normals */
  float (*tangents)[4];              /* output tangents */
  int num_faces;                     /* number of polygons */
};

void BKE_mesh_calc_loop_tangent_single_ex(const float (*vert_positions)[3],
                                          const int /*numVerts*/,
                                          const int *corner_verts,
                                          float (*r_looptangent)[4],
                                          const float (*corner_normals)[3],
                                          const float (*loop_uvs)[2],
                                          const int /*numLoops*/,
                                          const blender::OffsetIndices<int> faces,
                                          ReportList *reports)
{
  /* Compute Mikktspace's tangent normals. */
  BKEMeshToTangent mesh_to_tangent;
  mesh_to_tangent.faces = faces;
  mesh_to_tangent.corner_verts = corner_verts;
  mesh_to_tangent.positions = vert_positions;
  mesh_to_tangent.luvs = loop_uvs;
  mesh_to_tangent.corner_normals = corner_normals;
  mesh_to_tangent.tangents = r_looptangent;
  mesh_to_tangent.num_faces = int(faces.size());

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

void BKE_mesh_calc_loop_tangent_single(Mesh *mesh,
                                       const char *uvmap,
                                       float (*r_looptangents)[4],
                                       ReportList *reports)
{
  using namespace blender;
  using namespace blender::bke;
  if (!uvmap) {
    uvmap = CustomData_get_active_layer_name(&mesh->corner_data, CD_PROP_FLOAT2);
  }

  const AttributeAccessor attributes = mesh->attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(uvmap, AttrDomain::Corner);
  if (uv_map.is_empty()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Tangent space computation needs a UV Map, \"%s\" not found, aborting",
                uvmap);
    return;
  }

  BKE_mesh_calc_loop_tangent_single_ex(
      reinterpret_cast<const float(*)[3]>(mesh->vert_positions().data()),
      mesh->verts_num,
      mesh->corner_verts().data(),
      r_looptangents,
      reinterpret_cast<const float(*)[3]>(mesh->corner_normals().data()),
      reinterpret_cast<const float(*)[2]>(uv_map.data()),
      mesh->corners_num,
      mesh->faces(),
      reports);
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

  uint GetLoop(const uint face_num, const uint vert_num, blender::int3 &tri, int &face_index)
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
    return uint(tri[int(vert_num)]);
  }

  mikk::float3 GetPosition(const uint face_num, const uint vert_num)
  {
    blender::int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    return mikk::float3(positions[corner_verts[loop_index]]);
  }

  mikk::float3 GetTexCoord(const uint face_num, const uint vert_num)
  {
    blender::int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    if (mloopuv != nullptr) {
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
    blender::int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);
    if (precomputedLoopNormals) {
      return mikk::float3(precomputedLoopNormals[loop_index]);
    }
    if (!sharp_faces.is_empty() && sharp_faces[face_index]) { /* flat */
      if (precomputedFaceNormals) {
        return mikk::float3(precomputedFaceNormals[face_index]);
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
    blender::int3 tri;
    int face_index;
    uint loop_index = GetLoop(face_num, vert_num, tri, face_index);

    copy_v4_fl4(tangent[loop_index], T.x, T.y, T.z, orientation ? 1.0f : -1.0f);
  }

  const float (*precomputedFaceNormals)[3];
  const float (*precomputedLoopNormals)[3];
  const blender::int3 *corner_tris;
  const int *tri_faces;
  const float2 *mloopuv; /* texture coordinates */
  blender::OffsetIndices<int> faces;
  const int *corner_verts;     /* indices */
  const float (*positions)[3]; /* vertex coordinates */
  const float (*vert_normals)[3];
  const float (*orco)[3];
  float (*tangent)[4]; /* destination */
  blender::Span<bool> sharp_faces;
  int numTessFaces;

#ifdef USE_TRI_DETECT_QUADS
  /* map from 'fake' face index to corner_tris,
   * quads will point to the first corner_tris of the quad */
  const int *face_as_quad_map;
  int num_face_as_quad_map;
#endif
};

static void DM_calc_loop_tangents_thread(TaskPool *__restrict /*pool*/, void *taskdata)
{
  SGLSLMeshToTangent *mesh_data = static_cast<SGLSLMeshToTangent *>(taskdata);

  mikk::Mikktspace<SGLSLMeshToTangent> mikk(*mesh_data);
  mikk.genTangSpace();
}

void BKE_mesh_add_loop_tangent_named_layer_for_uv(CustomData *uv_data,
                                                  CustomData *tan_data,
                                                  int numLoopData,
                                                  const char *layer_name)
{
  if (CustomData_get_named_layer_index(tan_data, CD_TANGENT, layer_name) == -1 &&
      CustomData_get_named_layer_index(uv_data, CD_PROP_FLOAT2, layer_name) != -1)
  {
    CustomData_add_layer_named(tan_data, CD_TANGENT, CD_SET_DEFAULT, numLoopData, layer_name);
  }
}

void BKE_mesh_calc_loop_tangent_step_0(const CustomData *loopData,
                                       bool calc_active_tangent,
                                       const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                       int tangent_names_count,
                                       bool *rcalc_act,
                                       bool *rcalc_ren,
                                       int *ract_uv_n,
                                       int *rren_uv_n,
                                       char *ract_uv_name,
                                       char *rren_uv_name,
                                       short *rtangent_mask)
{
  /* Active uv in viewport */
  int layer_index = CustomData_get_layer_index(loopData, CD_PROP_FLOAT2);
  *ract_uv_n = CustomData_get_active_layer(loopData, CD_PROP_FLOAT2);
  ract_uv_name[0] = 0;
  if (*ract_uv_n != -1) {
    BLI_strncpy(
        ract_uv_name, loopData->layers[*ract_uv_n + layer_index].name, MAX_CUSTOMDATA_LAYER_NAME);
  }

  /* Active tangent in render */
  *rren_uv_n = CustomData_get_render_layer(loopData, CD_PROP_FLOAT2);
  rren_uv_name[0] = 0;
  if (*rren_uv_n != -1) {
    BLI_strncpy(
        rren_uv_name, loopData->layers[*rren_uv_n + layer_index].name, MAX_CUSTOMDATA_LAYER_NAME);
  }

  /* If active tangent not in tangent_names we take it into account */
  *rcalc_act = false;
  *rcalc_ren = false;
  for (int i = 0; i < tangent_names_count; i++) {
    if (tangent_names[i][0] == 0) {
      calc_active_tangent = true;
    }
  }
  if (calc_active_tangent) {
    *rcalc_act = true;
    *rcalc_ren = true;
    for (int i = 0; i < tangent_names_count; i++) {
      if (STREQ(ract_uv_name, tangent_names[i])) {
        *rcalc_act = false;
      }
      if (STREQ(rren_uv_name, tangent_names[i])) {
        *rcalc_ren = false;
      }
    }
  }
  *rtangent_mask = 0;

  const int uv_layer_num = CustomData_number_of_layers(loopData, CD_PROP_FLOAT2);
  for (int n = 0; n < uv_layer_num; n++) {
    const char *name = CustomData_get_layer_name(loopData, CD_PROP_FLOAT2, n);
    bool add = false;
    for (int i = 0; i < tangent_names_count; i++) {
      if (tangent_names[i][0] && STREQ(tangent_names[i], name)) {
        add = true;
        break;
      }
    }
    if (!add && ((*rcalc_act && ract_uv_name[0] && STREQ(ract_uv_name, name)) ||
                 (*rcalc_ren && rren_uv_name[0] && STREQ(rren_uv_name, name))))
    {
      add = true;
    }
    if (add) {
      *rtangent_mask |= short(1 << n);
    }
  }

  if (uv_layer_num == 0) {
    *rtangent_mask |= DM_TANGENT_MASK_ORCO;
  }
}

void BKE_mesh_calc_loop_tangent_ex(const float (*vert_positions)[3],
                                   const blender::OffsetIndices<int> faces,
                                   const int *corner_verts,
                                   const blender::int3 *corner_tris,
                                   const int *corner_tri_faces,
                                   const uint corner_tris_len,
                                   const blender::Span<bool> sharp_faces,

                                   CustomData *loopdata,
                                   bool calc_active_tangent,
                                   const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                   int tangent_names_len,
                                   const float (*vert_normals)[3],
                                   const float (*face_normals)[3],
                                   const float (*corner_normals)[3],
                                   const float (*vert_orco)[3],
                                   /* result */
                                   CustomData *loopdata_out,
                                   const uint loopdata_out_len,
                                   short *tangent_mask_curr_p)
{
  int act_uv_n = -1;
  int ren_uv_n = -1;
  bool calc_act = false;
  bool calc_ren = false;
  char act_uv_name[MAX_CUSTOMDATA_LAYER_NAME];
  char ren_uv_name[MAX_CUSTOMDATA_LAYER_NAME];
  short tangent_mask = 0;
  short tangent_mask_curr = *tangent_mask_curr_p;

  BKE_mesh_calc_loop_tangent_step_0(loopdata,
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
    /* Check we have all the needed layers */
    /* Allocate needed tangent layers */
    for (int i = 0; i < tangent_names_len; i++) {
      if (tangent_names[i][0]) {
        BKE_mesh_add_loop_tangent_named_layer_for_uv(
            loopdata, loopdata_out, int(loopdata_out_len), tangent_names[i]);
      }
    }
    if ((tangent_mask & DM_TANGENT_MASK_ORCO) &&
        CustomData_get_named_layer_index(loopdata, CD_TANGENT, "") == -1)
    {
      CustomData_add_layer_named(
          loopdata_out, CD_TANGENT, CD_SET_DEFAULT, int(loopdata_out_len), "");
    }
    if (calc_act && act_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          loopdata, loopdata_out, int(loopdata_out_len), act_uv_name);
    }
    if (calc_ren && ren_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          loopdata, loopdata_out, int(loopdata_out_len), ren_uv_name);
    }

#ifdef USE_TRI_DETECT_QUADS
    int num_face_as_quad_map;
    int *face_as_quad_map = nullptr;

    /* map faces to quads */
    if (corner_tris_len != uint(faces.size())) {
      /* Over allocate, since we don't know how many ngon or quads we have. */

      /* Map fake face index to corner_tris. */
      face_as_quad_map = static_cast<int *>(MEM_mallocN(sizeof(int) * corner_tris_len, __func__));
      int k, j;
      for (k = 0, j = 0; j < int(corner_tris_len); k++, j++) {
        face_as_quad_map[k] = j;
        /* step over all quads */
        if (faces[corner_tri_faces[j]].size() == 4) {
          j++; /* Skips the next corner_tri. */
        }
      }
      num_face_as_quad_map = k;
    }
    else {
      num_face_as_quad_map = int(corner_tris_len);
    }
#endif

    /* Calculation */
    if (corner_tris_len != 0) {
      TaskPool *task_pool = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);

      tangent_mask_curr = 0;
      /* Calculate tangent layers */
      SGLSLMeshToTangent data_array[MAX_MTFACE];
      const int tangent_layer_num = CustomData_number_of_layers(loopdata_out, CD_TANGENT);
      for (int n = 0; n < tangent_layer_num; n++) {
        int index = CustomData_get_layer_index_n(loopdata_out, CD_TANGENT, n);
        BLI_assert(n < MAX_MTFACE);
        SGLSLMeshToTangent *mesh2tangent = &data_array[n];
        mesh2tangent->numTessFaces = int(corner_tris_len);
#ifdef USE_TRI_DETECT_QUADS
        mesh2tangent->face_as_quad_map = face_as_quad_map;
        mesh2tangent->num_face_as_quad_map = num_face_as_quad_map;
#endif
        mesh2tangent->positions = vert_positions;
        mesh2tangent->vert_normals = vert_normals;
        mesh2tangent->faces = faces;
        mesh2tangent->corner_verts = corner_verts;
        mesh2tangent->corner_tris = corner_tris;
        mesh2tangent->tri_faces = corner_tri_faces;
        mesh2tangent->sharp_faces = sharp_faces;
        /* NOTE: we assume we do have tessellated loop normals at this point
         * (in case it is object-enabled), have to check this is valid. */
        mesh2tangent->precomputedLoopNormals = corner_normals;
        mesh2tangent->precomputedFaceNormals = face_normals;

        mesh2tangent->orco = nullptr;
        mesh2tangent->mloopuv = static_cast<const float2 *>(CustomData_get_layer_named(
            loopdata, CD_PROP_FLOAT2, loopdata_out->layers[index].name));

        /* Fill the resulting tangent_mask */
        if (!mesh2tangent->mloopuv) {
          mesh2tangent->orco = vert_orco;
          if (!mesh2tangent->orco) {
            continue;
          }

          tangent_mask_curr |= DM_TANGENT_MASK_ORCO;
        }
        else {
          int uv_ind = CustomData_get_named_layer_index(
              loopdata, CD_PROP_FLOAT2, loopdata_out->layers[index].name);
          int uv_start = CustomData_get_layer_index(loopdata, CD_PROP_FLOAT2);
          BLI_assert(uv_ind != -1 && uv_start != -1);
          BLI_assert(uv_ind - uv_start < MAX_MTFACE);
          tangent_mask_curr |= short(1 << (uv_ind - uv_start));
        }

        mesh2tangent->tangent = static_cast<float(*)[4]>(loopdata_out->layers[index].data);
        BLI_task_pool_push(task_pool, DM_calc_loop_tangents_thread, mesh2tangent, false, nullptr);
      }

      BLI_assert(tangent_mask_curr == tangent_mask);
      BLI_task_pool_work_and_wait(task_pool);
      BLI_task_pool_free(task_pool);
    }
    else {
      tangent_mask_curr = tangent_mask;
    }
#ifdef USE_TRI_DETECT_QUADS
    if (face_as_quad_map) {
      MEM_freeN(face_as_quad_map);
    }
#  undef USE_TRI_DETECT_QUADS

#endif

    *tangent_mask_curr_p = tangent_mask_curr;

    /* Update active layer index */
    int act_uv_index = (act_uv_n != -1) ?
                           CustomData_get_layer_index_n(loopdata, CD_PROP_FLOAT2, act_uv_n) :
                           -1;
    if (act_uv_index != -1) {
      int tan_index = CustomData_get_named_layer_index(
          loopdata, CD_TANGENT, loopdata->layers[act_uv_index].name);
      CustomData_set_layer_active_index(loopdata, CD_TANGENT, tan_index);
    } /* else tangent has been built from orco */

    /* Update render layer index */
    int ren_uv_index = (ren_uv_n != -1) ?
                           CustomData_get_layer_index_n(loopdata, CD_PROP_FLOAT2, ren_uv_n) :
                           -1;
    if (ren_uv_index != -1) {
      int tan_index = CustomData_get_named_layer_index(
          loopdata, CD_TANGENT, loopdata->layers[ren_uv_index].name);
      CustomData_set_layer_render_index(loopdata, CD_TANGENT, tan_index);
    } /* else tangent has been built from orco */
  }
}

void BKE_mesh_calc_loop_tangents(Mesh *me_eval,
                                 bool calc_active_tangent,
                                 const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                 int tangent_names_len)
{
  /* TODO(@ideasman42): store in Mesh.runtime to avoid recalculation. */
  using namespace blender;
  using namespace blender::bke;
  const blender::Span<int3> corner_tris = me_eval->corner_tris();
  const bke::AttributeAccessor attributes = me_eval->attributes();
  const VArraySpan sharp_face = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
  short tangent_mask = 0;
  BKE_mesh_calc_loop_tangent_ex(
      reinterpret_cast<const float(*)[3]>(me_eval->vert_positions().data()),
      me_eval->faces(),
      me_eval->corner_verts().data(),
      corner_tris.data(),
      me_eval->corner_tri_faces().data(),
      uint(corner_tris.size()),
      sharp_face,
      &me_eval->corner_data,
      calc_active_tangent,
      tangent_names,
      tangent_names_len,
      reinterpret_cast<const float(*)[3]>(me_eval->vert_normals().data()),
      reinterpret_cast<const float(*)[3]>(me_eval->face_normals().data()),
      reinterpret_cast<const float(*)[3]>(me_eval->corner_normals().data()),
      /* may be nullptr */
      static_cast<const float(*)[3]>(CustomData_get_layer(&me_eval->vert_data, CD_ORCO)),
      /* result */
      &me_eval->corner_data,
      uint(me_eval->corners_num),
      &tangent_mask);
}

/** \} */
