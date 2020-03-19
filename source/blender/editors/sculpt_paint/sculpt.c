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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Sculpt PBVH abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multires, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid. */

void SCULPT_vertex_random_access_init(SculptSession *ss)
{
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT);
  }
}

int SCULPT_vertex_count_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->totvert;
    case PBVH_BMESH:
      return BM_mesh_elem_count(BKE_pbvh_get_bmesh(ss->pbvh), BM_VERT);
    case PBVH_GRIDS:
      return BKE_pbvh_get_grid_num_vertices(ss->pbvh);
  }

  return 0;
}

const float *SCULPT_vertex_co_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        const MVert *mverts = BKE_pbvh_get_verts(ss->pbvh);
        return mverts[index].co;
      }
      else {
        return ss->mvert[index].co;
      }
    }
    case PBVH_BMESH:
      return BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index)->co;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }
  return NULL;
}

static void SCULPT_vertex_normal_get(SculptSession *ss, int index, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      normal_short_to_float_v3(no, ss->mvert[index].no);
      return;
    case PBVH_BMESH:
      copy_v3_v3(no, BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index)->no);
      break;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      copy_v3_v3(no, CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index)));
      break;
    }
  }
}

float SCULPT_vertex_mask_get(SculptSession *ss, int index)
{
  BMVert *v;
  float *mask;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->vmask[index];
    case PBVH_BMESH:
      v = BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index);
      mask = BM_ELEM_CD_GET_VOID_P(v, CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK));
      return *mask;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return *CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }

  return 0.0f;
}

static int SCULPT_active_vertex_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->active_vertex_index;
    case PBVH_BMESH:
      return ss->active_vertex_index;
    case PBVH_GRIDS:
      return ss->active_vertex_index;
  }

  return 0;
}

static const float *SCULPT_active_vertex_co_get(SculptSession *ss)
{
  return SCULPT_vertex_co_get(ss, SCULPT_active_vertex_get(ss));
}

static void SCULPT_active_vertex_normal_get(SculptSession *ss, float normal[3])
{
  SCULPT_vertex_normal_get(ss, SCULPT_active_vertex_get(ss), normal);
}

/* Sculpt Face Sets and Visibility. */

static void SCULPT_vertex_visible_set(SculptSession *ss, int index, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      SET_FLAG_FROM_TEST(ss->mvert[index].flag, !visible, ME_HIDE);
      ss->mvert[index].flag |= ME_VERT_PBVH_UPDATE;
      break;
    case PBVH_BMESH:
      BM_elem_flag_set(BM_vert_at_index(ss->bm, index), BM_ELEM_HIDDEN, !visible);
      break;
    case PBVH_GRIDS:
      break;
  }
}

static bool SCULPT_vertex_visible_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return !(ss->mvert[index].flag & ME_HIDE);
    case PBVH_BMESH:
      return !BM_elem_flag_test(BM_vert_at_index(ss->bm, index), BM_ELEM_HIDDEN);
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

static void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      for (int i = 0; i < ss->totpoly; i++) {
        if (abs(ss->face_sets[i]) == face_set) {
          if (visible) {
            ss->face_sets[i] = abs(ss->face_sets[i]);
          }
          else {
            ss->face_sets[i] = -abs(ss->face_sets[i]);
          }
        }
      }
      break;
    case PBVH_BMESH:
      break;
    case PBVH_GRIDS:
      break;
  }
}

static void SCULPT_face_sets_visibility_invert(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      for (int i = 0; i < ss->totpoly; i++) {
        ss->face_sets[i] *= -1;
      }
      break;
    case PBVH_BMESH:
      break;
    case PBVH_GRIDS:
      break;
  }
}

static void SCULPT_face_sets_visibility_all_set(SculptSession *ss, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      for (int i = 0; i < ss->totpoly; i++) {
        if (visible) {
          ss->face_sets[i] = abs(ss->face_sets[i]);
        }
        else {
          ss->face_sets[i] = -abs(ss->face_sets[i]);
        }
      }
      break;
    case PBVH_BMESH:
      break;
    case PBVH_GRIDS:
      break;
  }
}

static bool SCULPT_vertex_any_face_set_visible_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      for (int j = 0; j < ss->pmap[index].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] > 0) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH:
      return true;
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

static bool SCULPT_vertex_all_face_sets_visible_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      for (int j = 0; j < ss->pmap[index].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] < 0) {
          return false;
        }
      }
      return true;
    }
    case PBVH_BMESH:
      return true;
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

static void SCULPT_vertex_face_set_set(SculptSession *ss, int index, int face_set)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      for (int j = 0; j < ss->pmap[index].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] > 0) {
          ss->face_sets[vert_map->indices[j]] = abs(face_set);
        }
      }
    } break;
    case PBVH_BMESH:
      break;
    case PBVH_GRIDS:
      break;
  }
}

static int SCULPT_vertex_face_set_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      int face_set = 0;
      for (int i = 0; i < ss->pmap[index].count; i++) {
        if (ss->face_sets[vert_map->indices[i]] > face_set) {
          face_set = abs(ss->face_sets[vert_map->indices[i]]);
        }
      }
      return face_set;
    }
    case PBVH_BMESH:
      return 0;
    case PBVH_GRIDS:
      return 0;
  }
  return 0;
}

static bool SCULPT_vertex_has_face_set(SculptSession *ss, int index, int face_set)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      for (int i = 0; i < ss->pmap[index].count; i++) {
        if (ss->face_sets[vert_map->indices[i]] == face_set) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH:
      return true;
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

static void sculpt_visibility_sync_face_sets_to_vertex(SculptSession *ss, int index)
{
  SCULPT_vertex_visible_set(ss, index, SCULPT_vertex_any_face_set_visible_get(ss, index));
}

void SCULPT_visibility_sync_all_face_sets_to_vertices(SculptSession *ss)
{
  for (int i = 0; i < ss->totvert; i++) {
    sculpt_visibility_sync_face_sets_to_vertex(ss, i);
  }
}

static void sculpt_visibility_sync_vertex_to_face_sets(SculptSession *ss, int index)
{
  MeshElemMap *vert_map = &ss->pmap[index];
  const bool visible = SCULPT_vertex_visible_get(ss, index);
  for (int i = 0; i < ss->pmap[index].count; i++) {
    if (visible) {
      ss->face_sets[vert_map->indices[i]] = abs(ss->face_sets[vert_map->indices[i]]);
    }
    else {
      ss->face_sets[vert_map->indices[i]] = -abs(ss->face_sets[vert_map->indices[i]]);
    }
  }
  ss->mvert[index].flag |= ME_VERT_PBVH_UPDATE;
}

void SCULPT_visibility_sync_all_vertex_to_face_sets(SculptSession *ss)
{
  for (int i = 0; i < ss->totvert; i++) {
    sculpt_visibility_sync_vertex_to_face_sets(ss, i);
  }
}

static bool sculpt_vertex_has_unique_face_set(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index];
      int face_set = -1;
      for (int i = 0; i < ss->pmap[index].count; i++) {
        if (face_set == -1) {
          face_set = abs(ss->face_sets[vert_map->indices[i]]);
        }
        else {
          if (abs(ss->face_sets[vert_map->indices[i]]) != face_set) {
            return false;
          }
        }
      }
      return true;
    }
    case PBVH_BMESH:
      return false;
    case PBVH_GRIDS:
      return false;
  }
  return false;
}

static int SCULPT_face_set_next_available_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      int next_face_set = 0;
      for (int i = 0; i < ss->totpoly; i++) {
        if (abs(ss->face_sets[i]) > next_face_set) {
          next_face_set = abs(ss->face_sets[i]);
        }
      }
      next_face_set++;
      return next_face_set;
    }
    case PBVH_BMESH:
      return 0;
    case PBVH_GRIDS:
      return 0;
  }
  return 0;
}

/* Sculpt Neighbor Iterators */

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256

static void sculpt_vertex_neighbor_add(SculptVertexNeighborIter *iter, int neighbor_index)
{
  for (int i = 0; i < iter->size; i++) {
    if (iter->neighbors[i] == neighbor_index) {
      return;
    }
  }

  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = MEM_mallocN(iter->capacity * sizeof(int), "neighbor array");
      memcpy(iter->neighbors, iter->neighbors_fixed, sizeof(int) * iter->size);
    }
    else {
      iter->neighbors = MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(int), "neighbor array");
    }
  }

  iter->neighbors[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbors_get_bmesh(SculptSession *ss,
                                              int index,
                                              SculptVertexNeighborIter *iter)
{
  BMVert *v = BM_vert_at_index(ss->bm, index);
  BMIter liter;
  BMLoop *l;
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    const BMVert *adj_v[2] = {l->prev->v, l->next->v};
    for (int i = 0; i < ARRAY_SIZE(adj_v); i++) {
      const BMVert *v_other = adj_v[i];
      if (BM_elem_index_get(v_other) != (int)index) {
        sculpt_vertex_neighbor_add(iter, BM_elem_index_get(v_other));
      }
    }
  }
}

static void sculpt_vertex_neighbors_get_faces(SculptSession *ss,
                                              int index,
                                              SculptVertexNeighborIter *iter)
{
  MeshElemMap *vert_map = &ss->pmap[(int)index];
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  for (int i = 0; i < ss->pmap[(int)index].count; i++) {
    const MPoly *p = &ss->mpoly[vert_map->indices[i]];
    uint f_adj_v[2];
    if (poly_get_adj_loops_from_vert(p, ss->mloop, (int)index, f_adj_v) != -1) {
      for (int j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
        if (f_adj_v[j] != (int)index) {
          sculpt_vertex_neighbor_add(iter, f_adj_v[j]);
        }
      }
    }
  }
}

static void sculpt_vertex_neighbors_get_grids(SculptSession *ss,
                                              const int index,
                                              const bool include_duplicates,
                                              SculptVertexNeighborIter *iter)
{
  /* TODO: optimize this. We could fill SculptVertexNeighborIter directly,
   * maybe provide coordinate and mask pointers directly rather than converting
   * back and forth between CCGElem and global index. */
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;

  SubdivCCGCoord coord = {.grid_index = grid_index,
                          .x = vertex_index % key->grid_size,
                          .y = vertex_index / key->grid_size};

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(ss->subdiv_ccg, &coord, include_duplicates, &neighbors);

  iter->size = 0;
  iter->num_duplicates = neighbors.num_duplicates;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  for (int i = 0; i < neighbors.size; i++) {
    sculpt_vertex_neighbor_add(iter,
                               neighbors.coords[i].grid_index * key->grid_area +
                                   neighbors.coords[i].y * key->grid_size + neighbors.coords[i].x);
  }

  if (neighbors.coords != neighbors.coords_fixed) {
    MEM_freeN(neighbors.coords);
  }
}

void SCULPT_vertex_neighbors_get(SculptSession *ss,
                                 const int index,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      sculpt_vertex_neighbors_get_faces(ss, index, iter);
      return;
    case PBVH_BMESH:
      sculpt_vertex_neighbors_get_bmesh(ss, index, iter);
      return;
    case PBVH_GRIDS:
      sculpt_vertex_neighbors_get_grids(ss, index, include_duplicates, iter);
      return;
  }
}

static bool sculpt_vertex_is_boundary(SculptSession *ss, const int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      const MeshElemMap *vert_map = &ss->pmap[index];

      if (vert_map->count <= 1) {
        return false;
      }

      if (!SCULPT_vertex_all_face_sets_visible_get(ss, index)) {
        return false;
      }

      for (int i = 0; i < vert_map->count; i++) {
        const MPoly *p = &ss->mpoly[vert_map->indices[i]];
        unsigned f_adj_v[2];
        if (poly_get_adj_loops_from_vert(p, ss->mloop, index, f_adj_v) != -1) {
          int j;
          for (j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
            if (!(vert_map->count != 2 || ss->pmap[f_adj_v[j]].count <= 2)) {
              return false;
            }
          }
        }
      }
      return true;
    }
    case PBVH_BMESH: {
      BMVert *v = BM_vert_at_index(ss->bm, index);
      return BM_vert_is_boundary(v);
    }

    case PBVH_GRIDS:
      return true;
  }

  return true;
}

/* Utils */
bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm)
{
  bool is_in_symmetry_area = true;
  for (int i = 0; i < 3; i++) {
    char symm_it = 1 << i;
    if (symm & symm_it) {
      if (pco[i] == 0.0f) {
        if (vco[i] > 0.0f) {
          is_in_symmetry_area = false;
        }
      }
      if (vco[i] * pco[i] < 0.0f) {
        is_in_symmetry_area = false;
      }
    }
  }
  return is_in_symmetry_area;
}

typedef struct NearestVertexTLSData {
  int nearest_vertex_index;
  float nearest_vertex_distance_squared;
} NearestVertexTLSData;

static void do_nearest_vertex_get_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  NearestVertexTLSData *nvtd = tls->userdata_chunk;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
    if (distance_squared < nvtd->nearest_vertex_distance_squared &&
        distance_squared < data->max_distance_squared) {
      nvtd->nearest_vertex_index = vd.index;
      nvtd->nearest_vertex_distance_squared = distance_squared;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void nearest_vertex_get_reduce(const void *__restrict UNUSED(userdata),
                                      void *__restrict chunk_join,
                                      void *__restrict chunk)
{
  NearestVertexTLSData *join = chunk_join;
  NearestVertexTLSData *nvtd = chunk;
  if (join->nearest_vertex_index == -1) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

int SCULPT_nearest_vertex_get(
    Sculpt *sd, Object *ob, const float co[3], float max_distance, bool use_original)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  int totnode;
  SculptSearchSphereData data = {
      .ss = ss,
      .sd = sd,
      .radius_squared = max_distance * max_distance,
      .original = use_original,
      .center = co,
  };
  BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, &totnode);
  if (totnode == 0) {
    return -1;
  }

  SculptThreadedTaskData task_data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .max_distance_squared = max_distance * max_distance,
  };

  copy_v3_v3(task_data.nearest_vertex_search_co, co);
  NearestVertexTLSData nvtd;
  nvtd.nearest_vertex_index = -1;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = nearest_vertex_get_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexTLSData);
  BKE_pbvh_parallel_range(0, totnode, &task_data, do_nearest_vertex_get_task_cb, &settings);

  MEM_SAFE_FREE(nodes);

  return nvtd.nearest_vertex_index;
}

bool SCULPT_is_symmetry_iteration_valid(char i, char symm)
{
  return i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)));
}

/* Checks if a vertex is inside the brush radius from any of its mirrored axis. */
static bool sculpt_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                                      const float br_co[3],
                                                      float radius,
                                                      char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      float location[3];
      flip_v3_v3(location, br_co, (char)i);
      if (len_squared_v3v3(location, vertex) < radius * radius) {
        return true;
      }
    }
  }
  return false;
}

/* Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices. */

void SCULPT_floodfill_init(SculptSession *ss, SculptFloodFill *flood)
{
  int vertex_count = SCULPT_vertex_count_get(ss);
  SCULPT_vertex_random_access_init(ss);

  flood->queue = BLI_gsqueue_new(sizeof(int));
  flood->visited_vertices = MEM_callocN(vertex_count * sizeof(char), "visited vertices");
}

static void sculpt_floodfill_add_initial(SculptFloodFill *flood, int index)
{
  BLI_gsqueue_push(flood->queue, &index);
}

void SCULPT_floodfill_add_active(
    Sculpt *sd, Object *ob, SculptSession *ss, SculptFloodFill *flood, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      int v = -1;
      if (i == 0) {
        v = SCULPT_active_vertex_get(ss);
      }
      else if (radius > 0.0f) {
        float radius_squared = (radius == FLT_MAX) ? FLT_MAX : radius * radius;
        float location[3];
        flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), i);
        v = SCULPT_nearest_vertex_get(sd, ob, location, radius_squared, false);
      }
      if (v != -1) {
        sculpt_floodfill_add_initial(flood, v);
      }
    }
  }
}

void SCULPT_floodfill_execute(
    SculptSession *ss,
    SculptFloodFill *flood,
    bool (*func)(SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata),
    void *userdata)
{
  while (!BLI_gsqueue_is_empty(flood->queue)) {
    int from_v;
    BLI_gsqueue_pop(flood->queue, &from_v);
    SculptVertexNeighborIter ni;
    sculpt_vertex_duplicates_and_neighbors_iter_begin(ss, from_v, ni)
    {
      const int to_v = ni.index;
      if (flood->visited_vertices[to_v] == 0 && SCULPT_vertex_visible_get(ss, to_v)) {
        flood->visited_vertices[to_v] = 1;

        if (func(ss, from_v, to_v, ni.is_duplicate, userdata)) {
          BLI_gsqueue_push(flood->queue, &to_v);
        }
      }
    }
    sculpt_vertex_neighbors_iter_end(ni);
  }
}

void SCULPT_floodfill_free(SculptFloodFill *flood)
{
  MEM_SAFE_FREE(flood->visited_vertices);
  BLI_gsqueue_free(flood->queue);
  flood->queue = NULL;
}

/** \name Tool Capabilities
 *
 * Avoid duplicate checks, internal logic only,
 * share logic with #rna_def_sculpt_capabilities where possible.
 *
 * \{ */

/* Check if there are any active modifiers in stack.
 * Used for flushing updates at enter/exit sculpt mode. */
static bool sculpt_has_active_modifiers(Scene *scene, Object *ob)
{
  ModifierData *md;
  VirtualModifierData virtualModifierData;

  md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

  /* Exception for shape keys because we can edit those. */
  for (; md; md = md->next) {
    if (modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
      return true;
    }
  }

  return false;
}

static bool sculpt_tool_needs_original(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_THUMB,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_POSE);
}

static bool sculpt_tool_is_proxy_used(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_SMOOTH,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_POSE,
              SCULPT_TOOL_CLOTH,
              SCULPT_TOOL_DRAW_FACE_SETS);
}

static bool sculpt_brush_use_topology_rake(const SculptSession *ss, const Brush *brush)
{
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(brush->sculpt_tool) &&
         (brush->topology_rake_factor > 0.0f) && (ss->bm != NULL);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession *ss, const Brush *brush)
{
  return ((SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool) &&
           (ss->cache->normal_weight > 0.0f)) ||

          ELEM(brush->sculpt_tool,
               SCULPT_TOOL_BLOB,
               SCULPT_TOOL_CREASE,
               SCULPT_TOOL_DRAW,
               SCULPT_TOOL_DRAW_SHARP,
               SCULPT_TOOL_CLOTH,
               SCULPT_TOOL_LAYER,
               SCULPT_TOOL_NUDGE,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_THUMB) ||

          (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA)) ||
         sculpt_brush_use_topology_rake(ss, brush);
}
/** \} */

static bool sculpt_brush_needs_rake_rotation(const Brush *brush)
{
  return SCULPT_TOOL_HAS_RAKE(brush->sculpt_tool) && (brush->rake_factor != 0.0f);
}

typedef enum StrokeFlags {
  CLIP_X = 1,
  CLIP_Y = 2,
  CLIP_Z = 4,
} StrokeFlags;

/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires. */
static void sculpt_orig_vert_data_unode_init(SculptOrigVertData *data,
                                             Object *ob,
                                             SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  memset(data, 0, sizeof(*data));
  data->unode = unode;

  if (bm) {
    data->bm_log = ss->bm_log;
  }
  else {
    data->coords = data->unode->co;
    data->normals = data->unode->no;
    data->vmasks = data->unode->mask;
  }
}

/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires. */
void SCULPT_orig_vert_data_init(SculptOrigVertData *data, Object *ob, PBVHNode *node)
{
  SculptUndoNode *unode;
  unode = SCULPT_undo_push_node(ob, node, SCULPT_UNDO_COORDS);
  sculpt_orig_vert_data_unode_init(data, ob, unode);
}

/* Update a SculptOrigVertData for a particular vertex from the PBVH
 * iterator. */
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, PBVHVertexIter *iter)
{
  if (orig_data->unode->type == SCULPT_UNDO_COORDS) {
    if (orig_data->bm_log) {
      BM_log_original_vert_data(orig_data->bm_log, iter->bm_vert, &orig_data->co, &orig_data->no);
    }
    else {
      orig_data->co = orig_data->coords[iter->i];
      orig_data->no = orig_data->normals[iter->i];
    }
  }
  else if (orig_data->unode->type == SCULPT_UNDO_MASK) {
    if (orig_data->bm_log) {
      orig_data->mask = BM_log_original_mask(orig_data->bm_log, iter->bm_vert);
    }
    else {
      orig_data->mask = orig_data->vmasks[iter->i];
    }
  }
}

static void sculpt_rake_data_update(struct SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

static void sculpt_rake_rotate(const SculptSession *ss,
                               const float sculpt_co[3],
                               const float v_co[3],
                               float factor,
                               float r_delta[3])
{
  float vec_rot[3];

#if 0
  /* lerp */
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);
  mul_qt_v3(ss->cache->rake_rotation_symmetry, vec_rot);
  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
  mul_v3_fl(r_delta, factor);
#else
  /* slerp */
  float q_interp[4];
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);

  copy_qt_qt(q_interp, ss->cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
#endif
}

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss->cache->grab_delta_symmetry`.
 */
static void sculpt_project_v3_normal_align(SculptSession *ss,
                                           const float normal_weight,
                                           float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss->cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss->cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss->cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/** \name SculptProjectVector
 *
 * Fast-path for #project_plane_v3_v3v3
 *
 * \{ */

typedef struct SculptProjectVector {
  float plane[3];
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;

} SculptProjectVector;

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float plane[3])
{
  copy_v3_v3(spvc->plane, plane);
  spvc->len_sq = len_squared_v3(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float vec[3], float r_vec[3])
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  madd_v3_v3fl(r_vec, spvc->plane, dot_v3v3(vec, spvc->plane) * spvc->len_sq_inv_neg);
#endif
}

/** \} */

/**********************************************************************/

/* Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without. Same goes for alt-
 * key smoothing. */
static bool sculpt_stroke_is_dynamic_topology(const SculptSession *ss, const Brush *brush)
{
  return ((BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) &&

          (!ss->cache || (!ss->cache->alt_smooth)) &&

          /* Requires mesh restore, which doesn't work with
           * dynamic-topology. */
          !(brush->flag & BRUSH_ANCHORED) && !(brush->flag & BRUSH_DRAG_DOT) &&

          SCULPT_TOOL_HAS_DYNTOPO(brush->sculpt_tool));
}

/*** paint mesh ***/

static void paint_mesh_restore_co_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  SculptUndoNode *unode;
  SculptUndoType type = (data->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                        SCULPT_UNDO_COORDS);

  if (ss->bm) {
    unode = SCULPT_undo_push_node(data->ob, data->nodes[n], type);
  }
  else {
    unode = SCULPT_undo_get_node(data->nodes[n]);
  }

  if (unode) {
    PBVHVertexIter vd;
    SculptOrigVertData orig_data;

    sculpt_orig_vert_data_unode_init(&orig_data, data->ob, unode);

    BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
    {
      SCULPT_orig_vert_data_update(&orig_data, &vd);

      if (orig_data.unode->type == SCULPT_UNDO_COORDS) {
        copy_v3_v3(vd.co, orig_data.co);
        if (vd.no) {
          copy_v3_v3_short(vd.no, orig_data.no);
        }
        else {
          normal_short_to_float_v3(vd.fno, orig_data.no);
        }
      }
      else if (orig_data.unode->type == SCULPT_UNDO_MASK) {
        *vd.mask = orig_data.mask;
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
    BKE_pbvh_vertex_iter_end;

    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void paint_mesh_restore_co(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  /**
   * Disable OpenMP when dynamic-topology is enabled. Otherwise, new entries might be inserted by
   * #sculpt_undo_push_node() into the GHash used internally by #BM_log_original_vert_co()
   * by a different thread. See T33787. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP) && !ss->bm, totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, paint_mesh_restore_co_task_cb, &settings);

  MEM_SAFE_FREE(nodes);
}

/*** BVH Tree ***/

static void sculpt_extend_redraw_rect_previous(Object *ob, rcti *rect)
{
  /* Expand redraw rect with redraw rect from previous step to
   * prevent partial-redraw issues caused by fast strokes. This is
   * needed here (not in sculpt_flush_update) as it was before
   * because redraw rectangle should be the same in both of
   * optimized PBVH draw function and 3d view redraw (if not -- some
   * mesh parts could disappear from screen (sergey). */
  SculptSession *ss = ob->sculpt;

  if (ss->cache) {
    if (!BLI_rcti_is_empty(&ss->cache->previous_r)) {
      BLI_rcti_union(rect, &ss->cache->previous_r);
    }
  }
}

/* Get a screen-space rectangle of the modified area. */
bool SCULPT_get_redraw_rect(ARegion *region, RegionView3D *rv3d, Object *ob, rcti *rect)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  float bb_min[3], bb_max[3];

  if (!pbvh) {
    return false;
  }

  BKE_pbvh_redraw_BB(pbvh, bb_min, bb_max);

  /* Convert 3D bounding box to screen space. */
  if (!paint_convert_bb_to_rect(rect, bb_min, bb_max, region, rv3d, ob)) {
    return false;
  }

  return true;
}

void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *region, Object *ob)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  /* Copy here, original will be used below. */
  rcti rect = ob->sculpt->cache->current_r;

  sculpt_extend_redraw_rect_previous(ob, &rect);

  paint_calc_redraw_planes(planes, region, ob, &rect);

  /* We will draw this rect, so now we can set it as the previous partial rect.
   * Note that we don't update with the union of previous/current (rect), only with
   * the current. Thus we avoid the rectangle needlessly growing to include
   * all the stroke area. */
  ob->sculpt->cache->previous_r = ob->sculpt->cache->current_r;

  /* Clear redraw flag from nodes. */
  if (pbvh) {
    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateRedraw);
  }
}

/************************ Brush Testing *******************/

void SCULPT_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
  RegionView3D *rv3d = ss->cache ? ss->cache->vc->rv3d : ss->rv3d;
  View3D *v3d = ss->cache ? ss->cache->vc->v3d : ss->v3d;

  test->radius_squared = ss->cache ? ss->cache->radius_squared :
                                     ss->cursor_radius * ss->cursor_radius;
  if (ss->cache) {
    copy_v3_v3(test->location, ss->cache->location);
    test->mirror_symmetry_pass = ss->cache->mirror_symmetry_pass;
  }
  else {
    copy_v3_v3(test->location, ss->cursor_location);
    test->mirror_symmetry_pass = 0;
  }

  /* Just for initialize. */
  test->dist = 0.0f;

  /* Only for 2D projection. */
  zero_v4(test->plane_view);
  zero_v4(test->plane_tool);

  test->mirror_symmetry_pass = ss->cache ? ss->cache->mirror_symmetry_pass : 0;

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    test->clip_rv3d = rv3d;
  }
  else {
    test->clip_rv3d = NULL;
  }
}

BLI_INLINE bool sculpt_brush_test_clipping(const SculptBrushTest *test, const float co[3])
{
  RegionView3D *rv3d = test->clip_rv3d;
  if (!rv3d) {
    return false;
  }
  float symm_co[3];
  flip_v3_v3(symm_co, co, test->mirror_symmetry_pass);
  return ED_view3d_clipping_test(rv3d, symm_co, true);
}

bool SCULPT_brush_test_sphere(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return false;
    }
    test->dist = sqrtf(distsq);
    return true;
  }
  else {
    return false;
  }
}

bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return false;
    }
    test->dist = distsq;
    return true;
  }
  else {
    return false;
  }
}

bool SCULPT_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3])
{
  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }
  return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3])
{
  float co_proj[3];
  closest_to_plane_normalized_v3(co_proj, test->plane_view, co);
  float distsq = len_squared_v3v3(co_proj, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return false;
    }
    test->dist = distsq;
    return true;
  }
  else {
    return false;
  }
}

bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness)
{
  float side = M_SQRT1_2;
  float local_co[3];

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  mul_v3_m4v3(local_co, local, co);

  local_co[0] = fabsf(local_co[0]);
  local_co[1] = fabsf(local_co[1]);
  local_co[2] = fabsf(local_co[2]);

  /* Keep the square and circular brush tips the same size. */
  side += (1.0f - side) * roundness;

  const float hardness = 1.0f - roundness;
  const float constant_side = hardness * side;
  const float falloff_side = roundness * side;

  if (local_co[0] <= side && local_co[1] <= side && local_co[2] <= side) {
    /* Corner, distance to the center of the corner circle. */
    if (min_ff(local_co[0], local_co[1]) > constant_side) {
      float r_point[3];
      copy_v3_fl(r_point, constant_side);
      test->dist = len_v2v2(r_point, local_co) / falloff_side;
      return true;
    }
    /* Side, distance to the square XY axis. */
    if (max_ff(local_co[0], local_co[1]) > constant_side) {
      test->dist = (max_ff(local_co[0], local_co[1]) - constant_side) / falloff_side;
      return true;
    }
    /* Inside the square, constant distance. */
    test->dist = 0.0f;
    return true;
  }
  else {
    /* Outside the square. */
    return false;
  }
}

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape)
{
  SCULPT_brush_test_init(ss, test);
  SculptBrushTestFn sculpt_brush_test_sq_fn;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    sculpt_brush_test_sq_fn = SCULPT_brush_test_sphere_sq;
  }
  else {
    /* PAINT_FALLOFF_SHAPE_TUBE */
    plane_from_point_normal_v3(test->plane_view, test->location, ss->cache->view_normal);
    sculpt_brush_test_sq_fn = SCULPT_brush_test_circle_sq;
  }
  return sculpt_brush_test_sq_fn;
}

const float *SCULPT_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape)
{
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    return ss->cache->sculpt_normal_symm;
  }
  else {
    /* PAINT_FALLOFF_SHAPE_TUBE */
    return ss->cache->view_normal;
  }
}

static float frontface(const Brush *br,
                       const float sculpt_normal[3],
                       const short no[3],
                       const float fno[3])
{
  if (br->flag & BRUSH_FRONTFACE) {
    float dot;

    if (no) {
      float tmp[3];

      normal_short_to_float_v3(tmp, no);
      dot = dot_v3v3(tmp, sculpt_normal);
    }
    else {
      dot = dot_v3v3(fno, sculpt_normal);
    }
    return dot > 0.0f ? dot : 0.0f;
  }
  else {
    return 1.0f;
  }
}

#if 0

static bool sculpt_brush_test_cyl(SculptBrushTest *test,
                                  float co[3],
                                  float location[3],
                                  const float area_no[3])
{
  if (sculpt_brush_test_sphere_fast(test, co)) {
    float t1[3], t2[3], t3[3], dist;

    sub_v3_v3v3(t1, location, co);
    sub_v3_v3v3(t2, x2, location);

    cross_v3_v3v3(t3, area_no, t1);

    dist = len_v3(t3) / len_v3(t2);

    test->dist = dist;

    return 1;
  }

  return 0;
}

#endif

/* Automasking */

static bool sculpt_automasking_enabled(SculptSession *ss, const Brush *br)
{
  if (sculpt_stroke_is_dynamic_topology(ss, br)) {
    return false;
  }
  if (br->automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    return true;
  }
  if (br->automasking_flags & BRUSH_AUTOMASKING_FACE_SETS) {
    return true;
  }
  if (br->automasking_flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
    return true;
  }
  return false;
}

float SCULPT_automasking_factor_get(SculptSession *ss, int vert)
{
  if (ss->cache->automask) {
    return ss->cache->automask[vert];
  }
  else {
    return 1.0f;
  }
}

static void sculpt_automasking_end(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->cache && ss->cache->automask) {
    MEM_freeN(ss->cache->automask);
  }
}

static bool sculpt_automasking_is_constrained_by_radius(Brush *br)
{
  /* 2D falloff is not constrained by radius. */
  if (br->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    return false;
  }

  if (ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE)) {
    return true;
  }
  return false;
}

typedef struct AutomaskFloodFillData {
  float *automask_factor;
  float radius;
  bool use_radius;
  float location[3];
  char symm;
} AutomaskFloodFillData;

static bool automask_floodfill_cb(
    SculptSession *ss, int UNUSED(from_v), int to_v, bool UNUSED(is_duplicate), void *userdata)
{
  AutomaskFloodFillData *data = userdata;

  data->automask_factor[to_v] = 1.0f;
  return (!data->use_radius ||
          sculpt_is_vertex_inside_brush_radius_symm(
              SCULPT_vertex_co_get(ss, to_v), data->location, data->radius, data->symm));
}

static float *sculpt_topology_automasking_init(Sculpt *sd, Object *ob, float *automask_factor)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!sculpt_automasking_enabled(ss, brush)) {
    return NULL;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Topology masking: pmap missing");
    return NULL;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    ss->cache->automask[i] = 0.0f;
  }

  /* Flood fill automask to connected vertices. Limited to vertices inside
   * the brush radius if the tool requires it. */
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, ss->cache->radius);

  AutomaskFloodFillData fdata = {
      .automask_factor = automask_factor,
      .radius = ss->cache->radius,
      .use_radius = sculpt_automasking_is_constrained_by_radius(brush),
      .symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL,
  };
  copy_v3_v3(fdata.location, SCULPT_active_vertex_co_get(ss));
  SCULPT_floodfill_execute(ss, &flood, automask_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  return automask_factor;
}

static float *sculpt_face_sets_automasking_init(Sculpt *sd, Object *ob, float *automask_factor)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!sculpt_automasking_enabled(ss, brush)) {
    return NULL;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Face Sets automasking: pmap missing");
    return NULL;
  }

  int tot_vert = SCULPT_vertex_count_get(ss);
  int active_face_set = SCULPT_vertex_face_set_get(ss, SCULPT_active_vertex_get(ss));
  for (int i = 0; i < tot_vert; i++) {
    if (!SCULPT_vertex_has_face_set(ss, i, active_face_set)) {
      automask_factor[i] *= 0.0f;
    }
  }

  return automask_factor;
}

#define EDGE_DISTANCE_INF -1

static float *sculpt_boundary_edges_automasking_init(Object *ob,
                                                     int propagation_steps,
                                                     float *automask_factor)
{
  SculptSession *ss = ob->sculpt;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Boundary Edges masking: pmap missing");
    return NULL;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  int *edge_distance = MEM_callocN(sizeof(int) * totvert, "automask_factor");

  for (int i = 0; i < totvert; i++) {
    edge_distance[i] = EDGE_DISTANCE_INF;
    if (!sculpt_vertex_is_boundary(ss, i)) {
      edge_distance[i] = 0;
    }
  }

  for (int propagation_it = 0; propagation_it < propagation_steps; propagation_it++) {
    for (int i = 0; i < totvert; i++) {
      if (edge_distance[i] == EDGE_DISTANCE_INF) {
        SculptVertexNeighborIter ni;
        sculpt_vertex_neighbors_iter_begin(ss, i, ni)
        {
          if (edge_distance[ni.index] == propagation_it) {
            edge_distance[i] = propagation_it + 1;
          }
        }
        sculpt_vertex_neighbors_iter_end(ni);
      }
    }
  }

  for (int i = 0; i < totvert; i++) {
    if (edge_distance[i] != EDGE_DISTANCE_INF) {
      const float p = 1.0f - ((float)edge_distance[i] / (float)propagation_steps);
      const float edge_boundary_automask = 3.0f * p * p - 2.0f * p * p * p;
      automask_factor[i] *= (1.0f - edge_boundary_automask);
    }
  }

  MEM_SAFE_FREE(edge_distance);
  return automask_factor;
}

static void sculpt_automasking_init(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (!sculpt_automasking_enabled(ss, brush)) {
    return;
  }

  ss->cache->automask = MEM_callocN(sizeof(float) * SCULPT_vertex_count_get(ss),
                                    "automask_factor");

  for (int i = 0; i < totvert; i++) {
    ss->cache->automask[i] = 1.0f;
  }

  if (brush->automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    SCULPT_vertex_random_access_init(ss);
    sculpt_topology_automasking_init(sd, ob, ss->cache->automask);
  }
  if (brush->automasking_flags & BRUSH_AUTOMASKING_FACE_SETS) {
    SCULPT_vertex_random_access_init(ss);
    sculpt_face_sets_automasking_init(sd, ob, ss->cache->automask);
  }

  if (brush->automasking_flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
    SCULPT_vertex_random_access_init(ss);
    sculpt_boundary_edges_automasking_init(
        ob, brush->automasking_boundary_edges_propagation_steps, ss->cache->automask);
  }
}

/* ===== Sculpting =====
 */
static void flip_v3(float v[3], const ePaintSymmetryFlags symm)
{
  flip_v3_v3(v, v, symm);
}

static void flip_qt(float quat[3], const ePaintSymmetryFlags symm)
{
  flip_qt_qt(quat, quat, symm);
}

static float calc_overlap(StrokeCache *cache, const char symm, const char axis, const float angle)
{
  float mirror[3];
  float distsq;

  flip_v3_v3(mirror, cache->true_location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  distsq = len_squared_v3v3(mirror, cache->true_location);

  if (distsq <= 4.0f * (cache->radius_squared)) {
    return (2.0f * (cache->radius) - sqrtf(distsq)) / (2.0f * (cache->radius));
  }
  else {
    return 0.0f;
  }
}

static float calc_radial_symmetry_feather(Sculpt *sd,
                                          StrokeCache *cache,
                                          const char symm,
                                          const char axis)
{
  float overlap = 0.0f;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache *cache)
{
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float overlap;
    const int symm = cache->symmetry;

    overlap = 0.0f;
    for (int i = 0; i <= symm; i++) {
      if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {

        overlap += calc_overlap(cache, i, 0, 0);

        overlap += calc_radial_symmetry_feather(sd, cache, i, 'X');
        overlap += calc_radial_symmetry_feather(sd, cache, i, 'Y');
        overlap += calc_radial_symmetry_feather(sd, cache, i, 'Z');
      }
    }

    return 1.0f / overlap;
  }
  else {
    return 1.0f;
  }
}

/** \name Calculate Normal and Center
 *
 * Calculate geometry surrounding the brush center.
 * (optionally using original coordinates).
 *
 * Functions are:
 * - #calc_area_center
 * - #calc_area_normal
 * - #calc_area_normal_and_center
 *
 * \note These are all _very_ similar, when changing one, check others.
 * \{ */

typedef struct AreaNormalCenterTLSData {
  /* 0 = towards view, 1 = flipped */
  float area_cos[2][3];
  float area_nos[2][3];
  int count_no[2];
  int count_co[2];
} AreaNormalCenterTLSData;

static void calc_area_normal_and_center_task_cb(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  AreaNormalCenterTLSData *anctd = tls->userdata_chunk;
  const bool use_area_nos = data->use_area_nos;
  const bool use_area_cos = data->use_area_cos;

  PBVHVertexIter vd;
  SculptUndoNode *unode = NULL;

  bool use_original = false;
  bool normal_test_r, area_test_r;

  if (ss->cache && ss->cache->original) {
    unode = SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    use_original = (unode->co || unode->bm_entry);
  }

  SculptBrushTest normal_test;
  SculptBrushTestFn sculpt_brush_normal_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &normal_test, data->brush->falloff_shape);

  /* Update the test radius to sample the normal using the normal radius of the brush. */
  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(normal_test.radius_squared);
    /* Layer brush produces artifacts with normal and area radius. */
    if (!(ss->cache && data->brush->sculpt_tool == SCULPT_TOOL_LAYER)) {
      test_radius *= data->brush->normal_radius_factor;
    }
    normal_test.radius_squared = test_radius * test_radius;
  }

  SculptBrushTest area_test;
  SculptBrushTestFn sculpt_brush_area_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &area_test, data->brush->falloff_shape);

  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(area_test.radius_squared);
    /* Layer brush produces artifacts with normal and area radius */
    if (!(ss->cache && data->brush->sculpt_tool == SCULPT_TOOL_LAYER)) {
      /* Enable area radius control only on Scrape for now */
      if (ELEM(data->brush->sculpt_tool, SCULPT_TOOL_SCRAPE, SCULPT_TOOL_FILL) &&
          data->brush->area_radius_factor > 0.0f) {
        test_radius *= data->brush->area_radius_factor;
      }
      else {
        test_radius *= data->brush->normal_radius_factor;
      }
    }
    area_test.radius_squared = test_radius * test_radius;
  }

  /* When the mesh is edited we can't rely on original coords
   * (original mesh may not even have verts in brush radius). */
  if (use_original && data->has_bm_orco) {
    float(*orco_coords)[3];
    int(*orco_tris)[3];
    int orco_tris_num;

    BKE_pbvh_node_get_bm_orco_data(data->nodes[n], &orco_tris, &orco_tris_num, &orco_coords);

    for (int i = 0; i < orco_tris_num; i++) {
      const float *co_tri[3] = {
          orco_coords[orco_tris[i][0]],
          orco_coords[orco_tris[i][1]],
          orco_coords[orco_tris[i][2]],
      };
      float co[3];

      closest_on_tri_to_point_v3(co, normal_test.location, UNPACK3(co_tri));

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (normal_test_r || area_test_r) {
        float no[3];
        int flip_index;

        normal_tri_v3(no, UNPACK3(co_tri));

        flip_index = (dot_v3v3(ss->cache->view_normal, no) <= 0.0f);
        if (use_area_cos && area_test_r) {
          add_v3_v3(anctd->area_cos[flip_index], co);
          anctd->count_co[flip_index] += 1;
        }
        if (use_area_nos && normal_test_r) {
          add_v3_v3(anctd->area_nos[flip_index], no);
          anctd->count_no[flip_index] += 1;
        }
      }
    }
  }
  else {
    BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
    {
      const float *co;
      /* For bm_vert only. */
      const short *no_s;

      if (use_original) {
        if (unode->bm_entry) {
          BM_log_original_vert_data(ss->bm_log, vd.bm_vert, &co, &no_s);
        }
        else {
          co = unode->co[vd.i];
          no_s = unode->no[vd.i];
        }
      }
      else {
        co = vd.co;
      }

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (normal_test_r || area_test_r) {
        float no_buf[3];
        const float *no;
        int flip_index;

        data->any_vertex_sampled = true;

        if (use_original) {
          normal_short_to_float_v3(no_buf, no_s);
          no = no_buf;
        }
        else {
          if (vd.no) {
            normal_short_to_float_v3(no_buf, vd.no);
            no = no_buf;
          }
          else {
            no = vd.fno;
          }
        }

        flip_index = (dot_v3v3(ss->cache ? ss->cache->view_normal : ss->cursor_view_normal, no) <=
                      0.0f);
        if (use_area_cos && area_test_r) {
          add_v3_v3(anctd->area_cos[flip_index], co);
          anctd->count_co[flip_index] += 1;
        }
        if (use_area_nos && normal_test_r) {
          add_v3_v3(anctd->area_nos[flip_index], no);
          anctd->count_no[flip_index] += 1;
        }
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_area_normal_and_center_reduce(const void *__restrict UNUSED(userdata),
                                               void *__restrict chunk_join,
                                               void *__restrict chunk)
{
  AreaNormalCenterTLSData *join = chunk_join;
  AreaNormalCenterTLSData *anctd = chunk;

  /* For flatten center. */
  add_v3_v3(join->area_cos[0], anctd->area_cos[0]);
  add_v3_v3(join->area_cos[1], anctd->area_cos[1]);

  /* For area normal. */
  add_v3_v3(join->area_nos[0], anctd->area_nos[0]);
  add_v3_v3(join->area_nos[1], anctd->area_nos[1]);

  /* Weights. */
  add_v2_v2_int(join->count_no, anctd->count_no);
  add_v2_v2_int(join->count_co, anctd->count_co);
}

static void calc_area_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since we share logic with vertex paint. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_cos = true,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] != 0) {
      mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
      break;
    }
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }
}

void SCULPT_calc_area_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  bool use_threading = (sd->flags & SCULPT_USE_OPENMP);
  SCULPT_pbvh_calc_area_normal(brush, ob, nodes, totnode, use_threading, r_area_no);
}

/* Expose 'calc_area_normal' externally. */
bool SCULPT_pbvh_calc_area_normal(const Brush *brush,
                                  Object *ob,
                                  PBVHNode **nodes,
                                  int totnode,
                                  bool use_threading,
                                  float r_area_no[3])
{
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_nos = true,
      .any_vertex_sampled = false,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, use_threading, totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For area normal. */
  for (int i = 0; i < ARRAY_SIZE(anctd.area_nos); i++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[i]) != 0.0f) {
      break;
    }
  }

  return data.any_vertex_sampled;
}

/* This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time. */
static void calc_area_normal_and_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_cos = true,
      .use_area_nos = true,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] != 0) {
      mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
      break;
    }
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }

  /* For area normal. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_nos); n++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[n]) != 0.0f) {
      break;
    }
  }
}

/** \} */

/* Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor. */
static float brush_strength(const Sculpt *sd,
                            const StrokeCache *cache,
                            const float feather,
                            const UnifiedPaintSettings *ups)
{
  const Scene *scene = cache->vc->scene;
  const Brush *brush = BKE_paint_brush((Paint *)&sd->paint);

  /* Primary strength input; square it to make lower values more sensitive. */
  const float root_alpha = BKE_brush_alpha_get(scene, brush);
  const float alpha = root_alpha * root_alpha;
  const float dir = (brush->flag & BRUSH_DIR_IN) ? -1.0f : 1.0f;
  const float pressure = BKE_brush_use_alpha_pressure(brush) ? cache->pressure : 1.0f;
  const float pen_flip = cache->pen_flip ? -1.0f : 1.0f;
  const float invert = cache->invert ? -1.0f : 1.0f;
  float overlap = ups->overlap_factor;
  /* Spacing is integer percentage of radius, divide by 50 to get
   * normalized diameter. */

  float flip = dir * invert * pen_flip;
  if (brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
    flip = 1.0f;
  }

  /* Pressure final value after being tweaked depending on the brush. */
  float final_pressure;

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      final_pressure = pow4f(pressure);
      overlap = (1.0f + overlap) / 2.0f;
      return 0.25f * alpha * flip * final_pressure * overlap * feather;
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_LAYER:
      return alpha * flip * pressure * overlap * feather;
    case SCULPT_TOOL_CLOTH:
      /* Expand is more sensible to strength as it keeps expanding the cloth when sculpting over
       * the same vertices. */
      if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_EXPAND) {
        return 0.1f * alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Multiply by 10 by default to get a larger range of strength depending on the size of the
         * brush and object. */
        return 10.0f * alpha * flip * pressure * overlap * feather;
      }
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_SLIDE_RELAX:
      return alpha * pressure * overlap * feather * 2.0f;
    case SCULPT_TOOL_CLAY_STRIPS:
      /* Clay Strips needs less strength to compensate the curve. */
      final_pressure = pressure * pressure * pressure;
      return alpha * flip * final_pressure * overlap * feather * 0.3f;
    case SCULPT_TOOL_CLAY_THUMB:
      final_pressure = pressure * pressure;
      return alpha * flip * final_pressure * overlap * feather * 1.3f;

    case SCULPT_TOOL_MASK:
      overlap = (1.0f + overlap) / 2.0f;
      switch ((BrushMaskTool)brush->mask_tool) {
        case BRUSH_MASK_DRAW:
          return alpha * flip * pressure * overlap * feather;
        case BRUSH_MASK_SMOOTH:
          return alpha * pressure * feather;
      }
      BLI_assert(!"Not supposed to happen");
      return 0.0f;

    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_BLOB:
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_INFLATE:
      if (flip > 0.0f) {
        return 0.250f * alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.125f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FLATTEN:
      if (flip > 0.0f) {
        overlap = (1.0f + overlap) / 2.0f;
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Reduce strength for DEEPEN, PEAKS, and CONTRAST. */
        return 0.5f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_SMOOTH:
      return alpha * pressure * feather;

    case SCULPT_TOOL_PINCH:
      if (flip > 0.0f) {
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.25f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_NUDGE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * pressure * overlap * feather;

    case SCULPT_TOOL_THUMB:
      return alpha * pressure * feather;

    case SCULPT_TOOL_SNAKE_HOOK:
      return root_alpha * feather;

    case SCULPT_TOOL_GRAB:
      return root_alpha * feather;

    case SCULPT_TOOL_ROTATE:
      return alpha * pressure * feather;

    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_POSE:
      return root_alpha * feather;

    default:
      return 0.0f;
  }
}

/* Return a multiplier for brush strength on a particular vertex. */
float SCULPT_brush_strength_factor(SculptSession *ss,
                                   const Brush *br,
                                   const float brush_point[3],
                                   const float len,
                                   const short vno[3],
                                   const float fno[3],
                                   const float mask,
                                   const int vertex_index,
                                   const int thread_id)
{
  StrokeCache *cache = ss->cache;
  const Scene *scene = cache->vc->scene;
  const MTex *mtex = &br->mtex;
  float avg = 1.0f;
  float rgba[4];
  float point[3];

  sub_v3_v3v3(point, brush_point, cache->plane_offset);

  if (!mtex->tex) {
    avg = 1.0f;
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex location directly into a texture. */
    avg = BKE_brush_sample_tex_3d(scene, br, point, rgba, 0, ss->tex_pool);
  }
  else if (ss->texcache) {
    float symm_point[3], point_2d[2];
    /* Quite warnings. */
    float x = 0.0f, y = 0.0f;

    /* If the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */

    flip_v3_v3(symm_point, point, cache->mirror_symmetry_pass);

    if (cache->radial_symmetry_pass) {
      mul_m4_v3(cache->symm_rot_mat_inv, symm_point);
    }

    ED_view3d_project_float_v2_m4(cache->vc->region, symm_point, point_2d, cache->projection_mat);

    /* Still no symmetry supported for other paint modes.
     * Sculpt does it DIY. */
    if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
      /* Similar to fixed mode, but projects from brush angle
       * rather than view direction. */

      mul_m4_v3(cache->brush_local_mat, symm_point);

      x = symm_point[0];
      y = symm_point[1];

      x *= br->mtex.size[0];
      y *= br->mtex.size[1];

      x += br->mtex.ofs[0];
      y += br->mtex.ofs[1];

      avg = paint_get_tex_pixel(&br->mtex, x, y, ss->tex_pool, thread_id);

      avg += br->texture_sample_bias;
    }
    else {
      const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
      avg = BKE_brush_sample_tex_3d(scene, br, point_3d, rgba, 0, ss->tex_pool);
    }
  }

  /* Hardness. */
  float final_len = len;
  const float hardness = br->hardness;
  float p = len / cache->radius;
  if (p < hardness) {
    final_len = 0.0f;
  }
  else if (hardness == 1.0f) {
    final_len = cache->radius;
  }
  else {
    p = (p - hardness) / (1.0f - hardness);
    final_len = p * cache->radius;
  }

  /* Falloff curve. */
  avg *= BKE_brush_curve_strength(br, final_len, cache->radius);
  avg *= frontface(br, cache->view_normal, vno, fno);

  /* Paint mask. */
  avg *= 1.0f - mask;

  /* Automasking. */
  avg *= SCULPT_automasking_factor_get(ss, vertex_index);

  return avg;
}

/* Test AABB against sphere. */
bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v)
{
  SculptSearchSphereData *data = data_v;
  const float *center;
  float nearest[3];
  if (data->center) {
    center = data->center;
  }
  else {
    center = data->ss->cache ? data->ss->cache->location : data->ss->cursor_location;
  }
  float t[3], bb_min[3], bb_max[3];

  if (data->ignore_fully_masked) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_max);
  }

  for (int i = 0; i < 3; i++) {
    if (bb_min[i] > center[i]) {
      nearest[i] = bb_min[i];
    }
    else if (bb_max[i] < center[i]) {
      nearest[i] = bb_max[i];
    }
    else {
      nearest[i] = center[i];
    }
  }

  sub_v3_v3v3(t, center, nearest);

  return len_squared_v3(t) < data->radius_squared;
}

/* 2D projection (distance to line). */
bool SCULPT_search_circle_cb(PBVHNode *node, void *data_v)
{
  SculptSearchCircleData *data = data_v;
  float bb_min[3], bb_max[3];

  if (data->ignore_fully_masked) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_min);
  }

  float dummy_co[3], dummy_depth;
  const float dist_sq = dist_squared_ray_to_aabb_v3(
      data->dist_ray_to_aabb_precalc, bb_min, bb_max, dummy_co, &dummy_depth);

  /* Seems like debug code.
   * Maybe this function can just return true if the node is not fully masked. */
  return dist_sq < data->radius_squared || true;
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags. */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3])
{
  for (int i = 0; i < 3; i++) {
    if (sd->flags & (SCULPT_LOCK_X << i)) {
      continue;
    }

    if ((ss->cache->flag & (CLIP_X << i)) && (fabsf(co[i]) <= ss->cache->clip_tolerance[i])) {
      co[i] = 0.0f;
    }
    else {
      co[i] = val[i];
    }
  }
}

static PBVHNode **sculpt_pbvh_gather_cursor_update(Object *ob,
                                                   Sculpt *sd,
                                                   bool use_original,
                                                   int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  SculptSearchSphereData data = {
      .ss = ss,
      .sd = sd,
      .radius_squared = ss->cursor_radius,
      .original = use_original,
      .ignore_fully_masked = false,
      .center = NULL,
  };
  BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
  return nodes;
}

static PBVHNode **sculpt_pbvh_gather_generic(Object *ob,
                                             Sculpt *sd,
                                             const Brush *brush,
                                             bool use_original,
                                             float radius_scale,
                                             int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;

  /* Build a list of all nodes that are potentially within the cursor or brush's area of influence.
   */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data = {
        .ss = ss,
        .sd = sd,
        .radius_squared = square_f(ss->cache->radius * radius_scale),
        .original = use_original,
        .ignore_fully_masked = brush->sculpt_tool != SCULPT_TOOL_MASK,
        .center = NULL,
    };
    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
  }
  else {
    struct DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data = {
        .ss = ss,
        .sd = sd,
        .radius_squared = ss->cache ? square_f(ss->cache->radius * radius_scale) :
                                      ss->cursor_radius,
        .original = use_original,
        .dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc,
        .ignore_fully_masked = brush->sculpt_tool != SCULPT_TOOL_MASK,
    };
    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_circle_cb, &data, &nodes, r_totnode);
  }
  return nodes;
}

/* Calculate primary direction of movement for many brushes. */
static void calc_sculpt_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  const SculptSession *ss = ob->sculpt;

  switch (brush->sculpt_plane) {
    case SCULPT_DISP_DIR_VIEW:
      copy_v3_v3(r_area_no, ss->cache->true_view_normal);
      break;

    case SCULPT_DISP_DIR_X:
      ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Y:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Z:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
      break;

    case SCULPT_DISP_DIR_AREA:
      SCULPT_calc_area_normal(sd, ob, nodes, totnode, r_area_no);
      break;

    default:
      break;
  }
}

static void update_sculpt_normal(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  StrokeCache *cache = ob->sculpt->cache;
  /* Grab brush does not update the sculpt normal during a stroke. */
  const bool update_normal = !(brush->flag & BRUSH_ORIGINAL_NORMAL) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_GRAB) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK &&
                               cache->normal_weight > 0.0f);

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
      (cache->first_time || update_normal)) {
    calc_sculpt_normal(sd, ob, nodes, totnode, cache->sculpt_normal);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->sculpt_normal, cache->sculpt_normal, cache->view_normal);
      normalize_v3(cache->sculpt_normal);
    }
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
  }
  else {
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
    flip_v3(cache->sculpt_normal_symm, cache->mirror_symmetry_pass);
    mul_m4_v3(cache->symm_rot_mat, cache->sculpt_normal_symm);
  }
}

static void calc_local_y(ViewContext *vc, const float center[3], float y[3])
{
  Object *ob = vc->obact;
  float loc[3], mval_f[2] = {0.0f, 1.0f};
  float zfac;

  mul_v3_m4v3(loc, ob->imat, center);
  zfac = ED_view3d_calc_zfac(vc->rv3d, loc, NULL);

  ED_view3d_win_to_delta(vc->region, mval_f, y, zfac);
  normalize_v3(y);

  add_v3_v3(y, ob->loc);
  mul_m4_v3(ob->imat, y);
}

static void calc_brush_local_mat(const Brush *brush, Object *ob, float local_mat[4][4])
{
  const StrokeCache *cache = ob->sculpt->cache;
  float tmat[4][4];
  float mat[4][4];
  float scale[4][4];
  float angle, v[3];
  float up[3];

  /* Ensure ob->imat is up to date. */
  invert_m4_m4(ob->imat, ob->obmat);

  /* Initialize last column of matrix. */
  mat[0][3] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;

  /* Get view's up vector in object-space. */
  calc_local_y(cache->vc, cache->location, up);

  /* Calculate the X axis of the local matrix. */
  cross_v3_v3v3(v, up, cache->sculpt_normal);
  /* Apply rotation (user angle, rake, etc.) to X axis. */
  angle = brush->mtex.rot - cache->special_rotation;
  rotate_v3_v3v3fl(mat[0], v, cache->sculpt_normal, angle);

  /* Get other axes. */
  cross_v3_v3v3(mat[1], cache->sculpt_normal, mat[0]);
  copy_v3_v3(mat[2], cache->sculpt_normal);

  /* Set location. */
  copy_v3_v3(mat[3], cache->location);

  /* Scale by brush radius. */
  normalize_m4(mat);
  scale_m4_fl(scale, cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Return inverse (for converting from modelspace coords to local
   * area coords). */
  invert_m4_m4(local_mat, tmat);
}

static void update_brush_local_mat(Sculpt *sd, Object *ob)
{
  StrokeCache *cache = ob->sculpt->cache;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0) {
    calc_brush_local_mat(BKE_paint_brush(&sd->paint), ob, cache->brush_local_mat);
  }
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
 * a smoothed location for vert. Skips corner vertices (used by only one
 * polygon). */
static void neighbor_average(SculptSession *ss, float avg[3], uint vert)
{
  const MeshElemMap *vert_map = &ss->pmap[vert];
  const MVert *mvert = ss->mvert;
  float(*deform_co)[3] = ss->deform_cos;

  /* Don't modify corner vertices. */
  if (vert_map->count > 1) {
    int total = 0;

    zero_v3(avg);

    for (int i = 0; i < vert_map->count; i++) {
      const MPoly *p = &ss->mpoly[vert_map->indices[i]];
      uint f_adj_v[2];

      if (poly_get_adj_loops_from_vert(p, ss->mloop, vert, f_adj_v) != -1) {
        for (int j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
          if (vert_map->count != 2 || ss->pmap[f_adj_v[j]].count <= 2) {
            add_v3_v3(avg, deform_co ? deform_co[f_adj_v[j]] : mvert[f_adj_v[j]].co);

            total++;
          }
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, deform_co ? deform_co[vert] : mvert[vert].co);
}

/* Similar to neighbor_average(), but returns an averaged mask value
 * instead of coordinate. Also does not restrict based on border or
 * corner vertices. */
static float neighbor_average_mask(SculptSession *ss, uint vert)
{
  const float *vmask = ss->vmask;
  float avg = 0.0f;
  int total = 0;

  for (int i = 0; i < ss->pmap[vert].count; i++) {
    const MPoly *p = &ss->mpoly[ss->pmap[vert].indices[i]];
    uint f_adj_v[2];

    if (poly_get_adj_loops_from_vert(p, ss->mloop, vert, f_adj_v) != -1) {
      for (int j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
        avg += vmask[f_adj_v[j]];
        total++;
      }
    }
  }

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    return vmask[vert];
  }
}

/* Same logic as neighbor_average(), but for bmesh rather than mesh. */
static void bmesh_neighbor_average(float avg[3], BMVert *v)
{
  /* logic for 3 or more is identical. */
  const int vfcount = BM_vert_face_count_at_most(v, 3);

  /* Don't modify corner vertices. */
  if (vfcount > 1) {
    BMIter liter;
    BMLoop *l;
    int total = 0;

    zero_v3(avg);

    BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
      const BMVert *adj_v[2] = {l->prev->v, l->next->v};

      for (int i = 0; i < ARRAY_SIZE(adj_v); i++) {
        const BMVert *v_other = adj_v[i];
        if (vfcount != 2 || BM_vert_face_count_at_most(v_other, 2) <= 2) {
          add_v3_v3(avg, v_other->co);
          total++;
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, v->co);
}

/* For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure. */
static void bmesh_four_neighbor_average(float avg[3], float direction[3], BMVert *v)
{

  float avg_co[3] = {0.0f, 0.0f, 0.0f};
  float tot_co = 0.0f;

  BMIter eiter;
  BMEdge *e;

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      copy_v3_v3(avg, v->co);
      return;
    }
    BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
    float vec[3];
    sub_v3_v3v3(vec, v_other->co, v->co);
    madd_v3_v3fl(vec, v->no, -dot_v3v3(vec, v->no));
    normalize_v3(vec);

    /* fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction. */
    float fac = dot_v3v3(vec, direction);
    fac = fac * fac - 0.5f;
    fac *= fac;
    madd_v3_v3fl(avg_co, v_other->co, fac);
    tot_co += fac;
  }

  /* In case vert has no Edge s. */
  if (tot_co > 0.0f) {
    mul_v3_v3fl(avg, avg_co, 1.0f / tot_co);

    /* Preserve volume. */
    float vec[3];
    sub_v3_v3(avg, v->co);
    mul_v3_v3fl(vec, v->no, dot_v3v3(avg, v->no));
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, v->co);
  }
  else {
    zero_v3(avg);
  }
}

/* Same logic as neighbor_average_mask(), but for bmesh rather than mesh. */
static float bmesh_neighbor_average_mask(BMVert *v, const int cd_vert_mask_offset)
{
  BMIter liter;
  BMLoop *l;
  float avg = 0.0f;
  int total = 0;

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    /* Skip this vertex. */
    const BMVert *adj_v[2] = {l->prev->v, l->next->v};

    for (int i = 0; i < ARRAY_SIZE(adj_v); i++) {
      const BMVert *v_other = adj_v[i];
      const float *vmask = BM_ELEM_CD_GET_VOID_P(v_other, cd_vert_mask_offset);
      avg += (*vmask);
      total++;
    }
  }

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    const float *vmask = BM_ELEM_CD_GET_VOID_P(v, cd_vert_mask_offset);
    return (*vmask);
  }
}

static void grids_neighbor_average(SculptSession *ss, float result[3], int index)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, index, ni)
  {
    add_v3_v3(avg, SCULPT_vertex_co_get(ss, ni.index));
    total++;
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (total > 0) {
    mul_v3_v3fl(result, avg, 1.0f / (float)total);
  }
  else {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, index));
  }
}

static float grids_neighbor_average_mask(SculptSession *ss, int index)
{
  float avg = 0.0f;
  int total = 0;

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, index, ni)
  {
    avg += SCULPT_vertex_mask_get(ss, ni.index);
    total++;
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    return SCULPT_vertex_mask_get(ss, index);
  }
}

/* Note: uses after-struct allocated mem to store actual cache... */
typedef struct SculptDoBrushSmoothGridDataChunk {
  size_t tmpgrid_size;
} SculptDoBrushSmoothGridDataChunk;

typedef struct {
  SculptSession *ss;
  const float *ray_start;
  const float *ray_normal;
  bool hit;
  float depth;
  bool original;

  int active_vertex_index;
  float *face_normal;

  struct IsectRayPrecalc isect_precalc;
} SculptRaycastData;

typedef struct {
  const float *ray_start;
  bool hit;
  float depth;
  float edge_length;

  struct IsectRayPrecalc isect_precalc;
} SculptDetailRaycastData;

typedef struct {
  SculptSession *ss;
  const float *ray_start, *ray_normal;
  bool hit;
  float depth;
  float dist_sq_to_ray;
  bool original;
} SculptFindNearestToRayData;

static void do_smooth_brush_mesh_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.index,
                                         tls->thread_id);
      if (smooth_mask) {
        float val = neighbor_average_mask(ss, vd.vert_indices[vd.i]) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        neighbor_average(ss, avg, vd.vert_indices[vd.i]);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        sculpt_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_bmesh_task_cb_ex(void *__restrict userdata,
                                             const int n,
                                             const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  smooth_mask ? 0.0f : *vd.mask,
                                                                  vd.index,
                                                                  tls->thread_id);
      if (smooth_mask) {
        float val = bmesh_neighbor_average_mask(vd.bm_vert, vd.cd_vert_mask_offset) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        bmesh_neighbor_average(avg, vd.bm_vert);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        sculpt_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_topology_rake_bmesh_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;

  float direction[3];
  copy_v3_v3(direction, ss->cache->grab_delta_symmetry);

  float tmp[3];
  mul_v3_v3fl(
      tmp, ss->cache->sculpt_normal_symm, dot_v3v3(ss->cache->sculpt_normal_symm, direction));
  sub_v3_v3(direction, tmp);
  normalize_v3(direction);

  /* Cancel if there's no grab data. */
  if (is_zero_v3(direction)) {
    return;
  }

  float bstrength = data->strength;
  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength *
                         SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      *vd.mask,
                                                      vd.index,
                                                      tls->thread_id) *
                         ss->cache->pressure;

      float avg[3], val[3];

      bmesh_four_neighbor_average(avg, direction, vd.bm_vert);

      sub_v3_v3v3(val, avg, vd.co);

      madd_v3_v3v3fl(val, vd.co, val, fade);

      sculpt_clip(sd, ss, vd.co, val);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_multires_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.index,
                                         tls->thread_id);
      if (smooth_mask) {
        float val = grids_neighbor_average_mask(ss, vd.index) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];
        grids_neighbor_average(ss, avg, vd.index);
        sub_v3_v3v3(val, avg, vd.co);
        madd_v3_v3v3fl(val, vd.co, val, fade);
        sculpt_clip(sd, ss, vd.co, val);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void smooth(Sculpt *sd,
                   Object *ob,
                   PBVHNode **nodes,
                   const int totnode,
                   float bstrength,
                   const bool smooth_mask)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  int iteration, count;
  float last;

  CLAMP(bstrength, 0.0f, 1.0f);

  count = (int)(bstrength * max_iterations);
  last = max_iterations * (bstrength - count * fract);

  if (type == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"sculpt smooth: pmap missing");
    return;
  }

  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .smooth_mask = smooth_mask,
        .strength = strength,
    };

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

    switch (type) {
      case PBVH_GRIDS:
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_multires_task_cb_ex, &settings);
        break;
      case PBVH_FACES:
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_mesh_task_cb_ex, &settings);
        break;
      case PBVH_BMESH:
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_bmesh_task_cb_ex, &settings);
        break;
    }
  }
}

static void bmesh_topology_rake(
    Sculpt *sd, Object *ob, PBVHNode **nodes, const int totnode, float bstrength)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  CLAMP(bstrength, 0.0f, 1.0f);

  /* Interactions increase both strength and quality. */
  const int iterations = 3;

  int iteration;
  const int count = iterations * bstrength + 1;
  const float factor = iterations * bstrength / count;

  for (iteration = 0; iteration <= count; iteration++) {

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .strength = factor,
    };
    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

    BKE_pbvh_parallel_range(0, totnode, &data, do_topology_rake_bmesh_task_cb_ex, &settings);
  }
}

static void do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false);
}

static void do_mask_brush_draw_task_cb_ex(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = SCULPT_brush_strength_factor(
          ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.index, tls->thread_id);

      if (bstrength > 0.0f) {
        (*vd.mask) += fade * bstrength * (1.0f - *vd.mask);
      }
      else {
        (*vd.mask) += fade * bstrength * (*vd.mask);
      }
      CLAMP(*vd.mask, 0.0f, 1.0f);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_mask_brush_draw(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_mask_brush_draw_task_cb_ex, &settings);
}

static void do_mask_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((BrushMaskTool)brush->mask_tool) {
    case BRUSH_MASK_DRAW:
      do_mask_brush_draw(sd, ob, nodes, totnode);
      break;
    case BRUSH_MASK_SMOOTH:
      smooth(sd, ob, nodes, totnode, ss->cache->bstrength, true);
      break;
  }
}

static void do_draw_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* Offset vertex. */
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX - this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_initialize(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_draw_brush_task_cb_ex, &settings);
}

static void do_draw_face_sets_brush_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      if (fade > 0.05f) {
        SCULPT_vertex_face_set_set(ss, vd.index, ss->cache->paint_face_set);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_relax_face_sets_brush_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const bool relax_face_sets = !(ss->cache->iteration_count % 3 == 0);
  /* This operations needs a stregth tweak as the relax deformation is too weak by default. */
  if (relax_face_sets) {
    bstrength *= 2.0f;
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (relax_face_sets != sculpt_vertex_has_unique_face_set(ss, vd.index)) {
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask ? *vd.mask : 0.0f,
                                                                    vd.index,
                                                                    tls->thread_id);

        SCULPT_relax_vertex(ss, &vd, fade * bstrength, relax_face_sets, vd.co);
        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_face_sets_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->first_time && ss->cache->mirror_symmetry_pass == 0 &&
      ss->cache->radial_symmetry_pass == 0) {
    if (ss->cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      ss->cache->paint_face_set = SCULPT_vertex_face_set_get(ss, SCULPT_active_vertex_get(ss));
    }
    else {
      /* By default create a new Face Sets. */
      ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
    }
  }

  BKE_curvemapping_initialize(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  if (ss->cache->alt_smooth) {
    for (int i = 0; i < 4; i++) {
      BKE_pbvh_parallel_range(0, totnode, &data, do_relax_face_sets_brush_task_cb_ex, &settings);
    }
  }
  else {
    BKE_pbvh_parallel_range(0, totnode, &data, do_draw_face_sets_brush_task_cb_ex, &settings);
  }
}

static void do_draw_sharp_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      /* Offset vertex. */
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      orig_data.co,
                                                      sqrtf(test.dist),
                                                      orig_data.no,
                                                      NULL,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_sharp_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX - this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_initialize(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_draw_sharp_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */

/** \name Sculpt Topology Brush
 * \{ */

static void do_topology_slide_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      orig_data.co,
                                                      sqrtf(test.dist),
                                                      orig_data.no,
                                                      NULL,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);
      float current_disp[3];
      float current_disp_norm[3];
      float final_disp[3];
      zero_v3(final_disp);
      sub_v3_v3v3(current_disp, ss->cache->location, ss->cache->last_location);
      normalize_v3_v3(current_disp_norm, current_disp);
      mul_v3_v3fl(current_disp, current_disp_norm, ss->cache->bstrength);
      SculptVertexNeighborIter ni;
      sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
      {
        float vertex_disp[3];
        float vertex_disp_norm[3];
        sub_v3_v3v3(vertex_disp, SCULPT_vertex_co_get(ss, ni.index), vd.co);
        normalize_v3_v3(vertex_disp_norm, vertex_disp);
        if (dot_v3v3(current_disp_norm, vertex_disp_norm) > 0.0f) {
          madd_v3_v3fl(final_disp, vertex_disp_norm, dot_v3v3(current_disp, vertex_disp));
        }
      }
      sculpt_vertex_neighbors_iter_end(ni);

      mul_v3_v3fl(proxy[vd.i], final_disp, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_relax_vertex(SculptSession *ss,
                         PBVHVertexIter *vd,
                         float factor,
                         bool filter_boundary_face_sets,
                         float *r_final_pos)
{
  float smooth_pos[3];
  float final_disp[3];
  int count = 0;
  zero_v3(smooth_pos);

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, vd->index, ni)
  {
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !sculpt_vertex_has_unique_face_set(ss, ni.index))) {
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.index));
      count++;
    }
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (count > 0) {
    mul_v3_fl(smooth_pos, 1.0f / (float)count);
  }
  else {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  float smooth_closest_plane[3];
  float vno[3];
  if (vd->no) {
    normal_short_to_float_v3(vno, vd->no);
  }
  else {
    copy_v3_v3(vno, vd->fno);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  plane_from_point_normal_v3(plane, vd->co, vno);
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

static void do_topology_relax_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n]);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      orig_data.co,
                                                      sqrtf(test.dist),
                                                      orig_data.no,
                                                      NULL,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

      SCULPT_relax_vertex(ss, &vd, fade * bstrength, false, vd.co);
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_slide_relax_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->first_time) {
    return;
  }

  BKE_curvemapping_initialize(brush->curve);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  if (ss->cache->alt_smooth) {
    for (int i = 0; i < 4; i++) {
      BKE_pbvh_parallel_range(0, totnode, &data, do_topology_relax_task_cb_ex, &settings);
    }
  }
  else {
    BKE_pbvh_parallel_range(0, totnode, &data, do_topology_slide_task_cb_ex, &settings);
  }
}

static void calc_sculpt_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->mirror_symmetry_pass == 0 && ss->cache->radial_symmetry_pass == 0 &&
      ss->cache->tile_pass == 0 &&
      (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_PLANE) ||
       !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* For area normal. */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

/** \} */

/**
 * Used for 'SCULPT_TOOL_CREASE' and 'SCULPT_TOOL_BLOB'
 */
static void do_crease_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float flippedbstrength = data->flippedbstrength;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* Offset vertex. */
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);
      float val1[3];
      float val2[3];

      /* First we pinch. */
      sub_v3_v3v3(val1, test.location, vd.co);
      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(val1, val1, ss->cache->view_normal);
      }

      mul_v3_fl(val1, fade * flippedbstrength);

      sculpt_project_v3(spvc, val1, val1);

      /* Then we draw. */
      mul_v3_v3fl(val2, offset, fade);

      add_v3_v3v3(proxy[vd.i], val1, val2);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  const Scene *scene = ss->cache->vc->scene;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  float bstrength = ss->cache->bstrength;
  float flippedbstrength, crease_correction;
  float brush_alpha;

  SculptProjectVector spvc;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  crease_correction = brush->crease_pinch_factor * brush->crease_pinch_factor;
  brush_alpha = BKE_brush_alpha_get(scene, brush);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* We always want crease to pinch or blob to relax even when draw is negative. */
  flippedbstrength = (bstrength < 0.0f) ? -crease_correction * bstrength :
                                          crease_correction * bstrength;

  if (brush->sculpt_tool == SCULPT_TOOL_BLOB) {
    flippedbstrength *= -1.0f;
  }

  /* Use surface normal for 'spvc', so the vertices are pinched towards a line instead of a single
   * point. Without this we get a 'flat' surface surrounding the pinch. */
  sculpt_project_v3_cache_init(&spvc, ss->cache->sculpt_normal_symm);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .spvc = &spvc,
      .offset = offset,
      .flippedbstrength = flippedbstrength,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_crease_brush_task_cb_ex, &settings);
}

static void do_pinch_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*stroke_xz)[3] = data->stroke_xz;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  float x_object_space[3];
  float z_object_space[3];
  copy_v3_v3(x_object_space, stroke_xz[0]);
  copy_v3_v3(z_object_space, stroke_xz[1]);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);
      float disp_center[3];
      float x_disp[3];
      float z_disp[3];
      /* Calcualte displacement from the vertex to the brush center. */
      sub_v3_v3v3(disp_center, test.location, vd.co);

      /* Project the displacement into the X vector (aligned to the stroke). */
      mul_v3_v3fl(x_disp, x_object_space, dot_v3v3(disp_center, x_object_space));

      /* Project the displacement into the Z vector (aligned to the surface normal). */
      mul_v3_v3fl(z_disp, z_object_space, dot_v3v3(disp_center, z_object_space));

      /* Add the two projected vectors to calculate the final displacement. The Y component is
       * removed */
      add_v3_v3v3(disp_center, x_disp, z_disp);

      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(disp_center, disp_center, ss->cache->view_normal);
      }
      mul_v3_v3fl(proxy[vd.i], disp_center, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float area_no[3];
  float area_co[3];

  float mat[4][4];
  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  /* delay the first daub because grab delta is not setup */
  if (ss->cache->first_time) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Init mat */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  float stroke_xz[2][3];
  normalize_v3_v3(stroke_xz[0], mat[0]);
  normalize_v3_v3(stroke_xz[1], mat[2]);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .stroke_xz = stroke_xz,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_pinch_brush_task_cb_ex, &settings);
}

static void do_grab_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  orig_data.co,
                                                                  sqrtf(test.dist),
                                                                  orig_data.no,
                                                                  NULL,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_grab_brush_task_cb_ex, &settings);
}

static void do_elastic_deform_brush_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;
  const float *location = ss->cache->location;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  float dir;
  if (ss->cache->mouse[0] > ss->cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush->elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss->cache->mirror_symmetry_pass;
    if (symm == 1 || symm == 2 || symm == 4 || symm == 7) {
      dir = -dir;
    }
  }

  KelvinletParams params;
  float force = len_v3(grab_delta) * dir * bstrength;
  BKE_kelvinlet_init_params(
      &params, ss->cache->radius, force, 1.0f, brush->elastic_deform_volume_preservation);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float final_disp[3];
    switch (brush->elastic_deform_type) {
      case BRUSH_ELASTIC_DEFORM_GRAB:
        BKE_kelvinlet_grab(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
        BKE_kelvinlet_grab_biscale(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
        BKE_kelvinlet_grab_triscale(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_SCALE:
        BKE_kelvinlet_scale(
            final_disp, &params, orig_data.co, location, ss->cache->sculpt_normal_symm);
        break;
      case BRUSH_ELASTIC_DEFORM_TWIST:
        BKE_kelvinlet_twist(
            final_disp, &params, orig_data.co, location, ss->cache->sculpt_normal_symm);
        break;
    }

    if (vd.mask) {
      mul_v3_fl(final_disp, 1.0f - *vd.mask);
    }

    mul_v3_fl(final_disp, SCULPT_automasking_factor_get(ss, vd.index));

    copy_v3_v3(proxy[vd.i], final_disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_elastic_deform_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_elastic_deform_brush_task_cb_ex, &settings);
}

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3])
{
  ePaintSymmetryAreas symm_area = PAINT_SYMM_AREA_DEFAULT;
  if (co[0] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_X;
  }
  if (co[1] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Y;
  }
  if (co[2] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Z;
  }
  return symm_area;
}

void SCULPT_flip_v3_by_symm_area(float v[3],
                                 const ePaintSymmetryFlags symm,
                                 const ePaintSymmetryAreas symmarea,
                                 const float pivot[3])
{
  for (char i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = 1 << i;
    if (symm & symm_it) {
      if (symmarea & symm_it) {
        flip_v3(v, symm_it);
      }
      if (pivot[0] < 0) {
        flip_v3(v, symm_it);
      }
    }
  }
}

void SCULPT_flip_quat_by_symm_area(float quat[3],
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float pivot[3])
{
  for (char i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = 1 << i;
    if (symm & symm_it) {
      if (symmarea & symm_it) {
        flip_qt(quat, symm_it);
      }
      if (pivot[0] < 0) {
        flip_qt(quat, symm_it);
      }
    }
  }
}

void SCULPT_calc_brush_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  zero_v3(r_area_co);
  zero_v3(r_area_no);

  if (ss->cache->mirror_symmetry_pass == 0 && ss->cache->radial_symmetry_pass == 0 &&
      ss->cache->tile_pass == 0 &&
      (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_PLANE) ||
       !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* fFlatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* For area normal. */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

static void do_nudge_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], cono, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_nudge_brush_task_cb_ex, &settings);
}

static void do_snake_hook_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;
  const bool do_rake_rotation = ss->cache->is_rake_rotation_valid;
  const bool do_pinch = (brush->crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - brush->crease_pinch_factor) *
                                  (len_v3(grab_delta) / ss->cache->radius)) :
                                 0.0f;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

      /* Negative pinch will inflate, helps maintain volume. */
      if (do_pinch) {
        float delta_pinch_init[3], delta_pinch[3];

        sub_v3_v3v3(delta_pinch, vd.co, test.location);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(delta_pinch, delta_pinch, ss->cache->true_view_normal);
        }

        /* Important to calculate based on the grabbed location
         * (intentionally ignore fade here). */
        add_v3_v3(delta_pinch, grab_delta);

        sculpt_project_v3(spvc, delta_pinch, delta_pinch);

        copy_v3_v3(delta_pinch_init, delta_pinch);

        float pinch_fade = pinch * fade;
        /* When reducing, scale reduction back by how close to the center we are,
         * so we don't pinch into nothingness. */
        if (pinch > 0.0f) {
          /* Square to have even less impact for close vertices. */
          pinch_fade *= pow2f(min_ff(1.0f, len_v3(delta_pinch) / ss->cache->radius));
        }
        mul_v3_fl(delta_pinch, 1.0f + pinch_fade);
        sub_v3_v3v3(delta_pinch, delta_pinch_init, delta_pinch);
        add_v3_v3(proxy[vd.i], delta_pinch);
      }

      if (do_rake_rotation) {
        float delta_rotate[3];
        sculpt_rake_rotate(ss, test.location, vd.co, fade, delta_rotate);
        add_v3_v3(proxy[vd.i], delta_rotate);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const float bstrength = ss->cache->bstrength;
  float grab_delta[3];

  SculptProjectVector spvc;

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (bstrength < 0.0f) {
    negate_v3(grab_delta);
  }

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  /* Optionally pinch while painting. */
  if (brush->crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .spvc = &spvc,
      .grab_delta = grab_delta,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_snake_hook_brush_task_cb_ex, &settings);
}

static void do_thumb_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  orig_data.co,
                                                                  sqrtf(test.dist),
                                                                  orig_data.no,
                                                                  NULL,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], cono, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_thumb_brush_task_cb_ex, &settings);
}

static void do_rotate_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float angle = data->angle;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      float vec[3], rot[3][3];
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  orig_data.co,
                                                                  sqrtf(test.dist),
                                                                  orig_data.no,
                                                                  NULL,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      sub_v3_v3v3(vec, orig_data.co, ss->cache->location);
      axis_angle_normalized_to_mat3(rot, ss->cache->sculpt_normal_symm, angle * fade);
      mul_v3_m3v3(proxy[vd.i], rot, vec);
      add_v3_v3(proxy[vd.i], ss->cache->location);
      sub_v3_v3(proxy[vd.i], orig_data.co);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  static const int flip[8] = {1, -1, -1, 1, -1, 1, 1, -1};
  const float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .angle = angle,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_rotate_brush_task_cb_ex, &settings);
}

static void do_layer_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float *layer_disp;
  const float bstrength = ss->cache->bstrength;
  const float lim = (bstrength < 0.0f) ? -data->brush->height : data->brush->height;
  /* XXX: layer brush needs conversion to proxy but its more complicated */
  /* proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co; */

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  /* Why does this have to be thread-protected? */
  BLI_mutex_lock(&data->mutex);
  layer_disp = BKE_pbvh_node_layer_disp_get(ss->pbvh, data->nodes[n]);
  BLI_mutex_unlock(&data->mutex);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);
      float *disp = &layer_disp[vd.i];
      float val[3];

      *disp += fade;

      /* Don't let the displacement go past the limit. */
      if ((lim < 0.0f && *disp < lim) || (lim >= 0.0f && *disp > lim)) {
        *disp = lim;
      }

      mul_v3_v3fl(val, offset, *disp);

      if (!ss->multires && !ss->bm && ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
        int index = vd.vert_indices[vd.i];

        /* Persistent base. */
        add_v3_v3(val, ss->layer_co[index]);
      }
      else {
        add_v3_v3(val, orig_data.co);
      }

      sculpt_clip(sd, ss, vd.co, val);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];

  mul_v3_v3v3(offset, ss->cache->scale, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };
  BLI_mutex_init(&data.mutex);

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_layer_brush_task_cb_ex, &settings);

  BLI_mutex_end(&data.mutex);
}

static void do_inflate_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);
      float val[3];

      if (vd.fno) {
        copy_v3_v3(val, vd.fno);
      }
      else {
        normal_short_to_float_v3(val, vd.no);
      }

      mul_v3_fl(val, fade * ss->cache->radius);
      mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_inflate_brush_task_cb_ex, &settings);
}

int SCULPT_plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3])
{
  return (!(brush->flag & BRUSH_PLANE_TRIM) ||
          ((dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared)));
}

static bool plane_point_side_flip(const float co[3], const float plane[4], const bool flip)
{
  float d = plane_point_side_v3(plane, co);
  if (flip) {
    d = -d;
  }
  return d <= 0.0f;
}

int SCULPT_plane_point_side(const float co[3], const float plane[4])
{
  float d = plane_point_side_v3(plane, co);
  return d <= 0.0f;
}

float SCULPT_brush_plane_offset_get(Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  float rv = brush->plane_offset;

  if (brush->flag & BRUSH_OFFSET_PRESSURE) {
    rv *= ss->cache->pressure;
  }

  return rv;
}

static void do_flatten_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float intr[3];
      float val[3];

      closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

      sub_v3_v3v3(val, intr, vd.co);

      if (SCULPT_plane_trim(ss->cache, brush, val)) {
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask ? *vd.mask : 0.0f,
                                                                    vd.index,
                                                                    tls->thread_id);

        mul_v3_v3fl(proxy[vd.i], val, fade);

        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_flatten_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */

/** \name Sculpt Clay Brush
 * \{ */

typedef struct ClaySampleData {
  float plane_dist[2];
} ClaySampleData;

static void calc_clay_surface_task_cb(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  ClaySampleData *csd = tls->userdata_chunk;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;
  float plane[4];

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush->normal_radius_factor;
  test.radius_squared = test_radius * test_radius;
  plane_from_point_normal_v3(plane, area_co, area_no);

  if (is_zero_v4(plane)) {
    return;
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {

    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float plane_dist = dist_signed_to_plane_v3(vd.co, plane);
      float plane_dist_abs = fabsf(plane_dist);
      if (plane_dist > 0.0f) {
        csd->plane_dist[0] = MIN2(csd->plane_dist[0], plane_dist_abs);
      }
      else {
        csd->plane_dist[1] = MIN2(csd->plane_dist[1], plane_dist_abs);
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_clay_surface_reduce(const void *__restrict UNUSED(userdata),
                                     void *__restrict chunk_join,
                                     void *__restrict chunk)
{
  ClaySampleData *join = chunk_join;
  ClaySampleData *csd = chunk;
  join->plane_dist[0] = MIN2(csd->plane_dist[0], join->plane_dist[0]);
  join->plane_dist[1] = MIN2(csd->plane_dist[1], join->plane_dist[1]);
}

static void do_clay_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = fabsf(ss->cache->bstrength);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float intr[3];
      float val[3];
      closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

      sub_v3_v3v3(val, intr, vd.co);

      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], val, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = fabsf(ss->cache->radius);
  const float initial_radius = fabsf(ss->cache->initial_radius);
  bool flip = ss->cache->bstrength < 0.0f;

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;

  float area_no[3];
  float area_co[3];
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SculptThreadedTaskData sample_data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .area_no = area_no,
      .area_co = ss->cache->location,
  };

  ClaySampleData csd = {{0}};

  PBVHParallelSettings sample_settings;
  BKE_pbvh_parallel_range_settings(&sample_settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  sample_settings.func_reduce = calc_clay_surface_reduce;
  sample_settings.userdata_chunk = &csd;
  sample_settings.userdata_chunk_size = sizeof(ClaySampleData);

  BKE_pbvh_parallel_range(0, totnode, &sample_data, calc_clay_surface_task_cb, &sample_settings);

  float d_offset = (csd.plane_dist[0] + csd.plane_dist[1]);
  d_offset = min_ff(radius, d_offset);
  d_offset = d_offset / radius;
  d_offset = 1.0f - d_offset;
  displace = fabsf(initial_radius * (0.25f + offset + (d_offset * 0.15f)));
  if (flip) {
    displace = -displace;
  }

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  copy_v3_v3(area_co, ss->cache->location);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_clay_brush_task_cb_ex, &settings);
}

static void do_clay_strips_brush_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  SculptBrushTest test;
  float(*proxy)[3];
  const bool flip = (ss->cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SCULPT_brush_test_init(ss, &test);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (SCULPT_brush_test_cube(&test, vd.co, mat, brush->tip_roundness)) {
      if (plane_point_side_flip(vd.co, test.plane_tool, flip)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (SCULPT_plane_trim(ss->cache, brush, val)) {
          /* The normal from the vertices is ignored, it causes glitch with planes, see: T44390. */
          const float fade = bstrength *
                             SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          ss->cache->radius * test.dist,
                                                          vd.no,
                                                          vd.fno,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.index,
                                                          tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_clay_strips_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0.0f);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.25f + offset);

  /* The sculpt-plane normal (whatever its set to). */
  float area_no_sp[3];

  /* Geometry normal */
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (ss->cache->first_time) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Init brush local space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = area_co,
      .mat = mat,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_clay_strips_brush_task_cb_ex, &settings);
}

static void do_fill_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (SCULPT_plane_point_side(vd.co, test.plane_tool)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (SCULPT_plane_trim(ss->cache, brush, val)) {
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      sqrtf(test.dist),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask ? *vd.mask : 0.0f,
                                                                      vd.index,
                                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_fill_brush_task_cb_ex, &settings);
}

static void do_scrape_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (!SCULPT_plane_point_side(vd.co, test.plane_tool)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (SCULPT_plane_trim(ss->cache, brush, val)) {
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      sqrtf(test.dist),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask ? *vd.mask : 0.0f,
                                                                      vd.index,
                                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = -radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_scrape_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */

/** \name Sculpt Clay Thumb Brush
 * \{ */

static void do_clay_thumb_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = data->clay_strength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  float plane_tilt[4];
  float normal_tilt[3];
  float imat[4][4];

  invert_m4_m4(imat, mat);
  rotate_v3_v3v3fl(normal_tilt, area_no_sp, imat[0], DEG2RADF(-ss->cache->clay_thumb_front_angle));

  /* Plane aligned to the geometry normal (back part of the brush). */
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  /* Tilted plane (front part of the brush). */
  plane_from_point_normal_v3(plane_tilt, area_co, normal_tilt);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float local_co[3];
      mul_v3_m4v3(local_co, mat, vd.co);
      float intr[3], intr_tilt[3];
      float val[3];

      closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
      closest_to_plane_normalized_v3(intr_tilt, plane_tilt, vd.co);

      /* Mix the deformation of the aligned and the tilted plane based on the brush space vertex
       * coordinates. */
      /* We can also control the mix with a curve if it produces noticeable artifacts in the center
       * of the brush. */
      const float tilt_mix = local_co[1] > 0.0f ? 0.0f : 1.0f;
      interp_v3_v3v3(intr, intr, intr_tilt, tilt_mix);
      sub_v3_v3v3(val, intr_tilt, vd.co);

      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.index,
                                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], val, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static float sculpt_clay_thumb_get_stabilized_pressure(StrokeCache *cache)
{
  float final_pressure = 0.0f;
  for (int i = 0; i < CLAY_STABILIZER_LEN; i++) {
    final_pressure += cache->clay_pressure_stabilizer[i];
  }
  return final_pressure / (float)CLAY_STABILIZER_LEN;
}

static void do_clay_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.25f + offset);

  /* Sampled geometry normal and area center. */
  float area_no_sp[3];
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (ss->cache->first_time) {
    ss->cache->clay_thumb_front_angle = 0.0f;
    return;
  }

  /* Simulate the clay accumulation by increasing the plane angle as more samples are added to the
   * stroke. */
  if (ss->cache->mirror_symmetry_pass == 0) {
    ss->cache->clay_thumb_front_angle += 0.8f;
    CLAMP(ss->cache->clay_thumb_front_angle, 0.0f, 60.0f);
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Displace the brush planes. */
  copy_v3_v3(area_co, ss->cache->location);
  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Init brush local space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  float clay_strength = ss->cache->bstrength *
                        sculpt_clay_thumb_get_stabilized_pressure(ss->cache);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = ss->cache->location,
      .mat = mat,
      .clay_strength = clay_strength,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_clay_thumb_brush_task_cb_ex, &settings);
}

/** \} */

static void do_gravity_task_cb_ex(void *__restrict userdata,
                                  const int n,
                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_gravity(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float offset[3];
  float gravity_vector[3];

  mul_v3_v3fl(gravity_vector, ss->cache->gravity_direction, -ss->cache->radius_squared);

  /* Offset with as much as possible factored in already. */
  mul_v3_v3v3(offset, gravity_vector, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_gravity_task_cb_ex, &settings);
}

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3])
{
  Mesh *me = (Mesh *)ob->data;
  float(*ofs)[3] = NULL;
  int a;
  const int kb_act_idx = ob->shapenr - 1;
  KeyBlock *currkey;

  /* For relative keys editing of base should update other keys. */
  if (BKE_keyblock_is_basis(me->key, kb_act_idx)) {
    ofs = BKE_keyblock_convert_to_vertcos(ob, kb);

    /* Calculate key coord offsets (from previous location). */
    for (a = 0; a < me->totvert; a++) {
      sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
    }

    /* Apply offsets on other keys. */
    for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
      if ((currkey != kb) && (currkey->relative == kb_act_idx)) {
        BKE_keyblock_update_from_offset(ob, currkey, ofs);
      }
    }

    MEM_freeN(ofs);
  }

  /* Modifying of basis key should update mesh. */
  if (kb == me->key->refkey) {
    MVert *mvert = me->mvert;

    for (a = 0; a < me->totvert; a++, mvert++) {
      copy_v3_v3(mvert->co, vertCos[a]);
    }

    BKE_mesh_calc_normals(me);
  }

  /* Apply new coords on active key block, no need to re-allocate kb->data here! */
  BKE_keyblock_update_from_vertcos(ob, kb, vertCos);
}

/* Note: we do the topology update before any brush actions to avoid
 * issues with the proxies. The size of the proxy can't change, so
 * topology must be updated first. */
static void sculpt_topology_update(Sculpt *sd,
                                   Object *ob,
                                   Brush *brush,
                                   UnifiedPaintSettings *UNUSED(ups))
{
  SculptSession *ss = ob->sculpt;

  int n, totnode;
  /* Build a list of all nodes that are potentially within the brush's area of influence. */
  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                             ss->cache->original;
  const float radius_scale = 1.25f;
  PBVHNode **nodes = sculpt_pbvh_gather_generic(
      ob, sd, brush, use_original, radius_scale, &totnode);

  /* Only act if some verts are inside the brush area. */
  if (totnode) {
    PBVHTopologyUpdateMode mode = 0;
    float location[3];

    if (!(sd->flags & SCULPT_DYNTOPO_DETAIL_MANUAL)) {
      if (sd->flags & SCULPT_DYNTOPO_SUBDIVIDE) {
        mode |= PBVH_Subdivide;
      }

      if ((sd->flags & SCULPT_DYNTOPO_COLLAPSE) || (brush->sculpt_tool == SCULPT_TOOL_SIMPLIFY)) {
        mode |= PBVH_Collapse;
      }
    }

    for (n = 0; n < totnode; n++) {
      SCULPT_undo_push_node(ob,
                            nodes[n],
                            brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                     SCULPT_UNDO_COORDS);
      BKE_pbvh_node_mark_update(nodes[n]);

      if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
        BKE_pbvh_node_mark_topology_update(nodes[n]);
        BKE_pbvh_bmesh_node_save_orig(ss->bm, nodes[n]);
      }
    }

    if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
      BKE_pbvh_bmesh_update_topology(ss->pbvh,
                                     mode,
                                     ss->cache->location,
                                     ss->cache->view_normal,
                                     ss->cache->radius,
                                     (brush->flag & BRUSH_FRONTFACE) != 0,
                                     (brush->falloff_shape != PAINT_FALLOFF_SHAPE_SPHERE));
    }

    MEM_SAFE_FREE(nodes);

    /* Update average stroke position. */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);
  }
}

static void do_brush_action_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  /* Face Sets modifications do a single undo push */
  if (data->brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
    /* Draw face sets in smooth mode moves the vertices. */
    if (ss->cache->alt_smooth) {
      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
      BKE_pbvh_node_mark_update(data->nodes[n]);
    }
  }
  else {
    SCULPT_undo_push_node(data->ob,
                          data->nodes[n],
                          data->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                         SCULPT_UNDO_COORDS);
  }

  if (data->brush->sculpt_tool == SCULPT_TOOL_MASK) {
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
  else {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void do_brush_action(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups)
{
  SculptSession *ss = ob->sculpt;
  int totnode;
  PBVHNode **nodes;

  /* Build a list of all nodes that are potentially within the brush's area of influence. */

  /* These brushes need to update all nodes as they are not constrained by the brush radius */
  /* Elastic deform needs all nodes to avoid artifacts as the effect of the brush is not
   * constrained by the radius. */
  /* Pose needs all nodes because it applies all symmetry iterations at the same time and the IK
   * chain can grow to any area of the model. */
  /* This can be optimized by filtering the nodes after calculating the chain. */
  if (ELEM(brush->sculpt_tool, SCULPT_TOOL_ELASTIC_DEFORM, SCULPT_TOOL_POSE)) {
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    SculptSearchSphereData data = {
        .ss = ss,
        .sd = sd,
        .radius_squared = square_f(ss->cache->radius * (1.0 + brush->cloth_sim_limit)),
        .original = false,
        .ignore_fully_masked = false,
        .center = ss->cache->initial_location,
    };
    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, &totnode);
  }
  else {
    const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                               ss->cache->original;
    float radius_scale = 1.0f;
    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (brush->sculpt_tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = 2.0f;
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale, &totnode);
  }

  /* Only act if some verts are inside the brush area. */
  if (totnode) {
    float location[3];

    SculptThreadedTaskData task_data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
    };

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &task_data, do_brush_action_task_cb, &settings);

    /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
     * vertices and uses regular coords undo. */
    if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS && ss->cache->first_time &&
        ss->cache->mirror_symmetry_pass == 0 && !ss->cache->alt_smooth) {
      SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);
    }

    if (sculpt_brush_needs_normal(ss, brush)) {
      update_sculpt_normal(sd, ob, nodes, totnode);
    }

    if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA) {
      update_brush_local_mat(sd, ob);
    }

    if (ss->cache->first_time && ss->cache->mirror_symmetry_pass == 0) {
      if (sculpt_automasking_enabled(ss, brush)) {
        sculpt_automasking_init(sd, ob);
      }
    }

    if (brush->sculpt_tool == SCULPT_TOOL_POSE && ss->cache->first_time &&
        ss->cache->mirror_symmetry_pass == 0) {
      SCULPT_pose_brush_init(sd, ob, ss, brush);
    }

    bool invert = ss->cache->pen_flip || ss->cache->invert || brush->flag & BRUSH_DIR_IN;

    /* Apply one type of brush action. */
    switch (brush->sculpt_tool) {
      case SCULPT_TOOL_DRAW:
        do_draw_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SMOOTH:
        do_smooth_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CREASE:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_BLOB:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_PINCH:
        do_pinch_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_INFLATE:
        do_inflate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_GRAB:
        do_grab_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ROTATE:
        do_rotate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SNAKE_HOOK:
        do_snake_hook_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_NUDGE:
        do_nudge_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_THUMB:
        do_thumb_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_LAYER:
        do_layer_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FLATTEN:
        do_flatten_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY:
        do_clay_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY_STRIPS:
        do_clay_strips_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_MULTIPLANE_SCRAPE:
        SCULPT_do_multiplane_scrape_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY_THUMB:
        do_clay_thumb_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FILL:
        if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
          do_scrape_brush(sd, ob, nodes, totnode);
        }
        else {
          do_fill_brush(sd, ob, nodes, totnode);
        }
        break;
      case SCULPT_TOOL_SCRAPE:
        if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
          do_fill_brush(sd, ob, nodes, totnode);
        }
        else {
          do_scrape_brush(sd, ob, nodes, totnode);
        }
        break;
      case SCULPT_TOOL_MASK:
        do_mask_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_POSE:
        SCULPT_do_pose_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DRAW_SHARP:
        do_draw_sharp_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ELASTIC_DEFORM:
        do_elastic_deform_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SLIDE_RELAX:
        do_slide_relax_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLOTH:
        SCULPT_do_cloth_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DRAW_FACE_SETS:
        do_draw_face_sets_brush(sd, ob, nodes, totnode);
        break;
    }

    if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
        brush->autosmooth_factor > 0) {
      if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
        smooth(sd,
               ob,
               nodes,
               totnode,
               brush->autosmooth_factor * (1.0f - ss->cache->pressure),
               false);
      }
      else {
        smooth(sd, ob, nodes, totnode, brush->autosmooth_factor, false);
      }
    }

    if (sculpt_brush_use_topology_rake(ss, brush)) {
      bmesh_topology_rake(sd, ob, nodes, totnode, brush->topology_rake_factor);
    }

    /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
    if (ss->cache->supports_gravity && brush->sculpt_tool != SCULPT_TOOL_CLOTH) {
      do_gravity(sd, ob, nodes, totnode, sd->gravity_factor);
    }

    MEM_SAFE_FREE(nodes);

    /* Update average stroke position. */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);

    add_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter++;
    /* Update last stroke position. */
    ups->last_stroke_valid = true;
  }
}

/* Flush displacement from deformed PBVH vertex to original mesh. */
static void sculpt_flush_pbvhvert_deform(Object *ob, PBVHVertexIter *vd)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;
  float disp[3], newco[3];
  int index = vd->vert_indices[vd->i];

  sub_v3_v3v3(disp, vd->co, ss->deform_cos[index]);
  mul_m3_v3(ss->deform_imats[index], disp);
  add_v3_v3v3(newco, disp, ss->orig_cos[index]);

  copy_v3_v3(ss->deform_cos[index], vd->co);
  copy_v3_v3(ss->orig_cos[index], newco);

  if (!ss->shapekey_active) {
    copy_v3_v3(me->mvert[index].co, newco);
  }
}

static void sculpt_combine_proxies_task_cb(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  Object *ob = data->ob;

  /* These brushes start from original coordinates. */
  const bool use_orco = ELEM(data->brush->sculpt_tool,
                             SCULPT_TOOL_GRAB,
                             SCULPT_TOOL_ROTATE,
                             SCULPT_TOOL_THUMB,
                             SCULPT_TOOL_ELASTIC_DEFORM,
                             SCULPT_TOOL_POSE);

  PBVHVertexIter vd;
  PBVHProxyNode *proxies;
  int proxy_count;
  float(*orco)[3] = NULL;

  if (use_orco && !ss->bm) {
    orco = SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS)->co;
  }

  BKE_pbvh_node_get_proxies(data->nodes[n], &proxies, &proxy_count);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    float val[3];

    if (use_orco) {
      if (ss->bm) {
        copy_v3_v3(val, BM_log_original_vert_co(ss->bm_log, vd.bm_vert));
      }
      else {
        copy_v3_v3(val, orco[vd.i]);
      }
    }
    else {
      copy_v3_v3(val, vd.co);
    }

    for (int p = 0; p < proxy_count; p++) {
      add_v3_v3(val, proxies[p].co[vd.i]);
    }

    sculpt_clip(sd, ss, vd.co, val);

    if (ss->deform_modifiers_active) {
      sculpt_flush_pbvhvert_deform(ob, &vd);
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_free_proxies(data->nodes[n]);
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

  /* First line is tools that don't support proxies. */
  if (ss->cache->supports_gravity || (sculpt_tool_is_proxy_used(brush->sculpt_tool) == false)) {
    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
    };

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, sculpt_combine_proxies_task_cb, &settings);
  }

  MEM_SAFE_FREE(nodes);
}

/*Copy the modified vertices from bvh to the active key. */
static void sculpt_update_keyblock(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  float(*vertCos)[3];

  /* Keyblock update happens after handling deformation caused by modifiers,
   * so ss->orig_cos would be updated with new stroke. */
  if (ss->orig_cos) {
    vertCos = ss->orig_cos;
  }
  else {
    vertCos = BKE_pbvh_vert_coords_alloc(ss->pbvh);
  }

  if (vertCos) {
    SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);

    if (vertCos != ss->orig_cos) {
      MEM_freeN(vertCos);
    }
  }
}

static void sculpt_flush_stroke_deform_task_cb(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Object *ob = data->ob;
  float(*vertCos)[3] = data->vertCos;

  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_flush_pbvhvert_deform(ob, &vd);

    if (vertCos) {
      int index = vd.vert_indices[vd.i];
      copy_v3_v3(vertCos[index], ss->orig_cos[index]);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

/* Flush displacement from deformed PBVH to original layer. */
static void sculpt_flush_stroke_deform(Sculpt *sd, Object *ob, bool is_proxy_used)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (is_proxy_used) {
    /* This brushes aren't using proxies, so sculpt_combine_proxies() wouldn't propagate needed
     * deformation to original base. */

    int totnode;
    Mesh *me = (Mesh *)ob->data;
    PBVHNode **nodes;
    float(*vertCos)[3] = NULL;

    if (ss->shapekey_active) {
      vertCos = MEM_mallocN(sizeof(*vertCos) * me->totvert, "flushStrokeDeofrm keyVerts");

      /* Mesh could have isolated verts which wouldn't be in BVH, to deal with this we copy old
       * coordinates over new ones and then update coordinates for all vertices from BVH. */
      memcpy(vertCos, ss->orig_cos, sizeof(*vertCos) * me->totvert);
    }

    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .vertCos = vertCos,
    };

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, sculpt_flush_stroke_deform_task_cb, &settings);

    if (vertCos) {
      SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);
      MEM_freeN(vertCos);
    }

    MEM_SAFE_FREE(nodes);

    /* Modifiers could depend on mesh normals, so we should update them.
     * Note, then if sculpting happens on locked key, normals should be re-calculate after applying
     * coords from keyblock on base mesh. */
    BKE_mesh_calc_normals(me);
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
 * calculate multiple modifications to the mesh when symmetry is enabled. */
void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle)
{
  flip_v3_v3(cache->location, cache->true_location, symm);
  flip_v3_v3(cache->last_location, cache->true_last_location, symm);
  flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
  flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);

  flip_v3_v3(cache->initial_location, cache->true_initial_location, symm);
  flip_v3_v3(cache->initial_normal, cache->true_initial_normal, symm);

  /* XXX This reduces the length of the grab delta if it approaches the line of symmetry
   * XXX However, a different approach appears to be needed. */
#if 0
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float frac = 1.0f / max_overlap_count(sd);
    float reduce = (feather - frac) / (1.0f - frac);

    printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

    if (frac < 1.0f) {
      mul_v3_fl(cache->grab_delta_symmetry, reduce);
    }
  }
#endif

  unit_m4(cache->symm_rot_mat);
  unit_m4(cache->symm_rot_mat_inv);
  zero_v3(cache->plane_offset);

  /* Expects XYZ. */
  if (axis) {
    rotate_m4(cache->symm_rot_mat, axis, angle);
    rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
  }

  mul_m4_v3(cache->symm_rot_mat, cache->location);
  mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

  if (cache->supports_gravity) {
    flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
    mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
  }

  if (cache->is_rake_rotation_valid) {
    flip_qt_qt(cache->rake_rotation_symmetry, cache->rake_rotation, symm);
  }
}

typedef void (*BrushActionFunc)(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups);

static void do_tiled(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, BrushActionFunc action)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float radius = cache->radius;
  BoundBox *bb = BKE_object_boundbox_get(ob);
  const float *bbMin = bb->vec[0];
  const float *bbMax = bb->vec[6];
  const float *step = sd->paint.tile_offset;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  /* Position of the "prototype" stroke for tiling. */
  float orgLoc[3];
  copy_v3_v3(orgLoc, cache->location);

  for (int dim = 0; dim < 3; dim++) {
    if ((sd->paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* First do the "untiled" position to initialize the stroke for this location. */
  cache->tile_pass = 0;
  action(sd, ob, brush, ups);

  /* Now do it for all the tiles. */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }

        ++cache->tile_pass;

        for (int dim = 0; dim < 3; dim++) {
          cache->location[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
        }
        action(sd, ob, brush, ups);
      }
    }
  }
}

static void do_radial_symmetry(Sculpt *sd,
                               Object *ob,
                               Brush *brush,
                               UnifiedPaintSettings *ups,
                               BrushActionFunc action,
                               const char symm,
                               const int axis,
                               const float UNUSED(feather))
{
  SculptSession *ss = ob->sculpt;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    ss->cache->radial_symmetry_pass = i;
    SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);
    do_tiled(sd, ob, brush, ups, action);
  }
}

/* Noise texture gives different values for the same input coord; this
 * can tear a multires mesh during sculpting so do a stitch in this
 * case. */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (ss->multires && mtex->tex && mtex->tex->type == TEX_NOISE) {
    multires_stitch_grids(ob);
  }
}

static void do_symmetrical_brush_actions(Sculpt *sd,
                                         Object *ob,
                                         BrushActionFunc action,
                                         UnifiedPaintSettings *ups)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  float feather = calc_symmetry_feather(sd, ss->cache);

  cache->bstrength = brush_strength(sd, cache, feather, ups);
  cache->symmetry = symm;

  /* symm is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
      cache->mirror_symmetry_pass = i;
      cache->radial_symmetry_pass = 0;

      SCULPT_cache_calc_brushdata_symm(cache, i, 0, 0);
      do_tiled(sd, ob, brush, ups, action);

      do_radial_symmetry(sd, ob, brush, ups, action, i, 'X', feather);
      do_radial_symmetry(sd, ob, brush, ups, action, i, 'Y', feather);
      do_radial_symmetry(sd, ob, brush, ups, action, i, 'Z', feather);
    }
  }
}

static void sculpt_update_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int radius = BKE_brush_size_get(scene, brush);

  if (ss->texcache) {
    MEM_freeN(ss->texcache);
    ss->texcache = NULL;
  }

  if (ss->tex_pool) {
    BKE_image_pool_free(ss->tex_pool);
    ss->tex_pool = NULL;
  }

  /* Need to allocate a bigger buffer for bigger brush size. */
  ss->texcache_side = 2 * radius;
  if (!ss->texcache || ss->texcache_side > ss->texcache_actual) {
    ss->texcache = BKE_brush_gen_texture_cache(brush, radius, false);
    ss->texcache_actual = ss->texcache_side;
    ss->tex_pool = BKE_image_pool_new();
  }
}

bool SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT;
}

bool SCULPT_mode_poll_view3d(bContext *C)
{
  return (SCULPT_mode_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll_view3d(bContext *C)
{
  return (SCULPT_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll(bContext *C)
{
  return SCULPT_mode_poll(C) && paint_poll(C);
}

static const char *sculpt_tool_name(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((eBrushSculptTool)brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      return "Draw Brush";
    case SCULPT_TOOL_SMOOTH:
      return "Smooth Brush";
    case SCULPT_TOOL_CREASE:
      return "Crease Brush";
    case SCULPT_TOOL_BLOB:
      return "Blob Brush";
    case SCULPT_TOOL_PINCH:
      return "Pinch Brush";
    case SCULPT_TOOL_INFLATE:
      return "Inflate Brush";
    case SCULPT_TOOL_GRAB:
      return "Grab Brush";
    case SCULPT_TOOL_NUDGE:
      return "Nudge Brush";
    case SCULPT_TOOL_THUMB:
      return "Thumb Brush";
    case SCULPT_TOOL_LAYER:
      return "Layer Brush";
    case SCULPT_TOOL_FLATTEN:
      return "Flatten Brush";
    case SCULPT_TOOL_CLAY:
      return "Clay Brush";
    case SCULPT_TOOL_CLAY_STRIPS:
      return "Clay Strips Brush";
    case SCULPT_TOOL_CLAY_THUMB:
      return "Clay Thumb Brush";
    case SCULPT_TOOL_FILL:
      return "Fill Brush";
    case SCULPT_TOOL_SCRAPE:
      return "Scrape Brush";
    case SCULPT_TOOL_SNAKE_HOOK:
      return "Snake Hook Brush";
    case SCULPT_TOOL_ROTATE:
      return "Rotate Brush";
    case SCULPT_TOOL_MASK:
      return "Mask Brush";
    case SCULPT_TOOL_SIMPLIFY:
      return "Simplify Brush";
    case SCULPT_TOOL_DRAW_SHARP:
      return "Draw Sharp Brush";
    case SCULPT_TOOL_ELASTIC_DEFORM:
      return "Elastic Deform Brush";
    case SCULPT_TOOL_POSE:
      return "Pose Brush";
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      return "Multi-plane Scrape Brush";
    case SCULPT_TOOL_SLIDE_RELAX:
      return "Slide/Relax Brush";
    case SCULPT_TOOL_CLOTH:
      return "Cloth Brush";
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return "Draw Face Sets";
  }

  return "Sculpting";
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

void SCULPT_cache_free(StrokeCache *cache)
{
  if (cache->dial) {
    MEM_freeN(cache->dial);
  }
  if (cache->pose_ik_chain) {
    SCULPT_pose_ik_chain_free(cache->pose_ik_chain);
  }

  if (cache->cloth_sim) {
    SCULPT_cloth_simulation_free(cache->cloth_sim);
  }

  MEM_freeN(cache);
}

/* Initialize mirror modifier clipping. */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
  ModifierData *md;

  for (md = ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
      MirrorModifierData *mmd = (MirrorModifierData *)md;

      if (mmd->flag & MOD_MIR_CLIPPING) {
        /* Check each axis for mirroring. */
        for (int i = 0; i < 3; i++) {
          if (mmd->flag & (MOD_MIR_AXIS_X << i)) {
            /* Enable sculpt clipping. */
            ss->cache->flag |= CLIP_X << i;

            /* Update the clip tolerance. */
            if (mmd->tolerance > ss->cache->clip_tolerance[i]) {
              ss->cache->clip_tolerance[i] = mmd->tolerance;
            }
          }
        }
      }
    }
  }
}

/* Initialize the stroke cache invariants from operator properties. */
static void sculpt_update_cache_invariants(
    bContext *C, Sculpt *sd, SculptSession *ss, wmOperator *op, const float mouse[2])
{
  StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  ViewContext *vc = paint_stroke_view_context(op->customdata);
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  float max_scale;
  int mode;

  ss->cache = cache;

  /* Set scaling adjustment. */
  if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
    max_scale = 1.0f;
  }
  else {
    max_scale = 0.0f;
    for (int i = 0; i < 3; i++) {
      max_scale = max_ff(max_scale, fabsf(ob->scale[i]));
    }
  }
  cache->scale[0] = max_scale / ob->scale[0];
  cache->scale[1] = max_scale / ob->scale[1];
  cache->scale[2] = max_scale / ob->scale[2];

  cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

  cache->flag = 0;

  sculpt_init_mirror_clipping(ob, ss);

  /* Initial mouse location. */
  if (mouse) {
    copy_v2_v2(cache->initial_mouse, mouse);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  copy_v3_v3(cache->initial_location, ss->cursor_location);
  copy_v3_v3(cache->true_initial_location, ss->cursor_location);

  copy_v3_v3(cache->initial_normal, ss->cursor_normal);
  copy_v3_v3(cache->true_initial_normal, ss->cursor_normal);

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* Interpret invert as following normal, for grab brushes. */
  if (SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool)) {
    if (cache->invert) {
      cache->invert = false;
      cache->normal_weight = (cache->normal_weight == 0.0f);
    }
  }

  /* Not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey). */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  /* Alt-Smooth. */
  if (cache->alt_smooth) {
    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      cache->saved_mask_brush_tool = brush->mask_tool;
      brush->mask_tool = BRUSH_MASK_SMOOTH;
    }
    else if (ELEM(brush->sculpt_tool, SCULPT_TOOL_SLIDE_RELAX, SCULPT_TOOL_DRAW_FACE_SETS)) {
      /* Do nothing, this tool has its own smooth mode. */
    }
    else {
      Paint *p = &sd->paint;
      Brush *br;
      int size = BKE_brush_size_get(scene, brush);

      BLI_strncpy(cache->saved_active_brush_name,
                  brush->id.name + 2,
                  sizeof(cache->saved_active_brush_name));

      br = (Brush *)BKE_libblock_find_name(bmain, ID_BR, "Smooth");
      if (br) {
        BKE_paint_brush_set(p, br);
        brush = br;
        cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
        BKE_brush_size_set(scene, brush, size);
        BKE_curvemapping_initialize(brush->curve);
      }
    }
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

  /* Truly temporary data that isn't stored in properties. */

  cache->vc = vc;

  cache->brush = brush;

  /* Cache projection matrix. */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(cache->true_view_normal, viewDir);

  cache->supports_gravity =
      (!ELEM(brush->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_SIMPLIFY) &&
       (sd->gravity_factor > 0.0f));
  /* Get gravity vector in world space. */
  if (cache->supports_gravity) {
    if (sd->gravity_object) {
      Object *gravity_object = sd->gravity_object;

      copy_v3_v3(cache->true_gravity_direction, gravity_object->obmat[2]);
    }
    else {
      cache->true_gravity_direction[0] = cache->true_gravity_direction[1] = 0.0f;
      cache->true_gravity_direction[2] = 1.0f;
    }

    /* Transform to sculpted object space. */
    mul_m3_v3(mat, cache->true_gravity_direction);
    normalize_v3(cache->true_gravity_direction);
  }

  /* Initialize layer brush displacements and persistent coords. */
  if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
    /* Not supported yet for multires or dynamic topology. */
    if (!ss->multires && !ss->bm && !ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
      if (!ss->layer_co) {
        ss->layer_co = MEM_mallocN(sizeof(float) * 3 * ss->totvert, "sculpt mesh vertices copy");
      }

      if (ss->deform_cos) {
        memcpy(ss->layer_co, ss->deform_cos, ss->totvert);
      }
      else {
        for (int i = 0; i < ss->totvert; i++) {
          copy_v3_v3(ss->layer_co[i], ss->mvert[i].co);
        }
      }
    }

    if (ss->bm) {
      /* Free any remaining layer displacements from nodes. If not and topology changes
       * from using another tool, then next layer toolstroke
       * can access past disp array bounds. */
      BKE_pbvh_free_layer_disp(ss->pbvh);
    }
  }

  /* Make copies of the mesh vertex locations and normals for some tools. */
  if (brush->flag & BRUSH_ANCHORED) {
    cache->original = true;
  }

  /* Draw sharp does not need the original coordinates to produce the accumulate effect, so it
   * should work the opposite way. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
    cache->original = true;
  }

  if (SCULPT_TOOL_HAS_ACCUMULATE(brush->sculpt_tool)) {
    if (!(brush->flag & BRUSH_ACCUMULATE)) {
      cache->original = true;
      if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
        cache->original = false;
      }
    }
  }

  cache->first_time = true;

#define PIXEL_INPUT_THRESHHOLD 5
  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->dial = BLI_dial_initialize(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
  }

#undef PIXEL_INPUT_THRESHHOLD
}

static float sculpt_brush_dynamic_size_get(Brush *brush, StrokeCache *cache, float initial_size)
{
  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      return max_ff(initial_size * 0.20f, initial_size * pow3f(cache->pressure));
    case SCULPT_TOOL_CLAY_STRIPS:
      return max_ff(initial_size * 0.30f, initial_size * pow2f(cache->pressure));
    case SCULPT_TOOL_CLAY_THUMB: {
      float clay_stabilized_pressure = sculpt_clay_thumb_get_stabilized_pressure(cache);
      return initial_size * clay_stabilized_pressure;
    }
    default:
      return initial_size * cache->pressure;
  }
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float mouse[2] = {
      cache->mouse[0],
      cache->mouse[1],
  };
  int tool = brush->sculpt_tool;

  if (ELEM(tool,
           SCULPT_TOOL_GRAB,
           SCULPT_TOOL_ELASTIC_DEFORM,
           SCULPT_TOOL_CLOTH,
           SCULPT_TOOL_NUDGE,
           SCULPT_TOOL_CLAY_STRIPS,
           SCULPT_TOOL_PINCH,
           SCULPT_TOOL_MULTIPLANE_SCRAPE,
           SCULPT_TOOL_CLAY_THUMB,
           SCULPT_TOOL_SNAKE_HOOK,
           SCULPT_TOOL_POSE,
           SCULPT_TOOL_THUMB) ||
      sculpt_brush_use_topology_rake(ss, brush)) {
    float grab_location[3], imat[4][4], delta[3], loc[3];

    if (cache->first_time) {
      if (tool == SCULPT_TOOL_GRAB && brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
        copy_v3_v3(cache->orig_grab_location, SCULPT_active_vertex_co_get(ss));
      }
      else {
        copy_v3_v3(cache->orig_grab_location, cache->true_location);
      }
    }
    else if (tool == SCULPT_TOOL_SNAKE_HOOK) {
      add_v3_v3(cache->true_location, cache->grab_delta);
    }

    /* Compute 3d coordinate at same z from original location + mouse. */
    mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
    ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->region, loc, mouse, grab_location);

    /* Compute delta to move verts by. */
    if (!cache->first_time) {
      switch (tool) {
        case SCULPT_TOOL_GRAB:
        case SCULPT_TOOL_POSE:
        case SCULPT_TOOL_THUMB:
        case SCULPT_TOOL_ELASTIC_DEFORM:
          sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
          invert_m4_m4(imat, ob->obmat);
          mul_mat3_m4_v3(imat, delta);
          add_v3_v3(cache->grab_delta, delta);
          break;
        case SCULPT_TOOL_CLAY_STRIPS:
        case SCULPT_TOOL_PINCH:
        case SCULPT_TOOL_CLOTH:
        case SCULPT_TOOL_MULTIPLANE_SCRAPE:
        case SCULPT_TOOL_CLAY_THUMB:
        case SCULPT_TOOL_NUDGE:
        case SCULPT_TOOL_SNAKE_HOOK:
          if (brush->flag & BRUSH_ANCHORED) {
            float orig[3];
            mul_v3_m4v3(orig, ob->obmat, cache->orig_grab_location);
            sub_v3_v3v3(cache->grab_delta, grab_location, orig);
          }
          else {
            sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
          }
          invert_m4_m4(imat, ob->obmat);
          mul_mat3_m4_v3(imat, cache->grab_delta);
          break;
        default:
          /* Use for 'Brush.topology_rake_factor'. */
          sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
          break;
      }
    }
    else {
      zero_v3(cache->grab_delta);
    }

    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->grab_delta, cache->grab_delta, ss->cache->true_view_normal);
    }

    copy_v3_v3(cache->old_grab_location, grab_location);

    if (tool == SCULPT_TOOL_GRAB) {
      if (brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
        copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
      }
      else {
        copy_v3_v3(cache->anchored_location, cache->true_location);
      }
    }
    else if (tool == SCULPT_TOOL_ELASTIC_DEFORM) {
      copy_v3_v3(cache->anchored_location, cache->true_location);
    }
    else if (tool == SCULPT_TOOL_THUMB) {
      copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
    }

    if (ELEM(tool,
             SCULPT_TOOL_GRAB,
             SCULPT_TOOL_THUMB,
             SCULPT_TOOL_ELASTIC_DEFORM,
             SCULPT_TOOL_POSE)) {
      /* Location stays the same for finding vertices in brush radius. */
      copy_v3_v3(cache->true_location, cache->orig_grab_location);

      ups->draw_anchored = true;
      copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
      ups->anchored_size = ups->pixel_radius;
    }

    /* Handle 'rake' */
    cache->is_rake_rotation_valid = false;

    invert_m4_m4(imat, ob->obmat);
    mul_mat3_m4_v3(imat, grab_location);

    if (cache->first_time) {
      copy_v3_v3(cache->rake_data.follow_co, grab_location);
    }

    if (sculpt_brush_needs_rake_rotation(brush)) {
      cache->rake_data.follow_dist = cache->radius * SCULPT_RAKE_BRUSH_FACTOR;

      if (!is_zero_v3(cache->grab_delta)) {
        const float eps = 0.00001f;

        float v1[3], v2[3];

        copy_v3_v3(v1, cache->rake_data.follow_co);
        copy_v3_v3(v2, cache->rake_data.follow_co);
        sub_v3_v3(v2, cache->grab_delta);

        sub_v3_v3(v1, grab_location);
        sub_v3_v3(v2, grab_location);

        if ((normalize_v3(v2) > eps) && (normalize_v3(v1) > eps) &&
            (len_squared_v3v3(v1, v2) > eps)) {
          const float rake_dist_sq = len_squared_v3v3(cache->rake_data.follow_co, grab_location);
          const float rake_fade = (rake_dist_sq > square_f(cache->rake_data.follow_dist)) ?
                                      1.0f :
                                      sqrtf(rake_dist_sq) / cache->rake_data.follow_dist;

          float axis[3], angle;
          float tquat[4];

          rotation_between_vecs_to_quat(tquat, v1, v2);

          /* Use axis-angle to scale rotation since the factor may be above 1. */
          quat_to_axis_angle(axis, &angle, tquat);
          normalize_v3(axis);

          angle *= brush->rake_factor * rake_fade;
          axis_angle_normalized_to_quat(cache->rake_rotation, axis, angle);
          cache->is_rake_rotation_valid = true;
        }
      }
      sculpt_rake_data_update(&cache->rake_data, grab_location);
    }
  }
}

/* Initialize the stroke cache variants from operator properties. */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (cache->first_time ||
      !((brush->flag & BRUSH_ANCHORED) || (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) ||
        (brush->sculpt_tool == SCULPT_TOOL_ROTATE) ||
        (brush->sculpt_tool == SCULPT_TOOL_CLOTH &&
         brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB))) {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
  RNA_float_get_array(ptr, "mouse", cache->mouse);

  /* XXX: Use pressure value from first brush step for brushes which don't support strokes (grab,
   * thumb). They depends on initial state and brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle changing
   * events. We should avoid this after events system re-design. */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
  }

  /* Truly temporary data that isn't stored in properties. */
  if (cache->first_time) {
    if (!BKE_brush_use_locked_size(scene, brush)) {
      cache->initial_radius = paint_calc_object_space_radius(
          cache->vc, cache->true_location, BKE_brush_size_get(scene, brush));
      BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
    }
    else {
      cache->initial_radius = BKE_brush_unprojected_radius_get(scene, brush);
    }
  }

  /* Clay stabilized pressure. */
  if (brush->sculpt_tool == SCULPT_TOOL_CLAY_THUMB) {
    if (ss->cache->first_time) {
      for (int i = 0; i < CLAY_STABILIZER_LEN; i++) {
        ss->cache->clay_pressure_stabilizer[i] = 0.0f;
      }
      ss->cache->clay_pressure_stabilizer_index = 0;
    }
    else {
      cache->clay_pressure_stabilizer[cache->clay_pressure_stabilizer_index] = cache->pressure;
      cache->clay_pressure_stabilizer_index += 1;
      if (cache->clay_pressure_stabilizer_index >= CLAY_STABILIZER_LEN) {
        cache->clay_pressure_stabilizer_index = 0;
      }
    }
  }

  if (BKE_brush_use_size_pressure(brush) &&
      paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT)) {
    cache->radius = sculpt_brush_dynamic_size_get(brush, cache, cache->initial_radius);
    cache->dyntopo_pixel_radius = sculpt_brush_dynamic_size_get(
        brush, cache, ups->initial_pixel_radius);
  }
  else {
    cache->radius = cache->initial_radius;
    cache->dyntopo_pixel_radius = ups->initial_pixel_radius;
  }

  cache->radius_squared = cache->radius * cache->radius;

  if (brush->flag & BRUSH_ANCHORED) {
    /* True location has been calculated as part of the stroke system already here. */
    if (brush->flag & BRUSH_EDGE_TO_EDGE) {
      RNA_float_get_array(ptr, "location", cache->true_location);
    }

    cache->radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, ups->pixel_radius);
    cache->radius_squared = cache->radius * cache->radius;

    copy_v3_v3(cache->anchored_location, cache->true_location);
  }

  sculpt_update_brush_delta(ups, ob, brush);

  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    copy_v3_v3(cache->anchored_location, cache->true_location);
    ups->anchored_size = ups->pixel_radius;
  }

  cache->special_rotation = ups->brush_rotation;

  cache->iteration_count++;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth). */
static bool sculpt_needs_connectivity_info(const Brush *brush, SculptSession *ss, int stroke_mode)
{
  if (ss && ss->pbvh && sculpt_automasking_enabled(ss, brush)) {
    return true;
  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss && ss->cache && ss->cache->alt_smooth) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) || (brush->autosmooth_factor > 0) ||
          ((brush->sculpt_tool == SCULPT_TOOL_MASK) && (brush->mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush->sculpt_tool == SCULPT_TOOL_POSE) ||
          (brush->sculpt_tool == SCULPT_TOOL_SLIDE_RELAX) ||
          (brush->sculpt_tool == SCULPT_TOOL_CLOTH) ||
          (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS));
}

static void sculpt_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  bool need_pmap = sculpt_needs_connectivity_info(brush, ss, 0);
  if (ss->shapekey_active || ss->deform_modifiers_active ||
      (!BKE_sculptsession_use_pbvh_draw(ob, v3d) && need_pmap)) {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_sculpt_update_object_for_edit(depsgraph, ob, need_pmap, false);
  }
}

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptRaycastData *srd = data_v;
    float(*origco)[3] = NULL;
    bool use_origco = false;

    if (srd->original && srd->ss->cache) {
      if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
        use_origco = true;
      }
      else {
        /* Intersect with coordinates from before we started stroke. */
        SculptUndoNode *unode = SCULPT_undo_get_node(node);
        origco = (unode) ? unode->co : NULL;
        use_origco = origco ? true : false;
      }
    }

    if (BKE_pbvh_node_raycast(srd->ss->pbvh,
                              node,
                              origco,
                              use_origco,
                              srd->ray_start,
                              srd->ray_normal,
                              &srd->isect_precalc,
                              &srd->depth,
                              &srd->active_vertex_index,
                              srd->face_normal)) {
      srd->hit = true;
      *tmin = srd->depth;
    }
  }
}

static void sculpt_find_nearest_to_ray_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptFindNearestToRayData *srd = data_v;
    float(*origco)[3] = NULL;
    bool use_origco = false;

    if (srd->original && srd->ss->cache) {
      if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
        use_origco = true;
      }
      else {
        /* Intersect with coordinates from before we started stroke. */
        SculptUndoNode *unode = SCULPT_undo_get_node(node);
        origco = (unode) ? unode->co : NULL;
        use_origco = origco ? true : false;
      }
    }

    if (BKE_pbvh_node_find_nearest_to_ray(srd->ss->pbvh,
                                          node,
                                          origco,
                                          use_origco,
                                          srd->ray_start,
                                          srd->ray_normal,
                                          &srd->depth,
                                          &srd->dist_sq_to_ray)) {
      srd->hit = true;
      *tmin = srd->dist_sq_to_ray;
    }
  }
}

static void sculpt_raycast_detail_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptDetailRaycastData *srd = data_v;
    if (BKE_pbvh_bmesh_node_raycast_detail(
            node, srd->ray_start, &srd->isect_precalc, &srd->depth, &srd->edge_length)) {
      srd->hit = true;
      *tmin = srd->depth;
    }
  }
}

static float sculpt_raycast_init(ViewContext *vc,
                                 const float mouse[2],
                                 float ray_start[3],
                                 float ray_end[3],
                                 float ray_normal[3],
                                 bool original)
{
  float obimat[4][4];
  float dist;
  Object *ob = vc->obact;
  RegionView3D *rv3d = vc->region->regiondata;
  View3D *v3d = vc->v3d;

  /* TODO: what if the segment is totally clipped? (return == 0). */
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->region, vc->v3d, mouse, ray_start, ray_end, true);

  invert_m4_m4(obimat, ob->obmat);
  mul_m4_v3(obimat, ray_start);
  mul_m4_v3(obimat, ray_end);

  sub_v3_v3v3(ray_normal, ray_end, ray_start);
  dist = normalize_v3(ray_normal);

  if ((rv3d->is_persp == false) &&
      /* If the ray is clipped, don't adjust its start/end. */
      RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    BKE_pbvh_raycast_project_ray_root(ob->sculpt->pbvh, original, ray_start, ray_end, ray_normal);

    /* rRecalculate the normal. */
    sub_v3_v3v3(ray_normal, ray_end, ray_start);
    dist = normalize_v3(ray_normal);
  }

  return dist;
}

/* Gets the normal, location and active vertex location of the geometry under the cursor. This also
 * updates the active vertex and cursor related data of the SculptSession using the mouse position
 */
bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Sculpt *sd = scene->toolsettings->sculpt;
  Object *ob;
  SculptSession *ss;
  ViewContext vc;
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3], sampled_normal[3],
      mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  int totnode;
  bool original = false, hit = false;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;
  ss = ob->sculpt;

  if (!ss->pbvh) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* PBVH raycast to get active vertex and face normal. */
  depth = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);
  sculpt_stroke_modifiers_check(C, ob, brush);

  SculptRaycastData srd = {
      .original = original,
      .ss = ob->sculpt,
      .hit = false,
      .ray_start = ray_start,
      .ray_normal = ray_normal,
      .depth = depth,
      .face_normal = face_normal,
  };
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  BKE_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original);

  /* Cursor is not over the mesh, return default values. */
  if (!srd.hit) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* Update the active vertex of the SculptSession. */
  ss->active_vertex_index = srd.active_vertex_index;
  copy_v3_v3(out->active_vertex_co, SCULPT_active_vertex_co_get(ss));

  copy_v3_v3(out->location, ray_normal);
  mul_v3_fl(out->location, srd.depth);
  add_v3_v3(out->location, ray_start);

  /* Option to return the face normal directly for performance o accuracy reasons. */
  if (!use_sampled_normal) {
    copy_v3_v3(out->normal, srd.face_normal);
    return hit;
  }

  /* Sampled normal calculation. */
  float radius;

  /* Update cursor data in SculptSession. */
  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, vc.rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(ss->cursor_view_normal, viewDir);
  copy_v3_v3(ss->cursor_normal, srd.face_normal);
  copy_v3_v3(ss->cursor_location, out->location);
  ss->rv3d = vc.rv3d;
  ss->v3d = vc.v3d;

  if (!BKE_brush_use_locked_size(scene, brush)) {
    radius = paint_calc_object_space_radius(&vc, out->location, BKE_brush_size_get(scene, brush));
  }
  else {
    radius = BKE_brush_unprojected_radius_get(scene, brush);
  }
  ss->cursor_radius = radius;

  PBVHNode **nodes = sculpt_pbvh_gather_cursor_update(ob, sd, original, &totnode);

  /* In case there are no nodes under the cursor, return the face normal. */
  if (!totnode) {
    MEM_SAFE_FREE(nodes);
    copy_v3_v3(out->normal, srd.face_normal);
    return true;
  }

  /* Calculate the sampled normal. */
  if (SCULPT_pbvh_calc_area_normal(brush, ob, nodes, totnode, true, sampled_normal)) {
    copy_v3_v3(out->normal, sampled_normal);
    copy_v3_v3(ss->cursor_sampled_normal, sampled_normal);
  }
  else {
    /* Use face normal when there are no vertices to sample inside the cursor radius. */
    copy_v3_v3(out->normal, srd.face_normal);
  }
  MEM_SAFE_FREE(nodes);
  return true;
}

/* Do a raycast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise. */
bool SCULPT_stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob;
  SculptSession *ss;
  StrokeCache *cache;
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3];
  bool original;
  ViewContext vc;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;

  ss = ob->sculpt;
  cache = ss->cache;
  original = (cache) ? cache->original : false;

  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  sculpt_stroke_modifiers_check(C, ob, brush);

  depth = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT);
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
  }

  bool hit = false;
  {
    SculptRaycastData srd;
    srd.ss = ob->sculpt;
    srd.ray_start = ray_start;
    srd.ray_normal = ray_normal;
    srd.hit = false;
    srd.depth = depth;
    srd.original = original;
    srd.face_normal = face_normal;
    isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

    BKE_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original);
    if (srd.hit) {
      hit = true;
      copy_v3_v3(out, ray_normal);
      mul_v3_fl(out, srd.depth);
      add_v3_v3(out, ray_start);
    }
  }

  if (!hit) {
    if (ELEM(brush->falloff_shape, PAINT_FALLOFF_SHAPE_TUBE)) {
      SculptFindNearestToRayData srd = {
          .original = original,
          .ss = ob->sculpt,
          .hit = false,
          .ray_start = ray_start,
          .ray_normal = ray_normal,
          .depth = FLT_MAX,
          .dist_sq_to_ray = FLT_MAX,
      };
      BKE_pbvh_find_nearest_to_ray(
          ss->pbvh, sculpt_find_nearest_to_ray_cb, &srd, ray_start, ray_normal, srd.original);
      if (srd.hit) {
        hit = true;
        copy_v3_v3(out, ray_normal);
        mul_v3_fl(out, srd.depth);
        add_v3_v3(out, ray_start);
      }
    }
  }

  return hit;
}

static void sculpt_brush_init_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  /* Init mtex nodes. */
  if (mtex->tex && mtex->tex->nodetree) {
    /* Has internal flag to detect it only does it once. */
    ntreeTexBeginExecTree(mtex->tex->nodetree);
  }

  /* TODO: Shouldn't really have to do this at the start of every stroke, but sculpt would need
   * some sort of notification when changes are made to the texture. */
  sculpt_update_tex(scene, sd, ss);
}

static void sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = CTX_data_active_object(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  bool is_smooth;
  bool need_mask = false;

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    need_mask = true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    need_mask = true;
  }

  view3d_operator_needs_opengl(C);
  sculpt_brush_init_tex(scene, sd, ss);

  is_smooth = sculpt_needs_connectivity_info(brush, ss, mode);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, is_smooth, need_mask);
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Restore the mesh before continuing with anchored stroke. */
  if ((brush->flag & BRUSH_ANCHORED) ||
      ((brush->sculpt_tool == SCULPT_TOOL_GRAB ||
        brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM ||
        brush->sculpt_tool == SCULPT_TOOL_CLOTH) &&
       BKE_brush_use_size_pressure(brush)) ||
      (brush->flag & BRUSH_DRAG_DOT)) {
    paint_mesh_restore_co(sd, ob);
  }
}

/* Copy the PBVH bounding box into the object's bounding box. */
void SCULPT_update_object_bounding_box(Object *ob)
{
  if (ob->runtime.bb) {
    float bb_min[3], bb_max[3];

    BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
    BKE_boundbox_init_from_minmax(ob->runtime.bb, bb_min, bb_max);
  }
}

static void sculpt_flush_update_step(bContext *C, SculptUpdateType update_flags)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  MultiresModifierData *mmd = ss->multires;
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (rv3d) {
    /* Mark for faster 3D viewport redraws. */
    rv3d->rflag |= RV3D_PAINTING;
  }

  if (mmd != NULL) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  /* Only current viewport matters, slower update for all viewports will
   * be done in sculpt_flush_update_done. */
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    /* Slow update with full dependency graph update and all that comes with it.
     * Needed when there are modifiers or full shading in the 3D viewport. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }
  else {
    /* Fast path where we just update the BVH nodes that changed, and redraw
     * only the part of the 3D viewport where changes happened. */
    rcti r;

    if (update_flags & SCULPT_UPDATE_COORDS) {
      BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
      /* Update the object's bounding box too so that the object
       * doesn't get incorrectly clipped during drawing in
       * draw_mesh_object(). [#33790] */
      SCULPT_update_object_bounding_box(ob);
    }

    if (SCULPT_get_redraw_rect(region, CTX_wm_region_view3d(C), ob, &r)) {
      if (ss->cache) {
        ss->cache->current_r = r;
      }

      /* previous is not set in the current cache else
       * the partial rect will always grow */
      sculpt_extend_redraw_rect_previous(ob, &r);

      r.xmin += region->winrct.xmin - 2;
      r.xmax += region->winrct.xmin + 2;
      r.ymin += region->winrct.ymin - 2;
      r.ymax += region->winrct.ymin + 2;
      ED_region_tag_redraw_partial(region, &r, true);
    }
  }
}

static void sculpt_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags)
{
  /* After we are done drawing the stroke, check if we need to do a more
   * expensive depsgraph tag to update geometry. */
  wmWindowManager *wm = CTX_wm_manager(C);
  View3D *current_v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = ob->data;

  /* Always needed for linked duplicates. */
  bool need_tag = (ID_REAL_USERS(&mesh->id) > 1);

  if (rv3d) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    bScreen *screen = WM_window_get_active_screen(win);
    for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      SpaceLink *sl = sa->spacedata.first;
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        if (v3d != current_v3d) {
          need_tag |= !BKE_sculptsession_use_pbvh_draw(ob, v3d);
        }

        /* Tag all 3D viewports for redraw now that we are done. Others
         * viewports did not get a full redraw, and anti-aliasing for the
         * current viewport was deactivated. */
        for (ARegion *region = sa->regionbase.first; region; region = region->next) {
          if (region->regiontype == RGN_TYPE_WINDOW) {
            ED_region_tag_redraw(region);
          }
        }
      }
    }
  }

  if (update_flags & SCULPT_UPDATE_COORDS) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);
  }

  if (update_flags & SCULPT_UPDATE_MASK) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BKE_pbvh_bmesh_after_stroke(ss->pbvh);
  }

  /* Optimization: if there is locked key and active modifiers present in */
  /* the stack, keyblock is updating at each step. otherwise we could update */
  /* keyblock only when stroke is finished. */
  if (ss->shapekey_active && !ss->deform_modifiers_active) {
    sculpt_update_keyblock(ob);
  }

  if (need_tag) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0). */
static bool over_mesh(bContext *C, struct wmOperator *UNUSED(op), float x, float y)
{
  float mouse[2], co[3];

  mouse[0] = x;
  mouse[1] = y;

  return SCULPT_stroke_get_location(C, co, mouse);
}

static bool sculpt_stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  /* Don't start the stroke until mouse goes over the mesh.
   * note: mouse will only be null when re-executing the saved stroke.
   * We have exception for 'exec' strokes since they may not set 'mouse',
   * only 'location', see: T52195. */
  if (((op->flag & OP_IS_INVOKE) == 0) || (mouse == NULL) ||
      over_mesh(C, op, mouse[0], mouse[1])) {
    Object *ob = CTX_data_active_object(C);
    SculptSession *ss = ob->sculpt;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

    ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

    sculpt_update_cache_invariants(C, sd, ss, op, mouse);

    SCULPT_undo_push_begin(sculpt_tool_name(sd));

    return true;
  }
  else {
    return false;
  }
}

static void sculpt_stroke_update_step(bContext *C,
                                      struct PaintStroke *UNUSED(stroke),
                                      PointerRNA *itemptr)
{
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  sculpt_stroke_modifiers_check(C, ob, brush);
  sculpt_update_cache_variants(C, sd, ob, itemptr);
  sculpt_restore_mesh(sd, ob);

  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    float object_space_constant_detail = 1.0f / (sd->constant_detail * mat4_to_scale(ob->obmat));
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail);
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh, ss->cache->radius * sd->detail_percent / 100.0f);
  }
  else {
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh,
                                   (ss->cache->radius / ss->cache->dyntopo_pixel_radius) *
                                       (float)(sd->detail_size * U.pixelsize) / 0.4f);
  }

  if (sculpt_stroke_is_dynamic_topology(ss, brush)) {
    do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
  }

  do_symmetrical_brush_actions(sd, ob, do_brush_action, ups);
  sculpt_combine_proxies(sd, ob);

  /* Hack to fix noise texture tearing mesh. */
  sculpt_fix_noise_tear(sd, ob);

  /* TODO(sergey): This is not really needed for the solid shading,
   * which does use pBVH drawing anyway, but texture and wireframe
   * requires this.
   *
   * Could be optimized later, but currently don't think it's so
   * much common scenario.
   *
   * Same applies to the DEG_id_tag_update() invoked from
   * sculpt_flush_update_step().
   */
  if (ss->deform_modifiers_active) {
    sculpt_flush_stroke_deform(sd, ob, sculpt_tool_is_proxy_used(brush->sculpt_tool));
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }

  ss->cache->first_time = false;
  copy_v3_v3(ss->cache->true_last_location, ss->cache->true_location);

  /* Cleanup. */
  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    sculpt_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  else {
    sculpt_flush_update_step(C, SCULPT_UPDATE_COORDS);
  }
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (mtex->tex && mtex->tex->nodetree) {
    ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
  }
}

static void sculpt_stroke_done(const bContext *C, struct PaintStroke *UNUSED(stroke))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Finished. */
  if (ss->cache) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    Brush *brush = BKE_paint_brush(&sd->paint);
    BLI_assert(brush == ss->cache->brush); /* const, so we shouldn't change. */
    ups->draw_inverted = false;

    sculpt_stroke_modifiers_check(C, ob, brush);

    /* Alt-Smooth. */
    if (ss->cache->alt_smooth) {
      if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
        brush->mask_tool = ss->cache->saved_mask_brush_tool;
      }
      else if (ELEM(brush->sculpt_tool, SCULPT_TOOL_SLIDE_RELAX, SCULPT_TOOL_DRAW_FACE_SETS)) {
        /* Do nothing. */
      }
      else {
        BKE_brush_size_set(scene, brush, ss->cache->saved_smooth_size);
        brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, ss->cache->saved_active_brush_name);
        if (brush) {
          BKE_paint_brush_set(&sd->paint, brush);
        }
      }
    }

    if (sculpt_automasking_enabled(ss, brush)) {
      sculpt_automasking_end(ob);
    }

    SCULPT_cache_free(ss->cache);
    ss->cache = NULL;

    SCULPT_undo_push_end();

    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      sculpt_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    }
    else {
      sculpt_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }

  sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  struct PaintStroke *stroke;
  int ignore_background_click;
  int retval;

  sculpt_brush_stroke_init(C, op);

  stroke = paint_stroke_new(C,
                            op,
                            SCULPT_stroke_get_location,
                            sculpt_stroke_test_start,
                            sculpt_stroke_update_step,
                            NULL,
                            sculpt_stroke_done,
                            event->type);

  op->customdata = stroke;

  /* For tablet rotation. */
  ignore_background_click = RNA_boolean_get(op->ptr, "ignore_background_click");

  if (ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
    paint_stroke_free(C, op);
    return OPERATOR_PASS_THROUGH;
  }

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op);
    return OPERATOR_FINISHED;
  }
  /* Add modal handler. */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
  sculpt_brush_stroke_init(C, op);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    sculpt_stroke_test_start,
                                    sculpt_stroke_update_step,
                                    NULL,
                                    sculpt_stroke_done,
                                    0);

  /* Frees op->customdata. */
  paint_stroke_exec(C, op);

  return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  /* XXX Canceling strokes that way does not work with dynamic topology,
   *     user will have to do real undo for now. See T46456. */
  if (ss->cache && !sculpt_stroke_is_dynamic_topology(ss, brush)) {
    paint_mesh_restore_co(sd, ob);
  }

  paint_stroke_cancel(C, op);

  if (ss->cache) {
    SCULPT_cache_free(ss->cache);
    ss->cache = NULL;
  }

  sculpt_brush_exit_tex(sd);
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt";
  ot->idname = "SCULPT_OT_brush_stroke";
  ot->description = "Sculpt a stroke into the geometry";

  /* API callbacks. */
  ot->invoke = sculpt_brush_stroke_invoke;
  ot->modal = paint_stroke_modal;
  ot->exec = sculpt_brush_stroke_exec;
  ot->poll = SCULPT_poll;
  ot->cancel = sculpt_brush_stroke_cancel;

  /* Flags (sculpt does own undo? (ton)). */
  ot->flag = OPTYPE_BLOCKING;

  /* Properties. */

  paint_stroke_operator_properties(ot);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  0,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");
}

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush). */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator *UNUSED(op))
{
  SculptSession *ss = CTX_data_active_object(C)->sculpt;

  if (ss) {
    if (ss->layer_co) {
      MEM_freeN(ss->layer_co);
    }
    ss->layer_co = NULL;
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Persistent Base";
  ot->idname = "SCULPT_OT_set_persistent_base";
  ot->description = "Reset the copy of the mesh that is being sculpted on";

  /* API callbacks. */
  ot->exec = sculpt_set_persistent_base_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Dynamic Topology **************************/

static void sculpt_dynamic_topology_triangulate(BMesh *bm)
{
  if (bm->totloop != bm->totface * 3) {
    BM_mesh_triangulate(
        bm, MOD_TRIANGULATE_QUAD_BEAUTY, MOD_TRIANGULATE_NGON_EARCLIP, 4, false, NULL, NULL, NULL);
  }
}

void sculpt_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = NULL;
  }

  if (ss->pmap) {
    MEM_freeN(ss->pmap);
    ss->pmap = NULL;
  }

  if (ss->pmap_mem) {
    MEM_freeN(ss->pmap_mem);
    ss->pmap_mem = NULL;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void sculpt_dyntopo_node_layers_add(SculptSession *ss)
{
  int cd_node_layer_index;

  char layer_id[] = "_dyntopo_node_id";

  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
  if (cd_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_INT, layer_id);
    cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
  }

  ss->cd_vert_node_offset = CustomData_get_n_offset(
      &ss->bm->vdata,
      CD_PROP_INT,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT));

  ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;

  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
  if (cd_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->pdata, CD_PROP_INT, layer_id);
    cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
  }

  ss->cd_face_node_offset = CustomData_get_n_offset(
      &ss->bm->pdata,
      CD_PROP_INT,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT));

  ss->bm->pdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;
}

static void sculpt_dynamic_topology_enable_ex(Main *bmain,
                                              Depsgraph *depsgraph,
                                              Scene *scene,
                                              Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  sculpt_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove [#36280]. */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh. */
  ss->bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));

  BM_mesh_bm_from_me(ss->bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                         .use_shapekey = true,
                         .active_shapekey = ob->shapenr,
                     }));
  sculpt_dynamic_topology_triangulate(ss->bm);
  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
  sculpt_dyntopo_node_layers_add(ss);
  /* Make sure the data for existing faces are initialized. */
  if (me->totpoly != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Enable logging for undo/redo. */
  ss->bm_log = BM_log_create(ss->bm);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void sculpt_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  sculpt_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data. */
    CustomData_free(&me->vdata, me->totvert);
    CustomData_free(&me->edata, me->totedge);
    CustomData_free(&me->fdata, me->totface);
    CustomData_free(&me->ldata, me->totloop);
    CustomData_free(&me->pdata, me->totpoly);

    /* Copy over stored custom data. */
    me->totvert = unode->geom_totvert;
    me->totloop = unode->geom_totloop;
    me->totpoly = unode->geom_totpoly;
    me->totedge = unode->geom_totedge;
    me->totface = 0;
    CustomData_copy(
        &unode->geom_vdata, &me->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, unode->geom_totvert);
    CustomData_copy(
        &unode->geom_edata, &me->edata, CD_MASK_MESH.emask, CD_DUPLICATE, unode->geom_totedge);
    CustomData_copy(
        &unode->geom_ldata, &me->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, unode->geom_totloop);
    CustomData_copy(
        &unode->geom_pdata, &me->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, unode->geom_totpoly);

    BKE_mesh_update_customdata_pointers(me, false);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Typically valid but with global-undo they can be NULL. [#36234] */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = NULL;
  }
  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = NULL;
  }

  BKE_particlesystem_reset_all(ob);
  BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void sculpt_dynamic_topology_disable(bContext *C, SculptUndoNode *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  sculpt_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

static void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                                      Depsgraph *depsgraph,
                                                      Scene *scene,
                                                      Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm) {
    SCULPT_undo_push_begin("Dynamic topology disable");
    SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
    sculpt_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, NULL);
    SCULPT_undo_push_end();
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == NULL) {
    SCULPT_undo_push_begin("Dynamic topology enable");
    sculpt_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
    SCULPT_undo_push_end();
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  WM_cursor_wait(true);

  if (ss->bm) {
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

  return OPERATOR_FINISHED;
}

enum eDynTopoWarnFlag {
  DYNTOPO_WARN_VDATA = (1 << 0),
  DYNTOPO_WARN_EDATA = (1 << 1),
  DYNTOPO_WARN_LDATA = (1 << 2),
  DYNTOPO_WARN_MODIFIER = (1 << 3),
};

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Vertex Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & DYNTOPO_WARN_MODIFIER) {
    const char *msg_error = TIP_("Generative Modifiers Detected!");
    const char *msg = TIP_(
        "Keeping the modifiers will increase polycount when returning to object mode");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, NULL);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static enum eDynTopoWarnFlag sculpt_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = ob->data;
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = 0;

  BLI_assert(ss->bm == NULL);
  UNUSED_VARS_NDEBUG(ss);

  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (!ELEM(i, CD_MVERT, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX)) {
      if (CustomData_has_layer(&me->vdata, i)) {
        flag |= DYNTOPO_WARN_VDATA;
      }
      if (CustomData_has_layer(&me->edata, i)) {
        flag |= DYNTOPO_WARN_EDATA;
      }
      if (CustomData_has_layer(&me->ldata, i)) {
        flag |= DYNTOPO_WARN_LDATA;
      }
    }
  }

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
      if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == eModifierTypeType_Constructive) {
        flag |= DYNTOPO_WARN_MODIFIER;
        break;
      }
    }
  }

  return flag;
}

static int sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = sculpt_dynamic_topology_check(scene, ob);

    if (flag) {
      /* The mesh has customdata that will be lost, let the user confirm this is OK. */
      return dyntopo_warning_popup(C, op->type, flag);
    }
  }

  return sculpt_dynamic_topology_toggle_exec(C, op);
}

static void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Dynamic Topology Toggle";
  ot->idname = "SCULPT_OT_dynamic_topology_toggle";
  ot->description = "Dynamic topology alters the mesh topology while sculpting";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************* SCULPT_OT_optimize *************************/

static int sculpt_optimize_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  sculpt_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static bool sculpt_and_dynamic_topology_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  return SCULPT_mode_poll(C) && ob->sculpt->bm;
}

/* The BVH gets less optimal more quickly with dynamic topology than
 * regular sculpting. There is no doubt more clever stuff we can do to
 * optimize it on the fly, but for now this gives the user a nicer way
 * to recalculate it than toggling modes. */
static void SCULPT_OT_optimize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Rebuild BVH";
  ot->idname = "SCULPT_OT_optimize";
  ot->description = "Recalculate the sculpt BVH to improve performance";

  /* API callbacks. */
  ot->exec = sculpt_optimize_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* Dynamic topology symmetrize ********************/

static bool sculpt_no_multires_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (SCULPT_mode_poll(C) && ob->sculpt && ob->sculpt->pbvh) {
    return BKE_pbvh_type(ob->sculpt->pbvh) != PBVH_GRIDS;
  }
  return false;
}

static int sculpt_symmetrize_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);
  const Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH:
      /* Dyntopo Symmetrize. */

      /* To simplify undo for symmetrize, all BMesh elements are logged
       * as deleted, then after symmetrize operation all BMesh elements
       * are logged as added (as opposed to attempting to store just the
       * parts that symmetrize modifies). */
      SCULPT_undo_push_begin("Dynamic topology symmetrize");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);
      BM_log_before_all_removed(ss->bm, ss->bm_log);

      BM_mesh_toolflags_set(ss->bm, true);

      /* Symmetrize and re-triangulate. */
      BMO_op_callf(ss->bm,
                   (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                   "symmetrize input=%avef direction=%i  dist=%f",
                   sd->symmetrize_direction,
                   0.00001f);
      sculpt_dynamic_topology_triangulate(ss->bm);

      /* Bisect operator flags edges (keep tags clean for edge queue). */
      BM_mesh_elem_hflag_disable_all(ss->bm, BM_EDGE, BM_ELEM_TAG, false);

      BM_mesh_toolflags_set(ss->bm, false);

      /* Finish undo. */
      BM_log_all_added(ss->bm, ss->bm_log);
      SCULPT_undo_push_end();

      break;
    case PBVH_FACES:
      /* Mesh Symmetrize. */
      ED_sculpt_undo_geometry_begin(ob, "mesh symmetrize");
      Mesh *mesh = ob->data;
      Mesh *mesh_mirror;
      MirrorModifierData mmd = {{0}};
      int axis = 0;
      mmd.flag = 0;
      mmd.tolerance = 0.005f;
      switch (sd->symmetrize_direction) {
        case BMO_SYMMETRIZE_NEGATIVE_X:
          axis = 0;
          mmd.flag |= MOD_MIR_AXIS_X | MOD_MIR_BISECT_AXIS_X | MOD_MIR_BISECT_FLIP_AXIS_X;
          break;
        case BMO_SYMMETRIZE_NEGATIVE_Y:
          axis = 1;
          mmd.flag |= MOD_MIR_AXIS_Y | MOD_MIR_BISECT_AXIS_Y | MOD_MIR_BISECT_FLIP_AXIS_Y;
          break;
        case BMO_SYMMETRIZE_NEGATIVE_Z:
          axis = 2;
          mmd.flag |= MOD_MIR_AXIS_Z | MOD_MIR_BISECT_AXIS_Z | MOD_MIR_BISECT_FLIP_AXIS_Z;
          break;
        case BMO_SYMMETRIZE_POSITIVE_X:
          axis = 0;
          mmd.flag |= MOD_MIR_AXIS_X | MOD_MIR_BISECT_AXIS_X;
          break;
        case BMO_SYMMETRIZE_POSITIVE_Y:
          axis = 1;
          mmd.flag |= MOD_MIR_AXIS_Y | MOD_MIR_BISECT_AXIS_Y;
          break;
        case BMO_SYMMETRIZE_POSITIVE_Z:
          axis = 2;
          mmd.flag |= MOD_MIR_AXIS_Z | MOD_MIR_BISECT_AXIS_Z;
          break;
      }
      mesh_mirror = BKE_mesh_mirror_apply_mirror_on_axis(&mmd, NULL, ob, mesh, axis);
      if (mesh_mirror) {
        BKE_mesh_nomain_to_mesh(mesh_mirror, mesh, ob, &CD_MASK_MESH, true);
      }
      ED_sculpt_undo_geometry_end(ob);
      BKE_mesh_calc_normals(ob->data);
      BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

      break;
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  /* Redraw. */
  sculpt_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_symmetrize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Symmetrize";
  ot->idname = "SCULPT_OT_symmetrize";
  ot->description = "Symmetrize the topology modifications";

  /* API callbacks. */
  ot->exec = sculpt_symmetrize_exec;
  ot->poll = sculpt_no_multires_poll;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Create persistent sculpt mode data. */
  BKE_sculpt_toolsettings_data_ensure(scene);

  ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");
  ob->sculpt->mode_type = OB_MODE_SCULPT;
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false);
}

static int ed_object_sculptmode_flush_recalc_flag(Scene *scene,
                                                  Object *ob,
                                                  MultiresModifierData *mmd)
{
  int flush_recalc = 0;
  /* Multires in sculpt mode could have different from object mode subdivision level. */
  flush_recalc |= mmd && BKE_multires_sculpt_level_get(mmd) != mmd->lvl;
  /* If object has got active modifiers, it's dm could be different in sculpt mode.  */
  flush_recalc |= sculpt_has_active_modifiers(scene, ob);
  return flush_recalc;
}

void ED_object_sculptmode_enter_ex(Main *bmain,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   const bool force_dyntopo,
                                   ReportList *reports)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Enter sculpt mode. */
  ob->mode |= mode_flag;

  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);

  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);

  if (flush_recalc) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  /* Create sculpt mode session data. */
  if (ob->sculpt) {
    BKE_sculptsession_free(ob);
  }

  /* Make sure derived final from original object does not reference possibly
   * freed memory. */
  BKE_object_free_derived_caches(ob);

  sculpt_init_session(depsgraph, scene, ob);

  /* Mask layer is required. */
  if (mmd) {
    /* XXX, we could attempt to support adding mask data mid-sculpt mode (with multi-res)
     * but this ends up being quite tricky (and slow). */
    BKE_sculpt_mask_layers_ensure(ob, mmd);
  }

  if (!(fabsf(ob->scale[0] - ob->scale[1]) < 1e-4f &&
        fabsf(ob->scale[1] - ob->scale[2]) < 1e-4f)) {
    BKE_report(
        reports, RPT_WARNING, "Object has non-uniform scale, sculpting may be unpredictable");
  }
  else if (is_negative_m4(ob->obmat)) {
    BKE_report(reports, RPT_WARNING, "Object has negative scale, sculpting may be unpredictable");
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, PAINT_MODE_SCULPT);
  BKE_paint_init(bmain, scene, PAINT_MODE_SCULPT, PAINT_CURSOR_SCULPT);

  paint_cursor_start_explicit(paint, bmain->wm.first, SCULPT_poll_view3d);

  /* Check dynamic-topology flag; re-enter dynamic-topology mode when changing modes,
   * As long as no data was added that is not supported. */
  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    const char *message_unsupported = NULL;
    if (me->totloop != me->totpoly * 3) {
      message_unsupported = TIP_("non-triangle face");
    }
    else if (mmd != NULL) {
      message_unsupported = TIP_("multi-res modifier");
    }
    else {
      enum eDynTopoWarnFlag flag = sculpt_dynamic_topology_check(scene, ob);
      if (flag == 0) {
        /* pass */
      }
      else if (flag & DYNTOPO_WARN_VDATA) {
        message_unsupported = TIP_("vertex data");
      }
      else if (flag & DYNTOPO_WARN_EDATA) {
        message_unsupported = TIP_("edge data");
      }
      else if (flag & DYNTOPO_WARN_LDATA) {
        message_unsupported = TIP_("face data");
      }
      else if (flag & DYNTOPO_WARN_MODIFIER) {
        message_unsupported = TIP_("constructive modifier");
      }
      else {
        BLI_assert(0);
      }
    }

    if ((message_unsupported == NULL) || force_dyntopo) {
      /* Needed because we may be entering this mode before the undo system loads. */
      wmWindowManager *wm = bmain->wm.first;
      bool has_undo = wm->undo_stack != NULL;
      /* Undo push is needed to prevent memory leak. */
      if (has_undo) {
        SCULPT_undo_push_begin("Dynamic topology enable");
      }
      sculpt_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
      if (has_undo) {
        SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
        SCULPT_undo_push_end();
      }
    }
    else {
      BKE_reportf(
          reports, RPT_WARNING, "Dynamic Topology found: %s, disabled", message_unsupported);
      me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
    }
  }

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_enter(struct bContext *C, Depsgraph *depsgraph, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, reports);
}

void ED_object_sculptmode_exit_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  multires_flush_sculpt_updates(ob);

  /* Not needed for now. */
#if 0
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);
#endif

  /* Always for now, so leaving sculpt mode always ensures scene is in
   * a consistent state. */
  if (true || /* flush_recalc || */ (ob->sculpt && ob->sculpt->bm)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dynamic topology must be disabled before exiting sculpt
     * mode to ensure the undo stack stays in a consistent
     * state. */
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);

    /* Store so we know to re-enable when entering sculpt mode. */
    me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
  }

  /* Leave sculpt mode. */
  ob->mode &= ~mode_flag;

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_exit(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
}

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
  }
  else {
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, op->reports);
    BKE_paint_toolslots_brush_validate(bmain, &ts->sculpt->paint);

    if (ob->mode & mode_flag) {
      Mesh *me = ob->data;
      /* Dyntopo add's it's own undo step. */
      if ((me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see T71564. */
        wmWindowManager *wm = CTX_wm_manager(C);
        if (wm->op_undo_depth <= 1) {
          SCULPT_undo_push_begin(op->type->name);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt Mode";
  ot->idname = "SCULPT_OT_sculptmode_toggle";
  ot->description = "Toggle sculpt mode in 3D view";

  /* API callbacks. */
  ot->exec = sculpt_mode_toggle_exec;
  ot->poll = ED_operator_object_active_editable_mesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool sculpt_and_constant_or_manual_detail_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  return SCULPT_mode_poll(C) && ob->sculpt->bm &&
         (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL));
}

static int sculpt_detail_flood_fill_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  float size;
  float bb_min[3], bb_max[3], center[3], dim[3];
  int totnodes;
  PBVHNode **nodes;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnodes);

  if (!totnodes) {
    return OPERATOR_CANCELLED;
  }

  for (int i = 0; i < totnodes; i++) {
    BKE_pbvh_node_mark_topology_update(nodes[i]);
  }
  /* Get the bounding box, it's center and size. */
  BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
  add_v3_v3v3(center, bb_min, bb_max);
  mul_v3_fl(center, 0.5f);
  sub_v3_v3v3(dim, bb_max, bb_min);
  size = max_fff(dim[0], dim[1], dim[2]);

  /* Update topology size. */
  float object_space_constant_detail = 1.0f / (sd->constant_detail * mat4_to_scale(ob->obmat));
  BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail);

  SCULPT_undo_push_begin("Dynamic topology flood fill");
  SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_COORDS);

  while (BKE_pbvh_bmesh_update_topology(
      ss->pbvh, PBVH_Collapse | PBVH_Subdivide, center, NULL, size, false, false)) {
    for (int i = 0; i < totnodes; i++) {
      BKE_pbvh_node_mark_topology_update(nodes[i]);
    }
  }

  MEM_SAFE_FREE(nodes);
  SCULPT_undo_push_end();

  /* Force rebuild of pbvh for better BB placement. */
  sculpt_pbvh_clear(ob);
  /* Redraw. */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_detail_flood_fill(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Detail Flood Fill";
  ot->idname = "SCULPT_OT_detail_flood_fill";
  ot->description = "Flood fill the mesh with the selected detail setting";

  /* API callbacks. */
  ot->exec = sculpt_detail_flood_fill_exec;
  ot->poll = sculpt_and_constant_or_manual_detail_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

typedef enum eSculptSampleDetailModeTypes {
  SAMPLE_DETAIL_DYNTOPO = 0,
  SAMPLE_DETAIL_VOXEL = 1,
} eSculptSampleDetailModeTypes;

static EnumPropertyItem prop_sculpt_sample_detail_mode_types[] = {
    {SAMPLE_DETAIL_DYNTOPO, "DYNTOPO", 0, "Dyntopo", "Sample dyntopo detail"},
    {SAMPLE_DETAIL_VOXEL, "VOXEL", 0, "Voxel", "Sample mesh voxel size"},
    {0, NULL, 0, NULL, NULL},
};

static void sample_detail_voxel(bContext *C, ViewContext *vc, int mx, int my)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = vc->obact;
  Mesh *mesh = ob->data;

  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  SCULPT_vertex_random_access_init(ss);

  /* Update the active vertex. */
  float mouse[2] = {mx, my};
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false);

  /* Average the edge length of the connected edges to the active vertex. */
  int active_vertex = SCULPT_active_vertex_get(ss);
  const float *active_vertex_co = SCULPT_active_vertex_co_get(ss);
  float edge_length = 0.0f;
  int tot = 0;
  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, active_vertex, ni)
  {
    edge_length += len_v3v3(active_vertex_co, SCULPT_vertex_co_get(ss, ni.index));
    tot += 1;
  }
  sculpt_vertex_neighbors_iter_end(ni);
  if (tot > 0) {
    mesh->remesh_voxel_size = edge_length / (float)tot;
  }
}

static void sample_detail_dyntopo(bContext *C, ViewContext *vc, ARegion *region, int mx, int my)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = vc->obact;
  Brush *brush = BKE_paint_brush(&sd->paint);

  sculpt_stroke_modifiers_check(C, ob, brush);

  float mouse[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};
  float ray_start[3], ray_end[3], ray_normal[3];
  float depth = sculpt_raycast_init(vc, mouse, ray_start, ray_end, ray_normal, false);

  SculptDetailRaycastData srd;
  srd.hit = 0;
  srd.ray_start = ray_start;
  srd.depth = depth;
  srd.edge_length = 0.0f;
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

  BKE_pbvh_raycast(ob->sculpt->pbvh, sculpt_raycast_detail_cb, &srd, ray_start, ray_normal, false);

  if (srd.hit && srd.edge_length > 0.0f) {
    /* Convert edge length to world space detail resolution. */
    sd->constant_detail = 1 / (srd.edge_length * mat4_to_scale(ob->obmat));
  }
}

static int sample_detail(bContext *C, int mx, int my, int mode)
{
  /* Find 3D view to pick from. */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *sa = BKE_screen_find_area_xy(screen, SPACE_VIEW3D, mx, my);
  ARegion *region = (sa) ? BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my) : NULL;
  if (region == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Set context to 3D view. */
  ScrArea *prev_sa = CTX_wm_area(C);
  ARegion *prev_ar = CTX_wm_region(C);
  CTX_wm_area_set(C, sa);
  CTX_wm_region_set(C, region);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  Object *ob = vc.obact;
  SculptSession *ss = ob->sculpt;

  if (!ss->pbvh) {
    return OPERATOR_CANCELLED;
  }

  /* Pick sample detail. */
  switch (mode) {
    case SAMPLE_DETAIL_DYNTOPO:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
        CTX_wm_area_set(C, prev_sa);
        CTX_wm_region_set(C, prev_ar);
        return OPERATOR_CANCELLED;
      }
      sample_detail_dyntopo(C, &vc, region, mx, my);
      break;
    case SAMPLE_DETAIL_VOXEL:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
        CTX_wm_area_set(C, prev_sa);
        CTX_wm_region_set(C, prev_ar);
        return OPERATOR_CANCELLED;
      }
      sample_detail_voxel(C, &vc, mx, my);
      break;
  }

  /* Restore context. */
  CTX_wm_area_set(C, prev_sa);
  CTX_wm_region_set(C, prev_ar);

  return OPERATOR_FINISHED;
}

static int sculpt_sample_detail_size_exec(bContext *C, wmOperator *op)
{
  int ss_co[2];
  RNA_int_get_array(op->ptr, "location", ss_co);
  int mode = RNA_enum_get(op->ptr, "mode");
  return sample_detail(C, ss_co[0], ss_co[1], mode);
}

static int sculpt_sample_detail_size_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
  ED_workspace_status_text(C, TIP_("Click on the mesh to set the detail"));
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EYEDROPPER);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_detail_size_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        int ss_co[2] = {event->x, event->y};

        int mode = RNA_enum_get(op->ptr, "mode");
        sample_detail(C, ss_co[0], ss_co[1], mode);

        RNA_int_set_array(op->ptr, "location", ss_co);
        WM_cursor_modal_restore(CTX_wm_window(C));
        ED_workspace_status_text(C, NULL);
        WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

        return OPERATOR_FINISHED;
      }
      break;

    case RIGHTMOUSE: {
      WM_cursor_modal_restore(CTX_wm_window(C));
      ED_workspace_status_text(C, NULL);

      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_sample_detail_size(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sample Detail Size";
  ot->idname = "SCULPT_OT_sample_detail_size";
  ot->description = "Sample the mesh detail on clicked point";

  /* API callbacks. */
  ot->invoke = sculpt_sample_detail_size_invoke;
  ot->exec = sculpt_sample_detail_size_exec;
  ot->modal = sculpt_sample_detail_size_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int_array(ot->srna,
                    "location",
                    2,
                    NULL,
                    0,
                    SHRT_MAX,
                    "Location",
                    "Screen Coordinates of sampling",
                    0,
                    SHRT_MAX);
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_sample_detail_mode_types,
               SAMPLE_DETAIL_DYNTOPO,
               "Detail Mode",
               "Target sculpting workflow that is going to use the sampled size");
}

/* Dynamic-topology detail size.
 *
 * This should be improved further, perhaps by showing a triangle
 * grid rather than brush alpha. */
static void set_brush_rc_props(PointerRNA *ptr, const char *prop)
{
  char *path = BLI_sprintfN("tool_settings.sculpt.brush.%s", prop);
  RNA_string_set(ptr, "data_path_primary", path);
  MEM_freeN(path);
}

static int sculpt_set_detail_size_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_radial_control", true);

  WM_operator_properties_create_ptr(&props_ptr, ot);

  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(
        &props_ptr, "data_path_primary", "tool_settings.sculpt.constant_detail_resolution");
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_percent");
  }
  else {
    set_brush_rc_props(&props_ptr, "detail_size");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_size");
  }

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);

  WM_operator_properties_free(&props_ptr);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_detail_size(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Detail Size";
  ot->idname = "SCULPT_OT_set_detail_size";
  ot->description =
      "Set the mesh detail (either relative or constant one, depending on current dyntopo mode)";

  /* API callbacks. */
  ot->exec = sculpt_set_detail_size_exec;
  ot->poll = sculpt_and_dynamic_topology_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
static void filter_cache_init_task_cb(void *__restrict userdata,
                                      const int i,
                                      const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  PBVHNode *node = data->nodes[i];

  SCULPT_undo_push_node(data->ob, node, SCULPT_UNDO_COORDS);
}

static void sculpt_filter_cache_init(Object *ob, Sculpt *sd)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  ss->filter_cache = MEM_callocN(sizeof(FilterCache), "filter cache");

  ss->filter_cache->random_seed = rand();

  float center[3] = {0.0f};
  SculptSearchSphereData search_data = {
      .original = true,
      .center = center,
      .radius_squared = FLT_MAX,
      .ignore_fully_masked = true,

  };
  BKE_pbvh_search_gather(pbvh,
                         SCULPT_search_sphere_cb,
                         &search_data,
                         &ss->filter_cache->nodes,
                         &ss->filter_cache->totnode);

  for (int i = 0; i < ss->filter_cache->totnode; i++) {
    BKE_pbvh_node_mark_normals_update(ss->filter_cache->nodes[i]);
  }

  /* mesh->runtime.subdiv_ccg is not available. Updating of the normals is done during drawing.
   * Filters can't use normals in multires. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    BKE_pbvh_update_normals(ss->pbvh, NULL);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(
      0, ss->filter_cache->totnode, &data, filter_cache_init_task_cb, &settings);
}

static void sculpt_filter_cache_free(SculptSession *ss)
{
  if (ss->filter_cache->nodes) {
    MEM_freeN(ss->filter_cache->nodes);
  }
  if (ss->filter_cache->mask_update_it) {
    MEM_freeN(ss->filter_cache->mask_update_it);
  }
  if (ss->filter_cache->prev_mask) {
    MEM_freeN(ss->filter_cache->prev_mask);
  }
  if (ss->filter_cache->normal_factor) {
    MEM_freeN(ss->filter_cache->normal_factor);
  }
  if (ss->filter_cache->prev_face_set) {
    MEM_freeN(ss->filter_cache->prev_face_set);
  }
  if (ss->filter_cache->automask) {
    MEM_freeN(ss->filter_cache->automask);
  }

  MEM_freeN(ss->filter_cache);
  ss->filter_cache = NULL;
}

typedef enum eSculptMeshFilterTypes {
  MESH_FILTER_SMOOTH = 0,
  MESH_FILTER_SCALE = 1,
  MESH_FILTER_INFLATE = 2,
  MESH_FILTER_SPHERE = 3,
  MESH_FILTER_RANDOM = 4,
  MESH_FILTER_RELAX = 5,
  MESH_FILTER_RELAX_FACE_SETS = 6,
} eSculptMeshFilterTypes;

static EnumPropertyItem prop_mesh_filter_types[] = {
    {MESH_FILTER_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth mesh"},
    {MESH_FILTER_SCALE, "SCALE", 0, "Scale", "Scale mesh"},
    {MESH_FILTER_INFLATE, "INFLATE", 0, "Inflate", "Inflate mesh"},
    {MESH_FILTER_SPHERE, "SPHERE", 0, "Sphere", "Morph into sphere"},
    {MESH_FILTER_RANDOM, "RANDOM", 0, "Random", "Randomize vertex positions"},
    {MESH_FILTER_RELAX, "RELAX", 0, "Relax", "Relax mesh"},
    {MESH_FILTER_RELAX_FACE_SETS,
     "RELAX_FACE_SETS",
     0,
     "Relax Face Sets",
     "Smooth the edges of all the Face Sets"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eMeshFilterDeformAxis {
  MESH_FILTER_DEFORM_X = 1 << 0,
  MESH_FILTER_DEFORM_Y = 1 << 1,
  MESH_FILTER_DEFORM_Z = 1 << 2,
} eMeshFilterDeformAxis;

static EnumPropertyItem prop_mesh_filter_deform_axis_items[] = {
    {MESH_FILTER_DEFORM_X, "X", 0, "X", "Deform in the X axis"},
    {MESH_FILTER_DEFORM_Y, "Y", 0, "Y", "Deform in the Y axis"},
    {MESH_FILTER_DEFORM_Z, "Z", 0, "Z", "Deform in the Z axis"},
    {0, NULL, 0, NULL, NULL},
};

static bool sculpt_mesh_filter_needs_pmap(int filter_type)
{
  return ELEM(filter_type, MESH_FILTER_SMOOTH, MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS);
}

static void mesh_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  const int filter_type = data->filter_type;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[i]);

  /* When using the relax face sets mehs filter, each 3 iterations, do a whole mesh relax to smooth
   * the contents of the Face Set. */
  /* This produces better results as the relax operation is no completely focused on the
   * boundaries. */
  const bool relax_face_sets = !(ss->filter_cache->iteration_count % 3 == 0);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float orig_co[3], val[3], avg[3], normal[3], disp[3], disp2[3], transform[3][3], final_pos[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;

    if (fade == 0.0f) {
      continue;
    }

    if (ss->filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
      if (!SCULPT_vertex_has_face_set(ss, vd.index, ss->filter_cache->active_face_set)) {
        continue;
      }
      /* Skip the edges of the face set when relaxing or smoothing. There is a relax face set
       * option to relax the boindaries independently. */
      if (filter_type == MESH_FILTER_RELAX) {
        if (!sculpt_vertex_has_unique_face_set(ss, vd.index)) {
          continue;
        }
      }
    }

    if (ELEM(filter_type, MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS)) {
      copy_v3_v3(orig_co, vd.co);
    }
    else {
      copy_v3_v3(orig_co, orig_data.co);
    }

    if (filter_type == MESH_FILTER_RELAX_FACE_SETS) {
      if (relax_face_sets == sculpt_vertex_has_unique_face_set(ss, vd.index)) {
        continue;
      }
    }

    switch (filter_type) {
      case MESH_FILTER_SMOOTH:
        CLAMP(fade, -1.0f, 1.0f);
        switch (BKE_pbvh_type(ss->pbvh)) {
          case PBVH_FACES:
            neighbor_average(ss, avg, vd.index);
            break;
          case PBVH_BMESH:
            bmesh_neighbor_average(avg, vd.bm_vert);
            break;
          case PBVH_GRIDS:
            grids_neighbor_average(ss, avg, vd.index);
            break;
        }
        sub_v3_v3v3(val, avg, orig_co);
        madd_v3_v3v3fl(val, orig_co, val, fade);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_INFLATE:
        normal_short_to_float_v3(normal, orig_data.no);
        mul_v3_v3fl(disp, normal, fade);
        break;
      case MESH_FILTER_SCALE:
        unit_m3(transform);
        scale_m3_fl(transform, 1.0f + fade);
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_SPHERE:
        normalize_v3_v3(disp, orig_co);
        if (fade > 0.0f) {
          mul_v3_v3fl(disp, disp, fade);
        }
        else {
          mul_v3_v3fl(disp, disp, -fade);
        }

        unit_m3(transform);
        if (fade > 0.0f) {
          scale_m3_fl(transform, 1.0f - fade);
        }
        else {
          scale_m3_fl(transform, 1.0f + fade);
        }
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp2, val, orig_co);

        mid_v3_v3v3(disp, disp, disp2);
        break;
      case MESH_FILTER_RANDOM: {
        normal_short_to_float_v3(normal, orig_data.no);
        /* Index is not unique for multires, so hash by vertex coordinates. */
        const uint *hash_co = (const uint *)orig_co;
        const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^
                          BLI_hash_int_2d(hash_co[2], ss->filter_cache->random_seed);
        mul_v3_fl(normal, hash * (1.0f / (float)0xFFFFFFFF) - 0.5f);
        mul_v3_v3fl(disp, normal, fade);
        break;
      }
      case MESH_FILTER_RELAX: {
        SCULPT_relax_vertex(
            ss, &vd, clamp_f(fade * ss->filter_cache->automask[vd.index], 0.0f, 1.0f), false, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
      case MESH_FILTER_RELAX_FACE_SETS: {
        SCULPT_relax_vertex(ss, &vd, clamp_f(fade, 0.0f, 1.0f), relax_face_sets, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
    }

    for (int it = 0; it < 3; it++) {
      if (!ss->filter_cache->enabled_axis[it]) {
        disp[it] = 0.0f;
      }
    }

    add_v3_v3v3(final_pos, orig_co, disp);
    copy_v3_v3(vd.co, final_pos);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

static int sculpt_mesh_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    sculpt_filter_cache_free(ss);
    SCULPT_undo_push_end();
    sculpt_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  float len = event->prevclickx - event->mval[0];
  filter_strength = filter_strength * -len * 0.001f * UI_DPI_FAC;

  SCULPT_vertex_random_access_init(ss);

  bool needs_pmap = sculpt_mesh_filter_needs_pmap(filter_type);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = filter_type,
      .filter_strength = filter_strength,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(0, ss->filter_cache->totnode, &data, mesh_filter_task_cb, &settings);

  ss->filter_cache->iteration_count++;

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    sculpt_flush_stroke_deform(sd, ob, true);
  }

  /* The relax mesh filter needs the updated normals of the modified mesh after each iteration. */
  if (ELEM(MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS)) {
    BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);
  }

  sculpt_flush_update_step(C, SCULPT_UPDATE_COORDS);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_mesh_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "type");
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  int deform_axis = RNA_enum_get(op->ptr, "deform_axis");
  if (deform_axis == 0) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "use_face_sets")) {
    /* Update the active vertex */
    float mouse[2];
    SculptCursorGeometryInfo sgi;
    mouse[0] = event->mval[0];
    mouse[1] = event->mval[1];
    SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  }

  SCULPT_vertex_random_access_init(ss);

  bool needs_pmap = sculpt_mesh_filter_needs_pmap(filter_type);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES && needs_pmap && !ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin("Mesh filter");

  sculpt_filter_cache_init(ob, sd);

  if (RNA_boolean_get(op->ptr, "use_face_sets")) {
    ss->filter_cache->active_face_set = SCULPT_vertex_face_set_get(ss,
                                                                   SCULPT_active_vertex_get(ss));
  }
  else {
    ss->filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  }

  ss->filter_cache->enabled_axis[0] = deform_axis & MESH_FILTER_DEFORM_X;
  ss->filter_cache->enabled_axis[1] = deform_axis & MESH_FILTER_DEFORM_Y;
  ss->filter_cache->enabled_axis[2] = deform_axis & MESH_FILTER_DEFORM_Z;

  if (RNA_enum_get(op->ptr, "type") == MESH_FILTER_RELAX) {
    const int totvert = SCULPT_vertex_count_get(ss);
    ss->filter_cache->automask = MEM_mallocN(totvert * sizeof(float),
                                             "Relax filter edge automask");
    for (int i = 0; i < totvert; i++) {
      ss->filter_cache->automask[i] = 1.0f;
    }
    sculpt_boundary_edges_automasking_init(ob, 1, ss->filter_cache->automask);
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_mesh_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter mesh";
  ot->idname = "SCULPT_OT_mesh_filter";
  ot->description = "Applies a filter to modify the current mesh";

  /* API callbacks. */
  ot->invoke = sculpt_mesh_filter_invoke;
  ot->modal = sculpt_mesh_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* RNA. */
  RNA_def_enum(ot->srna,
               "type",
               prop_mesh_filter_types,
               MESH_FILTER_INFLATE,
               "Filter type",
               "Operation that is going to be applied to the mesh");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter Strength", -10.0f, 10.0f);
  RNA_def_enum_flag(ot->srna,
                    "deform_axis",
                    prop_mesh_filter_deform_axis_items,
                    MESH_FILTER_DEFORM_X | MESH_FILTER_DEFORM_Y | MESH_FILTER_DEFORM_Z,
                    "Deform axis",
                    "Apply the deformation in the selected axis");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_face_sets",
                             false,
                             "Use Face Sets",
                             "Apply the filter only to the Face Mask under the cursor");
}

typedef enum eSculptMaskFilterTypes {
  MASK_FILTER_SMOOTH = 0,
  MASK_FILTER_SHARPEN = 1,
  MASK_FILTER_GROW = 2,
  MASK_FILTER_SHRINK = 3,
  MASK_FILTER_CONTRAST_INCREASE = 5,
  MASK_FILTER_CONTRAST_DECREASE = 6,
} eSculptMaskFilterTypes;

static EnumPropertyItem prop_mask_filter_types[] = {
    {MASK_FILTER_SMOOTH, "SMOOTH", 0, "Smooth Mask", "Smooth mask"},
    {MASK_FILTER_SHARPEN, "SHARPEN", 0, "Sharpen Mask", "Sharpen mask"},
    {MASK_FILTER_GROW, "GROW", 0, "Grow Mask", "Grow mask"},
    {MASK_FILTER_SHRINK, "SHRINK", 0, "Shrink Mask", "Shrink mask"},
    {MASK_FILTER_CONTRAST_INCREASE,
     "CONTRAST_INCREASE",
     0,
     "Increase contrast",
     "Increase the contrast of the paint mask"},
    {MASK_FILTER_CONTRAST_DECREASE,
     "CONTRAST_DECREASE",
     0,
     "Decrease contrast",
     "Decrease the contrast of the paint mask"},
    {0, NULL, 0, NULL, NULL},
};

static void mask_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  bool update = false;

  const int mode = data->filter_type;
  float contrast = 0.0f;

  PBVHVertexIter vd;

  if (mode == MASK_FILTER_CONTRAST_INCREASE) {
    contrast = 0.1f;
  }

  if (mode == MASK_FILTER_CONTRAST_DECREASE) {
    contrast = -0.1f;
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float delta, gain, offset, max, min;
    float prev_val = *vd.mask;
    SculptVertexNeighborIter ni;
    switch (mode) {
      case MASK_FILTER_SMOOTH:
      case MASK_FILTER_SHARPEN: {
        float val = 0.0f;

        switch (BKE_pbvh_type(ss->pbvh)) {
          case PBVH_FACES:
            val = neighbor_average_mask(ss, vd.index);
            break;
          case PBVH_BMESH:
            val = bmesh_neighbor_average_mask(vd.bm_vert, vd.cd_vert_mask_offset);
            break;
          case PBVH_GRIDS:
            val = grids_neighbor_average_mask(ss, vd.index);
            break;
        }

        val -= *vd.mask;

        if (mode == MASK_FILTER_SMOOTH) {
          *vd.mask += val;
        }
        else if (mode == MASK_FILTER_SHARPEN) {
          if (*vd.mask > 0.5f) {
            *vd.mask += 0.05f;
          }
          else {
            *vd.mask -= 0.05f;
          }
          *vd.mask += val / 2.0f;
        }
        break;
      }
      case MASK_FILTER_GROW:
        max = 0.0f;
        sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
        {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f > max) {
            max = vmask_f;
          }
        }
        sculpt_vertex_neighbors_iter_end(ni);
        *vd.mask = max;
        break;
      case MASK_FILTER_SHRINK:
        min = 1.0f;
        sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
        {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f < min) {
            min = vmask_f;
          }
        }
        sculpt_vertex_neighbors_iter_end(ni);
        *vd.mask = min;
        break;
      case MASK_FILTER_CONTRAST_INCREASE:
      case MASK_FILTER_CONTRAST_DECREASE:
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (-delta);
        }
        else {
          delta *= -1.0f;
          offset = gain * (delta);
        }
        *vd.mask = gain * (*vd.mask) + offset;
        break;
    }
    CLAMP(*vd.mask, 0.0f, 1.0f);
    if (*vd.mask != prev_val) {
      update = true;
    }
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static int sculpt_mask_filter_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int totnode;
  int filter_type = RNA_enum_get(op->ptr, "filter_type");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  SCULPT_vertex_random_access_init(ss);

  if (!ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  int num_verts = SCULPT_vertex_count_get(ss);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  SCULPT_undo_push_begin("Mask filter");

  for (int i = 0; i < totnode; i++) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);
  }

  float *prev_mask = NULL;
  int iterations = RNA_int_get(op->ptr, "iterations");

  /* Auto iteration count calculates the number of iteration based on the vertices of the mesh to
   * avoid adding an unnecessary amount of undo steps when using the operator from a shortcut.
   * One iteration per 50000 vertices in the mesh should be fine in most cases.
   * Maybe we want this to be configurable. */
  if (RNA_boolean_get(op->ptr, "auto_iteration_count")) {
    iterations = (int)(num_verts / 50000.0f) + 1;
  }

  for (int i = 0; i < iterations; i++) {
    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      prev_mask = MEM_mallocN(num_verts * sizeof(float), "prevmask");
      for (int j = 0; j < num_verts; j++) {
        prev_mask[j] = SCULPT_vertex_mask_get(ss, j);
      }
    }

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .nodes = nodes,
        .filter_type = filter_type,
        .prev_mask = prev_mask,
    };

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, mask_filter_task_cb, &settings);

    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      MEM_freeN(prev_mask);
    }
  }

  MEM_SAFE_FREE(nodes);

  SCULPT_undo_push_end();

  ED_region_tag_redraw(region);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  return OPERATOR_FINISHED;
}

static void SCULPT_OT_mask_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Filter";
  ot->idname = "SCULPT_OT_mask_filter";
  ot->description = "Applies a filter to modify the current mask";

  /* API callbacks. */
  ot->exec = sculpt_mask_filter_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* RNA. */
  RNA_def_enum(ot->srna,
               "filter_type",
               prop_mask_filter_types,
               MASK_FILTER_SMOOTH,
               "Type",
               "Filter that is going to be applied to the mask");
  RNA_def_int(ot->srna,
              "iterations",
              1,
              1,
              100,
              "Iterations",
              "Number of times that the filter is going to be applied",
              1,
              100);
  RNA_def_boolean(
      ot->srna,
      "auto_iteration_count",
      false,
      "Auto Iteration Count",
      "Use a automatic number of iterations based on the number of vertices of the sculpt");
}

static float neighbor_dirty_mask(SculptSession *ss, PBVHVertexIter *vd)
{
  int total = 0;
  float avg[3];
  zero_v3(avg);

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, vd->index, ni)
  {
    float normalized[3];
    sub_v3_v3v3(normalized, SCULPT_vertex_co_get(ss, ni.index), vd->co);
    normalize_v3(normalized);
    add_v3_v3(avg, normalized);
    total++;
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (total > 0) {
    mul_v3_fl(avg, 1.0f / total);
    float normal[3];
    if (vd->no) {
      normal_short_to_float_v3(normal, vd->no);
    }
    else {
      copy_v3_v3(normal, vd->fno);
    }
    float dot = dot_v3v3(avg, normal);
    float angle = max_ff(saacosf(dot), 0.0f);
    return angle;
  }
  return 0.0f;
}

typedef struct DirtyMaskRangeData {
  float min, max;
} DirtyMaskRangeData;

static void dirty_mask_compute_range_task_cb(void *__restrict userdata,
                                             const int i,
                                             const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  DirtyMaskRangeData *range = tls->userdata_chunk;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float dirty_mask = neighbor_dirty_mask(ss, &vd);
    range->min = min_ff(dirty_mask, range->min);
    range->max = max_ff(dirty_mask, range->max);
  }
  BKE_pbvh_vertex_iter_end;
}

static void dirty_mask_compute_range_reduce(const void *__restrict UNUSED(userdata),
                                            void *__restrict chunk_join,
                                            void *__restrict chunk)
{
  DirtyMaskRangeData *join = chunk_join;
  DirtyMaskRangeData *range = chunk;
  join->min = min_ff(range->min, join->min);
  join->max = max_ff(range->max, join->max);
}

static void dirty_mask_apply_task_cb(void *__restrict userdata,
                                     const int i,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  PBVHVertexIter vd;

  const bool dirty_only = data->dirty_mask_dirty_only;
  const float min = data->dirty_mask_min;
  const float max = data->dirty_mask_max;

  float range = max - min;
  if (range < 0.0001f) {
    range = 0.0f;
  }
  else {
    range = 1.0f / range;
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float dirty_mask = neighbor_dirty_mask(ss, &vd);
    float mask = *vd.mask + (1.0f - ((dirty_mask - min) * range));
    if (dirty_only) {
      mask = fminf(mask, 0.5f) * 2.0f;
    }
    *vd.mask = CLAMPIS(mask, 0.0f, 1.0f);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_mask(node);
}

static int sculpt_dirty_mask_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int totnode;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  SCULPT_vertex_random_access_init(ss);

  if (!ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  SCULPT_undo_push_begin("Dirty Mask");

  for (int i = 0; i < totnode; i++) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .dirty_mask_dirty_only = RNA_boolean_get(op->ptr, "dirty_only"),
  };
  DirtyMaskRangeData range = {
      .min = FLT_MAX,
      .max = -FLT_MAX,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

  settings.func_reduce = dirty_mask_compute_range_reduce;
  settings.userdata_chunk = &range;
  settings.userdata_chunk_size = sizeof(DirtyMaskRangeData);

  BKE_pbvh_parallel_range(0, totnode, &data, dirty_mask_compute_range_task_cb, &settings);
  data.dirty_mask_min = range.min;
  data.dirty_mask_max = range.max;
  BKE_pbvh_parallel_range(0, totnode, &data, dirty_mask_apply_task_cb, &settings);

  MEM_SAFE_FREE(nodes);

  BKE_pbvh_update_vertex_data(pbvh, PBVH_UpdateMask);

  SCULPT_undo_push_end();

  ED_region_tag_redraw(region);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_dirty_mask(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Dirty Mask";
  ot->idname = "SCULPT_OT_dirty_mask";
  ot->description = "Generates a mask based on the geometry cavity and pointiness";

  /* API callbacks. */
  ot->exec = sculpt_dirty_mask_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* RNA. */
  RNA_def_boolean(
      ot->srna, "dirty_only", false, "Dirty Only", "Don't calculate cleans for convex areas");
}

static void sculpt_mask_expand_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  MEM_freeN(op->customdata);

  for (int n = 0; n < ss->filter_cache->totnode; n++) {
    PBVHNode *node = ss->filter_cache->nodes[n];
    if (create_face_set) {
      for (int i = 0; i < ss->totpoly; i++) {
        ss->face_sets[i] = ss->filter_cache->prev_face_set[i];
      }
    }
    else {
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
      {
        *vd.mask = ss->filter_cache->prev_mask[vd.index];
      }
      BKE_pbvh_vertex_iter_end;
    }

    BKE_pbvh_node_mark_redraw(node);
  }

  if (!create_face_set) {
    sculpt_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  sculpt_filter_cache_free(ss);
  SCULPT_undo_push_end();
  sculpt_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  ED_workspace_status_text(C, NULL);
}

static void sculpt_expand_task_cb(void *__restrict userdata,
                                  const int i,
                                  const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  PBVHVertexIter vd;
  int update_it = data->mask_expand_update_it;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
  {
    int vi = vd.index;
    float final_mask = *vd.mask;
    if (data->mask_expand_use_normals) {
      if (ss->filter_cache->normal_factor[SCULPT_active_vertex_get(ss)] <
          ss->filter_cache->normal_factor[vd.index]) {
        final_mask = 1.0f;
      }
      else {
        final_mask = 0.0f;
      }
    }
    else {
      if (ss->filter_cache->mask_update_it[vi] <= update_it &&
          ss->filter_cache->mask_update_it[vi] != 0) {
        final_mask = 1.0f;
      }
      else {
        final_mask = 0.0f;
      }
    }

    if (data->mask_expand_create_face_set) {
      if (final_mask == 1.0f) {
        SCULPT_vertex_face_set_set(ss, vd.index, ss->filter_cache->new_face_set);
      }
      BKE_pbvh_node_mark_redraw(node);
    }
    else {

      if (data->mask_expand_keep_prev_mask) {
        final_mask = MAX2(ss->filter_cache->prev_mask[vd.index], final_mask);
      }

      if (data->mask_expand_invert_mask) {
        final_mask = 1.0f - final_mask;
      }

      if (*vd.mask != final_mask) {
        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
        *vd.mask = final_mask;
        BKE_pbvh_node_mark_update_mask(node);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static int sculpt_mask_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  ARegion *region = CTX_wm_region(C);
  float prevclick_f[2];
  copy_v2_v2(prevclick_f, op->customdata);
  int prevclick[2] = {(int)prevclick_f[0], (int)prevclick_f[1]};
  int len = (int)len_v2v2_int(prevclick, event->mval);
  len = abs(len);
  int mask_speed = RNA_int_get(op->ptr, "mask_speed");
  int mask_expand_update_it = len / mask_speed;
  mask_expand_update_it = mask_expand_update_it + 1;

  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  if (RNA_boolean_get(op->ptr, "use_cursor")) {
    SculptCursorGeometryInfo sgi;
    float mouse[2];
    mouse[0] = event->mval[0];
    mouse[1] = event->mval[1];
    SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
    mask_expand_update_it = ss->filter_cache->mask_update_it[(int)SCULPT_active_vertex_get(ss)];
  }

  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS)) {
    /* Returning OPERATOR_CANCELLED will leak memory due to not finishing
     * undo. Better solution could be to make paint_mesh_restore_co work
     * for this case. */
    sculpt_mask_expand_cancel(C, op);
    return OPERATOR_FINISHED;
  }

  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS)) {

    /* Smooth iterations. */
    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .nodes = ss->filter_cache->nodes,
        .filter_type = MASK_FILTER_SMOOTH,
    };

    int smooth_iterations = RNA_int_get(op->ptr, "smooth_iterations");
    BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false);
    for (int i = 0; i < smooth_iterations; i++) {
      PBVHParallelSettings settings;
      BKE_pbvh_parallel_range_settings(
          &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
      BKE_pbvh_parallel_range(0, ss->filter_cache->totnode, &data, mask_filter_task_cb, &settings);
    }

    /* Pivot position. */
    if (RNA_boolean_get(op->ptr, "update_pivot")) {
      const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
      const float threshold = 0.2f;
      float avg[3];
      int total = 0;
      zero_v3(avg);

      for (int n = 0; n < ss->filter_cache->totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, ss->filter_cache->nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < (0.5f + threshold) && mask > (0.5f - threshold)) {
            if (SCULPT_check_vertex_pivot_symmetry(
                    vd.co, ss->filter_cache->mask_expand_initial_co, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }

      if (total > 0) {
        mul_v3_fl(avg, 1.0f / total);
        copy_v3_v3(ss->pivot_pos, avg);
      }
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
    }

    MEM_freeN(op->customdata);

    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }

    sculpt_filter_cache_free(ss);

    SCULPT_undo_push_end();
    sculpt_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  /* When pressing Ctrl, expand directly to the max number of iterations. This allows to flood fill
   * mask and face sets by connectivity directly. */
  if (event->ctrl) {
    mask_expand_update_it = ss->filter_cache->mask_update_last_it - 1;
  }

  if (!ELEM(event->type, MOUSEMOVE, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (mask_expand_update_it == ss->filter_cache->mask_update_current_it) {
    ED_region_tag_redraw(region);
    return OPERATOR_RUNNING_MODAL;
  }

  if (mask_expand_update_it < ss->filter_cache->mask_update_last_it) {

    if (create_face_set) {
      for (int i = 0; i < ss->totpoly; i++) {
        ss->face_sets[i] = ss->filter_cache->prev_face_set[i];
      }
    }
    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .nodes = ss->filter_cache->nodes,
        .mask_expand_update_it = mask_expand_update_it,
        .mask_expand_use_normals = RNA_boolean_get(op->ptr, "use_normals"),
        .mask_expand_invert_mask = RNA_boolean_get(op->ptr, "invert"),
        .mask_expand_keep_prev_mask = RNA_boolean_get(op->ptr, "keep_previous_mask"),
        .mask_expand_create_face_set = RNA_boolean_get(op->ptr, "create_face_set"),
    };
    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(
        &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
    BKE_pbvh_parallel_range(0, ss->filter_cache->totnode, &data, sculpt_expand_task_cb, &settings);
    ss->filter_cache->mask_update_current_it = mask_expand_update_it;
  }

  sculpt_flush_update_step(C, SCULPT_UPDATE_MASK);

  return OPERATOR_RUNNING_MODAL;
}

typedef struct MaskExpandFloodFillData {
  float original_normal[3];
  float edge_sensitivity;
  bool use_normals;
} MaskExpandFloodFillData;

static bool mask_expand_floodfill_cb(
    SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata)
{
  MaskExpandFloodFillData *data = userdata;

  if (!is_duplicate) {
    int to_it = ss->filter_cache->mask_update_it[from_v] + 1;
    ss->filter_cache->mask_update_it[to_v] = to_it;
    if (to_it > ss->filter_cache->mask_update_last_it) {
      ss->filter_cache->mask_update_last_it = to_it;
    }

    if (data->use_normals) {
      float current_normal[3], prev_normal[3];
      SCULPT_vertex_normal_get(ss, to_v, current_normal);
      SCULPT_vertex_normal_get(ss, from_v, prev_normal);
      const float from_edge_factor = ss->filter_cache->edge_factor[from_v];
      ss->filter_cache->edge_factor[to_v] = dot_v3v3(current_normal, prev_normal) *
                                            from_edge_factor;
      ss->filter_cache->normal_factor[to_v] = dot_v3v3(data->original_normal, current_normal) *
                                              powf(from_edge_factor, data->edge_sensitivity);
      CLAMP(ss->filter_cache->normal_factor[to_v], 0.0f, 1.0f);
    }
  }
  else {
    /* PBVH_GRIDS duplicate handling. */
    ss->filter_cache->mask_update_it[to_v] = ss->filter_cache->mask_update_it[from_v];
    if (data->use_normals) {
      ss->filter_cache->edge_factor[to_v] = ss->filter_cache->edge_factor[from_v];
      ss->filter_cache->normal_factor[to_v] = ss->filter_cache->normal_factor[from_v];
    }
  }

  return true;
}

static int sculpt_mask_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  const bool use_normals = RNA_boolean_get(op->ptr, "use_normals");
  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  SculptCursorGeometryInfo sgi;
  float mouse[2];
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];

  SCULPT_vertex_random_access_init(ss);

  op->customdata = MEM_mallocN(2 * sizeof(float), "initial mouse position");
  copy_v2_v2(op->customdata, mouse);

  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  int vertex_count = SCULPT_vertex_count_get(ss);

  ss->filter_cache = MEM_callocN(sizeof(FilterCache), "filter cache");

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &ss->filter_cache->nodes, &ss->filter_cache->totnode);

  SCULPT_undo_push_begin("Mask Expand");

  if (create_face_set) {
    SCULPT_undo_push_node(ob, ss->filter_cache->nodes[0], SCULPT_UNDO_FACE_SETS);
    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }
  }
  else {
    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      SCULPT_undo_push_node(ob, ss->filter_cache->nodes[i], SCULPT_UNDO_MASK);
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }
  }

  ss->filter_cache->mask_update_it = MEM_callocN(sizeof(int) * vertex_count,
                                                 "mask update iteration");
  if (use_normals) {
    ss->filter_cache->normal_factor = MEM_callocN(sizeof(float) * vertex_count,
                                                  "mask update normal factor");
    ss->filter_cache->edge_factor = MEM_callocN(sizeof(float) * vertex_count,
                                                "mask update normal factor");
    for (int i = 0; i < vertex_count; i++) {
      ss->filter_cache->edge_factor[i] = 1.0f;
    }
  }

  if (create_face_set) {
    ss->filter_cache->prev_face_set = MEM_callocN(sizeof(float) * ss->totpoly, "prev face mask");
    for (int i = 0; i < ss->totpoly; i++) {
      ss->filter_cache->prev_face_set[i] = ss->face_sets[i];
    }
    ss->filter_cache->new_face_set = SCULPT_face_set_next_available_get(ss);
  }
  else {
    ss->filter_cache->prev_mask = MEM_callocN(sizeof(float) * vertex_count, "prev mask");
    for (int i = 0; i < vertex_count; i++) {
      ss->filter_cache->prev_mask[i] = SCULPT_vertex_mask_get(ss, i);
    }
  }

  ss->filter_cache->mask_update_last_it = 1;
  ss->filter_cache->mask_update_current_it = 1;
  ss->filter_cache->mask_update_it[SCULPT_active_vertex_get(ss)] = 0;

  copy_v3_v3(ss->filter_cache->mask_expand_initial_co, SCULPT_active_vertex_co_get(ss));

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, FLT_MAX);

  MaskExpandFloodFillData fdata = {
      .use_normals = use_normals,
      .edge_sensitivity = RNA_int_get(op->ptr, "edge_sensitivity"),
  };
  SCULPT_active_vertex_normal_get(ss, fdata.original_normal);
  SCULPT_floodfill_execute(ss, &flood, mask_expand_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  if (use_normals) {
    for (int repeat = 0; repeat < 2; repeat++) {
      for (int i = 0; i < vertex_count; i++) {
        float avg = 0.0f;
        SculptVertexNeighborIter ni;
        sculpt_vertex_neighbors_iter_begin(ss, i, ni)
        {
          avg += ss->filter_cache->normal_factor[ni.index];
        }
        sculpt_vertex_neighbors_iter_end(ni);
        ss->filter_cache->normal_factor[i] = avg / ni.size;
      }
    }

    MEM_SAFE_FREE(ss->filter_cache->edge_factor);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .mask_expand_update_it = 0,
      .mask_expand_use_normals = RNA_boolean_get(op->ptr, "use_normals"),
      .mask_expand_invert_mask = RNA_boolean_get(op->ptr, "invert"),
      .mask_expand_keep_prev_mask = RNA_boolean_get(op->ptr, "keep_previous_mask"),
      .mask_expand_create_face_set = RNA_boolean_get(op->ptr, "create_face_set"),
  };
  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(0, ss->filter_cache->totnode, &data, sculpt_expand_task_cb, &settings);

  const char *status_str = TIP_(
      "Move the mouse to expand the mask from the active vertex. LMB: confirm mask, ESC/RMB: "
      "cancel");
  ED_workspace_status_text(C, status_str);

  sculpt_flush_update_step(C, SCULPT_UPDATE_MASK);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_mask_expand(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Expand";
  ot->idname = "SCULPT_OT_mask_expand";
  ot->description = "Expands a mask from the initial active vertex under the cursor";

  /* API callbacks. */
  ot->invoke = sculpt_mask_expand_invoke;
  ot->modal = sculpt_mask_expand_modal;
  ot->cancel = sculpt_mask_expand_cancel;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  ot->prop = RNA_def_boolean(ot->srna, "invert", true, "Invert", "Invert the new mask");
  ot->prop = RNA_def_boolean(
      ot->srna, "use_cursor", true, "Use Cursor", "Expand the mask to the cursor position");
  ot->prop = RNA_def_boolean(ot->srna,
                             "update_pivot",
                             true,
                             "Update Pivot Position",
                             "Set the pivot position to the mask border after creating the mask");
  ot->prop = RNA_def_int(ot->srna, "smooth_iterations", 2, 0, 10, "Smooth iterations", "", 0, 10);
  ot->prop = RNA_def_int(ot->srna, "mask_speed", 5, 1, 10, "Mask speed", "", 1, 10);

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_normals",
                             true,
                             "Use Normals",
                             "Generate the mask using the normals and curvature of the model");
  ot->prop = RNA_def_boolean(ot->srna,
                             "keep_previous_mask",
                             false,
                             "Keep Previous Mask",
                             "Generate the new mask on top of the current one");
  ot->prop = RNA_def_int(ot->srna,
                         "edge_sensitivity",
                         300,
                         0,
                         2000,
                         "Edge Detection Sensitivity",
                         "Sensitivity for expanding the mask across sculpted sharp edges when "
                         "using normals to generate the mask",
                         0,
                         2000);
  ot->prop = RNA_def_boolean(ot->srna,
                             "create_face_set",
                             false,
                             "Expand Face Mask",
                             "Expand a new Face Mask instead of the sculpt mask");
}

void SCULPT_geometry_preview_lines_update(bContext *C, SculptSession *ss, float radius)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);

  ss->preview_vert_index_count = 0;
  int totpoints = 0;

  /* This function is called from the cursor drawing code, so the PBVH may not be build yet. */
  if (!ss->pbvh) {
    return;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  if (!ss->pmap) {
    return;
  }

  float brush_co[3];
  copy_v3_v3(brush_co, SCULPT_active_vertex_co_get(ss));

  char *visited_vertices = MEM_callocN(SCULPT_vertex_count_get(ss) * sizeof(char),
                                       "visited vertices");

  /* Assuming an average of 6 edges per vertex in a triangulated mesh. */
  const int max_preview_vertices = SCULPT_vertex_count_get(ss) * 3 * 2;

  if (ss->preview_vert_index_list == NULL) {
    ss->preview_vert_index_list = MEM_callocN(max_preview_vertices * sizeof(int), "preview lines");
  }

  GSQueue *not_visited_vertices = BLI_gsqueue_new(sizeof(int));
  int active_v = SCULPT_active_vertex_get(ss);
  BLI_gsqueue_push(not_visited_vertices, &active_v);

  while (!BLI_gsqueue_is_empty(not_visited_vertices)) {
    int from_v;
    BLI_gsqueue_pop(not_visited_vertices, &from_v);
    SculptVertexNeighborIter ni;
    sculpt_vertex_neighbors_iter_begin(ss, from_v, ni)
    {
      if (totpoints + (ni.size * 2) < max_preview_vertices) {
        int to_v = ni.index;
        ss->preview_vert_index_list[totpoints] = from_v;
        totpoints++;
        ss->preview_vert_index_list[totpoints] = to_v;
        totpoints++;
        if (visited_vertices[to_v] == 0) {
          visited_vertices[to_v] = 1;
          const float *co = SCULPT_vertex_co_get(ss, to_v);
          if (len_squared_v3v3(brush_co, co) < radius * radius) {
            BLI_gsqueue_push(not_visited_vertices, &to_v);
          }
        }
      }
    }
    sculpt_vertex_neighbors_iter_end(ni);
  }

  BLI_gsqueue_free(not_visited_vertices);

  MEM_freeN(visited_vertices);

  ss->preview_vert_index_count = totpoints;
}
void ED_sculpt_init_transform(struct bContext *C)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  copy_v3_v3(ss->init_pivot_pos, ss->pivot_pos);
  copy_v4_v4(ss->init_pivot_rot, ss->pivot_rot);

  SCULPT_undo_push_begin("Transform");
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false);

  ss->pivot_rot[3] = 1.0f;

  SCULPT_vertex_random_access_init(ss);
  sculpt_filter_cache_init(ob, sd);
}

static void sculpt_transform_task_cb(void *__restrict userdata,
                                     const int i,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{

  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[i]);

  PBVHVertexIter vd;

  SCULPT_undo_push_node(data->ob, node, SCULPT_UNDO_COORDS);
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float transformed_co[3], orig_co[3], disp[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    copy_v3_v3(orig_co, orig_data.co);
    char symm_area = SCULPT_get_vertex_symm_area(orig_co);

    copy_v3_v3(transformed_co, orig_co);
    mul_m4_v3(data->transform_mats[(int)symm_area], transformed_co);
    sub_v3_v3v3(disp, transformed_co, orig_co);
    mul_v3_fl(disp, 1.0f - fade);

    add_v3_v3v3(vd.co, orig_co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

void ED_sculpt_update_modal_transform(struct bContext *C)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  SCULPT_vertex_random_access_init(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
  };

  float final_pivot_pos[3], d_t[3], d_r[4];
  float t_mat[4][4], r_mat[4][4], s_mat[4][4], pivot_mat[4][4], pivot_imat[4][4],
      transform_mat[4][4];

  copy_v3_v3(final_pivot_pos, ss->pivot_pos);
  for (int i = 0; i < PAINT_SYMM_AREAS; i++) {
    ePaintSymmetryAreas v_symm = i;

    copy_v3_v3(final_pivot_pos, ss->pivot_pos);

    unit_m4(pivot_mat);

    unit_m4(t_mat);
    unit_m4(r_mat);
    unit_m4(s_mat);

    /* Translation matrix. */
    sub_v3_v3v3(d_t, ss->pivot_pos, ss->init_pivot_pos);
    SCULPT_flip_v3_by_symm_area(d_t, symm, v_symm, ss->init_pivot_pos);
    translate_m4(t_mat, d_t[0], d_t[1], d_t[2]);

    /* Rotation matrix. */
    sub_qt_qtqt(d_r, ss->pivot_rot, ss->init_pivot_rot);
    normalize_qt(d_r);
    SCULPT_flip_quat_by_symm_area(d_r, symm, v_symm, ss->init_pivot_pos);
    quat_to_mat4(r_mat, d_r);

    /* Scale matrix. */
    size_to_mat4(s_mat, ss->pivot_scale);

    /* Pivot matrix. */
    SCULPT_flip_v3_by_symm_area(final_pivot_pos, symm, v_symm, ss->init_pivot_pos);
    translate_m4(pivot_mat, final_pivot_pos[0], final_pivot_pos[1], final_pivot_pos[2]);
    invert_m4_m4(pivot_imat, pivot_mat);

    /* Final transform matrix. */
    mul_m4_m4m4(transform_mat, r_mat, t_mat);
    mul_m4_m4m4(transform_mat, transform_mat, s_mat);
    mul_m4_m4m4(data.transform_mats[i], transform_mat, pivot_imat);
    mul_m4_m4m4(data.transform_mats[i], pivot_mat, data.transform_mats[i]);
  }

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(
      0, ss->filter_cache->totnode, &data, sculpt_transform_task_cb, &settings);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    sculpt_flush_stroke_deform(sd, ob, true);
  }

  sculpt_flush_update_step(C, SCULPT_UPDATE_COORDS);
}

void ED_sculpt_end_transform(struct bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  if (ss->filter_cache) {
    sculpt_filter_cache_free(ss);
  }
  SCULPT_undo_push_end();
  sculpt_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
}

typedef enum eSculptPivotPositionModes {
  SCULPT_PIVOT_POSITION_ORIGIN = 0,
  SCULPT_PIVOT_POSITION_UNMASKED = 1,
  SCULPT_PIVOT_POSITION_MASK_BORDER = 2,
  SCULPT_PIVOT_POSITION_ACTIVE_VERTEX = 3,
  SCULPT_PIVOT_POSITION_CURSOR_SURFACE = 4,
} eSculptPivotPositionModes;

static EnumPropertyItem prop_sculpt_pivot_position_types[] = {
    {SCULPT_PIVOT_POSITION_ORIGIN,
     "ORIGIN",
     0,
     "Origin",
     "Sets the pivot to the origin of the sculpt"},
    {SCULPT_PIVOT_POSITION_UNMASKED,
     "UNMASKED",
     0,
     "Unmasked",
     "Sets the pivot position to the average position of the unmasked vertices"},
    {SCULPT_PIVOT_POSITION_MASK_BORDER,
     "BORDER",
     0,
     "Mask border",
     "Sets the pivot position to the center of the border of the mask"},
    {SCULPT_PIVOT_POSITION_ACTIVE_VERTEX,
     "ACTIVE",
     0,
     "Active vertex",
     "Sets the pivot position to the active vertex position"},
    {SCULPT_PIVOT_POSITION_CURSOR_SURFACE,
     "SURFACE",
     0,
     "Surface",
     "Sets the pivot position to the surface under the cursor"},
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_set_pivot_position_exec(bContext *C, wmOperator *op)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  int mode = RNA_enum_get(op->ptr, "mode");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true);

  /* Pivot to center. */
  if (mode == SCULPT_PIVOT_POSITION_ORIGIN) {
    zero_v3(ss->pivot_pos);
  }
  /* Pivot to active vertex. */
  else if (mode == SCULPT_PIVOT_POSITION_ACTIVE_VERTEX) {
    copy_v3_v3(ss->pivot_pos, SCULPT_active_vertex_co_get(ss));
  }
  /* Pivot to raycast surface. */
  else if (mode == SCULPT_PIVOT_POSITION_CURSOR_SURFACE) {
    float stroke_location[3];
    float mouse[2];
    mouse[0] = RNA_float_get(op->ptr, "mouse_x");
    mouse[1] = RNA_float_get(op->ptr, "mouse_y");
    if (SCULPT_stroke_get_location(C, stroke_location, mouse)) {
      copy_v3_v3(ss->pivot_pos, stroke_location);
    }
  }
  else {
    PBVHNode **nodes;
    int totnode;
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    float avg[3];
    int total = 0;
    zero_v3(avg);

    /* Pivot to unmasked. */
    if (mode == SCULPT_PIVOT_POSITION_UNMASKED) {
      for (int n = 0; n < totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < 1.0f) {
            if (SCULPT_check_vertex_pivot_symmetry(vd.co, ss->pivot_pos, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    }
    /* Pivot to mask border. */
    else if (mode == SCULPT_PIVOT_POSITION_MASK_BORDER) {
      const float threshold = 0.2f;

      for (int n = 0; n < totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < (0.5f + threshold) && mask > (0.5f - threshold)) {
            if (SCULPT_check_vertex_pivot_symmetry(vd.co, ss->pivot_pos, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      copy_v3_v3(ss->pivot_pos, avg);
    }

    MEM_SAFE_FREE(nodes);
  }

  ED_region_tag_redraw(region);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

  return OPERATOR_FINISHED;
}

static int sculpt_set_pivot_position_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_float_set(op->ptr, "mouse_x", event->mval[0]);
  RNA_float_set(op->ptr, "mouse_y", event->mval[1]);
  return sculpt_set_pivot_position_exec(C, op);
}

static void SCULPT_OT_set_pivot_position(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Pivot Position";
  ot->idname = "SCULPT_OT_set_pivot_position";
  ot->description = "Sets the sculpt transform pivot position";

  /* API callbacks. */
  ot->invoke = sculpt_set_pivot_position_invoke;
  ot->exec = sculpt_set_pivot_position_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_pivot_position_types,
               SCULPT_PIVOT_POSITION_UNMASKED,
               "Mode",
               "");

  RNA_def_float(ot->srna,
                "mouse_x",
                0.0f,
                0.0f,
                FLT_MAX,
                "Mouse Position X",
                "Position of the mouse used for \"Surface\" mode",
                0.0f,
                10000.0f);
  RNA_def_float(ot->srna,
                "mouse_y",
                0.0f,
                0.0f,
                FLT_MAX,
                "Mouse Position Y",
                "Position of the mouse used for \"Surface\" mode",
                0.0f,
                10000.0f);
}

typedef enum eSculptFaceGroupsCreateModes {
  SCULPT_FACE_SET_MASKED = 0,
  SCULPT_FACE_SET_VISIBLE = 1,
  SCULPT_FACE_SET_ALL = 2,
} eSculptFaceGroupsCreateModes;

static EnumPropertyItem prop_sculpt_face_set_create_types[] = {
    {
        SCULPT_FACE_SET_MASKED,
        "MASKED",
        0,
        "Face Set From Masked",
        "Create a new Face Set from the masked faces",
    },
    {
        SCULPT_FACE_SET_VISIBLE,
        "VISIBLE",
        0,
        "Face Set From Visible",
        "Create a new Face Set from the visible vertices",
    },
    {
        SCULPT_FACE_SET_ALL,
        "ALL",
        0,
        "Face Set Full Mesh",
        "Create an unique Face Set with all faces in the sculpt",
    },
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_face_set_create_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Dyntopo and Multires not supported for now. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, mode == SCULPT_FACE_SET_MASKED);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  float threshold = 0.5f;

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin("face set change");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  const int next_face_set = SCULPT_face_set_next_available_get(ss);

  if (mode == SCULPT_FACE_SET_MASKED) {
    for (int i = 0; i < tot_vert; i++) {
      if (SCULPT_vertex_mask_get(ss, i) >= threshold && SCULPT_vertex_visible_get(ss, i)) {
        SCULPT_vertex_face_set_set(ss, i, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_VISIBLE) {
    for (int i = 0; i < tot_vert; i++) {
      if (SCULPT_vertex_visible_get(ss, i)) {
        SCULPT_vertex_face_set_set(ss, i, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_ALL) {
    for (int i = 0; i < tot_vert; i++) {
      SCULPT_vertex_face_set_set(ss, i, next_face_set);
    }
  }

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);

  SCULPT_undo_push_end();

  ED_region_tag_redraw(region);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_face_sets_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Face Set";
  ot->idname = "SCULPT_OT_face_sets_create";
  ot->description = "Create a new Face Set";

  /* api callbacks */
  ot->invoke = sculpt_face_set_create_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_set_create_types, SCULPT_FACE_SET_MASKED, "Mode", "");
}

typedef enum eSculptFaceGroupVisibilityModes {
  SCULPT_FACE_SET_VISIBILITY_TOGGLE = 0,
  SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE = 1,
  SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE = 2,
  SCULPT_FACE_SET_VISIBILITY_INVERT = 3,
  SCULPT_FACE_SET_VISIBILITY_SHOW_ALL = 4,
} eSculptFaceGroupVisibilityModes;

static EnumPropertyItem prop_sculpt_face_sets_change_visibility_types[] = {
    {
        SCULPT_FACE_SET_VISIBILITY_TOGGLE,
        "TOGGLE",
        0,
        "Toggle Visibility",
        "Hide all Face Sets except for the active one",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE,
        "SHOW_ACTIVE",
        0,
        "Show Active Face Set",
        "Show Active Face Set",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE,
        "HIDE_ACTIVE",
        0,
        "Hide Active Face Sets",
        "Hide Active Face Sets",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_INVERT,
        "INVERT",
        0,
        "Invert Face Set Visibility",
        "Invert Face Set Visibility",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_SHOW_ALL,
        "SHOW_ALL",
        0,
        "Show All Face Sets",
        "Show All Face Sets",
    },
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_face_sets_change_visibility_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  /* Dyntopo and Multires not supported for now. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  const int mode = RNA_enum_get(op->ptr, "mode");
  int active_vertex_index = SCULPT_active_vertex_get(ss);
  int active_face_set = SCULPT_vertex_face_set_get(ss, active_vertex_index);

  SCULPT_undo_push_begin("Hide area");

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (totnode == 0) {
    MEM_SAFE_FREE(nodes);
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  if (mode == SCULPT_FACE_SET_VISIBILITY_TOGGLE) {
    bool hidden_vertex = false;
    for (int i = 0; i < tot_vert; i++) {
      if (!SCULPT_vertex_visible_get(ss, i)) {
        hidden_vertex = true;
        break;
      }
    }

    for (int i = 0; i < ss->totpoly; i++) {
      if (ss->face_sets[i] < 0) {
        hidden_vertex = true;
        break;
      }
    }
    if (hidden_vertex) {
      SCULPT_face_sets_visibility_all_set(ss, true);
    }
    else {
      SCULPT_face_sets_visibility_all_set(ss, false);
      SCULPT_face_set_visibility_set(ss, active_face_set, true);
    }
  }

  if (mode == SCULPT_FACE_SET_VISIBILITY_SHOW_ALL) {
    SCULPT_face_sets_visibility_all_set(ss, true);
  }

  if (mode == SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE) {
    SCULPT_face_sets_visibility_all_set(ss, false);
    SCULPT_face_set_visibility_set(ss, active_face_set, true);
    for (int i = 0; i < tot_vert; i++) {
      SCULPT_vertex_visible_set(ss,
                                i,
                                SCULPT_vertex_visible_get(ss, i) &&
                                    SCULPT_vertex_has_face_set(ss, i, active_face_set));
    }
  }

  if (mode == SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE) {
    SCULPT_face_set_visibility_set(ss, active_face_set, false);
  }

  if (mode == SCULPT_FACE_SET_VISIBILITY_INVERT) {
    SCULPT_face_sets_visibility_invert(ss);
  }

  /* For modes that use the cursor active vertex, update the rotation origin for viewport
   * navigation. */
  if (ELEM(mode, SCULPT_FACE_SET_VISIBILITY_TOGGLE, SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE)) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    float location[3];
    copy_v3_v3(location, SCULPT_active_vertex_co_get(ss));
    mul_m4_v3(ob->obmat, location);
    copy_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter = 1;
    ups->last_stroke_valid = true;
  }

  /* Sync face sets visibility and vertex visibility. */
  SCULPT_visibility_sync_all_face_sets_to_vertices(ss);

  SCULPT_undo_push_end();

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_visibility(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  MEM_SAFE_FREE(nodes);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts(ob->data);
  }

  ED_region_tag_redraw(region);
  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  View3D *v3d = CTX_wm_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  return OPERATOR_FINISHED;
}

static void SCULPT_OT_face_sets_change_visibility(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  /* Api callbacks. */
  ot->invoke = sculpt_face_sets_change_visibility_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_face_sets_change_visibility_types,
               SCULPT_FACE_SET_VISIBILITY_TOGGLE,
               "Mode",
               "");
}

static int sculpt_face_sets_randomize_colors_invoke(bContext *C,
                                                    wmOperator *UNUSED(op),
                                                    const wmEvent *UNUSED(event))
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);

  /* Dyntopo and Multires not supported for now. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  Mesh *mesh = ob->data;

  mesh->face_sets_color_seed += 1;
  if (ss->face_sets) {
    const int random_index = clamp_i(
        ss->totpoly * BLI_hash_int_01(mesh->face_sets_color_seed), 0, max_ii(0, ss->totpoly - 1));
    mesh->face_sets_color_default = ss->face_sets[random_index];
  }
  BKE_pbvh_face_sets_color_set(pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);

  View3D *v3d = CTX_wm_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  }

  ED_region_tag_redraw(region);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Randomize Face Sets Colors";
  ot->idname = "SCULPT_OT_face_sets_randomize_colors";
  ot->description = "Generates a new set of random colors to render the Face Sets in the viewport";

  /* Api callbacks. */
  ot->invoke = sculpt_face_sets_randomize_colors_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_operatortypes_sculpt(void)
{
  WM_operatortype_append(SCULPT_OT_brush_stroke);
  WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
  WM_operatortype_append(SCULPT_OT_set_persistent_base);
  WM_operatortype_append(SCULPT_OT_dynamic_topology_toggle);
  WM_operatortype_append(SCULPT_OT_optimize);
  WM_operatortype_append(SCULPT_OT_symmetrize);
  WM_operatortype_append(SCULPT_OT_detail_flood_fill);
  WM_operatortype_append(SCULPT_OT_sample_detail_size);
  WM_operatortype_append(SCULPT_OT_set_detail_size);
  WM_operatortype_append(SCULPT_OT_mesh_filter);
  WM_operatortype_append(SCULPT_OT_mask_filter);
  WM_operatortype_append(SCULPT_OT_dirty_mask);
  WM_operatortype_append(SCULPT_OT_mask_expand);
  WM_operatortype_append(SCULPT_OT_set_pivot_position);
  WM_operatortype_append(SCULPT_OT_face_sets_create);
  WM_operatortype_append(SCULPT_OT_face_sets_change_visibility);
  WM_operatortype_append(SCULPT_OT_face_sets_randomize_colors);
}
