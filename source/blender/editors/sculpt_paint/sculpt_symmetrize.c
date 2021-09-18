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
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

typedef uint MirrTopoHash_t;

typedef struct MirrTopoVert_t {
  MirrTopoHash_t hash;
  int v_index;
} MirrTopoVert_t;

static int mirrtopo_hash_sort(const void *l1, const void *l2)
{
  if ((MirrTopoHash_t)(intptr_t)l1 > (MirrTopoHash_t)(intptr_t)l2) {
    return 1;
  }
  if ((MirrTopoHash_t)(intptr_t)l1 < (MirrTopoHash_t)(intptr_t)l2) {
    return -1;
  }
  return 0;
}

static int mirrtopo_vert_sort(const void *v1, const void *v2)
{
  if (((MirrTopoVert_t *)v1)->hash > ((MirrTopoVert_t *)v2)->hash) {
    return 1;
  }
  if (((MirrTopoVert_t *)v1)->hash < ((MirrTopoVert_t *)v2)->hash) {
    return -1;
  }
  return 0;
}

void SCULPT_symmetrize_map_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  if (ss->vertex_info.symmetrize_map) {
    /* Nothing to do. */
    return;
  }

  MEdge *medge = NULL, *med;

  int a, last;
  int totvert, totedge;
  int tot_unique = -1, tot_unique_prev = -1;
  int tot_unique_edges = 0, tot_unique_edges_prev;

  MirrTopoHash_t *topo_hash = NULL;
  MirrTopoHash_t *topo_hash_prev = NULL;
  MirrTopoVert_t *topo_pairs;
  MirrTopoHash_t topo_pass = 1;

  int *index_lookup; /* direct access to mesh_topo_store->index_lookup */

  totvert = me->totvert;
  topo_hash = MEM_callocN(totvert * sizeof(MirrTopoHash_t), "TopoMirr");

  /* Initialize the vert-edge-user counts used to detect unique topology */
  totedge = me->totedge;
  medge = me->medge;

  for (a = 0, med = medge; a < totedge; a++, med++) {
    const uint i1 = med->v1, i2 = med->v2;
    topo_hash[i1]++;
    topo_hash[i2]++;
  }

  topo_hash_prev = MEM_dupallocN(topo_hash);

  tot_unique_prev = -1;
  tot_unique_edges_prev = -1;
  while (true) {
    /* use the number of edges per vert to give verts unique topology IDs */

    tot_unique_edges = 0;

    /* This can make really big numbers, wrapping around here is fine */
    for (a = 0, med = medge; a < totedge; a++, med++) {
      const uint i1 = med->v1, i2 = med->v2;
      topo_hash[i1] += topo_hash_prev[i2] * topo_pass;
      topo_hash[i2] += topo_hash_prev[i1] * topo_pass;
      tot_unique_edges += (topo_hash[i1] != topo_hash[i2]);
    }
    memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

    /* sort so we can count unique values */
    qsort(topo_hash_prev, totvert, sizeof(MirrTopoHash_t), mirrtopo_hash_sort);

    tot_unique = 1; /* account for skipping the first value */
    for (a = 1; a < totvert; a++) {
      if (topo_hash_prev[a - 1] != topo_hash_prev[a]) {
        tot_unique++;
      }
    }

    if ((tot_unique <= tot_unique_prev) && (tot_unique_edges <= tot_unique_edges_prev)) {
      /* Finish searching for unique values when 1 loop doesn't give a
       * higher number of unique values compared to the previous loop. */
      break;
    }
    tot_unique_prev = tot_unique;
    tot_unique_edges_prev = tot_unique_edges;
    /* Copy the hash calculated this iteration, so we can use them next time */
    memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

    topo_pass++;
  }

  /* Hash/Index pairs are needed for sorting to find index pairs */
  topo_pairs = MEM_callocN(sizeof(MirrTopoVert_t) * totvert, "MirrTopoPairs");

  /* since we are looping through verts, initialize these values here too */
  index_lookup = MEM_mallocN(totvert * sizeof(int), "mesh_topo_lookup");

  for (a = 0; a < totvert; a++) {
    topo_pairs[a].hash = topo_hash[a];
    topo_pairs[a].v_index = a;

    /* initialize lookup */
    index_lookup[a] = -1;
  }

  qsort(topo_pairs, totvert, sizeof(MirrTopoVert_t), mirrtopo_vert_sort);

  last = 0;

  /* Get the pairs out of the sorted hashes, note, totvert+1 means we can use the previous 2,
   * but you cant ever access the last 'a' index of MirrTopoPairs */
  for (a = 1; a <= totvert; a++) {
    if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
      const int match_count = a - last;
      if (match_count == 2) {
        const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
        index_lookup[j] = k;
        index_lookup[k] = j;
      }
      else if (match_count == 1) {
        /* Center vertex. */
        const int j = topo_pairs[a - 1].v_index;
        index_lookup[j] = j;
      }
      last = a;
    }
  }

  MEM_freeN(topo_pairs);
  topo_pairs = NULL;

  MEM_freeN(topo_hash);
  MEM_freeN(topo_hash_prev);

  ss->vertex_info.symmetrize_map = index_lookup;
}

static void do_shape_symmetrize_brush_task_cb(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {

    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    const int symmetrical_index = ss->vertex_info.symmetrize_map[vd.index];
    const SculptVertRef symmetrical_vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh,
                                                                            symmetrical_index);

    if (symmetrical_index == -1) {
      continue;
    }

    float symm_co[3];
    copy_v3_v3(symm_co, SCULPT_vertex_co_get(ss, symmetrical_vertex));

    symm_co[0] *= -1;
    float new_co[3];
    copy_v3_v3(new_co, symm_co);

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    float disp[3];
    sub_v3_v3v3(disp, new_co, vd.co);
    madd_v3_v3v3fl(vd.co, vd.co, disp, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }

    BKE_pbvh_vertex_iter_end;
  }
}

/* Public functions. */

/* Main Brush Function. */
void SCULPT_do_symmetrize_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  if (!SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    return;
  }

  SCULPT_symmetrize_map_ensure(ob);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_shape_symmetrize_brush_task_cb, &settings);
}
