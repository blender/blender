/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "bmesh.h"

#include "ED_mesh.h"
#include "ED_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "WM_api.h"
#include "WM_types.h"

#include <math.h>
#include <stdlib.h>

#if 1
#  ifdef NDEBUG
#    define NDEBUG_UNDEFD
#    undef NDEBUG
#  endif

#  include "BLI_assert.h"

#  ifdef NDEBUG_UNDEFD
#    define NDEBUG 1
#  endif
#endif

#define BOUNDARY_VERTEX_NONE -1
#define BOUNDARY_STEPS_NONE -1

#define TSTN 4

static void boundary_color_vis(SculptSession *ss, SculptBoundary *boundary);
static void SCULPT_boundary_build_smoothco(SculptSession *ss, SculptBoundary *boundary);

typedef struct BoundaryInitialVertexFloodFillData {
  PBVHVertRef initial_vertex;
  int initial_vertex_i;
  int boundary_initial_vertex_steps;
  PBVHVertRef boundary_initial_vertex;
  int boundary_initial_vertex_i;
  int *floodfill_steps;
  float radius_sq;
} BoundaryInitialVertexFloodFillData;

static bool boundary_initial_vertex_floodfill_cb(
    SculptSession *ss, PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate, void *userdata)
{
  BoundaryInitialVertexFloodFillData *data = userdata;

  int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);

  if (!SCULPT_vertex_visible_get(ss, to_v)) {
    return false;
  }

  if (!is_duplicate) {
    data->floodfill_steps[to_v_i] = data->floodfill_steps[from_v_i] + 1;
  }
  else {
    data->floodfill_steps[to_v_i] = data->floodfill_steps[from_v_i];
  }

  if (SCULPT_vertex_is_boundary(ss, to_v, SCULPT_BOUNDARY_MESH)) {
    if (data->floodfill_steps[to_v_i] < data->boundary_initial_vertex_steps) {
      data->boundary_initial_vertex_steps = data->floodfill_steps[to_v_i];
      data->boundary_initial_vertex_i = to_v_i;
      data->boundary_initial_vertex = to_v;
    }
  }

  const float len_sq = len_squared_v3v3(SCULPT_vertex_co_get(ss, data->initial_vertex),
                                        SCULPT_vertex_co_get(ss, to_v));
  return len_sq < data->radius_sq;
}

/* From a vertex index anywhere in the mesh, returns the closest vertex in a mesh boundary inside
 * the given radius, if it exists. */
static PBVHVertRef sculpt_boundary_get_closest_boundary_vertex(SculptSession *ss,
                                                               const PBVHVertRef initial_vertex,
                                                               const float radius)
{
  if (SCULPT_vertex_is_boundary(ss, initial_vertex, SCULPT_BOUNDARY_MESH)) {
    return initial_vertex;
  }

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial(&flood, initial_vertex);

  BoundaryInitialVertexFloodFillData fdata = {
      .initial_vertex = initial_vertex,
      .boundary_initial_vertex = {BOUNDARY_VERTEX_NONE},
      .boundary_initial_vertex_steps = INT_MAX,
      .radius_sq = radius * radius,
  };

  fdata.floodfill_steps = MEM_calloc_arrayN(
      SCULPT_vertex_count_get(ss), sizeof(int) * TSTN, "floodfill steps");

  SCULPT_floodfill_execute(ss, &flood, boundary_initial_vertex_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  MEM_freeN(fdata.floodfill_steps);
  return fdata.boundary_initial_vertex;
}

/* Used to allocate the memory of the boundary index arrays. This was decided considered the most
 * common use cases for the brush deformers, taking into account how many vertices those
 * deformations usually need in the boundary. */
static int BOUNDARY_INDICES_BLOCK_SIZE = 300;

static void sculpt_boundary_index_add(SculptBoundary *boundary,
                                      const PBVHVertRef new_vertex,
                                      const int new_index,
                                      const float distance,
                                      GSet *included_verts)
{

  boundary->verts[boundary->verts_num] = new_vertex;

  if (boundary->distance) {
    boundary->distance[new_index] = distance;
  }
  if (included_verts) {
    BLI_gset_add(included_verts, POINTER_FROM_INT(new_index));
  }
  boundary->verts_num++;
  if (boundary->verts_num >= boundary->verts_capacity) {
    boundary->verts_capacity += BOUNDARY_INDICES_BLOCK_SIZE;
    boundary->verts = MEM_reallocN_id(
        boundary->verts, boundary->verts_capacity * sizeof(PBVHVertRef), "boundary indices");
  }
};

static void sculpt_boundary_preview_edge_add(SculptBoundary *boundary,
                                             const PBVHVertRef v1,
                                             const PBVHVertRef v2)
{

  boundary->edges[boundary->edges_num].v1 = v1;
  boundary->edges[boundary->edges_num].v2 = v2;
  boundary->edges_num++;

  if (boundary->edges_num >= boundary->edges_capacity) {
    boundary->edges_capacity += BOUNDARY_INDICES_BLOCK_SIZE;
    boundary->edges = MEM_reallocN_id(boundary->edges,
                                      boundary->edges_capacity *
                                          sizeof(SculptBoundaryPreviewEdge) * TSTN,
                                      "boundary edges");
  }
};

/**
 * This function is used to check where the propagation should stop when calculating the boundary,
 * as well as to check if the initial vertex is valid.
 */
static bool sculpt_boundary_is_vertex_in_editable_boundary(SculptSession *ss,
                                                           const PBVHVertRef initial_vertex)
{

  if (!SCULPT_vertex_visible_get(ss, initial_vertex)) {
    return false;
  }

  int neighbor_count = 0;
  int boundary_vertex_count = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, initial_vertex, ni) {
    if (SCULPT_vertex_visible_get(ss, ni.vertex)) {
      neighbor_count++;
      if (SCULPT_vertex_is_boundary(ss, ni.vertex, SCULPT_BOUNDARY_MESH)) {
        boundary_vertex_count++;
      }
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Corners are ambiguous as it can't be decide which boundary should be active. The flood fill
   * should also stop at corners. */
  if (neighbor_count <= 2) {
    return false;
  }

  /* Non manifold geometry in the mesh boundary.
   * The deformation result will be unpredictable and not very useful. */
  if (boundary_vertex_count > 2) {
    return false;
  }

  return true;
}

/* Flood fill that adds to the boundary data all the vertices from a boundary and its duplicates.
 */

typedef struct BoundaryFloodFillData {
  SculptBoundary *boundary;
  GSet *included_verts;
  EdgeSet *preview_edges;

  PBVHVertRef last_visited_vertex;

} BoundaryFloodFillData;

static bool boundary_floodfill_cb(
    SculptSession *ss, PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate, void *userdata)
{
  int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);

  BoundaryFloodFillData *data = userdata;
  SculptBoundary *boundary = data->boundary;

  if (!SCULPT_vertex_is_boundary(ss, to_v, SCULPT_BOUNDARY_MESH)) {
    return false;
  }
  const float edge_len = len_v3v3(SCULPT_vertex_co_get(ss, from_v),
                                  SCULPT_vertex_co_get(ss, to_v));
  const float distance_boundary_to_dst = boundary->distance ?
                                             boundary->distance[from_v_i] + edge_len :
                                             0.0f;
  sculpt_boundary_index_add(
      boundary, to_v, to_v_i, distance_boundary_to_dst, data->included_verts);
  if (!is_duplicate) {
    sculpt_boundary_preview_edge_add(boundary, from_v, to_v);
  }
  return sculpt_boundary_is_vertex_in_editable_boundary(ss, to_v);
}

static float *calc_boundary_tangent(SculptSession *ss, SculptBoundary *boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  float dir[3];

  float(*tangents)[3] = MEM_calloc_arrayN(
      totvert, sizeof(float) * 3, "boundary->boundary_tangents");

  for (int i = 0; i < totvert; i++) {
    float f1 = boundary->boundary_dist[i];

    if (f1 == FLT_MAX) {
      continue;
    }

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    const float *co1 = SCULPT_vertex_co_get(ss, vertex);

    zero_v3(dir);

    SculptVertexNeighborIter ni;

    float no1[3];
    SCULPT_vertex_normal_get(ss, vertex, no1);

#if 0
    volatile int val = SCULPT_vertex_valence_get(ss, vertex);
    float *ws = BLI_array_alloca(ws, val);
    float *cot1 = BLI_array_alloca(cot1, val);
    float *cot2 = BLI_array_alloca(cot2, val);
    float *areas = BLI_array_alloca(areas, val);
    float totarea;

    SCULPT_get_cotangents(ss, vertex, ws, cot1, cot2, areas, &totarea);

    float(*cos)[3] = BLI_array_alloca(cos, val);
    float *scalars = BLI_array_alloca(scalars, val);

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      scalars[ni.i] = boundary->boundary_dist[ni.index];
      copy_v3_v3(cos[ni.i], SCULPT_vertex_co_get(ss, ni.vertex));
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    for (int j1 = 0; j1 < val; j1++) {
      int j2 = (j1 + 1) % val;

      float *co2 = cos[j1];
      float *co3 = cos[j2];
      float dir2[3];
      float dir3[3];

      float f2 = scalars[j1];
      float f3 = scalars[j2];

      if (f2 == FLT_MAX || f1 == FLT_MAX) {
        continue;
      }

      float du = f2 - f1;
      float dv = f3 - f1;

      sub_v3_v3v3(dir2, co2, co1);
      sub_v3_v3v3(dir3, co3, co1);

      mul_v3_fl(dir2, du);
      mul_v3_fl(dir3, dv);

      add_v3_v3(dir2, dir3);
      // normalize_v3(dir2);

      float w = 1.0;  // ws[j1];

      madd_v3_v3v3fl(dir, dir, dir2, w);
    }

    normalize_v3(dir);
    copy_v3_v3(tangents[i], dir);

#else

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      const float *co2 = SCULPT_vertex_co_get(ss, ni.vertex);
      float no2[3];

      SCULPT_vertex_normal_get(ss, ni.vertex, no2);

      // int i2 = BKE_pbvh_vertex_to_index(ss->pbvh, ni.vertex);
      int i2 = ni.index;

      float f2 = boundary->boundary_dist[i2];
      float dir2[3];

      sub_v3_v3v3(dir2, co2, co1);

      if (f2 == FLT_MAX) {
        continue;
      }

      float distsqr = len_squared_v3v3(co1, co2);
      if (distsqr == 0.0f) {
        continue;
      }

      float w = (f2 - f1) / distsqr;

      mul_v3_fl(dir2, w);
      add_v3_v3(dir, dir2);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    normalize_v3(dir);
    negate_v3(dir);

    copy_v3_v3(tangents[i], dir);
#endif
  }

  return (float *)tangents;
}

static void sculpt_boundary_cotan_init(SculptSession *ss, SculptBoundary *boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  boundary->boundary_cotangents = MEM_calloc_arrayN(
      totvert, sizeof(StoredCotangentW), "StoredCotangentW");
  StoredCotangentW *cotw = boundary->boundary_cotangents;

  for (int i = 0; i < totvert; i++, cotw++) {
    if (boundary->boundary_dist[i] == FLT_MAX) {
      cotw->length = 0;
      cotw->weights = NULL;
      continue;
    }

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    const int val = SCULPT_vertex_valence_get(ss, vertex);

    cotw->length = val;

    if (val < MAX_STORED_COTANGENTW_EDGES) {
      cotw->weights = cotw->static_weights;
    }
    else {
      cotw->weights = (float *)MEM_malloc_arrayN(val, sizeof(*cotw->weights), "cotw->weights");
    }

    SCULPT_get_cotangents(ss, vertex, cotw->weights, NULL, NULL, NULL, NULL);
  }
}

static void sculpt_boundary_indices_init(Object *ob,
                                         SculptSession *ss,
                                         SculptBoundary *boundary,
                                         const bool init_boundary_distances,
                                         const PBVHVertRef initial_boundary_vertex,
                                         float radius)
{

  const int totvert = SCULPT_vertex_count_get(ss);
  boundary->verts = MEM_malloc_arrayN(
      BOUNDARY_INDICES_BLOCK_SIZE, sizeof(PBVHVertRef), "boundary indices");

  if (init_boundary_distances) {
    boundary->distance = MEM_calloc_arrayN(totvert, sizeof(float) * TSTN, "boundary distances");
  }
  boundary->edges = MEM_malloc_arrayN(
      BOUNDARY_INDICES_BLOCK_SIZE, sizeof(SculptBoundaryPreviewEdge) * TSTN, "boundary edges");

  GSet *included_verts = BLI_gset_int_new_ex("included verts", BOUNDARY_INDICES_BLOCK_SIZE);
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);

  int initial_boundary_index = BKE_pbvh_vertex_to_index(ss->pbvh, initial_boundary_vertex);

  boundary->initial_vertex = initial_boundary_vertex;
  boundary->initial_vertex_i = initial_boundary_index;

  copy_v3_v3(boundary->initial_vertex_position,
             SCULPT_vertex_co_get(ss, boundary->initial_vertex));
  sculpt_boundary_index_add(
      boundary, initial_boundary_vertex, initial_boundary_index, 0.0f, included_verts);
  SCULPT_floodfill_add_initial(&flood, boundary->initial_vertex);

  BoundaryFloodFillData fdata = {
      .boundary = boundary,
      .included_verts = included_verts,
      .last_visited_vertex = {BOUNDARY_VERTEX_NONE},

  };

  SCULPT_floodfill_execute(ss, &flood, boundary_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  GSet *boundary_verts;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    boundary_verts = BLI_gset_int_new_ex("included vertices", BOUNDARY_INDICES_BLOCK_SIZE);

    GSetIterator gi;

    GSET_ITER (gi, included_verts) {
      BMVert *v = (BMVert *)BLI_gsetIterator_getKey(&gi);
      BLI_gset_add(boundary_verts, POINTER_FROM_INT(v->head.index));
    }
  }
  else {
    boundary_verts = included_verts;
  }

  boundary->boundary_closest = MEM_calloc_arrayN(totvert, sizeof(PBVHVertRef), "boundary_closest");
  boundary->boundary_dist = SCULPT_geodesic_distances_create(
      ob, boundary_verts, radius, boundary->boundary_closest, NULL);

  sculpt_boundary_cotan_init(ss, boundary);

#if 0  // smooth geodesic scalar field
  float *boundary_dist = MEM_calloc_arrayN(totvert, sizeof(float), "boundary_dist");

  for (int iteration = 0; iteration < 4; iteration++) {
    for (int i = 0; i < totvert; i++) {
      if (boundary->boundary_dist[i] == FLT_MAX) {
        boundary_dist[i] = FLT_MAX;
        continue;
      }

      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
      float tot = 0.0f;

      StoredCotangentW *cotw = boundary->boundary_cotangents + i;

      SculptVertexNeighborIter ni;
      int j = 0;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        if (boundary->boundary_dist[ni.index] == FLT_MAX) {
          j++;
          continue;
        }

        const float w = cotw->weights[j];

        boundary_dist[i] += boundary->boundary_dist[ni.index] * w;

        tot += w;
        j++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (tot == 0.0f) {
        boundary_dist[i] = FLT_MAX;
      }
      else {
        boundary_dist[i] /= tot;
      }
    }

    SWAP(float *, boundary_dist, boundary->boundary_dist);
  }

  MEM_SAFE_FREE(boundary_dist);
#endif

  boundary->boundary_tangents = (float(*)[3])calc_boundary_tangent(ss, boundary);

#if 1  // smooth geodesic tangent field
  float(*boundary_tangents)[3] = MEM_calloc_arrayN(
      totvert, sizeof(float) * 3, "boundary_tangents");

  for (int iteration = 0; iteration < 4; iteration++) {
    for (int i = 0; i < totvert; i++) {

      if (boundary->boundary_dist[i] == FLT_MAX) {
        copy_v3_v3(boundary_tangents[i], boundary->boundary_tangents[i]);
        continue;
      }

      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
      float tot = 0.0f;

      // StoredCotangentW *cotw = boundary->boundary_cotangents + i;
      float tan[3] = {0.0f, 0.0f, 0.0f};

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        if (boundary->boundary_dist[ni.index] == FLT_MAX) {
          continue;
        }

        add_v3_v3(tan, boundary->boundary_tangents[ni.index]);

        tot += 1.0f;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (tot == 0.0f) {
        continue;
      }

      normalize_v3(tan);
      interp_v3_v3v3(boundary_tangents[i], boundary->boundary_tangents[i], tan, 0.75f);
      normalize_v3(boundary_tangents[i]);
    }

    float(*tmp)[3] = boundary_tangents;
    boundary_tangents = boundary->boundary_tangents;
    boundary->boundary_tangents = tmp;
  }

  MEM_SAFE_FREE(boundary_tangents);
#endif

  boundary_color_vis(ss, boundary);

  if (boundary_verts != included_verts) {
    BLI_gset_free(boundary_verts, NULL);
  }

  /* Check if the boundary loops into itself and add the extra preview edge to close the loop. */
  if (fdata.last_visited_vertex.i != BOUNDARY_VERTEX_NONE &&
      sculpt_boundary_is_vertex_in_editable_boundary(ss, fdata.last_visited_vertex)) {
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, fdata.last_visited_vertex, ni) {
      if (BLI_gset_haskey(included_verts, POINTER_FROM_INT(ni.index)) &&
          sculpt_boundary_is_vertex_in_editable_boundary(ss, ni.vertex)) {
        sculpt_boundary_preview_edge_add(boundary, fdata.last_visited_vertex, ni.vertex);
        boundary->forms_loop = true;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_gset_free(included_verts, NULL);
}

static void boundary_color_vis(SculptSession *ss, SculptBoundary *boundary)
{
  if (boundary->boundary_dist && G.debug_value == 890 && ss->bm &&
      CustomData_has_layer(&ss->bm->vdata, CD_PROP_COLOR)) {
    const int cd_color = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);

    BMIter iter;
    BMVert *v;
    int i = 0;

    float min = 1e17f, max = -1e17f;

    // calc bounds
    BM_ITER_MESH_INDEX (v, &iter, ss->bm, BM_VERTS_OF_MESH, i) {
      float f = boundary->boundary_dist[i];

      if (f == FLT_MAX) {
        continue;
      }

      min = MIN2(min, f);
      max = MAX2(max, f);
    }

    float scale = max != min ? 1.0f / (max - min) : 0.0f;

    BM_ITER_MESH_INDEX (v, &iter, ss->bm, BM_VERTS_OF_MESH, i) {
      MPropCol *mcol = BM_ELEM_CD_GET_VOID_P(v, cd_color);

      float f = boundary->boundary_dist[i];

      if (f == FLT_MAX) {
        mcol->color[0] = mcol->color[1] = 1.0f;
        mcol->color[2] = 0.0f;
        mcol->color[3] = 1.0f;
        continue;
      }
      else {
        f = (f - min) * scale;
      }

      mcol->color[0] = mcol->color[1] = mcol->color[2] = f;
      mcol->color[3] = 1.0f;
    }
  }
}

/**
 * This functions initializes all data needed to calculate falloffs and deformation from the
 * boundary into the mesh into a #SculptBoundaryEditInfo array. This includes how many steps are
 * needed to go from a boundary vertex to an interior vertex and which vertex of the boundary is
 * the closest one.
 */
static void sculpt_boundary_edit_data_init(SculptSession *ss,
                                           SculptBoundary *boundary,
                                           const PBVHVertRef initial_vertex,
                                           const float radius)
{
  const int totvert = SCULPT_vertex_count_get(ss);

  const bool has_duplicates = BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS;

  boundary->edit_info = MEM_malloc_arrayN(
      totvert, sizeof(SculptBoundaryEditInfo) * TSTN, "Boundary edit info");

  for (int i = 0; i < totvert; i++) {
    boundary->edit_info[i].original_vertex_i = BOUNDARY_VERTEX_NONE;
    boundary->edit_info[i].propagation_steps_num = BOUNDARY_STEPS_NONE;
  }

  GSQueue *current_iteration = BLI_gsqueue_new(sizeof(PBVHVertRef));
  GSQueue *next_iteration = BLI_gsqueue_new(sizeof(PBVHVertRef));

  /* Initialized the first iteration with the vertices already in the boundary. This is propagation
   * step 0. */
  BLI_bitmap *visited_verts = BLI_BITMAP_NEW(SCULPT_vertex_count_get(ss), "visited_verts");
  for (int i = 0; i < boundary->verts_num; i++) {
    int index = BKE_pbvh_vertex_to_index(ss->pbvh, boundary->verts[i]);

    boundary->edit_info[index].original_vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh,
                                                                            boundary->verts[i]);
    boundary->edit_info[index].propagation_steps_num = 0;

    /* This ensures that all duplicate vertices in the boundary have the same original_vertex
     * index, so the deformation for them will be the same. */
    if (has_duplicates) {
      SculptVertexNeighborIter ni_duplis;
      SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, boundary->verts[i], ni_duplis) {
        if (ni_duplis.is_duplicate) {
          boundary->edit_info[ni_duplis.index].original_vertex_i = BKE_pbvh_vertex_to_index(
              ss->pbvh, boundary->verts[i]);
        }
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni_duplis);
    }

    BLI_gsqueue_push(current_iteration, &boundary->verts[i]);
  }

  int propagation_steps_num = 0;
  float accum_distance = 0.0f;

  while (true) {
    /* Stop adding steps to edit info. This happens when a steps is further away from the boundary
     * than the brush radius or when the entire mesh was already processed. */
    if (accum_distance > radius || BLI_gsqueue_is_empty(current_iteration)) {
      boundary->max_propagation_steps = propagation_steps_num;
      break;
    }

    while (!BLI_gsqueue_is_empty(current_iteration)) {
      PBVHVertRef from_v;
      BLI_gsqueue_pop(current_iteration, &from_v);
      const int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
        const bool is_visible = SCULPT_vertex_visible_get(ss, ni.vertex);

        if (!is_visible ||
            boundary->edit_info[ni.index].propagation_steps_num != BOUNDARY_STEPS_NONE) {
          continue;
        }

        boundary->edit_info[ni.index].original_vertex_i =
            boundary->edit_info[from_v_i].original_vertex_i;

        BLI_BITMAP_ENABLE(visited_verts, ni.index);

        if (ni.is_duplicate) {
          /* Grids duplicates handling. */
          boundary->edit_info[ni.index].propagation_steps_num =
              boundary->edit_info[from_v_i].propagation_steps_num;
        }
        else {
          boundary->edit_info[ni.index].propagation_steps_num =
              boundary->edit_info[from_v_i].propagation_steps_num + 1;

          BLI_gsqueue_push(next_iteration, &ni.vertex);

          /* When copying the data to the neighbor for the next iteration, it has to be copied to
           * all its duplicates too. This is because it is not possible to know if the updated
           * neighbor or one if its uninitialized duplicates is going to come first in order to
           * copy the data in the from_v neighbor iterator. */
          if (has_duplicates) {
            SculptVertexNeighborIter ni_duplis;
            SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, ni.vertex, ni_duplis) {
              if (ni_duplis.is_duplicate) {
                boundary->edit_info[ni_duplis.index].original_vertex_i =
                    boundary->edit_info[from_v_i].original_vertex_i;
                boundary->edit_info[ni_duplis.index].propagation_steps_num =
                    boundary->edit_info[from_v_i].propagation_steps_num + 1;
              }
            }
            SCULPT_VERTEX_NEIGHBORS_ITER_END(ni_duplis);
          }

          /* Check the distance using the vertex that was propagated from the initial vertex that
           * was used to initialize the boundary. */
          if (boundary->edit_info[from_v_i].original_vertex_i ==
              BKE_pbvh_vertex_to_index(ss->pbvh, initial_vertex)) {
            boundary->pivot_vertex = ni.vertex;
            copy_v3_v3(boundary->initial_pivot_position, SCULPT_vertex_co_get(ss, ni.vertex));
            accum_distance += len_v3v3(SCULPT_vertex_co_get(ss, from_v),
                                       SCULPT_vertex_co_get(ss, ni.vertex));
          }
        }
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
    }

    /* Copy the new vertices to the queue to be processed in the next iteration. */
    while (!BLI_gsqueue_is_empty(next_iteration)) {
      PBVHVertRef next_v;
      BLI_gsqueue_pop(next_iteration, &next_v);
      BLI_gsqueue_push(current_iteration, &next_v);
    }

    propagation_steps_num++;
  }

  MEM_SAFE_FREE(visited_verts);

  BLI_gsqueue_free(current_iteration);
  BLI_gsqueue_free(next_iteration);
}

/* This functions assigns a falloff factor to each one of the SculptBoundaryEditInfo structs based
 * on the brush curve and its propagation steps. The falloff goes from the boundary into the mesh.
 */
static void sculpt_boundary_falloff_factor_init(
    SculptSession *ss, Sculpt *sd, SculptBoundary *boundary, Brush *brush, const float radius)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  BKE_curvemapping_init(brush->curve);

  int boundary_type = SCULPT_get_int(ss, boundary_falloff_type, sd, brush);

  for (int i = 0; i < totvert; i++) {
    if (boundary->edit_info[i].propagation_steps_num != -1) {
      boundary->edit_info[i].strength_factor = BKE_brush_curve_strength(
          brush, boundary->edit_info[i].propagation_steps_num, boundary->max_propagation_steps);
    }

    if (boundary->edit_info[i].original_vertex_i ==
        BKE_pbvh_vertex_to_index(ss->pbvh, boundary->initial_vertex)) {
      /* All vertices that are propagated from the original vertex won't be affected by the
       * boundary falloff, so there is no need to calculate anything else. */
      continue;
    }

    if (!boundary->distance) {
      /* There are falloff modes that do not require to modify the previously calculated falloff
       * based on boundary distances. */
      continue;
    }

    const float boundary_distance = boundary->distance[boundary->edit_info[i].original_vertex_i];
    float falloff_distance = 0.0f;
    float direction = 1.0f;

    switch (boundary_type) {
      case BRUSH_BOUNDARY_FALLOFF_RADIUS:
        falloff_distance = boundary_distance;
        break;
      case BRUSH_BOUNDARY_FALLOFF_LOOP: {
        const int div = (int)(boundary_distance / radius);
        const float mod = fmodf(boundary_distance, radius);
        falloff_distance = div % 2 == 0 ? mod : radius - mod;
      } break;
      case BRUSH_BOUNDARY_FALLOFF_LOOP_INVERT: {
        const int div = (int)(boundary_distance / radius);
        const float mod = fmodf(boundary_distance, radius);
        falloff_distance = div % 2 == 0 ? mod : radius - mod;
        /* Inverts the faloff in the intervals 1 2 5 6 9 10 ... */
        if (((div - 1) & 2) == 0) {
          direction = -1.0f;
        }
      } break;
      case BRUSH_BOUNDARY_FALLOFF_CONSTANT:
        /* For constant falloff distances are not allocated, so this should never happen. */
        BLI_assert(false);
    }

    boundary->edit_info[i].strength_factor *= direction * BKE_brush_curve_strength(
                                                              brush, falloff_distance, radius);
  }
}

/* Main function to get SculptBoundary data both for brush deformation and viewport preview. Can
 * return NULL if there is no boundary from the given vertex using the given radius. */
SculptBoundary *SCULPT_boundary_data_init(
    Sculpt *sd, Object *object, Brush *brush, const PBVHVertRef initial_vertex, const float radius)
{
  if (initial_vertex.i == PBVH_REF_NONE) {
    return NULL;
  }

  SculptSession *ss = object->sculpt;

  // XXX force update of BMVert->head.index
  if (ss->bm) {
    ss->bm->elem_index_dirty |= BM_VERT;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(object);

  const PBVHVertRef boundary_initial_vertex = sculpt_boundary_get_closest_boundary_vertex(
      ss, initial_vertex, radius);

  if (boundary_initial_vertex.i == BOUNDARY_VERTEX_NONE) {
    return NULL;
  }

  /* Starting from a vertex that is the limit of a boundary is ambiguous, so return NULL instead of
   * forcing a random active boundary from a corner. */
  if (!sculpt_boundary_is_vertex_in_editable_boundary(ss, initial_vertex)) {
    return NULL;
  }

  SculptBoundary *boundary = MEM_callocN(sizeof(SculptBoundary) * TSTN, "Boundary edit data");

  boundary->deform_target = SCULPT_get_int(ss, deform_target, sd, brush);

  const bool init_boundary_distances = brush ?
                                           SCULPT_get_int(ss, boundary_falloff_type, sd, brush) !=
                                               BRUSH_BOUNDARY_FALLOFF_CONSTANT :
                                           false;

  const float boundary_radius = brush ?
                                    radius *
                                        (1.0f + SCULPT_get_float(ss, boundary_offset, sd, brush)) :
                                    radius;

  sculpt_boundary_indices_init(
      object, ss, boundary, init_boundary_distances, boundary_initial_vertex, boundary_radius);

  sculpt_boundary_edit_data_init(ss, boundary, boundary_initial_vertex, boundary_radius);

  if (ss->cache) {
    SCULPT_boundary_build_smoothco(ss, boundary);
  }

  return boundary;
}

void SCULPT_boundary_data_free(SculptBoundary *boundary)
{
  MEM_SAFE_FREE(boundary->verts);
  MEM_SAFE_FREE(boundary->edges);
  MEM_SAFE_FREE(boundary->distance);

  MEM_SAFE_FREE(boundary->boundary_dist);
  MEM_SAFE_FREE(boundary->boundary_tangents);
  MEM_SAFE_FREE(boundary->boundary_closest);
  MEM_SAFE_FREE(boundary->smoothco);

  MEM_SAFE_FREE(boundary->edit_info);
  MEM_SAFE_FREE(boundary->bend.pivot_positions);
  MEM_SAFE_FREE(boundary->bend.pivot_rotation_axis);
  MEM_SAFE_FREE(boundary->slide.directions);
  MEM_SAFE_FREE(boundary->circle.origin);
  MEM_SAFE_FREE(boundary->circle.radius);

  StoredCotangentW *cotw = boundary->boundary_cotangents;

  if (cotw) {
    for (int i = 0; i < boundary->sculpt_totvert; i++, cotw++) {
      if (cotw->weights != cotw->static_weights) {
        MEM_SAFE_FREE(cotw->weights);
      }
    }
  }

  MEM_SAFE_FREE(boundary->boundary_cotangents);
  MEM_SAFE_FREE(boundary);
}

typedef struct ScalarFieldWalkData {
  PBVHVertRef v;
  float co[3];

  struct {
    PBVHVertRef v1, v2;
  } edge;

  float t, f;
  bool has_edge;
} ScalarFieldWalkData;

static void sculpt_walk_scalar_field_init(SculptSession *ss,
                                          PBVHVertRef v,
                                          ScalarFieldWalkData *wd,
                                          float *field)
{
  wd->v = v;
  copy_v3_v3(wd->co, SCULPT_vertex_co_get(ss, v));
  wd->has_edge = false;
  wd->t = 0.0f;

  int i = BKE_pbvh_vertex_to_index(ss->pbvh, v);
  wd->f = field[i];
}

/*walk in decreasing direction of scalar field*/
static bool sculpt_walk_scalar_field(SculptSession *ss,
                                     ScalarFieldWalkData *wd,
                                     float *field,
                                     float (*dfield)[3])
{
  SculptVertexNeighborIter ni;
  SculptVertexNeighborIter ni2;
  PBVHVertRef v = wd->v, minv1 = {-1LL}, minv2 = {-1LL};
  float mindis1 = FLT_MAX, mindis2 = FLT_MAX;
  float minf1 = 0.0, minf2 = 0.0;

  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v, ni) {
    const float *co2 = SCULPT_vertex_co_get(ss, ni.vertex);
    float f2 = field[ni.index];

    if (ni.vertex.i == v.i) {
      continue;
    }

    if (f2 > wd->f) {
      continue;
    }

    float len = len_v3v3(co2, wd->co);
    float dist = f2 * len;

    if (dist >= mindis1) {
      continue;
    }

    mindis1 = dist;
    minf1 = f2;
    minv1 = ni.vertex;

    mindis2 = FLT_MAX;

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ni.vertex, ni2) {
      if (ni2.vertex.i == ni.vertex.i) {
        continue;
      }

      const float *co3 = SCULPT_vertex_co_get(ss, ni2.vertex);
      float f3 = field[ni2.index];

      float len2 = len_v3v3(co3, wd->co);
      float dist2 = f3 * len;  // wd->f + (f2 - wd->f) * len;

      if (dist2 < mindis2) {
        mindis2 = dist2;
        minf2 = f3;
        minv2 = ni2.vertex;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni2);
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (minv1.i == -1LL) {
    // didn't find anything
    return false;
  }

  if (minv2.i == -1LL) {
    wd->v = minv1;
    copy_v3_v3(wd->co, SCULPT_vertex_co_get(ss, minv1));
    wd->has_edge = false;
    wd->f = minf1;

    return true;
  }

  wd->has_edge = true;
  wd->edge.v1 = minv1;
  wd->edge.v2 = minv2;

  /*
  on factor
  load_package "avector";

  comment: relative to wd.co;
  a := avec(ax, ay, az);
  b := avec(bx, by, bz);

  dva := avec(dvax, dvay, dvaz);
  dvb := avec(dvbx, dvby, dvbz);

  la := a dot a;
  lb := b dot b;

  f2  := a + (b - a) * t;
  df2 := dva + (dvb - dva)*t;

  ll := f2 dot f2;
  f1 := (minf1 + (minf2 - minf1)*t) * ll;

  ff := solve(df(f1, t, 2), t);
  f  := part(ff, 1, 2);


  */

  const float *a = SCULPT_vertex_co_get(ss, minv1);
  const float *b = SCULPT_vertex_co_get(ss, minv2);

  float ax = a[0] - wd->co[0];
  float ay = a[1] - wd->co[1];
  float az = a[2] - wd->co[2];

  float bx = b[0] - wd->co[0];
  float by = b[1] - wd->co[1];
  float bz = b[2] - wd->co[2];

  float div = (by * by + bz * bz + bx * bx + (az - 2.0 * bz) * az + (ay - 2.0 * by) * ay +
               (ax - 2.0 * bx) * ax);

  float t = ((ay - by) * ay + (az - bz) * az + (ax - bx) * ax) / div;

  float m1m2 = minf1 + minf2;

  float ans4 = -2.0 * (by * by + bz * bz + bx * bx) * m1m2 * az * bz * minf1 -
               (2.0 * (minf1 + minf2) * bz - az * minf2) * az * az * az * minf2;

  float sqr2 = (by * by + bz * bz + bx * bx);
  sqr2 = sqr2 * sqr2;

  float ans3 =
      (2.0 *
           ((4.0 * (minf1 * minf1 - minf1 * minf2 + minf2 * minf2) * by -
             (minf1 + minf2) * ay * minf2) *
                ay +
            (4.0 * (minf1 * minf1 - minf1 * minf2 + minf2 * minf2) * bz -
             (minf1 + minf2) * az * minf2) *
                az -
            (by * by + bz * bz + bx * bx) * m1m2 * minf1) *
           bx -
       (2.0 * m1m2 * bx - ax * minf2) * ax * ax * minf2 -
       ((by * by + bz * bz) * (3.0 * minf1 * minf1 - 8.0 * minf1 * minf2 + 3.0 * minf2 * minf2) -
        (minf1 * minf1 + 4.0 * minf1 * minf2 + minf2 * minf2) * bx * bx +
        2.0 * (m1m2 * bz - az * minf2) * az * minf2 +
        2.0 * (m1m2 * by - ay * minf2) * ay * minf2) *
           ax) *
          ax -
      (2.0 *
           ((by * by + bz * bz + bx * bx) * m1m2 * minf1 -
            (4.0 * (minf1 * minf1 - minf1 * minf2 + minf2 * minf2) * bz - m1m2 * az * minf2) *
                az) *
           by +
       (2.0 * (minf1 + minf2) * by - ay * minf2) * ay * ay * minf2 +
       ((bx * bx + bz * bz) * (3.0 * minf1 * minf1 - 8.0 * minf1 * minf2 + 3.0 * minf2 * minf2) -
        (minf1 * minf1 + 4.0 * minf1 * minf2 + minf2 * minf2) * by * by +
        2.0 * (m1m2 * bz - az * minf2) * az * minf2) *
           ay) *
          ay -
      ((bx * bx + by * by) * (3.0 * minf1 * minf1 - 8.0 * minf1 * minf2 + 3.0 * minf2 * minf2) -
       (minf1 * minf1 + 4.0 * minf1 * minf2 + minf2 * minf2) * bz * bz) *
          az * az +
      sqr2 * minf1 * minf1 + ans4;

  float ans2 = sqrtf(ans3);

  float ans1 = (by * by + bz * bz + bx * bx) * minf1 +
               ((3.0 * minf1 - 2.0 * minf2) * az - 2.0 * (2.0 * minf1 - minf2) * bz) * az +
               ((3.0 * minf1 - 2.0 * minf2) * ay - 2.0 * (2.0 * minf1 - minf2) * by) * ay +
               ((3.0 * minf1 - 2.0 * minf2) * ax - 2.0 * (2.0 * minf1 - minf2) * bx) * ax + ans2;

  t = ans1 / (3.0 *
              (by * by + bz * bz + bx * bx + (az - 2.0 * bz) * az + (ay - 2.0 * by) * ay +
               (ax - 2.0 * bx) * ax) *
              (minf1 - minf2));

#if 1
  t = (((3.0 * minf1 - 2.0 * minf2) * ay - 2.0 * (2.0 * minf1 - minf2) * by) * ay +
       ((3.0 * minf1 - 2.0 * minf2) * az - 2.0 * (2.0 * minf1 - minf2) * bz) * az +
       ((3.0 * minf1 - 2.0 * minf2) * ax - 2.0 * (2.0 * minf1 - minf2) * bx) * ax +
       (by * by + bz * bz + bx * bx) * minf1) /
      (3.0 *
       (by * by + bz * bz + bx * bx + (az - 2.0 * bz) * az + (ay - 2.0 * by) * ay +
        (ax - 2.0 * bx) * ax) *
       (minf1 - minf2));
#endif

  t = t < 0.0f ? 0.0f : t;
  t = t > 1.0f ? 1.0f : t;

  t = 0.5f;
  wd->t = t;

  wd->v = minv1;
  wd->f = minf1 + (minf2 - minf1) * wd->t;
  float co[3];

  interp_v3_v3v3(co, SCULPT_vertex_co_get(ss, minv1), SCULPT_vertex_co_get(ss, minv2), wd->t);

  float f3 = wd->f * len_v3v3(wd->co, co);
  if (f3 > mindis1 || f3 > mindis2) {
    wd->f = minf1;
    t = 0.0f;
    interp_v3_v3v3(co, SCULPT_vertex_co_get(ss, minv1), SCULPT_vertex_co_get(ss, minv2), wd->t);
  }

  copy_v3_v3(wd->co, co);

  return true;
}

Object *sculpt_get_vis_object(bContext *C, SculptSession *ss, char *name)
{
  if (!C) {
    C = ss->cache->C;
  }

  Scene *scene = CTX_data_scene(C);
  ViewLayer *vlayer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Object *actob = CTX_data_active_object(C);

  Object *ob = (Object *)BKE_libblock_find_name(bmain, ID_OB, name);

  if (!ob) {
    Mesh *me = BKE_mesh_add(bmain, name);

    ob = BKE_object_add_only_object(bmain, OB_MESH, name);
    ob->data = (void *)me;
    id_us_plus((ID *)me);

    DEG_id_tag_update_ex(
        bmain, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

    LayerCollection *layer_collection = BKE_layer_collection_get_active(vlayer);
    BKE_collection_object_add(bmain, layer_collection->collection, ob);
  }

  copy_v3_v3(ob->loc, actob->loc);
  copy_v3_v3(ob->rot, actob->rot);
  BKE_object_to_mat4(ob, ob->object_to_world);

  DEG_id_type_tag(bmain, ID_OB);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
  DEG_id_tag_update(&scene->id, 0);

  Mesh *me = (Mesh *)ob->data;

  DEG_id_tag_update(&me->id, ID_RECALC_ALL);
  return ob;
}

void sculpt_end_vis_object(bContext *C, SculptSession *ss, Object *ob, BMesh *bm)
{
  if (!C) {
    C = ss->cache->C;
  }

  Main *bmain = CTX_data_main(C);

  Mesh *me = (Mesh *)ob->data;

  BM_mesh_bm_to_me(bmain,
                   NULL,
                   bm,
                   me,
                   (&(struct BMeshToMeshParams){.calc_object_remap = false,
                                                .update_shapekey_indices = false,
                                                .copy_temp_cdlayers = false}));

  DEG_id_tag_update(&me->id, ID_RECALC_ALL);
}

//#define VISBM

/* These functions initialize the required vectors for the desired deformation using the
 * SculptBoundaryEditInfo. They calculate the data using the vertices that have the
 * max_propagation_steps value and them this data is copied to the rest of the vertices using the
 * original vertex index. */
static void sculpt_boundary_bend_data_init(SculptSession *ss,
                                           SculptBoundary *boundary,
                                           float radius)
{
#ifdef VISBM
  Object *visob = get_vis_object(ss, "_vis_sculpt_boundary_bend_data_init");
  BMAllocTemplate alloc = {512, 512, 512, 512};
  BMesh *visbm = BM_mesh_create(&alloc,
                                (&(struct BMeshCreateParams){.use_unique_ids = 0,
                                                             .use_id_elem_mask = 0,
                                                             .use_id_map = 0,
                                                             .use_toolflags = 0,
                                                             .no_reuse_ids = 0}));
#endif

  const int totvert = SCULPT_vertex_count_get(ss);
  boundary->bend.pivot_rotation_axis = MEM_calloc_arrayN(
      totvert, 3 * sizeof(float), "pivot rotation axis");
  boundary->bend.pivot_positions = MEM_calloc_arrayN(
      totvert, 4 * sizeof(float), "pivot positions");

  for (int i = 0; i < totvert; i++) {

    if (boundary->edit_info[i].propagation_steps_num != boundary->max_propagation_steps) {
      continue;
    }
  }

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (boundary->edit_info[i].original_vertex_i == BOUNDARY_VERTEX_NONE) {
      continue;
    }

    if (boundary->edit_info[i].propagation_steps_num != boundary->max_propagation_steps) {
      continue;
    }

    if (boundary->edit_info[i].original_vertex.i == -1) {
      continue;
    }

    const float *co1 = SCULPT_vertex_co_get(ss, vertex);

    float dir[3];
    float normal[3];
    SCULPT_vertex_normal_get(ss, vertex, normal);
    sub_v3_v3v3(dir, SCULPT_vertex_co_get(ss, boundary->edit_info[i].original_vertex), co1);

    normalize_v3(dir);

    float olddir[3];
    copy_v3_v3(olddir, dir);

    if (boundary->boundary_dist[i] != FLT_MAX) {
      zero_v3(dir);
      copy_v3_v3(dir, boundary->boundary_tangents[i]);

      if (dot_v3v3(dir, dir) < 0.00001f) {
        sub_v3_v3v3(dir,
                    SCULPT_vertex_co_get(ss, boundary->edit_info[i].original_vertex),
                    SCULPT_vertex_co_get(ss, vertex));
      }
    }
    else {
      // continue;
    }

    cross_v3_v3v3(
        boundary->bend.pivot_rotation_axis[boundary->edit_info[i].original_vertex_i], dir, normal);
    normalize_v3(boundary->bend.pivot_rotation_axis[boundary->edit_info[i].original_vertex_i]);

    float pos[3];

    copy_v3_v3(pos, co1);

    copy_v3_v3(boundary->bend.pivot_positions[boundary->edit_info[i].original_vertex_i], pos);
    boundary->bend.pivot_positions[boundary->edit_info[i].original_vertex_i][3] = 1.0f;
  }

  for (int i = 0; i < totvert; i++) {
    if (boundary->bend.pivot_positions[i][3] > 1.0f) {
      mul_v3_fl(boundary->bend.pivot_positions[i], 1.0f / boundary->bend.pivot_positions[i][3]);
      boundary->bend.pivot_positions[i][3] = 1.0f;
    }
  }

  // fix any remaining boundaries without pivots
  for (int vi = 0; vi < boundary->verts_num; vi++) {
    PBVHVertRef v = boundary->verts[vi];
    const float *co1 = SCULPT_vertex_co_get(ss, v);
    int i = BKE_pbvh_vertex_to_index(ss->pbvh, v);

    if (boundary->bend.pivot_positions[i][3] != 0.0f) {
      continue;
    }

    float minlen = FLT_MAX;

    // nasty inner loop here
    for (int j = 0; j < totvert; j++) {
      if (boundary->edit_info[j].propagation_steps_num != boundary->max_propagation_steps) {
        continue;
      }

      PBVHVertRef v2 = BKE_pbvh_index_to_vertex(ss->pbvh, j);
      const float *co2 = SCULPT_vertex_co_get(ss, v2);

      float len = len_v3v3(co2, co1);

      if (len < minlen) {
        minlen = len;
        copy_v3_v3(boundary->bend.pivot_positions[i], co2);
        boundary->bend.pivot_positions[i][3] = 1.0f;
      }
    }
  }

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    const float *co1 = SCULPT_vertex_co_get(ss, vertex);
    float dir[3];

    if (boundary->edit_info[i].propagation_steps_num == BOUNDARY_STEPS_NONE) {
      continue;
    }
    float pos[3], oco[3];
    copy_v3_v3(pos, boundary->bend.pivot_positions[boundary->edit_info[i].original_vertex_i]);
    copy_v3_v3(oco, SCULPT_vertex_co_get(ss, boundary->edit_info[i].original_vertex));

    if (boundary->boundary_dist[i] != FLT_MAX) {
      float no[3];

      SCULPT_vertex_normal_get(ss, vertex, no);

      // snap to radial plane
      cross_v3_v3v3(dir, no, boundary->boundary_tangents[i]);
      normalize_v3(dir);
      //*

      sub_v3_v3(pos, oco);
      normalize_v3(pos);
      mul_v3_fl(pos, radius);
      add_v3_v3(pos, oco);

      sub_v3_v3(pos, co1);
      madd_v3_v3fl(pos, dir, -dot_v3v3(dir, pos));
      add_v3_v3(pos, co1);

      //*/

      copy_v3_v3(boundary->bend.pivot_rotation_axis[i], dir);
    }
    else {
      zero_v3(dir);

      // printf("boundary info missing tangent\n");
      copy_v3_v3(boundary->bend.pivot_rotation_axis[i],
                 boundary->bend.pivot_rotation_axis[boundary->edit_info[i].original_vertex_i]);
    }

    copy_v3_v3(boundary->bend.pivot_positions[i], pos);

#ifdef VISBM
    {
      BMVert *v1, *v2;
      v1 = BM_vert_create(visbm, co1, NULL, BM_CREATE_NOP);

      v2 = BM_vert_create(visbm, pos, NULL, BM_CREATE_NOP);
      BM_edge_create(visbm, v1, v2, NULL, BM_CREATE_NOP);

      float tmp[3];
      madd_v3_v3v3fl(tmp, co1, dir, 0.35);

      v2 = BM_vert_create(visbm, tmp, NULL, BM_CREATE_NOP);
      BM_edge_create(visbm, v1, v2, NULL, BM_CREATE_NOP);
    }
#endif
  }

#ifdef VISBM
  end_vis_object(ss, visob, visbm);
#endif
}

static void sculpt_boundary_slide_data_init(SculptSession *ss, SculptBoundary *boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  boundary->slide.directions = MEM_calloc_arrayN(
      totvert, 3 * sizeof(float) * TSTN, "slide directions");

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (boundary->edit_info[i].propagation_steps_num != boundary->max_propagation_steps) {
      continue;
    }

    sub_v3_v3v3(boundary->slide.directions[boundary->edit_info[i].original_vertex_i],
                SCULPT_vertex_co_get(ss, boundary->edit_info[i].original_vertex),
                SCULPT_vertex_co_get(ss, vertex));

    normalize_v3(boundary->slide.directions[boundary->edit_info[i].original_vertex_i]);
  }

  for (int i = 0; i < totvert; i++) {
    if (boundary->edit_info[i].propagation_steps_num == BOUNDARY_STEPS_NONE) {
      continue;
    }
    copy_v3_v3(boundary->slide.directions[i],
               boundary->slide.directions[boundary->edit_info[i].original_vertex_i]);
  }
}

static void do_boundary_brush_circle_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symmetry_pass = ss->cache->mirror_symmetry_pass;
  const SculptBoundary *boundary = ss->cache->boundaries[symmetry_pass];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    const int propagation_steps = boundary->edit_info[vd.index].propagation_steps_num;
    float *circle_origin = boundary->circle.origin[propagation_steps];
    float circle_disp[3];
    sub_v3_v3v3(circle_disp, circle_origin, orig_data.co);
    normalize_v3(circle_disp);
    mul_v3_fl(circle_disp, -boundary->circle.radius[propagation_steps]);
    float target_circle_co[3];
    add_v3_v3v3(target_circle_co, circle_origin, circle_disp);

    float disp[3];
    sub_v3_v3v3(disp, target_circle_co, vd.co);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    madd_v3_v3v3fl(target_co,
                   vd.co,
                   disp,
                   boundary->edit_info[vd.index].strength_factor * mask * automask * strength);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_boundary_twist_data_init(SculptSession *ss, SculptBoundary *boundary)
{
  zero_v3(boundary->twist.pivot_position);
  float(*poly_verts)[3] = MEM_malloc_arrayN(boundary->verts_num, sizeof(float[3]), "poly verts");
  for (int i = 0; i < boundary->verts_num; i++) {
    add_v3_v3(boundary->twist.pivot_position, SCULPT_vertex_co_get(ss, boundary->verts[i]));
    copy_v3_v3(poly_verts[i], SCULPT_vertex_co_get(ss, boundary->verts[i]));
  }
  mul_v3_fl(boundary->twist.pivot_position, 1.0f / boundary->verts_num);
  if (boundary->forms_loop) {
    normal_poly_v3(boundary->twist.rotation_axis, poly_verts, boundary->verts_num);
  }
  else {
    sub_v3_v3v3(boundary->twist.rotation_axis,
                SCULPT_vertex_co_get(ss, boundary->pivot_vertex),
                SCULPT_vertex_co_get(ss, boundary->initial_vertex));
    normalize_v3(boundary->twist.rotation_axis);
  }
  MEM_freeN(poly_verts);
}

static void sculpt_boundary_circle_data_init(SculptSession *ss, SculptBoundary *boundary)
{

  const int totvert = SCULPT_vertex_count_get(ss);
  const int totcircles = boundary->max_propagation_steps + 1;

  boundary->circle.radius = MEM_calloc_arrayN(totcircles, sizeof(float), "radius");
  boundary->circle.origin = MEM_calloc_arrayN(totcircles, sizeof(float) * 3, "origin");

  int *count = MEM_calloc_arrayN(totcircles, sizeof(int), "count");
  for (int i = 0; i < totvert; i++) {
    const int propagation_step_index = boundary->edit_info[i].propagation_steps_num;
    if (propagation_step_index == -1) {
      continue;
    }

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    add_v3_v3(boundary->circle.origin[propagation_step_index], SCULPT_vertex_co_get(ss, vertex));
    count[propagation_step_index]++;
  }

  for (int i = 0; i < totcircles; i++) {
    mul_v3_fl(boundary->circle.origin[i], 1.0f / count[i]);
  }

  for (int i = 0; i < totvert; i++) {
    const int propagation_step_index = boundary->edit_info[i].propagation_steps_num;
    if (propagation_step_index == -1) {
      continue;
    }

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    boundary->circle.radius[propagation_step_index] += len_v3v3(
        boundary->circle.origin[propagation_step_index], SCULPT_vertex_co_get(ss, vertex));
  }

  for (int i = 0; i < totcircles; i++) {
    boundary->circle.radius[i] *= 1.0f / count[i];
  }

  MEM_freeN(count);
}

static float sculpt_boundary_displacement_from_grab_delta_get(SculptSession *ss,
                                                              SculptBoundary *boundary)
{
  float plane[4];
  float pos[3];
  float normal[3];
  sub_v3_v3v3(normal, ss->cache->initial_location, boundary->initial_pivot_position);
  normalize_v3(normal);
  plane_from_point_normal_v3(plane, ss->cache->initial_location, normal);
  add_v3_v3v3(pos, ss->cache->initial_location, ss->cache->grab_delta_symmetry);
  return dist_signed_to_plane_v3(pos, plane);
}

/* Deformation tasks callbacks. */
static void do_boundary_brush_bend_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symm_area = ss->cache->mirror_symmetry_pass;
  SculptBoundary *boundary = ss->cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  const float disp = strength * sculpt_boundary_displacement_from_grab_delta_get(ss, boundary);
  float angle_factor = disp / ss->cache->radius;
  /* Angle Snapping when inverting the brush. */
  if (ss->cache->invert) {
    angle_factor = floorf(angle_factor * 10) / 10.0f;
  }
  const float angle = angle_factor * M_PI;
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    float t_orig_co[3];
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);

    sub_v3_v3v3(t_orig_co, orig_data.co, boundary->bend.pivot_positions[vd.index]);
    rotate_v3_v3v3fl(target_co,
                     t_orig_co,
                     boundary->bend.pivot_rotation_axis[vd.index],
                     angle * boundary->edit_info[vd.index].strength_factor * mask * automask);
    add_v3_v3(target_co, boundary->bend.pivot_positions[vd.index]);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_boundary_brush_slide_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symm_area = ss->cache->mirror_symmetry_pass;
  SculptBoundary *boundary = ss->cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  const float disp = sculpt_boundary_displacement_from_grab_delta_get(ss, boundary);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   boundary->slide.directions[vd.index],
                   boundary->edit_info[vd.index].strength_factor * disp * mask * automask *
                       strength);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_boundary_brush_inflate_task_cb_ex(void *__restrict userdata,
                                                 const int n,
                                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symm_area = ss->cache->mirror_symmetry_pass;
  SculptBoundary *boundary = ss->cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  const float disp = sculpt_boundary_displacement_from_grab_delta_get(ss, boundary);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    float normal[3];
    copy_v3_v3(normal, orig_data.no);

    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   orig_data.no,
                   boundary->edit_info[vd.index].strength_factor * disp * mask * automask *
                       strength);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_boundary_brush_grab_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symm_area = ss->cache->mirror_symmetry_pass;
  SculptBoundary *boundary = ss->cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   ss->cache->grab_delta_symmetry,
                   boundary->edit_info[vd.index].strength_factor * mask * automask * strength);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_boundary_brush_twist_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symm_area = ss->cache->mirror_symmetry_pass;
  SculptBoundary *boundary = ss->cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  const float disp = strength * sculpt_boundary_displacement_from_grab_delta_get(ss, boundary);
  float angle_factor = disp / ss->cache->radius;
  /* Angle Snapping when inverting the brush. */
  if (ss->cache->invert) {
    angle_factor = floorf(angle_factor * 10) / 10.0f;
  }
  const float angle = angle_factor * M_PI;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    const float automask = SCULPT_automasking_factor_get(
        ss->cache->automasking, ss, vd.vertex, &automask_data);
    float t_orig_co[3];
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    sub_v3_v3v3(t_orig_co, orig_data.co, boundary->twist.pivot_position);
    rotate_v3_v3v3fl(target_co,
                     t_orig_co,
                     boundary->twist.rotation_axis,
                     angle * mask * automask * boundary->edit_info[vd.index].strength_factor);
    add_v3_v3(target_co, boundary->twist.pivot_position);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_boundary_brush_smooth_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const int symmetry_pass = ss->cache->mirror_symmetry_pass;
  const SculptBoundary *boundary = ss->cache->boundaries[symmetry_pass];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  const float strength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (boundary->edit_info[vd.index].propagation_steps_num == -1) {
      continue;
    }

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!SCULPT_check_vertex_pivot_symmetry(
            orig_data.co, boundary->initial_vertex_position, symm)) {
      continue;
    }

    float coord_accum[3] = {0.0f, 0.0f, 0.0f};
    int total_neighbors = 0;
    const int current_propagation_steps = boundary->edit_info[vd.index].propagation_steps_num;
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      if (current_propagation_steps == boundary->edit_info[ni.index].propagation_steps_num) {
        add_v3_v3(coord_accum, SCULPT_vertex_co_get(ss, ni.vertex));
        total_neighbors++;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    if (total_neighbors == 0) {
      continue;
    }
    float disp[3];
    float avg[3];
    const float mask = vd.mask ? 1.0f - *vd.mask : 1.0f;
    mul_v3_v3fl(avg, coord_accum, 1.0f / total_neighbors);
    sub_v3_v3v3(disp, avg, vd.co);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);
    madd_v3_v3v3fl(
        target_co, vd.co, disp, boundary->edit_info[vd.index].strength_factor * mask * strength);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void SCULPT_boundary_autosmooth(SculptSession *ss, SculptBoundary *boundary)
{
  PBVHNode **nodes;
  int totnode;

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  float bstrength = SCULPT_get_float(ss, autosmooth, NULL, ss->cache->brush);

  CLAMP(bstrength, 0.0f, 1.0f);

  const int count = (int)(bstrength * max_iterations);
  const float last = max_iterations * (bstrength - count * fract);

  const float boundary_radius = ss->cache->radius * (1.0f + ss->cache->brush->boundary_offset) *
                                SCULPT_get_float(
                                    ss, autosmooth_radius_scale, NULL, ss->cache->brush);

  BKE_curvemapping_init(ss->cache->brush->curve);

  BKE_pbvh_get_nodes(ss->pbvh, PBVH_Leaf, &nodes, &totnode);

  float bound_smooth = SCULPT_get_float(ss, boundary_smooth, NULL, ss->cache->brush);
  float projection = SCULPT_get_float(ss, autosmooth_projection, NULL, ss->cache->brush);
  float slide_fset = SCULPT_get_float(ss, fset_slide, NULL, ss->cache->brush);

  for (int iteration = 0; iteration <= count; iteration++) {
    for (int i = 0; i < totnode; i++) {
      const float strength = (iteration != count) ? 1.0f : last;

      PBVHNode *node = nodes[i];
      PBVHVertexIter vd;

      BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
        if (boundary->boundary_dist[vd.index] == FLT_MAX) {
          continue;
        }

        if (boundary->edit_info[vd.index].propagation_steps_num == BOUNDARY_STEPS_NONE) {
          continue;
        }

        float fac = boundary->boundary_dist[vd.index] / boundary_radius;

        if (fac > 1.0f) {
          continue;
        }

        fac = BKE_brush_curve_strength(ss->cache->brush, fac, 1.0f);

        float sco[3];

        SCULPT_neighbor_coords_average_interior(
            ss,
            sco,
            vd.vertex,
            &((SculptSmoothArgs){.projection = projection,
                                 .slide_fset = slide_fset,
                                 .bound_smooth = bound_smooth,
                                 .bound_scl = NULL,
                                 .do_weighted_smooth = ss->cache->brush->flag2 &
                                                       BRUSH_SMOOTH_USE_AREA_WEIGHT,
                                 .preserve_fset_boundaries = ss->cache->brush->flag2 &
                                                             BRUSH_SMOOTH_PRESERVE_FACE_SETS}));

        float *co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);

        interp_v3_v3v3(co, co, sco, strength * fac);
        BKE_pbvh_node_mark_update(node);
      }
      BKE_pbvh_vertex_iter_end;
    }
  }

  MEM_SAFE_FREE(nodes);
}

static void SCULPT_boundary_build_smoothco(SculptSession *ss, SculptBoundary *boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  PBVHNode **nodes;
  int totnode;

  boundary->smoothco = MEM_calloc_arrayN(totvert, sizeof(float) * 3, "boundary->smoothco");

  float bound_smooth = SCULPT_get_float(ss, boundary_smooth, NULL, ss->cache->brush);
  float projection = SCULPT_get_float(ss, autosmooth_projection, NULL, ss->cache->brush);
  float slide_fset = SCULPT_get_float(ss, fset_slide, NULL, ss->cache->brush);

  BKE_pbvh_get_nodes(ss->pbvh, PBVH_Leaf, &nodes, &totnode);

  for (int iteration = 0; iteration < 3; iteration++) {
    for (int i = 0; i < totnode; i++) {
      PBVHNode *node = nodes[i];
      PBVHVertexIter vd;

      BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
        if (boundary->boundary_dist[vd.index] == FLT_MAX) {
          continue;
        }

        float sco[3];

        SCULPT_neighbor_coords_average_interior(
            ss,
            sco,
            vd.vertex,
            &((SculptSmoothArgs){.projection = projection,
                                 .slide_fset = slide_fset,
                                 .bound_smooth = bound_smooth,
                                 .do_weighted_smooth = ss->cache->brush->flag2 &
                                                       BRUSH_SMOOTH_USE_AREA_WEIGHT,
                                 .preserve_fset_boundaries = ss->cache->brush->flag2 &
                                                             BRUSH_SMOOTH_PRESERVE_FACE_SETS}));

        float *co = SCULPT_brush_deform_target_vertex_co_get(ss, boundary->deform_target, &vd);

        interp_v3_v3v3(sco, sco, co, 0.25);
        BKE_pbvh_node_mark_update(node);

        copy_v3_v3(boundary->smoothco[vd.index], sco);
      }
      BKE_pbvh_vertex_iter_end;
    }
  }

  MEM_SAFE_FREE(nodes);
}

/* Main Brush Function. */
void SCULPT_do_boundary_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_cotangents_begin(ob, ss);

  const float radius = ss->cache->radius;
  const float boundary_radius = brush ?
                                    radius *
                                        (1.0f + SCULPT_get_float(ss, boundary_offset, sd, brush)) :
                                    radius;

  const int symm_area = ss->cache->mirror_symmetry_pass;
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {

    PBVHVertRef initial_vertex;

    if (ss->cache->mirror_symmetry_pass == 0) {
      initial_vertex = SCULPT_active_vertex_get(ss);
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_area);
      initial_vertex = SCULPT_nearest_vertex_get(
          sd, ob, location, ss->cache->radius_squared, false);
    }

    ss->cache->boundaries[symm_area] = SCULPT_boundary_data_init(
        sd, ob, brush, initial_vertex, ss->cache->initial_radius);

    if (ss->cache->boundaries[symm_area]) {
      switch (SCULPT_get_int(ss, boundary_deform_type, sd, brush)) {
        case BRUSH_BOUNDARY_DEFORM_BEND:
          sculpt_boundary_bend_data_init(ss, ss->cache->boundaries[symm_area], boundary_radius);
          break;
        case BRUSH_BOUNDARY_DEFORM_EXPAND:
          sculpt_boundary_slide_data_init(ss, ss->cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_TWIST:
          sculpt_boundary_twist_data_init(ss, ss->cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_CIRCLE:
          sculpt_boundary_circle_data_init(ss, ss->cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_INFLATE:
        case BRUSH_BOUNDARY_DEFORM_GRAB:
          /* Do nothing. These deform modes don't need any extra data to be precomputed. */
          break;
      }

      sculpt_boundary_falloff_factor_init(
          ss, sd, ss->cache->boundaries[symm_area], brush, ss->cache->initial_radius);
    }

    if (ss->bm && ss->cache->boundaries[symm_area] &&
        ss->cache->boundaries[symm_area]->boundary_dist) {
      PBVHNode **nodes2;
      int totnode2 = 0;

      BKE_pbvh_get_nodes(ss->pbvh, PBVH_Leaf, &nodes2, &totnode2);

      for (int i = 0; i < totnode2; i++) {
        PBVHNode *node = nodes2[i];
        PBVHVertexIter vd;

        bool ok = false;

        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
          if (ss->cache->boundaries[symm_area]->boundary_dist[vd.index] != FLT_MAX) {
            ok = true;
            break;
          }
        }
        BKE_pbvh_vertex_iter_end;

        if (ok) {
          SCULPT_ensure_dyntopo_node_undo(ob, node, SCULPT_UNDO_COORDS, -1);
        }
      }
    }
  }

  /* No active boundary under the cursor. */
  if (!ss->cache->boundaries[symm_area]) {
    return;
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);

  switch (SCULPT_get_int(ss, boundary_deform_type, sd, brush)) {
    case BRUSH_BOUNDARY_DEFORM_BEND:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_bend_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_EXPAND:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_slide_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_INFLATE:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_inflate_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_GRAB:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_grab_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_TWIST:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_twist_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_SMOOTH:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_smooth_task_cb_ex, &settings);
      break;
    case BRUSH_BOUNDARY_DEFORM_CIRCLE:
      BLI_task_parallel_range(0, totnode, &data, do_boundary_brush_circle_task_cb_ex, &settings);
      break;
  }

  if (SCULPT_get_float(ss, autosmooth, sd, brush) > 0.0f) {
    BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);

    SCULPT_boundary_autosmooth(ss, ss->cache->boundaries[symm_area]);
  }
}

void SCULPT_boundary_edges_preview_draw(const uint gpuattr,
                                        SculptSession *ss,
                                        const float outline_col[3],
                                        const float outline_alpha)
{
  if (!ss->boundary_preview) {
    return;
  }
  immUniformColor3fvAlpha(outline_col, outline_alpha);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, ss->boundary_preview->edges_num * 2);
  for (int i = 0; i < ss->boundary_preview->edges_num; i++) {
    immVertex3fv(gpuattr, SCULPT_vertex_co_get(ss, ss->boundary_preview->edges[i].v1));
    immVertex3fv(gpuattr, SCULPT_vertex_co_get(ss, ss->boundary_preview->edges[i].v2));
  }
  immEnd();
}

void SCULPT_boundary_pivot_line_preview_draw(const uint gpuattr, SculptSession *ss)
{
  if (!ss->boundary_preview) {
    return;
  }
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex3fv(gpuattr, SCULPT_vertex_co_get(ss, ss->boundary_preview->pivot_vertex));
  immVertex3fv(gpuattr, SCULPT_vertex_co_get(ss, ss->boundary_preview->initial_vertex));
  immEnd();
}
