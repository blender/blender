/* SPDX-FileCopyrightText: 2011-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>

#include "CLG_log.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_ordered_edge.hh"
#include "BLI_sort.hh"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"

#include "DEG_depsgraph.hh"

#include "MEM_guardedalloc.h"

using blender::float3;
using blender::MutableSpan;
using blender::Span;

/* corner v/e are unsigned, so using max uint_32 value as invalid marker... */
#define INVALID_CORNER_EDGE_MARKER 4294967295u

static CLG_LogRef LOG = {"bke.mesh"};

void strip_loose_faces_corners(Mesh *mesh, blender::BitSpan faces_to_remove);
void mesh_strip_edges(Mesh *mesh);

/* -------------------------------------------------------------------- */
/** \name Internal functions
 * \{ */

union EdgeUUID {
  uint32_t verts[2];
  int64_t edval;
};

struct SortFaceLegacy {
  EdgeUUID es[4];
  uint index;
};

/* Used to detect faces using exactly the same vertices. */
/* Used to detect corners used by no (disjoint) or more than one (intersect) faces. */
struct SortFace {
  int *verts = nullptr;
  int numverts = 0;
  int corner_start = 0;
  uint index = 0;
  bool invalid = false;
};

static void edge_store_assign(uint32_t verts[2], const uint32_t v1, const uint32_t v2)
{
  if (v1 < v2) {
    verts[0] = v1;
    verts[1] = v2;
  }
  else {
    verts[0] = v2;
    verts[1] = v1;
  }
}

static void edge_store_from_mface_quad(EdgeUUID es[4], const MFace *mf)
{
  edge_store_assign(es[0].verts, mf->v1, mf->v2);
  edge_store_assign(es[1].verts, mf->v2, mf->v3);
  edge_store_assign(es[2].verts, mf->v3, mf->v4);
  edge_store_assign(es[3].verts, mf->v4, mf->v1);
}

static void edge_store_from_mface_tri(EdgeUUID es[4], const MFace *mf)
{
  edge_store_assign(es[0].verts, mf->v1, mf->v2);
  edge_store_assign(es[1].verts, mf->v2, mf->v3);
  edge_store_assign(es[2].verts, mf->v3, mf->v1);
  es[3].verts[0] = es[3].verts[1] = UINT_MAX;
}

static bool search_legacy_face_cmp(const SortFaceLegacy &sfa, const SortFaceLegacy &sfb)
{
  if (sfa.es[0].edval != sfb.es[0].edval) {
    return sfa.es[0].edval < sfb.es[0].edval;
  }
  if (sfa.es[1].edval != sfb.es[1].edval) {
    return sfa.es[1].edval < sfb.es[1].edval;
  }
  if (sfa.es[2].edval != sfb.es[2].edval) {
    return sfa.es[2].edval < sfb.es[2].edval;
  }
  return sfa.es[3].edval < sfb.es[3].edval;
}

static bool search_face_cmp(const SortFace &sp1, const SortFace &sp2)
{
  /* Reject all invalid faces at end of list! */
  if (sp1.invalid || sp2.invalid) {
    return sp1.invalid < sp2.invalid;
  }
  /* Else, sort on first non-equal verts (remember verts of valid faces are sorted). */
  const int max_idx = std::min(sp1.numverts, sp2.numverts);
  for (int idx = 0; idx < max_idx; idx++) {
    const int v1_i = sp1.verts[idx];
    const int v2_i = sp2.verts[idx];
    if (v1_i != v2_i) {
      return v1_i < v2_i;
    }
  }
  return sp1.numverts < sp2.numverts;
}

static bool search_face_corner_cmp(const SortFace &sp1, const SortFace &sp2)
{
  /* Reject all invalid faces at end of list! */
  if (sp1.invalid || sp2.invalid) {
    return sp1.invalid < sp2.invalid;
  }
  /* Else, sort on corner start. */
  return sp1.corner_start < sp2.corner_start;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Validation
 * \{ */

#define PRINT_MSG(...) \
  if (do_verbose) { \
    CLOG_INFO(&LOG, 1, __VA_ARGS__); \
  } \
  ((void)0)

#define PRINT_ERR(...) \
  do { \
    is_valid = false; \
    if (do_verbose) { \
      CLOG_ERROR(&LOG, __VA_ARGS__); \
    } \
  } while (0)

/* NOLINTNEXTLINE: readability-function-size */
bool BKE_mesh_validate_arrays(Mesh *mesh,
                              float (*vert_positions)[3],
                              uint verts_num,
                              blender::int2 *edges,
                              uint edges_num,
                              MFace *legacy_faces,
                              uint legacy_faces_num,
                              const int *corner_verts,
                              int *corner_edges,
                              uint corners_num,
                              const int *face_offsets,
                              uint faces_num,
                              MDeformVert *dverts, /* assume verts_num length */
                              const bool do_verbose,
                              const bool do_fixes,
                              bool *r_changed)
{
  using namespace blender;
  using namespace blender::bke;
#define REMOVE_EDGE_TAG(_me) \
  { \
    _me[0] = _me[1]; \
    free_flag.edges = do_fixes; \
  } \
  (void)0
#define IS_REMOVED_EDGE(_me) (_me[0] == _me[1])

#define REMOVE_CORNER_TAG(corner) \
  { \
    corner_edges[corner] = INVALID_CORNER_EDGE_MARKER; \
    free_flag.face_corners = do_fixes; \
  } \
  (void)0
  blender::BitVector<> faces_to_remove(faces_num);

  blender::bke::AttributeWriter<int> material_indices =
      mesh->attributes_for_write().lookup_for_write<int>("material_index");
  blender::MutableVArraySpan<int> material_indices_span(material_indices.varray);

  uint i, j;
  int *v;

  bool is_valid = true;

  union {
    struct {
      int verts : 1;
      int verts_weight : 1;
      int corners_edge : 1;
    };
    int as_flag;
  } fix_flag;

  union {
    struct {
      int edges : 1;
      int faces : 1;
      /* This regroups corners and faces! */
      int face_corners : 1;
      int mselect : 1;
    };
    int as_flag;
  } free_flag;

  union {
    struct {
      int edges : 1;
    };
    int as_flag;
  } recalc_flag;

  Map<OrderedEdge, int> edge_hash;
  edge_hash.reserve(edges_num);

  BLI_assert(!(do_fixes && mesh == nullptr));

  fix_flag.as_flag = 0;
  free_flag.as_flag = 0;
  recalc_flag.as_flag = 0;

  PRINT_MSG("verts(%u), edges(%u), corners(%u), faces(%u)",
            verts_num,
            edges_num,
            corners_num,
            faces_num);

  if (edges_num == 0 && faces_num != 0) {
    PRINT_ERR("\tLogical error, %u faces and 0 edges", faces_num);
    recalc_flag.edges = do_fixes;
  }

  for (i = 0; i < verts_num; i++) {
    for (j = 0; j < 3; j++) {
      if (!isfinite(vert_positions[i][j])) {
        PRINT_ERR("\tVertex %u: has invalid coordinate", i);

        if (do_fixes) {
          zero_v3(vert_positions[i]);

          fix_flag.verts = true;
        }
      }
    }
  }

  for (i = 0; i < edges_num; i++) {
    blender::int2 &edge = edges[i];
    bool remove = false;

    if (edge[0] == edge[1]) {
      PRINT_ERR("\tEdge %u: has matching verts, both %d", i, edge[0]);
      remove = do_fixes;
    }
    if (edge[0] >= verts_num) {
      PRINT_ERR("\tEdge %u: v1 index out of range, %d", i, edge[0]);
      remove = do_fixes;
    }
    if (edge[1] >= verts_num) {
      PRINT_ERR("\tEdge %u: v2 index out of range, %d", i, edge[1]);
      remove = do_fixes;
    }

    if ((edge[0] != edge[1]) && edge_hash.contains(edge)) {
      PRINT_ERR("\tEdge %u: is a duplicate of %d", i, edge_hash.lookup(edge));
      remove = do_fixes;
    }

    if (remove == false) {
      if (edge[0] != edge[1]) {
        edge_hash.add(edge, i);
      }
    }
    else {
      REMOVE_EDGE_TAG(edge);
    }
  }

  if (legacy_faces && !face_offsets) {
#define REMOVE_FACE_TAG(_mf) \
  { \
    _mf->v3 = 0; \
    free_flag.faces = do_fixes; \
  } \
  (void)0
#define CHECK_FACE_VERT_INDEX(a, b) \
  if (mf->a == mf->b) { \
    PRINT_ERR("    face %u: verts invalid, " STRINGIFY(a) "/" STRINGIFY(b) " both %u", i, mf->a); \
    remove = do_fixes; \
  } \
  (void)0
#define CHECK_FACE_EDGE(a, b) \
  if (!edge_hash.contains({mf->a, mf->b})) { \
    PRINT_ERR("    face %u: edge " STRINGIFY(a) "/" STRINGIFY(b) " (%u,%u) is missing edge data", \
              i, \
              mf->a, \
              mf->b); \
    recalc_flag.edges = do_fixes; \
  } \
  (void)0

    MFace *mf;
    const MFace *mf_prev;

    Array<SortFaceLegacy> sort_faces(legacy_faces_num);
    SortFaceLegacy *sf;
    SortFaceLegacy *sf_prev;
    uint totsortface = 0;

    PRINT_ERR("No faces, only tessellated Faces");

    for (i = 0, mf = legacy_faces, sf = sort_faces.data(); i < legacy_faces_num; i++, mf++) {
      bool remove = false;
      int fidx;
      uint fv[4];

      fidx = mf->v4 ? 3 : 2;
      do {
        fv[fidx] = *(&(mf->v1) + fidx);
        if (fv[fidx] >= verts_num) {
          PRINT_ERR("\tFace %u: 'v%d' index out of range, %u", i, fidx + 1, fv[fidx]);
          remove = do_fixes;
        }
      } while (fidx--);

      if (remove == false) {
        if (mf->v4) {
          CHECK_FACE_VERT_INDEX(v1, v2);
          CHECK_FACE_VERT_INDEX(v1, v3);
          CHECK_FACE_VERT_INDEX(v1, v4);

          CHECK_FACE_VERT_INDEX(v2, v3);
          CHECK_FACE_VERT_INDEX(v2, v4);

          CHECK_FACE_VERT_INDEX(v3, v4);
        }
        else {
          CHECK_FACE_VERT_INDEX(v1, v2);
          CHECK_FACE_VERT_INDEX(v1, v3);

          CHECK_FACE_VERT_INDEX(v2, v3);
        }

        if (remove == false) {
          if (edges_num) {
            if (mf->v4) {
              CHECK_FACE_EDGE(v1, v2);
              CHECK_FACE_EDGE(v2, v3);
              CHECK_FACE_EDGE(v3, v4);
              CHECK_FACE_EDGE(v4, v1);
            }
            else {
              CHECK_FACE_EDGE(v1, v2);
              CHECK_FACE_EDGE(v2, v3);
              CHECK_FACE_EDGE(v3, v1);
            }
          }

          sf->index = i;

          if (mf->v4) {
            edge_store_from_mface_quad(sf->es, mf);
            std::sort(sf->es, sf->es + 4, [](const EdgeUUID &a, const EdgeUUID &b) {
              return a.edval < b.edval;
            });
          }
          else {
            edge_store_from_mface_tri(sf->es, mf);
            std::sort(sf->es, sf->es + 3, [](const EdgeUUID &a, const EdgeUUID &b) {
              return a.edval < b.edval;
            });
          }

          totsortface++;
          sf++;
        }
      }

      if (remove) {
        REMOVE_FACE_TAG(mf);
      }
    }

    blender::parallel_sort(sort_faces.begin(), sort_faces.end(), search_legacy_face_cmp);

    sf = sort_faces.data();
    sf_prev = sf;
    sf++;

    for (i = 1; i < totsortface; i++, sf++) {
      bool remove = false;

      /* on a valid mesh, code below will never run */
      if (memcmp(sf->es, sf_prev->es, sizeof(sf_prev->es)) == 0) {
        mf = legacy_faces + sf->index;

        if (do_verbose) {
          mf_prev = legacy_faces + sf_prev->index;

          if (mf->v4) {
            PRINT_ERR("\tFace %u & %u: are duplicates (%u,%u,%u,%u) (%u,%u,%u,%u)",
                      sf->index,
                      sf_prev->index,
                      mf->v1,
                      mf->v2,
                      mf->v3,
                      mf->v4,
                      mf_prev->v1,
                      mf_prev->v2,
                      mf_prev->v3,
                      mf_prev->v4);
          }
          else {
            PRINT_ERR("\tFace %u & %u: are duplicates (%u,%u,%u) (%u,%u,%u)",
                      sf->index,
                      sf_prev->index,
                      mf->v1,
                      mf->v2,
                      mf->v3,
                      mf_prev->v1,
                      mf_prev->v2,
                      mf_prev->v3);
          }
        }

        remove = do_fixes;
      }
      else {
        sf_prev = sf;
      }

      if (remove) {
        REMOVE_FACE_TAG(mf);
      }
    }

#undef REMOVE_FACE_TAG
#undef CHECK_FACE_VERT_INDEX
#undef CHECK_FACE_EDGE
  }

  /* Checking corners and faces is a bit tricky, as they are quite intricate...
   *
   * Faces must have:
   * - a valid corner_start value.
   * - a valid corners_num value (>= 3 and corner_start+corners_num < mesh.corners_num).
   *
   * corners must have:
   * - a valid v value.
   * - a valid e value (corresponding to the edge it defines with the next corner in face).
   *
   * Also, corners not used by faces can be discarded.
   * And "intersecting" corners (i.e. corners used by more than one face) are invalid,
   * so be sure to leave at most one face per corner!
   */
  {
    BitVector<> vert_tag(mesh->verts_num);
    Array<SortFace> sort_faces(faces_num);
    Array<int> sort_face_verts(faces_num == 0 ? 0 : face_offsets[faces_num]);
    int64_t sort_face_verts_offset = 0;

    for (const int64_t i : blender::IndexRange(faces_num)) {
      SortFace *sp = &sort_faces[i];
      const int face_start = face_offsets[i];
      const int face_size = face_offsets[i + 1] - face_start;
      sp->index = i;

      /* Material index, isolated from other tests here. While large indices are clamped,
       * negative indices aren't supported by drawing, exporters etc.
       * To check the indices are in range, use #BKE_mesh_validate_material_indices */
      if (material_indices && material_indices_span[i] < 0) {
        PRINT_ERR("\tFace %u has invalid material (%d)", sp->index, material_indices_span[i]);
        if (do_fixes) {
          material_indices_span[i] = 0;
        }
      }

      if (face_start < 0 || face_size < 3) {
        /* Invalid corner data. */
        PRINT_ERR("\tFace %u is invalid (corner_start: %d, corners_num: %d)",
                  sp->index,
                  face_start,
                  face_size);
        sp->invalid = true;
      }
      else if (face_start + face_size > corners_num) {
        /* Invalid corner data. */
        PRINT_ERR(
            "\tFace %u uses corners out of range "
            "(corner_start: %d, corner_end: %d, max number of corners: %u)",
            sp->index,
            face_start,
            face_start + face_size - 1,
            corners_num - 1);
        sp->invalid = true;
      }
      else {
        /* Face itself is valid, for now. */
        int v1, v2; /* v1 is prev corner vert idx, v2 is current corner one. */
        sp->invalid = false;
        sp->verts = v = sort_face_verts.data() + sort_face_verts_offset;
        sort_face_verts_offset += face_size;
        sp->numverts = face_size;
        sp->corner_start = face_start;

        /* Ideally we would only have to do that once on all vertices
         * before we start checking each face, but several faces can use same vert,
         * so we have to ensure here all verts of current face are cleared. */
        for (j = 0; j < face_size; j++) {
          const int vert = corner_verts[sp->corner_start + j];
          if (vert < verts_num) {
            vert_tag[vert].reset();
          }
        }

        /* Test all face's corners' vert idx. */
        for (j = 0; j < face_size; j++, v++) {
          const int vert = corner_verts[sp->corner_start + j];
          if (vert >= verts_num) {
            /* Invalid vert idx. */
            PRINT_ERR("\tCorner %u has invalid vert reference (%d)", sp->corner_start + j, vert);
            sp->invalid = true;
          }
          else if (vert_tag[vert].test()) {
            PRINT_ERR("\tFace %u has duplicated vert reference at corner (%u)", uint(i), j);
            sp->invalid = true;
          }
          else {
            vert_tag[vert].set();
          }
          *v = vert;
        }

        if (sp->invalid) {
          sp++;
          continue;
        }

        /* Test all face's corners. */
        for (j = 0; j < face_size; j++) {
          const int corner = sp->corner_start + j;
          const int vert = corner_verts[corner];
          const int edge_i = corner_edges[corner];
          v1 = vert;
          v2 = corner_verts[sp->corner_start + (j + 1) % face_size];
          if (!edge_hash.contains({v1, v2})) {
            /* Edge not existing. */
            PRINT_ERR("\tFace %u needs missing edge (%d, %d)", sp->index, v1, v2);
            if (do_fixes) {
              recalc_flag.edges = true;
            }
            else {
              sp->invalid = true;
            }
          }
          else if (edge_i >= edges_num) {
            /* Invalid edge idx.
             * We already know from previous text that a valid edge exists, use it (if allowed)! */
            if (do_fixes) {
              int prev_e = edge_i;
              corner_edges[corner] = edge_hash.lookup({v1, v2});
              fix_flag.corners_edge = true;
              PRINT_ERR("\tCorner %d has invalid edge reference (%d), fixed using edge %d",
                        corner,
                        prev_e,
                        corner_edges[corner]);
            }
            else {
              PRINT_ERR("\tCorner %d has invalid edge reference (%d)", corner, edge_i);
              sp->invalid = true;
            }
          }
          else {
            const blender::int2 &edge = edges[edge_i];
            if (IS_REMOVED_EDGE(edge) ||
                !((edge[0] == v1 && edge[1] == v2) || (edge[0] == v2 && edge[1] == v1)))
            {
              /* The pointed edge is invalid (tagged as removed, or vert idx mismatch),
               * and we already know from previous test that a valid one exists,
               * use it (if allowed)! */
              if (do_fixes) {
                int prev_e = edge_i;
                corner_edges[corner] = edge_hash.lookup({v1, v2});
                fix_flag.corners_edge = true;
                PRINT_ERR(
                    "\tFace %u has invalid edge reference (%d, is_removed: %d), fixed using edge "
                    "%d",
                    sp->index,
                    prev_e,
                    IS_REMOVED_EDGE(edge),
                    corner_edges[corner]);
              }
              else {
                PRINT_ERR("\tFace %u has invalid edge reference (%d)", sp->index, edge_i);
                sp->invalid = true;
              }
            }
          }
        }

        if (!sp->invalid) {
          /* Needed for checking faces using same verts below. */
          std::sort(sp->verts, sp->verts + sp->numverts);
        }
      }
      sp++;
    }
    BLI_assert(sort_face_verts_offset <= sort_face_verts.size());

    vert_tag.clear_and_shrink();

    /* Second check pass, testing faces using the same verts. */
    blender::parallel_sort(sort_faces.begin(), sort_faces.end(), search_face_cmp);
    SortFace *sp, *prev_sp;
    sp = prev_sp = sort_faces.data();
    sp++;

    for (i = 1; i < faces_num; i++, sp++) {
      int p1_nv = sp->numverts, p2_nv = prev_sp->numverts;
      const int *p1_v = sp->verts, *p2_v = prev_sp->verts;

      if (sp->invalid) {
        /* Break, because all known invalid faces have been put at the end by the sort above. */
        break;
      }

      /* Test same faces. */
      if ((p1_nv == p2_nv) && (memcmp(p1_v, p2_v, p1_nv * sizeof(*p1_v)) == 0)) {
        if (do_verbose) {
          /* TODO: convert list to string */
          PRINT_ERR("\tFaces %u and %u use same vertices (%d", prev_sp->index, sp->index, *p1_v);
          for (j = 1; j < p1_nv; j++) {
            PRINT_ERR(", %d", p1_v[j]);
          }
          PRINT_ERR("), considering face %u as invalid.", sp->index);
        }
        else {
          is_valid = false;
        }
        sp->invalid = true;
      }
      else {
        prev_sp = sp;
      }
    }

    /* Third check pass, testing corners used by none or more than one face. */
    blender::parallel_sort(sort_faces.begin(), sort_faces.end(), search_face_corner_cmp);
    sp = sort_faces.data();
    prev_sp = nullptr;
    int prev_end = 0;
    for (i = 0; i < faces_num; i++, sp++) {
      /* We don't need the verts anymore, and avoid us another corner! */
      sp->verts = nullptr;

      /* Note above prev_sp: in following code, we make sure it is always valid face (or nullptr).
       */
      if (sp->invalid) {
        if (do_fixes) {
          faces_to_remove[sp->index].set();
          free_flag.face_corners = do_fixes;
          /* DO NOT REMOVE ITS corners!!!
           * As already invalid faces are at the end of the SortFace list, the corners they
           * were the only users have already been tagged as "to remove" during previous
           * iterations, and we don't want to remove some corners that may be used by
           * another valid face! */
        }
      }
      /* Test corners users. */
      else {
        /* Unused corners. */
        if (prev_end < sp->corner_start) {
          int corner;
          for (j = prev_end, corner = prev_end; j < sp->corner_start; j++, corner++) {
            PRINT_ERR("\tCorner %u is unused.", j);
            if (do_fixes) {
              REMOVE_CORNER_TAG(corner);
            }
          }
          prev_end = sp->corner_start + sp->numverts;
          prev_sp = sp;
        }
        /* Multi-used corners. */
        else if (prev_end > sp->corner_start) {
          PRINT_ERR(
              "\tFaces %u and %u share corners from %d to %d, considering face %u as invalid.",
              prev_sp->index,
              sp->index,
              sp->corner_start,
              prev_end,
              sp->index);
          if (do_fixes) {
            faces_to_remove[sp->index].set();
            free_flag.face_corners = do_fixes;
            /* DO NOT REMOVE ITS corners!!!
             * They might be used by some next, valid face!
             * Just not updating prev_end/prev_sp vars is enough to ensure the corners
             * effectively no more needed will be marked as "to be removed"! */
          }
        }
        else {
          prev_end = sp->corner_start + sp->numverts;
          prev_sp = sp;
        }
      }
    }
    /* We may have some remaining unused corners to get rid of! */
    if (prev_end < corners_num) {
      int corner;
      for (j = prev_end, corner = prev_end; j < corners_num; j++, corner++) {
        PRINT_ERR("\tCorner %u is unused.", j);
        if (do_fixes) {
          REMOVE_CORNER_TAG(corner);
        }
      }
    }
  }

  /* fix deform verts */
  if (dverts) {
    MDeformVert *dv;
    for (i = 0, dv = dverts; i < verts_num; i++, dv++) {
      MDeformWeight *dw;

      for (j = 0, dw = dv->dw; j < dv->totweight; j++, dw++) {
        /* NOTE: greater than max defgroups is accounted for in our code, but not < 0. */
        if (!isfinite(dw->weight)) {
          PRINT_ERR("\tVertex deform %u, group %u has weight: %f", i, dw->def_nr, dw->weight);
          if (do_fixes) {
            dw->weight = 0.0f;
            fix_flag.verts_weight = true;
          }
        }
        else if (dw->weight < 0.0f || dw->weight > 1.0f) {
          PRINT_ERR("\tVertex deform %u, group %u has weight: %f", i, dw->def_nr, dw->weight);
          if (do_fixes) {
            CLAMP(dw->weight, 0.0f, 1.0f);
            fix_flag.verts_weight = true;
          }
        }

        /* Not technically incorrect since this is unsigned, however,
         * a value over INT_MAX is almost certainly caused by wrapping an uint. */
        if (dw->def_nr >= INT_MAX) {
          PRINT_ERR("\tVertex deform %u, has invalid group %u", i, dw->def_nr);
          if (do_fixes) {
            BKE_defvert_remove_group(dv, dw);
            fix_flag.verts_weight = true;

            if (dv->dw) {
              /* re-allocated, the new values compensate for stepping
               * within the for corner and may not be valid */
              j--;
              dw = dv->dw + j;
            }
            else { /* all freed */
              break;
            }
          }
        }
      }
    }
  }

#undef REMOVE_EDGE_TAG
#undef IS_REMOVED_EDGE
#undef REMOVE_CORNER_TAG
#undef REMOVE_FACE_TAG

  if (mesh) {
    if (free_flag.faces) {
      BKE_mesh_strip_loose_faces(mesh);
    }

    if (free_flag.face_corners) {
      strip_loose_faces_corners(mesh, faces_to_remove);
    }

    if (free_flag.edges) {
      mesh_strip_edges(mesh);
    }

    if (recalc_flag.edges) {
      mesh_calc_edges(*mesh, true, false);
    }
  }

  if (mesh && mesh->mselect) {
    MSelect *msel;

    for (i = 0, msel = mesh->mselect; i < mesh->totselect; i++, msel++) {
      int tot_elem = 0;

      if (msel->index < 0) {
        PRINT_ERR(
            "\tMesh select element %u type %d index is negative, "
            "resetting selection stack.\n",
            i,
            msel->type);
        free_flag.mselect = do_fixes;
        break;
      }

      switch (msel->type) {
        case ME_VSEL:
          tot_elem = mesh->verts_num;
          break;
        case ME_ESEL:
          tot_elem = mesh->edges_num;
          break;
        case ME_FSEL:
          tot_elem = mesh->faces_num;
          break;
      }

      if (msel->index > tot_elem) {
        PRINT_ERR(
            "\tMesh select element %u type %d index %d is larger than data array size %d, "
            "resetting selection stack.\n",
            i,
            msel->type,
            msel->index,
            tot_elem);

        free_flag.mselect = do_fixes;
        break;
      }
    }

    if (free_flag.mselect) {
      MEM_freeN(mesh->mselect);
      mesh->mselect = nullptr;
      mesh->totselect = 0;
    }
  }

  material_indices_span.save();
  material_indices.finish();

  PRINT_MSG("%s: finished\n\n", __func__);

  *r_changed = (fix_flag.as_flag || free_flag.as_flag || recalc_flag.as_flag);

  BLI_assert((*r_changed == false) || (do_fixes == true));

  return is_valid;
}

static bool mesh_validate_customdata(CustomData *data,
                                     eCustomDataMask mask,
                                     const uint totitems,
                                     const bool do_verbose,
                                     const bool do_fixes,
                                     bool *r_change)
{
  bool is_valid = true;
  bool has_fixes = false;
  int i = 0;

  PRINT_MSG("%s: Checking %d CD layers...\n", __func__, data->totlayer);

  /* Set dummy values so the layer-type is always initialized on first access. */
  int layer_num = -1;
  int layer_num_type = -1;

  while (i < data->totlayer) {
    CustomDataLayer *layer = &data->layers[i];
    const eCustomDataType type = eCustomDataType(layer->type);
    bool ok = true;

    /* Count layers when the type changes. */
    if (layer_num_type != type) {
      layer_num = CustomData_number_of_layers(data, type);
      layer_num_type = type;
    }

    /* Validate active index, for a time this could be set to a negative value, see: #105860. */
    int *active_index_array[] = {
        &layer->active,
        &layer->active_rnd,
        &layer->active_clone,
        &layer->active_mask,
    };
    for (int *active_index : Span(active_index_array, ARRAY_SIZE(active_index_array))) {
      if (*active_index < 0) {
        PRINT_ERR("\tCustomDataLayer type %d has a negative active index (%d)\n",
                  layer->type,
                  *active_index);
        if (do_fixes) {
          *active_index = 0;
          has_fixes = true;
        }
      }
      else {
        if (*active_index >= layer_num) {
          PRINT_ERR("\tCustomDataLayer type %d has an out of bounds active index (%d >= %d)\n",
                    layer->type,
                    *active_index,
                    layer_num);
          if (do_fixes) {
            BLI_assert(layer_num > 0);
            *active_index = layer_num - 1;
            has_fixes = true;
          }
        }
      }
    }

    if (CustomData_layertype_is_singleton(type)) {
      if (layer_num > 1) {
        PRINT_ERR("\tCustomDataLayer type %d is a singleton, found %d in Mesh structure\n",
                  type,
                  layer_num);
        ok = false;
      }
    }

    if (mask != 0) {
      eCustomDataMask layer_typemask = CD_TYPE_AS_MASK(type);
      if ((layer_typemask & mask) == 0) {
        PRINT_ERR("\tCustomDataLayer type %d which isn't in the mask\n", type);
        ok = false;
      }
    }

    if (ok == false) {
      if (do_fixes) {
        CustomData_free_layer(data, type, 0, i);
        has_fixes = true;
      }
    }

    if (ok) {
      if (CustomData_layer_validate(layer, totitems, do_fixes)) {
        PRINT_ERR("\tCustomDataLayer type %d has some invalid data\n", type);
        has_fixes = do_fixes;
      }
      i++;
    }
  }

  PRINT_MSG("%s: Finished (is_valid=%d)\n\n", __func__, int(!has_fixes));

  *r_change = has_fixes;

  return is_valid;
}

bool BKE_mesh_validate_all_customdata(CustomData *vert_data,
                                      const uint verts_num,
                                      CustomData *edge_data,
                                      const uint edges_num,
                                      CustomData *corner_data,
                                      const uint corners_num,
                                      CustomData *face_data,
                                      const uint faces_num,
                                      const bool check_meshmask,
                                      const bool do_verbose,
                                      const bool do_fixes,
                                      bool *r_change)
{
  bool is_valid = true;
  bool is_change_v, is_change_e, is_change_l, is_change_p;
  CustomData_MeshMasks mask = {0};
  if (check_meshmask) {
    mask = CD_MASK_MESH;
  }

  is_valid &= mesh_validate_customdata(
      vert_data, mask.vmask, verts_num, do_verbose, do_fixes, &is_change_v);
  is_valid &= mesh_validate_customdata(
      edge_data, mask.emask, edges_num, do_verbose, do_fixes, &is_change_e);
  is_valid &= mesh_validate_customdata(
      corner_data, mask.lmask, corners_num, do_verbose, do_fixes, &is_change_l);
  is_valid &= mesh_validate_customdata(
      face_data, mask.pmask, faces_num, do_verbose, do_fixes, &is_change_p);

  const int uv_maps_num = CustomData_number_of_layers(corner_data, CD_PROP_FLOAT2);
  if (uv_maps_num > MAX_MTFACE) {
    PRINT_ERR(
        "\tMore UV layers than %d allowed, %d last ones won't be available for render, shaders, "
        "etc.\n",
        MAX_MTFACE,
        uv_maps_num - MAX_MTFACE);
  }

  /* check indices of clone/stencil */
  if (do_fixes && CustomData_get_clone_layer(corner_data, CD_PROP_FLOAT2) >= uv_maps_num) {
    CustomData_set_layer_clone(corner_data, CD_PROP_FLOAT2, 0);
    is_change_l = true;
  }
  if (do_fixes && CustomData_get_stencil_layer(corner_data, CD_PROP_FLOAT2) >= uv_maps_num) {
    CustomData_set_layer_stencil(corner_data, CD_PROP_FLOAT2, 0);
    is_change_l = true;
  }

  *r_change = (is_change_v || is_change_e || is_change_l || is_change_p);

  return is_valid;
}

bool BKE_mesh_validate(Mesh *mesh, const bool do_verbose, const bool cddata_check_mask)
{
  bool changed;

  if (do_verbose) {
    CLOG_INFO(&LOG, 0, "MESH: %s", mesh->id.name + 2);
  }

  BKE_mesh_validate_all_customdata(&mesh->vert_data,
                                   mesh->verts_num,
                                   &mesh->edge_data,
                                   mesh->edges_num,
                                   &mesh->corner_data,
                                   mesh->corners_num,
                                   &mesh->face_data,
                                   mesh->faces_num,
                                   cddata_check_mask,
                                   do_verbose,
                                   true,
                                   &changed);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<blender::int2> edges = mesh->edges_for_write();
  Span<int> face_offsets = mesh->face_offsets();
  Span<int> corner_verts = mesh->corner_verts();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();

  MDeformVert *dverts = static_cast<MDeformVert *>(
      CustomData_get_layer_for_write(&mesh->vert_data, CD_MDEFORMVERT, mesh->verts_num));
  BKE_mesh_validate_arrays(
      mesh,
      reinterpret_cast<float(*)[3]>(positions.data()),
      positions.size(),
      edges.data(),
      edges.size(),
      (MFace *)CustomData_get_layer_for_write(&mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy),
      mesh->totface_legacy,
      corner_verts.data(),
      corner_edges.data(),
      corner_verts.size(),
      face_offsets.data(),
      mesh->faces_num,
      dverts,
      do_verbose,
      true,
      &changed);

  if (changed) {
    BKE_mesh_runtime_clear_cache(mesh);
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY_ALL_MODES);
    return true;
  }

  return false;
}

bool BKE_mesh_is_valid(Mesh *mesh)
{
  const bool do_verbose = true;
  const bool do_fixes = false;

  bool is_valid = true;
  bool changed = true;

  is_valid &= BKE_mesh_validate_all_customdata(
      &mesh->vert_data,
      mesh->verts_num,
      &mesh->edge_data,
      mesh->edges_num,
      &mesh->corner_data,
      mesh->corners_num,
      &mesh->face_data,
      mesh->faces_num,
      false, /* setting mask here isn't useful, gives false positives */
      do_verbose,
      do_fixes,
      &changed);

  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<blender::int2> edges = mesh->edges_for_write();
  Span<int> face_offsets = mesh->face_offsets();
  Span<int> corner_verts = mesh->corner_verts();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();

  MDeformVert *dverts = static_cast<MDeformVert *>(
      CustomData_get_layer_for_write(&mesh->vert_data, CD_MDEFORMVERT, mesh->verts_num));
  is_valid &= BKE_mesh_validate_arrays(
      mesh,
      reinterpret_cast<float(*)[3]>(positions.data()),
      positions.size(),
      edges.data(),
      edges.size(),
      (MFace *)CustomData_get_layer_for_write(&mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy),
      mesh->totface_legacy,
      corner_verts.data(),
      corner_edges.data(),
      corner_verts.size(),
      face_offsets.data(),
      mesh->faces_num,
      dverts,
      do_verbose,
      do_fixes,
      &changed);

  BLI_assert(changed == false);

  return is_valid;
}

bool BKE_mesh_validate_material_indices(Mesh *mesh)
{
  const int mat_nr_max = max_ii(0, mesh->totcol - 1);
  bool is_valid = true;

  blender::bke::AttributeWriter<int> material_indices =
      mesh->attributes_for_write().lookup_for_write<int>("material_index");
  blender::MutableVArraySpan<int> material_indices_span(material_indices.varray);
  for (const int i : material_indices_span.index_range()) {
    if (material_indices_span[i] < 0 || material_indices_span[i] > mat_nr_max) {
      material_indices_span[i] = 0;
      is_valid = false;
    }
  }
  material_indices_span.save();
  material_indices.finish();

  if (!is_valid) {
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY_ALL_MODES);
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Stripping (removing invalid data)
 * \{ */

void strip_loose_faces_corners(Mesh *mesh, blender::BitSpan faces_to_remove)
{
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();

  int a, b;
  /* New corners idx! */
  int *new_idx = (int *)MEM_mallocN(sizeof(int) * mesh->corners_num, __func__);

  for (a = b = 0; a < mesh->faces_num; a++) {
    bool invalid = false;
    int start = face_offsets[a];
    int size = face_offsets[a + 1] - start;
    int stop = start + size;

    if (faces_to_remove[a]) {
      invalid = true;
    }
    else if (stop > mesh->corners_num || stop < start || size < 0) {
      invalid = true;
    }
    else {
      /* If one of the face's corners is invalid, the whole face is invalid! */
      if (corner_edges.slice(start, size).as_span().contains(INVALID_CORNER_EDGE_MARKER)) {
        invalid = true;
      }
    }

    if (size >= 3 && !invalid) {
      if (a != b) {
        face_offsets[b] = face_offsets[a];
        CustomData_copy_data(&mesh->face_data, &mesh->face_data, a, b, 1);
      }
      b++;
    }
  }
  if (a != b) {
    CustomData_free_elem(&mesh->face_data, b, a - b);
    mesh->faces_num = b;
  }

  /* And now, get rid of invalid corners. */
  int corner = 0;
  for (a = b = 0; a < mesh->corners_num; a++, corner++) {
    if (corner_edges[corner] != INVALID_CORNER_EDGE_MARKER) {
      if (a != b) {
        CustomData_copy_data(&mesh->corner_data, &mesh->corner_data, a, b, 1);
      }
      new_idx[a] = b;
      b++;
    }
    else {
      /* XXX Theoretically, we should be able to not do this, as no remaining face
       *     should use any stripped corner. But for security's sake... */
      new_idx[a] = -a;
    }
  }
  if (a != b) {
    CustomData_free_elem(&mesh->corner_data, b, a - b);
    mesh->corners_num = b;
  }

  face_offsets[mesh->faces_num] = mesh->corners_num;

  /* And now, update faces' start corner index. */
  /* NOTE: At this point, there should never be any face using a stripped corner! */
  for (const int i : blender::IndexRange(mesh->faces_num)) {
    face_offsets[i] = new_idx[face_offsets[i]];
    BLI_assert(face_offsets[i] >= 0);
  }

  MEM_freeN(new_idx);
}

void mesh_strip_edges(Mesh *mesh)
{
  blender::int2 *e;
  int a, b;
  uint *new_idx = (uint *)MEM_mallocN(sizeof(int) * mesh->edges_num, __func__);
  MutableSpan<blender::int2> edges = mesh->edges_for_write();

  for (a = b = 0, e = edges.data(); a < mesh->edges_num; a++, e++) {
    if ((*e)[0] != (*e)[1]) {
      if (a != b) {
        memcpy(&edges[b], e, sizeof(edges[b]));
        CustomData_copy_data(&mesh->edge_data, &mesh->edge_data, a, b, 1);
      }
      new_idx[a] = b;
      b++;
    }
    else {
      new_idx[a] = INVALID_CORNER_EDGE_MARKER;
    }
  }
  if (a != b) {
    CustomData_free_elem(&mesh->edge_data, b, a - b);
    mesh->edges_num = b;
  }

  /* And now, update corners' edge indices. */
  /* XXX We hope no corner was pointing to a stripped edge!
   *     Else, its e will be set to INVALID_CORNER_EDGE_MARKER :/ */
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();
  for (const int i : corner_edges.index_range()) {
    corner_edges[i] = new_idx[corner_edges[i]];
  }

  MEM_freeN(new_idx);
}

/** \} */
