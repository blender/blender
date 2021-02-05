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

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"

#include "MOD_modifiertypes.h"
#include "MOD_solidify_util.h" /* Own include. */
#include "MOD_util.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Solidify Function
 * \{ */

/* Data structures for manifold solidify. */

typedef struct NewFaceRef {
  MPoly *face;
  uint index;
  bool reversed;
  struct NewEdgeRef **link_edges;
} NewFaceRef;

typedef struct OldEdgeFaceRef {
  uint *faces;
  uint faces_len;
  bool *faces_reversed;
  uint used;
} OldEdgeFaceRef;

typedef struct OldVertEdgeRef {
  uint *edges;
  uint edges_len;
} OldVertEdgeRef;

typedef struct NewEdgeRef {
  uint old_edge;
  NewFaceRef *faces[2];
  struct EdgeGroup *link_edge_groups[2];
  float angle;
  uint new_edge;
} NewEdgeRef;

typedef struct EdgeGroup {
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
} EdgeGroup;

typedef struct FaceKeyPair {
  float angle;
  NewFaceRef *face;
} FaceKeyPair;

static int comp_float_int_pair(const void *a, const void *b)
{
  FaceKeyPair *x = (FaceKeyPair *)a;
  FaceKeyPair *y = (FaceKeyPair *)b;
  return (int)(x->angle > y->angle) - (int)(x->angle < y->angle);
}

/* NOLINTNEXTLINE: readability-function-size */
Mesh *MOD_solidify_nonmanifold_modifyMesh(ModifierData *md,
                                          const ModifierEvalContext *ctx,
                                          Mesh *mesh)
{
  Mesh *result;
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;

  MVert *mv, *mvert, *orig_mvert;
  MEdge *ed, *medge, *orig_medge;
  MLoop *ml, *mloop, *orig_mloop;
  MPoly *mp, *mpoly, *orig_mpoly;
  const uint numVerts = (uint)mesh->totvert;
  const uint numEdges = (uint)mesh->totedge;
  const uint numPolys = (uint)mesh->totpoly;
  const uint numLoops = (uint)mesh->totloop;

  if (numPolys == 0 && numVerts != 0) {
    return mesh;
  }

  /* Only use material offsets if we have 2 or more materials. */
  const short mat_nrs = ctx->object->totcol > 1 ? ctx->object->totcol : 1;
  const short mat_nr_max = mat_nrs - 1;
  const short mat_ofs = mat_nrs > 1 ? smd->mat_ofs : 0;
  const short mat_ofs_rim = mat_nrs > 1 ? smd->mat_ofs_rim : 0;

  float(*poly_nors)[3] = NULL;

  const float ofs_front = (smd->offset_fac + 1.0f) * 0.5f * smd->offset;
  const float ofs_back = ofs_front - smd->offset * smd->offset_fac;
  const float ofs_front_clamped = max_ff(1e-5f, fabsf(smd->offset > 0 ? ofs_front : ofs_back));
  const float ofs_back_clamped = max_ff(1e-5f, fabsf(smd->offset > 0 ? ofs_back : ofs_front));
  const float offset_fac_vg = smd->offset_fac_vg;
  const float offset_fac_vg_inv = 1.0f - smd->offset_fac_vg;
  const float offset = fabsf(smd->offset) * smd->offset_clamp;
  const bool do_angle_clamp = smd->flag & MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP;
  const bool do_flip = (smd->flag & MOD_SOLIDIFY_FLIP) != 0;
  const bool do_rim = smd->flag & MOD_SOLIDIFY_RIM;
  const bool do_shell = ((smd->flag & MOD_SOLIDIFY_RIM) && (smd->flag & MOD_SOLIDIFY_NOSHELL)) ==
                        0;
  const bool do_clamp = (smd->offset_clamp != 0.0f);

  const float bevel_convex = smd->bevel_convex;

  MDeformVert *dvert;
  const bool defgrp_invert = (smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0;
  int defgrp_index;
  const int shell_defgrp_index = BKE_object_defgroup_name_index(ctx->object,
                                                                smd->shell_defgrp_name);
  const int rim_defgrp_index = BKE_object_defgroup_name_index(ctx->object, smd->rim_defgrp_name);

  MOD_get_vgroup(ctx->object, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  const bool do_flat_faces = dvert && (smd->flag & MOD_SOLIDIFY_NONMANIFOLD_FLAT_FACES);

  orig_mvert = mesh->mvert;
  orig_medge = mesh->medge;
  orig_mloop = mesh->mloop;
  orig_mpoly = mesh->mpoly;

  uint numNewVerts = 0;
  uint numNewEdges = 0;
  uint numNewLoops = 0;
  uint numNewPolys = 0;

#define MOD_SOLIDIFY_EMPTY_TAG ((uint)-1)

  /* Calculate only face normals. */
  poly_nors = MEM_malloc_arrayN(numPolys, sizeof(*poly_nors), __func__);
  BKE_mesh_calc_normals_poly(orig_mvert,
                             NULL,
                             (int)numVerts,
                             orig_mloop,
                             orig_mpoly,
                             (int)numLoops,
                             (int)numPolys,
                             poly_nors,
                             true);

  NewFaceRef *face_sides_arr = MEM_malloc_arrayN(
      numPolys * 2, sizeof(*face_sides_arr), "face_sides_arr in solidify");
  bool *null_faces =
      (smd->nonmanifold_offset_mode == MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS) ?
          MEM_calloc_arrayN(numPolys, sizeof(*null_faces), "null_faces in solidify") :
          NULL;
  uint largest_ngon = 3;
  /* Calculate face to #NewFaceRef map. */
  {
    mp = orig_mpoly;
    for (uint i = 0; i < numPolys; i++, mp++) {
      /* Make normals for faces without area (should really be avoided though). */
      if (len_squared_v3(poly_nors[i]) < 0.5f) {
        MEdge *e = orig_medge + orig_mloop[mp->loopstart].e;
        float edgedir[3];
        sub_v3_v3v3(edgedir, orig_mvert[e->v2].co, orig_mvert[e->v1].co);
        if (fabsf(edgedir[2]) < fabsf(edgedir[1])) {
          poly_nors[i][2] = 1.0f;
        }
        else {
          poly_nors[i][1] = 1.0f;
        }
        if (null_faces) {
          null_faces[i] = true;
        }
      }

      NewEdgeRef **link_edges = MEM_calloc_arrayN(
          (uint)mp->totloop, sizeof(*link_edges), "NewFaceRef::link_edges in solidify");
      face_sides_arr[i * 2] = (NewFaceRef){
          .face = mp, .index = i, .reversed = false, .link_edges = link_edges};
      link_edges = MEM_calloc_arrayN(
          (uint)mp->totloop, sizeof(*link_edges), "NewFaceRef::link_edges in solidify");
      face_sides_arr[i * 2 + 1] = (NewFaceRef){
          .face = mp, .index = i, .reversed = true, .link_edges = link_edges};
      if (mp->totloop > largest_ngon) {
        largest_ngon = (uint)mp->totloop;
      }
      /* add to final mesh face count */
      if (do_shell) {
        numNewPolys += 2;
        numNewLoops += (uint)mp->totloop * 2;
      }
    }
  }

  uint *edge_adj_faces_len = MEM_calloc_arrayN(
      numEdges, sizeof(*edge_adj_faces_len), "edge_adj_faces_len in solidify");
  /* Count for each edge how many faces it has adjacent. */
  {
    mp = orig_mpoly;
    for (uint i = 0; i < numPolys; i++, mp++) {
      ml = orig_mloop + mp->loopstart;
      for (uint j = 0; j < mp->totloop; j++, ml++) {
        edge_adj_faces_len[ml->e]++;
      }
    }
  }

  /* Original edge to #NewEdgeRef map. */
  NewEdgeRef ***orig_edge_data_arr = MEM_calloc_arrayN(
      numEdges, sizeof(*orig_edge_data_arr), "orig_edge_data_arr in solidify");
  /* Original edge length cache. */
  float *orig_edge_lengths = MEM_calloc_arrayN(
      numEdges, sizeof(*orig_edge_lengths), "orig_edge_lengths in solidify");
  /* Edge groups for every original vert. */
  EdgeGroup **orig_vert_groups_arr = MEM_calloc_arrayN(
      numVerts, sizeof(*orig_vert_groups_arr), "orig_vert_groups_arr in solidify");
  /* vertex map used to map duplicates. */
  uint *vm = MEM_malloc_arrayN(numVerts, sizeof(*vm), "orig_vert_map in solidify");
  for (uint i = 0; i < numVerts; i++) {
    vm[i] = i;
  }

  uint edge_index = 0;
  uint loop_index = 0;
  uint poly_index = 0;

  bool has_singularities = false;

  /* Vert edge adjacent map. */
  OldVertEdgeRef **vert_adj_edges = MEM_calloc_arrayN(
      numVerts, sizeof(*vert_adj_edges), "vert_adj_edges in solidify");
  /* Original vertex positions (changed for degenerated geometry). */
  float(*orig_mvert_co)[3] = MEM_malloc_arrayN(
      numVerts, sizeof(*orig_mvert_co), "orig_mvert_co in solidify");
  /* Fill in the original vertex positions. */
  for (uint i = 0; i < numVerts; i++) {
    orig_mvert_co[i][0] = orig_mvert[i].co[0];
    orig_mvert_co[i][1] = orig_mvert[i].co[1];
    orig_mvert_co[i][2] = orig_mvert[i].co[2];
  }

  /* Create edge to #NewEdgeRef map. */
  {
    OldEdgeFaceRef **edge_adj_faces = MEM_calloc_arrayN(
        numEdges, sizeof(*edge_adj_faces), "edge_adj_faces in solidify");

    /* Create link_faces for edges. */
    {
      mp = orig_mpoly;
      for (uint i = 0; i < numPolys; i++, mp++) {
        ml = orig_mloop + mp->loopstart;
        for (uint j = 0; j < mp->totloop; j++, ml++) {
          const uint edge = ml->e;
          const bool reversed = orig_medge[edge].v2 != ml->v;
          OldEdgeFaceRef *old_face_edge_ref = edge_adj_faces[edge];
          if (old_face_edge_ref == NULL) {
            const uint len = edge_adj_faces_len[edge];
            BLI_assert(len > 0);
            uint *adj_faces = MEM_malloc_arrayN(
                len, sizeof(*adj_faces), "OldEdgeFaceRef::faces in solidify");
            bool *adj_faces_reversed = MEM_malloc_arrayN(
                len, sizeof(*adj_faces_reversed), "OldEdgeFaceRef::reversed in solidify");
            adj_faces[0] = i;
            for (uint k = 1; k < len; k++) {
              adj_faces[k] = MOD_SOLIDIFY_EMPTY_TAG;
            }
            adj_faces_reversed[0] = reversed;
            OldEdgeFaceRef *ref = MEM_mallocN(sizeof(*ref), "OldEdgeFaceRef in solidify");
            *ref = (OldEdgeFaceRef){adj_faces, len, adj_faces_reversed, 1};
            edge_adj_faces[edge] = ref;
          }
          else {
            for (uint k = 1; k < old_face_edge_ref->faces_len; k++) {
              if (old_face_edge_ref->faces[k] == MOD_SOLIDIFY_EMPTY_TAG) {
                old_face_edge_ref->faces[k] = i;
                old_face_edge_ref->faces_reversed[k] = reversed;
                break;
              }
            }
          }
        }
      }
    }

    float edgedir[3] = {0, 0, 0};
    uint *vert_adj_edges_len = MEM_calloc_arrayN(
        numVerts, sizeof(*vert_adj_edges_len), "vert_adj_edges_len in solidify");

    /* Calculate edge lengths and len vert_adj edges. */
    {
      bool *face_singularity = MEM_calloc_arrayN(
          numPolys, sizeof(*face_singularity), "face_sides_arr in solidify");

      const float merge_tolerance_sqr = smd->merge_tolerance * smd->merge_tolerance;
      uint *combined_verts = MEM_calloc_arrayN(
          numVerts, sizeof(*combined_verts), "combined_verts in solidify");

      ed = orig_medge;
      for (uint i = 0; i < numEdges; i++, ed++) {
        if (edge_adj_faces_len[i] > 0) {
          uint v1 = vm[ed->v1];
          uint v2 = vm[ed->v2];
          if (v1 == v2) {
            continue;
          }

          if (v2 < v1) {
            SWAP(uint, v1, v2);
          }
          sub_v3_v3v3(edgedir, orig_mvert_co[v2], orig_mvert_co[v1]);
          orig_edge_lengths[i] = len_squared_v3(edgedir);

          if (orig_edge_lengths[i] <= merge_tolerance_sqr) {
            /* Merge verts. But first check if that would create a higher poly count. */
            /* This check is very slow. It would need the vertex edge links to get
             * accelerated that are not yet available at this point. */
            bool can_merge = true;
            for (uint k = 0; k < numEdges && can_merge; k++) {
              if (k != i && edge_adj_faces_len[k] > 0 &&
                  (ELEM(vm[orig_medge[k].v1], v1, v2) != ELEM(vm[orig_medge[k].v2], v1, v2))) {
                for (uint j = 0; j < edge_adj_faces[k]->faces_len && can_merge; j++) {
                  mp = orig_mpoly + edge_adj_faces[k]->faces[j];
                  uint changes = 0;
                  int cur = mp->totloop - 1;
                  for (int next = 0; next < mp->totloop && changes <= 2; next++) {
                    uint cur_v = vm[orig_mloop[mp->loopstart + cur].v];
                    uint next_v = vm[orig_mloop[mp->loopstart + next].v];
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
                          (float)(combined_verts[v1] + combined_verts[v2] + 2));
            add_v3_v3(orig_mvert_co[v1], edgedir);
            for (uint j = v2; j < numVerts; j++) {
              if (vm[j] == v2) {
                vm[j] = v1;
              }
            }
            vert_adj_edges_len[v1] += vert_adj_edges_len[v2];
            vert_adj_edges_len[v2] = 0;
            combined_verts[v1] += combined_verts[v2] + 1;

            if (do_shell) {
              numNewLoops -= edge_adj_faces_len[i] * 2;
            }

            edge_adj_faces_len[i] = 0;
            MEM_freeN(edge_adj_faces[i]->faces);
            MEM_freeN(edge_adj_faces[i]->faces_reversed);
            MEM_freeN(edge_adj_faces[i]);
            edge_adj_faces[i] = NULL;
          }
          else {
            orig_edge_lengths[i] = sqrtf(orig_edge_lengths[i]);
            vert_adj_edges_len[v1]++;
            vert_adj_edges_len[v2]++;
          }
        }
      }
      /* remove zero faces in a second pass */
      ed = orig_medge;
      for (uint i = 0; i < numEdges; i++, ed++) {
        const uint v1 = vm[ed->v1];
        const uint v2 = vm[ed->v2];
        if (v1 == v2 && edge_adj_faces[i]) {
          /* Remove polys. */
          for (uint j = 0; j < edge_adj_faces[i]->faces_len; j++) {
            const uint face = edge_adj_faces[i]->faces[j];
            if (!face_singularity[face]) {
              bool is_singularity = true;
              for (uint k = 0; k < orig_mpoly[face].totloop; k++) {
                if (vm[orig_mloop[((uint)orig_mpoly[face].loopstart) + k].v] != v1) {
                  is_singularity = false;
                  break;
                }
              }
              if (is_singularity) {
                face_singularity[face] = true;
                /* remove from final mesh poly count */
                if (do_shell) {
                  numNewPolys -= 2;
                }
              }
            }
          }

          if (do_shell) {
            numNewLoops -= edge_adj_faces_len[i] * 2;
          }

          edge_adj_faces_len[i] = 0;
          MEM_freeN(edge_adj_faces[i]->faces);
          MEM_freeN(edge_adj_faces[i]->faces_reversed);
          MEM_freeN(edge_adj_faces[i]);
          edge_adj_faces[i] = NULL;
        }
      }

      MEM_freeN(face_singularity);
      MEM_freeN(combined_verts);
    }

    /* Create vert_adj_edges for verts. */
    {
      ed = orig_medge;
      for (uint i = 0; i < numEdges; i++, ed++) {
        if (edge_adj_faces_len[i] > 0) {
          const uint vs[2] = {vm[ed->v1], vm[ed->v2]};
          uint invalid_edge_index = 0;
          bool invalid_edge_reversed = false;
          for (uint j = 0; j < 2; j++) {
            const uint vert = vs[j];
            const uint len = vert_adj_edges_len[vert];
            if (len > 0) {
              OldVertEdgeRef *old_edge_vert_ref = vert_adj_edges[vert];
              if (old_edge_vert_ref == NULL) {
                uint *adj_edges = MEM_calloc_arrayN(
                    len, sizeof(*adj_edges), "OldVertEdgeRef::edges in solidify");
                adj_edges[0] = i;
                for (uint k = 1; k < len; k++) {
                  adj_edges[k] = MOD_SOLIDIFY_EMPTY_TAG;
                }
                OldVertEdgeRef *ref = MEM_mallocN(sizeof(*ref), "OldVertEdgeRef in solidify");
                *ref = (OldVertEdgeRef){adj_edges, 1};
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
                  if (vm[orig_medge[edge].v1] == vs[1 - j]) {
                    invalid_edge_index = edge + 1;
                    invalid_edge_reversed = (j == 0);
                    break;
                  }
                  if (vm[orig_medge[edge].v2] == vs[1 - j]) {
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
                    i_adj_faces->faces[k] != MOD_SOLIDIFY_EMPTY_TAG) {
                  i_adj_faces->faces[k] = MOD_SOLIDIFY_EMPTY_TAG;
                  invalid_adj_faces->faces[l] = MOD_SOLIDIFY_EMPTY_TAG;
                  j++;
                }
              }
            }
            /* remove from final face count */
            if (do_shell) {
              numNewPolys -= 2 * j;
              numNewLoops -= 4 * j;
            }
            const uint len = i_adj_faces->faces_len + invalid_adj_faces->faces_len - 2 * j;
            uint *adj_faces = MEM_malloc_arrayN(
                len, sizeof(*adj_faces), "OldEdgeFaceRef::faces in solidify");
            bool *adj_faces_loops_reversed = MEM_malloc_arrayN(
                len, sizeof(*adj_faces_loops_reversed), "OldEdgeFaceRef::reversed in solidify");
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

    /* Filter duplicate polys. */
    {
      ed = orig_medge;
      /* Iterate over edges and only check the faces around an edge for duplicates
       * (performance optimization). */
      for (uint i = 0; i < numEdges; i++, ed++) {
        if (edge_adj_faces_len[i] > 0) {
          const OldEdgeFaceRef *adj_faces = edge_adj_faces[i];
          uint adj_len = adj_faces->faces_len;
          /* Not that #adj_len doesn't need to equal edge_adj_faces_len anymore
           * because #adj_len is shared when a face got collapsed to an edge. */
          if (adj_len > 1) {
            /* For each face pair check if they have equal verts. */
            for (uint j = 0; j < adj_len; j++) {
              const uint face = adj_faces->faces[j];
              const int j_loopstart = orig_mpoly[face].loopstart;
              const int totloop = orig_mpoly[face].totloop;
              const uint j_first_v = vm[orig_mloop[j_loopstart].v];
              for (uint k = j + 1; k < adj_len; k++) {
                if (orig_mpoly[adj_faces->faces[k]].totloop != totloop) {
                  continue;
                }
                /* Find first face first loop vert in second face loops. */
                const int k_loopstart = orig_mpoly[adj_faces->faces[k]].loopstart;
                int l;
                ml = orig_mloop + k_loopstart;
                for (l = 0; l < totloop && vm[ml->v] != j_first_v; l++, ml++) {
                  /* Pass. */
                }
                if (l == totloop) {
                  continue;
                }
                /* Check if all following loops have equal verts. */
                const bool reversed = adj_faces->faces_reversed[j] != adj_faces->faces_reversed[k];
                const int count_dir = reversed ? -1 : 1;
                bool has_diff = false;
                ml = orig_mloop + j_loopstart;
                for (int m = 0, n = l + totloop; m < totloop && !has_diff;
                     m++, n += count_dir, ml++) {
                  has_diff = has_diff || vm[ml->v] != vm[orig_mloop[k_loopstart + n % totloop].v];
                }
                /* If the faces are equal, discard one (j). */
                if (!has_diff) {
                  ml = orig_mloop + j_loopstart;
                  uint del_loops = 0;
                  for (uint m = 0; m < totloop; m++, ml++) {
                    const uint e = ml->e;
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
                             face_index++) {
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
                          edge_adj_faces[e] = NULL;
                        }
                      }
                      else if (e_adj_faces->used > 1) {
                        for (uint n = 0; n < numEdges; n++) {
                          if (edge_adj_faces[n] == e_adj_faces && edge_adj_faces_len[n] > 0) {
                            edge_adj_faces_len[n]--;
                            if (edge_adj_faces_len[n] == 0) {
                              edge_adj_faces[n]->used--;
                              edge_adj_faces[n] = NULL;
                            }
                            break;
                          }
                        }
                      }
                      del_loops++;
                    }
                  }
                  if (do_shell) {
                    numNewPolys -= 2;
                    numNewLoops -= 2 * (uint)del_loops;
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
      ed = orig_medge;
      for (uint i = 0; i < numEdges; i++, ed++) {
        const uint v1 = vm[ed->v1];
        const uint v2 = vm[ed->v2];
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
                    vm[vm[orig_medge[e].v1] == v1 ? orig_medge[e].v2 : orig_medge[e].v1];
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
                    vm[vm[orig_medge[e].v1] == v2 ? orig_medge[e].v2 : orig_medge[e].v1];
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
          FaceKeyPair *sorted_faces = MEM_malloc_arrayN(
              adj_len, sizeof(*sorted_faces), "sorted_faces in solidify");
          if (adj_len > 1) {
            new_edges_len = adj_len;
            /* Get keys for sorting. */
            float ref_nor[3] = {0, 0, 0};
            float nor[3];
            for (uint j = 0; j < adj_len; j++) {
              const bool reverse = adj_faces_reversed[j];
              const uint face_i = adj_faces_faces[j];
              if (reverse) {
                negate_v3_v3(nor, poly_nors[face_i]);
              }
              else {
                copy_v3_v3(nor, poly_nors[face_i]);
              }
              float d = 1;
              if (orig_mpoly[face_i].totloop > 3) {
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
              sorted_faces[j].face = face_sides_arr + adj_faces_faces[j] * 2 +
                                     (adj_faces_reversed[j] ? 1 : 0);
            }
            /* Sort faces by order around the edge (keep order in faces,
             * reversed and face_angles the same). */
            qsort(sorted_faces, adj_len, sizeof(*sorted_faces), comp_float_int_pair);
          }
          else {
            new_edges_len = 2;
            sorted_faces[0].face = face_sides_arr + adj_faces_faces[0] * 2 +
                                   (adj_faces_reversed[0] ? 1 : 0);
            if (do_rim) {
              /* Only add the loops parallel to the edge for now. */
              numNewLoops += 2;
              numNewPolys++;
            }
          }

          /* Create a list of new edges and fill it. */
          NewEdgeRef **new_edges = MEM_malloc_arrayN(
              new_edges_len + 1, sizeof(*new_edges), "new_edges in solidify");
          new_edges[new_edges_len] = NULL;
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
              faces[1] = NULL;
              angle = 0;
            }
            NewEdgeRef *edge_data = MEM_mallocN(sizeof(*edge_data), "edge_data in solidify");
            uint edge_data_edge_index = MOD_SOLIDIFY_EMPTY_TAG;
            if (do_shell || (adj_len == 1 && do_rim)) {
              edge_data_edge_index = 0;
            }
            *edge_data = (NewEdgeRef){.old_edge = i,
                                      .faces = {faces[0], faces[1]},
                                      .link_edge_groups = {NULL, NULL},
                                      .angle = angle,
                                      .new_edge = edge_data_edge_index};
            new_edges[j] = edge_data;
            for (uint k = 0; k < 2; k++) {
              if (faces[k] != NULL) {
                ml = orig_mloop + faces[k]->face->loopstart;
                for (int l = 0; l < faces[k]->face->totloop; l++, ml++) {
                  if (edge_adj_faces[ml->e] == edge_adj_faces[i]) {
                    if (ml->e != i && orig_edge_data_arr[ml->e] == NULL) {
                      orig_edge_data_arr[ml->e] = new_edges;
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
            numNewEdges += new_edges_len;
          }
        }
      }
    }

    for (uint i = 0; i < numEdges; i++) {
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
    for (uint i = 0; i < numVerts; i++, adj_edges_ptr++) {
      if (*adj_edges_ptr != NULL && (*adj_edges_ptr)->edges_len >= 2) {
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
            /* TODO check where the null pointer come from,
             * because there should not be any... */
            if (new_edges) {
              /* count the number of new edges around the original vert */
              while (*new_edges) {
                unassigned_edges_len++;
                new_edges++;
              }
            }
          }
          NewEdgeRef **unassigned_edges = MEM_malloc_arrayN(
              unassigned_edges_len, sizeof(*unassigned_edges), "unassigned_edges in solidify");
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
          edge_groups = MEM_calloc_arrayN(
              edge_groups_len + 1, sizeof(*edge_groups), "edge_groups in solidify");

          uint assigned_edges_len = 0;
          NewEdgeRef *found_edge = NULL;
          uint found_edge_index = 0;
          bool insert_at_start = false;
          uint eg_capacity = 5;
          NewFaceRef *eg_track_faces[2] = {NULL, NULL};
          NewFaceRef *last_open_edge_track = NULL;

          while (assigned_edges_len < unassigned_edges_len) {
            found_edge = NULL;
            insert_at_start = false;
            if (eg_index >= 0 && edge_groups[eg_index].edges_len == 0) {
              /* Called every time a new group was started in the last iteration. */
              /* Find an unused edge to start the next group
               * and setup variables to start creating it. */
              uint j = 0;
              NewEdgeRef *edge = NULL;
              while (!edge && j < unassigned_edges_len) {
                edge = unassigned_edges[j++];
                if (edge && last_open_edge_track &&
                    (edge->faces[0] != last_open_edge_track || edge->faces[1] != NULL)) {
                  edge = NULL;
                }
              }
              if (!edge && last_open_edge_track) {
                topo_groups++;
                last_open_edge_track = NULL;
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
              BLI_assert(edge != NULL);
              found_edge_index = j - 1;
              found_edge = edge;
              if (!last_open_edge_track && vm[orig_medge[edge->old_edge].v1] == i) {
                eg_track_faces[0] = edge->faces[0];
                eg_track_faces[1] = edge->faces[1];
                if (edge->faces[1] == NULL) {
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
                   found_edge_index++, edge_ptr++) {
                if (*edge_ptr) {
                  NewEdgeRef *edge = *edge_ptr;
                  if (edge->faces[0] == eg_track_faces[1]) {
                    insert_at_start = false;
                    eg_track_faces[1] = edge->faces[1];
                    found_edge = edge;
                    if (edge->faces[1] == NULL) {
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
                    if (edge->faces[1] == NULL) {
                      edge_groups[eg_index].is_orig_closed = false;
                    }
                    break;
                  }
                  if (edge->faces[1] != NULL) {
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
              unassigned_edges[found_edge_index] = NULL;
              assigned_edges_len++;
              const uint needed_capacity = edge_groups[eg_index].edges_len + 1;
              if (needed_capacity > eg_capacity) {
                eg_capacity = needed_capacity + 1;
                NewEdgeRef **new_eg = MEM_calloc_arrayN(
                    eg_capacity, sizeof(*new_eg), "edge_group realloc in solidify");
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
                  NULL) {
                last_open_edge_track = NULL;
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
              NewEdgeRef **edges = MEM_calloc_arrayN(
                  eg_capacity, sizeof(*edges), "edge_group in solidify");
              edge_groups[eg_index] = (EdgeGroup){
                  .valid = true,
                  .edges = edges,
                  .edges_len = 0,
                  .open_face_edge = MOD_SOLIDIFY_EMPTY_TAG,
                  .is_orig_closed = true,
                  .is_even_split = false,
                  .split = 0,
                  .is_singularity = false,
                  .topo_group = topo_groups,
                  .co = {0.0f, 0.0f, 0.0f},
                  .no = {0.0f, 0.0f, 0.0f},
                  .new_vert = MOD_SOLIDIFY_EMPTY_TAG,
              };
              eg_track_faces[0] = NULL;
              eg_track_faces[1] = NULL;
            }
          }
          /* #eg_index is the number of groups from here on. */
          eg_index++;
          /* #topo_groups is the number of topo groups from here on. */
          topo_groups++;

          MEM_freeN(unassigned_edges);

          /* TODO reshape the edge_groups array to its actual size
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
                bool *doubles = MEM_calloc_arrayN(
                    edges_len, sizeof(*doubles), "doubles in solidify");
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
                               (first_unique_end == -1 ? 0 : first_unique_end) + (int)edges_len ||
                           first_split != last_split))) {
                    const uint k = real_k % edges_len;
                    if (!doubles[k]) {
                      if (first_unique_end != -1 && unique_start == -1) {
                        unique_start = (int)real_k;
                      }
                    }
                    else if (first_unique_end == -1) {
                      first_unique_end = (int)k;
                    }
                    else if (unique_start != -1) {
                      const uint split = (((uint)unique_start + real_k + 1) / 2) % edges_len;
                      const bool is_even_split = (((uint)unique_start + real_k) & 1);
                      if (last_split != -1) {
                        /* Override g on first split (no insert). */
                        if (prior_splits != splits) {
                          memmove(edge_groups + j + add_index + 1,
                                  edge_groups + j + add_index,
                                  ((uint)eg_index - j) * sizeof(*edge_groups));
                          add_index++;
                        }
                        if (last_split > split) {
                          const uint size = (split + edges_len) - (uint)last_split;
                          NewEdgeRef **edges = MEM_malloc_arrayN(
                              size, sizeof(*edges), "edge_group split in solidify");
                          memcpy(edges,
                                 g.edges + last_split,
                                 (edges_len - (uint)last_split) * sizeof(*edges));
                          memcpy(edges + (edges_len - (uint)last_split),
                                 g.edges,
                                 split * sizeof(*edges));
                          edge_groups[j + add_index] = (EdgeGroup){
                              .valid = true,
                              .edges = edges,
                              .edges_len = size,
                              .open_face_edge = MOD_SOLIDIFY_EMPTY_TAG,
                              .is_orig_closed = g.is_orig_closed,
                              .is_even_split = is_even_split,
                              .split = add_index - prior_index + 1 + (uint)!g.is_orig_closed,
                              .is_singularity = false,
                              .topo_group = g.topo_group,
                              .co = {0.0f, 0.0f, 0.0f},
                              .no = {0.0f, 0.0f, 0.0f},
                              .new_vert = MOD_SOLIDIFY_EMPTY_TAG,
                          };
                        }
                        else {
                          const uint size = split - (uint)last_split;
                          NewEdgeRef **edges = MEM_malloc_arrayN(
                              size, sizeof(*edges), "edge_group split in solidify");
                          memcpy(edges, g.edges + last_split, size * sizeof(*edges));
                          edge_groups[j + add_index] = (EdgeGroup){
                              .valid = true,
                              .edges = edges,
                              .edges_len = size,
                              .open_face_edge = MOD_SOLIDIFY_EMPTY_TAG,
                              .is_orig_closed = g.is_orig_closed,
                              .is_even_split = is_even_split,
                              .split = add_index - prior_index + 1 + (uint)!g.is_orig_closed,
                              .is_singularity = false,
                              .topo_group = g.topo_group,
                              .co = {0.0f, 0.0f, 0.0f},
                              .no = {0.0f, 0.0f, 0.0f},
                              .new_vert = MOD_SOLIDIFY_EMPTY_TAG,
                          };
                        }
                        splits++;
                      }
                      last_split = (int)split;
                      if (first_split == -1) {
                        first_split = (int)split;
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
                                ((uint)eg_index + add_index - (j + prior_index)) *
                                    sizeof(*edge_groups));
                        memmove(edge_groups + (j + add_index + 2),
                                edge_groups + (j + add_index + 1),
                                ((uint)eg_index - j) * sizeof(*edge_groups));
                        add_index++;
                      }
                      else {
                        memmove(edge_groups + (j + add_index + 2),
                                edge_groups + (j + add_index + 1),
                                ((uint)eg_index - j - 1) * sizeof(*edge_groups));
                      }
                      NewEdgeRef **edges = MEM_malloc_arrayN(
                          (uint)first_split, sizeof(*edges), "edge_group split in solidify");
                      memcpy(edges, g.edges, (uint)first_split * sizeof(*edges));
                      edge_groups[j + prior_index] = (EdgeGroup){
                          .valid = true,
                          .edges = edges,
                          .edges_len = (uint)first_split,
                          .open_face_edge = MOD_SOLIDIFY_EMPTY_TAG,
                          .is_orig_closed = g.is_orig_closed,
                          .is_even_split = first_even_split,
                          .split = 1,
                          .is_singularity = false,
                          .topo_group = g.topo_group,
                          .co = {0.0f, 0.0f, 0.0f},
                          .no = {0.0f, 0.0f, 0.0f},
                          .new_vert = MOD_SOLIDIFY_EMPTY_TAG,
                      };
                      add_index++;
                      splits++;
                      edges = MEM_malloc_arrayN(edges_len - (uint)last_split,
                                                sizeof(*edges),
                                                "edge_group split in solidify");
                      memcpy(edges,
                             g.edges + last_split,
                             (edges_len - (uint)last_split) * sizeof(*edges));
                      edge_groups[j + add_index] = (EdgeGroup){
                          .valid = true,
                          .edges = edges,
                          .edges_len = (edges_len - (uint)last_split),
                          .open_face_edge = MOD_SOLIDIFY_EMPTY_TAG,
                          .is_orig_closed = g.is_orig_closed,
                          .is_even_split = false,
                          .split = add_index - prior_index + 1,
                          .is_singularity = false,
                          .topo_group = g.topo_group,
                          .co = {0.0f, 0.0f, 0.0f},
                          .no = {0.0f, 0.0f, 0.0f},
                          .new_vert = MOD_SOLIDIFY_EMPTY_TAG,
                      };
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
        /* Count new edges, loops, polys and add to link_edge_groups. */
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
              const uint flip = (uint)(vm[orig_medge[(*e)->old_edge].v2] == i);
              BLI_assert(flip || vm[orig_medge[(*e)->old_edge].v1] == i);
              (*e)->link_edge_groups[flip] = g;
            }
            uint added = 0;
            if (do_shell || (do_rim && !g->is_orig_closed)) {
              BLI_assert(g->new_vert == MOD_SOLIDIFY_EMPTY_TAG);
              g->new_vert = numNewVerts++;
              if (do_rim || (do_shell && g->split)) {
                new_verts++;
                contains_splits += (g->split != 0);
                contains_open_splits |= g->split && !g->is_orig_closed;
                added = g->split;
              }
            }
            open_edges += (uint)(added < last_added);
            if (!first_set) {
              first_set = true;
              first_added = added;
            }
            last_added = added;
            if (!(g + 1)->valid || g->topo_group != (g + 1)->topo_group) {
              if (new_verts > 2) {
                numNewPolys++;
                numNewEdges += new_verts;
                open_edges += (uint)(first_added < last_added);
                open_edges -= (uint)(open_edges && !contains_open_splits);
                if (do_shell && do_rim) {
                  numNewLoops += new_verts * 2;
                }
                else if (do_shell) {
                  numNewLoops += new_verts * 2 - open_edges;
                }
                else {  // do_rim
                  numNewLoops += new_verts * 2 + open_edges - contains_splits;
                }
              }
              else if (new_verts == 2) {
                numNewEdges++;
                numNewLoops += 2u - (uint)(!(do_rim && do_shell) && contains_open_splits);
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
    for (OldVertEdgeRef **p = vert_adj_edges; i < numVerts; i++, p++) {
      if (*p) {
        MEM_freeN((*p)->edges);
        MEM_freeN(*p);
      }
    }
    MEM_freeN(vert_adj_edges);
  }

  /* TODO create_regions if fix_intersections. */

  /* General use pointer for #EdgeGroup iteration. */
  EdgeGroup **gs_ptr;

  /* Calculate EdgeGroup vertex coordinates. */
  {
    float *face_weight = NULL;

    if (do_flat_faces) {
      face_weight = MEM_malloc_arrayN(numPolys, sizeof(*face_weight), "face_weight in solidify");

      mp = orig_mpoly;
      for (uint i = 0; i < numPolys; i++, mp++) {
        float scalar_vgroup = 1.0f;
        int loopend = mp->loopstart + mp->totloop;
        ml = orig_mloop + mp->loopstart;
        for (int j = mp->loopstart; j < loopend; j++, ml++) {
          MDeformVert *dv = &dvert[ml->v];
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

    mv = orig_mvert;
    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < numVerts; i++, mv++, gs_ptr++) {
      if (*gs_ptr) {
        EdgeGroup *g = *gs_ptr;
        for (uint j = 0; g->valid; j++, g++) {
          if (!g->is_singularity) {
            float *nor = g->no;
            float move_nor[3] = {0, 0, 0};
            bool disable_boundary_fix = (smd->nonmanifold_boundary_mode ==
                                             MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE ||
                                         (g->is_orig_closed || g->split));
            /* Constraints Method. */
            if (smd->nonmanifold_offset_mode == MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS) {
              NewEdgeRef *first_edge = NULL;
              NewEdgeRef **edge_ptr = g->edges;
              /* Contains normal and offset [nx, ny, nz, ofs]. */
              float(*normals_queue)[4] = MEM_malloc_arrayN(
                  g->edges_len + 1, sizeof(*normals_queue), "normals_queue in solidify");
              uint queue_index = 0;

              float face_nors[3][3];
              float nor_ofs[3];

              const bool cycle = (g->is_orig_closed && !g->split) || g->is_even_split;
              for (uint k = 0; k < g->edges_len; k++, edge_ptr++) {
                if (!(k & 1) || (!cycle && k == g->edges_len - 1)) {
                  NewEdgeRef *edge = *edge_ptr;
                  for (uint l = 0; l < 2; l++) {
                    NewFaceRef *face = edge->faces[l];
                    if (face && (first_edge == NULL ||
                                 (first_edge->faces[0] != face && first_edge->faces[1] != face))) {
                      float ofs = face->reversed ? ofs_back_clamped : ofs_front_clamped;
                      /* Use face_weight here to make faces thinner. */
                      if (do_flat_faces) {
                        ofs *= face_weight[face->index];
                      }

                      if (!null_faces[face->index]) {
                        /* And normal to the queue. */
                        mul_v3_v3fl(normals_queue[queue_index],
                                    poly_nors[face->index],
                                    face->reversed ? -1 : 1);
                        normals_queue[queue_index++][3] = ofs;
                      }
                      else {
                        /* Just use this approximate normal of the null face if there is no other
                         * normal to use. */
                        mul_v3_v3fl(face_nors[0], poly_nors[face->index], face->reversed ? -1 : 1);
                        nor_ofs[0] = ofs;
                      }
                    }
                  }
                  if ((cycle && k == 0) || (!cycle && k + 3 >= g->edges_len)) {
                    first_edge = edge;
                  }
                }
              }
              uint face_nors_len = 0;
              const float stop_explosion = 0.999f - fabsf(smd->offset_fac) * 0.05f;
              while (queue_index > 0) {
                if (face_nors_len == 0) {
                  if (queue_index <= 2) {
                    for (uint k = 0; k < queue_index; k++) {
                      copy_v3_v3(face_nors[k], normals_queue[k]);
                      nor_ofs[k] = normals_queue[k][3];
                    }
                    face_nors_len = queue_index;
                    queue_index = 0;
                  }
                  else {
                    /* Find most different two normals. */
                    float min_p = 2;
                    uint min_n0 = 0;
                    uint min_n1 = 0;
                    for (uint k = 0; k < queue_index; k++) {
                      for (uint m = k + 1; m < queue_index; m++) {
                        float p = dot_v3v3(normals_queue[k], normals_queue[m]);
                        if (p <= min_p + FLT_EPSILON) {
                          min_p = p;
                          min_n0 = m;
                          min_n1 = k;
                        }
                      }
                    }
                    copy_v3_v3(face_nors[0], normals_queue[min_n0]);
                    copy_v3_v3(face_nors[1], normals_queue[min_n1]);
                    nor_ofs[0] = normals_queue[min_n0][3];
                    nor_ofs[1] = normals_queue[min_n1][3];
                    face_nors_len = 2;
                    queue_index--;
                    memmove(normals_queue + min_n0,
                            normals_queue + min_n0 + 1,
                            (queue_index - min_n0) * sizeof(*normals_queue));
                    queue_index--;
                    memmove(normals_queue + min_n1,
                            normals_queue + min_n1 + 1,
                            (queue_index - min_n1) * sizeof(*normals_queue));
                    min_p = 1;
                    min_n1 = 0;
                    float max_p = -1;
                    for (uint k = 0; k < queue_index; k++) {
                      max_p = -1;
                      for (uint m = 0; m < face_nors_len; m++) {
                        float p = dot_v3v3(face_nors[m], normals_queue[k]);
                        if (p > max_p + FLT_EPSILON) {
                          max_p = p;
                        }
                      }
                      if (max_p <= min_p + FLT_EPSILON) {
                        min_p = max_p;
                        min_n1 = k;
                      }
                    }
                    if (min_p < 0.8) {
                      copy_v3_v3(face_nors[2], normals_queue[min_n1]);
                      nor_ofs[2] = normals_queue[min_n1][3];
                      face_nors_len++;
                      queue_index--;
                      memmove(normals_queue + min_n1,
                              normals_queue + min_n1 + 1,
                              (queue_index - min_n1) * sizeof(*normals_queue));
                    }
                  }
                }
                else {
                  uint best = 0;
                  uint best_group = 0;
                  float best_p = -1.0f;
                  for (uint k = 0; k < queue_index; k++) {
                    for (uint m = 0; m < face_nors_len; m++) {
                      float p = dot_v3v3(face_nors[m], normals_queue[k]);
                      if (p > best_p + FLT_EPSILON) {
                        best_p = p;
                        best = m;
                        best_group = k;
                      }
                    }
                  }
                  add_v3_v3(face_nors[best], normals_queue[best_group]);
                  normalize_v3(face_nors[best]);
                  nor_ofs[best] = (nor_ofs[best] + normals_queue[best_group][3]) * 0.5f;
                  queue_index--;
                  memmove(normals_queue + best_group,
                          normals_queue + best_group + 1,
                          (queue_index - best_group) * sizeof(*normals_queue));
                }
              }
              MEM_freeN(normals_queue);

              /* When up to 3 constraint normals are found. */
              if (ELEM(face_nors_len, 2, 3)) {
                const float q = dot_v3v3(face_nors[0], face_nors[1]);
                float d = 1.0f - q * q;
                cross_v3_v3v3(move_nor, face_nors[0], face_nors[1]);
                if (d > FLT_EPSILON * 10 && q < stop_explosion) {
                  d = 1.0f / d;
                  mul_v3_fl(face_nors[0], (nor_ofs[0] - nor_ofs[1] * q) * d);
                  mul_v3_fl(face_nors[1], (nor_ofs[1] - nor_ofs[0] * q) * d);
                }
                else {
                  d = 1.0f / (fabsf(q) + 1.0f);
                  mul_v3_fl(face_nors[0], nor_ofs[0] * d);
                  mul_v3_fl(face_nors[1], nor_ofs[1] * d);
                }
                add_v3_v3v3(nor, face_nors[0], face_nors[1]);
                if (face_nors_len == 3) {
                  float *free_nor = move_nor;
                  mul_v3_fl(face_nors[2], nor_ofs[2]);
                  d = dot_v3v3(face_nors[2], free_nor);
                  if (LIKELY(fabsf(d) > FLT_EPSILON)) {
                    sub_v3_v3v3(face_nors[0], nor, face_nors[2]); /* Override face_nor[0]. */
                    mul_v3_fl(free_nor, dot_v3v3(face_nors[2], face_nors[0]) / d);
                    sub_v3_v3(nor, free_nor);
                  }
                  disable_boundary_fix = true;
                }
              }
              else {
                BLI_assert(face_nors_len < 2);
                mul_v3_v3fl(nor, face_nors[0], nor_ofs[0]);
                disable_boundary_fix = true;
              }
            }
            /* Fixed/Even Method. */
            else {
              float total_angle = 0;
              float total_angle_back = 0;
              NewEdgeRef *first_edge = NULL;
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
                    if (face && (first_edge == NULL ||
                                 (first_edge->faces[0] != face && first_edge->faces[1] != face))) {
                      float angle = 1.0f;
                      float ofs = face->reversed ? -ofs_back_clamped : ofs_front_clamped;
                      /* Use face_weight here to make faces thinner. */
                      if (do_flat_faces) {
                        ofs *= face_weight[face->index];
                      }

                      if (smd->nonmanifold_offset_mode ==
                          MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN) {
                        MLoop *ml_next = orig_mloop + face->face->loopstart;
                        ml = ml_next + (face->face->totloop - 1);
                        MLoop *ml_prev = ml - 1;
                        for (int m = 0; m < face->face->totloop && vm[ml->v] != i;
                             m++, ml_next++) {
                          ml_prev = ml;
                          ml = ml_next;
                        }
                        angle = angle_v3v3v3(orig_mvert_co[vm[ml_prev->v]],
                                             orig_mvert_co[i],
                                             orig_mvert_co[vm[ml_next->v]]);
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
                      mul_v3_v3fl(face_nor, poly_nors[face->index], angle * ofs);
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
                edge_ptr = g->edges + 1;
                float tmp[3];
                uint k;
                for (k = 1; k + 1 < g->edges_len; k++, edge_ptr++) {
                  MEdge *e = orig_medge + (*edge_ptr)->old_edge;
                  sub_v3_v3v3(
                      tmp, orig_mvert_co[vm[e->v1] == i ? e->v2 : e->v1], orig_mvert_co[i]);
                  add_v3_v3(move_nor, tmp);
                }
                if (k == 1) {
                  disable_boundary_fix = true;
                }
                else {
                  disable_boundary_fix = normalize_v3(move_nor) == 0.0f;
                }
              }
              else {
                disable_boundary_fix = true;
              }
            }
            /* Fix boundary verts. */
            if (!disable_boundary_fix) {
              /* Constraint normal, nor * constr_nor == 0 after this fix. */
              float constr_nor[3];
              MEdge *e0_edge = orig_medge + g->edges[0]->old_edge;
              MEdge *e1_edge = orig_medge + g->edges[g->edges_len - 1]->old_edge;
              float e0[3];
              float e1[3];
              sub_v3_v3v3(e0,
                          orig_mvert_co[vm[e0_edge->v1] == i ? e0_edge->v2 : e0_edge->v1],
                          orig_mvert_co[i]);
              sub_v3_v3v3(e1,
                          orig_mvert_co[vm[e1_edge->v1] == i ? e1_edge->v2 : e1_edge->v1],
                          orig_mvert_co[i]);
              if (smd->nonmanifold_boundary_mode == MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT) {
                cross_v3_v3v3(constr_nor, e0, e1);
              }
              else {
                float f0[3];
                float f1[3];
                if (g->edges[0]->faces[0]->reversed) {
                  negate_v3_v3(f0, poly_nors[g->edges[0]->faces[0]->index]);
                }
                else {
                  copy_v3_v3(f0, poly_nors[g->edges[0]->faces[0]->index]);
                }
                if (g->edges[g->edges_len - 1]->faces[0]->reversed) {
                  negate_v3_v3(f1, poly_nors[g->edges[g->edges_len - 1]->faces[0]->index]);
                }
                else {
                  copy_v3_v3(f1, poly_nors[g->edges[g->edges_len - 1]->faces[0]->index]);
                }
                float n0[3];
                float n1[3];
                cross_v3_v3v3(n0, e0, f0);
                cross_v3_v3v3(n1, f1, e1);
                normalize_v3(n0);
                normalize_v3(n1);
                add_v3_v3v3(constr_nor, n0, n1);
              }
              float d = dot_v3v3(constr_nor, move_nor);
              if (LIKELY(fabsf(d) > FLT_EPSILON)) {
                mul_v3_fl(move_nor, dot_v3v3(constr_nor, nor) / d);
                sub_v3_v3(nor, move_nor);
              }
            }
            float scalar_vgroup = 1;
            /* Use vertex group. */
            if (dvert && !do_flat_faces) {
              MDeformVert *dv = &dvert[i];
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
                    if (e_ang > angle) {
                      angle = e_ang;
                    }
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

  /* TODO create vertdata for intersection fixes (intersection fixing per topology region). */

  /* Correction for adjacent one sided groups around a vert to
   * prevent edge duplicates and null polys. */
  uint(*singularity_edges)[2] = NULL;
  uint totsingularity = 0;
  if (has_singularities) {
    has_singularities = false;
    uint i = 0;
    uint singularity_edges_len = 1;
    singularity_edges = MEM_malloc_arrayN(
        singularity_edges_len, sizeof(*singularity_edges), "singularity_edges in solidify");
    for (NewEdgeRef ***new_edges = orig_edge_data_arr; i < numEdges; i++, new_edges++) {
      if (*new_edges && (do_shell || edge_adj_faces_len[i] == 1) && (**new_edges)->old_edge == i) {
        for (NewEdgeRef **l = *new_edges; *l; l++) {
          if ((*l)->link_edge_groups[0]->is_singularity &&
              (*l)->link_edge_groups[1]->is_singularity) {
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
                singularity_edges = MEM_reallocN_id(singularity_edges,
                                                    singularity_edges_len *
                                                        sizeof(*singularity_edges),
                                                    "singularity_edges in solidify");
              }
              singularity_edges[totsingularity][0] = v1;
              singularity_edges[totsingularity][1] = v2;
              totsingularity++;
              if (edge_adj_faces_len[i] == 1 && do_rim) {
                numNewLoops -= 2;
                numNewPolys--;
              }
            }
            else {
              numNewEdges--;
            }
          }
        }
      }
    }
  }

  /* Create Mesh *result with proper capacity. */
  result = BKE_mesh_new_nomain_from_template(
      mesh, (int)(numNewVerts), (int)(numNewEdges), 0, (int)(numNewLoops), (int)(numNewPolys));

  mpoly = result->mpoly;
  mloop = result->mloop;
  medge = result->medge;
  mvert = result->mvert;

  int *origindex_edge = CustomData_get_layer(&result->edata, CD_ORIGINDEX);
  int *origindex_poly = CustomData_get_layer(&result->pdata, CD_ORIGINDEX);

  if (bevel_convex != 0.0f) {
    /* make sure bweight is enabled */
    result->cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
  }

  /* Checks that result has dvert data. */
  if (shell_defgrp_index != -1 || rim_defgrp_index != -1) {
    dvert = CustomData_duplicate_referenced_layer(&result->vdata, CD_MDEFORMVERT, result->totvert);
    /* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
    if (dvert == NULL) {
      /* Add a valid data layer! */
      dvert = CustomData_add_layer(
          &result->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, result->totvert);
    }
    result->dvert = dvert;
  }

  /* Make_new_verts. */
  {
    gs_ptr = orig_vert_groups_arr;
    for (uint i = 0; i < numVerts; i++, gs_ptr++) {
      EdgeGroup *gs = *gs_ptr;
      if (gs) {
        EdgeGroup *g = gs;
        for (uint j = 0; g->valid; j++, g++) {
          if (g->new_vert != MOD_SOLIDIFY_EMPTY_TAG) {
            CustomData_copy_data(&mesh->vdata, &result->vdata, (int)i, (int)g->new_vert, 1);
            copy_v3_v3(mvert[g->new_vert].co, g->co);
            mvert[g->new_vert].flag = orig_mvert[i].flag;
          }
        }
      }
    }
  }

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  /* Make edges. */
  {
    uint i = 0;
    edge_index += totsingularity;
    for (NewEdgeRef ***new_edges = orig_edge_data_arr; i < numEdges; i++, new_edges++) {
      if (*new_edges && (do_shell || edge_adj_faces_len[i] == 1) && (**new_edges)->old_edge == i) {
        for (NewEdgeRef **l = *new_edges; *l; l++) {
          if ((*l)->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
            const uint v1 = (*l)->link_edge_groups[0]->new_vert;
            const uint v2 = (*l)->link_edge_groups[1]->new_vert;
            uint insert = edge_index;
            if (has_singularities && ((*l)->link_edge_groups[0]->is_singularity &&
                                      (*l)->link_edge_groups[1]->is_singularity)) {
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
            CustomData_copy_data(&mesh->edata, &result->edata, (int)i, (int)insert, 1);
            BLI_assert(v1 != MOD_SOLIDIFY_EMPTY_TAG);
            BLI_assert(v2 != MOD_SOLIDIFY_EMPTY_TAG);
            medge[insert].v1 = v1;
            medge[insert].v2 = v2;
            medge[insert].flag = orig_medge[(*l)->old_edge].flag | ME_EDGEDRAW | ME_EDGERENDER;
            medge[insert].crease = orig_medge[(*l)->old_edge].crease;
            medge[insert].bweight = orig_medge[(*l)->old_edge].bweight;
            if (bevel_convex != 0.0f && (*l)->faces[1] != NULL) {
              medge[insert].bweight = (char)clamp_i(
                  (int)medge[insert].bweight + (int)(((*l)->angle > M_PI + FLT_EPSILON ?
                                                          clamp_f(bevel_convex, 0.0f, 1.0f) :
                                                          ((*l)->angle < M_PI - FLT_EPSILON ?
                                                               clamp_f(bevel_convex, -1.0f, 0.0f) :
                                                               0)) *
                                                     255),
                  0,
                  255);
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
     * Note: that there can be vertices that only have one group. They are called singularities.
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
    for (uint i = 0; i < numVerts; i++, gs_ptr++) {
      EdgeGroup *gs = *gs_ptr;
      /* check if the vertex is present (may be dissolved because of proximity) */
      if (gs) {
        printf("%d:\n", i);
        for (EdgeGroup *g = gs; g->valid; g++) {
          NewEdgeRef **e = g->edges;
          for (uint j = 0; j < g->edges_len; j++, e++) {
            printf("%u/%d, ", (*e)->old_edge, (int)(*e)->new_edge);
          }
          printf("(tg:%u)(s:%u,c:%d)\n", g->topo_group, g->split, g->is_orig_closed);
        }
      }
    }
  }
#endif

  /* Make boundary edges/faces. */
  {
    gs_ptr = orig_vert_groups_arr;
    mv = orig_mvert;
    for (uint i = 0; i < numVerts; i++, gs_ptr++, mv++) {
      EdgeGroup *gs = *gs_ptr;
      if (gs) {
        EdgeGroup *g = gs;
        EdgeGroup *g2 = gs;
        EdgeGroup *last_g = NULL;
        EdgeGroup *first_g = NULL;
        /* Data calculation cache. */
        char max_crease;
        char last_max_crease = 0;
        char first_max_crease = 0;
        char max_bweight;
        char last_max_bweight = 0;
        char first_max_bweight = 0;
        short flag;
        short last_flag = 0;
        short first_flag = 0;
        for (uint j = 0; g->valid; g++) {
          if ((do_rim && !g->is_orig_closed) || (do_shell && g->split)) {
            max_crease = 0;
            max_bweight = 0;
            flag = 0;

            BLI_assert(g->edges_len >= 2);

            if (g->edges_len == 2) {
              max_crease = min_cc(orig_medge[g->edges[0]->old_edge].crease,
                                  orig_medge[g->edges[1]->old_edge].crease);
            }
            else {
              for (uint k = 1; k < g->edges_len - 1; k++) {
                ed = orig_medge + g->edges[k]->old_edge;
                if (ed->crease > max_crease) {
                  max_crease = ed->crease;
                }
                if (g->edges[k]->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
                  char bweight = medge[g->edges[k]->new_edge].bweight;
                  if (bweight > max_bweight) {
                    max_bweight = bweight;
                  }
                }
                flag |= ed->flag;
              }
            }

            const char bweight_open_edge = min_cc(
                orig_medge[g->edges[0]->old_edge].bweight,
                orig_medge[g->edges[g->edges_len - 1]->old_edge].bweight);
            if (bweight_open_edge > 0) {
              max_bweight = min_cc(bweight_open_edge, max_bweight);
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
              first_flag = flag;
            }
            else {
              last_g->open_face_edge = edge_index;
              CustomData_copy_data(&mesh->edata,
                                   &result->edata,
                                   (int)last_g->edges[0]->old_edge,
                                   (int)edge_index,
                                   1);
              if (origindex_edge) {
                origindex_edge[edge_index] = ORIGINDEX_NONE;
              }
              medge[edge_index].v1 = last_g->new_vert;
              medge[edge_index].v2 = g->new_vert;
              medge[edge_index].flag = ME_EDGEDRAW | ME_EDGERENDER |
                                       ((last_flag | flag) & (ME_SEAM | ME_SHARP));
              medge[edge_index].crease = min_cc(last_max_crease, max_crease);
              medge[edge_index++].bweight = max_cc(mv->bweight,
                                                   min_cc(last_max_bweight, max_bweight));
            }
            last_g = g;
            last_max_crease = max_crease;
            last_max_bweight = max_bweight;
            last_flag = flag;
            j++;
          }
          if (!(g + 1)->valid || g->topo_group != (g + 1)->topo_group) {
            if (j == 2) {
              last_g->open_face_edge = edge_index - 1;
            }
            if (j > 2) {
              CustomData_copy_data(&mesh->edata,
                                   &result->edata,
                                   (int)last_g->edges[0]->old_edge,
                                   (int)edge_index,
                                   1);
              if (origindex_edge) {
                origindex_edge[edge_index] = ORIGINDEX_NONE;
              }
              last_g->open_face_edge = edge_index;
              medge[edge_index].v1 = last_g->new_vert;
              medge[edge_index].v2 = first_g->new_vert;
              medge[edge_index].flag = ME_EDGEDRAW | ME_EDGERENDER |
                                       ((last_flag | first_flag) & (ME_SEAM | ME_SHARP));
              medge[edge_index].crease = min_cc(last_max_crease, first_max_crease);
              medge[edge_index++].bweight = max_cc(mv->bweight,
                                                   min_cc(last_max_bweight, first_max_bweight));

              /* Loop data. */
              int *loops = MEM_malloc_arrayN(j, sizeof(*loops), "loops in solidify");
              /* The #mat_nr is from consensus. */
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
                    if (g3->edges[0]->faces[0]->face->mat_nr == l) {
                      face = g3->edges[0]->faces[0]->index;
                      count++;
                    }
                    NewEdgeRef *le = g3->edges[g3->edges_len - 1];
                    if (le->faces[1] && le->faces[1]->face->mat_nr == l) {
                      face = le->faces[1]->index;
                      count++;
                    }
                    else if (!le->faces[1] && le->faces[0]->face->mat_nr == l) {
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
                  &mesh->pdata, &result->pdata, (int)most_mat_nr_face, (int)poly_index, 1);
              if (origindex_poly) {
                origindex_poly[poly_index] = ORIGINDEX_NONE;
              }
              mpoly[poly_index].loopstart = (int)loop_index;
              mpoly[poly_index].totloop = (int)j;
              mpoly[poly_index].mat_nr = most_mat_nr +
                                         (g->is_orig_closed || !do_rim ? 0 : mat_ofs_rim);
              CLAMP(mpoly[poly_index].mat_nr, 0, mat_nr_max);
              mpoly[poly_index].flag = orig_mpoly[most_mat_nr_face].flag;
              poly_index++;

              for (uint k = 0; g2->valid && k < j; g2++) {
                if ((do_rim && !g2->is_orig_closed) || (do_shell && g2->split)) {
                  MPoly *face = g2->edges[0]->faces[0]->face;
                  ml = orig_mloop + face->loopstart;
                  for (int l = 0; l < face->totloop; l++, ml++) {
                    if (vm[ml->v] == i) {
                      loops[k] = face->loopstart + l;
                      break;
                    }
                  }
                  k++;
                }
              }

              if (!do_flip) {
                for (uint k = 0; k < j; k++) {
                  CustomData_copy_data(&mesh->ldata, &result->ldata, loops[k], (int)loop_index, 1);
                  mloop[loop_index].v = medge[edge_index - j + k].v1;
                  mloop[loop_index++].e = edge_index - j + k;
                }
              }
              else {
                for (uint k = 1; k <= j; k++) {
                  CustomData_copy_data(
                      &mesh->ldata, &result->ldata, loops[j - k], (int)loop_index, 1);
                  mloop[loop_index].v = medge[edge_index - k].v2;
                  mloop[loop_index++].e = edge_index - k;
                }
              }
              MEM_freeN(loops);
            }
            /* Reset everything for the next poly. */
            j = 0;
            last_g = NULL;
            first_g = NULL;
            last_max_crease = 0;
            first_max_crease = 0;
            last_max_bweight = 0;
            first_max_bweight = 0;
            last_flag = 0;
            first_flag = 0;
          }
        }
      }
    }
  }

  /* Make boundary faces. */
  if (do_rim) {
    for (uint i = 0; i < numEdges; i++) {
      if (edge_adj_faces_len[i] == 1 && orig_edge_data_arr[i] &&
          (*orig_edge_data_arr[i])->old_edge == i) {
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

        MPoly *face = (*new_edges)->faces[0]->face;
        CustomData_copy_data(
            &mesh->pdata, &result->pdata, (int)(*new_edges)->faces[0]->index, (int)poly_index, 1);
        mpoly[poly_index].loopstart = (int)loop_index;
        mpoly[poly_index].totloop = 4 - (int)(v1_singularity || v2_singularity);
        mpoly[poly_index].mat_nr = face->mat_nr + mat_ofs_rim;
        CLAMP(mpoly[poly_index].mat_nr, 0, mat_nr_max);
        mpoly[poly_index].flag = face->flag;
        poly_index++;

        int loop1 = -1;
        int loop2 = -1;
        ml = orig_mloop + face->loopstart;
        const uint old_v1 = vm[orig_medge[edge1->old_edge].v1];
        const uint old_v2 = vm[orig_medge[edge1->old_edge].v2];
        for (uint j = 0; j < face->totloop; j++, ml++) {
          if (vm[ml->v] == old_v1) {
            loop1 = face->loopstart + (int)j;
          }
          else if (vm[ml->v] == old_v2) {
            loop2 = face->loopstart + (int)j;
          }
        }
        BLI_assert(loop1 != -1 && loop2 != -1);
        MEdge *open_face_edge;
        uint open_face_edge_index;
        if (!do_flip) {
          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&result->dvert[medge[edge1->new_edge].v1], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(&mesh->ldata, &result->ldata, loop1, (int)loop_index, 1);
          mloop[loop_index].v = medge[edge1->new_edge].v1;
          mloop[loop_index++].e = edge1->new_edge;

          if (!v2_singularity) {
            open_face_edge_index = edge1->link_edge_groups[1]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&result->dvert[medge[edge1->new_edge].v2], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(&mesh->ldata, &result->ldata, loop2, (int)loop_index, 1);
            mloop[loop_index].v = medge[edge1->new_edge].v2;
            open_face_edge = medge + open_face_edge_index;
            if (ELEM(medge[edge2->new_edge].v2, open_face_edge->v1, open_face_edge->v2)) {
              mloop[loop_index++].e = open_face_edge_index;
            }
            else {
              mloop[loop_index++].e = edge2->link_edge_groups[1]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&result->dvert[medge[edge2->new_edge].v2], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(&mesh->ldata, &result->ldata, loop2, (int)loop_index, 1);
          mloop[loop_index].v = medge[edge2->new_edge].v2;
          mloop[loop_index++].e = edge2->new_edge;

          if (!v1_singularity) {
            open_face_edge_index = edge2->link_edge_groups[0]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&result->dvert[medge[edge2->new_edge].v1], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(&mesh->ldata, &result->ldata, loop1, (int)loop_index, 1);
            mloop[loop_index].v = medge[edge2->new_edge].v1;
            open_face_edge = medge + open_face_edge_index;
            if (ELEM(medge[edge1->new_edge].v1, open_face_edge->v1, open_face_edge->v2)) {
              mloop[loop_index++].e = open_face_edge_index;
            }
            else {
              mloop[loop_index++].e = edge1->link_edge_groups[0]->open_face_edge;
            }
          }
        }
        else {
          if (!v1_singularity) {
            open_face_edge_index = edge1->link_edge_groups[0]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&result->dvert[medge[edge1->new_edge].v1], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(&mesh->ldata, &result->ldata, loop1, (int)loop_index, 1);
            mloop[loop_index].v = medge[edge1->new_edge].v1;
            open_face_edge = medge + open_face_edge_index;
            if (ELEM(medge[edge2->new_edge].v1, open_face_edge->v1, open_face_edge->v2)) {
              mloop[loop_index++].e = open_face_edge_index;
            }
            else {
              mloop[loop_index++].e = edge2->link_edge_groups[0]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&result->dvert[medge[edge2->new_edge].v1], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(&mesh->ldata, &result->ldata, loop1, (int)loop_index, 1);
          mloop[loop_index].v = medge[edge2->new_edge].v1;
          mloop[loop_index++].e = edge2->new_edge;

          if (!v2_singularity) {
            open_face_edge_index = edge2->link_edge_groups[1]->open_face_edge;
            if (rim_defgrp_index != -1) {
              BKE_defvert_ensure_index(&result->dvert[medge[edge2->new_edge].v2], rim_defgrp_index)
                  ->weight = 1.0f;
            }
            CustomData_copy_data(&mesh->ldata, &result->ldata, loop2, (int)loop_index, 1);
            mloop[loop_index].v = medge[edge2->new_edge].v2;
            open_face_edge = medge + open_face_edge_index;
            if (ELEM(medge[edge1->new_edge].v2, open_face_edge->v1, open_face_edge->v2)) {
              mloop[loop_index++].e = open_face_edge_index;
            }
            else {
              mloop[loop_index++].e = edge1->link_edge_groups[1]->open_face_edge;
            }
          }

          if (rim_defgrp_index != -1) {
            BKE_defvert_ensure_index(&result->dvert[medge[edge1->new_edge].v2], rim_defgrp_index)
                ->weight = 1.0f;
          }
          CustomData_copy_data(&mesh->ldata, &result->ldata, loop2, (int)loop_index, 1);
          mloop[loop_index].v = medge[edge1->new_edge].v2;
          mloop[loop_index++].e = edge1->new_edge;
        }
      }
    }
  }

  /* Make faces. */
  if (do_shell) {
    NewFaceRef *fr = face_sides_arr;
    uint *face_loops = MEM_malloc_arrayN(
        largest_ngon * 2, sizeof(*face_loops), "face_loops in solidify");
    uint *face_verts = MEM_malloc_arrayN(
        largest_ngon * 2, sizeof(*face_verts), "face_verts in solidify");
    uint *face_edges = MEM_malloc_arrayN(
        largest_ngon * 2, sizeof(*face_edges), "face_edges in solidify");
    for (uint i = 0; i < numPolys * 2; i++, fr++) {
      const uint loopstart = (uint)fr->face->loopstart;
      uint totloop = (uint)fr->face->totloop;
      uint valid_edges = 0;
      uint k = 0;
      while (totloop > 0 && (!fr->link_edges[totloop - 1] ||
                             fr->link_edges[totloop - 1]->new_edge == MOD_SOLIDIFY_EMPTY_TAG)) {
        totloop--;
      }
      if (totloop > 0) {
        NewEdgeRef *prior_edge = fr->link_edges[totloop - 1];
        uint prior_flip = (uint)(vm[orig_medge[prior_edge->old_edge].v1] ==
                                 vm[orig_mloop[loopstart + (totloop - 1)].v]);
        for (uint j = 0; j < totloop; j++) {
          NewEdgeRef *new_edge = fr->link_edges[j];
          if (new_edge && new_edge->new_edge != MOD_SOLIDIFY_EMPTY_TAG) {
            valid_edges++;
            const uint flip = (uint)(vm[orig_medge[new_edge->old_edge].v2] ==
                                     vm[orig_mloop[loopstart + j].v]);
            BLI_assert(flip ||
                       vm[orig_medge[new_edge->old_edge].v1] == vm[orig_mloop[loopstart + j].v]);
            /* The vert that's in the current loop. */
            const uint new_v1 = new_edge->link_edge_groups[flip]->new_vert;
            /* The vert that's in the next loop. */
            const uint new_v2 = new_edge->link_edge_groups[1 - flip]->new_vert;
            if (k == 0 || face_verts[k - 1] != new_v1) {
              face_loops[k] = loopstart + j;
              if (fr->reversed) {
                face_edges[k] = prior_edge->link_edge_groups[prior_flip]->open_face_edge;
              }
              else {
                face_edges[k] = new_edge->link_edge_groups[flip]->open_face_edge;
              }
              BLI_assert(k == 0 || medge[face_edges[k]].v2 == face_verts[k - 1] ||
                         medge[face_edges[k]].v1 == face_verts[k - 1]);
              BLI_assert(face_edges[k] == MOD_SOLIDIFY_EMPTY_TAG ||
                         medge[face_edges[k]].v2 == new_v1 || medge[face_edges[k]].v1 == new_v1);
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
          CustomData_copy_data(&mesh->pdata, &result->pdata, (int)(i / 2), (int)poly_index, 1);
          mpoly[poly_index].loopstart = (int)loop_index;
          mpoly[poly_index].totloop = (int)k;
          mpoly[poly_index].mat_nr = fr->face->mat_nr + (fr->reversed != do_flip ? mat_ofs : 0);
          CLAMP(mpoly[poly_index].mat_nr, 0, mat_nr_max);
          mpoly[poly_index].flag = fr->face->flag;
          if (fr->reversed != do_flip) {
            for (int l = (int)k - 1; l >= 0; l--) {
              if (shell_defgrp_index != -1) {
                BKE_defvert_ensure_index(&result->dvert[face_verts[l]], shell_defgrp_index)
                    ->weight = 1.0f;
              }
              CustomData_copy_data(
                  &mesh->ldata, &result->ldata, (int)face_loops[l], (int)loop_index, 1);
              mloop[loop_index].v = face_verts[l];
              mloop[loop_index++].e = face_edges[l];
            }
          }
          else {
            uint l = k - 1;
            for (uint next_l = 0; next_l < k; next_l++) {
              CustomData_copy_data(
                  &mesh->ldata, &result->ldata, (int)face_loops[l], (int)loop_index, 1);
              mloop[loop_index].v = face_verts[l];
              mloop[loop_index++].e = face_edges[next_l];
              l = next_l;
            }
          }
          poly_index++;
        }
      }
    }
    MEM_freeN(face_loops);
    MEM_freeN(face_verts);
    MEM_freeN(face_edges);
  }
  if (edge_index != numNewEdges) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: edges array wrong size: %u instead of %u",
                           numNewEdges,
                           edge_index);
  }
  if (poly_index != numNewPolys) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: polys array wrong size: %u instead of %u",
                           numNewPolys,
                           poly_index);
  }
  if (loop_index != numNewLoops) {
    BKE_modifier_set_error(ctx->object,
                           md,
                           "Internal Error: loops array wrong size: %u instead of %u",
                           numNewLoops,
                           loop_index);
  }
  BLI_assert(edge_index == numNewEdges);
  BLI_assert(poly_index == numNewPolys);
  BLI_assert(loop_index == numNewLoops);

  /* Free remaining memory */
  {
    MEM_freeN(vm);
    MEM_freeN(edge_adj_faces_len);
    uint i = 0;
    for (EdgeGroup **p = orig_vert_groups_arr; i < numVerts; i++, p++) {
      if (*p) {
        for (EdgeGroup *eg = *p; eg->valid; eg++) {
          MEM_freeN(eg->edges);
        }
        MEM_freeN(*p);
      }
    }
    MEM_freeN(orig_vert_groups_arr);
    i = numEdges;
    for (NewEdgeRef ***p = orig_edge_data_arr + (numEdges - 1); i > 0; i--, p--) {
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
    for (NewFaceRef *p = face_sides_arr; i < numPolys * 2; i++, p++) {
      MEM_freeN(p->link_edges);
    }
    MEM_freeN(face_sides_arr);
    MEM_freeN(poly_nors);
  }

#undef MOD_SOLIDIFY_EMPTY_TAG

  return result;
}

/** \} */
