/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_ghash.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_uvedit.hh"

#include "GPU_batch.hh"
#include "GPU_state.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "uvedit_intern.hh"

using blender::Vector;

/* ********************** smart stitch operator *********************** */

namespace {

/* object that stores display data for previewing before confirming stitching */
struct StitchPreviewer {
  /* here we'll store the preview triangle indices of the mesh */
  float *preview_polys;
  /* uvs per face. */
  uint *uvs_per_polygon;
  /* Number of preview polygons. */
  uint num_polys;
  /* preview data. These will be either the previewed vertices or edges
   * depending on stitch mode settings */
  float *preview_stitchable;
  float *preview_unstitchable;
  /* here we'll store the number of elements to be drawn */
  uint num_stitchable;
  uint num_unstitchable;
  uint preview_uvs;
  /* ...and here we'll store the static island triangles */
  float *static_tris;
  uint num_static_tris;
};

struct IslandStitchData;

/**
 * This is a straightforward implementation, count the UVs in the island
 * that will move and take the mean displacement/rotation and apply it to all
 * elements of the island except from the stitchable.
 */
struct IslandStitchData {
  /* rotation can be used only for edges, for vertices there is no such notion */
  float rotation;
  float rotation_neg;
  float translation[2];
  /* Used for rotation, the island will rotate around this point */
  float medianPoint[2];
  int numOfElements;
  int num_rot_elements;
  int num_rot_elements_neg;
  /* flag to remember if island has been added for preview */
  char addedForPreview;
  /* flag an island to be considered for determining static island */
  char stitchableCandidate;
  /* if edge rotation is used, flag so that vertex rotation is not used */
  bool use_edge_rotation;
};

/* just for averaging UVs */
struct UVVertAverage {
  float uv[2];
  ushort count;
};

struct UvEdge {
  /** index to uv buffer */
  uint uv1;
  uint uv2;
  /** general use flag
   * (Used to check if edge is boundary here, and propagates to adjacency elements) */
  uchar flag;
  /**
   * Element that guarantees `element.l` has the edge on
   * `element.loop_of_face_index` and `element->loop_of_face_index + 1` is the second UV.
   */
  UvElement *element;
  /** next uv edge with the same exact vertices as this one.
   * Calculated at startup to save time */
  UvEdge *next;
  /** point to first of common edges. Needed for iteration */
  UvEdge *first;
};

/* stitch state object */
struct StitchState {
  /** The `aspect[0] / aspect[1]`. */
  float aspect;
  /* object for editmesh */
  Object *obedit;
  /* editmesh, cached for use in modal handler */
  BMEditMesh *em;

  /* element map for getting info about uv connectivity */
  UvElementMap *element_map;
  /* edge container */
  UvEdge *uvedges;
  /* container of first of a group of coincident uvs, these will be operated upon */
  UvElement **uvs;
  /* maps uvelements to their first coincident uv */
  int *map;
  /* 2D normals per uv to calculate rotation for snapping */
  float *normals;
  /* edge storage */
  UvEdge *edges;
  /* hash for quick lookup of edges */
  GHash *edge_hash;
  /* which islands to stop at (to make active) when pressing 'I' */
  bool *island_is_stitchable;

  /* count of separate uvs and edges */
  int total_separate_edges;
  int total_separate_uvs;
  /* hold selection related information */
  void **selection_stack;
  int selection_size;

  /* store number of primitives per face so that we can allocate the active island buffer later */
  uint *tris_per_island;
  /* preview data */
  StitchPreviewer *stitch_preview;
};

/* Stitch state container. */
struct StitchStateContainer {
  /* clear seams of stitched edges after stitch */
  bool clear_seams;
  /* use limit flag */
  bool use_limit;
  /* limit to operator, same as original operator */
  float limit_dist;
  /* snap uv islands together during stitching */
  bool snap_islands;
  /* stitch at midpoints or at islands */
  bool midpoints;
  /* vert or edge mode used for stitching */
  char mode;
  /* handle for drawing */
  void *draw_handle;
  /* island that stays in place */
  int static_island;

  /* Objects and states are aligned. */
  int objects_len;
  Object **objects;
  StitchState **states;

  int active_object_index;
};

struct PreviewPosition {
  int data_position;
  int polycount_position;
};
/*
 * defines for UvElement/UcEdge flags
 */
enum {
  STITCH_SELECTED = 1,
  STITCH_STITCHABLE = 2,
  STITCH_PROCESSED = 4,
  STITCH_BOUNDARY = 8,
  STITCH_STITCHABLE_CANDIDATE = 16,
};

#define STITCH_NO_PREVIEW -1

enum StitchModes {
  STITCH_VERT,
  STITCH_EDGE,
};

/** #UvElement identification. */
struct UvElementID {
  int faceIndex;
  int elementIndex;
};

/** #StitchState initialization. */
struct StitchStateInit {
  int uv_selected_count;
  UvElementID *to_select;
};

}  // namespace

/* constructor */
static StitchPreviewer *stitch_preview_init()
{
  StitchPreviewer *stitch_preview;

  stitch_preview = MEM_mallocN<StitchPreviewer>("stitch_previewer");
  stitch_preview->preview_polys = nullptr;
  stitch_preview->preview_stitchable = nullptr;
  stitch_preview->preview_unstitchable = nullptr;
  stitch_preview->uvs_per_polygon = nullptr;

  stitch_preview->preview_uvs = 0;
  stitch_preview->num_polys = 0;
  stitch_preview->num_stitchable = 0;
  stitch_preview->num_unstitchable = 0;

  stitch_preview->static_tris = nullptr;

  stitch_preview->num_static_tris = 0;

  return stitch_preview;
}

/* destructor...yeah this should be C++ :) */
static void stitch_preview_delete(StitchPreviewer *stitch_preview)
{
  if (stitch_preview) {
    MEM_SAFE_FREE(stitch_preview->preview_polys);
    MEM_SAFE_FREE(stitch_preview->uvs_per_polygon);
    MEM_SAFE_FREE(stitch_preview->preview_stitchable);
    MEM_SAFE_FREE(stitch_preview->preview_unstitchable);
    MEM_SAFE_FREE(stitch_preview->static_tris);
    MEM_freeN(stitch_preview);
  }
}

/* This function updates the header of the UV editor when the stitch tool updates its settings */
static void stitch_update_header(StitchStateContainer *ssc, bContext *C)
{
  WorkspaceStatus status(C);
  status.item(IFACE_("Confirm"), ICON_MOUSE_LMB);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
  status.item(fmt::format("{} {}",
                          IFACE_("Select"),
                          (ssc->mode == STITCH_VERT ? IFACE_("Vertices") : IFACE_("Edges"))),
              ICON_EVENT_SHIFT,
              ICON_MOUSE_RMB);
  status.item(fmt::format("{} : {}",
                          IFACE_("Mode"),
                          (ssc->mode == STITCH_VERT ? IFACE_("Vertex") : IFACE_("Edge"))),
              ICON_EVENT_TAB);
  status.item(IFACE_("Switch Island"), ICON_EVENT_I);
  status.item_bool(IFACE_("Snap"), ssc->snap_islands, ICON_EVENT_S);
  status.item_bool(IFACE_("Midpoints"), ssc->midpoints, ICON_EVENT_M);
  status.item_bool(IFACE_("Limit"), ssc->use_limit, ICON_EVENT_L);
  if (ssc->use_limit) {
    status.item(fmt::format("{} ({:.2f})", IFACE_("Limit Distance"), ssc->limit_dist),
                ICON_EVENT_ALT,
                ICON_MOUSE_MMB_SCROLL);
  }
}

static void stitch_uv_rotate(const float mat[2][2],
                             const float medianPoint[2],
                             float uv[2],
                             float aspect)
{
  float uv_rotation_result[2];

  uv[1] /= aspect;

  sub_v2_v2(uv, medianPoint);
  mul_v2_m2v2(uv_rotation_result, mat, uv);
  add_v2_v2v2(uv, uv_rotation_result, medianPoint);

  uv[1] *= aspect;
}

/* check if two uvelements are stitchable.
 * This should only operate on -different- separate UvElements */
static bool stitch_check_uvs_stitchable(const int cd_loop_uv_offset,
                                        UvElement *element,
                                        UvElement *element_iter,
                                        StitchStateContainer *ssc)
{
  float limit;

  if (element_iter == element) {
    return false;
  }

  limit = ssc->limit_dist;

  if (ssc->use_limit) {
    BMLoop *l;

    l = element->l;
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    l = element_iter->l;
    float *luv_iter = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

    if (fabsf(luv[0] - luv_iter[0]) < limit && fabsf(luv[1] - luv_iter[1]) < limit) {
      return true;
    }
    return false;
  }
  return true;
}

static bool stitch_check_edges_stitchable(const int cd_loop_uv_offset,
                                          UvEdge *edge,
                                          UvEdge *edge_iter,
                                          StitchStateContainer *ssc,
                                          StitchState *state)
{
  float limit;

  if (edge_iter == edge) {
    return false;
  }

  limit = ssc->limit_dist;

  if (ssc->use_limit) {
    float *luv_orig1 = BM_ELEM_CD_GET_FLOAT_P(state->uvs[edge->uv1]->l, cd_loop_uv_offset);
    float *luv_iter1 = BM_ELEM_CD_GET_FLOAT_P(state->uvs[edge_iter->uv1]->l, cd_loop_uv_offset);

    float *luv_orig2 = BM_ELEM_CD_GET_FLOAT_P(state->uvs[edge->uv2]->l, cd_loop_uv_offset);
    float *luv_iter2 = BM_ELEM_CD_GET_FLOAT_P(state->uvs[edge_iter->uv2]->l, cd_loop_uv_offset);

    if (fabsf(luv_orig1[0] - luv_iter1[0]) < limit && fabsf(luv_orig1[1] - luv_iter1[1]) < limit &&
        fabsf(luv_orig2[0] - luv_iter2[0]) < limit && fabsf(luv_orig2[1] - luv_iter2[1]) < limit)
    {
      return true;
    }
    return false;
  }
  return true;
}

static bool stitch_check_uvs_state_stitchable(const int cd_loop_uv_offset,
                                              UvElement *element,
                                              UvElement *element_iter,
                                              StitchStateContainer *ssc)
{
  if ((ssc->snap_islands && element->island == element_iter->island) ||
      (!ssc->midpoints && element->island == element_iter->island))
  {
    return false;
  }

  return stitch_check_uvs_stitchable(cd_loop_uv_offset, element, element_iter, ssc);
}

static bool stitch_check_edges_state_stitchable(const int cd_loop_uv_offset,
                                                UvEdge *edge,
                                                UvEdge *edge_iter,
                                                StitchStateContainer *ssc,
                                                StitchState *state)
{
  if ((ssc->snap_islands && edge->element->island == edge_iter->element->island) ||
      (!ssc->midpoints && edge->element->island == edge_iter->element->island))
  {
    return false;
  }

  return stitch_check_edges_stitchable(cd_loop_uv_offset, edge, edge_iter, ssc, state);
}

/* calculate snapping for islands */
static void stitch_calculate_island_snapping(const int cd_loop_uv_offset,
                                             StitchState *state,
                                             PreviewPosition *preview_position,
                                             StitchPreviewer *preview,
                                             IslandStitchData *island_stitch_data,
                                             int final)
{
  UvElement *element;

  for (int i = 0; i < state->element_map->total_islands; i++) {
    if (island_stitch_data[i].addedForPreview) {
      int numOfIslandUVs = 0, j;
      int totelem = island_stitch_data[i].num_rot_elements_neg +
                    island_stitch_data[i].num_rot_elements;
      float rotation;
      float rotation_mat[2][2];

      /* check to avoid divide by 0 */
      if (island_stitch_data[i].num_rot_elements > 1) {
        island_stitch_data[i].rotation /= island_stitch_data[i].num_rot_elements;
      }

      if (island_stitch_data[i].num_rot_elements_neg > 1) {
        island_stitch_data[i].rotation_neg /= island_stitch_data[i].num_rot_elements_neg;
      }

      if (island_stitch_data[i].numOfElements > 1) {
        island_stitch_data[i].medianPoint[0] /= island_stitch_data[i].numOfElements;
        island_stitch_data[i].medianPoint[1] /= island_stitch_data[i].numOfElements;

        island_stitch_data[i].translation[0] /= island_stitch_data[i].numOfElements;
        island_stitch_data[i].translation[1] /= island_stitch_data[i].numOfElements;
      }

      island_stitch_data[i].medianPoint[1] /= state->aspect;
      if ((island_stitch_data[i].rotation + island_stitch_data[i].rotation_neg < float(M_PI_2)) ||
          island_stitch_data[i].num_rot_elements == 0 ||
          island_stitch_data[i].num_rot_elements_neg == 0)
      {
        rotation = (island_stitch_data[i].rotation * island_stitch_data[i].num_rot_elements -
                    island_stitch_data[i].rotation_neg *
                        island_stitch_data[i].num_rot_elements_neg) /
                   totelem;
      }
      else {
        rotation = (island_stitch_data[i].rotation * island_stitch_data[i].num_rot_elements +
                    (2.0f * float(M_PI) - island_stitch_data[i].rotation_neg) *
                        island_stitch_data[i].num_rot_elements_neg) /
                   totelem;
      }

      angle_to_mat2(rotation_mat, rotation);
      numOfIslandUVs = state->element_map->island_total_uvs[i];
      element = &state->element_map->storage[state->element_map->island_indices[i]];
      for (j = 0; j < numOfIslandUVs; j++, element++) {
        /* stitchable uvs have already been processed, don't process */
        if (!(element->flag & STITCH_PROCESSED)) {
          BMLoop *l;

          l = element->l;
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

          if (final) {

            stitch_uv_rotate(rotation_mat, island_stitch_data[i].medianPoint, luv, state->aspect);

            add_v2_v2(luv, island_stitch_data[i].translation);
          }

          else {

            int face_preview_pos =
                preview_position[BM_elem_index_get(element->l->f)].data_position;

            stitch_uv_rotate(rotation_mat,
                             island_stitch_data[i].medianPoint,
                             preview->preview_polys + face_preview_pos +
                                 2 * element->loop_of_face_index,
                             state->aspect);

            add_v2_v2(preview->preview_polys + face_preview_pos + 2 * element->loop_of_face_index,
                      island_stitch_data[i].translation);
          }
        }
        /* cleanup */
        element->flag &= STITCH_SELECTED;
      }
    }
  }
}

static void stitch_island_calculate_edge_rotation(const int cd_loop_uv_offset,
                                                  UvEdge *edge,
                                                  StitchStateContainer *ssc,
                                                  StitchState *state,
                                                  UVVertAverage *uv_average,
                                                  const uint *uvfinal_map,
                                                  IslandStitchData *island_stitch_data)
{
  UvElement *element1, *element2;
  float uv1[2], uv2[2];
  float edgecos, edgesin;
  int index1, index2;
  float rotation;

  element1 = state->uvs[edge->uv1];
  element2 = state->uvs[edge->uv2];

  float *luv1 = BM_ELEM_CD_GET_FLOAT_P(element1->l, cd_loop_uv_offset);
  float *luv2 = BM_ELEM_CD_GET_FLOAT_P(element2->l, cd_loop_uv_offset);

  if (ssc->mode == STITCH_VERT) {
    index1 = uvfinal_map[element1 - state->element_map->storage];
    index2 = uvfinal_map[element2 - state->element_map->storage];
  }
  else {
    index1 = edge->uv1;
    index2 = edge->uv2;
  }
  /* the idea here is to take the directions of the edges and find the rotation between
   * final and initial direction. This, using inner and outer vector products,
   * gives the angle. Directions are differences so... */
  uv1[0] = luv2[0] - luv1[0];
  uv1[1] = luv2[1] - luv1[1];

  uv1[1] /= state->aspect;

  uv2[0] = uv_average[index2].uv[0] - uv_average[index1].uv[0];
  uv2[1] = uv_average[index2].uv[1] - uv_average[index1].uv[1];

  uv2[1] /= state->aspect;

  normalize_v2(uv1);
  normalize_v2(uv2);

  edgecos = dot_v2v2(uv1, uv2);
  edgesin = cross_v2v2(uv1, uv2);
  rotation = acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));

  if (edgesin > 0.0f) {
    island_stitch_data[element1->island].num_rot_elements++;
    island_stitch_data[element1->island].rotation += rotation;
  }
  else {
    island_stitch_data[element1->island].num_rot_elements_neg++;
    island_stitch_data[element1->island].rotation_neg += rotation;
  }
}

static void stitch_island_calculate_vert_rotation(const int cd_loop_uv_offset,
                                                  UvElement *element,
                                                  StitchStateContainer *ssc,
                                                  StitchState *state,
                                                  IslandStitchData *island_stitch_data)
{
  float rotation = 0, rotation_neg = 0;
  int rot_elem = 0, rot_elem_neg = 0;

  if (element->island == ssc->static_island && !ssc->midpoints) {
    return;
  }

  UvElement *element_iter = BM_uv_element_get_head(state->element_map, element);
  for (; element_iter; element_iter = element_iter->next) {
    if (element_iter->separate &&
        stitch_check_uvs_state_stitchable(cd_loop_uv_offset, element, element_iter, ssc))
    {
      float normal[2];

      /* only calculate rotation against static island uv verts */
      if (!ssc->midpoints && element_iter->island != ssc->static_island) {
        continue;
      }

      int index_tmp1 = element_iter - state->element_map->storage;
      index_tmp1 = state->map[index_tmp1];
      int index_tmp2 = element - state->element_map->storage;
      index_tmp2 = state->map[index_tmp2];

      negate_v2_v2(normal, state->normals + index_tmp2 * 2);
      float edgecos = dot_v2v2(normal, state->normals + index_tmp1 * 2);
      float edgesin = cross_v2v2(normal, state->normals + index_tmp1 * 2);
      if (edgesin > 0.0f) {
        rotation += acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));
        rot_elem++;
      }
      else {
        rotation_neg += acosf(max_ff(-1.0f, min_ff(1.0f, edgecos)));
        rot_elem_neg++;
      }
    }
  }

  if (ssc->midpoints) {
    rotation /= 2.0f;
    rotation_neg /= 2.0f;
  }
  island_stitch_data[element->island].num_rot_elements += rot_elem;
  island_stitch_data[element->island].rotation += rotation;
  island_stitch_data[element->island].num_rot_elements_neg += rot_elem_neg;
  island_stitch_data[element->island].rotation_neg += rotation_neg;
}

static void state_delete(StitchState *state)
{
  if (state) {
    if (state->island_is_stitchable) {
      MEM_freeN(state->island_is_stitchable);
    }
    if (state->element_map) {
      BM_uv_element_map_free(state->element_map);
    }
    if (state->uvs) {
      MEM_freeN(state->uvs);
    }
    if (state->selection_stack) {
      MEM_freeN(state->selection_stack);
    }
    if (state->tris_per_island) {
      MEM_freeN(state->tris_per_island);
    }
    if (state->map) {
      MEM_freeN(state->map);
    }
    if (state->normals) {
      MEM_freeN(state->normals);
    }
    if (state->edges) {
      MEM_freeN(state->edges);
    }
    stitch_preview_delete(state->stitch_preview);
    state->stitch_preview = nullptr;
    if (state->edge_hash) {
      BLI_ghash_free(state->edge_hash, nullptr, nullptr);
    }
    MEM_freeN(state);
  }
}

static void state_delete_all(StitchStateContainer *ssc)
{
  if (ssc) {
    for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
      state_delete(ssc->states[ob_index]);
    }
    MEM_freeN(ssc->states);
    MEM_freeN(ssc->objects);
    MEM_freeN(ssc);
  }
}

static void stitch_uv_edge_generate_linked_edges(GHash *edge_hash, StitchState *state)
{
  UvEdge *edges = state->edges;
  const int *map = state->map;
  UvElementMap *element_map = state->element_map;
  for (int i = 0; i < state->total_separate_edges; i++) {
    UvEdge *edge = edges + i;

    if (edge->first) {
      continue;
    }

    /* only boundary edges can be stitched. Yes. Sorry about that :p */
    if (edge->flag & STITCH_BOUNDARY) {
      UvElement *element1 = state->uvs[edge->uv1];
      UvElement *element2 = state->uvs[edge->uv2];

      /* Now iterate through all faces and try to find edges sharing the same vertices */
      UvElement *iter1 = BM_uv_element_get_head(state->element_map, element1);
      UvEdge *last_set = edge;
      int elemindex2 = BM_elem_index_get(element2->l->v);

      edge->first = edge;

      for (; iter1; iter1 = iter1->next) {
        UvElement *iter2 = nullptr;

        /* check to see if other vertex of edge belongs to same vertex as */
        if (BM_elem_index_get(iter1->l->next->v) == elemindex2) {
          iter2 = BM_uv_element_get(element_map, iter1->l->next);
        }
        else if (BM_elem_index_get(iter1->l->prev->v) == elemindex2) {
          iter2 = BM_uv_element_get(element_map, iter1->l->prev);
        }

        if (iter2) {
          int index1 = map[iter1 - element_map->storage];
          int index2 = map[iter2 - element_map->storage];
          UvEdge edgetmp;
          UvEdge *edge2, *eiter;
          bool valid = true;

          /* make sure the indices are well behaved */
          if (index1 > index2) {
            std::swap(index1, index2);
          }

          edgetmp.uv1 = index1;
          edgetmp.uv2 = index2;

          /* get the edge from the hash */
          edge2 = static_cast<UvEdge *>(BLI_ghash_lookup(edge_hash, &edgetmp));

          /* more iteration to make sure non-manifold case is handled nicely */
          for (eiter = edge; eiter; eiter = eiter->next) {
            if (edge2 == eiter) {
              valid = false;
              break;
            }
          }

          if (valid) {
            /* here I am taking care of non manifold case, assuming more than two matching edges.
             * I am not too sure we want this though */
            last_set->next = edge2;
            last_set = edge2;
            /* set first, similarly to uv elements.
             * Now we can iterate among common edges easily */
            edge2->first = edge;
          }
        }
      }
    }
    else {
      /* so stitchability code works */
      edge->first = edge;
    }
  }
}

/* checks for remote uvs that may be stitched with a certain uv, flags them if stitchable. */
static void determine_uv_stitchability(const int cd_loop_uv_offset,
                                       UvElement *element,
                                       StitchStateContainer *ssc,
                                       StitchState *state,
                                       IslandStitchData *island_stitch_data)
{
  UvElement *element_iter = BM_uv_element_get_head(state->element_map, element);
  for (; element_iter; element_iter = element_iter->next) {
    if (element_iter->separate) {
      if (stitch_check_uvs_stitchable(cd_loop_uv_offset, element, element_iter, ssc)) {
        island_stitch_data[element_iter->island].stitchableCandidate = 1;
        island_stitch_data[element->island].stitchableCandidate = 1;
        element->flag |= STITCH_STITCHABLE_CANDIDATE;
      }
    }
  }
}

static void determine_uv_edge_stitchability(const int cd_loop_uv_offset,
                                            UvEdge *edge,
                                            StitchStateContainer *ssc,
                                            StitchState *state,
                                            IslandStitchData *island_stitch_data)
{
  UvEdge *edge_iter = edge->first;

  for (; edge_iter; edge_iter = edge_iter->next) {
    if (stitch_check_edges_stitchable(cd_loop_uv_offset, edge, edge_iter, ssc, state)) {
      island_stitch_data[edge_iter->element->island].stitchableCandidate = 1;
      island_stitch_data[edge->element->island].stitchableCandidate = 1;
      edge->flag |= STITCH_STITCHABLE_CANDIDATE;
    }
  }
}

/* set preview buffer position of UV face in editface->tmp.l */
static void stitch_set_face_preview_buffer_position(BMFace *efa,
                                                    StitchPreviewer *preview,
                                                    PreviewPosition *preview_position)
{
  int index = BM_elem_index_get(efa);

  if (preview_position[index].data_position == STITCH_NO_PREVIEW) {
    preview_position[index].data_position = preview->preview_uvs * 2;
    preview_position[index].polycount_position = preview->num_polys++;
    preview->preview_uvs += efa->len;
  }
}

/* setup face preview for all coincident uvs and their faces */
static void stitch_setup_face_preview_for_uv_group(UvElement *element,
                                                   StitchStateContainer *ssc,
                                                   StitchState *state,
                                                   IslandStitchData *island_stitch_data,
                                                   PreviewPosition *preview_position)
{
  StitchPreviewer *preview = state->stitch_preview;

  /* static island does not change so returning immediately */
  if (ssc->snap_islands && !ssc->midpoints && ssc->static_island == element->island) {
    return;
  }

  if (ssc->snap_islands) {
    island_stitch_data[element->island].addedForPreview = 1;
  }

  do {
    stitch_set_face_preview_buffer_position(element->l->f, preview, preview_position);
    element = element->next;
  } while (element && !element->separate);
}

/* checks if uvs are indeed stitchable and registers so that they can be shown in preview */
static void stitch_validate_uv_stitchability(const int cd_loop_uv_offset,
                                             UvElement *element,
                                             StitchStateContainer *ssc,
                                             StitchState *state,
                                             IslandStitchData *island_stitch_data,
                                             PreviewPosition *preview_position)
{
  StitchPreviewer *preview = state->stitch_preview;

  /* If not the active object, then it's unstitchable */
  if (ssc->states[ssc->active_object_index] != state) {
    preview->num_unstitchable++;
    return;
  }

  UvElement *element_iter = BM_uv_element_get_head(state->element_map, element);
  for (; element_iter; element_iter = element_iter->next) {
    if (element_iter->separate) {
      if (element_iter == element) {
        continue;
      }
      if (stitch_check_uvs_state_stitchable(cd_loop_uv_offset, element, element_iter, ssc)) {
        if ((element_iter->island == ssc->static_island) ||
            (element->island == ssc->static_island))
        {
          element->flag |= STITCH_STITCHABLE;
          preview->num_stitchable++;
          stitch_setup_face_preview_for_uv_group(
              element, ssc, state, island_stitch_data, preview_position);
          return;
        }
      }
    }
  }

  /* this can happen if the uvs to be stitched are not on a stitchable island */
  if (!(element->flag & STITCH_STITCHABLE)) {
    preview->num_unstitchable++;
  }
}

static void stitch_validate_edge_stitchability(const int cd_loop_uv_offset,
                                               UvEdge *edge,
                                               StitchStateContainer *ssc,
                                               StitchState *state,
                                               IslandStitchData *island_stitch_data,
                                               PreviewPosition *preview_position)
{
  StitchPreviewer *preview = state->stitch_preview;

  /* If not the active object, then it's unstitchable */
  if (ssc->states[ssc->active_object_index] != state) {
    preview->num_unstitchable++;
    return;
  }

  UvEdge *edge_iter = edge->first;

  for (; edge_iter; edge_iter = edge_iter->next) {
    if (edge_iter == edge) {
      continue;
    }
    if (stitch_check_edges_state_stitchable(cd_loop_uv_offset, edge, edge_iter, ssc, state)) {
      if ((edge_iter->element->island == ssc->static_island) ||
          (edge->element->island == ssc->static_island))
      {
        edge->flag |= STITCH_STITCHABLE;
        preview->num_stitchable++;
        stitch_setup_face_preview_for_uv_group(
            state->uvs[edge->uv1], ssc, state, island_stitch_data, preview_position);
        stitch_setup_face_preview_for_uv_group(
            state->uvs[edge->uv2], ssc, state, island_stitch_data, preview_position);
        return;
      }
    }
  }

  /* this can happen if the uvs to be stitched are not on a stitchable island */
  if (!(edge->flag & STITCH_STITCHABLE)) {
    preview->num_unstitchable++;
  }
}

static void stitch_propagate_uv_final_position(Scene *scene,
                                               UvElement *element,
                                               int index,
                                               PreviewPosition *preview_position,
                                               UVVertAverage *final_position,
                                               StitchStateContainer *ssc,
                                               StitchState *state,
                                               const bool final)
{
  BMesh *bm = state->em->bm;
  StitchPreviewer *preview = state->stitch_preview;

  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  if (element->flag & STITCH_STITCHABLE) {
    UvElement *element_iter = element;
    /* propagate to coincident uvs */
    do {
      BMLoop *l;

      l = element_iter->l;
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);

      element_iter->flag |= STITCH_PROCESSED;
      /* either flush to preview or to the MTFace, if final */
      if (final) {
        copy_v2_v2(luv, final_position[index].uv);

        uvedit_uv_select_enable(scene, state->em->bm, l);
      }
      else {
        int face_preview_pos =
            preview_position[BM_elem_index_get(element_iter->l->f)].data_position;
        if (face_preview_pos != STITCH_NO_PREVIEW) {
          copy_v2_v2(preview->preview_polys + face_preview_pos +
                         2 * element_iter->loop_of_face_index,
                     final_position[index].uv);
        }
      }

      /* end of calculations, keep only the selection flag */
      if ((!ssc->snap_islands) ||
          ((!ssc->midpoints) && (element_iter->island == ssc->static_island)))
      {
        element_iter->flag &= STITCH_SELECTED;
      }

      element_iter = element_iter->next;
    } while (element_iter && !element_iter->separate);
  }
}

/* main processing function. It calculates preview and final positions. */
static int stitch_process_data(StitchStateContainer *ssc,
                               StitchState *state,
                               Scene *scene,
                               int final)
{
  int i;
  StitchPreviewer *preview;
  IslandStitchData *island_stitch_data = nullptr;
  int previous_island = ssc->static_island;
  BMesh *bm = state->em->bm;
  BMFace *efa;
  BMIter iter;
  UVVertAverage *final_position = nullptr;
  bool is_active_state = (state == ssc->states[ssc->active_object_index]);

  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);

  char stitch_midpoints = ssc->midpoints;
  /* Used to map UV indices to UV-average indices for selection. */
  uint *uvfinal_map = nullptr;
  /* per face preview position in preview buffer */
  PreviewPosition *preview_position = nullptr;

  /* cleanup previous preview */
  stitch_preview_delete(state->stitch_preview);
  preview = state->stitch_preview = stitch_preview_init();
  if (preview == nullptr) {
    return 0;
  }

  preview_position = static_cast<PreviewPosition *>(
      MEM_mallocN(bm->totface * sizeof(*preview_position), "stitch_face_preview_position"));
  /* each face holds its position in the preview buffer in tmp. -1 is uninitialized */
  for (i = 0; i < bm->totface; i++) {
    preview_position[i].data_position = STITCH_NO_PREVIEW;
  }

  island_stitch_data = MEM_calloc_arrayN<IslandStitchData>(state->element_map->total_islands,
                                                           "stitch_island_data");
  if (!island_stitch_data) {
    return 0;
  }

  /* store indices to editVerts and Faces. May be unneeded but ensuring anyway */
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

  /****************************************
   * First determine stitchability of uvs *
   ****************************************/

  for (i = 0; i < state->selection_size; i++) {
    if (ssc->mode == STITCH_VERT) {
      UvElement *element = (UvElement *)state->selection_stack[i];
      determine_uv_stitchability(cd_loop_uv_offset, element, ssc, state, island_stitch_data);
    }
    else {
      UvEdge *edge = (UvEdge *)state->selection_stack[i];
      determine_uv_edge_stitchability(cd_loop_uv_offset, edge, ssc, state, island_stitch_data);
    }
  }

  /* Remember stitchable candidates as places the 'I' button will stop at. */
  for (int island_idx = 0; island_idx < state->element_map->total_islands; island_idx++) {
    state->island_is_stitchable[island_idx] = island_stitch_data[island_idx].stitchableCandidate ?
                                                  true :
                                                  false;
  }

  if (is_active_state) {
    /* set static island to one that is added for preview */
    ssc->static_island %= state->element_map->total_islands;
    while (!(island_stitch_data[ssc->static_island].stitchableCandidate)) {
      ssc->static_island++;
      ssc->static_island %= state->element_map->total_islands;
      /* this is entirely possible if for example limit stitching
       * with no stitchable verts or no selection */
      if (ssc->static_island == previous_island) {
        break;
      }
    }
  }

  for (i = 0; i < state->selection_size; i++) {
    if (ssc->mode == STITCH_VERT) {
      UvElement *element = (UvElement *)state->selection_stack[i];
      if (element->flag & STITCH_STITCHABLE_CANDIDATE) {
        element->flag &= ~STITCH_STITCHABLE_CANDIDATE;
        stitch_validate_uv_stitchability(
            cd_loop_uv_offset, element, ssc, state, island_stitch_data, preview_position);
      }
      else {
        /* add to preview for unstitchable */
        preview->num_unstitchable++;
      }
    }
    else {
      UvEdge *edge = (UvEdge *)state->selection_stack[i];
      if (edge->flag & STITCH_STITCHABLE_CANDIDATE) {
        edge->flag &= ~STITCH_STITCHABLE_CANDIDATE;
        stitch_validate_edge_stitchability(
            cd_loop_uv_offset, edge, ssc, state, island_stitch_data, preview_position);
      }
      else {
        preview->num_unstitchable++;
      }
    }
  }

  /*********************************************************************
   * Setup the stitchable & unstitchable preview buffers and fill      *
   * them with the appropriate data                                    *
   *********************************************************************/
  if (!final) {
    float *luv;
    int stitchBufferIndex = 0, unstitchBufferIndex = 0;
    int preview_size = (ssc->mode == STITCH_VERT) ? 2 : 4;
    /* initialize the preview buffers */
    preview->preview_stitchable = (float *)MEM_mallocN(
        preview->num_stitchable * sizeof(float) * preview_size, "stitch_preview_stitchable_data");
    preview->preview_unstitchable = (float *)MEM_mallocN(preview->num_unstitchable *
                                                             sizeof(float) * preview_size,
                                                         "stitch_preview_unstitchable_data");

    /* will cause cancel and freeing of all data structures so OK */
    if (!preview->preview_stitchable || !preview->preview_unstitchable) {
      return 0;
    }

    /* fill the appropriate preview buffers */
    if (ssc->mode == STITCH_VERT) {
      for (i = 0; i < state->total_separate_uvs; i++) {
        UvElement *element = state->uvs[i];
        if (element->flag & STITCH_STITCHABLE) {
          luv = BM_ELEM_CD_GET_FLOAT_P(element->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 2], luv);
          stitchBufferIndex++;
        }
        else if (element->flag & STITCH_SELECTED) {
          luv = BM_ELEM_CD_GET_FLOAT_P(element->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 2], luv);
          unstitchBufferIndex++;
        }
      }
    }
    else {
      for (i = 0; i < state->total_separate_edges; i++) {
        UvEdge *edge = state->edges + i;
        UvElement *element1 = state->uvs[edge->uv1];
        UvElement *element2 = state->uvs[edge->uv2];

        if (edge->flag & STITCH_STITCHABLE) {
          luv = BM_ELEM_CD_GET_FLOAT_P(element1->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 4], luv);

          luv = BM_ELEM_CD_GET_FLOAT_P(element2->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_stitchable[stitchBufferIndex * 4 + 2], luv);

          stitchBufferIndex++;
          BLI_assert(stitchBufferIndex <= preview->num_stitchable);
        }
        else if (edge->flag & STITCH_SELECTED) {
          luv = BM_ELEM_CD_GET_FLOAT_P(element1->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 4], luv);

          luv = BM_ELEM_CD_GET_FLOAT_P(element2->l, cd_loop_uv_offset);
          copy_v2_v2(&preview->preview_unstitchable[unstitchBufferIndex * 4 + 2], luv);

          unstitchBufferIndex++;
          BLI_assert(unstitchBufferIndex <= preview->num_unstitchable);
        }
      }
    }
  }

  if (ssc->states[ssc->active_object_index] != state) {
    /* This is not the active object/state, exit here */
    MEM_freeN(island_stitch_data);
    MEM_freeN(preview_position);
    return 1;
  }

  /****************************************
   * Setup preview for stitchable islands *
   ****************************************/
  if (ssc->snap_islands) {
    for (i = 0; i < state->element_map->total_islands; i++) {
      if (island_stitch_data[i].addedForPreview) {
        int numOfIslandUVs = state->element_map->island_total_uvs[i];
        UvElement *element = &state->element_map->storage[state->element_map->island_indices[i]];
        for (int j = 0; j < numOfIslandUVs; j++, element++) {
          stitch_set_face_preview_buffer_position(element->l->f, preview, preview_position);
        }
      }
    }
  }

  /*********************************************************************
   * Setup the remaining preview buffers and fill them with the        *
   * appropriate data                                                  *
   *********************************************************************/
  if (!final) {
    BMIter liter;
    BMLoop *l;
    float *luv;
    uint buffer_index = 0;

    /* initialize the preview buffers */
    preview->preview_polys = static_cast<float *>(
        MEM_mallocN(sizeof(float[2]) * preview->preview_uvs, "tri_uv_stitch_prev"));
    preview->uvs_per_polygon = MEM_malloc_arrayN<uint>(preview->num_polys, "tri_uv_stitch_prev");

    preview->static_tris = static_cast<float *>(
        MEM_mallocN((sizeof(float[6]) * state->tris_per_island[ssc->static_island]),
                    "static_island_preview_tris"));

    preview->num_static_tris = state->tris_per_island[ssc->static_island];
    /* will cause cancel and freeing of all data structures so OK */
    if (!preview->preview_polys) {
      return 0;
    }

    /* copy data from UVs to the preview display buffers */
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      /* just to test if face was added for processing.
       * uvs of unselected vertices will return null */
      UvElement *element = BM_uv_element_get(state->element_map, BM_FACE_FIRST_LOOP(efa));

      if (element) {
        int numoftris = efa->len - 2;
        int index = BM_elem_index_get(efa);
        int face_preview_pos = preview_position[index].data_position;
        if (face_preview_pos != STITCH_NO_PREVIEW) {
          preview->uvs_per_polygon[preview_position[index].polycount_position] = efa->len;
          BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
            luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
            copy_v2_v2(preview->preview_polys + face_preview_pos + i * 2, luv);
          }
        }

        /* if this is the static_island on the active object */
        if (element->island == ssc->static_island) {
          BMLoop *fl = BM_FACE_FIRST_LOOP(efa);
          float *fuv = BM_ELEM_CD_GET_FLOAT_P(fl, cd_loop_uv_offset);

          BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
            if (i < numoftris) {
              /* using next since the first uv is already accounted for */
              BMLoop *lnext = l->next;
              float *luvnext = BM_ELEM_CD_GET_FLOAT_P(lnext->next, cd_loop_uv_offset);
              luv = BM_ELEM_CD_GET_FLOAT_P(lnext, cd_loop_uv_offset);

              memcpy(preview->static_tris + buffer_index, fuv, sizeof(float[2]));
              memcpy(preview->static_tris + buffer_index + 2, luv, sizeof(float[2]));
              memcpy(preview->static_tris + buffer_index + 4, luvnext, sizeof(float[2]));
              buffer_index += 6;
            }
            else {
              break;
            }
          }
        }
      }
    }
  }

  /******************************************************
   * Here we calculate the final coordinates of the uvs *
   ******************************************************/

  if (ssc->mode == STITCH_VERT) {
    final_position = static_cast<UVVertAverage *>(
        MEM_callocN(state->selection_size * sizeof(*final_position), "stitch_uv_average"));
    uvfinal_map = static_cast<uint *>(
        MEM_mallocN(state->element_map->total_uvs * sizeof(*uvfinal_map), "stitch_uv_final_map"));
  }
  else {
    final_position = static_cast<UVVertAverage *>(
        MEM_callocN(state->total_separate_uvs * sizeof(*final_position), "stitch_uv_average"));
  }

  /* first pass, calculate final position for stitchable uvs of the static island */
  for (i = 0; i < state->selection_size; i++) {
    if (ssc->mode == STITCH_VERT) {
      UvElement *element = static_cast<UvElement *>(state->selection_stack[i]);

      if (element->flag & STITCH_STITCHABLE) {
        BMLoop *l = element->l;
        float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

        uvfinal_map[element - state->element_map->storage] = i;

        copy_v2_v2(final_position[i].uv, luv);
        final_position[i].count = 1;

        if (ssc->snap_islands && element->island == ssc->static_island && !stitch_midpoints) {
          continue;
        }

        UvElement *element_iter = state->element_map->vertex[BM_elem_index_get(l->v)];
        for (; element_iter; element_iter = element_iter->next) {
          if (element_iter->separate) {
            if (stitch_check_uvs_state_stitchable(cd_loop_uv_offset, element, element_iter, ssc)) {
              l = element_iter->l;
              luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
              if (stitch_midpoints) {
                add_v2_v2(final_position[i].uv, luv);
                final_position[i].count++;
              }
              else if (element_iter->island == ssc->static_island) {
                /* if multiple uvs on the static island exist,
                 * last checked remains. to disambiguate we need to limit or use
                 * edge stitch */
                copy_v2_v2(final_position[i].uv, luv);
              }
            }
          }
        }
      }
      if (stitch_midpoints) {
        final_position[i].uv[0] /= final_position[i].count;
        final_position[i].uv[1] /= final_position[i].count;
      }
    }
    else {
      UvEdge *edge = static_cast<UvEdge *>(state->selection_stack[i]);

      if (edge->flag & STITCH_STITCHABLE) {
        float *luv2, *luv1;
        BMLoop *l;
        UvEdge *edge_iter;

        l = state->uvs[edge->uv1]->l;
        luv1 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
        l = state->uvs[edge->uv2]->l;
        luv2 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

        copy_v2_v2(final_position[edge->uv1].uv, luv1);
        copy_v2_v2(final_position[edge->uv2].uv, luv2);
        final_position[edge->uv1].count = 1;
        final_position[edge->uv2].count = 1;

        state->uvs[edge->uv1]->flag |= STITCH_STITCHABLE;
        state->uvs[edge->uv2]->flag |= STITCH_STITCHABLE;

        if (ssc->snap_islands && edge->element->island == ssc->static_island && !stitch_midpoints)
        {
          continue;
        }

        for (edge_iter = edge->first; edge_iter; edge_iter = edge_iter->next) {
          if (stitch_check_edges_state_stitchable(cd_loop_uv_offset, edge, edge_iter, ssc, state))
          {
            l = state->uvs[edge_iter->uv1]->l;
            luv1 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
            l = state->uvs[edge_iter->uv2]->l;
            luv2 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

            if (stitch_midpoints) {
              add_v2_v2(final_position[edge->uv1].uv, luv1);
              final_position[edge->uv1].count++;
              add_v2_v2(final_position[edge->uv2].uv, luv2);
              final_position[edge->uv2].count++;
            }
            else if (edge_iter->element->island == ssc->static_island) {
              copy_v2_v2(final_position[edge->uv1].uv, luv1);
              copy_v2_v2(final_position[edge->uv2].uv, luv2);
            }
          }
        }
      }
    }
  }

  /* Take mean position here.
   * For edge case, this can't be done inside the loop for shared UV-verts. */
  if (ssc->mode == STITCH_EDGE && stitch_midpoints) {
    for (i = 0; i < state->total_separate_uvs; i++) {
      final_position[i].uv[0] /= final_position[i].count;
      final_position[i].uv[1] /= final_position[i].count;
    }
  }

  /* second pass, calculate island rotation and translation before modifying any uvs */
  if (ssc->snap_islands) {
    if (ssc->mode == STITCH_VERT) {
      for (i = 0; i < state->selection_size; i++) {
        UvElement *element = static_cast<UvElement *>(state->selection_stack[i]);

        if (element->flag & STITCH_STITCHABLE) {
          BMLoop *l;
          float *luv;

          l = element->l;
          luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

          /* accumulate each islands' translation from stitchable elements.
           * It is important to do here because in final pass MTFaces
           * get modified and result is zero. */
          island_stitch_data[element->island].translation[0] += final_position[i].uv[0] - luv[0];
          island_stitch_data[element->island].translation[1] += final_position[i].uv[1] - luv[1];
          island_stitch_data[element->island].medianPoint[0] += luv[0];
          island_stitch_data[element->island].medianPoint[1] += luv[1];
          island_stitch_data[element->island].numOfElements++;
        }
      }

      /* only calculate rotation when an edge has been fully selected */
      for (i = 0; i < state->total_separate_edges; i++) {
        UvEdge *edge = state->edges + i;
        if ((edge->flag & STITCH_BOUNDARY) && (state->uvs[edge->uv1]->flag & STITCH_STITCHABLE) &&
            (state->uvs[edge->uv2]->flag & STITCH_STITCHABLE))
        {
          stitch_island_calculate_edge_rotation(cd_loop_uv_offset,
                                                edge,
                                                ssc,
                                                state,
                                                final_position,
                                                uvfinal_map,
                                                island_stitch_data);
          island_stitch_data[state->uvs[edge->uv1]->island].use_edge_rotation = true;
        }
      }

      /* clear seams of stitched edges */
      if (final && ssc->clear_seams) {
        for (i = 0; i < state->total_separate_edges; i++) {
          UvEdge *edge = state->edges + i;
          if ((state->uvs[edge->uv1]->flag & STITCH_STITCHABLE) &&
              (state->uvs[edge->uv2]->flag & STITCH_STITCHABLE))
          {
            BM_elem_flag_disable(edge->element->l->e, BM_ELEM_SEAM);
          }
        }
      }

      for (i = 0; i < state->selection_size; i++) {
        UvElement *element = static_cast<UvElement *>(state->selection_stack[i]);
        if (!island_stitch_data[element->island].use_edge_rotation) {
          if (element->flag & STITCH_STITCHABLE) {
            stitch_island_calculate_vert_rotation(
                cd_loop_uv_offset, element, ssc, state, island_stitch_data);
          }
        }
      }
    }
    else {
      for (i = 0; i < state->total_separate_uvs; i++) {
        UvElement *element = state->uvs[i];

        if (element->flag & STITCH_STITCHABLE) {
          BMLoop *l;
          float *luv;

          l = element->l;
          luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

          /* accumulate each islands' translation from stitchable elements.
           * it is important to do here because in final pass MTFaces
           * get modified and result is zero. */
          island_stitch_data[element->island].translation[0] += final_position[i].uv[0] - luv[0];
          island_stitch_data[element->island].translation[1] += final_position[i].uv[1] - luv[1];
          island_stitch_data[element->island].medianPoint[0] += luv[0];
          island_stitch_data[element->island].medianPoint[1] += luv[1];
          island_stitch_data[element->island].numOfElements++;
        }
      }

      for (i = 0; i < state->selection_size; i++) {
        UvEdge *edge = static_cast<UvEdge *>(state->selection_stack[i]);

        if (edge->flag & STITCH_STITCHABLE) {
          stitch_island_calculate_edge_rotation(
              cd_loop_uv_offset, edge, ssc, state, final_position, nullptr, island_stitch_data);
          island_stitch_data[state->uvs[edge->uv1]->island].use_edge_rotation = true;
        }
      }

      /* clear seams of stitched edges */
      if (final && ssc->clear_seams) {
        for (i = 0; i < state->selection_size; i++) {
          UvEdge *edge = static_cast<UvEdge *>(state->selection_stack[i]);
          if (edge->flag & STITCH_STITCHABLE) {
            BM_elem_flag_disable(edge->element->l->e, BM_ELEM_SEAM);
          }
        }
      }
    }
  }

  /* third pass, propagate changes to coincident uvs */
  for (i = 0; i < state->selection_size; i++) {
    if (ssc->mode == STITCH_VERT) {
      UvElement *element = static_cast<UvElement *>(state->selection_stack[i]);

      stitch_propagate_uv_final_position(
          scene, element, i, preview_position, final_position, ssc, state, final);
    }
    else {
      UvEdge *edge = static_cast<UvEdge *>(state->selection_stack[i]);

      stitch_propagate_uv_final_position(scene,
                                         state->uvs[edge->uv1],
                                         edge->uv1,
                                         preview_position,
                                         final_position,
                                         ssc,
                                         state,
                                         final);
      stitch_propagate_uv_final_position(scene,
                                         state->uvs[edge->uv2],
                                         edge->uv2,
                                         preview_position,
                                         final_position,
                                         ssc,
                                         state,
                                         final);

      edge->flag &= (STITCH_SELECTED | STITCH_BOUNDARY);
    }
  }

  /* final pass, calculate Island translation/rotation if needed */
  if (ssc->snap_islands) {
    stitch_calculate_island_snapping(
        cd_loop_uv_offset, state, preview_position, preview, island_stitch_data, final);
  }

  MEM_freeN(final_position);
  if (ssc->mode == STITCH_VERT) {
    MEM_freeN(uvfinal_map);
  }
  MEM_freeN(island_stitch_data);
  MEM_freeN(preview_position);

  return 1;
}

static int stitch_process_data_all(StitchStateContainer *ssc, Scene *scene, int final)
{
  for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
    if (!stitch_process_data(ssc, ssc->states[ob_index], scene, final)) {
      return 0;
    }
  }

  return 1;
}

/* Stitch hash initialization functions */
static uint uv_edge_hash(const void *key)
{
  const UvEdge *edge = static_cast<const UvEdge *>(key);
  BLI_assert(edge->uv1 < edge->uv2);
  return (BLI_ghashutil_uinthash(edge->uv2) + BLI_ghashutil_uinthash(edge->uv1));
}

static bool uv_edge_compare(const void *a, const void *b)
{
  const UvEdge *edge1 = static_cast<const UvEdge *>(a);
  const UvEdge *edge2 = static_cast<const UvEdge *>(b);
  BLI_assert(edge1->uv1 < edge1->uv2);
  BLI_assert(edge2->uv1 < edge2->uv2);

  if ((edge1->uv1 == edge2->uv1) && (edge1->uv2 == edge2->uv2)) {
    return false;
  }
  return true;
}

/* select all common edges */
static void stitch_select_edge(UvEdge *edge, StitchState *state, int always_select)
{
  UvEdge *eiter;
  UvEdge **selection_stack = (UvEdge **)state->selection_stack;

  for (eiter = edge->first; eiter; eiter = eiter->next) {
    if (eiter->flag & STITCH_SELECTED) {
      int i;
      if (always_select) {
        continue;
      }

      eiter->flag &= ~STITCH_SELECTED;
      for (i = 0; i < state->selection_size; i++) {
        if (selection_stack[i] == eiter) {
          (state->selection_size)--;
          selection_stack[i] = selection_stack[state->selection_size];
          break;
        }
      }
    }
    else {
      eiter->flag |= STITCH_SELECTED;
      selection_stack[state->selection_size++] = eiter;
    }
  }
}

/* Select all common uvs */
static void stitch_select_uv(UvElement *element, StitchState *state, int always_select)
{
  UvElement **selection_stack = (UvElement **)state->selection_stack;
  UvElement *element_iter = BM_uv_element_get_head(state->element_map, element);
  /* first deselect all common uvs */
  for (; element_iter; element_iter = element_iter->next) {
    if (element_iter->separate) {
      /* only separators go to selection */
      if (element_iter->flag & STITCH_SELECTED) {
        int i;
        if (always_select) {
          continue;
        }

        element_iter->flag &= ~STITCH_SELECTED;
        for (i = 0; i < state->selection_size; i++) {
          if (selection_stack[i] == element_iter) {
            (state->selection_size)--;
            selection_stack[i] = selection_stack[state->selection_size];
            break;
          }
        }
      }
      else {
        element_iter->flag |= STITCH_SELECTED;
        selection_stack[state->selection_size++] = element_iter;
      }
    }
  }
}

static void stitch_set_selection_mode(StitchState *state, const char from_stitch_mode)
{
  void **old_selection_stack = state->selection_stack;
  int old_selection_size = state->selection_size;
  state->selection_size = 0;

  if (from_stitch_mode == STITCH_VERT) {
    int i;
    state->selection_stack = static_cast<void **>(
        MEM_mallocN(state->total_separate_edges * sizeof(*state->selection_stack),
                    "stitch_new_edge_selection_stack"));

    /* check if both elements of an edge are selected */
    for (i = 0; i < state->total_separate_edges; i++) {
      UvEdge *edge = state->edges + i;
      UvElement *element1 = state->uvs[edge->uv1];
      UvElement *element2 = state->uvs[edge->uv2];

      if ((element1->flag & STITCH_SELECTED) && (element2->flag & STITCH_SELECTED)) {
        stitch_select_edge(edge, state, true);
      }
    }

    /* unselect selected uvelements */
    for (i = 0; i < old_selection_size; i++) {
      UvElement *element = static_cast<UvElement *>(old_selection_stack[i]);

      element->flag &= ~STITCH_SELECTED;
    }
  }
  else {
    int i;
    state->selection_stack = static_cast<void **>(
        MEM_mallocN(state->total_separate_uvs * sizeof(*state->selection_stack),
                    "stitch_new_vert_selection_stack"));

    for (i = 0; i < old_selection_size; i++) {
      UvEdge *edge = static_cast<UvEdge *>(old_selection_stack[i]);
      UvElement *element1 = state->uvs[edge->uv1];
      UvElement *element2 = state->uvs[edge->uv2];

      stitch_select_uv(element1, state, true);
      stitch_select_uv(element2, state, true);

      edge->flag &= ~STITCH_SELECTED;
    }
  }
  MEM_freeN(old_selection_stack);
}

static void stitch_switch_selection_mode_all(StitchStateContainer *ssc)
{
  for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
    stitch_set_selection_mode(ssc->states[ob_index], ssc->mode);
  }

  if (ssc->mode == STITCH_VERT) {
    ssc->mode = STITCH_EDGE;
  }
  else {
    ssc->mode = STITCH_VERT;
  }
}

static void stitch_calculate_edge_normal(const int cd_loop_uv_offset,
                                         UvEdge *edge,
                                         float *normal,
                                         float aspect)
{
  BMLoop *l1 = edge->element->l;
  float tangent[2];

  float *luv1 = BM_ELEM_CD_GET_FLOAT_P(l1, cd_loop_uv_offset);
  float *luv2 = BM_ELEM_CD_GET_FLOAT_P(l1->next, cd_loop_uv_offset);

  sub_v2_v2v2(tangent, luv2, luv1);

  tangent[1] /= aspect;

  normal[0] = tangent[1];
  normal[1] = -tangent[0];

  normalize_v2(normal);
}

/**
 */
static void stitch_draw_vbo(blender::gpu::VertBuf *vbo, GPUPrimType prim_type, const float col[4])
{
  blender::gpu::Batch *batch = GPU_batch_create_ex(prim_type, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_batch_uniform_4fv(batch, "color", col);
  GPU_batch_draw(batch);
  GPU_batch_discard(batch);
}

/* TODO: make things prettier : store batches inside StitchPreviewer instead of the bare verts pos
 */
static void stitch_draw(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{

  StitchStateContainer *ssc = (StitchStateContainer *)arg;

  for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
    int j, index = 0;
    uint num_line = 0, num_tri, tri_idx = 0, line_idx = 0;
    StitchState *state = ssc->states[ob_index];
    StitchPreviewer *stitch_preview = state->stitch_preview;
    blender::gpu::VertBuf *vbo, *vbo_line;
    float col[4];

    static GPUVertFormat format = {0};
    static uint pos_id;
    if (format.attr_len == 0) {
      pos_id = GPU_vertformat_attr_add(&format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    }

    GPU_blend(GPU_BLEND_ALPHA);

    /* Static Triangles. */
    if (stitch_preview->static_tris) {
      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_ACTIVE, col);
      vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*vbo, stitch_preview->num_static_tris * 3);
      for (int i = 0; i < stitch_preview->num_static_tris * 3; i++) {
        GPU_vertbuf_attr_set(vbo, pos_id, i, &stitch_preview->static_tris[i * 2]);
      }
      stitch_draw_vbo(vbo, GPU_PRIM_TRIS, col);
    }

    /* Preview Polys */
    if (stitch_preview->preview_polys) {
      for (int i = 0; i < stitch_preview->num_polys; i++) {
        num_line += stitch_preview->uvs_per_polygon[i];
      }

      num_tri = num_line - 2 * stitch_preview->num_polys;

      /* we need to convert the polys into triangles / lines */
      vbo = GPU_vertbuf_create_with_format(format);
      vbo_line = GPU_vertbuf_create_with_format(format);

      GPU_vertbuf_data_alloc(*vbo, num_tri * 3);
      GPU_vertbuf_data_alloc(*vbo_line, num_line * 2);

      for (int i = 0; i < stitch_preview->num_polys; i++) {
        BLI_assert(stitch_preview->uvs_per_polygon[i] >= 3);

        /* Start line */
        GPU_vertbuf_attr_set(vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index]);
        GPU_vertbuf_attr_set(
            vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index + 2]);

        for (j = 1; j < stitch_preview->uvs_per_polygon[i] - 1; j++) {
          GPU_vertbuf_attr_set(vbo, pos_id, tri_idx++, &stitch_preview->preview_polys[index]);
          GPU_vertbuf_attr_set(
              vbo, pos_id, tri_idx++, &stitch_preview->preview_polys[index + (j + 0) * 2]);
          GPU_vertbuf_attr_set(
              vbo, pos_id, tri_idx++, &stitch_preview->preview_polys[index + (j + 1) * 2]);

          GPU_vertbuf_attr_set(
              vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index + (j + 0) * 2]);
          GPU_vertbuf_attr_set(
              vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index + (j + 1) * 2]);
        }

        /* Closing line */
        GPU_vertbuf_attr_set(vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index]);
        /* `j = uvs_per_polygon[i] - 1` */
        GPU_vertbuf_attr_set(
            vbo_line, pos_id, line_idx++, &stitch_preview->preview_polys[index + j * 2]);

        index += stitch_preview->uvs_per_polygon[i] * 2;
      }

      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_FACE, col);
      stitch_draw_vbo(vbo, GPU_PRIM_TRIS, col);
      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_EDGE, col);
      stitch_draw_vbo(vbo_line, GPU_PRIM_LINES, col);
    }

    GPU_blend(GPU_BLEND_NONE);

    /* draw stitch vert/lines preview */
    if (ssc->mode == STITCH_VERT) {
      GPU_point_size(UI_GetThemeValuef(TH_VERTEX_SIZE) * 2.0f);

      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_STITCHABLE, col);
      vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*vbo, stitch_preview->num_stitchable);
      for (int i = 0; i < stitch_preview->num_stitchable; i++) {
        GPU_vertbuf_attr_set(vbo, pos_id, i, &stitch_preview->preview_stitchable[i * 2]);
      }
      stitch_draw_vbo(vbo, GPU_PRIM_POINTS, col);

      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_UNSTITCHABLE, col);
      vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*vbo, stitch_preview->num_unstitchable);
      for (int i = 0; i < stitch_preview->num_unstitchable; i++) {
        GPU_vertbuf_attr_set(vbo, pos_id, i, &stitch_preview->preview_unstitchable[i * 2]);
      }
      stitch_draw_vbo(vbo, GPU_PRIM_POINTS, col);
    }
    else {
      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_STITCHABLE, col);
      vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*vbo, stitch_preview->num_stitchable * 2);
      for (int i = 0; i < stitch_preview->num_stitchable * 2; i++) {
        GPU_vertbuf_attr_set(vbo, pos_id, i, &stitch_preview->preview_stitchable[i * 2]);
      }
      stitch_draw_vbo(vbo, GPU_PRIM_LINES, col);

      UI_GetThemeColor4fv(TH_STITCH_PREVIEW_UNSTITCHABLE, col);
      vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*vbo, stitch_preview->num_unstitchable * 2);
      for (int i = 0; i < stitch_preview->num_unstitchable * 2; i++) {
        GPU_vertbuf_attr_set(vbo, pos_id, i, &stitch_preview->preview_unstitchable[i * 2]);
      }
      stitch_draw_vbo(vbo, GPU_PRIM_LINES, col);
    }
  }
}

static UvEdge *uv_edge_get(BMLoop *l, StitchState *state)
{
  UvEdge tmp_edge;

  UvElement *element1 = BM_uv_element_get(state->element_map, l);
  UvElement *element2 = BM_uv_element_get(state->element_map, l->next);

  if (!element1 || !element2) {
    return nullptr;
  }

  int uv1 = state->map[element1 - state->element_map->storage];
  int uv2 = state->map[element2 - state->element_map->storage];

  if (uv1 < uv2) {
    tmp_edge.uv1 = uv1;
    tmp_edge.uv2 = uv2;
  }
  else {
    tmp_edge.uv1 = uv2;
    tmp_edge.uv2 = uv1;
  }

  return static_cast<UvEdge *>(BLI_ghash_lookup(state->edge_hash, &tmp_edge));
}

static StitchState *stitch_init(bContext *C,
                                wmOperator *op,
                                StitchStateContainer *ssc,
                                Object *obedit,
                                StitchStateInit *state_init)
{
  /* for fast edge lookup... */
  GHash *edge_hash;
  /* ...and actual edge storage */
  UvEdge *edges;
  int total_edges;
  /* maps uvelements to their first coincident uv */
  int *map;
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  GHashIterator gh_iter;
  UvEdge *all_edges;
  StitchState *state;
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

  state = MEM_callocN<StitchState>("stitch state obj");

  /* initialize state */
  state->obedit = obedit;
  state->em = em;

  /* Workaround for sync-select & face-select mode which implies all selected faces are detached,
   * for stitch this isn't useful behavior, see #86924. */
  const int selectmode_orig = scene->toolsettings->selectmode;
  scene->toolsettings->selectmode = SCE_SELECT_VERTEX;
  state->element_map = BM_uv_element_map_create(state->em->bm, scene, false, true, true, true);
  scene->toolsettings->selectmode = selectmode_orig;

  if (!state->element_map) {
    state_delete(state);
    return nullptr;
  }

  state->aspect = ED_uvedit_get_aspect_y(obedit);

  int unique_uvs = state->element_map->total_unique_uvs;
  state->total_separate_uvs = unique_uvs;

  /* Allocate the unique uv buffers */
  state->uvs = static_cast<UvElement **>(
      MEM_mallocN(sizeof(*state->uvs) * unique_uvs, "uv_stitch_unique_uvs"));
  /* internal uvs need no normals but it is hard and slow to keep a map of
   * normals only for boundary uvs, so allocating for all uvs.
   * Times 2 because each `float[2]` is stored as `{n[2 * i], n[2*i + 1]}`. */
  state->normals = MEM_calloc_arrayN<float>(2 * unique_uvs, "uv_stitch_normals");
  state->map = map = MEM_malloc_arrayN<int>(state->element_map->total_uvs, "uv_stitch_unique_map");
  /* Allocate the edge stack */
  edge_hash = BLI_ghash_new(uv_edge_hash, uv_edge_compare, "stitch_edge_hash");
  all_edges = MEM_malloc_arrayN<UvEdge>(state->element_map->total_uvs, "ssc_edges");

  BLI_assert(!state->stitch_preview); /* Paranoia. */
  if (!state->uvs || !map || !edge_hash || !all_edges) {
    state_delete(state);
    return nullptr;
  }

  /* Index for the UvElements. */
  int counter = -1;
  /* initialize the unique UVs and map */
  for (int i = 0; i < em->bm->totvert; i++) {
    UvElement *element = state->element_map->vertex[i];
    for (; element; element = element->next) {
      if (element->separate) {
        counter++;
        state->uvs[counter] = element;
      }
      /* Pointer arithmetic to the rescue, as always :). */
      map[element - state->element_map->storage] = counter;
    }
  }

  counter = 0;
  /* Now, on to generate our uv connectivity data */
  const bool face_selected = !(ts->uv_flag & UV_FLAG_SELECT_SYNC);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (face_selected && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      UvElement *element = BM_uv_element_get(state->element_map, l);
      int itmp1 = element - state->element_map->storage;
      int itmp2 = BM_uv_element_get(state->element_map, l->next) - state->element_map->storage;
      UvEdge *edge;

      int offset1 = map[itmp1];
      int offset2 = map[itmp2];

      all_edges[counter].next = nullptr;
      all_edges[counter].first = nullptr;
      all_edges[counter].flag = 0;
      all_edges[counter].element = element;
      /* Using an order policy, sort UVs according to address space.
       * This avoids having two different UvEdges with the same UVs on different positions. */
      if (offset1 < offset2) {
        all_edges[counter].uv1 = offset1;
        all_edges[counter].uv2 = offset2;
      }
      else {
        all_edges[counter].uv1 = offset2;
        all_edges[counter].uv2 = offset1;
      }

      edge = static_cast<UvEdge *>(BLI_ghash_lookup(edge_hash, &all_edges[counter]));
      if (edge) {
        edge->flag = 0;
      }
      else {
        BLI_ghash_insert(edge_hash, &all_edges[counter], &all_edges[counter]);
        all_edges[counter].flag = STITCH_BOUNDARY;
      }
      counter++;
    }
  }

  total_edges = BLI_ghash_len(edge_hash);
  state->edges = edges = MEM_malloc_arrayN<UvEdge>(total_edges, "stitch_edges");

  /* I assume any system will be able to at least allocate an iterator :p */
  if (!edges) {
    state_delete(state);
    return nullptr;
  }

  state->total_separate_edges = total_edges;

  /* fill the edges with data */
  int i = 0;
  GHASH_ITER (gh_iter, edge_hash) {
    edges[i++] = *((UvEdge *)BLI_ghashIterator_getKey(&gh_iter));
  }

  /* cleanup temporary stuff */
  MEM_freeN(all_edges);

  BLI_ghash_free(edge_hash, nullptr, nullptr);

  /* Refill an edge hash to create edge connectivity data. */
  state->edge_hash = edge_hash = BLI_ghash_new(uv_edge_hash, uv_edge_compare, "stitch_edge_hash");
  for (i = 0; i < total_edges; i++) {
    BLI_ghash_insert(edge_hash, edges + i, edges + i);
  }
  stitch_uv_edge_generate_linked_edges(edge_hash, state);

  /***** calculate 2D normals for boundary uvs *****/

  /* we use boundary edges to calculate 2D normals.
   * to disambiguate the direction of the normal, we also need
   * a point "inside" the island, that can be provided by
   * the winding of the face (assuming counter-clockwise flow). */

  for (i = 0; i < total_edges; i++) {
    UvEdge *edge = edges + i;
    float normal[2];
    if (edge->flag & STITCH_BOUNDARY) {
      stitch_calculate_edge_normal(offsets.uv, edge, normal, state->aspect);

      add_v2_v2(state->normals + edge->uv1 * 2, normal);
      add_v2_v2(state->normals + edge->uv2 * 2, normal);

      normalize_v2(state->normals + edge->uv1 * 2);
      normalize_v2(state->normals + edge->uv2 * 2);
    }
  }

  /***** fill selection stack *******/

  state->selection_size = 0;

  /* Load old selection if redoing operator with different settings */
  if (state_init != nullptr) {
    int faceIndex, elementIndex;
    UvElement *element;
    enum StitchModes stored_mode = StitchModes(RNA_enum_get(op->ptr, "stored_mode"));

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);

    int selected_count = state_init->uv_selected_count;

    if (stored_mode == STITCH_VERT) {
      state->selection_stack = static_cast<void **>(
          MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_uvs,
                      "uv_stitch_selection_stack"));

      while (selected_count--) {
        faceIndex = state_init->to_select[selected_count].faceIndex;
        elementIndex = state_init->to_select[selected_count].elementIndex;
        efa = BM_face_at_index(em->bm, faceIndex);
        element = BM_uv_element_get(
            state->element_map,
            static_cast<BMLoop *>(BM_iter_at_index(nullptr, BM_LOOPS_OF_FACE, efa, elementIndex)));
        stitch_select_uv(element, state, 1);
      }
    }
    else {
      state->selection_stack = static_cast<void **>(
          MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_edges,
                      "uv_stitch_selection_stack"));

      while (selected_count--) {
        UvEdge tmp_edge, *edge;
        int uv1, uv2;
        faceIndex = state_init->to_select[selected_count].faceIndex;
        elementIndex = state_init->to_select[selected_count].elementIndex;
        efa = BM_face_at_index(em->bm, faceIndex);
        element = BM_uv_element_get(
            state->element_map,
            static_cast<BMLoop *>(BM_iter_at_index(nullptr, BM_LOOPS_OF_FACE, efa, elementIndex)));
        uv1 = map[element - state->element_map->storage];

        element = BM_uv_element_get(
            state->element_map,
            static_cast<BMLoop *>(
                BM_iter_at_index(nullptr, BM_LOOPS_OF_FACE, efa, (elementIndex + 1) % efa->len)));
        uv2 = map[element - state->element_map->storage];

        if (uv1 < uv2) {
          tmp_edge.uv1 = uv1;
          tmp_edge.uv2 = uv2;
        }
        else {
          tmp_edge.uv1 = uv2;
          tmp_edge.uv2 = uv1;
        }

        edge = static_cast<UvEdge *>(BLI_ghash_lookup(edge_hash, &tmp_edge));

        stitch_select_edge(edge, state, true);
      }
    }
    /* if user has switched the operator mode after operation, we need to convert
     * the stored format */
    if (ssc->mode != stored_mode) {
      stitch_set_selection_mode(state, stored_mode);
    }
  }
  else {
    if (ssc->mode == STITCH_VERT) {
      state->selection_stack = static_cast<void **>(
          MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_uvs,
                      "uv_stitch_selection_stack"));

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
          if (uvedit_uv_select_test(scene, em->bm, l, offsets)) {
            UvElement *element = BM_uv_element_get(state->element_map, l);
            if (element) {
              stitch_select_uv(element, state, 1);
            }
          }
        }
      }
    }
    else {
      state->selection_stack = static_cast<void **>(
          MEM_mallocN(sizeof(*state->selection_stack) * state->total_separate_edges,
                      "uv_stitch_selection_stack"));

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!(ts->uv_flag & UV_FLAG_SELECT_SYNC) &&
            (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) || !BM_elem_flag_test(efa, BM_ELEM_SELECT)))
        {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_edge_select_test(scene, em->bm, l, offsets)) {
            UvEdge *edge = uv_edge_get(l, state);
            if (edge) {
              stitch_select_edge(edge, state, true);
            }
          }
        }
      }
    }
  }

  /***** initialize static island preview data *****/

  state->tris_per_island = MEM_malloc_arrayN<uint>(state->element_map->total_islands,
                                                   "stitch island tris");
  for (i = 0; i < state->element_map->total_islands; i++) {
    state->tris_per_island[i] = 0;
  }

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    UvElement *element = BM_uv_element_get(state->element_map, BM_FACE_FIRST_LOOP(efa));

    if (element) {
      state->tris_per_island[element->island] += (efa->len > 2) ? efa->len - 2 : 0;
    }
  }

  state->island_is_stitchable = MEM_calloc_arrayN<bool>(state->element_map->total_islands,
                                                        "stitch I stops");
  if (!state->island_is_stitchable) {
    state_delete(state);
    return nullptr;
  }

  if (!stitch_process_data(ssc, state, scene, false)) {
    state_delete(state);
    return nullptr;
  }

  return state;
}

static bool goto_next_island(StitchStateContainer *ssc)
{
  StitchState *active_state = ssc->states[ssc->active_object_index];
  StitchState *original_active_state = active_state;

  int original_island = ssc->static_island;

  do {
    ssc->static_island++;
    if (ssc->static_island >= active_state->element_map->total_islands) {
      /* go to next object */
      ssc->active_object_index++;
      ssc->active_object_index %= ssc->objects_len;

      active_state = ssc->states[ssc->active_object_index];
      ssc->static_island = 0;
    }

    if (active_state->island_is_stitchable[ssc->static_island]) {
      /* We're at an island to make active */
      return true;
    }
  } while (!(active_state == original_active_state && ssc->static_island == original_island));

  return false;
}

static int stitch_init_all(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  if (!region) {
    return 0;
  }

  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;

  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, v3d);

  if (objects.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No objects selected");
    return 0;
  }

  if (objects.size() > RNA_MAX_ARRAY_LENGTH) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Stitching only works with less than %i objects selected (%i selected)",
                RNA_MAX_ARRAY_LENGTH,
                int(objects.size()));
    return 0;
  }

  StitchStateContainer *ssc = MEM_callocN<StitchStateContainer>("stitch collection");

  op->customdata = ssc;

  ssc->use_limit = RNA_boolean_get(op->ptr, "use_limit");
  ssc->limit_dist = RNA_float_get(op->ptr, "limit");
  ssc->snap_islands = RNA_boolean_get(op->ptr, "snap_islands");
  ssc->midpoints = RNA_boolean_get(op->ptr, "midpoint_snap");
  ssc->clear_seams = RNA_boolean_get(op->ptr, "clear_seams");
  ssc->active_object_index = RNA_int_get(op->ptr, "active_object_index");
  ssc->static_island = 0;

  if (RNA_struct_property_is_set(op->ptr, "mode")) {
    ssc->mode = RNA_enum_get(op->ptr, "mode");
  }
  else {
    if (ts->uv_flag & UV_FLAG_SELECT_SYNC) {
      if (ts->selectmode & SCE_SELECT_VERTEX) {
        ssc->mode = STITCH_VERT;
      }
      else {
        ssc->mode = STITCH_EDGE;
      }
    }
    else {
      if (ts->uv_selectmode & UV_SELECT_VERT) {
        ssc->mode = STITCH_VERT;
      }
      else {
        ssc->mode = STITCH_EDGE;
      }
    }
  }

  ssc->objects = MEM_calloc_arrayN<Object *>(objects.size(), "Object *ssc->objects");
  ssc->states = MEM_calloc_arrayN<StitchState *>(objects.size(), "StitchState");
  ssc->objects_len = 0;

  int *objs_selection_count = nullptr;
  UvElementID *selected_uvs_arr = nullptr;
  StitchStateInit *state_init = nullptr;

  if (RNA_struct_property_is_set(op->ptr, "selection") &&
      RNA_struct_property_is_set(op->ptr, "objects_selection_count"))
  {
    /* Retrieve list of selected UVs, one list contains all selected UVs
     * for all objects. */

    objs_selection_count = static_cast<int *>(
        MEM_mallocN(sizeof(int *) * objects.size(), "objects_selection_count"));
    RNA_int_get_array(op->ptr, "objects_selection_count", objs_selection_count);

    int total_selected = 0;
    for (uint ob_index = 0; ob_index < objects.size(); ob_index++) {
      total_selected += objs_selection_count[ob_index];
    }

    selected_uvs_arr = MEM_calloc_arrayN<UvElementID>(total_selected, "selected_uvs_arr");
    int sel_idx = 0;
    RNA_BEGIN (op->ptr, itemptr, "selection") {
      BLI_assert(sel_idx < total_selected);
      selected_uvs_arr[sel_idx].faceIndex = RNA_int_get(&itemptr, "face_index");
      selected_uvs_arr[sel_idx].elementIndex = RNA_int_get(&itemptr, "element_index");
      sel_idx++;
    }
    RNA_END;

    RNA_collection_clear(op->ptr, "selection");

    state_init = MEM_callocN<StitchStateInit>("UV_init_selected");
    state_init->to_select = selected_uvs_arr;
  }

  for (uint ob_index = 0; ob_index < objects.size(); ob_index++) {
    Object *obedit = objects[ob_index];

    if (state_init != nullptr) {
      state_init->uv_selected_count = objs_selection_count[ob_index];
    }

    StitchState *stitch_state_ob = stitch_init(C, op, ssc, obedit, state_init);

    if (state_init != nullptr) {
      /* Move pointer to beginning of next object's data. */
      state_init->to_select += state_init->uv_selected_count;
    }

    if (stitch_state_ob) {
      ssc->objects[ssc->objects_len] = obedit;
      ssc->states[ssc->objects_len] = stitch_state_ob;
      ssc->objects_len++;
    }
  }

  MEM_SAFE_FREE(selected_uvs_arr);
  MEM_SAFE_FREE(objs_selection_count);
  MEM_SAFE_FREE(state_init);

  if (ssc->objects_len == 0) {
    state_delete_all(ssc);
    BKE_report(op->reports, RPT_ERROR, "Could not initialize stitching on any selected object");
    return 0;
  }

  ssc->active_object_index %= ssc->objects_len;

  ssc->static_island = RNA_int_get(op->ptr, "static_island");

  StitchState *state = ssc->states[ssc->active_object_index];
  ssc->static_island %= state->element_map->total_islands;

  /* If the initial active object doesn't have any stitchable islands
   * then no active island will be seen in the UI.
   * Make sure we're on a stitchable object and island. */
  if (!state->island_is_stitchable[ssc->static_island]) {
    goto_next_island(ssc);
    state = ssc->states[ssc->active_object_index];
  }

  /* process active stitchobj again now that it can detect it's the active stitchobj */
  stitch_process_data(ssc, state, scene, false);

  stitch_update_header(ssc, C);

  ssc->draw_handle = ED_region_draw_cb_activate(
      region->runtime->type, stitch_draw, ssc, REGION_DRAW_POST_VIEW);

  return 1;
}

static wmOperatorStatus stitch_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (!stitch_init_all(C, op)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);

  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  StitchStateContainer *ssc = (StitchStateContainer *)op->customdata;

  for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
    StitchState *state = ssc->states[ob_index];
    Object *obedit = state->obedit;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_RUNNING_MODAL;
}

static void stitch_exit(bContext *C, wmOperator *op, int finished)
{
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ScrArea *area = CTX_wm_area(C);

  StitchStateContainer *ssc = (StitchStateContainer *)op->customdata;

  if (finished) {
    RNA_float_set(op->ptr, "limit", ssc->limit_dist);
    RNA_boolean_set(op->ptr, "use_limit", ssc->use_limit);
    RNA_boolean_set(op->ptr, "snap_islands", ssc->snap_islands);
    RNA_boolean_set(op->ptr, "midpoint_snap", ssc->midpoints);
    RNA_boolean_set(op->ptr, "clear_seams", ssc->clear_seams);
    RNA_enum_set(op->ptr, "mode", ssc->mode);
    RNA_enum_set(op->ptr, "stored_mode", ssc->mode);
    RNA_int_set(op->ptr, "active_object_index", ssc->active_object_index);

    RNA_int_set(op->ptr, "static_island", ssc->static_island);

    int *objs_selection_count = nullptr;
    objs_selection_count = static_cast<int *>(
        MEM_mallocN(sizeof(int *) * ssc->objects_len, "objects_selection_count"));

    /* Store selection for re-execution of stitch
     * - Store all selected UVs in "selection"
     * - Store how many each object has in "objects_selection_count". */
    RNA_collection_clear(op->ptr, "selection");
    for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
      StitchState *state = ssc->states[ob_index];
      Object *obedit = state->obedit;

      PointerRNA itemptr;
      for (int i = 0; i < state->selection_size; i++) {
        UvElement *element;

        if (ssc->mode == STITCH_VERT) {
          element = static_cast<UvElement *>(state->selection_stack[i]);
        }
        else {
          element = ((UvEdge *)state->selection_stack[i])->element;
        }
        RNA_collection_add(op->ptr, "selection", &itemptr);

        RNA_int_set(&itemptr, "face_index", BM_elem_index_get(element->l->f));
        RNA_int_set(&itemptr, "element_index", element->loop_of_face_index);
      }
      uvedit_live_unwrap_update(sima, scene, obedit);

      objs_selection_count[ob_index] = state->selection_size;
    }

    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "objects_selection_count");
    RNA_def_property_array(prop, ssc->objects_len);
    RNA_int_set_array(op->ptr, "objects_selection_count", objs_selection_count);
    MEM_freeN(objs_selection_count);
  }

  if (area) {
    ED_workspace_status_text(C, nullptr);
  }

  ED_region_draw_cb_exit(CTX_wm_region(C)->runtime->type, ssc->draw_handle);

  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
    StitchState *state = ssc->states[ob_index];
    Object *obedit = state->obedit;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  state_delete_all(ssc);

  op->customdata = nullptr;
}

static void stitch_cancel(bContext *C, wmOperator *op)
{
  stitch_exit(C, op, 0);
}

static wmOperatorStatus stitch_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  if (!stitch_init_all(C, op)) {
    return OPERATOR_CANCELLED;
  }
  if (stitch_process_data_all((StitchStateContainer *)op->customdata, scene, 1)) {
    stitch_exit(C, op, 1);
    return OPERATOR_FINISHED;
  }
  stitch_cancel(C, op);
  return OPERATOR_CANCELLED;
}

static StitchState *stitch_select(bContext *C,
                                  Scene *scene,
                                  const wmEvent *event,
                                  StitchStateContainer *ssc)
{
  /* add uv under mouse to processed uv's */
  float co[2];
  ARegion *region = CTX_wm_region(C);
  UvNearestHit hit = uv_nearest_hit_init_max(&region->v2d);

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);

  if (ssc->mode == STITCH_VERT) {
    if (uv_find_nearest_vert_multi(scene, {ssc->objects, ssc->objects_len}, co, 0.0f, &hit)) {
      /* Add vertex to selection, deselect all common uv's of vert other than selected and
       * update the preview. This behavior was decided so that you can do stuff like deselect
       * the opposite stitchable vertex and the initial still gets deselected */

      /* find StitchState from hit->ob */
      StitchState *state = nullptr;
      for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
        if (hit.ob == ssc->objects[ob_index]) {
          state = ssc->states[ob_index];
          break;
        }
      }

      /* This works due to setting of tmp in find nearest uv vert */
      UvElement *element = BM_uv_element_get(state->element_map, hit.l);
      if (element) {
        stitch_select_uv(element, state, false);
      }

      return state;
    }
  }
  else if (uv_find_nearest_edge_multi(scene, {ssc->objects, ssc->objects_len}, co, 0.0f, &hit)) {
    /* find StitchState from hit->ob */
    StitchState *state = nullptr;
    for (uint ob_index = 0; ob_index < ssc->objects_len; ob_index++) {
      if (hit.ob == ssc->objects[ob_index]) {
        state = ssc->states[ob_index];
        break;
      }
    }

    UvEdge *edge = uv_edge_get(hit.l, state);
    stitch_select_edge(edge, state, false);

    return state;
  }

  return nullptr;
}

static wmOperatorStatus stitch_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  StitchStateContainer *ssc;
  Scene *scene = CTX_data_scene(C);

  ssc = static_cast<StitchStateContainer *>(op->customdata);
  StitchState *active_state = ssc->states[ssc->active_object_index];

  switch (event->type) {
    case MIDDLEMOUSE:
      return OPERATOR_PASS_THROUGH;

      /* Cancel */
    case EVT_ESCKEY:
      stitch_cancel(C, op);
      return OPERATOR_CANCELLED;

    case LEFTMOUSE:
    case EVT_PADENTER:
    case EVT_RETKEY:
      if (event->val == KM_PRESS) {
        if (stitch_process_data(ssc, active_state, scene, true)) {
          stitch_exit(C, op, 1);
          return OPERATOR_FINISHED;
        }

        stitch_cancel(C, op);
        return OPERATOR_CANCELLED;
      }
      return OPERATOR_PASS_THROUGH;

      /* Increase limit */
    case EVT_PADPLUSKEY:
    case WHEELUPMOUSE:
      if ((event->val == KM_PRESS) && (event->modifier & KM_ALT)) {
        ssc->limit_dist += 0.01f;
        if (!stitch_process_data(ssc, active_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        break;
      }
      else {
        return OPERATOR_PASS_THROUGH;
      }
      /* Decrease limit */
    case EVT_PADMINUS:
    case WHEELDOWNMOUSE:
      if ((event->val == KM_PRESS) && (event->modifier & KM_ALT)) {
        ssc->limit_dist -= 0.01f;
        ssc->limit_dist = std::max(0.01f, ssc->limit_dist);
        if (!stitch_process_data(ssc, active_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        break;
      }
      else {
        return OPERATOR_PASS_THROUGH;
      }

      /* Use Limit (Default off) */
    case EVT_LKEY:
      if (event->val == KM_PRESS) {
        ssc->use_limit = !ssc->use_limit;
        if (!stitch_process_data(ssc, active_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        break;
      }
      return OPERATOR_RUNNING_MODAL;

    case EVT_IKEY:
      if (event->val == KM_PRESS) {
        /* Move to next island and maybe next object */

        if (goto_next_island(ssc)) {
          StitchState *new_active_state = ssc->states[ssc->active_object_index];

          /* active_state is the original active state */
          if (active_state != new_active_state) {
            if (!stitch_process_data(ssc, active_state, scene, false)) {
              stitch_cancel(C, op);
              return OPERATOR_CANCELLED;
            }
          }

          if (!stitch_process_data(ssc, new_active_state, scene, false)) {
            stitch_cancel(C, op);
            return OPERATOR_CANCELLED;
          }
        }
        break;
      }
      return OPERATOR_RUNNING_MODAL;

    case EVT_MKEY:
      if (event->val == KM_PRESS) {
        ssc->midpoints = !ssc->midpoints;
        if (!stitch_process_data(ssc, active_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
      }
      break;

      /* Select geometry */
    case RIGHTMOUSE:
      if ((event->modifier & KM_SHIFT) == 0) {
        stitch_cancel(C, op);
        return OPERATOR_CANCELLED;
      }
      if (event->val == KM_PRESS) {
        StitchState *selected_state = stitch_select(C, scene, event, ssc);

        if (selected_state && !stitch_process_data(ssc, selected_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        break;
      }
      return OPERATOR_RUNNING_MODAL;

      /* snap islands on/off */
    case EVT_SKEY:
      if (event->val == KM_PRESS) {
        ssc->snap_islands = !ssc->snap_islands;
        if (!stitch_process_data(ssc, active_state, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        break;
      }
      else {
        return OPERATOR_RUNNING_MODAL;
      }

      /* switch between edge/vertex mode */
    case EVT_TABKEY:
      if (event->val == KM_PRESS) {
        stitch_switch_selection_mode_all(ssc);

        if (!stitch_process_data_all(ssc, scene, false)) {
          stitch_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
      }
      break;

    default:
      return OPERATOR_RUNNING_MODAL;
  }

  /* if updated settings, renew feedback message */
  stitch_update_header(ssc, C);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_RUNNING_MODAL;
}

void UV_OT_stitch(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem stitch_modes[] = {
      {STITCH_VERT, "VERTEX", 0, "Vertex", ""},
      {STITCH_EDGE, "EDGE", 0, "Edge", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Stitch";
  ot->description = "Stitch selected UV vertices by proximity";
  ot->idname = "UV_OT_stitch";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->invoke = stitch_invoke;
  ot->modal = stitch_modal;
  ot->exec = stitch_exec;
  ot->cancel = stitch_cancel;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(
      ot->srna, "use_limit", false, "Use Limit", "Stitch UVs within a specified limit distance");
  RNA_def_boolean(ot->srna,
                  "snap_islands",
                  true,
                  "Snap Islands",
                  "Snap islands together (on edge stitch mode, rotates the islands too)");

  RNA_def_float(ot->srna,
                "limit",
                0.01f,
                0.0f,
                FLT_MAX,
                "Limit",
                "Limit distance in normalized coordinates",
                0.0,
                FLT_MAX);
  RNA_def_int(ot->srna,
              "static_island",
              0,
              0,
              INT_MAX,
              "Static Island",
              "Island that stays in place when stitching islands",
              0,
              INT_MAX);
  RNA_def_int(ot->srna,
              "active_object_index",
              0,
              0,
              INT_MAX,
              "Active Object",
              "Index of the active object",
              0,
              INT_MAX);
  RNA_def_boolean(ot->srna,
                  "midpoint_snap",
                  false,
                  "Snap at Midpoint",
                  "UVs are stitched at midpoint instead of at static island");
  RNA_def_boolean(ot->srna, "clear_seams", true, "Clear Seams", "Clear seams of stitched edges");
  RNA_def_enum(ot->srna,
               "mode",
               stitch_modes,
               STITCH_VERT,
               "Operation Mode",
               "Use vertex or edge stitching");
  prop = RNA_def_enum(ot->srna,
                      "stored_mode",
                      stitch_modes,
                      STITCH_VERT,
                      "Stored Operation Mode",
                      "Use vertex or edge stitching");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_collection_runtime(
      ot->srna, "selection", &RNA_SelectedUvElement, "Selection", "");
  /* Selection should not be editable or viewed in toolbar */
  RNA_def_property_flag(prop, PROP_HIDDEN);

  /* test should not be editable or viewed in toolbar */
  prop = RNA_def_int_array(ot->srna,
                           "objects_selection_count",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Objects Selection Count",
                           "",
                           0,
                           INT_MAX);
  RNA_def_property_array(prop, 6);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}
