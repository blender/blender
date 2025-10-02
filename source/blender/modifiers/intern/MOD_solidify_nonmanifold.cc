/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <algorithm>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"

#include "MOD_solidify_util.hh" /* Own include. */
#include "MOD_util.hh"

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

/**
 * Similar to #project_v3_v3v3_normalized that returns the dot-product.
 */
static float project_v3_v3(float r[3], const float a[3])
{
  float d = dot_v3v3(r, a);
  r[0] -= a[0] * d;
  r[1] -= a[1] * d;
  r[2] -= a[2] * d;
  return d;
}

static float angle_signed_on_axis_normalized_v3v3_v3(const float n[3],
                                                     const float ref_n[3],
                                                     const float axis[3])
{
  float d = dot_v3v3(n, ref_n);
  CLAMP(d, -1, 1);
  float angle = acosf(d);
  float cross[3];
  cross_v3_v3v3(cross, n, ref_n);
  if (dot_v3v3(cross, axis) >= 0) {
    angle = 2 * M_PI - angle;
  }
  return angle;
}

static float clamp_nonzero(const float value, const float epsilon)
{
  BLI_assert(!(epsilon < 0.0f));
  /* Return closest value with `abs(value) >= epsilon`. */
  if (value < 0.0f) {
    return min_ff(value, -epsilon);
  }
  return max_ff(value, epsilon);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Solidify Function
 * \{ */

/* Data structures for manifold solidify. */

struct NewEdgeRef;

struct NewFaceRef {
  blender::IndexRange face = {};
  uint index = 0;
  bool reversed = false;
  NewEdgeRef **link_edges = nullptr;
};

struct OldEdgeFaceRef {
  uint *faces;
  uint faces_len;
  bool *faces_reversed;
  uint used;
};

struct OldVertEdgeRef {
  uint *edges;
  uint edges_len;
};

struct NewEdgeRef {
  uint old_edge;
  NewFaceRef *faces[2];
  struct EdgeGroup *link_edge_groups[2];
  float angle;
  uint new_edge;
};

struct EdgeGroup {
  bool valid;
  NewEdgeRef **edges;
  uint edges_len;
  uint open_face_edge;
  bool is_orig_closed;
  bool is_even_split;
  uint split;
  bool is_singularity;
  uint topo_group;
  float co[3];
  float no[3];
  uint new_vert;
};

struct FaceKeyPair {
  float angle;
  NewFaceRef *face;
};

static int comp_float_int_pair(const void *a, const void *b)
{
  FaceKeyPair *x = (FaceKeyPair *)a;
  FaceKeyPair *y = (FaceKeyPair *)b;
  return int(x->angle > y->angle) - int(x->angle < y->angle);
}

/* NOLINTNEXTLINE: readability-function-size */
Mesh *MOD_solidify_nonmanifold_modifyMesh(ModifierData *md,
                                          const ModifierEvalContext *ctx,
                                          Mesh *mesh)
{
  using namespace blender;
  Mesh *result;
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;

  const uint verts_num = uint(mesh->verts_num);
  const uint edges_num = uint(mesh->edges_num);
  const uint faces_num = uint(mesh->faces_num);

  if (faces_num == 0 && verts_num != 0) {
    return mesh;
  }

  /* Only use material offsets if we have 2 or more materials. */
  const short mat_nrs = ctx->object->totcol > 1 ? ctx->object->totcol : 1;
  const short mat_nr_max = mat_nrs - 1;
  const short mat_ofs = mat_nrs > 1 ? smd->mat_ofs : 0;
  const short mat_ofs_rim = mat_nrs > 1 ? smd->mat_ofs_rim : 0;

  /* #ofs_front and #ofs_back are the offset from the original
   * surface along the normal, where #oft_front is along the positive
   * and #oft_back is along the negative normal. */
  const float ofs_front = (smd->offset_fac + 1.0f) * 0.5f * smd->offset;
  const float ofs_back = ofs_front - smd->offset * smd->offset_fac;
  /* #ofs_front_clamped and #ofs_back_clamped are the same as
   * #ofs_front and #ofs_back, but never zero. */
  const float ofs_front_clamped = clamp_nonzero(ofs_front, 1e-5f);
  const float ofs_back_clamped = clamp_nonzero(ofs_back, 1e-5f);
  const float offset_fac_vg = smd->offset_fac_vg;
  const float offset_fac_vg_inv = 1.0f - smd->offset_fac_vg;
  const float offset = fabsf(smd->offset) * smd->offset_clamp;
  const bool do_angle_clamp = smd->flag & MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP;
  /* #do_flip, flips the normals of the result. This is inverted if negative thickness
   * is used, since simple solidify with negative thickness keeps the faces facing outside. */
  const bool do_flip = ((smd->flag & MOD_SOLIDIFY_FLIP) != 0) == (smd->offset > 0);
  const bool do_rim = smd->flag & MOD_SOLIDIFY_RIM;
  const bool do_shell = ((smd->flag & MOD_SOLIDIFY_RIM) && (smd->flag & MOD_SOLIDIFY_NOSHELL)) ==
                        0;
  const bool do_clamp = (smd->offset_clamp != 0.0f);

  const float bevel_convex = smd->bevel_convex;

  const MDeformVert *dvert;
  const bool defgrp_invert = (smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0;
  int defgrp_index;
  const int shell_defgrp_index = BKE_id_defgroup_name_index(&mesh->id, smd->shell_defgrp_name);
  const int rim_defgrp_index = BKE_id_defgroup_name_index(&mesh->id, smd->rim_defgrp_name);

  MOD_get_vgroup(ctx->object, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  const bool do_flat_faces = dvert && (smd->flag & MOD_SOLIDIFY_NONMANIFOLD_FLAT_FACES);

  const blender::Span<blender::float3> orig_vert_positions = mesh->vert_positions();
  const blender::Span<int2> orig_edges = mesh->edges();
  const blender::OffsetIndices orig_faces = mesh->faces();
  const blender::Span<int> orig_corner_verts = mesh->corner_verts();
  const blender::Span<int> orig_corner_edges = mesh->corner_edges();
  const bke::AttributeAccessor orig_attributes = mesh->attributes();

  /* These might be null. */
  const VArraySpan orig_vert_bweight = *orig_attributes.lookup<float>("bevel_weight_vert",
                                                                      bke::AttrDomain::Point);
  const VArraySpan orig_edge_bweight = *orig_attributes.lookup<float>("bevel_weight_edge",
                                                                      bke::AttrDomain::Edge);
  const VArraySpan orig_edge_crease = *orig_attributes.lookup<float>("crease_edge",
                                                                     bke::AttrDomain::Edge);

  uint new_verts_num = 0;
  uint new_edges_num = 0;
  uint new_loops_num = 0;
  uint new_faces_num = 0;

#define MOD_SOLIDIFY_EMPTY_TAG uint(-1)

  /* Calculate only face normals. Copied because they are modified directly below. */
  blender::Array<blender::float3> face_nors = mesh->face_normals_true();

  blender::Array<NewFaceRef> face_sides_arr(faces_num * 2);
  bool *null_faces = (smd->nonmanifold_offset_mode ==
                      MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS) ?
                         MEM_calloc_arrayN<bool>(faces_num, __func__) :
                         nullptr;
  uint largest_ngon = 3;
  /* Calculate face to #NewFaceRef map. */
  {
    for (const int i : orig_faces.index_range()) {
      const blender::IndexRange &face = orig_faces[i];
      /* Make normals for faces without area (should really be avoided though). */
      if (len_squared_v3(face_nors[i]) < 0.5f) {
        const int2 &edge = orig_edges[orig_corner_edges[face.start()]];
        float edgedir[3];
        sub_v3_v3v3(edgedir, orig_vert_positions[edge[1]], orig_vert_positions[edge[0]]);
        if (fabsf(edgedir[2]) < fabsf(edgedir[1])) {
          face_nors[i][2] = 1.0f;
        }
        else {
          face_nors[i][1] = 1.0f;
        }
        if (null_faces) {
          null_faces[i] = true;
        }
      }

      NewEdgeRef **link_edges = MEM_calloc_arrayN<NewEdgeRef *>(uint(face.size()), __func__);

      NewFaceRef new_face_ref_a{};
      new_face_ref_a.face = face;
      new_face_ref_a.index = uint(i);
      new_face_ref_a.reversed = false;
      new_face_ref_a.link_edges = link_edges;
      face_sides_arr[i * 2] = std::move(new_face_ref_a);

      link_edges = MEM_calloc_arrayN<NewEdgeRef *>(uint(face.size()), __func__);

      NewFaceRef new_face_ref_b{};
      new_face_ref_b.face = face;
      new_face_ref_b.index = uint(i);
      new_face_ref_b.reversed = true;
      new_face_ref_b.link_edges = link_edges;
      face_sides_arr[i * 2 + 1] = std::move(new_face_ref_b);

      if (face.size() > largest_ngon) {
        largest_ngon = uint(face.size());
      }
      /* add to final mesh face count */
      if (do_shell) {
        new_faces_num += 2;
        new_loops_num += uint(face.size() * 2);
      }
    }
  }

  uint *edge_adj_faces_len = MEM_calloc_arrayN<uint>(edges_num, __func__);
  /* Count for each edge how many faces it has adjacent. */
  {
    for (const int64_t i : orig_faces.index_range()) {
      for (const int64_t edge : orig_corner_edges.slice(orig_faces[i])) {
        edge_adj_faces_len[edge]++;
      }
    }
  }

  /* Original edge to #NewEdgeRef map. */
  NewEdgeRef ***orig_edge_data_arr = MEM_calloc_arrayN<NewEdgeRef **>(edges_num, __func__);
  /* Original edge length cache. */
  float *orig_edge_lengths = MEM_calloc_arrayN<float>(edges_num, __func__);
  /* Edge groups for every original vert. */
  EdgeGroup **orig_vert_groups_arr = MEM_calloc_arrayN<EdgeGroup *>(verts_num, __func__);
  /* vertex map used to map duplicates. */
  uint *vm = MEM_malloc_arrayN<uint>(verts_num, __func__);
  for (uint i = 0; i < verts_num; i++) {
    vm[i] = i;
  }

  uint edge_index = 0;
  uint loop_index = 0;
  uint face_index = 0;

  bool has_singularities = false;

  /* Vert edge adjacent map. */
  OldVertEdgeRef **vert_adj_edges = MEM_calloc_arrayN<OldVertEdgeRef *>(verts_num, __func__);
  /* Original vertex positions (changed for degenerated geometry). */
  float (*orig_mvert_co)[3] = MEM_malloc_arrayN<float[3]>(verts_num, __func__);
  /* Fill in the original vertex positions. */
  for (uint i = 0; i < verts_num; i++) {
    orig_mvert_co[i][0] = orig_vert_positions[i][0];
    orig_mvert_co[i][1] = orig_vert_positions[i][1];
    orig_mvert_co[i][2] = orig_vert_positions[i][2];
  }

  /* Create edge to #NewEdgeRef map. */
  {
    OldEdgeFaceRef **edge_adj_faces = MEM_calloc_arrayN<OldEdgeFaceRef *>(edges_num, __func__);

    /* Create link_faces for edges. */
    {
      for (const int64_t i : orig_faces.index_range()) {
        for (const int64_t corner : orig_faces[i]) {
          const int vert = orig_corner_verts[corner];
          const int edge = orig_corner_edges[corner];
          const bool reversed = orig_edges[edge][1] != vert;
          OldEdgeFaceRef *old_face_edge_ref = edge_adj_faces[edge];
          if (old_face_edge_ref == nullptr) {
            const uint len = edge_adj_faces_len[edge];
            BLI_assert(len > 0);
            uint *adj_faces = MEM_malloc_arrayN<uint>(len, __func__);
            bool *adj_faces_reversed = MEM_malloc_arrayN<bool>(len, __func__);
            adj_faces[0] = uint(i);
            for (uint k = 1; k < len; k++) {
              adj_faces[k] = MOD_SOLIDIFY_EMPTY_TAG;
            }
            adj_faces_reversed[0] = reversed;
            OldEdgeFaceRef *ref = MEM_mallocN<OldEdgeFaceRef>(__func__);
            *ref = OldEdgeFaceRef{adj_faces, len, adj_faces_reversed, 1};
            edge_adj_faces[edge] = ref;
          }
          else {
            for (uint k = 1; k < old_face_edge_ref->faces_len; k++) {
              if (old_face_edge_ref->faces[k] == MOD_SOLIDIFY_EMPTY_TAG) {
                old_face_edge_ref->faces[k] = uint(i);
                old_face_edge_ref->faces_reversed[k] = reversed;
                break;
              }
            }
          }
        }
      }
    }

    float edgedir[3] = {0, 0, 0};
    uint *vert_adj_edges_len = MEM_calloc_arrayN<uint>(verts_num, __func__);

    /* Calculate edge lengths and len vert_adj edges. */
    {
      bool *face_singularity = MEM_calloc_arrayN<bool>(faces_num, __func__);

      const float merge_tolerance_sqr = smd->merge_tolerance * smd->merge_tolerance;
      uint *combined_verts = MEM_calloc_arrayN<uint>(verts_num, __func__);

      for (uint i = 0; i < edges_num; i++) {
        const int2 &edge = orig_edges[i];
        if (edge_adj_faces_len[i] > 0) {
          uint v1 = vm[edge[0]];
          uint v2 = vm[edge[1]];
          if (v1 == v2) {
            continue;
          }

          if (v2 < v1) {
            std::swap(v1, v2);
          }
          sub_v3_v3v3(edgedir, orig_mvert_co[v2], orig_mvert_co[v1]);
          orig_edge_lengths[i] = len_squared_v3(edgedir);

          if (orig_edge_lengths[i] <= merge_tolerance_sqr) {
            /* Merge verts. But first check if that would create a higher face count. */
            /* This check is very slow. It would need the vertex edge links to get
             * accelerated that are not yet available at this point. */
            bool can_merge = true;
            for (uint k = 0; k < edges_num && can_merge; k++) {
              if (k != i && edge_adj_faces_len[k] > 0 &&
                  (ELEM(vm[orig_edges[k][0]], v1, v2) != ELEM(vm[orig_edges[k][1]], v1, v2)))
              {
                for (uint j = 0; j < edge_adj_faces[k]->faces_len && can_merge; j++) {
                  const blender::IndexRange face = orig_faces[edge_adj_faces[k]->faces[j]];
                  uint changes = 0;
                  int cur = face.size() - 1;
                  for (int next = 0; next < face.size() && changes <= 2; next++) {
                    uint cur_v = vm[orig_corner_verts[face[cur]]];
                    uint next_v = vm[orig_corner_verts[face[next]]];
                    changes += (ELEM(cur_v, v1, v2) != ELEM(next_v, v1, v2));
                    cur = next;
                  }
                  can_merge = can_merge && changes <= 2;
                }
              }
            }

            if (!can_merge) {
              orig_edge_lengths[i] = 0.0f;
              vert_adj_edges_len[v1]++;
              vert_adj_edges_len[v2]++;
              continue;
            }

            mul_v3_fl(edgedir,
                      (combined_verts[v2] + 1) /
                          float(combined_verts[v1] + combined_verts[v2] + 2));
            add_v3_v3(orig_mvert_co[v1], edgedir);
            for (uint j = v2; j < verts_num; j++) {
              if (vm[j] == v2) {
                vm[j] = v1;
              }
            }
            vert_adj_edges_len[v1] += vert_adj_edges_len[v2];
            vert_adj_edges_len[v2] = 0;
            combined_verts[v1] += combined_verts[v2] + 1;

            if (do_shell) {
              new_loops_num -= edge_adj_faces_len[i] * 2;
            }

            edge_adj_faces_len[i] = 0;
            MEM_freeN(edge_adj_faces[i]->faces);
            MEM_freeN(edge_adj_faces[i]->faces_reversed);
            MEM_freeN(edge_adj_faces[i]);
            edge_adj_faces[i] = nullptr;
          }
          else {
            orig_edge_lengths[i] = sqrtf(orig_edge_lengths[i]);
            vert_adj_edges_len[v1]++;
            vert_adj_edges_len[v2]++;
          }
        }
      }
      /* remove zero faces in a second pass */
      for (uint i = 0; i < edges_num; i++) {
        const int2 &edge = orig_edges[i];
        const uint v1 = vm[edge[0]];
        const uint v2 = vm[edge[1]];
        if (v1 == v2 && edge_adj_faces[i]) {
          /* Remove faces. */
          for (uint j = 0; j < edge_adj_faces[i]->faces_len; j++) {
            const uint face = edge_adj_faces[i]->faces[j];
            if (!face_singularity[face]) {
              bool is_singularity = true;
              for (const int vert : orig_corner_verts.slice(orig_faces[face])) {
                if (vm[vert] != v1) {
                  is_singularity = false;
                  break;
                }
              }
              if (is_singularity) {
                face_singularity[face] = true;
                /* remove from final mesh face count */
                if (do_shell) {
                  new_faces_num -= 2;
                }
              }
            }
          }

          if (do_shell) {
            new_loops_num -= edge_adj_faces_len[i] * 2;
          }

          edge_adj_faces_len[i] = 0;
          MEM_freeN(edge_adj_faces[i]->faces);
          MEM_freeN(edge_adj_faces[i]->faces_reversed);
          MEM_freeN(edge_adj_faces[i]);
          edge_adj_faces[i] = nullptr;
        }
      }

      MEM_freeN(face_singularity);
      MEM_freeN(combined_verts);
    }

    /* Create vert_adj_edges for verts. */
    {
      for (uint i = 0; i < edges_num; i++) {
        const int2 &edge = orig_edges[i];
        if (edge_adj_faces_len[i] > 0) {
          const int vs[2] = {int(vm[edge[0]]), int(vm[edge[1]])};
          uint invalid_edge_index = 0;
          bool invalid_edge_reversed = false;
          for (uint j = 0; j < 2; j++) {
            const int vert = vs[j];
            const uint len = vert_adj_edges_len[vert];
            if (len > 0) {
              OldVertEdgeRef *old_edge_vert_ref = vert_adj_edges[vert];
              if (old_edge_vert_ref == nullptr) {
                uint *adj_edges = MEM_calloc_arrayN<uint>(len, __func__);
                adj_edges[0] = i;
                for (uint k = 1; k < len; k++) {
                  adj_edges[k] = MOD_SOLIDIFY_EMPTY_TAG;
                }
                OldVertEdgeRef *ref = MEM_mallocN<OldVertEdgeRef>(__func__);
                *ref = OldVertEdgeRef{adj_edges, 1};
                vert_adj_edges[vert] = ref;
              }
              else {
                const uint *f = old_edge_vert_ref->edges;
                for (uint k = 0; k < len && k <= old_edge_vert_ref->edges_len; k++, f++) {
                  const uint edge = old_edge_vert_ref->edges[k];
                  if (edge == MOD_SOLIDIFY_EMPTY_TAG || k == old_edge_vert_ref->edges_len) {
                    old_edge_vert_ref->edges[k] = i;
                    old_edge_vert_ref->edges_len++;
                    break;
                  }
                  if (vm[orig_edges[edge][0]] == vs[1 - j]) {
                    invalid_edge_index = edge + 1;
                    invalid_edge_reversed = (j == 0);
                    break;
                  }
                  if (vm[orig_edges[edge][1]] == vs[1 - j]) {
                    invalid_edge_index = edge + 1;
                    invalid_edge_reversed = (j == 1);
                    break;
                  }
                }
                if (invalid_edge_index) {
                  if (j == 1) {
                    /* Should never actually be executed. */
                    vert_adj_edges[vs[0]]->edges_len--;
                  }
                  break;
                }
              }
            }
          }
          /* Remove zero faces that are in shape of an edge. */
          if (invalid_edge_index) {
            const uint tmp = invalid_edge_index - 1;
            invalid_edge_index = i;
            i = tmp;
            OldEdgeFaceRef *i_adj_faces = edge_adj_faces[i];
            OldEdgeFaceRef *invalid_adj_faces = edge_adj_faces[invalid_edge_index];
            uint j = 0;
            for (uint k = 0; k < i_adj_faces->faces_len; k++) {
              for (uint l = 0; l < invalid_adj_faces->faces_len; l++) {
                if (i_adj_faces->faces[k] == invalid_adj_faces->faces[l] &&
                    i_adj_faces->faces[k] != MOD_SOLIDIFY_EMPTY_TAG)
                {
                  i_adj_faces->faces[k] = MOD_SOLIDIFY_EMPTY_TAG;
                  invalid_adj_faces->faces[l] = MOD_SOLIDIFY_EMPTY_TAG;
                  j++;
                }
              }
            }
            /* remove from final face count */
            if (do_shell) {
              new_faces_num -= 2 * j;
              new_loops_num -= 4 * j;
            }
            const uint len = i_adj_faces->faces_len + invalid_adj_faces->faces_len - 2 * j;
            uint *adj_faces = MEM_malloc_arrayN<uint>(len, __func__);
            bool *adj_faces_loops_reversed = MEM_malloc_arrayN<bool>(len, __func__);
            /* Clean merge of adj_faces. */
            j = 0;
            for (uint k = 0; k < i_adj_faces->faces_len; k++) {
              if (i_adj_faces->faces[k] != MOD_SOLIDIFY_EMPTY_TAG) {
                adj_faces[j] = i_adj_faces->faces[k];
                adj_faces_loops_reversed[j++] = i_adj_faces->faces_reversed[k];
              }
            }
            for (uint k = 0; k < invalid_adj_faces->faces_len; k++) {
              if (invalid_adj_faces->faces[k] != MOD_SOLIDIFY_EMPTY_TAG) {
                adj_faces[j] = invalid_adj_faces->faces[k];
                adj_faces_loops_reversed[j++] = (invalid_edge_reversed !=
                                                 invalid_adj_faces->faces_reversed[k]);
              }
            }
            BLI_assert(j == len);
            edge_adj_faces_len[invalid_edge_index] = 0;
            edge_adj_faces_len[i] = len;
            MEM_freeN(i_adj_faces->faces);
            MEM_freeN(i_adj_faces->faces_reversed);
            i_adj_faces->faces_len = len;
            i_adj_faces->faces = adj_faces;
            i_adj_faces->faces_reversed = adj_faces_loops_reversed;
            i_adj_faces->used += invalid_adj_faces->used;
            MEM_freeN(invalid_adj_faces->faces);
            MEM_freeN(invalid_adj_faces->faces_reversed);
            MEM_freeN(invalid_adj_faces);
            edge_adj_faces[invalid_edge_index] = i_adj_faces;
            /* Reset counter to continue. */
            i = invalid_edge_index;
          }
        }
      }
    }

    MEM_freeN(vert_adj_edges_len);

    /* Filter duplicate faces. */
    {
      const int2 *edge = orig_edges.data();
      /* Iterate over edges and only check the faces around an edge for duplicates
       * (performance optimization). */
      for (uint i = 0; i < edges_num; i++, edge++) {
        if (edge_adj_faces_len[i] > 0) {
          const OldEdgeFaceRef *adj_faces = edge_adj_faces[i];
          uint adj_len = adj_faces->faces_len;
          /* Not that #adj_len doesn't need to equal edge_adj_faces_len anymore
           * because #adj_len is shared when a face got collapsed to an edge. */
          if (adj_len > 1) {
            /* For each face pair check if they have equal verts. */
            for (uint j = 0; j < adj_len; j++) {
              const uint face = adj_faces->faces[j];
              const int j_loopstart = orig_faces[face].start();
              const int totloop = orig_faces[face].size();
              const uint j_first_v = vm[orig_corner_verts[j_loopstart]];
              for (uint k = j + 1; k < adj_len; k++) {
                if (orig_faces[adj_faces->faces[k]].size() != totloop) {
                  continue;
                }
                /* Find first face first loop vert in second face loops. */
                const int k_loopstart = orig_faces[adj_faces->faces[k]].start();
                int l;
                {
                  const int *corner_vert = &orig_corner_verts[k_loopstart];
                  for (l = 0; l < totloop && vm[*corner_vert] != j_first_v; l++, corner_vert++) {
                    /* Pass. */
                  }
                }
                if (l == totloop) {
                  continue;
                }
                /* Check if all following loops have equal verts. */
                const bool reversed = adj_faces->faces_reversed[j] != adj_faces->faces_reversed[k];
                const int count_dir = reversed ? -1 : 1;
                bool has_diff = false;
                for (int m = 0, n = l + totloop; m < totloop && !has_diff; m++, n += count_dir) {
                  const int vert = orig_corner_verts[j_loopstart + m];
                  has_diff = has_diff ||
                             vm[vert] != vm[orig_corner_verts[k_loopstart + n % totloop]];
                }
                /* If the faces are equal, discard one (j). */
                if (!has_diff) {
                  uint del_loops = 0;
                  for (uint m = 0; m < totloop; m++) {
                    const int e = orig_corner_edges[j_loopstart + m];
                    OldEdgeFaceRef *e_adj_faces = edge_adj_faces[e];
                    if (e_adj_faces) {
                      uint face_index = j;
                      uint *e_adj_faces_faces = e_adj_faces->faces;
                      bool *e_adj_faces_reversed = e_adj_faces->faces_reversed;
                      const uint faces_len = e_adj_faces->faces_len;
                      if (e_adj_faces_faces != adj_faces->faces) {
                        /* Find index of e in #adj_faces. */
                        for (face_index = 0;
                             face_index < faces_len && e_adj_faces_faces[face_index] != face;
                             face_index++)
                        {
                          /* Pass. */
                        }
                        /* If not found. */
                        if (face_index == faces_len) {
                          continue;
                        }
                      }
                      else {
                        /* If we shrink #edge_adj_faces[i] we need to update this field. */
                        adj_len--;
                      }
                      memmove(e_adj_faces_faces + face_index,
                              e_adj_faces_faces + face_index + 1,
                              (faces_len - face_index - 1) * sizeof(*e_adj_faces_faces));
                      memmove(e_adj_faces_reversed + face_index,
                              e_adj_faces_reversed + face_index + 1,
                              (faces_len - face_index - 1) * sizeof(*e_adj_faces_reversed));
                      e_adj_faces->faces_len--;
                      if (edge_adj_faces_len[e] > 0) {
                        edge_adj_faces_len[e]--;
                        if (edge_adj_faces_len[e] == 0) {
                          e_adj_faces->used--;
                          edge_adj_faces[e] = nullptr;
                        }
                      }
                      else if (e_adj_faces->used > 1) {
                        for (uint n = 0; n < edges_num; n++) {
                          if (edge_adj_faces[n] == e_adj_faces && edge_adj_faces_len[n] > 0) {
                            edge_adj_faces_len[n]--;
                            if (edge_adj_faces_len[n] == 0) {
                              edge_adj_faces[n]->used--;
                              edge_adj_faces[n] = nullptr;
                            }
                            break;
                          }
                        }
                      }
                      del_loops++;
                    }
                  }
                  if (do_shell) {
                    new_faces_num -= 2;
                    new_loops_num -= 2 * del_loops;
                  }
                  break;
                }
              }
            }
          }
        }
      }
    }

    /* Create #NewEdgeRef array. */
    {
      for (uint i = 0; i < edges_num; i++) {
        const int2 &edge = orig_edges[i];
        const uint v1 = vm[edge[0]];
        const uint v2 = vm[edge[1]];
        if (edge_adj_faces_len[i] > 0) {
          if (LIKELY(orig_edge_lengths[i] > FLT_EPSILON)) {
            sub_v3_v3v3(edgedir, orig_mvert_co[v2], orig_mvert_co[v1]);
            mul_v3_fl(edgedir, 1.0f / orig_edge_lengths[i]);
          }
          else {
            /* Smart fallback. */
            /* This makes merging non essential, but correct
             * merging will still give way better results. */
            float pos[3];
            copy_v3_v3(pos, orig_mvert_co[v2]);

            OldVertEdgeRef *link1 = vert_adj_edges[v1];
            float v1_dir[3];
            zero_v3(v1_dir);
            for (int j = 0; j < link1->edges_len; j++) {
              uint e = link1->edges[j];
              if (edge_adj_faces_len[e] > 0 && e != i) {
                uint other_v =
                    vm[vm[orig_edges[e][0]] == v1 ? orig_edges[e][1] : orig_edges[e][0]];
                sub_v3_v3v3(edgedir, orig_mvert_co[other_v], pos);
                add_v3_v3(v1_dir, edgedir);
              }
            }
            OldVertEdgeRef *link2 = vert_adj_edges[v2];
            float v2_dir[3];
            zero_v3(v2_dir);
            for (int j = 0; j < link2->edges_len; j++) {
              uint e = link2->edges[j];
              if (edge_adj_faces_len[e] > 0 && e != i) {
                uint other_v =
                    vm[vm[orig_edges[e][0]] == v2 ? orig_edges[e][1] : orig_edges[e][0]];
                sub_v3_v3v3(edgedir, orig_mvert_co[other_v], pos);
                add_v3_v3(v2_dir, edgedir);
              }
            }
            sub_v3_v3v3(edgedir, v2_dir, v1_dir);
            float len = normalize_v3(edgedir);
            if (len == 0.0f) {
              edgedir[0] = 0.0f;
              edgedir[1] = 0.0f;
              edgedir[2] = 1.0f;
            }
          }

          OldEdgeFaceRef *adj_faces = edge_adj_faces[i];
          const uint adj_len = adj_faces->faces_len;
          const uint *adj_faces_faces = adj_faces->faces;
          const bool *adj_faces_reversed = adj_faces->faces_reversed;
          uint new_edges_len = 0;
          FaceKeyPair *sorted_faces = MEM_malloc_arrayN<FaceKeyPair>(adj_len, __func__);
          if (adj_len > 1) {
            new_edges_len = adj_len;
            /* Get keys for sorting. */
            float ref_nor[3] = {0, 0, 0};
            float nor[3];
            for (uint j = 0; j < adj_len; j++) {
              const bool reverse = adj_faces_reversed[j];
              const uint face_i = adj_faces_faces[j];
              if (reverse) {
                negate_v3_v3(nor, face_nors[face_i]);
              }
              else {
                copy_v3_v3(nor, face_nors[face_i]);
              }
              float d = 1;
              if (orig_faces[face_i].size() > 3) {
                d = project_v3_v3(nor, edgedir);
                if (LIKELY(d != 0)) {
                  d = normalize_v3(nor);
                }
                else {
                  d = 1;
                }
              }
              if (UNLIKELY(d == 0.0f)) {
                sorted_faces[j].angle = 0.0f;
              }
              else if (j == 0) {
                copy_v3_v3(ref_nor, nor);
                sorted_faces[j].angle = 0.0f;
              }
              else {
                float angle = angle_signed_on_axis_normalized_v3v3_v3(nor, ref_nor, edgedir);
                sorted_faces[j].angle = -angle;
              }
              sorted_faces[j].face =
                  &face_sides_arr[adj_faces_faces[j] * 2 + (adj_faces_reversed[j] ? 1 : 0)];
            }
            /* Sort faces by order around the edge (keep order in faces,
             * reversed and face_angles the same). */
            qsort(sorted_faces, adj_len, sizeof(*sorted_faces), comp_float_int_pair);
          }
          else {
            new_edges_len = 2;
            sorted_faces[0].face =
                &face_sides_arr[adj_faces_faces[0] * 2 + (adj_faces_reversed[0] ? 1 : 0)];
            if (do_rim) {
              /* Only add the loops parallel to the edge for now. */
              new_loops_num += 2;
              new_faces_num++;
            }
          }

          /* Create a list of new edges and fill it. */
          NewEdgeRef **new_edges = MEM_malloc_arrayN<NewEdgeRef *>(new_edges_len + 1, __func__);
          new_edges[new_edges_len] = nullptr;
          NewFaceRef *faces[2];
          for (uint j = 0; j < new_edges_len; j++) {
            float angle;
            if (adj_len > 1) {
              const uint next_j = j + 1 == adj_len ? 0 : j + 1;
              faces[0] = sorted_faces[j].face;
              faces[1] = sorted_faces[next_j].face->reversed ? sorted_faces[next_j].face - 1 :
                                                               sorted_faces[next_j].face + 1;
              angle = sorted_faces[next_j].angle - sorted_faces[j].angle;
              if (angle < 0) {
                angle += 2 * M_PI;
              }
            }
            else {
              faces[0] = sorted_faces[0].face->reversed ? sorted_faces[0].face - j :
                                                          sorted_faces[0].face + j;
              faces[1] = nullptr;
              angle = 0;
            }
            NewEdgeRef *edge_data = MEM_mallocN<NewEdgeRef>(__func__);
            uint edge_data_edge_index = MOD_SOLIDIFY_EMPTY_TAG;
            if (do_shell || (adj_len == 1 && do_rim)) {
              edge_data_edge_index = 0;
            }

            NewEdgeRef new_edge_ref{};
            new_edge_ref.old_edge = i;
            new_edge_ref.faces[0] = faces[0];
            new_edge_ref.faces[1] = faces[1];
            new_edge_ref.link_edge_groups[0] = nullptr;
            new_edge_ref.link_edge_groups[1] = nullptr;
            new_edge_ref.angle = angle;
            new_edge_ref.new_edge = edge_data_edge_index;
            *edge_data = new_edge_ref;

            new_edges[j] = edge_data;
            for (uint k = 0; k < 2; k++) {
              if (faces[k] != nullptr) {
                for (int l = 0; l < faces[k]->face.size(); l++) {
                  const int edge = orig_corner_edges[faces[k]->face.start() + l];
                  if (edge_adj_faces[edge] == edge_adj_faces[i]) {
                    if (edge != i && orig_edge_data_arr[edge] == nullptr) {
                      orig_edge_data_arr[edge] = new_edges;
                    }
                    faces[k]->link_edges[l] = edge_data;
                    break;
                  }
                }
              }
            }
          }
          MEM_freeN(sorted_faces);
          orig_edge_data_arr[i] = new_edges;
          if (do_shell || (adj_len == 1 && do_rim)) {
            new_edges_num += new_edges_len;
          }
        }
      }
    }

    for (uint i = 0; i < edges_num; i++) {
      if (edge_adj_faces[i]) {
        if (edge_adj_faces[i]->used > 1) {
          edge_adj_faces[i]->used--;
        }
        else {
          MEM_freeN(edge_adj_faces[i]->faces);
          MEM_freeN(edge_adj_faces[i]->faces_reversed);
          MEM_freeN(edge_adj_faces[i]);
        }
      }
    }
    MEM_freeN(edge_adj_faces);
  }

  /* Create sorted edge groups for every vert. */
  {
    OldVertEdgeRef **adj_edges_ptr = vert_adj_edges;
    for (uint i = 0; i < verts_num; i++, adj_edges_ptr++) {
      if (*adj_edges_ptr != nullptr && (*adj_edges_ptr)->edges_len >= 2) {
        EdgeGroup *edge_groups;

        int eg_index = -1;
        bool contains_long_groups = false;
        uint topo_groups = 0;

        /* Initial sorted creation. */
        {
          const uint *adj_edges = (*adj_edges_ptr)->edges;
          const uint tot_adj_edges = (*adj_edges_ptr)->edges_len;

          uint unassigned_edges_len = 0;
          for (uint j = 0; j < tot_adj_edges; j++) {
            NewEdgeRef **new_edges = orig_edge_data_arr[adj_edges[j]];
            /* TODO: check where the null pointer come from,
             * because there should not be any... */
            if (new_edges) {
              /* count the number of new edges around the original vert */
              while (*new_edges) {
                unassigned_edges_len++;
                new_edges++;
              }
            }
          }
          NewEdgeRef **unassigned_edges = MEM_malloc_arrayN<NewEdgeRef *>(unassigned_edges_len,
                                                                          __func__);
          for (uint j = 0, k = 0; j < tot_adj_edges; j++) {
            NewEdgeRef **new_edges = orig_edge_data_arr[adj_edges[j]];
            if (new_edges) {
              while (*new_edges) {
                unassigned_edges[k++] = *new_edges;
                new_edges++;
              }
            }
          }

          /* An edge group will always contain min 2 edges
           * so max edge group count can be calculated. */
          uint edge_groups_len = unassigned_edges_len / 2;
          edge_groups = MEM_calloc_arrayN<EdgeGroup>(edge_groups_len + 1, __func__);

          uint assigned_edges_len = 0;
          NewEdgeRef *found_edge = nullptr;
          uint found_edge_index = 0;
          bool insert_at_start = false;
          uint eg_capacity = 5;
          NewFaceRef *eg_track_faces[2] = {nullptr, nullptr};
          NewFaceRef *last_open_edge_track = nullptr;

          while (assigned_edges_len < unassigned_edges_len) {
            found_edge = nullptr;
            insert_at_start = false;
            if (eg_index >= 0 && edge_groups[eg_index].edges_len == 0) {
              /* Called every time a new group was started in the last iteration. */
              /* Find an unused edge to start the next group
               * and setup variables to start creating it. */
              uint j = 0;
              NewEdgeRef *edge = nullptr;
              while (!edge && j < unassigned_edges_len) {
                edge = unassigned_edges[j++];
                if (edge && last_open_edge_track &&
                    (edge->faces[0] != last_open_edge_track || edge->faces[1] != nullptr))
                {
                  edge = nullptr;
                }
              }
              if (!edge && last_open_edge_track) {
                topo_groups++;
                last_open_edge_track = nullptr;
                edge_groups[eg_index].topo_group++;
                j = 0;
                while (!edge && j < unassigned_edges_len) {
                  edge = unassigned_edges[j++];
                }
              }
              else if (!last_open_edge_track && eg_index > 0) {
                topo_groups++;
                edge_groups[eg_index].topo_group++;
              }
              BLI_assert(edge != nullptr);
              found_edge_index = j - 1;
              found_edge = edge;
              if (!last_open_edge_track && vm[orig_edges[edge->old_edge][0]] == i) {
                eg_track_faces[0] = edge->faces[0];
                eg_track_faces[1] = edge->faces[1];
                if (edge->faces[1] == nullptr) {
                  last_open_edge_track = edge->faces[0]->reversed ? edge->faces[0] - 1 :
                                                                    edge->faces[0] + 1;
                }
              }
              else {
                eg_track_faces[0] = edge->faces[1];
                eg_track_faces[1] = edge->faces[0];
              }
            }
            else if (eg_index >= 0) {
              NewEdgeRef **edge_ptr = unassigned_edges;
              for (found_edge_index = 0; found_edge_index < unassigned_edges_len;
                   found_edge_index++, edge_ptr++)
              {
                if (*edge_ptr) {
                  NewEdgeRef *edge = *edge_ptr;
                  if (edge->faces[0] == eg_track_faces[1]) {
                    insert_at_start = false;
                    eg_track_faces[1] = edge->faces[1];
                    found_edge = edge;
                    if (edge->faces[1] == nullptr) {
                      edge_groups[eg_index].is_orig_closed = false;
                      last_open_edge_track = edge->faces[0]->reversed ? edge->faces[0] - 1 :
                                                                        edge->faces[0] + 1;
                    }
                    break;
                  }
                  if (edge->faces[0] == eg_track_faces[0]) {
                    insert_at_start = true;
                    eg_track_faces[0] = edge->faces[1];
                    found_edge = edge;
                    if (edge->faces[1] == nullptr) {
                      edge_groups[eg_index].is_orig_closed = false;
                    }
                    break;
                  }
                  if (edge->faces[1] != nullptr) {
                    if (edge->faces[1] == eg_track_faces[1]) {
                      insert_at_start = false;
                      eg_track_faces[1] = edge->faces[0];
                      found_edge = edge;
                      break;
                    }
                    if (edge->faces[1] == eg_track_faces[0]) {
                      insert_at_start = true;
                      eg_track_faces[0] = edge->faces[0];
                      found_edge = edge;
                      break;
                    }
                  }
                }
              }
            }
            if (found_edge) {
              unassigned_edges[found_edge_index] = nullptr;
              assigned_edges_len++;
              const uint needed_capacity = edge_groups[eg_index].edges_len + 1;
              if (needed_capacity > eg_capacity) {
                eg_capacity = needed_capacity + 1;
                NewEdgeRef **new_eg = MEM_calloc_arrayN<NewEdgeRef *>(eg_capacity, __func__);
                if (insert_at_start) {
                  memcpy(new_eg + 1,
                         edge_groups[eg_index].edges,
                         edge_groups[eg_index].edges_len * sizeof(*new_eg));
                }
                else {
                  memcpy(new_eg,
                         edge_groups[eg_index].edges,
                         edge_groups[eg_index].edges_len * sizeof(*new_eg));
                }
                MEM_freeN(edge_groups[eg_index].edges);
                edge_groups[eg_index].edges = new_eg;
              }
              else if (insert_at_start) {
                memmove(edge_groups[eg_index].edges + 1,
                        edge_groups[eg_index].edges,
                        edge_groups[eg_index].edges_len * sizeof(*edge_groups[eg_index].edges));
              }
              edge_groups[eg_index].edges[insert_at_start ? 0 : edge_groups[eg_index].edges_len] =
                  found_edge;
              edge_groups[eg_index].edges_len++;
              if (edge_groups[eg_index].edges[edge_groups[eg_index].edges_len - 1]->faces[1] !=
                  nullptr)
              {
                last_open_edge_track = nullptr;
              }
              if (edge_groups[eg_index].edges_len > 3) {
                contains_long_groups = true;
              }
            }
            else {
              /* called on first iteration to clean up the eg_index = -1 and start the first group,
               * or when the current group is found to be complete (no new found_edge) */
              eg_index++;
              BLI_assert(eg_index < edge_groups_len);
              eg_capacity = 5;
              NewEdgeRef **edges = MEM_calloc_arrayN<NewEdgeRef *>(eg_capacity, __func__);

              EdgeGroup edge_group{};
              edge_group.valid = true;
              edge_group.edges = edges;
              edge_group.edges_len = 0;
              edge_group.open_face_edge = MOD_SOLIDIFY_EMPTY_TAG;
              edge_group.is_orig_closed = true;
              edge_group.is_even_split = false;
              edge_group.split = 0;
              edge_group.is_singularity = false;
              edge_group.topo_group = topo_groups;
              zero_v3(edge_group.co);
              zero_v3(edge_group.no);
              edge_group.new_vert = MOD_SOLIDIFY_EMPTY_TAG;
              edge_groups[eg_index] = edge_group;

              eg_track_faces[0] = nullptr;
              eg_track_faces[1] = nullptr;
            }
          }
          /* #eg_index is the number of groups from here on. */
          eg_index++;
          /* #topo_groups is the number of topo groups from here on. */
          topo_groups++;

          MEM_freeN(unassigned_edges);

          /* TODO: reshape the edge_groups array to its actual size
           * after writing is finished to save on memory. */
        }

        /* Split of long self intersection groups */
        {
          uint splits = 0;
          if (contains_long_groups) {
            uint add_index = 0;
            for (uint j = 0; j < eg_index; j++) {
              const uint edges_len = edge_groups[j + add_index].edges_len;
              if (edges_len > 3) {
                bool has_doubles = false;
                bool *doubles = MEM_calloc_arrayN<bool>(edges_len, __func__);
                EdgeGroup g = edge_groups[j + add_index];
                for (uint k = 0; k < edges_len; k++) {
                  for (uint l = k + 1; l < edges_len; l++) {
                    if (g.edges[k]->old_edge == g.edges[l]->old_edge) {
                      doubles[k] = true;
                      doubles[l] = true;
                      has_doubles = true;
                    }
                  }
                }
                if (has_doubles) {
                  const uint prior_splits = splits;
                  const uint prior_index = add_index;
                  int unique_start = -1;
                  int first_unique_end = -1;
                  int last_split = -1;
                  int first_split = -1;
                  bool first_even_split = false;
                  uint real_k = 0;
                  while (real_k < edges_len ||
                         (g.is_orig_closed &&
                          (real_k <=
                               (first_unique_end == -1 ? 0 : first_unique_end) + int(edges_len) ||
                           first_split != last_split)))
                  {
                    const uint k = real_k % edges_len;
                    if (!doubles[k]) {
                      if (first_unique_end != -1 && unique_start == -1) {
                        unique_start = int(real_k);
                      }
                    }
                    else if (first_unique_end == -1) {
                      first_unique_end = int(k);
                    }
                    else if (unique_start != -1) {
                      const uint split = ((uint(unique_start) + real_k + 1) / 2) % edges_len;
                      const bool is_even_split = ((uint(unique_start) + real_k) & 1);
                      if (last_split != -1) {
                        /* Override g on first split (no insert). */
                        if (prior_splits != splits) {
                          memmove(edge_groups + j + add_index + 1,
                                  edge_groups + j + add_index,
                                  (uint(eg_index) - j) * sizeof(*edge_groups));
                          add_index++;
                        }
                        if (last_split > split) {
                          const uint edges_len_group = (split + edges_len) - uint(last_split);
                          NewEdgeRef **edges = MEM_malloc_arrayN<NewEdgeRef *>(edges_len_group,
                                                                               __func__);
                          memcpy(edges,
                                 g.edges + last_split,
                                 (edges_len - uint(last_split)) * sizeof(*edges));
                          memcpy(edges + (edges_len - uint(last_split)),
                                 g.edges,
                                 split * sizeof(*edges));

                          EdgeGroup edge_group{};
                          edge_group.valid = true;
                          edge_group.edges = edges;
                          edge_group.edges_len = edges_len_group;
                          edge_group.open_face_edge = MOD_SOLIDIFY_EMPTY_TAG;
                          edge_group.is_orig_closed = g.is_orig_closed;
                          edge_group.is_even_split = is_even_split;
                          edge_group.split = add_index - prior_index + 1 + uint(!g.is_orig_closed);
                          edge_group.is_singularity = false;
                          edge_group.topo_group = g.topo_group;
                          zero_v3(edge_group.co);
                          zero_v3(edge_group.no);
                          edge_group.new_vert = MOD_SOLIDIFY_EMPTY_TAG;
                          edge_groups[j + add_index] = edge_group;
                        }
                        else {
                          const uint edges_len_group = split - uint(last_split);
                          NewEdgeRef **edges = MEM_malloc_arrayN<NewEdgeRef *>(edges_len_group,
                                                                               __func__);
                          memcpy(edges, g.edges + last_split, edges_len_group * sizeof(*edges));

                          EdgeGroup edge_group{};
                          edge_group.valid = true;
                          edge_group.edges = edges;
                          edge_group.edges_len = edges_len_group;
                          edge_group.open_face_edge = MOD_SOLIDIFY_EMPTY_TAG;
                          edge_group.is_orig_closed = g.is_orig_closed;
                          edge_group.is_even_split = is_even_split;
                          edge_group.split = add_index - prior_index + 1 + uint(!g.is_orig_closed);
                          edge_group.is_singularity = false;
                          edge_group.topo_group = g.topo_group;
                          zero_v3(edge_group.co);
                          zero_v3(edge_group.no);
                          edge_group.new_vert = MOD_SOLIDIFY_EMPTY_TAG;
                          edge_groups[j + add_index] = edge_group;
                        }
                        splits++;
                      }
                      last_split = int(split);
                      if (first_split == -1) {
                        first_split = int(split);
                        first_even_split = is_even_split;
                      }
                      unique_start = -1;
                    }
                    real_k++;
                  }
                  if (first_split != -1) {
                    if (!g.is_orig_closed) {
                      if (prior_splits != splits) {
                        memmove(edge_groups + (j + prior_index + 1),
                                edge_groups + (j + prior_index),
                                (uint(eg_index) + add_index - (j + prior_index)) *
                                    sizeof(*edge_groups));
                        memmove(edge_groups + (j + add_index + 2),
                                edge_groups + (j + add_index + 1),
                                (uint(eg_index) - j) * sizeof(*edge_groups));
                        add_index++;
                      }
                      else {
                        memmove(edge_groups + (j + add_index + 2),
                                edge_groups + (j + add_index + 1),
                                (uint(eg_index) - j - 1) * sizeof(*edge_groups));
                      }
                      NewEdgeRef **edges = MEM_malloc_arrayN<NewEdgeRef *>(uint(first_split),
                                                                           __func__);
                      memcpy(edges, g.edges, uint(first_split) * sizeof(*edges));

                      EdgeGroup edge_group_a{};
                      edge_group_a.valid = true;
                      edge_group_a.edges = edges;
                      edge_group_a.edges_len = uint(first_split);
                      edge_group_a.open_face_edge = MOD_SOLIDIFY_EMPTY_TAG;
                      edge_group_a.is_orig_closed = g.is_orig_closed;
                      edge_group_a.is_even_split = first_even_split;
                      edge_group_a.split = 1;
                      edge_group_a.is_singularity = false;
                      edge_group_a.topo_group = g.topo_group;
                      zero_v3(edge_group_a.co);
                      zero_v3(edge_group_a.no);
                      edge_group_a.new_vert = MOD_SOLIDIFY_EMPTY_TAG;
                      edge_groups[j + prior_index] = edge_group_a;

                      add_index++;
                      splits++;
                      edges = MEM_malloc_arrayN<NewEdgeRef *>(edges_len - uint(last_split),
                                                              __func__);
                      memcpy(edges,
                             g.edges + last_split,
                             (edges_len - uint(last_split)) * sizeof(*edges));

                      EdgeGroup edge_group_b{};
                      edge_group_b.valid = true;
                      edge_group_b.edges = edges;
                      edge_group_b.edges_len = (edges_len - uint(last_split));
                      edge_group_b.open_face_edge = MOD_SOLIDIFY_EMPTY_TAG;
                      edge_group_b.is_orig_closed = g.is_orig_closed;
                      edge_group_b.is_even_split = false;
                      edge_group_b.split = add_index - prior_index + 1;
                      edge_group_b.is_singularity = false;
                      edge_group_b.topo_group = g.topo_group;
                      zero_v3(edge_group_b.co);
                      zero_v3(edge_group_b.no);
                      edge_group_b.new_vert = MOD_SOLIDIFY_EMPTY_TAG;
                      edge_groups[j + add_index] = edge_group_b;
                    }
                    if (prior_splits != splits) {
                      MEM_freeN(g.edges);
                    }
                  }
                  if (first_unique_end != -1 && prior_splits == splits) {
                    has_singularities = true;
                    edge_groups[j + add_index].is_singularity = true;
                  }
                }
                MEM_freeN(doubles);
              }
            }
          }
        }

        orig_vert_groups_arr[i] = edge_groups;
        /* Count new edges, loops, faces and add to link_edge_groups. */
        {
          uint new_verts = 0;
          bool contains_open_splits = false;
          uint open_edges = 0;
          uint contains_splits = 0;
          uint last_added = 0;
          uint first_added = 0;
          bool first_set = false;
          for (EdgeGroup *g = edge_groups; g->valid; g++) {
            NewEdgeRef **e = g->edges;
            for (uint j = 0; j < g->edges_len; j++, e++) {
              const uint flip = uint(vm[orig_edges[(*e)->old_edge][1]] == i);
              BLI_assert(flip || vm[orig_edges[(*e)->old_edge][0]] == i);
              (*e)->link_edge_groups[flip] = g;
            }
            uint added = 0;
            if (do_shell || (do_rim && !g->is_orig_closed)) {
              BLI_assert(g->new_vert == MOD_SOLIDIFY_EMPTY_TAG);
              g->new_vert = new_verts_num++;
              if (do_rim || (do_shell && g->split)) {
                new_verts++;
                contains_splits += (g->split != 0);
                contains_open_splits |= g->split && !g->is_orig_closed;
                added = g->split;
              }
            }
            open_edges += uint(added < last_added);
            if (!first_set) {
              first_set = true;
              first_added = added;
            }
            last_added = added;
            if (!(g + 1)->valid || g->topo_group != (g + 1)->topo_group) {
              if (new_verts > 2) {
                new_faces_num++;
                new_edges_num += new_verts;
                open_edges += uint(first_added < last_added);
                open_edges -= uint(open_edges && !contains_open_splits);
                if (do_shell && do_rim) {
                  new_loops_num += new_verts * 2;
                }
                else if (do_shell) {
                  new_loops_num += new_verts * 2 - open_edges;
                }
                else {  // do_rim
                  new_loops_num += new_verts * 2 + open_edges - contains_splits;
                }
              }
              else if (new_verts == 2) {
                new_edges_num++;
                new_loops_num += 2u - uint(!(do_rim && do_shell) && contains_open_splits);
              }
              new_verts = 0;
              contains_open_splits = false;
              contains_splits = 0;
              open_edges = 0;
              last_added = 0;
              first_added = 0;
              first_set = false;
            }
          }
        }
      }
    }
  }

  /* Free vert_adj_edges memory. */
  {
    uint i = 0;
    for (OldVertEdgeRef **p = vert_adj_edges; i < verts_num; i++, p++) {
      if (*p) {
        MEM_freeN((*p)->edges);
        MEM_freeN(*p);
      }
    }
    MEM_freeN(vert_adj_edges);
  }

  /* TODO: create_regions if fix_intersections. */

  /* General use pointer for #EdgeGroup iteration. */
  EdgeGroup **gs_ptr;

  /* Calculate EdgeGroup vertex coordinates. */
  {
    float *face_weight = nullptr;

    if (do_flat_faces) {
      face_weight = MEM_malloc_arrayN<float>(faces_num, __func__);

      for (const int i : orig_faces.index_range()) {
        float scalar_vgroup = 1.0f;
        for (const int vert : orig_corner_verts.slice(orig_faces[i])) {
          const MDeformVert *dv = &dvert[vert];
          if (defgrp_invert) {
            scalar_vgroup = min_ff(1.0f - BKE_defvert_find_weight(dv, defgrp_index),
                                   scalar_vgroup);
          }
          else {
            scalar_vgroup = min_ff(BKE_defvert_find_weight(dv, defgrp_index), scalar_vgroup);
          }
        }
        scalar_vgroup = offset_fac_vg + (scalar_vgroup * offset_fac_vg_inv);
        face_weight[i] = scalar_vgroup;
      }
    }

    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < verts_num; i++, gs_ptr++) {
      if (*gs_ptr) {
        for (EdgeGroup *g = *gs_ptr; g->valid; g++) {
          if (!g->is_singularity) {
            float *nor = g->no;
            /* During vertex position calculation, the algorithm decides if it wants to disable the
             * boundary fix to maintain correct thickness. If the used algorithm does not produce a
             * free move direction (move_nor), it can use approximate_free_direction to decide on
             * a movement direction based on the connected edges. */
            float move_nor[3] = {0, 0, 0};
            bool disable_boundary_fix = (smd->nonmanifold_boundary_mode ==
                                             MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE ||
                                         (g->is_orig_closed || g->split));
            bool approximate_free_direction = false;
            /* Constraints Method. */
            if (smd->nonmanifold_offset_mode == MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS) {
              NewEdgeRef *first_edge = nullptr;
              NewEdgeRef **edge_ptr = g->edges;
              /* Contains normal and offset `[nx, ny, nz, ofs]`. */
              float (*planes_queue)[4] = MEM_malloc_arrayN<float[4]>(g->edges_len + 1, __func__);
              uint queue_index = 0;

              float fallback_nor[3];
              float fallback_ofs = 0.0f;

              const bool cycle = (g->is_orig_closed && !g->split) || g->is_even_split;
              for (uint k = 0; k < g->edges_len; k++, edge_ptr++) {
                if (!(k & 1) || (!cycle && k == g->edges_len - 1)) {
                  NewEdgeRef *edge = *edge_ptr;
                  for (uint l = 0; l < 2; l++) {
                    NewFaceRef *face = edge->faces[l];
                    if (face && (first_edge == nullptr ||
                                 (first_edge->faces[0] != face && first_edge->faces[1] != face)))
                    {
                      float ofs = face->reversed ? ofs_back_clamped : ofs_front_clamped;
                      /* Use face_weight here to make faces thinner. */
                      if (do_flat_faces) {
                        ofs *= face_weight[face->index];
                      }

                      if (!null_faces[face->index]) {
                        /* And plane to the queue. */
                        mul_v3_v3fl(planes_queue[queue_index],
                                    face_nors[face->index],
                                    face->reversed ? -1 : 1);
                        planes_queue[queue_index++][3] = ofs;
                      }
                      else {
                        /* Just use this approximate normal of the null face if there is no other
                         * normal to use. */
                        mul_v3_v3fl(fallback_nor, face_nors[face->index], face->reversed ? -1 : 1);
                        fallback_ofs = ofs;
                      }
                    }
                  }
                  if ((cycle && k == 0) || (!cycle && k + 3 >= g->edges_len)) {
                    first_edge = edge;
                  }
                }
              }
              if (queue_index > 2) {
                /* Find the two most different normals. */
                float min_p = 2.0f;
                uint min_n0 = 0;
                uint min_n1 = 0;
                for (uint k = 0; k < queue_index; k++) {
                  for (uint m = k + 1; m < queue_index; m++) {
                    float p = dot_v3v3(planes_queue[k], planes_queue[m]);
                    if (p < min_p) {
                      min_p = p;
                      min_n0 = k;
                      min_n1 = m;
                    }
                  }
                }
                /* Put the two found normals, first in the array queue. */
                if (min_n1 != 0) {
                  swap_v4_v4(planes_queue[min_n0], planes_queue[0]);
                  swap_v4_v4(planes_queue[min_n1], planes_queue[1]);
                }
                else {
                  swap_v4_v4(planes_queue[min_n0], planes_queue[1]);
                }
                /* Find the third most important/different normal. */
                min_p = 1.0f;
                min_n1 = 2;
                float max_p = -1.0f;
                for (uint k = 2; k < queue_index; k++) {
                  max_p = max_ff(dot_v3v3(planes_queue[0], planes_queue[k]),
                                 dot_v3v3(planes_queue[1], planes_queue[k]));
                  if (max_p <= min_p) {
                    min_p = max_p;
                    min_n1 = k;
                  }
                }
                swap_v4_v4(planes_queue[min_n1], planes_queue[2]);
              }
              /* Remove/average duplicate normals in planes_queue. */
              while (queue_index > 2) {
                uint best_n0 = 0;
                uint best_n1 = 0;
                float best_p = -1.0f;
                float best_ofs_diff = 0.0f;
                for (uint k = 0; k < queue_index; k++) {
                  for (uint m = k + 1; m < queue_index; m++) {
                    float p = dot_v3v3(planes_queue[m], planes_queue[k]);
                    float ofs_diff = fabsf(planes_queue[m][3] - planes_queue[k][3]);
                    if (p > best_p + FLT_EPSILON || (p >= best_p && ofs_diff < best_ofs_diff)) {
                      best_p = p;
                      best_ofs_diff = ofs_diff;
                      best_n0 = k;
                      best_n1 = m;
                    }
                  }
                }
                /* Make sure there are no equal planes. This threshold is crucial for the
                 * methods below to work without numerical issues. */
                if (best_p < 0.98f) {
                  break;
                }
                add_v3_v3(planes_queue[best_n0], planes_queue[best_n1]);
                normalize_v3(planes_queue[best_n0]);
                planes_queue[best_n0][3] = (planes_queue[best_n0][3] + planes_queue[best_n1][3]) *
                                           0.5f;
                queue_index--;
                memmove(planes_queue + best_n1,
                        planes_queue + best_n1 + 1,
                        (queue_index - best_n1) * sizeof(*planes_queue));
              }
              const uint size = queue_index;
              /* If there is more than 2 planes at this vertex, the boundary fix should be disabled
               * to stay at the correct thickness for all the faces. This is not very good in
               * practice though, since that will almost always disable the boundary fix. Instead
               * introduce a threshold which decides whether the boundary fix can be used without
               * major thickness changes. If the following constant is 1.0, it would always
               * prioritize correct thickness. At 0.7 the thickness is allowed to change a bit if
               * necessary for the fix (~10%). Note this only applies if a boundary fix is used. */
              const float boundary_fix_threshold = 0.7f;
              if (size > 3) {
                /* Use the most general least squares method to find the best position. */
                float mat[3][3];
                zero_m3(mat);
                for (int k = 0; k < 3; k++) {
                  for (int m = 0; m < size; m++) {
                    madd_v3_v3fl(mat[k], planes_queue[m], planes_queue[m][k]);
                  }
                  /* Add a small epsilon to ensure the invert is going to work.
                   * This addition makes the inverse more stable and the results
                   * seem to get more precise. */
                  mat[k][k] += 5e-5f;
                }
                /* NOTE: this matrix invert fails if there is less than 3 different normals. */
                invert_m3(mat);
                zero_v3(nor);
                for (int k = 0; k < size; k++) {
                  madd_v3_v3fl(nor, planes_queue[k], planes_queue[k][3]);
                }
                mul_v3_m3v3(nor, mat, nor);

                if (!disable_boundary_fix) {
                  /* Figure out if the approximate boundary fix can get use here. */
                  float greatest_angle_cos = 1.0f;
                  for (uint k = 0; k < 2; k++) {
                    for (uint m = 2; m < size; m++) {
                      float p = dot_v3v3(planes_queue[m], planes_queue[k]);
                      greatest_angle_cos = std::min(p, greatest_angle_cos);
                    }
                  }
                  if (greatest_angle_cos > boundary_fix_threshold) {
                    approximate_free_direction = true;
                  }
                  else {
                    disable_boundary_fix = true;
                  }
                }
              }
              else if (size > 1) {
                /* When up to 3 constraint normals are found, there is a simple solution. */
                const float stop_explosion = 0.999f - fabsf(smd->offset_fac) * 0.05f;
                const float q = dot_v3v3(planes_queue[0], planes_queue[1]);
                float d = 1.0f - q * q;
                cross_v3_v3v3(move_nor, planes_queue[0], planes_queue[1]);
                normalize_v3(move_nor);
                if (d > FLT_EPSILON * 10 && q < stop_explosion) {
                  d = 1.0f / d;
                  mul_v3_fl(planes_queue[0], (planes_queue[0][3] - planes_queue[1][3] * q) * d);
                  mul_v3_fl(planes_queue[1], (planes_queue[1][3] - planes_queue[0][3] * q) * d);
                }
                else {
                  d = 1.0f / (fabsf(q) + 1.0f);
                  mul_v3_fl(planes_queue[0], planes_queue[0][3] * d);
                  mul_v3_fl(planes_queue[1], planes_queue[1][3] * d);
                }
                add_v3_v3v3(nor, planes_queue[0], planes_queue[1]);
                if (size == 3) {
                  d = dot_v3v3(planes_queue[2], move_nor);
                  /* The following threshold ignores the third plane if it is almost orthogonal to
                   * the still free direction. */
                  if (fabsf(d) > 0.02f) {
                    float tmp[3];
                    madd_v3_v3v3fl(tmp, nor, planes_queue[2], -planes_queue[2][3]);
                    mul_v3_v3fl(tmp, move_nor, dot_v3v3(planes_queue[2], tmp) / d);
                    sub_v3_v3(nor, tmp);
                    /* Disable boundary fix if the constraints would be majorly unsatisfied. */
                    if (fabsf(d) > 1.0f - boundary_fix_threshold) {
                      disable_boundary_fix = true;
                    }
                  }
                }
                approximate_free_direction = false;
              }
              else if (size == 1) {
                /* Face corner case. */
                mul_v3_v3fl(nor, planes_queue[0], planes_queue[0][3]);
                if (g->edges_len > 2) {
                  disable_boundary_fix = true;
                  approximate_free_direction = true;
                }
              }
              else {
                /* Fallback case for null faces. */
                mul_v3_v3fl(nor, fallback_nor, fallback_ofs);
                disable_boundary_fix = true;
              }
              MEM_freeN(planes_queue);
            }
            /* Fixed/Even Method. */
            else {
              float total_angle = 0;
              float total_angle_back = 0;
              NewEdgeRef *first_edge = nullptr;
              NewEdgeRef **edge_ptr = g->edges;
              float face_nor[3];
              float nor_back[3] = {0, 0, 0};
              bool has_back = false;
              bool has_front = false;
              bool cycle = (g->is_orig_closed && !g->split) || g->is_even_split;
              for (uint k = 0; k < g->edges_len; k++, edge_ptr++) {
                if (!(k & 1) || (!cycle && k == g->edges_len - 1)) {
                  NewEdgeRef *edge = *edge_ptr;
                  for (uint l = 0; l < 2; l++) {
                    NewFaceRef *face = edge->faces[l];
                    if (face && (first_edge == nullptr ||
                                 (first_edge->faces[0] != face && first_edge->faces[1] != face)))
                    {
                      float angle = 1.0f;
                      float ofs = face->reversed ? -ofs_back_clamped : ofs_front_clamped;
                      /* Use face_weight here to make faces thinner. */
                      if (do_flat_faces) {
                        ofs *= face_weight[face->index];
                      }

                      if (smd->nonmanifold_offset_mode ==
                          MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN)
                      {
                        int corner_next = face->face.start();
                        int corner = corner_next + (face->face.size() - 1);
                        int corner_prev = corner - 1;

                        for (int m = 0;
                             m < face->face.size() && vm[orig_corner_verts[corner]] != i;
                             m++, corner_next++)
                        {
                          corner_prev = corner;
                          corner = corner_next;
                        }
                        angle = angle_v3v3v3(orig_mvert_co[vm[orig_corner_verts[corner_prev]]],
                                             orig_mvert_co[i],
                                             orig_mvert_co[vm[orig_corner_verts[corner_next]]]);
                        if (face->reversed) {
                          total_angle_back += angle * ofs * ofs;
                        }
                        else {
                          total_angle += angle * ofs * ofs;
                        }
                      }
                      else {
                        if (face->reversed) {
                          total_angle_back++;
                        }
                        else {
                          total_angle++;
                        }
                      }
                      mul_v3_v3fl(face_nor, face_nors[face->index], angle * ofs);
                      if (face->reversed) {
                        add_v3_v3(nor_back, face_nor);
                        has_back = true;
                      }
                      else {
                        add_v3_v3(nor, face_nor);
                        has_front = true;
                      }
                    }
                  }
                  if ((cycle && k == 0) || (!cycle && k + 3 >= g->edges_len)) {
                    first_edge = edge;
                  }
                }
              }

              /* Set normal length with selected method. */
              if (smd->nonmanifold_offset_mode == MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN) {
                if (has_front) {
                  float length_sq = len_squared_v3(nor);
                  if (LIKELY(length_sq > FLT_EPSILON)) {
                    mul_v3_fl(nor, total_angle / length_sq);
                  }
                }
                if (has_back) {
                  float length_sq = len_squared_v3(nor_back);
                  if (LIKELY(length_sq > FLT_EPSILON)) {
                    mul_v3_fl(nor_back, total_angle_back / length_sq);
                  }
                  if (!has_front) {
                    copy_v3_v3(nor, nor_back);
                  }
                }
                if (has_front && has_back) {
                  float nor_length = len_v3(nor);
                  float nor_back_length = len_v3(nor_back);
                  float q = dot_v3v3(nor, nor_back);
                  if (LIKELY(fabsf(q) > FLT_EPSILON)) {
                    q /= nor_length * nor_back_length;
                  }
                  float d = 1.0f - q * q;
                  if (LIKELY(d > FLT_EPSILON)) {
                    d = 1.0f / d;
                    if (LIKELY(nor_length > FLT_EPSILON)) {
                      mul_v3_fl(nor, (1 - nor_back_length * q / nor_length) * d);
                    }
                    if (LIKELY(nor_back_length > FLT_EPSILON)) {
                      mul_v3_fl(nor_back, (1 - nor_length * q / nor_back_length) * d);
                    }
                    add_v3_v3(nor, nor_back);
                  }
                  else {
                    mul_v3_fl(nor, 0.5f);
                    mul_v3_fl(nor_back, 0.5f);
                    add_v3_v3(nor, nor_back);
                  }
                }
              }
              else {
                if (has_front && total_angle > FLT_EPSILON) {
                  mul_v3_fl(nor, 1.0f / total_angle);
                }
                if (has_back && total_angle_back > FLT_EPSILON) {
                  mul_v3_fl(nor_back, 1.0f / total_angle_back);
                  add_v3_v3(nor, nor_back);
                  if (has_front && total_angle > FLT_EPSILON) {
                    mul_v3_fl(nor, 0.5f);
                  }
                }
              }
              /* Set move_nor for boundary fix. */
              if (!disable_boundary_fix && g->edges_len > 2) {
                approximate_free_direction = true;
              }
              else {
                disable_boundary_fix = true;
              }
            }
            if (approximate_free_direction) {
              /* Set move_nor for boundary fix. */
              NewEdgeRef **edge_ptr = g->edges + 1;
              float tmp[3];
              int k;
              for (k = 1; k + 1 < g->edges_len; k++, edge_ptr++) {
                const int2 &edge = orig_edges[(*edge_ptr)->old_edge];
                sub_v3_v3v3(
                    tmp, orig_mvert_co[vm[edge[0]] == i ? edge[1] : edge[0]], orig_mvert_co[i]);
                add_v3_v3(move_nor, tmp);
              }
              if (k == 1) {
                disable_boundary_fix = true;
              }
              else {
                disable_boundary_fix = normalize_v3(move_nor) == 0.0f;
              }
            }
            /* Fix boundary verts. */
            if (!disable_boundary_fix) {
              /* Constraint normal, nor * constr_nor == 0 after this fix. */
              float constr_nor[3];
              const int2 &e0_edge = orig_edges[g->edges[0]->old_edge];
              const int2 &e1_edge = orig_edges[g->edges[g->edges_len - 1]->old_edge];
              float e0[3];
              float e1[3];
              sub_v3_v3v3(e0,
                          orig_mvert_co[vm[e0_edge[0]] == i ? e0_edge[1] : e0_edge[0]],
                          orig_mvert_co[i]);
              sub_v3_v3v3(e1,
                          orig_mvert_co[vm[e1_edge[0]] == i ? e1_edge[1] : e1_edge[0]],
                          orig_mvert_co[i]);
              if (smd->nonmanifold_boundary_mode == MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT) {
                cross_v3_v3v3(constr_nor, e0, e1);
                normalize_v3(constr_nor);
              }
              else {
                BLI_assert(smd->nonmanifold_boundary_mode ==
                           MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_ROUND);
                float f0[3];
                float f1[3];
                if (g->edges[0]->faces[0]->reversed) {
                  negate_v3_v3(f0, face_nors[g->edges[0]->faces[0]->index]);
                }
                else {
                  copy_v3_v3(f0, face_nors[g->edges[0]->faces[0]->index]);
                }
                if (g->edges[g->edges_len - 1]->faces[0]->reversed) {
                  negate_v3_v3(f1, face_nors[g->edges[g->edges_len - 1]->faces[0]->index]);
                }
                else {
                  copy_v3_v3(f1, face_nors[g->edges[g->edges_len - 1]->faces[0]->index]);
                }
                float n0[3];
                float n1[3];
                cross_v3_v3v3(n0, e0, f0);
                cross_v3_v3v3(n1, f1, e1);
                normalize_v3(n0);
                normalize_v3(n1);
                add_v3_v3v3(constr_nor, n0, n1);
                normalize_v3(constr_nor);
              }
              float d = dot_v3v3(constr_nor, move_nor);
              /* Only allow the thickness to increase about 10 times. */
              if (fabsf(d) > 0.1f) {
                mul_v3_fl(move_nor, dot_v3v3(constr_nor, nor) / d);
                sub_v3_v3(nor, move_nor);
              }
            }
            float scalar_vgroup = 1;
            /* Use vertex group. */
            if (dvert && !do_flat_faces) {
              const MDeformVert *dv = &dvert[i];
              if (defgrp_invert) {
                scalar_vgroup = 1.0f - BKE_defvert_find_weight(dv, defgrp_index);
              }
              else {
                scalar_vgroup = BKE_defvert_find_weight(dv, defgrp_index);
              }
              scalar_vgroup = offset_fac_vg + (scalar_vgroup * offset_fac_vg_inv);
            }
            /* Do clamping. */
            if (do_clamp) {
              if (do_angle_clamp) {
                if (g->edges_len > 2) {
                  float min_length = 0;
                  float angle = 0.5f * M_PI;
                  uint k = 0;
                  for (NewEdgeRef **p = g->edges; k < g->edges_len; k++, p++) {
                    float length = orig_edge_lengths[(*p)->old_edge];
                    float e_ang = (*p)->angle;
                    angle = std::max(e_ang, angle);
                    if (length < min_length || k == 0) {
                      min_length = length;
                    }
                  }
                  float cos_ang = cosf(angle * 0.5f);
                  if (cos_ang > 0) {
                    float max_off = min_length * 0.5f / cos_ang;
                    if (max_off < offset * 0.5f) {
                      scalar_vgroup *= max_off / offset * 2;
                    }
                  }
                }
              }
              else {
                float min_length = 0;
                uint k = 0;
                for (NewEdgeRef **p = g->edges; k < g->edges_len; k++, p++) {
                  float length = orig_edge_lengths[(*p)->old_edge];
                  if (length < min_length || k == 0) {
                    min_length = length;
                  }
                }
                if (min_length < offset) {
                  scalar_vgroup *= min_length / offset;
                }
              }
            }
            mul_v3_fl(nor, scalar_vgroup);
            add_v3_v3v3(g->co, nor, orig_mvert_co[i]);
          }
          else {
            copy_v3_v3(g->co, orig_mvert_co[i]);
          }
        }
      }
    }

    if (do_flat_faces) {
      MEM_freeN(face_weight);
    }
  }

  MEM_freeN(orig_mvert_co);
  if (null_faces) {
    MEM_freeN(null_faces);
  }

  /* TODO: create vertdata for intersection fixes (intersection fixing per topology region). */

  /* Correction for adjacent one sided groups around a vert to
   * prevent edge duplicates and null faces. */
  uint(*singularity_edges)[2] = nullptr;
  uint totsingularity = 0;
  if (has_singularities) {
    has_singularities = false;
    uint i = 0;
    uint singularity_edges_len = 1;
    singularity_edges = MEM_malloc_arrayN<uint[2]>(singularity_edges_len, __func__);
    for (NewEdgeRef ***new_edges = orig_edge_data_arr; i < edges_num; i++, new_edges++) {
      if (*new_edges && (do_shell || edge_adj_faces_len[i] == 1) && (**new_edges)->old_edge == i) {
        for (NewEdgeRef **l = *new_edges; *l; l++) {
          if ((*l)->link_edge_groups[0]->is_singularity &&
              (*l)->link_edge_groups[1]->is_singularity)
          {
            const uint v1 = (*l)->link_edge_groups[0]->new_vert;
            const uint v2 = (*l)->link_edge_groups[1]->new_vert;
            bool exists_already = false;
            uint j = 0;
            for (uint(*p)[2] = singularity_edges; j < totsingularity; p++, j++) {
              if (((*p)[0] == v1 && (*p)[1] == v2) || ((*p)[0] == v2 && (*p)[1] == v1)) {
                exists_already = true;
                break;
              }
            }
            if (!exists_already) {
              has_singularities = true;
              if (singularity_edges_len <= totsingularity) {
                singularity_edges_len = totsingularity + 1;
                singularity_edges = static_cast<uint(*)[2]>(
                    MEM_reallocN_id(singularity_edges,
                                    singularity_edges_len * sizeof(*singularity_edges),
                                    __func__));
              }
              singularity_edges[totsingularity][0] = v1;
              singularity_edges[totsingularity][1] = v2;
              totsingularity++;
              if (edge_adj_faces_len[i] == 1 && do_rim) {
                new_loops_num -= 2;
                new_faces_num--;
              }
            }
            else {
              new_edges_num--;
            }
          }
        }
      }
    }
  }

  /* Create Mesh *result with proper capacity. */
  result = BKE_mesh_new_nomain_from_template(
      mesh, int(new_verts_num), int(new_edges_num), int(new_faces_num), int(new_loops_num));

  blender::MutableSpan<float3> vert_positions = result->vert_positions_for_write();
  blender::MutableSpan<int2> edges = result->edges_for_write();
  blender::MutableSpan<int> face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> corner_edges = result->corner_edges_for_write();
  bke::MutableAttributeAccessor result_attributes = result->attributes_for_write();

  int *origindex_edge = static_cast<int *>(
      CustomData_get_layer_for_write(&result->edge_data, CD_ORIGINDEX, result->edges_num));
  int *origindex_face = static_cast<int *>(
      CustomData_get_layer_for_write(&result->face_data, CD_ORIGINDEX, result->faces_num));

  bke::SpanAttributeWriter<float> result_edge_bweight;
  if (orig_attributes.contains("bevel_weight_edge") ||
      (bevel_convex != 0.0f || !orig_vert_bweight.is_empty()))
  {
    result_edge_bweight = result_attributes.lookup_or_add_for_write_span<float>(
        "bevel_weight_edge", blender::bke::AttrDomain::Edge);
  }

  /* Checks that result has dvert data. */
  MDeformVert *dst_dvert = nullptr;
  if (shell_defgrp_index != -1 || rim_defgrp_index != -1) {
    dst_dvert = result->deform_verts_for_write().data();
  }

  /* Get vertex crease layer and ensure edge creases are active if vertex creases are found, since
   * they will introduce edge creases in the used custom interpolation method. */
  const VArraySpan vertex_crease = *orig_attributes.lookup<float>("crease_vert",
                                                                  bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> result_edge_crease;

  if (!vertex_crease.is_empty() || !orig_edge_crease.is_empty()) {
    result_edge_crease = result_attributes.lookup_or_add_for_write_span<float>(
        "crease_edge", blender::bke::AttrDomain::Edge);
    /* delete all vertex creases in the result if a rim is used. */
    if (do_rim) {
      result_attributes.remove("crease_vert");
    }
  }

  /* Make_new_verts. */
  {
    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < verts_num; i++, gs_ptr++) {
      EdgeGroup *gs = *gs_ptr;
      if (gs) {
        for (EdgeGroup *g = gs; g->valid; g++) {
          if (g->new_vert != MOD_SOLIDIFY_EMPTY_TAG) {
            CustomData_copy_data(
                &mesh->vert_data, &result->vert_data, int(i), int(g->new_vert), 1);
            copy_v3_v3(vert_positions[g->new_vert], g->co);
          }
        }
      }
    }
  }

  /* Make edges. */
  {
    uint i = 0;
    edge_index += totsingularity;
    for (NewEdgeRef ***new_edges = orig_edge_data_arr; i < edges_num; i++, new_edges++) {
      if (*new_edges && (do_shell || edge_adj_faces_len[i] == 1) && (**new_edges)->old_edge == i) {
        for (NewEdgeRef **l = *new_edges; *l; l++) {
          if ((*l)->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
            const uint v1 = (*l)->link_edge_groups[0]->new_vert;
            const uint v2 = (*l)->link_edge_groups[1]->new_vert;
            uint insert = edge_index;
            if (has_singularities && ((*l)->link_edge_groups[0]->is_singularity &&
                                      (*l)->link_edge_groups[1]->is_singularity))
            {
              uint j = 0;
              for (uint(*p)[2] = singularity_edges; j < totsingularity; p++, j++) {
                if (((*p)[0] == v1 && (*p)[1] == v2) || ((*p)[0] == v2 && (*p)[1] == v1)) {
                  insert = j;
                  break;
                }
              }
              BLI_assert(insert == j);
            }
            else {
              edge_index++;
            }
            CustomData_copy_data(&mesh->edge_data, &result->edge_data, int(i), int(insert), 1);
            BLI_assert(v1 != MOD_SOLIDIFY_EMPTY_TAG);
            BLI_assert(v2 != MOD_SOLIDIFY_EMPTY_TAG);
            edges[insert][0] = v1;
            edges[insert][1] = v2;
            if (result_edge_crease) {
              result_edge_crease.span[insert] = !orig_edge_crease.is_empty() ?
                                                    orig_edge_crease[(*l)->old_edge] :
                                                    0.0f;
            }
            if (result_edge_bweight) {
              result_edge_bweight.span[insert] = !orig_edge_bweight.is_empty() ?
                                                     orig_edge_bweight[(*l)->old_edge] :
                                                     0.0f;
              if (bevel_convex != 0.0f && (*l)->faces[1] != nullptr) {
                result_edge_bweight.span[insert] = clamp_f(
                    result_edge_bweight.span[insert] +
                        ((*l)->angle > M_PI + FLT_EPSILON ?
                             clamp_f(bevel_convex, 0.0f, 1.0f) :
                             ((*l)->angle < M_PI - FLT_EPSILON ?
                                  clamp_f(bevel_convex, -1.0f, 0.0f) :
                                  0)),
                    0.0f,
                    1.0f);
              }
            }
            (*l)->new_edge = insert;
          }
        }
      }
    }
  }
  if (singularity_edges) {
    MEM_freeN(singularity_edges);
  }

  /* DEBUG CODE FOR BUG-FIXING (can not be removed because every bug-fix needs this badly!). */
#if 0
  {
    /* this code will output the content of orig_vert_groups_arr.
     * in orig_vert_groups_arr these conditions must be met for every vertex:
     * - new_edge value should have no duplicates
     * - every old_edge value should appear twice
     * - every group should have at least two members (edges)
     * NOTE: that there can be vertices that only have one group. They are called singularities.
     * These vertices will only have one side (there is no way of telling apart front
     * from back like on a mobius strip)
     */

    /* Debug output format:
     * <original vertex id>:
     * {
     *   { <old edge id>/<new edge id>, } \
     *     (tg:<topology group id>)(s:<is split group>,c:<is closed group (before splitting)>)
     * }
     */
    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < verts_num; i++, gs_ptr++) {
      EdgeGroup *gs = *gs_ptr;
      /* check if the vertex is present (may be dissolved because of proximity) */
      if (gs) {
        printf("%d:\n", i);
        for (EdgeGroup *g = gs; g->valid; g++) {
          NewEdgeRef **e = g->edges;
          for (uint j = 0; j < g->edges_len; j++, e++) {
            printf("%u/%d, ", (*e)->old_edge, int(*e)->new_edge);
          }
          printf("(tg:%u)(s:%u,c:%d)\n", g->topo_group, g->split, g->is_orig_closed);
        }
      }
    }
  }
#endif
  const VArraySpan src_material_index = *orig_attributes.lookup<int>("material_index",
                                                                     bke::AttrDomain::Face);
  bke::SpanAttributeWriter dst_material_index =
      result_attributes.lookup_or_add_for_write_span<int>("material_index", bke::AttrDomain::Face);

  /* Make boundary edges/faces. */
  {
    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < verts_num; i++, gs_ptr++) {
      EdgeGroup *gs = *gs_ptr;
      if (gs) {
        EdgeGroup *g = gs;
        EdgeGroup *g2 = gs;
        EdgeGroup *last_g = nullptr;
        EdgeGroup *first_g = nullptr;
        float mv_crease = !vertex_crease.is_empty() ? vertex_crease[i] : 0.0f;
        float mv_bweight = !orig_vert_bweight.is_empty() ? orig_vert_bweight[i] : 0.0f;
        /* Data calculation cache. */
        float max_crease;
        float last_max_crease = 0.0f;
        float first_max_crease = 0.0f;
        float max_bweight;
        float last_max_bweight = 0.0f;
        float first_max_bweight = 0.0f;
        for (uint j = 0; g->valid; g++) {
          if ((do_rim && !g->is_orig_closed) || (do_shell && g->split)) {
            max_crease = 0;
            max_bweight = 0;

            BLI_assert(g->edges_len >= 2);

            if (g->edges_len == 2) {
              if (result_edge_crease) {
                if (!orig_edge_crease.is_empty()) {
                  max_crease = min_ff(orig_edge_crease[g->edges[0]->old_edge],
                                      orig_edge_crease[g->edges[1]->old_edge]);
                }
                else {
                  max_crease = 0.0f;
                }
              }
            }
            else {
              for (uint k = 1; k < g->edges_len - 1; k++) {
                const uint orig_edge_index = g->edges[k]->old_edge;
                if (result_edge_crease) {
                  if (!orig_edge_crease.is_empty() &&
                      orig_edge_crease[orig_edge_index] > max_crease)
                  {
                    max_crease = orig_edge_crease[orig_edge_index];
                  }
                }
                if (g->edges[k]->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
                  if (result_edge_bweight) {
                    float bweight = result_edge_bweight.span[g->edges[k]->new_edge];
                    max_bweight = std::max(bweight, max_bweight);
                  }
                }
              }
            }

            const float bweight_open_edge =
                !orig_edge_bweight.is_empty() ?
                    min_ff(orig_edge_bweight[g->edges[0]->old_edge],
                           orig_edge_bweight[g->edges[g->edges_len - 1]->old_edge]) :
                    0.0f;
            if (bweight_open_edge > 0) {
              max_bweight = min_ff(bweight_open_edge, max_bweight);
            }
            else {
              if (bevel_convex < 0.0f) {
                max_bweight = 0;
              }
            }
            if (!first_g) {
              first_g = g;
              first_max_crease = max_crease;
              first_max_bweight = max_bweight;
            }
            else {
              last_g->open_face_edge = edge_index;
              CustomData_copy_data(&mesh->edge_data,
                                   &result->edge_data,
                                   int(last_g->edges[0]->old_edge),
                                   int(edge_index),
                                   1);
              if (origindex_edge) {
                origindex_edge[edge_index] = ORIGINDEX_NONE;
              }
              edges[edge_index][0] = last_g->new_vert;
              edges[edge_index][1] = g->new_vert;
              if (result_edge_crease) {
                result_edge_crease.span[edge_index] = max_ff(mv_crease,
                                                             min_ff(last_max_crease, max_crease));
              }
              if (result_edge_bweight) {
                result_edge_bweight.span[edge_index] = max_ff(
                    mv_bweight, min_ff(last_max_bweight, max_bweight));
              }
              edge_index++;
            }
            last_g = g;
            last_max_crease = max_crease;
            last_max_bweight = max_bweight;
            j++;
          }
          if (!(g + 1)->valid || g->topo_group != (g + 1)->topo_group) {
            if (j == 2) {
              last_g->open_face_edge = edge_index - 1;
            }
            if (j > 2) {
              CustomData_copy_data(&mesh->edge_data,
                                   &result->edge_data,
                                   int(last_g->edges[0]->old_edge),
                                   int(edge_index),
                                   1);
              if (origindex_edge) {
                origindex_edge[edge_index] = ORIGINDEX_NONE;
              }
              last_g->open_face_edge = edge_index;
              edges[edge_index][0] = last_g->new_vert;
              edges[edge_index][1] = first_g->new_vert;
              if (result_edge_crease) {
                result_edge_crease.span[edge_index] = max_ff(
                    mv_crease, min_ff(last_max_crease, first_max_crease));
              }
              if (result_edge_bweight) {
                result_edge_bweight.span[edge_index] = max_ff(
                    mv_bweight, min_ff(last_max_bweight, first_max_bweight));
              }
              edge_index++;

              /* Loop data. */
              int *loops_data = MEM_malloc_arrayN<int>(j, __func__);
              /* The result material index is from consensus. */
              short most_mat_nr = 0;
              uint most_mat_nr_face = 0;
              uint most_mat_nr_count = 0;
              for (short l = 0; l < mat_nrs; l++) {
                uint count = 0;
                uint face = 0;
                uint k = 0;
                for (EdgeGroup *g3 = g2; g3->valid && k < j; g3++) {
                  if ((do_rim && !g3->is_orig_closed) || (do_shell && g3->split)) {
                    /* Check both far ends in terms of faces of an edge group. */
                    if ((!src_material_index.is_empty() ?
                             src_material_index[g3->edges[0]->faces[0]->index] :
                             0) == l)
                    {
                      face = g3->edges[0]->faces[0]->index;
                      count++;
                    }
                    NewEdgeRef *le = g3->edges[g3->edges_len - 1];
                    if (le->faces[1] &&
                        (!src_material_index.is_empty() ? src_material_index[le->faces[1]->index] :
                                                          0) == l)
                    {
                      face = le->faces[1]->index;
                      count++;
                    }
                    else if (!le->faces[1] && (!src_material_index.is_empty() ?
                                                   src_material_index[le->faces[0]->index] :
                                                   0) == l)
                    {
                      face = le->faces[0]->index;
                      count++;
                    }
                    k++;
                  }
                }
                if (count > most_mat_nr_count) {
                  most_mat_nr = l;
                  most_mat_nr_face = face;
                  most_mat_nr_count = count;
                }
              }
              CustomData_copy_data(
                  &mesh->face_data, &result->face_data, int(most_mat_nr_face), int(face_index), 1);
              if (origindex_face) {
                origindex_face[face_index] = ORIGINDEX_NONE;
              }
              face_offsets[face_index] = int(loop_index);
              dst_material_index.span[face_index] = most_mat_nr + (g->is_orig_closed || !do_rim ?
                                                                       0 :
                                                                       mat_ofs_rim);
              CLAMP(dst_material_index.span[face_index], 0, mat_nr_max);
              face_index++;

              for (uint k = 0; g2->valid && k < j; g2++) {
                if ((do_rim && !g2->is_orig_closed) || (do_shell && g2->split)) {
                  const blender::IndexRange face = g2->edges[0]->faces[0]->face;
                  for (int l = 0; l < face.size(); l++) {
                    const int vert = orig_corner_verts[face[l]];
                    if (vm[vert] == i) {
                      loops_data[k] = face[l];
                      break;
                    }
                  }
                  k++;
                }
              }

              if (!do_flip) {
                for (uint k = 0; k < j; k++) {
                  CustomData_copy_data(
                      &mesh->corner_data, &result->corner_data, loops_data[k], int(loop_index), 1);
                  corner_verts[loop_index] = edges[edge_index - j + k][0];
                  corner_edges[loop_index++] = edge_index - j + k;
                }
              }
              else {
                for (uint k = 1; k <= j; k++) {
                  CustomData_copy_data(&mesh->corner_data,
                                       &result->corner_data,
                                       loops_data[j - k],
                                       int(loop_index),
                                       1);
                  corner_verts[loop_index] = edges[edge_index - k][1];
                  corner_edges[loop_index++] = edge_index - k;
                }
              }
              MEM_freeN(loops_data);
            }
            /* Reset everything for the next face. */
            j = 0;
            last_g = nullptr;
            first_g = nullptr;
            last_max_crease = 0;
            first_max_crease = 0;
            last_max_bweight = 0;
            first_max_bweight = 0;
          }
        }
      }
    }
  }

  /* Make boundary faces. */
  if (do_rim) {
    for (uint i = 0; i < edges_num; i++) {
      if (edge_adj_faces_len[i] == 1 && orig_edge_data_arr[i] &&
          (*orig_edge_data_arr[i])->old_edge == i)
      {
        NewEdgeRef **new_edges = orig_edge_data_arr[i];

        NewEdgeRef *edge1 = new_edges[0];
        NewEdgeRef *edge2 = new_edges[1];
        const bool v1_singularity = edge1->link_edge_groups[0]->is_singularity &&
                                    edge2->link_edge_groups[0]->is_singularity;
        const bool v2_singularity = edge1->link_edge_groups[1]->is_singularity &&
                                    edge2->link_edge_groups[1]->is_singularity;
        if (v1_singularity && v2_singularity) {
          continue;
        }

        const uint orig_face_index = (*new_edges)->faces[0]->index;
        const blender::IndexRange face = (*new_edges)->faces[0]->face;
        CustomData_copy_data(&mesh->face_data,
                             &result->face_data,
                             int((*new_edges)->faces[0]->index),
                             int(face_index),
                             1);
        face_offsets[face_index] = int(loop_index);
        dst_material_index.span[face_index] = (!src_material_index.is_empty() ?
                                                   src_material_index[orig_face_index] :
                                                   0) +
                                              mat_ofs_rim;
        CLAMP(dst_material_index.span[face_index], 0, mat_nr_max);
        face_index++;

        int loop1 = -1;
        int loop2 = -1;
        const uint old_v1 = vm[orig_edges[edge1->old_edge][0]];
        const uint old_v2 = vm[orig_edges[edge1->old_edge][1]];
        for (uint j = 0; j < face.size(); j++) {
          const int vert = orig_corner_verts[face.start() + j];
          if (vm[vert] == old_v1) {
            loop1 = face.start() + int(j);
          }
          else if (vm[vert] == old_v2) {
            loop2 = face.start() + int(j);
          }
        }
        BLI_assert(loop1 != -1 && loop2 != -1);
        int2 open_face_edge;
        uint open_face_edge_index;
        if (!do_flip) {
          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&dst_dvert[edges[edge1->new_edge][0]], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(
              &mesh->corner_data, &result->corner_data, loop1, int(loop_index), 1);
          corner_verts[loop_index] = edges[edge1->new_edge][0];
          corner_edges[loop_index++] = edge1->new_edge;

          if (!v2_singularity) {
            open_face_edge_index = edge1->link_edge_groups[1]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&dst_dvert[edges[edge1->new_edge][1]], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(
                &mesh->corner_data, &result->corner_data, loop2, int(loop_index), 1);
            corner_verts[loop_index] = edges[edge1->new_edge][1];
            open_face_edge = edges[open_face_edge_index];
            if (ELEM(edges[edge2->new_edge][1], open_face_edge[0], open_face_edge[1])) {
              corner_edges[loop_index++] = open_face_edge_index;
            }
            else {
              corner_edges[loop_index++] = edge2->link_edge_groups[1]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&dst_dvert[edges[edge2->new_edge][1]], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(
              &mesh->corner_data, &result->corner_data, loop2, int(loop_index), 1);
          corner_verts[loop_index] = edges[edge2->new_edge][1];
          corner_edges[loop_index++] = edge2->new_edge;

          if (!v1_singularity) {
            open_face_edge_index = edge2->link_edge_groups[0]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&dst_dvert[edges[edge2->new_edge][0]], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(
                &mesh->corner_data, &result->corner_data, loop1, int(loop_index), 1);
            corner_verts[loop_index] = edges[edge2->new_edge][0];
            open_face_edge = edges[open_face_edge_index];
            if (ELEM(edges[edge1->new_edge][0], open_face_edge[0], open_face_edge[1])) {
              corner_edges[loop_index++] = open_face_edge_index;
            }
            else {
              corner_edges[loop_index++] = edge1->link_edge_groups[0]->open_face_edge;
            }
          }
        }
        else {
          if (!v1_singularity) {
            open_face_edge_index = edge1->link_edge_groups[0]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&dst_dvert[edges[edge1->new_edge][0]], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(
                &mesh->corner_data, &result->corner_data, loop1, int(loop_index), 1);
            corner_verts[loop_index] = edges[edge1->new_edge][0];
            open_face_edge = edges[open_face_edge_index];
            if (ELEM(edges[edge2->new_edge][0], open_face_edge[0], open_face_edge[1])) {
              corner_edges[loop_index++] = open_face_edge_index;
            }
            else {
              corner_edges[loop_index++] = edge2->link_edge_groups[0]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&dst_dvert[edges[edge2->new_edge][0]], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(
              &mesh->corner_data, &result->corner_data, loop1, int(loop_index), 1);
          corner_verts[loop_index] = edges[edge2->new_edge][0];
          corner_edges[loop_index++] = edge2->new_edge;

          if (!v2_singularity) {
            open_face_edge_index = edge2->link_edge_groups[1]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&dst_dvert[edges[edge2->new_edge][1]], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(
                &mesh->corner_data, &result->corner_data, loop2, int(loop_index), 1);
            corner_verts[loop_index] = edges[edge2->new_edge][1];
            open_face_edge = edges[open_face_edge_index];
            if (ELEM(edges[edge1->new_edge][1], open_face_edge[0], open_face_edge[1])) {
              corner_edges[loop_index++] = open_face_edge_index;
            }
            else {
              corner_edges[loop_index++] = edge1->link_edge_groups[1]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&dst_dvert[edges[edge1->new_edge][1]], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(
              &mesh->corner_data, &result->corner_data, loop2, int(loop_index), 1);
          corner_verts[loop_index] = edges[edge1->new_edge][1];
          corner_edges[loop_index++] = edge1->new_edge;
        }
      }
    }
  }

  /* Make faces. */
  if (do_shell) {
    uint *face_loops = MEM_malloc_arrayN<uint>(largest_ngon * 2, __func__);
    uint *face_verts = MEM_malloc_arrayN<uint>(largest_ngon * 2, __func__);
    uint *face_edges = MEM_malloc_arrayN<uint>(largest_ngon * 2, __func__);
    for (uint i = 0; i < faces_num * 2; i++) {
      NewFaceRef &fr = face_sides_arr[i];
      const uint loopstart = uint(fr.face.start());
      uint totloop = uint(fr.face.size());
      uint valid_edges = 0;
      uint k = 0;
      while (totloop > 0 && (!fr.link_edges[totloop - 1] ||
                             fr.link_edges[totloop - 1]->new_edge == MOD_SOLIDIFY_EMPTY_TAG))
      {
        totloop--;
      }
      if (totloop > 0) {
        NewEdgeRef *prior_edge = fr.link_edges[totloop - 1];
        uint prior_flip = uint(vm[orig_edges[prior_edge->old_edge][0]] ==
                               vm[orig_corner_verts[loopstart + (totloop - 1)]]);
        for (uint j = 0; j < totloop; j++) {
          NewEdgeRef *new_edge = fr.link_edges[j];
          if (new_edge && new_edge->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
            valid_edges++;
            const uint flip = uint(vm[orig_edges[new_edge->old_edge][1]] ==
                                   vm[orig_corner_verts[loopstart + j]]);
            BLI_assert(flip || vm[orig_edges[new_edge->old_edge][0]] ==
                                   vm[orig_corner_verts[loopstart + j]]);
            /* The vert that's in the current loop. */
            const uint new_v1 = new_edge->link_edge_groups[flip]->new_vert;
            /* The vert that's in the next loop. */
            const uint new_v2 = new_edge->link_edge_groups[1 - flip]->new_vert;
            if (k == 0 || face_verts[k - 1] != new_v1) {
              face_loops[k] = loopstart + j;
              if (fr.reversed) {
                face_edges[k] = prior_edge->link_edge_groups[prior_flip]->open_face_edge;
              }
              else {
                face_edges[k] = new_edge->link_edge_groups[flip]->open_face_edge;
              }
              BLI_assert(k == 0 || edges[face_edges[k]][1] == face_verts[k - 1] ||
                         edges[face_edges[k]][0] == face_verts[k - 1]);
              BLI_assert(face_edges[k] == MOD_SOLIDIFY_EMPTY_TAG ||
                         edges[face_edges[k]][1] == new_v1 || edges[face_edges[k]][0] == new_v1);
              face_verts[k++] = new_v1;
            }
            prior_edge = new_edge;
            prior_flip = 1 - flip;
            if (j < totloop - 1 || face_verts[0] != new_v2) {
              face_loops[k] = loopstart + (j + 1) % totloop;
              face_edges[k] = new_edge->new_edge;
              face_verts[k++] = new_v2;
            }
            else {
              face_edges[0] = new_edge->new_edge;
            }
          }
        }
        if (k > 2 && valid_edges > 2) {
          CustomData_copy_data(
              &mesh->face_data, &result->face_data, int(i / 2), int(face_index), 1);
          face_offsets[face_index] = int(loop_index);
          dst_material_index.span[face_index] = (!src_material_index.is_empty() ?
                                                     src_material_index[fr.index] :
                                                     0) +
                                                (fr.reversed != do_flip ? mat_ofs : 0);
          CLAMP(dst_material_index.span[face_index], 0, mat_nr_max);
          if (fr.reversed != do_flip) {
            for (int l = int(k) - 1; l >= 0; l--) {
              if (shell_defgrp_index != -1) {
                BKE_defvert_ensure_index(&dst_dvert[face_verts[l]], shell_defgrp_index)->weight =
                    1.0f;
              }
              CustomData_copy_data(&mesh->corner_data,
                                   &result->corner_data,
                                   int(face_loops[l]),
                                   int(loop_index),
                                   1);
              corner_verts[loop_index] = face_verts[l];
              corner_edges[loop_index++] = face_edges[l];
            }
          }
          else {
            uint l = k - 1;
            for (uint next_l = 0; next_l < k; next_l++) {
              CustomData_copy_data(&mesh->corner_data,
                                   &result->corner_data,
                                   int(face_loops[l]),
                                   int(loop_index),
                                   1);
              corner_verts[loop_index] = face_verts[l];
              corner_edges[loop_index++] = face_edges[next_l];
              l = next_l;
            }
          }
          face_index++;
        }
      }
    }
    MEM_freeN(face_loops);
    MEM_freeN(face_verts);
    MEM_freeN(face_edges);
  }
  if (edge_index != new_edges_num) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: edges array wrong size: %u instead of %u",
                           new_edges_num,
                           edge_index);
  }
  if (face_index != new_faces_num) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: faces array wrong size: %u instead of %u",
                           new_faces_num,
                           face_index);
  }
  if (loop_index != new_loops_num) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: loops array wrong size: %u instead of %u",
                           new_loops_num,
                           loop_index);
  }
  BLI_assert(edge_index == new_edges_num);
  BLI_assert(face_index == new_faces_num);
  BLI_assert(loop_index == new_loops_num);

  /* Free remaining memory */
  {
    MEM_freeN(vm);
    MEM_freeN(edge_adj_faces_len);
    uint i = 0;
    for (EdgeGroup **p = orig_vert_groups_arr; i < verts_num; i++, p++) {
      if (*p) {
        for (EdgeGroup *eg = *p; eg->valid; eg++) {
          MEM_freeN(eg->edges);
        }
        MEM_freeN(*p);
      }
    }
    MEM_freeN(orig_vert_groups_arr);
    i = edges_num;
    for (NewEdgeRef ***p = orig_edge_data_arr + (edges_num - 1); i > 0; i--, p--) {
      if (*p && (**p)->old_edge == i - 1) {
        for (NewEdgeRef **l = *p; *l; l++) {
          MEM_freeN(*l);
        }
        MEM_freeN(*p);
      }
    }
    MEM_freeN(orig_edge_data_arr);
    MEM_freeN(orig_edge_lengths);
    i = 0;
    for (NewFaceRef &p : face_sides_arr) {
      MEM_freeN(p.link_edges);
    }
  }

#undef MOD_SOLIDIFY_EMPTY_TAG

  dst_material_index.finish();
  result_edge_bweight.finish();
  result_edge_crease.finish();

  return result;
}

/** \} */
