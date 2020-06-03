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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

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

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

/* Draw Face Sets Brush. */

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
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
      MeshElemMap *vert_map = &ss->pmap[vd.index];
      for (int j = 0; j < ss->pmap[vd.index].count; j++) {
        const MPoly *p = &ss->mpoly[vert_map->indices[j]];

        float poly_center[3];
        BKE_mesh_calc_poly_center(p, &ss->mloop[p->loopstart], ss->mvert, poly_center);

        if (sculpt_brush_test_sq_fn(&test, poly_center)) {
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      sqrtf(test.dist),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask ? *vd.mask : 0.0f,
                                                                      vd.index,
                                                                      thread_id);

          if (fade > 0.05f && ss->face_sets[vert_map->indices[j]] > 0) {
            ss->face_sets[vert_map->indices[j]] = abs(ss->cache->paint_face_set);
          }
        }
      }
    }

    else if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
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
                                                                      thread_id);

          if (fade > 0.05f) {
            SCULPT_vertex_face_set_set(ss, vd.index, ss->cache->paint_face_set);
          }
        }
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

  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (relax_face_sets != SCULPT_vertex_has_unique_face_set(ss, vd.index)) {
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask ? *vd.mask : 0.0f,
                                                                    vd.index,
                                                                    thread_id);

        SCULPT_relax_vertex(ss, &vd, fade * bstrength, relax_face_sets, vd.co);
        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_draw_face_sets_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  BKE_curvemapping_initialize(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  if (ss->cache->alt_smooth) {
    for (int i = 0; i < 4; i++) {
      BLI_task_parallel_range(0, totnode, &data, do_relax_face_sets_brush_task_cb_ex, &settings);
    }
  }
  else {
    BLI_task_parallel_range(0, totnode, &data, do_draw_face_sets_brush_task_cb_ex, &settings);
  }
}

/* Face Sets Operators */

typedef enum eSculptFaceGroupsCreateModes {
  SCULPT_FACE_SET_MASKED = 0,
  SCULPT_FACE_SET_VISIBLE = 1,
  SCULPT_FACE_SET_ALL = 2,
  SCULPT_FACE_SET_SELECTION = 3,
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
    {
        SCULPT_FACE_SET_SELECTION,
        "SELECTION",
        0,
        "Face Set From Edit Mode Selection",
        "Create an Face Set corresponding to the Edit Mode face selection",
    },
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Dyntopo not suported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
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

    /* If all vertices in the sculpt are visible, create the new face set and update the default
     * color. This way the new face set will be white, which is a quick way of disabling all face
     * sets and the performance hit of rendering the overlay. */
    bool all_visible = true;
    for (int i = 0; i < tot_vert; i++) {
      if (!SCULPT_vertex_visible_get(ss, i)) {
        all_visible = false;
        break;
      }
    }

    if (all_visible) {
      Mesh *mesh = ob->data;
      mesh->face_sets_color_default = next_face_set;
      BKE_pbvh_face_sets_color_set(
          ss->pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);
    }

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

  if (mode == SCULPT_FACE_SET_SELECTION) {
    Mesh *mesh = ob->data;
    BMesh *bm;
    const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
    bm = BM_mesh_create(&allocsize,
                        &((struct BMeshCreateParams){
                            .use_toolflags = true,
                        }));

    BM_mesh_bm_from_me(bm,
                       mesh,
                       (&(struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));

    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        ss->face_sets[BM_elem_index_get(f)] = next_face_set;
      }
    }
    BM_mesh_free(bm);
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

void SCULPT_OT_face_sets_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Face Set";
  ot->idname = "SCULPT_OT_face_sets_create";
  ot->description = "Create a new Face Set";

  /* api callbacks */
  ot->exec = sculpt_face_set_create_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_set_create_types, SCULPT_FACE_SET_MASKED, "Mode", "");
}

typedef enum eSculptFaceSetsInitMode {
  SCULPT_FACE_SETS_FROM_LOOSE_PARTS = 0,
  SCULPT_FACE_SETS_FROM_MATERIALS = 1,
  SCULPT_FACE_SETS_FROM_NORMALS = 2,
  SCULPT_FACE_SETS_FROM_UV_SEAMS = 3,
  SCULPT_FACE_SETS_FROM_CREASES = 4,
  SCULPT_FACE_SETS_FROM_SHARP_EDGES = 5,
  SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT = 6,
  SCULPT_FACE_SETS_FROM_FACE_MAPS = 7,
} eSculptFaceSetsInitMode;

static EnumPropertyItem prop_sculpt_face_sets_init_types[] = {
    {
        SCULPT_FACE_SETS_FROM_LOOSE_PARTS,
        "LOOSE_PARTS",
        0,
        "Face Sets From Loose Parts",
        "Create a Face Set per loose part in the mesh",
    },
    {
        SCULPT_FACE_SETS_FROM_MATERIALS,
        "MATERIALS",
        0,
        "Face Sets From Material Slots",
        "Create a Face Set per Material Slot",
    },
    {
        SCULPT_FACE_SETS_FROM_NORMALS,
        "NORMALS",
        0,
        "Face Sets From Mesh Normals",
        "Create Face Sets for Faces that have similar normal",
    },
    {
        SCULPT_FACE_SETS_FROM_UV_SEAMS,
        "UV_SEAMS",
        0,
        "Face Sets From UV Seams",
        "Create Face Sets using UV Seams as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_CREASES,
        "CREASES",
        0,
        "Face Sets From Edge Creases",
        "Create Face Sets using Edge Creases as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT,
        "BEVEL_WEIGHT",
        0,
        "Face Sets From Bevel Weight",
        "Create Face Sets using Bevel Weights as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_SHARP_EDGES,
        "SHARP_EDGES",
        0,
        "Face Sets From Sharp Edges",
        "Create Face Sets using Sharp Edges as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_FACE_MAPS,
        "FACE_MAPS",
        0,
        "Face Sets From Face Maps",
        "Create a Face Set per Face Map",
    },
    {0, NULL, 0, NULL, NULL},
};

typedef bool (*face_sets_flood_fill_test)(
    BMesh *bm, BMFace *from_f, BMEdge *from_e, BMFace *to_f, const float threshold);

static bool sculpt_face_sets_init_loose_parts_test(BMesh *UNUSED(bm),
                                                   BMFace *UNUSED(from_f),
                                                   BMEdge *UNUSED(from_e),
                                                   BMFace *UNUSED(to_f),
                                                   const float UNUSED(threshold))
{
  return true;
}

static bool sculpt_face_sets_init_normals_test(
    BMesh *UNUSED(bm), BMFace *from_f, BMEdge *UNUSED(from_e), BMFace *to_f, const float threshold)
{
  return fabsf(dot_v3v3(from_f->no, to_f->no)) > threshold;
}

static bool sculpt_face_sets_init_uv_seams_test(BMesh *UNUSED(bm),
                                                BMFace *UNUSED(from_f),
                                                BMEdge *from_e,
                                                BMFace *UNUSED(to_f),
                                                const float UNUSED(threshold))
{
  return !BM_elem_flag_test(from_e, BM_ELEM_SEAM);
}

static bool sculpt_face_sets_init_crease_test(
    BMesh *bm, BMFace *UNUSED(from_f), BMEdge *from_e, BMFace *UNUSED(to_f), const float threshold)
{
  return BM_elem_float_data_get(&bm->edata, from_e, CD_CREASE) < threshold;
}

static bool sculpt_face_sets_init_bevel_weight_test(
    BMesh *bm, BMFace *UNUSED(from_f), BMEdge *from_e, BMFace *UNUSED(to_f), const float threshold)
{
  return BM_elem_float_data_get(&bm->edata, from_e, CD_BWEIGHT) < threshold;
}

static bool sculpt_face_sets_init_sharp_edges_test(BMesh *UNUSED(bm),
                                                   BMFace *UNUSED(from_f),
                                                   BMEdge *from_e,
                                                   BMFace *UNUSED(to_f),
                                                   const float UNUSED(threshold))
{
  return BM_elem_flag_test(from_e, BM_ELEM_SMOOTH);
}

static void sculpt_face_sets_init_flood_fill(Object *ob,
                                             face_sets_flood_fill_test test,
                                             const float threshold)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = ob->data;
  BMesh *bm;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BLI_bitmap *visited_faces = BLI_BITMAP_NEW(mesh->totpoly, "visited faces");
  const int totfaces = mesh->totpoly;

  int *face_sets = ss->face_sets;

  BM_mesh_elem_table_init(bm, BM_FACE);
  BM_mesh_elem_table_ensure(bm, BM_FACE);

  int next_face_set = 1;

  for (int i = 0; i < totfaces; i++) {
    if (!BLI_BITMAP_TEST(visited_faces, i)) {
      GSQueue *queue;
      queue = BLI_gsqueue_new(sizeof(int));

      face_sets[i] = next_face_set;
      BLI_BITMAP_ENABLE(visited_faces, i);
      BLI_gsqueue_push(queue, &i);

      while (!BLI_gsqueue_is_empty(queue)) {
        int from_f;
        BLI_gsqueue_pop(queue, &from_f);

        BMFace *f, *f_neighbor;
        BMEdge *ed;
        BMIter iter_a, iter_b;

        f = BM_face_at_index(bm, from_f);

        BM_ITER_ELEM (ed, &iter_a, f, BM_EDGES_OF_FACE) {
          BM_ITER_ELEM (f_neighbor, &iter_b, ed, BM_FACES_OF_EDGE) {
            if (f_neighbor != f) {
              int neighbor_face_index = BM_elem_index_get(f_neighbor);
              if (!BLI_BITMAP_TEST(visited_faces, neighbor_face_index)) {
                if (test(bm, f, ed, f_neighbor, threshold)) {
                  face_sets[neighbor_face_index] = next_face_set;
                  BLI_BITMAP_ENABLE(visited_faces, neighbor_face_index);
                  BLI_gsqueue_push(queue, &neighbor_face_index);
                }
              }
            }
          }
        }
      }

      next_face_set += 1;

      BLI_gsqueue_free(queue);
    }
  }

  MEM_SAFE_FREE(visited_faces);

  BM_mesh_free(bm);
}

static void sculpt_face_sets_init_loop(Object *ob, const int mode)
{
  Mesh *mesh = ob->data;
  SculptSession *ss = ob->sculpt;
  BMesh *bm;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));
  BMIter iter;
  BMFace *f;

  const int cd_fmaps_offset = CustomData_get_offset(&bm->pdata, CD_FACEMAP);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (mode == SCULPT_FACE_SETS_FROM_MATERIALS) {
      ss->face_sets[BM_elem_index_get(f)] = (int)(f->mat_nr + 1);
    }
    else if (mode == SCULPT_FACE_SETS_FROM_FACE_MAPS) {
      if (cd_fmaps_offset != -1) {
        ss->face_sets[BM_elem_index_get(f)] = BM_ELEM_CD_GET_INT(f, cd_fmaps_offset) + 2;
      }
      else {
        ss->face_sets[BM_elem_index_get(f)] = 1;
      }
    }
  }
  BM_mesh_free(bm);
}

static int sculpt_face_set_init_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin("face set change");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  const float threshold = RNA_float_get(op->ptr, "threshold");

  switch (mode) {
    case SCULPT_FACE_SETS_FROM_LOOSE_PARTS:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_loose_parts_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_MATERIALS:
      sculpt_face_sets_init_loop(ob, SCULPT_FACE_SETS_FROM_MATERIALS);
      break;
    case SCULPT_FACE_SETS_FROM_NORMALS:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_normals_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_UV_SEAMS:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_uv_seams_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_CREASES:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_crease_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_SHARP_EDGES:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_sharp_edges_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT:
      sculpt_face_sets_init_flood_fill(ob, sculpt_face_sets_init_bevel_weight_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_FACE_MAPS:
      sculpt_face_sets_init_loop(ob, SCULPT_FACE_SETS_FROM_FACE_MAPS);
      break;
  }

  SCULPT_undo_push_end();

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  SCULPT_visibility_sync_all_face_sets_to_vertices(ss);

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

void SCULPT_OT_face_sets_init(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Init Face Sets";
  ot->idname = "SCULPT_OT_face_sets_init";
  ot->description = "Initializes all Face Sets in the mesh";

  /* api callbacks */
  ot->exec = sculpt_face_set_init_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_init_types, SCULPT_FACE_SET_MASKED, "Mode", "");
  RNA_def_float(
      ot->srna,
      "threshold",
      0.5f,
      0.0f,
      1.0f,
      "Threshold",
      "Minimum value to consider a certain attribute a boundary when creating the Face Sets",
      0.0f,
      1.0f);
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

static int sculpt_face_sets_change_visibility_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  const int mode = RNA_enum_get(op->ptr, "mode");
  const int active_face_set = SCULPT_active_face_set_get(ss);

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

    /* This can fail with regular meshes with non-manifold geometry as the visibility state can't
     * be synced from face sets to non-manifold vertices. */
    if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      for (int i = 0; i < tot_vert; i++) {
        if (!SCULPT_vertex_visible_get(ss, i)) {
          hidden_vertex = true;
          break;
        }
      }
    }

    for (int i = 0; i < ss->totfaces; i++) {
      if (ss->face_sets[i] <= 0) {
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

void SCULPT_OT_face_sets_change_visibility(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  /* Api callbacks. */
  ot->exec = sculpt_face_sets_change_visibility_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_face_sets_change_visibility_types,
               SCULPT_FACE_SET_VISIBILITY_TOGGLE,
               "Mode",
               "");
}

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator *UNUSED(op))
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  Mesh *mesh = ob->data;

  mesh->face_sets_color_seed += 1;
  if (ss->face_sets) {
    const int random_index = clamp_i(ss->totfaces * BLI_hash_int_01(mesh->face_sets_color_seed),
                                     0,
                                     max_ii(0, ss->totfaces - 1));
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

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Randomize Face Sets Colors";
  ot->idname = "SCULPT_OT_face_sets_randomize_colors";
  ot->description = "Generates a new set of random colors to render the Face Sets in the viewport";

  /* Api callbacks. */
  ot->exec = sculpt_face_sets_randomize_colors_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

typedef enum eSculptFaceSetEditMode {
  SCULPT_FACE_SET_EDIT_GROW = 0,
  SCULPT_FACE_SET_EDIT_SHRINK = 1,
} eSculptFaceSetEditMode;

static EnumPropertyItem prop_sculpt_face_sets_edit_types[] = {
    {
        SCULPT_FACE_SET_EDIT_GROW,
        "GROW",
        0,
        "Grow Face Set",
        "Grows the Face Sets boundary by one face based on mesh topology",
    },
    {
        SCULPT_FACE_SET_EDIT_SHRINK,
        "SHRINK",
        0,
        "Shrink Face Set",
        "Shrinks the Face Sets boundary by one face based on mesh topology",
    },
    {0, NULL, 0, NULL, NULL},
};

static void sculpt_face_set_grow(Object *ob,
                                 SculptSession *ss,
                                 int *prev_face_sets,
                                 const int active_face_set_id)
{
  Mesh *mesh = BKE_mesh_from_object(ob);
  for (int p = 0; p < mesh->totpoly; p++) {
    const MPoly *c_poly = &mesh->mpoly[p];
    for (int l = 0; l < c_poly->totloop; l++) {
      const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
      const MeshElemMap *vert_map = &ss->pmap[c_loop->v];
      for (int i = 0; i < vert_map->count; i++) {
        const int neighbor_face_index = vert_map->indices[i];
        if (neighbor_face_index != p) {

          if (abs(prev_face_sets[neighbor_face_index]) == active_face_set_id) {
            ss->face_sets[p] = active_face_set_id;
          }
        }
      }
    }
  }
}

static void sculpt_face_set_shrink(Object *ob,
                                   SculptSession *ss,
                                   int *prev_face_sets,
                                   const int active_face_set_id)
{
  Mesh *mesh = BKE_mesh_from_object(ob);
  for (int p = 0; p < mesh->totpoly; p++) {
    if (abs(prev_face_sets[p]) == active_face_set_id) {
      const MPoly *c_poly = &mesh->mpoly[p];
      for (int l = 0; l < c_poly->totloop; l++) {
        const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
        const MeshElemMap *vert_map = &ss->pmap[c_loop->v];
        for (int i = 0; i < vert_map->count; i++) {
          const int neighbor_face_index = vert_map->indices[i];
          if (neighbor_face_index != p) {
            if (abs(prev_face_sets[neighbor_face_index]) != active_face_set_id) {
              ss->face_sets[p] = prev_face_sets[neighbor_face_index];
            }
          }
        }
      }
    }
  }
}

static void sculpt_face_set_apply_edit(Object *ob, const int active_face_set_id, const int mode)
{
  SculptSession *ss = ob->sculpt;

  int *prev_face_sets = MEM_dupallocN(ss->face_sets);

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_GROW:
      sculpt_face_set_grow(ob, ss, prev_face_sets, active_face_set_id);
      break;
    case SCULPT_FACE_SET_EDIT_SHRINK:
      sculpt_face_set_shrink(ob, ss, prev_face_sets, active_face_set_id);
      break;
  }

  MEM_SAFE_FREE(prev_face_sets);
}

static int sculpt_face_set_edit_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin("face set edit");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  const int active_face_set = SCULPT_active_face_set_get(ss);

  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode);

  SCULPT_undo_push_end();

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  SCULPT_visibility_sync_all_face_sets_to_vertices(ss);

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

void SCULPT_OT_face_sets_edit(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Edit Face Set";
  ot->idname = "SCULPT_OT_face_set_edit";
  ot->description = "Edits the current active Face Set";

  /* Api callbacks. */
  ot->invoke = sculpt_face_set_edit_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_edit_types, SCULPT_FACE_SET_EDIT_GROW, "Mode", "");
}
