/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines_stack.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_mesh.hh"
#include "BKE_particle.h"

#include "MOD_modifiertypes.hh"
#include "MOD_solidify_util.hh" /* own include */
#include "MOD_util.hh"

/* -------------------------------------------------------------------- */
/** \name High Quality Normal Calculation Function
 * \{ */

/* skip shell thickness for non-manifold edges, see #35710. */
#define USE_NONMANIFOLD_WORKAROUND

/* *** derived mesh high quality normal calculation function  *** */
/* could be exposed for other functions to use */

struct EdgeFaceRef {
  int p1; /* init as -1 */
  int p2;
};

BLI_INLINE bool edgeref_is_init(const EdgeFaceRef *edge_ref)
{
  return !((edge_ref->p1 == 0) && (edge_ref->p2 == 0));
}

/**
 * \param mesh: Mesh to calculate normals for.
 * \param face_normals: Precalculated face normals.
 * \param r_vert_nors: Return vert normals.
 */
static void mesh_calc_hq_normal(Mesh *mesh,
                                const blender::Span<blender::float3> face_normals,
                                float (*r_vert_nors)[3],
#ifdef USE_NONMANIFOLD_WORKAROUND
                                BLI_bitmap *edge_tmp_tag
#endif
)
{
  const int verts_num = mesh->totvert;
  const blender::Span<blender::int2> edges = mesh->edges();
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_edges = mesh->corner_edges();

  {
    EdgeFaceRef *edge_ref_array = MEM_cnew_array<EdgeFaceRef>(size_t(edges.size()), __func__);
    EdgeFaceRef *edge_ref;
    float edge_normal[3];

    /* Add an edge reference if it's not there, pointing back to the face index. */
    for (const int i : faces.index_range()) {
      for (const int edge_i : corner_edges.slice(faces[i])) {
        /* --- add edge ref to face --- */
        edge_ref = &edge_ref_array[edge_i];
        if (!edgeref_is_init(edge_ref)) {
          edge_ref->p1 = i;
          edge_ref->p2 = -1;
        }
        else if ((edge_ref->p1 != -1) && (edge_ref->p2 == -1)) {
          edge_ref->p2 = i;
        }
        else {
          /* 3+ faces using an edge, we can't handle this usefully */
          edge_ref->p1 = edge_ref->p2 = -1;
#ifdef USE_NONMANIFOLD_WORKAROUND
          BLI_BITMAP_ENABLE(edge_tmp_tag, edge_i);
#endif
        }
        /* --- done --- */
      }
    }

    int i;
    const blender::int2 *edge;
    for (i = 0, edge = edges.data(), edge_ref = edge_ref_array; i < edges.size();
         i++, edge++, edge_ref++)
    {
      /* Get the edge vert indices, and edge value (the face indices that use it) */

      if (edgeref_is_init(edge_ref) && (edge_ref->p1 != -1)) {
        if (edge_ref->p2 != -1) {
          /* We have 2 faces using this edge, calculate the edges normal
           * using the angle between the 2 faces as a weighting */
#if 0
          add_v3_v3v3(edge_normal, face_nors[edge_ref->f1], face_nors[edge_ref->f2]);
          normalize_v3_length(
              edge_normal,
              angle_normalized_v3v3(face_nors[edge_ref->f1], face_nors[edge_ref->f2]));
#else
          mid_v3_v3v3_angle_weighted(
              edge_normal, face_normals[edge_ref->p1], face_normals[edge_ref->p2]);
#endif
        }
        else {
          /* only one face attached to that edge */
          /* an edge without another attached- the weight on this is undefined */
          copy_v3_v3(edge_normal, face_normals[edge_ref->p1]);
        }
        add_v3_v3(r_vert_nors[(*edge)[0]], edge_normal);
        add_v3_v3(r_vert_nors[(*edge)[1]], edge_normal);
      }
    }
    MEM_freeN(edge_ref_array);
  }

  /* normalize vertex normals and assign */
  const blender::Span<blender::float3> vert_normals = mesh->vert_normals();
  for (int i = 0; i < verts_num; i++) {
    if (normalize_v3(r_vert_nors[i]) == 0.0f) {
      copy_v3_v3(r_vert_nors[i], vert_normals[i]);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Solidify Function
 * \{ */

/* NOLINTNEXTLINE: readability-function-size */
Mesh *MOD_solidify_extrude_modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;

  const uint verts_num = uint(mesh->totvert);
  const uint edges_num = uint(mesh->totedge);
  const uint faces_num = uint(mesh->faces_num);
  const uint loops_num = uint(mesh->totloop);
  uint newLoops = 0, newPolys = 0, newEdges = 0, newVerts = 0, rimVerts = 0;

  /* Only use material offsets if we have 2 or more materials. */
  const short mat_nr_max = ctx->object->totcol > 1 ? ctx->object->totcol - 1 : 0;
  const short mat_ofs = mat_nr_max ? smd->mat_ofs : 0;
  const short mat_ofs_rim = mat_nr_max ? smd->mat_ofs_rim : 0;

  /* use for edges */
  /* Over-allocate new_vert_arr, old_vert_arr. */
  uint *new_vert_arr = nullptr;
  STACK_DECLARE(new_vert_arr);

  uint *new_edge_arr = nullptr;
  STACK_DECLARE(new_edge_arr);

  uint *old_vert_arr = MEM_cnew_array<uint>(verts_num, "old_vert_arr in solidify");

  uint *edge_users = nullptr;
  int *edge_order = nullptr;

  float(*vert_nors)[3] = nullptr;
  blender::Span<blender::float3> face_normals;

  const bool need_face_normals = (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) ||
                                 (smd->flag & MOD_SOLIDIFY_EVEN) ||
                                 (smd->flag & MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP) ||
                                 (smd->bevel_convex != 0);

  const float ofs_orig = -(((-smd->offset_fac + 1.0f) * 0.5f) * smd->offset);
  const float ofs_new = smd->offset + ofs_orig;
  const float offset_fac_vg = smd->offset_fac_vg;
  const float offset_fac_vg_inv = 1.0f - smd->offset_fac_vg;
  const float bevel_convex = smd->bevel_convex;
  const bool do_flip = (smd->flag & MOD_SOLIDIFY_FLIP) != 0;
  const bool do_clamp = (smd->offset_clamp != 0.0f);
  const bool do_angle_clamp = do_clamp && (smd->flag & MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP) != 0;
  const bool do_bevel_convex = bevel_convex != 0.0f;
  const bool do_rim = (smd->flag & MOD_SOLIDIFY_RIM) != 0;
  const bool do_shell = !(do_rim && (smd->flag & MOD_SOLIDIFY_NOSHELL) != 0);

  /* weights */
  const MDeformVert *dvert;
  const bool defgrp_invert = (smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0;
  int defgrp_index;
  const int shell_defgrp_index = BKE_id_defgroup_name_index(&mesh->id, smd->shell_defgrp_name);
  const int rim_defgrp_index = BKE_id_defgroup_name_index(&mesh->id, smd->rim_defgrp_name);

  /* array size is doubled in case of using a shell */
  const uint stride = do_shell ? 2 : 1;

  const blender::Span<blender::float3> vert_normals = mesh->vert_normals();

  MOD_get_vgroup(ctx->object, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  const blender::Span<blender::float3> orig_vert_positions = mesh->vert_positions();
  const blender::Span<blender::int2> orig_edges = mesh->edges();
  const blender::OffsetIndices orig_faces = mesh->faces();
  const blender::Span<int> orig_corner_verts = mesh->corner_verts();
  const blender::Span<int> orig_corner_edges = mesh->corner_edges();

  if (need_face_normals) {
    /* calculate only face normals */
    face_normals = mesh->face_normals();
  }

  STACK_INIT(new_vert_arr, verts_num * 2);
  STACK_INIT(new_edge_arr, edges_num * 2);

  if (do_rim) {
    BLI_bitmap *orig_mvert_tag = BLI_BITMAP_NEW(verts_num, __func__);
    uint eidx;
    uint i;

#define INVALID_UNUSED uint(-1)
#define INVALID_PAIR uint(-2)

    new_vert_arr = static_cast<uint *>(
        MEM_malloc_arrayN(verts_num, 2 * sizeof(*new_vert_arr), __func__));
    new_edge_arr = static_cast<uint *>(
        MEM_malloc_arrayN(((edges_num * 2) + verts_num), sizeof(*new_edge_arr), __func__));

    edge_users = static_cast<uint *>(MEM_malloc_arrayN(edges_num, sizeof(*edge_users), __func__));
    edge_order = static_cast<int *>(MEM_malloc_arrayN(edges_num, sizeof(*edge_order), __func__));

    /* save doing 2 loops here... */
#if 0
    copy_vn_i(edge_users, edges_num, INVALID_UNUSED);
#endif

    for (eidx = 0; eidx < edges_num; eidx++) {
      edge_users[eidx] = INVALID_UNUSED;
    }

    for (const int64_t i : orig_faces.index_range()) {
      const blender::IndexRange face = orig_faces[i];
      int j;

      int corner_i_prev = face.last();

      for (j = 0; j < face.size(); j++) {
        const int corner_i = face[j];
        const int vert_i = orig_corner_verts[corner_i];
        const int prev_vert_i = orig_corner_verts[corner_i_prev];
        /* add edge user */
        eidx = int(orig_corner_edges[corner_i_prev]);
        if (edge_users[eidx] == INVALID_UNUSED) {
          const blender::int2 &edge = orig_edges[eidx];
          BLI_assert(ELEM(prev_vert_i, edge[0], edge[1]) && ELEM(vert_i, edge[0], edge[1]));
          edge_users[eidx] = (prev_vert_i > vert_i) == (edge[0] < edge[1]) ? uint(i) :
                                                                             (uint(i) + faces_num);
          edge_order[eidx] = j;
        }
        else {
          edge_users[eidx] = INVALID_PAIR;
        }
        corner_i_prev = corner_i;
      }
    }

    for (eidx = 0; eidx < edges_num; eidx++) {
      if (!ELEM(edge_users[eidx], INVALID_UNUSED, INVALID_PAIR)) {
        BLI_BITMAP_ENABLE(orig_mvert_tag, orig_edges[eidx][0]);
        BLI_BITMAP_ENABLE(orig_mvert_tag, orig_edges[eidx][1]);
        STACK_PUSH(new_edge_arr, eidx);
        newPolys++;
        newLoops += 4;
      }
    }

    for (i = 0; i < verts_num; i++) {
      if (BLI_BITMAP_TEST(orig_mvert_tag, i)) {
        old_vert_arr[i] = STACK_SIZE(new_vert_arr);
        STACK_PUSH(new_vert_arr, i);
        rimVerts++;
      }
      else {
        old_vert_arr[i] = INVALID_UNUSED;
      }
    }

    MEM_freeN(orig_mvert_tag);
  }

  if (do_shell == false) {
    /* only add rim vertices */
    newVerts = rimVerts;
    /* each extruded face needs an opposite edge */
    newEdges = newPolys;
  }
  else {
    /* (stride == 2) in this case, so no need to add newVerts/newEdges */
    BLI_assert(newVerts == 0);
    BLI_assert(newEdges == 0);
  }

#ifdef USE_NONMANIFOLD_WORKAROUND
  BLI_bitmap *edge_tmp_tag = BLI_BITMAP_NEW(mesh->totedge, __func__);
#endif

  if (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) {
    vert_nors = static_cast<float(*)[3]>(MEM_calloc_arrayN(verts_num, sizeof(float[3]), __func__));
    mesh_calc_hq_normal(mesh,
                        face_normals,
                        vert_nors
#ifdef USE_NONMANIFOLD_WORKAROUND
                        ,
                        edge_tmp_tag
#endif
    );
  }

  result = BKE_mesh_new_nomain_from_template(mesh,
                                             int((verts_num * stride) + newVerts),
                                             int((edges_num * stride) + newEdges + rimVerts),
                                             int((faces_num * stride) + newPolys),
                                             int((loops_num * stride) + newLoops));

  blender::MutableSpan<blender::float3> vert_positions = result->vert_positions_for_write();
  blender::MutableSpan<blender::int2> edges = result->edges_for_write();
  blender::MutableSpan<int> face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> corner_edges = result->corner_edges_for_write();

  if (do_shell) {
    CustomData_copy_data(&mesh->vert_data, &result->vert_data, 0, 0, int(verts_num));
    CustomData_copy_data(&mesh->vert_data, &result->vert_data, 0, int(verts_num), int(verts_num));

    CustomData_copy_data(&mesh->edge_data, &result->edge_data, 0, 0, int(edges_num));
    CustomData_copy_data(&mesh->edge_data, &result->edge_data, 0, int(edges_num), int(edges_num));

    CustomData_copy_data(&mesh->loop_data, &result->loop_data, 0, 0, int(loops_num));
    /* DO NOT copy here the 'copied' part of loop data, we want to reverse loops
     * (so that winding of copied face get reversed, so that normals get reversed
     * and point in expected direction...).
     * If we also copy data here, then this data get overwritten
     * (and allocated memory becomes a memory leak). */

    CustomData_copy_data(&mesh->face_data, &result->face_data, 0, 0, int(faces_num));
    CustomData_copy_data(&mesh->face_data, &result->face_data, 0, int(faces_num), int(faces_num));
    face_offsets.take_front(faces_num).copy_from(mesh->face_offsets().drop_back(1));
    for (const int i : orig_faces.index_range()) {
      face_offsets[faces_num + i] = orig_faces[i].start() + mesh->totloop;
    }
  }
  else {
    int i, j;
    CustomData_copy_data(&mesh->vert_data, &result->vert_data, 0, 0, int(verts_num));
    for (i = 0, j = int(verts_num); i < verts_num; i++) {
      if (old_vert_arr[i] != INVALID_UNUSED) {
        CustomData_copy_data(&mesh->vert_data, &result->vert_data, i, j, 1);
        j++;
      }
    }

    CustomData_copy_data(&mesh->edge_data, &result->edge_data, 0, 0, int(edges_num));

    for (i = 0, j = int(edges_num); i < edges_num; i++) {
      if (!ELEM(edge_users[i], INVALID_UNUSED, INVALID_PAIR)) {
        blender::int2 *ed_src, *ed_dst;
        CustomData_copy_data(&mesh->edge_data, &result->edge_data, i, j, 1);

        ed_src = &edges[i];
        ed_dst = &edges[j];
        (*ed_dst)[0] = old_vert_arr[(*ed_src)[0]] + verts_num;
        (*ed_dst)[1] = old_vert_arr[(*ed_src)[1]] + verts_num;
        j++;
      }
    }

    /* will be created later */
    CustomData_copy_data(&mesh->loop_data, &result->loop_data, 0, 0, int(loops_num));
    CustomData_copy_data(&mesh->face_data, &result->face_data, 0, 0, int(faces_num));
    face_offsets.take_front(faces_num).copy_from(mesh->face_offsets().drop_back(1));
  }

  float *result_edge_bweight = nullptr;
  if (do_bevel_convex) {
    result_edge_bweight = static_cast<float *>(CustomData_add_layer_named(
        &result->edge_data, CD_PROP_FLOAT, CD_SET_DEFAULT, result->totedge, "bevel_weight_edge"));
  }

  /* Initializes: (`i_end`, `do_shell_align`, `vert_index`). */
#define INIT_VERT_ARRAY_OFFSETS(test) \
  if (((ofs_new >= ofs_orig) == do_flip) == test) { \
    i_end = verts_num; \
    do_shell_align = true; \
    vert_index = 0; \
  } \
  else { \
    if (do_shell) { \
      i_end = verts_num; \
      do_shell_align = true; \
    } \
    else { \
      i_end = newVerts; \
      do_shell_align = false; \
    } \
    vert_index = verts_num; \
  } \
  (void)0

  int *dst_material_index = BKE_mesh_material_indices_for_write(result);

  /* flip normals */

  if (do_shell) {
    for (const int64_t i : blender::IndexRange(mesh->faces_num)) {
      const blender::IndexRange face = orig_faces[i];
      const int loop_end = face.size() - 1;
      int e;
      int j;

      /* reverses the loop direction (corner verts as well as custom-data)
       * Corner edges also need to be corrected too, done in a separate loop below. */
      const int corner_2 = face.start() + mesh->totloop;
#if 0
      for (j = 0; j < face.size(); j++) {
        CustomData_copy_data(&mesh->ldata,
                             &result->ldata,
                             face.start() + j,
                             face.start() + (loop_end - j) + mesh->totloop,
                             1);
      }
#else
      /* slightly more involved, keep the first vertex the same for the copy,
       * ensures the diagonals in the new face match the original. */
      j = 0;
      for (int j_prev = loop_end; j < face.size(); j_prev = j++) {
        CustomData_copy_data(&mesh->loop_data,
                             &result->loop_data,
                             face.start() + j,
                             face.start() + (loop_end - j_prev) + mesh->totloop,
                             1);
      }
#endif

      if (mat_ofs) {
        dst_material_index[faces_num + i] += mat_ofs;
        CLAMP(dst_material_index[faces_num + i], 0, mat_nr_max);
      }

      e = corner_edges[corner_2 + 0];
      for (j = 0; j < loop_end; j++) {
        corner_edges[corner_2 + j] = corner_edges[corner_2 + j + 1];
      }
      corner_edges[corner_2 + loop_end] = e;

      for (j = 0; j < face.size(); j++) {
        corner_verts[corner_2 + j] += verts_num;
        corner_edges[corner_2 + j] += edges_num;
      }
    }

    for (blender::int2 &edge : edges.slice(edges_num, edges_num)) {
      edge += verts_num;
    }
  }

  /* NOTE: copied vertex layers don't have flipped normals yet. do this after applying offset. */
  if ((smd->flag & MOD_SOLIDIFY_EVEN) == 0) {
    /* no even thickness, very simple */
    float ofs_new_vgroup;

    /* for clamping */
    float *vert_lens = nullptr;
    float *vert_angs = nullptr;
    const float offset = fabsf(smd->offset) * smd->offset_clamp;
    const float offset_sq = offset * offset;

    /* for bevel weight */
    float *edge_angs = nullptr;

    if (do_clamp) {
      vert_lens = static_cast<float *>(MEM_malloc_arrayN(verts_num, sizeof(float), "vert_lens"));
      copy_vn_fl(vert_lens, int(verts_num), FLT_MAX);
      for (uint i = 0; i < edges_num; i++) {
        const float ed_len_sq = len_squared_v3v3(vert_positions[edges[i][0]],
                                                 vert_positions[edges[i][1]]);
        vert_lens[edges[i][0]] = min_ff(vert_lens[edges[i][0]], ed_len_sq);
        vert_lens[edges[i][1]] = min_ff(vert_lens[edges[i][1]], ed_len_sq);
      }
    }

    if (do_angle_clamp || do_bevel_convex) {
      uint eidx;
      if (do_angle_clamp) {
        vert_angs = static_cast<float *>(MEM_malloc_arrayN(verts_num, sizeof(float), "vert_angs"));
        copy_vn_fl(vert_angs, int(verts_num), 0.5f * M_PI);
      }
      if (do_bevel_convex) {
        edge_angs = static_cast<float *>(MEM_malloc_arrayN(edges_num, sizeof(float), "edge_angs"));
        if (!do_rim) {
          edge_users = static_cast<uint *>(
              MEM_malloc_arrayN(edges_num, sizeof(*edge_users), "solid_mod edges"));
        }
      }
      uint(*edge_user_pairs)[2] = static_cast<uint(*)[2]>(
          MEM_malloc_arrayN(edges_num, sizeof(*edge_user_pairs), "edge_user_pairs"));
      for (eidx = 0; eidx < edges_num; eidx++) {
        edge_user_pairs[eidx][0] = INVALID_UNUSED;
        edge_user_pairs[eidx][1] = INVALID_UNUSED;
      }
      for (const int64_t i : orig_faces.index_range()) {
        const blender::IndexRange face = orig_faces[i];
        int prev_corner_i = face.last();
        for (const int corner_i : face) {
          const int vert_i = orig_corner_verts[corner_i];
          const int prev_vert_i = orig_corner_verts[prev_corner_i];
          /* add edge user */
          eidx = orig_corner_edges[prev_corner_i];
          const blender::int2 &ed = orig_edges[eidx];
          BLI_assert(ELEM(prev_vert_i, ed[0], ed[1]) && ELEM(vert_i, ed[0], ed[1]));
          char flip = char((prev_vert_i > vert_i) == (ed[0] < ed[1]));
          if (edge_user_pairs[eidx][flip] == INVALID_UNUSED) {
            edge_user_pairs[eidx][flip] = uint(i);
          }
          else {
            edge_user_pairs[eidx][0] = INVALID_PAIR;
            edge_user_pairs[eidx][1] = INVALID_PAIR;
          }
          prev_corner_i = corner_i;
        }
      }
      float e[3];
      for (uint i = 0; i < edges_num; i++) {
        const blender::int2 &edge = orig_edges[i];
        if (!ELEM(edge_user_pairs[i][0], INVALID_UNUSED, INVALID_PAIR) &&
            !ELEM(edge_user_pairs[i][1], INVALID_UNUSED, INVALID_PAIR))
        {
          const float *n0 = face_normals[edge_user_pairs[i][0]];
          const float *n1 = face_normals[edge_user_pairs[i][1]];
          sub_v3_v3v3(e, orig_vert_positions[edge[0]], orig_vert_positions[edge[1]]);
          normalize_v3(e);
          const float angle = angle_signed_on_axis_v3v3_v3(n0, n1, e);
          if (do_angle_clamp) {
            vert_angs[edge[0]] = max_ff(vert_angs[edge[0]], angle);
            vert_angs[edge[1]] = max_ff(vert_angs[edge[1]], angle);
          }
          if (do_bevel_convex) {
            edge_angs[i] = angle;
            if (!do_rim) {
              edge_users[i] = INVALID_PAIR;
            }
          }
        }
      }
      MEM_freeN(edge_user_pairs);
    }

    if (ofs_new != 0.0f) {
      uint i_orig, i_end;
      bool do_shell_align;

      ofs_new_vgroup = ofs_new;

      uint vert_index;
      INIT_VERT_ARRAY_OFFSETS(false);

      for (i_orig = 0; i_orig < i_end; i_orig++, vert_index++) {
        const uint i = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (dvert) {
          const MDeformVert *dv = &dvert[i];
          if (defgrp_invert) {
            ofs_new_vgroup = 1.0f - BKE_defvert_find_weight(dv, defgrp_index);
          }
          else {
            ofs_new_vgroup = BKE_defvert_find_weight(dv, defgrp_index);
          }
          ofs_new_vgroup = (offset_fac_vg + (ofs_new_vgroup * offset_fac_vg_inv)) * ofs_new;
        }
        if (do_clamp && offset > FLT_EPSILON) {
          /* always reset because we may have set before */
          if (dvert == nullptr) {
            ofs_new_vgroup = ofs_new;
          }
          if (do_angle_clamp) {
            float cos_ang = cosf(((2 * M_PI) - vert_angs[i]) * 0.5f);
            if (cos_ang > 0) {
              float max_off = sqrtf(vert_lens[i]) * 0.5f / cos_ang;
              if (max_off < offset * 0.5f) {
                ofs_new_vgroup *= max_off / offset * 2;
              }
            }
          }
          else {
            if (vert_lens[i] < offset_sq) {
              float scalar = sqrtf(vert_lens[i]) / offset;
              ofs_new_vgroup *= scalar;
            }
          }
        }
        if (vert_nors) {
          madd_v3_v3fl(vert_positions[vert_index], vert_nors[i], ofs_new_vgroup);
        }
        else {
          madd_v3_v3fl(vert_positions[vert_index], vert_normals[i], ofs_new_vgroup);
        }
      }
    }

    if (ofs_orig != 0.0f) {
      uint i_orig, i_end;
      bool do_shell_align;

      ofs_new_vgroup = ofs_orig;

      /* as above but swapped */
      uint vert_index;
      INIT_VERT_ARRAY_OFFSETS(true);

      for (i_orig = 0; i_orig < i_end; i_orig++, vert_index++) {
        const uint i = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (dvert) {
          const MDeformVert *dv = &dvert[i];
          if (defgrp_invert) {
            ofs_new_vgroup = 1.0f - BKE_defvert_find_weight(dv, defgrp_index);
          }
          else {
            ofs_new_vgroup = BKE_defvert_find_weight(dv, defgrp_index);
          }
          ofs_new_vgroup = (offset_fac_vg + (ofs_new_vgroup * offset_fac_vg_inv)) * ofs_orig;
        }
        if (do_clamp && offset > FLT_EPSILON) {
          /* always reset because we may have set before */
          if (dvert == nullptr) {
            ofs_new_vgroup = ofs_orig;
          }
          if (do_angle_clamp) {
            float cos_ang = cosf(vert_angs[i_orig] * 0.5f);
            if (cos_ang > 0) {
              float max_off = sqrtf(vert_lens[i]) * 0.5f / cos_ang;
              if (max_off < offset * 0.5f) {
                ofs_new_vgroup *= max_off / offset * 2;
              }
            }
          }
          else {
            if (vert_lens[i] < offset_sq) {
              float scalar = sqrtf(vert_lens[i]) / offset;
              ofs_new_vgroup *= scalar;
            }
          }
        }
        if (vert_nors) {
          madd_v3_v3fl(vert_positions[vert_index], vert_nors[i], ofs_new_vgroup);
        }
        else {
          madd_v3_v3fl(vert_positions[vert_index], vert_normals[i], ofs_new_vgroup);
        }
      }
    }

    if (do_bevel_convex) {
      for (uint i = 0; i < edges_num; i++) {
        if (edge_users[i] == INVALID_PAIR) {
          float angle = edge_angs[i];
          result_edge_bweight[i] = clamp_f(result_edge_bweight[i] +
                                               (angle < M_PI ? clamp_f(bevel_convex, 0.0f, 1.0f) :
                                                               clamp_f(bevel_convex, -1.0f, 0.0f)),
                                           0.0f,
                                           1.0f);
          if (do_shell) {
            result_edge_bweight[i + edges_num] = clamp_f(
                result_edge_bweight[i + edges_num] + (angle > M_PI ?
                                                          clamp_f(bevel_convex, 0.0f, 1.0f) :
                                                          clamp_f(bevel_convex, -1.0f, 0.0f)),
                0,
                1.0f);
          }
        }
      }
      if (!do_rim) {
        MEM_freeN(edge_users);
      }
      MEM_freeN(edge_angs);
    }

    if (do_clamp) {
      MEM_freeN(vert_lens);
      if (do_angle_clamp) {
        MEM_freeN(vert_angs);
      }
    }
  }
  else {
#ifdef USE_NONMANIFOLD_WORKAROUND
    const bool check_non_manifold = (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) != 0;
#endif
    /* same as EM_solidify() in editmesh_lib.c */
    float *vert_angles = static_cast<float *>(
        MEM_calloc_arrayN(verts_num, sizeof(float[2]), "mod_solid_pair")); /* 2 in 1 */
    float *vert_accum = vert_angles + verts_num;
    uint vidx;
    uint i;

    if (vert_nors == nullptr) {
      vert_nors = static_cast<float(*)[3]>(
          MEM_malloc_arrayN(verts_num, sizeof(float[3]), "mod_solid_vno"));
      for (i = 0; i < verts_num; i++) {
        copy_v3_v3(vert_nors[i], vert_normals[i]);
      }
    }

    for (const int64_t i : blender::IndexRange(faces_num)) {
      const blender::IndexRange face = orig_faces[i];
      /* #bke::mesh::face_angles_calc logic is inlined here */
      float nor_prev[3];
      float nor_next[3];

      int i_curr = face.size() - 1;
      int i_next = 0;

      const int *face_verts = &corner_verts[face.start()];
      const int *face_edges = &corner_edges[face.start()];

      sub_v3_v3v3(
          nor_prev, vert_positions[face_verts[i_curr - 1]], vert_positions[face_verts[i_curr]]);
      normalize_v3(nor_prev);

      while (i_next < face.size()) {
        float angle;
        sub_v3_v3v3(
            nor_next, vert_positions[face_verts[i_curr]], vert_positions[face_verts[i_next]]);
        normalize_v3(nor_next);
        angle = angle_normalized_v3v3(nor_prev, nor_next);

        /* --- not related to angle calc --- */
        if (angle < FLT_EPSILON) {
          angle = FLT_EPSILON;
        }

        vidx = face_verts[i_curr];
        vert_accum[vidx] += angle;

#ifdef USE_NONMANIFOLD_WORKAROUND
        /* skip 3+ face user edges */
        if ((check_non_manifold == false) ||
            LIKELY(!BLI_BITMAP_TEST(edge_tmp_tag, face_edges[i_curr]) &&
                   !BLI_BITMAP_TEST(edge_tmp_tag, face_edges[i_next])))
        {
          vert_angles[vidx] += shell_v3v3_normalized_to_dist(vert_nors[vidx], face_normals[i]) *
                               angle;
        }
        else {
          vert_angles[vidx] += angle;
        }
#else
        vert_angles[vidx] += shell_v3v3_normalized_to_dist(vert_nors[vidx], face_normals[i]) *
                             angle;
#endif
        /* --- end non-angle-calc section --- */

        /* step */
        copy_v3_v3(nor_prev, nor_next);
        i_curr = i_next;
        i_next++;
      }
    }

    /* vertex group support */
    if (dvert) {
      const MDeformVert *dv = dvert;
      float scalar;

      if (defgrp_invert) {
        for (i = 0; i < verts_num; i++, dv++) {
          scalar = 1.0f - BKE_defvert_find_weight(dv, defgrp_index);
          scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
          vert_angles[i] *= scalar;
        }
      }
      else {
        for (i = 0; i < verts_num; i++, dv++) {
          scalar = BKE_defvert_find_weight(dv, defgrp_index);
          scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
          vert_angles[i] *= scalar;
        }
      }
    }

    /* for angle clamp */
    float *vert_angs = nullptr;
    /* for bevel convex */
    float *edge_angs = nullptr;

    if (do_angle_clamp || do_bevel_convex) {
      uint eidx;
      if (do_angle_clamp) {
        vert_angs = static_cast<float *>(
            MEM_malloc_arrayN(verts_num, sizeof(float), "vert_angs even"));
        copy_vn_fl(vert_angs, int(verts_num), 0.5f * M_PI);
      }
      if (do_bevel_convex) {
        edge_angs = static_cast<float *>(
            MEM_malloc_arrayN(edges_num, sizeof(float), "edge_angs even"));
        if (!do_rim) {
          edge_users = static_cast<uint *>(
              MEM_malloc_arrayN(edges_num, sizeof(*edge_users), "solid_mod edges"));
        }
      }
      uint(*edge_user_pairs)[2] = static_cast<uint(*)[2]>(
          MEM_malloc_arrayN(edges_num, sizeof(*edge_user_pairs), "edge_user_pairs"));
      for (eidx = 0; eidx < edges_num; eidx++) {
        edge_user_pairs[eidx][0] = INVALID_UNUSED;
        edge_user_pairs[eidx][1] = INVALID_UNUSED;
      }
      for (const int i : orig_faces.index_range()) {
        const blender::IndexRange face = orig_faces[i];
        int prev_corner_i = face.start() + face.size() - 1;
        for (int j = 0; j < face.size(); j++) {
          const int corner_i = face.start() + j;
          const int vert_i = orig_corner_verts[corner_i];
          const int prev_vert_i = orig_corner_verts[prev_corner_i];

          /* add edge user */
          eidx = orig_corner_edges[prev_corner_i];
          const blender::int2 &edge = orig_edges[eidx];
          BLI_assert(ELEM(prev_vert_i, edge[0], edge[1]) && ELEM(vert_i, edge[0], edge[1]));
          char flip = char((prev_vert_i > vert_i) == (edge[0] < edge[1]));
          if (edge_user_pairs[eidx][flip] == INVALID_UNUSED) {
            edge_user_pairs[eidx][flip] = uint(i);
          }
          else {
            edge_user_pairs[eidx][0] = INVALID_PAIR;
            edge_user_pairs[eidx][1] = INVALID_PAIR;
          }
          prev_corner_i = corner_i;
        }
      }
      float e[3];
      for (i = 0; i < edges_num; i++) {
        const blender::int2 &edge = orig_edges[i];
        if (!ELEM(edge_user_pairs[i][0], INVALID_UNUSED, INVALID_PAIR) &&
            !ELEM(edge_user_pairs[i][1], INVALID_UNUSED, INVALID_PAIR))
        {
          const float *n0 = face_normals[edge_user_pairs[i][0]];
          const float *n1 = face_normals[edge_user_pairs[i][1]];
          if (do_angle_clamp) {
            const float angle = M_PI - angle_normalized_v3v3(n0, n1);
            vert_angs[edge[0]] = max_ff(vert_angs[edge[0]], angle);
            vert_angs[edge[1]] = max_ff(vert_angs[edge[1]], angle);
          }
          if (do_bevel_convex) {
            sub_v3_v3v3(e, orig_vert_positions[edge[0]], orig_vert_positions[edge[1]]);
            normalize_v3(e);
            edge_angs[i] = angle_signed_on_axis_v3v3_v3(n0, n1, e);
            if (!do_rim) {
              edge_users[i] = INVALID_PAIR;
            }
          }
        }
      }
      MEM_freeN(edge_user_pairs);
    }

    if (do_clamp) {
      const float clamp_fac = 1 + (do_angle_clamp ? fabsf(smd->offset_fac) : 0);
      const float offset = fabsf(smd->offset) * smd->offset_clamp * clamp_fac;
      if (offset > FLT_EPSILON) {
        float *vert_lens_sq = static_cast<float *>(
            MEM_malloc_arrayN(verts_num, sizeof(float), "vert_lens_sq"));
        const float offset_sq = offset * offset;
        copy_vn_fl(vert_lens_sq, int(verts_num), FLT_MAX);
        for (i = 0; i < edges_num; i++) {
          const float ed_len = len_squared_v3v3(vert_positions[edges[i][0]],
                                                vert_positions[edges[i][1]]);
          vert_lens_sq[edges[i][0]] = min_ff(vert_lens_sq[edges[i][0]], ed_len);
          vert_lens_sq[edges[i][1]] = min_ff(vert_lens_sq[edges[i][1]], ed_len);
        }
        if (do_angle_clamp) {
          for (i = 0; i < verts_num; i++) {
            float cos_ang = cosf(vert_angs[i] * 0.5f);
            if (cos_ang > 0) {
              float max_off = sqrtf(vert_lens_sq[i]) * 0.5f / cos_ang;
              if (max_off < offset * 0.5f) {
                vert_angles[i] *= max_off / offset * 2;
              }
            }
          }
          MEM_freeN(vert_angs);
        }
        else {
          for (i = 0; i < verts_num; i++) {
            if (vert_lens_sq[i] < offset_sq) {
              float scalar = sqrtf(vert_lens_sq[i]) / offset;
              vert_angles[i] *= scalar;
            }
          }
        }
        MEM_freeN(vert_lens_sq);
      }
    }

    if (do_bevel_convex) {
      for (i = 0; i < edges_num; i++) {
        if (edge_users[i] == INVALID_PAIR) {
          float angle = edge_angs[i];
          result_edge_bweight[i] = clamp_f(result_edge_bweight[i] +
                                               (angle < M_PI ? clamp_f(bevel_convex, 0.0f, 1.0f) :
                                                               clamp_f(bevel_convex, -1.0f, 0.0f)),
                                           0.0f,
                                           1.0f);
          if (do_shell) {
            result_edge_bweight[i + edges_num] = clamp_f(
                result_edge_bweight[i + edges_num] +
                    (angle > M_PI ? clamp_f(bevel_convex, 0, 1) : clamp_f(bevel_convex, -1, 0)),
                0.0f,
                1.0f);
          }
        }
      }
      if (!do_rim) {
        MEM_freeN(edge_users);
      }
      MEM_freeN(edge_angs);
    }

#undef INVALID_UNUSED
#undef INVALID_PAIR

    if (ofs_new != 0.0f) {
      uint i_orig, i_end;
      bool do_shell_align;

      uint vert_index;
      INIT_VERT_ARRAY_OFFSETS(false);

      for (i_orig = 0; i_orig < i_end; i_orig++, vert_index++) {
        const uint i_other = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (vert_accum[i_other]) { /* zero if unselected */
          madd_v3_v3fl(vert_positions[vert_index],
                       vert_nors[i_other],
                       ofs_new * (vert_angles[i_other] / vert_accum[i_other]));
        }
      }
    }

    if (ofs_orig != 0.0f) {
      uint i_orig, i_end;
      bool do_shell_align;

      /* same as above but swapped, intentional use of 'ofs_new' */
      uint vert_index;
      INIT_VERT_ARRAY_OFFSETS(true);

      for (i_orig = 0; i_orig < i_end; i_orig++, vert_index++) {
        const uint i_other = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (vert_accum[i_other]) { /* zero if unselected */
          madd_v3_v3fl(vert_positions[vert_index],
                       vert_nors[i_other],
                       ofs_orig * (vert_angles[i_other] / vert_accum[i_other]));
        }
      }
    }

    MEM_freeN(vert_angles);
  }

#ifdef USE_NONMANIFOLD_WORKAROUND
  MEM_SAFE_FREE(edge_tmp_tag);
#endif

  if (vert_nors) {
    MEM_freeN(vert_nors);
  }

  /* must recalculate normals with vgroups since they can displace unevenly #26888. */
  if (BKE_mesh_vert_normals_are_dirty(mesh) || do_rim || dvert) {
    /* Pass. */
  }
  else if (do_shell) {
    uint i;
    /* flip vertex normals for copied verts */
    for (i = 0; i < verts_num; i++) {
      negate_v3((float *)&vert_normals[i].x);
    }
  }

  /* Add vertex weights for rim and shell vgroups. */
  if (shell_defgrp_index != -1 || rim_defgrp_index != -1) {
    MDeformVert *dst_dvert = BKE_mesh_deform_verts_for_write(result);

    /* Ultimate security check. */
    if (dst_dvert != nullptr) {

      if (rim_defgrp_index != -1) {
        for (uint i = 0; i < rimVerts; i++) {
          BKE_defvert_ensure_index(&dst_dvert[new_vert_arr[i]], rim_defgrp_index)->weight = 1.0f;
          BKE_defvert_ensure_index(&dst_dvert[(do_shell ? new_vert_arr[i] : i) + verts_num],
                                   rim_defgrp_index)
              ->weight = 1.0f;
        }
      }

      if (shell_defgrp_index != -1) {
        for (uint i = verts_num; i < result->totvert; i++) {
          BKE_defvert_ensure_index(&dst_dvert[i], shell_defgrp_index)->weight = 1.0f;
        }
      }
    }
  }
  if (do_rim) {
    uint i;

    /* NOTE(@ideasman42): Unfortunately re-calculate the normals for the new edge
     * faces is necessary. This could be done in many ways, but probably the quickest
     * way is to calculate the average normals for side faces only.
     * Then blend them with the normals of the edge verts.
     *
     * At the moment its easiest to allocate an entire array for every vertex,
     * even though we only need edge verts. */

#define SOLIDIFY_SIDE_NORMALS

#ifdef SOLIDIFY_SIDE_NORMALS
    /* NOTE(@sybren): due to the code setting normals dirty a few lines above,
     * do_side_normals is always false. */
    const bool do_side_normals = !BKE_mesh_vert_normals_are_dirty(result);
    /* annoying to allocate these since we only need the edge verts, */
    float(*edge_vert_nos)[3] = do_side_normals ? static_cast<float(*)[3]>(MEM_calloc_arrayN(
                                                     verts_num, sizeof(float[3]), __func__)) :
                                                 nullptr;
    float nor[3];
#endif
    const float crease_rim = smd->crease_rim;
    const float crease_outer = smd->crease_outer;
    const float crease_inner = smd->crease_inner;

    int *origindex_edge;
    int *orig_ed;
    uint j;

    float *result_edge_crease = nullptr;
    if (crease_rim || crease_outer || crease_inner) {
      result_edge_crease = (float *)CustomData_add_layer_named(
          &result->edge_data, CD_PROP_FLOAT, CD_SET_DEFAULT, result->totedge, "crease_edge");
    }

    /* add faces & edges */
    origindex_edge = static_cast<int *>(
        CustomData_get_layer_for_write(&result->edge_data, CD_ORIGINDEX, result->totedge));
    orig_ed = (origindex_edge) ? &origindex_edge[(edges_num * stride) + newEdges] : nullptr;
    /* Start after copied edges. */
    int new_edge_index = int(edges_num * stride + newEdges);
    for (i = 0; i < rimVerts; i++) {
      edges[new_edge_index][0] = new_vert_arr[i];
      edges[new_edge_index][1] = (do_shell ? new_vert_arr[i] : i) + verts_num;

      if (orig_ed) {
        *orig_ed = ORIGINDEX_NONE;
        orig_ed++;
      }

      if (crease_rim) {
        result_edge_crease[new_edge_index] = crease_rim;
      }
      new_edge_index++;
    }

    /* faces */
    int new_face_index = int(faces_num * stride);
    blender::MutableSpan<int> new_corner_verts = corner_verts.drop_front(loops_num * stride);
    blender::MutableSpan<int> new_corner_edges = corner_edges.drop_front(loops_num * stride);
    j = 0;
    for (i = 0; i < newPolys; i++) {
      uint eidx = new_edge_arr[i];
      uint pidx = edge_users[eidx];
      int k1, k2;
      bool flip;

      if (pidx >= faces_num) {
        pidx -= faces_num;
        flip = true;
      }
      else {
        flip = false;
      }

      const blender::int2 &edge = edges[eidx];

      /* copy most of the face settings */
      CustomData_copy_data(
          &mesh->face_data, &result->face_data, int(pidx), int((faces_num * stride) + i), 1);

      const int old_face_size = orig_faces[pidx].size();
      face_offsets[new_face_index] = int(j + (loops_num * stride));

      /* prev loop */
      k1 = face_offsets[pidx] + (((edge_order[eidx] - 1) + old_face_size) % old_face_size);

      k2 = face_offsets[pidx] + (edge_order[eidx]);

      CustomData_copy_data(
          &mesh->loop_data, &result->loop_data, k2, int((loops_num * stride) + j + 0), 1);
      CustomData_copy_data(
          &mesh->loop_data, &result->loop_data, k1, int((loops_num * stride) + j + 1), 1);
      CustomData_copy_data(
          &mesh->loop_data, &result->loop_data, k1, int((loops_num * stride) + j + 2), 1);
      CustomData_copy_data(
          &mesh->loop_data, &result->loop_data, k2, int((loops_num * stride) + j + 3), 1);

      if (flip == false) {
        new_corner_verts[j] = edge[0];
        new_corner_edges[j++] = eidx;

        new_corner_verts[j] = edge[1];
        new_corner_edges[j++] = (edges_num * stride) + old_vert_arr[edge[1]] + newEdges;

        new_corner_verts[j] = (do_shell ? edge[1] : old_vert_arr[edge[1]]) + verts_num;
        new_corner_edges[j++] = (do_shell ? eidx : i) + edges_num;

        new_corner_verts[j] = (do_shell ? edge[0] : old_vert_arr[edge[0]]) + verts_num;
        new_corner_edges[j++] = (edges_num * stride) + old_vert_arr[edge[0]] + newEdges;
      }
      else {
        new_corner_verts[j] = edge[1];
        new_corner_edges[j++] = eidx;

        new_corner_verts[j] = edge[0];
        new_corner_edges[j++] = (edges_num * stride) + old_vert_arr[edge[0]] + newEdges;

        new_corner_verts[j] = (do_shell ? edge[0] : old_vert_arr[edge[0]]) + verts_num;
        new_corner_edges[j++] = (do_shell ? eidx : i) + edges_num;

        new_corner_verts[j] = (do_shell ? edge[1] : old_vert_arr[edge[1]]) + verts_num;
        new_corner_edges[j++] = (edges_num * stride) + old_vert_arr[edge[1]] + newEdges;
      }

      if (origindex_edge) {
        origindex_edge[new_corner_edges[j - 3]] = ORIGINDEX_NONE;
        origindex_edge[new_corner_edges[j - 1]] = ORIGINDEX_NONE;
      }

      /* use the next material index if option enabled */
      if (mat_ofs_rim) {
        dst_material_index[new_face_index] += mat_ofs_rim;
        CLAMP(dst_material_index[new_face_index], 0, mat_nr_max);
      }
      if (crease_outer) {
        /* crease += crease_outer; without wrapping */
        float *cr = &(result_edge_crease[eidx]);
        float tcr = *cr + crease_outer;
        *cr = tcr > 1.0f ? 1.0f : tcr;
      }

      if (crease_inner) {
        /* crease += crease_inner; without wrapping */
        float *cr = &(result_edge_crease[edges_num + (do_shell ? eidx : i)]);
        float tcr = *cr + crease_inner;
        *cr = tcr > 1.0f ? 1.0f : tcr;
      }

#ifdef SOLIDIFY_SIDE_NORMALS
      if (do_side_normals) {
        normal_quad_v3(nor,
                       vert_positions[new_corner_verts[j - 4]],
                       vert_positions[new_corner_verts[j - 3]],
                       vert_positions[new_corner_verts[j - 2]],
                       vert_positions[new_corner_verts[j - 1]]);

        add_v3_v3(edge_vert_nos[edge[0]], nor);
        add_v3_v3(edge_vert_nos[edge[1]], nor);
      }
#endif

      new_face_index++;
    }

#ifdef SOLIDIFY_SIDE_NORMALS
    if (do_side_normals) {
      for (i = 0; i < rimVerts; i++) {
        const blender::int2 &edge_orig = edges[i];
        const blender::int2 &edge = edges[edges_num * stride + i];
        float nor_cpy[3];
        int k;

        /* NOTE: only the first vertex (lower half of the index) is calculated. */
        BLI_assert(edge[0] < verts_num);
        normalize_v3_v3(nor_cpy, edge_vert_nos[edge_orig[0]]);

        for (k = 0; k < 2; k++) { /* loop over both verts of the edge */
          copy_v3_v3(nor, vert_normals[*(&edge[0] + k)]);
          add_v3_v3(nor, nor_cpy);
          normalize_v3(nor);
          copy_v3_v3((float *)&vert_normals[*(&edge[0] + k)].x, nor);
        }
      }

      MEM_freeN(edge_vert_nos);
    }
#endif

    MEM_freeN(new_vert_arr);
    MEM_freeN(new_edge_arr);

    MEM_freeN(edge_users);
    MEM_freeN(edge_order);
  }

  if (old_vert_arr) {
    MEM_freeN(old_vert_arr);
  }

  return result;
}

#undef SOLIDIFY_SIDE_NORMALS

/** \} */
