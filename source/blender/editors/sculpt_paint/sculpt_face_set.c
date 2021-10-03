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

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"
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
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

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

static int sculpt_face_material_get(SculptSession *ss, SculptFaceRef face)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;
      return f->mat_nr;
    }
    case PBVH_GRIDS:
    case PBVH_FACES:
      return ss->mpoly[face.i].mat_nr;
  }

  return -1;
}

int SCULPT_face_set_get(SculptSession *ss, SculptFaceRef face)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;
      return BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
    }
    case PBVH_GRIDS:
    case PBVH_FACES:
      return ss->face_sets[face.i];
  }
  return -1;
}

// returns previous face set
int SCULPT_face_set_set(SculptSession *ss, SculptFaceRef face, int fset)
{
  int ret = 0;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;
      ret = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

      BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      break;
    }
    case PBVH_FACES:
    case PBVH_GRIDS:
      ret = ss->face_sets[face.i];
      ss->face_sets[face.i] = fset;
      break;
  }

  return ret;
}

const char orig_faceset_attr_name[] = "_sculpt_original_fsets";

void SCULPT_face_check_origdata(SculptSession *ss, SculptFaceRef face)
{
  if (!ss->custom_layers[SCULPT_SCL_ORIG_FSETS]) {
    return;
  }

  short *s = (short *)SCULPT_temp_cdata_get_f(face, ss->custom_layers[SCULPT_SCL_ORIG_FSETS]);

  // pack ss->stroke_id in higher 16 bits
  if (s[1] != ss->stroke_id) {
    s[0] = SCULPT_face_set_get(ss, face);
    s[1] = ss->stroke_id;
  }
}

int SCULPT_face_set_original_get(SculptSession *ss, SculptFaceRef face)
{
  if (!ss->custom_layers[SCULPT_SCL_ORIG_FSETS]) {
    return SCULPT_face_set_get(ss, face);
  }

  short *s = (short *)SCULPT_temp_cdata_get_f(face, ss->custom_layers[SCULPT_SCL_ORIG_FSETS]);

  if (s[1] != ss->stroke_id) {
    s[0] = SCULPT_face_set_get(ss, face);
    s[1] = ss->stroke_id;
  }

  return s[0];
}

void SCULPT_face_ensure_original(SculptSession *ss)
{
  if (ss->custom_layers[SCULPT_SCL_ORIG_FSETS]) {
    return;
  }

  SculptCustomLayer *scl = MEM_callocN(sizeof(*scl), "orig fset scl");

  SCULPT_temp_customlayer_get(ss,
                              ATTR_DOMAIN_FACE,
                              CD_PROP_INT32,
                              "orig_faceset_attr_name",
                              scl,
                              &((SculptLayerParams){.permanent = false, .simple_array = false}));

  ss->custom_layers[SCULPT_SCL_ORIG_FSETS] = scl;
}

int SCULPT_face_set_flag_get(SculptSession *ss, SculptFaceRef face, char flag)
{
  if (ss->bm) {
    BMFace *f = (BMFace *)face.i;

    flag = BM_face_flag_from_mflag(flag);
    return f->head.hflag & flag;
  }
  else {
    return ss->mpoly[face.i].flag & flag;
  }
}

int SCULPT_face_set_flag_set(SculptSession *ss, SculptFaceRef face, char flag, bool state)
{
  int ret;

  if (ss->bm) {
    BMFace *f = (BMFace *)face.i;

    flag = BM_face_flag_from_mflag(flag);
    ret = f->head.hflag & flag;

    if (state) {
      f->head.hflag |= flag;
    }
    else {
      f->head.hflag &= ~flag;
    }
  }
  else {
    ret = ss->mpoly[face.i].flag & flag;

    if (state) {
      ss->mpoly[face.i].flag |= flag;
    }
    else {
      ss->mpoly[face.i].flag &= ~flag;
    }
  }

  return ret;
}
/* Utils. */
int ED_sculpt_face_sets_find_next_available_id(struct Mesh *mesh)
{
  int *face_sets = CustomData_get_layer(&mesh->pdata, CD_SCULPT_FACE_SETS);
  if (!face_sets) {
    return SCULPT_FACE_SET_NONE;
  }

  int next_face_set_id = 0;
  for (int i = 0; i < mesh->totpoly; i++) {
    next_face_set_id = max_ii(next_face_set_id, abs(face_sets[i]));
  }
  next_face_set_id++;

  return next_face_set_id;
}

void ED_sculpt_face_sets_initialize_none_to_id(struct Mesh *mesh, const int new_id)
{
  int *face_sets = CustomData_get_layer(&mesh->pdata, CD_SCULPT_FACE_SETS);
  if (!face_sets) {
    return;
  }

  for (int i = 0; i < mesh->totpoly; i++) {
    if (face_sets[i] == SCULPT_FACE_SET_NONE) {
      face_sets[i] = new_id;
    }
  }
}

int ED_sculpt_face_sets_active_update_and_get(bContext *C, Object *ob, const float mval[2])
{
  SculptSession *ss = ob->sculpt;
  if (!ss) {
    return SCULPT_FACE_SET_NONE;
  }

  SculptCursorGeometryInfo gi;
  if (!SCULPT_cursor_geometry_info_update(C, &gi, mval, false, false)) {
    return SCULPT_FACE_SET_NONE;
  }

  return SCULPT_active_face_set_get(ss);
}

static BMesh *sculpt_faceset_bm_begin(SculptSession *ss, Mesh *mesh)
{
  if (ss->bm) {
    return ss->bm;
  }

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
  BMesh *bm = BM_mesh_create(&allocsize,
                             &((struct BMeshCreateParams){
                                 .use_toolflags = true,
                             }));

  BM_mesh_bm_from_me(NULL,
                     bm,
                     mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));
  return bm;
}

static void sculpt_faceset_bm_end(SculptSession *ss, BMesh *bm)
{
  if (bm != ss->bm) {
    BM_mesh_free(bm);
  }
}

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
  const int active_fset = abs(ss->cache->paint_face_set);

  MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
  const float test_limit = 0.05f;
  int cd_mask = -1;

  if (ss->bm) {
    cd_mask = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);
  }

  /*check if we need to sample the current face set*/

  bool set_active_faceset = ss->cache->automasking &&
                            (brush->automasking_flags & BRUSH_AUTOMASKING_FACE_SETS);
  set_active_faceset = set_active_faceset && ss->cache->invert;
  set_active_faceset = set_active_faceset && ss->cache->automasking->settings.initial_face_set ==
                                                 ss->cache->automasking->settings.current_face_set;

  int automasking_fset_flag = 0;

  if (set_active_faceset) {
    // temporarily clear faceset flag
    automasking_fset_flag = ss->cache->automasking ? ss->cache->automasking->settings.flags &
                                                         BRUSH_AUTOMASKING_FACE_SETS :
                                                     0;
    ss->cache->automasking->settings.flags &= ~BRUSH_AUTOMASKING_FACE_SETS;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
      MeshElemMap *vert_map = &ss->pmap[vd.index];
      for (int j = 0; j < ss->pmap[vd.index].count; j++) {
        const MPoly *p = &ss->mpoly[vert_map->indices[j]];

        float poly_center[3];
        BKE_mesh_calc_poly_center(p, &ss->mloop[p->loopstart], mvert, poly_center);

        if (!sculpt_brush_test_sq_fn(&test, poly_center)) {
          continue;
        }
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask ? *vd.mask : 0.0f,
                                                                    vd.vertex,
                                                                    thread_id);

        if (fade > test_limit && ss->face_sets[vert_map->indices[j]] > 0) {
          bool ok = true;

          int fset = abs(ss->face_sets[vert_map->indices[j]]);

          // XXX kind of hackish, tries to sample faces that are within
          // 8 pixels of the center of the brush, and using a crude linear
          // scale at that - joeedh
          if (set_active_faceset &&
              fset != abs(ss->cache->automasking->settings.initial_face_set)) {

            float radius = ss->cache->radius;
            float pixels = 8;  // TODO: multiply with DPI
            radius = pixels * (radius / (float)ss->cache->dyntopo_pixel_radius);

            if (sqrtf(test.dist) < radius) {
              ss->cache->automasking->settings.initial_face_set = abs(fset);
              set_active_faceset = false;
              ss->cache->automasking->settings.flags |= BRUSH_AUTOMASKING_FACE_SETS;
            }
            else {
              ok = false;
            }
          }

          MLoop *ml = &ss->mloop[p->loopstart];

          for (int i = 0; i < p->totloop; i++, ml++) {
            MVert *v = &ss->mvert[ml->v];
            float fno[3];

            normal_short_to_float_v3(fno, v->no);
            float mask = ss->vmask ? ss->vmask[ml->v] : 0.0f;

            const float fade2 = bstrength *
                                SCULPT_brush_strength_factor(ss,
                                                             brush,
                                                             v->co,
                                                             sqrtf(test.dist),
                                                             v->no,
                                                             fno,
                                                             mask,
                                                             (SculptVertRef){.i = ml->v},
                                                             thread_id);

            if (fade2 < test_limit) {
              ok = false;
              break;
            }
          }

          if (ok) {
            ss->face_sets[vert_map->indices[j]] = abs(ss->cache->paint_face_set);
          }
        }
      }
    }
    else if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
      BMVert *v = vd.bm_vert;
      BMIter iter;
      BMFace *f;

      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        float poly_center[3];
        BM_face_calc_center_median(f, poly_center);

        if (sculpt_brush_test_sq_fn(&test, poly_center)) {
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      sqrtf(test.dist),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask ? *vd.mask : 0.0f,
                                                                      vd.vertex,
                                                                      thread_id);

          int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

          if (fade > test_limit && fset > 0) {
            BMLoop *l = f->l_first;

            bool ok = true;

            // XXX kind of hackish, tries to sample faces that are within
            // 8 pixels of the center of the brush, and using a crude linear
            // scale at that - joeedh
            if (set_active_faceset &&
                abs(fset) != abs(ss->cache->automasking->settings.initial_face_set)) {

              float radius = ss->cache->radius;
              float pixels = 8;  // TODO: multiple with DPI
              radius = pixels * (radius / (float)ss->cache->dyntopo_pixel_radius);

              if (sqrtf(test.dist) < radius) {
                ss->cache->automasking->settings.initial_face_set = abs(fset);
                set_active_faceset = false;
                ss->cache->automasking->settings.flags |= BRUSH_AUTOMASKING_FACE_SETS;
              }
              else {
                ok = false;
              }
            }

            do {
              short sno[3];
              float mask = cd_mask >= 0 ? BM_ELEM_CD_GET_FLOAT(l->v, cd_mask) : 0.0f;

              normal_float_to_short_v3(sno, l->v->no);

              const float fade2 = bstrength * SCULPT_brush_strength_factor(
                                                  ss,
                                                  brush,
                                                  l->v->co,
                                                  sqrtf(test.dist),
                                                  sno,
                                                  l->v->no,
                                                  mask,
                                                  (SculptVertRef){.i = (intptr_t)l->v},
                                                  thread_id);

              if (fade2 < test_limit) {
                ok = false;
                break;
              }

              MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, l->v);
              mv->flag |= DYNVERT_NEED_BOUNDARY;
            } while ((l = l->next) != f->l_first);

            if (ok) {
              BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, active_fset);
            }
          }
        }
      }
    }
    else if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      {
        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask ? *vd.mask : 0.0f,
                                                                    vd.vertex,
                                                                    thread_id);

        if (fade > 0.05f) {
          SCULPT_vertex_face_set_set(ss, vd.vertex, ss->cache->paint_face_set);
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  // restore automasking flag
  if (set_active_faceset) {
    ss->cache->automasking->settings.flags |= automasking_fset_flag;
  }
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
  /* This operations needs a strength tweak as the relax deformation is too weak by default. */
  if (relax_face_sets) {
    bstrength *= 2.0f;
  }

  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    if (relax_face_sets == SCULPT_vertex_has_unique_face_set(ss, vd.vertex)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    SCULPT_relax_vertex(ss, &vd, fade * bstrength, relax_face_sets, vd.co);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_draw_face_sets_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  bool threaded = true;

  /*for ctrl invert mode we have to set the automasking initial_face_set
    to the first non-current faceset that is found*/
  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    if (ss->cache->invert && ss->cache->automasking &&
        (brush->automasking_flags & BRUSH_AUTOMASKING_FACE_SETS)) {
      ss->cache->automasking->settings.current_face_set =
          ss->cache->automasking->settings.initial_face_set;
    }
  }

  if (ss->cache->invert && !ss->cache->alt_smooth && ss->cache->automasking &&
      ss->cache->automasking->settings.initial_face_set ==
          ss->cache->automasking->settings.current_face_set) {
    threaded = false;
  }

  // ctrl-click is single threaded since the tasks will set the initial face set
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, threaded, totnode);
  if (ss->cache->alt_smooth) {
    SCULPT_boundary_info_ensure(ob);
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
        "Face Set from Masked",
        "Create a new Face Set from the masked faces",
    },
    {
        SCULPT_FACE_SET_VISIBLE,
        "VISIBLE",
        0,
        "Face Set from Visible",
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
        "Face Set from Edit Mode Selection",
        "Create an Face Set corresponding to the Edit Mode face selection",
    },
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, mode == SCULPT_FACE_SET_MASKED, false);

  SCULPT_face_random_access_ensure(ss);
  SCULPT_vertex_random_access_ensure(ss);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  float threshold = 0.5f;

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, "face set change");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  const int next_face_set = SCULPT_face_set_next_available_get(ss);

  if (mode == SCULPT_FACE_SET_MASKED) {
    for (int i = 0; i < tot_vert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      if (SCULPT_vertex_mask_get(ss, vertex) >= threshold &&
          SCULPT_vertex_visible_get(ss, vertex)) {
        SCULPT_vertex_face_set_set(ss, vertex, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_VISIBLE) {

    /* If all vertices in the sculpt are visible, create the new face set and update the default
     * color. This way the new face set will be white, which is a quick way of disabling all face
     * sets and the performance hit of rendering the overlay. */
    bool all_visible = true;
    for (int i = 0; i < tot_vert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      if (!SCULPT_vertex_visible_get(ss, vertex)) {
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
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      if (SCULPT_vertex_visible_get(ss, vertex)) {
        SCULPT_vertex_face_set_set(ss, vertex, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_ALL) {
    for (int i = 0; i < tot_vert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_face_set_set(ss, vertex, next_face_set);
    }
  }

  if (mode == SCULPT_FACE_SET_SELECTION) {
    const int totface = ss->totfaces;

    for (int i = 0; i < totface; i++) {
      SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, i);

      // XXX check hidden?
      int ok = !SCULPT_face_set_flag_get(ss, fref, ME_HIDE);
      ok = ok && SCULPT_face_set_flag_get(ss, fref, ME_FACE_SEL);

      if (ok) {
        SCULPT_face_set_set(ss, fref, next_face_set);
      }
    }
  }

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);

  SCULPT_undo_push_end();

  SCULPT_tag_update_overlays(C);

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
  SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES = 8,
} eSculptFaceSetsInitMode;

static EnumPropertyItem prop_sculpt_face_sets_init_types[] = {
    {
        SCULPT_FACE_SETS_FROM_LOOSE_PARTS,
        "LOOSE_PARTS",
        0,
        "Face Sets from Loose Parts",
        "Create a Face Set per loose part in the mesh",
    },
    {
        SCULPT_FACE_SETS_FROM_MATERIALS,
        "MATERIALS",
        0,
        "Face Sets from Material Slots",
        "Create a Face Set per Material Slot",
    },
    {
        SCULPT_FACE_SETS_FROM_NORMALS,
        "NORMALS",
        0,
        "Face Sets from Mesh Normals",
        "Create Face Sets for Faces that have similar normal",
    },
    {
        SCULPT_FACE_SETS_FROM_UV_SEAMS,
        "UV_SEAMS",
        0,
        "Face Sets from UV Seams",
        "Create Face Sets using UV Seams as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_CREASES,
        "CREASES",
        0,
        "Face Sets from Edge Creases",
        "Create Face Sets using Edge Creases as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT,
        "BEVEL_WEIGHT",
        0,
        "Face Sets from Bevel Weight",
        "Create Face Sets using Bevel Weights as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_SHARP_EDGES,
        "SHARP_EDGES",
        0,
        "Face Sets from Sharp Edges",
        "Create Face Sets using Sharp Edges as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_FACE_MAPS,
        "FACE_MAPS",
        0,
        "Face Sets from Face Maps",
        "Create a Face Set per Face Map",
    },
    {
        SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES,
        "FACE_SET_BOUNDARIES",
        0,
        "Face Sets from Face Set Boundaries",
        "Create a Face Set per isolated Face Set",
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

static bool sculpt_face_sets_init_face_set_boundary_test(
    BMesh *bm, BMFace *from_f, BMEdge *UNUSED(from_e), BMFace *to_f, const float UNUSED(threshold))
{
  const int cd_face_sets_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);
  return BM_ELEM_CD_GET_INT(from_f, cd_face_sets_offset) ==
         BM_ELEM_CD_GET_INT(to_f, cd_face_sets_offset);
}

static void sculpt_face_sets_init_flood_fill(Object *ob,
                                             face_sets_flood_fill_test test,
                                             const float threshold)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = ob->data;
  BMesh *bm;

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  bm = sculpt_faceset_bm_begin(ss, mesh);

  BLI_bitmap *visited_faces = BLI_BITMAP_NEW(ss->totfaces, "visited faces");
  const int totfaces = ss->totfaces;  // mesh->totpoly;

  if (!ss->bm) {
    BM_mesh_elem_index_ensure(bm, BM_FACE);
    BM_mesh_elem_table_ensure(bm, BM_FACE);
  }

  int next_face_set = 1;

  for (int i = 0; i < totfaces; i++) {
    if (BLI_BITMAP_TEST(visited_faces, i)) {
      continue;
    }
    GSQueue *queue;
    queue = BLI_gsqueue_new(sizeof(int));

    SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, i);
    SCULPT_face_set_set(ss, fref, next_face_set);

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
          if (f_neighbor == f) {
            continue;
          }
          int neighbor_face_index = BM_elem_index_get(f_neighbor);
          if (BLI_BITMAP_TEST(visited_faces, neighbor_face_index)) {
            continue;
          }
          if (!test(bm, f, ed, f_neighbor, threshold)) {
            continue;
          }

          SculptFaceRef fref2 = BKE_pbvh_table_index_to_face(ss->pbvh, neighbor_face_index);
          SCULPT_face_set_set(ss, fref2, next_face_set);

          BLI_BITMAP_ENABLE(visited_faces, neighbor_face_index);
          BLI_gsqueue_push(queue, &neighbor_face_index);
        }
      }
    }

    next_face_set += 1;

    BLI_gsqueue_free(queue);
  }

  MEM_SAFE_FREE(visited_faces);

  sculpt_faceset_bm_end(ss, bm);
}

static void sculpt_face_sets_init_loop(Object *ob, const int mode)
{
  SculptSession *ss = ob->sculpt;

  SCULPT_face_random_access_ensure(ss);

  int cd_fmaps_offset = -1;
  if (ss->bm) {
    cd_fmaps_offset = CustomData_get_offset(&ss->bm->pdata, CD_FACEMAP);
  }

  Mesh *me = NULL;
  int *fmaps = NULL;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    me = ob->data;
    fmaps = CustomData_get_layer(&me->pdata, CD_FACEMAP);
  }
  else if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    fmaps = CustomData_get_layer(ss->pdata, CD_FACEMAP);
  }

  for (int i = 0; i < ss->totfaces; i++) {
    SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, i);

    if (mode == SCULPT_FACE_SETS_FROM_MATERIALS) {
      SCULPT_face_set_set(ss, fref, (int)(sculpt_face_material_get(ss, fref) + 1));
    }
    else if (mode == SCULPT_FACE_SETS_FROM_FACE_MAPS) {
      int fmap = 1;

      switch (BKE_pbvh_type(ss->pbvh)) {
        case PBVH_BMESH: {
          BMFace *f = (BMFace *)fref.i;

          if (cd_fmaps_offset >= 0) {
            fmap = BM_ELEM_CD_GET_INT(f, cd_fmaps_offset) + 2;
          }

          break;
        }
        case PBVH_FACES:
        case PBVH_GRIDS: {
          if (fmaps) {
            fmap = fmaps[i] + 2;
          }
          break;
        }
      }

      SCULPT_face_set_set(ss, fref, fmap);
    }
  }
}

static int sculpt_face_set_init_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, "face set change");
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
    case SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES:
      sculpt_face_sets_init_flood_fill(
          ob, sculpt_face_sets_init_face_set_boundary_test, threshold);
      break;
    case SCULPT_FACE_SETS_FROM_FACE_MAPS:
      sculpt_face_sets_init_loop(ob, SCULPT_FACE_SETS_FROM_FACE_MAPS);
      break;
  }

  SCULPT_undo_push_end();

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  SCULPT_visibility_sync_all_face_sets_to_vertices(ob);

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_visibility(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  MEM_SAFE_FREE(nodes);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts(ob->data);
  }

  SCULPT_tag_update_overlays(C);

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
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  const int mode = RNA_enum_get(op->ptr, "mode");
  const int active_face_set = SCULPT_active_face_set_get(ss);

  SCULPT_undo_push_begin(ob, "Hide area");

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
        SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

        if (!SCULPT_vertex_visible_get(ss, vertex)) {
          hidden_vertex = true;
          break;
        }
      }
    }
    else if (ss->bm) {
      BMIter iter;
      BMFace *f;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        if (BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset) <= 0) {
          hidden_vertex = true;
          break;
        }
      }
    }
    else {
      for (int i = 0; i < ss->totfaces; i++) {
        if (ss->face_sets[i] <= 0) {
          hidden_vertex = true;
          break;
        }
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
  SCULPT_visibility_sync_all_face_sets_to_vertices(ob);

  SCULPT_undo_push_end();

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_visibility(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  MEM_SAFE_FREE(nodes);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static int sculpt_face_sets_change_visibility_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  /* Update the active vertex and Face Set using the cursor position to avoid relying on the
   * paint cursor updates. */
  SculptCursorGeometryInfo sgi;
  float mouse[2];
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);

  return sculpt_face_sets_change_visibility_exec(C, op);
}

void SCULPT_OT_face_sets_change_visibility(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  /* Api callbacks. */
  ot->exec = sculpt_face_sets_change_visibility_exec;
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

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator *UNUSED(op))
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  Mesh *mesh = ob->data;

  SCULPT_face_random_access_ensure(ss);

  mesh->face_sets_color_seed += 1;
  if (ss->face_sets || (ss->bm && ss->cd_faceset_offset >= 0)) {
    const int random_index = clamp_i(ss->totfaces * BLI_hash_int_01(mesh->face_sets_color_seed),
                                     0,
                                     max_ii(0, ss->totfaces - 1));

    SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, random_index);
    mesh->face_sets_color_default = SCULPT_face_set_get(ss, fref);
  }
  BKE_pbvh_face_sets_color_set(pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);

  SCULPT_tag_update_overlays(C);

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
  SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY = 2,
  SCULPT_FACE_SET_EDIT_FAIR_POSITIONS = 3,
  SCULPT_FACE_SET_EDIT_FAIR_TANGENCY = 4,
  SCULPT_FACE_SET_EDIT_FAIR_CURVATURE = 5,
  SCULPT_FACE_SET_EDIT_FILL_COMPONENT = 6,
  SCULPT_FACE_SET_EDIT_EXTRUDE = 7,
  SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY = 8,
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
    {
        SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY,
        "DELETE_GEOMETRY",
        0,
        "Delete Geometry",
        "Deletes the faces that are assigned to the Face Set",
    },
    {
        SCULPT_FACE_SET_EDIT_FAIR_POSITIONS,
        "FAIR_POSITIONS",
        0,
        "Fair Positions",
        "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
        "vertex positions",
    },
    {
        SCULPT_FACE_SET_EDIT_FAIR_TANGENCY,
        "FAIR_TANGENCY",
        0,
        "Fair Tangency",
        "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
        "vertex tangents",
    },
    /*
    {
        SCULPT_FACE_SET_EDIT_FAIR_CURVATURE,
        "FAIR_CURVATURE",
        0,
        "Fair Curvature",
        "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
        "surface curvature",
    },
    */
    {
        SCULPT_FACE_SET_EDIT_FILL_COMPONENT,
        "FILL_COMPONENT",
        0,
        "Fill Component",
        "Expand a Face Set to fill all affected connected components",
    },
    {
        SCULPT_FACE_SET_EDIT_EXTRUDE,
        "EXTRUDE",
        0,
        "Extrude",
        "Extrude a Face Set along the normals of the faces",
    },
    {
        SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY,
        "ALL_TANGENCY",
        0,
        "All tangency",
        "Extrude a Face Set along the normals of the faces",
    },
    {0, NULL, 0, NULL, NULL},
};

static void sculpt_face_set_grow_bmesh(Object *ob,
                                       SculptSession *ss,
                                       const int *prev_face_sets,
                                       const int active_face_set_id,
                                       const bool modify_hidden)
{
  BMesh *bm = ss->bm;
  BMIter iter;
  BMFace *f;
  BMFace **faces = NULL;
  BLI_array_declare(faces);

  if (ss->cd_faceset_offset < 0) {
    return;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) && !modify_hidden) {
      continue;
    }

    int fset = abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset));

    if (fset == active_face_set_id) {
      BLI_array_append(faces, f);
    }
  }

  for (int i = 0; i < BLI_array_len(faces); i++) {
    BMFace *f = faces[i];
    BMLoop *l = f->l_first;

    do {
      if (l->radial_next != l) {
        BM_ELEM_CD_SET_INT(l->radial_next->f, ss->cd_faceset_offset, active_face_set_id);
      }
      l = l->next;
    } while (l != f->l_first);
  }

  BLI_array_free(faces);
}

static void sculpt_face_set_grow(Object *ob,
                                 SculptSession *ss,
                                 const int *prev_face_sets,
                                 const int active_face_set_id,
                                 const bool modify_hidden)
{
  if (ss && ss->bm) {
    sculpt_face_set_grow_bmesh(ob, ss, prev_face_sets, active_face_set_id, modify_hidden);
    return;
  }

  Mesh *mesh = BKE_mesh_from_object(ob);
  for (int p = 0; p < mesh->totpoly; p++) {
    if (!modify_hidden && prev_face_sets[p] <= 0) {
      continue;
    }
    const MPoly *c_poly = &mesh->mpoly[p];
    for (int l = 0; l < c_poly->totloop; l++) {
      const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
      const MeshElemMap *vert_map = &ss->pmap[c_loop->v];
      for (int i = 0; i < vert_map->count; i++) {
        const int neighbor_face_index = vert_map->indices[i];
        if (neighbor_face_index == p) {
          continue;
        }
        if (abs(prev_face_sets[neighbor_face_index]) == active_face_set_id) {
          ss->face_sets[p] = active_face_set_id;
        }
      }
    }
  }
}

static void sculpt_face_set_fill_component(Object *ob,
                                           SculptSession *ss,
                                           const int active_face_set_id,
                                           const bool UNUSED(modify_hidden))
{
  SCULPT_connected_components_ensure(ob);
  GSet *connected_components = BLI_gset_int_new("affected_components");

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_face_set(ss, vertex, active_face_set_id)) {
      continue;
    }
    const int vertex_connected_component = ss->vertex_info.connected_component[i];
    if (BLI_gset_haskey(connected_components, POINTER_FROM_INT(vertex_connected_component))) {
      continue;
    }
    BLI_gset_add(connected_components, POINTER_FROM_INT(vertex_connected_component));
  }

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    const int vertex_connected_component = ss->vertex_info.connected_component[i];
    if (!BLI_gset_haskey(connected_components, POINTER_FROM_INT(vertex_connected_component))) {
      continue;
    }

    SCULPT_vertex_face_set_set(ss, vertex, active_face_set_id);
  }

  BLI_gset_free(connected_components, NULL);
}

static void sculpt_face_set_shrink_bmesh(Object *ob,
                                         SculptSession *ss,
                                         const int *prev_face_sets,
                                         const int active_face_set_id,
                                         const bool modify_hidden)
{
  BMesh *bm = ss->bm;
  BMIter iter;
  BMFace *f;
  BMFace **faces = NULL;
  BLI_array_declare(faces);

  if (ss->cd_faceset_offset < 0) {
    return;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) && !modify_hidden) {
      continue;
    }

    int fset = abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset));

    if (fset == active_face_set_id) {
      BLI_array_append(faces, f);
    }
  }

  for (int i = 0; i < BLI_array_len(faces); i++) {
    BMFace *f = faces[i];
    BMLoop *l = f->l_first;

    do {
      if (!modify_hidden && BM_elem_flag_test(l->radial_next->f, BM_ELEM_HIDDEN)) {
        l = l->next;
        continue;
      }

      if (l->radial_next != l &&
          abs(BM_ELEM_CD_GET_INT(l->radial_next->f, ss->cd_faceset_offset)) !=
              abs(active_face_set_id)) {
        BM_ELEM_CD_SET_INT(f,
                           ss->cd_faceset_offset,
                           BM_ELEM_CD_GET_INT(l->radial_next->f, ss->cd_faceset_offset));
        break;
      }
      l = l->next;
    } while (l != f->l_first);
  }

  BLI_array_free(faces);
}

static void sculpt_face_set_shrink(Object *ob,
                                   SculptSession *ss,
                                   const int *prev_face_sets,
                                   const int active_face_set_id,
                                   const bool modify_hidden)
{
  if (ss && ss->bm) {
    sculpt_face_set_shrink_bmesh(ob, ss, prev_face_sets, active_face_set_id, modify_hidden);
    return;
  }

  Mesh *mesh = BKE_mesh_from_object(ob);
  for (int p = 0; p < mesh->totpoly; p++) {
    if (!modify_hidden && prev_face_sets[p] <= 0) {
      continue;
    }
    if (abs(prev_face_sets[p]) == active_face_set_id) {
      const MPoly *c_poly = &mesh->mpoly[p];
      for (int l = 0; l < c_poly->totloop; l++) {
        const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
        const MeshElemMap *vert_map = &ss->pmap[c_loop->v];
        for (int i = 0; i < vert_map->count; i++) {
          const int neighbor_face_index = vert_map->indices[i];
          if (neighbor_face_index == p) {
            continue;
          }
          if (abs(prev_face_sets[neighbor_face_index]) != active_face_set_id) {
            ss->face_sets[p] = prev_face_sets[neighbor_face_index];
          }
        }
      }
    }
  }
}

static bool check_single_face_set(SculptSession *ss, const bool check_visible_only)
{
  if (!ss->totfaces) {
    return true;
  }

  int first_face_set = SCULPT_FACE_SET_NONE;

  if (check_visible_only) {
    for (int f = 0; f < ss->totfaces; f++) {
      SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, f);
      int fset = SCULPT_face_set_get(ss, fref);

      if (fset > 0) {
        first_face_set = fset;
        break;
      }
    }
  }
  else {
    SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, 0);
    first_face_set = abs(SCULPT_face_set_get(ss, fref));
  }

  if (first_face_set == SCULPT_FACE_SET_NONE) {
    return true;
  }

  for (int f = 0; f < ss->totfaces; f++) {
    SculptFaceRef fref = BKE_pbvh_table_index_to_face(ss->pbvh, f);

    int fset = SCULPT_face_set_get(ss, fref);
    fset = check_visible_only ? abs(fset) : fset;

    if (fset != first_face_set) {
      return false;
    }
  }
  return true;
}

static void sculpt_face_set_delete_geometry(Object *ob,
                                            SculptSession *ss,
                                            const int active_face_set_id,
                                            const bool modify_hidden)
{

  Mesh *mesh = ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  if (ss->bm) {
    BMFace **faces = NULL;
    BLI_array_declare(faces);

    BMIter iter;
    BMFace *f;

    BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
      const int face_set_id = modify_hidden ? abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset)) :
                                              BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
      if (face_set_id == active_face_set_id) {
        BLI_array_append(faces, f);
      }
    }

    for (int i = 0; i < BLI_array_len(faces); i++) {
      BKE_pbvh_bmesh_remove_face(ss->pbvh, faces[i], true);
    }

    BLI_array_free(faces);
  }
  else {
    BMesh *bm = BM_mesh_create(&allocsize,
                               &((struct BMeshCreateParams){
                                   .use_toolflags = true,
                               }));

    BM_mesh_bm_from_me(ob,
                       bm,
                       mesh,
                       (&(struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));

    BM_mesh_elem_table_init(bm, BM_FACE);
    BM_mesh_elem_table_ensure(bm, BM_FACE);
    BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      const int face_index = BM_elem_index_get(f);
      const int face_set_id = modify_hidden ? abs(ss->face_sets[face_index]) :
                                              ss->face_sets[face_index];
      BM_elem_flag_set(f, BM_ELEM_TAG, face_set_id == active_face_set_id);
    }
    BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
    BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

    BM_mesh_bm_to_me(NULL,
                     ob,
                     bm,
                     ob->data,
                     (&(struct BMeshToMeshParams){
                         .calc_object_remap = false,
                     }));

    BM_mesh_free(bm);
  }
}

static void sculpt_face_set_edit_fair_face_set(Object *ob,
                                               const int active_face_set_id,
                                               const int fair_order)
{
  SculptSession *ss = ob->sculpt;

  const int totvert = SCULPT_vertex_count_get(ss);

  Mesh *mesh = ob->data;
  bool *fair_vertices = MEM_malloc_arrayN(sizeof(bool), totvert, "fair vertices");

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    fair_vertices[i] = !SCULPT_vertex_is_boundary(ss, vref, SCULPT_BOUNDARY_MESH) &&
                       SCULPT_vertex_has_face_set(ss, vref, active_face_set_id) &&
                       SCULPT_vertex_has_unique_face_set(ss, vref);
  }

  if (ss->bm) {
    BKE_bmesh_prefair_and_fair_vertices(ss->bm, fair_vertices, fair_order);
  }
  else {
    MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
    BKE_mesh_prefair_and_fair_vertices(mesh, mvert, fair_vertices, fair_order);
  }

  MEM_freeN(fair_vertices);
}

static void sculpt_face_set_apply_edit(Object *ob,
                                       const int active_face_set_id,
                                       const int mode,
                                       const bool modify_hidden)
{
  SculptSession *ss = ob->sculpt;

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_GROW: {
      int *prev_face_sets = ss->face_sets ? MEM_dupallocN(ss->face_sets) : NULL;
      sculpt_face_set_grow(ob, ss, prev_face_sets, active_face_set_id, modify_hidden);
      MEM_SAFE_FREE(prev_face_sets);
      break;
    }
    case SCULPT_FACE_SET_EDIT_SHRINK: {
      int *prev_face_sets = ss->face_sets ? MEM_dupallocN(ss->face_sets) : NULL;
      sculpt_face_set_shrink(ob, ss, prev_face_sets, active_face_set_id, modify_hidden);
      MEM_SAFE_FREE(prev_face_sets);
      break;
    }
    case SCULPT_FACE_SET_EDIT_FILL_COMPONENT: {
      sculpt_face_set_fill_component(ob, ss, active_face_set_id, modify_hidden);
      break;
    }
    case SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY:
      sculpt_face_set_delete_geometry(ob, ss, active_face_set_id, modify_hidden);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_POSITIONS:
      sculpt_face_set_edit_fair_face_set(ob, active_face_set_id, MESH_FAIRING_DEPTH_POSITION);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
      sculpt_face_set_edit_fair_face_set(ob, active_face_set_id, MESH_FAIRING_DEPTH_TANGENCY);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY: {
      GSet *face_sets_ids = BLI_gset_int_new("ids");
      for (int i = 0; i < ss->totfaces; i++) {
        BLI_gset_add(face_sets_ids, POINTER_FROM_INT(ss->face_sets[i]));
      }

      GSetIterator gs_iter;
      GSET_ITER (gs_iter, face_sets_ids) {
        const int face_set_id = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
        sculpt_face_set_edit_fair_face_set(ob, face_set_id, MESH_FAIRING_DEPTH_TANGENCY);
      }

      BLI_gset_free(face_sets_ids, NULL);
    } break;
    case SCULPT_FACE_SET_EDIT_FAIR_CURVATURE:
      sculpt_face_set_edit_fair_face_set(ob, active_face_set_id, MESH_FAIRING_DEPTH_CURVATURE);
      break;
  }
}

static bool sculpt_face_set_edit_is_operation_valid(SculptSession *ss,
                                                    const eSculptFaceSetEditMode mode,
                                                    const bool modify_hidden)
{
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  if (ELEM(mode, SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY, SCULPT_FACE_SET_EDIT_EXTRUDE)) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      /* Modification of base mesh geometry requires special remapping of multires displacement,
       * which does not happen here.
       * Disable delete operation. It can be supported in the future by doing similar
       * displacement data remapping as what happens in the mesh edit mode. */
      return false;
    }
    if (check_single_face_set(ss, !modify_hidden)) {
      /* Cancel the operator if the mesh only contains one Face Set to avoid deleting the
       * entire object. */
      return false;
    }
  }

  if (ELEM(mode, SCULPT_FACE_SET_EDIT_FAIR_POSITIONS, SCULPT_FACE_SET_EDIT_FAIR_TANGENCY)) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      /* TODO: Multires topology representation using grids and duplicates can't be used directly
       * by the fair algorithm. Multires topology needs to be exposed in a different way or
       * converted to a mesh for this operation. */
      return false;
    }
  }

  return true;
}

static void sculpt_face_set_edit_modify_geometry(bContext *C,
                                                 Object *ob,
                                                 const int active_face_set,
                                                 const eSculptFaceSetEditMode mode,
                                                 const bool modify_hidden)
{
  ED_sculpt_undo_geometry_begin(ob, "edit face set delete geometry");
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, modify_hidden);
  ED_sculpt_undo_geometry_end(ob);
  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static void face_set_edit_do_post_visibility_updates(Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  SCULPT_visibility_sync_all_face_sets_to_vertices(ob);

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_visibility(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts(ob->data);
  }
}

static void sculpt_face_set_edit_modify_face_sets(Object *ob,
                                                  const int active_face_set,
                                                  const eSculptFaceSetEditMode mode,
                                                  const bool modify_hidden)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (!nodes) {
    return;
  }
  SCULPT_undo_push_begin(ob, "face set edit");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, modify_hidden);
  SCULPT_undo_push_end();
  face_set_edit_do_post_visibility_updates(ob, nodes, totnode);
  MEM_freeN(nodes);
}

static void sculpt_face_set_edit_modify_coordinates(bContext *C,
                                                    Object *ob,
                                                    const int active_face_set,
                                                    const eSculptFaceSetEditMode mode)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  SCULPT_undo_push_begin(ob, "face set edit");
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update(nodes[i]);
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_COORDS);
  }
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, false);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
  SCULPT_undo_push_end();
  MEM_freeN(nodes);
}

typedef struct FaceSetExtrudeCD {
  int active_face_set;
  float cursor_location[3];
  float (*orig_co)[3];
  float init_mval[2];
  float (*orig_no)[3];
  int *verts;
  int totvert;
} FaceSetExtrudeCD;

static int sculpt_bm_mesh_elem_hflag_disable_all(BMesh *bm, char htype, char hflag)
{
  static int iters[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
  static int types[3] = {BM_VERT, BM_EDGE, BM_FACE};

  for (int i = 0; i < 3; i++) {
    int type = types[i];

    if (!(htype & type)) {
      continue;
    }

    BMIter iter;
    BMElem *elem;

    BM_ITER_MESH (elem, &iter, bm, iters[i]) {
      // do not call bm selection api
      // BM_elem_select_set(bm, elem, false);

      elem->head.hflag &= ~hflag;
    }
  }
}

void bmesh_radial_loop_append(BMEdge *e, BMLoop *l);
void bmesh_radial_loop_remove(BMEdge *e, BMLoop *l);
void bmesh_disk_edge_append(BMEdge *e, BMVert *v);
void bmesh_disk_edge_remove(BMEdge *e, BMVert *v);

ATTR_NO_OPT static void sculpt_face_set_extrude_id_2(
    Object *ob, SculptSession *ss, const int active_face_set_id, int **r_verts, int *r_totvert)
{
  Mesh *mesh = ob->data;
  const int next_face_set_id = ED_sculpt_face_sets_find_next_available_id(mesh);

  BMesh *bm = ss->bm ? ss->bm : sculpt_faceset_bm_begin(ss, mesh);

  BMVert **vs = NULL;
  BMEdge **es = NULL;
  BMFace **fs = NULL;
  BMVert **retvs = NULL;
  BMEdge **borderes = NULL;
  BMEdge **borderes_new = NULL;
  SmallHash *vmap = MEM_callocN(sizeof(*vmap), __func__);
  BLI_smallhash_init(vmap);
  SmallHash *emap = MEM_callocN(sizeof(*emap), __func__);
  BLI_smallhash_init(emap);

  BLI_array_declare(vs);
  BLI_array_declare(es);
  BLI_array_declare(fs);
  BLI_array_declare(retvs);

  BLI_array_declare(borderes);
  BLI_array_declare(borderes_new);

  BMIter iter;
  BMFace *f;

  BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);

  sculpt_bm_mesh_elem_hflag_disable_all(
      bm, BM_ALL_NOLOOP, BM_ELEM_SELECT | BM_ELEM_TAG_ALT | BM_ELEM_TAG);

  const int tag = BM_ELEM_TAG;
  const int tag2 = BM_ELEM_SELECT;

  BMLoop **ls = NULL;
  BMEdge **v1es = NULL;
  BMEdge **v2es = NULL;
  BMFace **borderfs1 = NULL;
  BMFace **borderfs2 = NULL;
  BMVert **bordervs1 = NULL;
  BMVert **bordervs2 = NULL;
  BMEdge **otheres = NULL;

  BLI_array_declare(otheres);
  BLI_array_declare(borderfs1);
  BLI_array_declare(borderfs2);
  BLI_array_declare(bordervs1);
  BLI_array_declare(bordervs2);
  BLI_array_declare(ls);
  BLI_array_declare(v1es);
  BLI_array_declare(v2es);

  // mark faceset region and its border edges
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

    if (fset == active_face_set_id) {
      f->head.hflag |= tag;

      BLI_array_append(fs, f);

      BMLoop *l = f->l_first;
      do {
        if (!(l->v->head.hflag & tag)) {
          l->v->head.hflag |= tag;

          BLI_array_append(vs, l->v);
          BLI_array_append(retvs, l->v);
        }

        if (!(l->e->head.hflag & tag)) {
          l->e->head.hflag |= tag;

          BLI_array_append(es, l->e);
        }
      } while ((l = l->next) != f->l_first);
    }
  }

  for (int i = 0; i < BLI_array_len(es); i++) {
    BMEdge *e = es[i];
    BMLoop *l2 = e->l;

    do {
      if (!(l2->f->head.hflag & tag)) {
        BLI_array_append(borderes, e);
        break;
      }
    } while ((l2 = l2->radial_next) != e->l);
  }

  for (int i = 0; i < BLI_array_len(borderes); i++) {
    BMEdge *e = borderes[i];
    BMLoop *l = e->l;

    e->head.hflag |= tag;

    if (!l) {
      continue;
    }

    BLI_array_clear(ls);
    do {
      if (l->f->head.hflag & tag2) {
        continue;
      }

      l->f->head.hflag |= tag2;

      if (l->f->head.hflag & tag) {
        BLI_array_append(borderfs1, l->f);
      }
      else {
        BLI_array_append(borderfs2, l->f);
      }

    } while ((l = l->radial_next) != e->l);
  }

  for (int i = 0; i < BLI_array_len(fs); i++) {
    fs[i]->head.hflag &= ~tag2;
  }

  for (int step = 0; step < 2; step++) {
    int len = step ? BLI_array_len(borderfs2) : BLI_array_len(borderfs1);
    BMFace **borderfs = step ? borderfs2 : borderfs1;

    for (int i = 0; i < len; i++) {
      BMFace *f = borderfs[i];
      BMLoop *l = f->l_first;

      do {
        if (l->e->head.hflag & tag) {
          bmesh_radial_loop_remove(l->e, l);
        }
        else if (!(l->e->head.hflag & tag) && !(l->e->head.hflag & tag2)) {
          l->e->head.hflag |= tag2;
          BLI_array_append(otheres, l->e);

          bmesh_disk_edge_remove(l->e, l->e->v1);
          bmesh_disk_edge_remove(l->e, l->e->v2);
        }
      } while ((l = l->next) != f->l_first);
    }
  }

  // create and splice in new border edges and vertices
  for (int i = 0; i < BLI_array_len(borderes); i++) {
    BMEdge *e = borderes[i];
    void **val = NULL;
    BMVert *v1_new, *v2_new;

    bmesh_disk_edge_remove(e, e->v1);
    bmesh_disk_edge_remove(e, e->v2);

    if (!BLI_smallhash_ensure_p(vmap, (uintptr_t)e->v1, &val)) {
      *val = v1_new = (void *)BM_vert_create(bm, e->v1->co, e->v1, BM_CREATE_NOP);
      BM_ELEM_CD_SET_INT(v1_new, ss->cd_face_node_offset, DYNTOPO_NODE_NONE);
    }
    else {
      v1_new = *val;
    }

    if (!BLI_smallhash_ensure_p(vmap, (uintptr_t)e->v2, &val)) {
      *val = v2_new = (void *)BM_vert_create(bm, e->v2->co, e->v2, BM_CREATE_NOP);
      BM_ELEM_CD_SET_INT(v2_new, ss->cd_face_node_offset, DYNTOPO_NODE_NONE);
    }
    else {
      v2_new = *val;
    }

    BMEdge *e_new = BM_edge_create(bm, v1_new, v2_new, e, BM_CREATE_NOP);
    e_new->head.hflag &= ~tag;

    bmesh_disk_edge_remove(e_new, v1_new);
    bmesh_disk_edge_remove(e_new, v2_new);

    // BM_log_edge_added(ss->bm_log, e_new);

    BLI_array_append(borderes_new, e_new);

    BLI_smallhash_insert(emap, (uintptr_t)e, e_new);
  }

  // relink borderfs1
  for (int i = 0; i < BLI_array_len(borderfs1); i++) {
    BMFace *f = borderfs1[i];
    BMLoop *l = f->l_first;

    do {
      bmesh_radial_loop_append(l->e, l);
    } while ((l = l->next) != f->l_first);
  }

  // relink borderfs2
  for (int i = 0; i < BLI_array_len(borderfs2); i++) {
    BMFace *f = borderfs2[i];
    BMLoop *l = f->l_first;

    do {
      if (l->e->head.hflag & tag) {
        l->e = BLI_smallhash_lookup(emap, l->e);
      }

      BMVert *newv = BLI_smallhash_lookup(vmap, l->v);
      if (newv) {
        l->v = newv;
      }
    } while ((l = l->next) != f->l_first);

    do {
      bmesh_radial_loop_append(l->e, l);
    } while ((l = l->next) != f->l_first);
  }

  for (int i = 0; i < BLI_array_len(borderes); i++) {
    BMEdge *e1 = borderes[i];
    BMEdge *e2 = borderes_new[i];

    bmesh_disk_edge_append(e1, e1->v1);
    bmesh_disk_edge_append(e1, e1->v2);

    bmesh_disk_edge_append(e2, e2->v1);
    bmesh_disk_edge_append(e2, e2->v2);
  }

  for (int i = 0; i < BLI_array_len(otheres); i++) {
    BMEdge *e = otheres[i];

    bmesh_disk_edge_append(e, e->v1);
    bmesh_disk_edge_append(e, e->v2);
  }
#if 0
  for (int i = 0; i < BLI_array_len(borderes); i++) {
    BMEdge *e = borderes[i];

    // first unlink local geometry

    BLI_array_clear(ls);
    BLI_array_clear(v1es);
    BLI_array_clear(v2es);

    // edge disk cycles
    for (int step = 0; step < 2; step++) {
      BMVert *v = step ? e->v2 : e->v1;
      BMEdge *e2 = v->e;

      // update ss->bm_log for undo
      if (!(e2->head.hflag & tag) && !(e2->head.hflag & tag2)) {
        BMLoop *l2 = e2->l;

        if (l2) {
          do {
            if (!(l2->f->head.hflag & tag) && !(l2->f->head.hflag & tag2)) {
              l2->f->head.hflag |= tag2;

              BKE_pbvh_bmesh_remove_face(ss->pbvh, l2->f, true);
              // BM_log_face_removed(ss->bm_log, l2->f);
            }

          } while ((l2 = l2->radial_next) != e2->l);
        }

        BM_log_edge_removed(ss->bm_log, e2);
        e2->head.hflag |= tag2;
      }

      do {
        if (step) {
          BLI_array_append(v1es, e2);
        }
        else {
          BLI_array_append(v2es, e2);
        }
      } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
    }

    // finish unlinking disk cycles
    e->v1->e = NULL;
    e->v2->e = NULL;

    // loop radial cycles
    BMLoop *l = e->l;
    do {
      BLI_array_append(ls, l);
    } while ((l = l->radial_next) != e->l);

    for (int j = 0; j < BLI_array_len(ls); j++) {
      l = ls[j];
      bmesh_radial_loop_remove(l->e, l);
    }

    // now splice geometry

    BMVert *v1_new = BM_vert_create(bm, e->v1->co, e->v1, BM_CREATE_NOP);
    BMVert *v2_new = BM_vert_create(bm, e->v2->co, e->v2, BM_CREATE_NOP);

    BM_log_vert_added(ss->bm_log, v1_new, ss->cd_vert_mask_offset);
    BM_log_vert_added(ss->bm_log, v2_new, ss->cd_vert_mask_offset);

    BMEdge *e_new = BM_edge_create(bm, v1_new, v2_new, e, BM_CREATE_NOP);
    BM_log_edge_added(ss->bm_log, e_new);

    BLI_array_append(borderes_new, e_new);

    for (int j = 0; j < BLI_array_len(ls); j++) {
      BMLoop *l = ls[j];

      if (l->f->head.hflag & tag) {
        l->e = e;
      }
      else {
        l->e = e_new;
        l->v = l->v == e->v1 ? v1_new : v2_new;
      }
    }

    bmesh_disk_edge_append(e, e->v1);
    bmesh_disk_edge_append(e, e->v2);

    for (int step = 0; step < 2; step++) {
      int len = step ? BLI_array_len(v2es) : BLI_array_len(v1es);
      BMEdge **ves = step ? v2es : v1es;
      BMVert *v_orig = step ? e->v2 : e->v1;
      BMVert *v_new = step ? e->v2 : e->v1;

      for (int j = 0; j < len; j++) {
        BMEdge *e2 = ves[j];

        if (e2 == e_new || e2 == e) {
          continue;
        }

        // part of kept geometry?
        if (e2->head.hflag & tag) {
          BMVert *v2 = e2->v1 == v_orig ? e2->v1 : e2->v2;
          bmesh_disk_edge_append(e2, v2);
        }
        else {
          BMVert *v2 = e2->v1 == v_orig ? v1_new : v2_new;
          bmesh_disk_edge_append(e2, v2);
        }
      }
    }

    // complete final relinking of loops
    for (int j = 0; j < BLI_array_len(ls); j++) {
      BMLoop *l = ls[j];

      bmesh_radial_loop_append(l->e, l);
    }
  }
#endif
  if (!ss->bm) {
    BM_mesh_bm_to_me(NULL,
                     NULL,
                     bm,
                     ob->data,
                     (&(struct BMeshToMeshParams){
                         .calc_object_remap = false,
                     }));
  }

  if (!ss->bm) {
    sculpt_faceset_bm_end(ss, bm);
  }

  *r_verts = MEM_malloc_arrayN(BLI_array_len(retvs), sizeof(int), "face set extrude verts");
  *r_totvert = BLI_array_len(retvs);

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  for (int i = 0; i < BLI_array_len(retvs); i++) {
    BMVert *v = retvs[i];
    (*r_verts)[i] = v->head.index;
  }

  BLI_array_free(vs);
  BLI_array_free(es);
  BLI_array_free(fs);
  BLI_array_free(retvs);
}

ATTR_NO_OPT static void sculpt_face_set_extrude_id(
    Object *ob, SculptSession *ss, const int active_face_set_id, int **r_verts, int *r_totvert)
{

  Mesh *mesh = ob->data;
  const int next_face_set_id = ED_sculpt_face_sets_find_next_available_id(mesh);

  BMesh *bm = sculpt_faceset_bm_begin(ss, mesh);
  if (ss->bm) {
    BKE_pbvh_bmesh_set_toolflags(ss->pbvh, true);
  }

  BM_mesh_elem_table_init(bm, BM_FACE);
  BM_mesh_elem_table_ensure(bm, BM_FACE);

  sculpt_bm_mesh_elem_hflag_disable_all(
      bm, BM_ALL_NOLOOP, BM_ELEM_SELECT | BM_ELEM_TAG_ALT | BM_ELEM_TAG);

  BMIter iter;
  BMFace *f;

  if (ss->bm && ss->pbvh) {
    BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);
  }

  BM_mesh_select_mode_set(bm, SCE_SELECT_FACE);

  BMVert **retvs = NULL;
  BMVert **vs = NULL;
  BMEdge **es = NULL;
  BLI_array_declare(vs);
  BLI_array_declare(es);
  BLI_array_declare(retvs);

  const int cd_faceset_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);

  const int tag1 = BM_ELEM_SELECT;
  const int tag2 = BM_ELEM_TAG_ALT;
  const int tag3 = BM_ELEM_TAG;
  float no[3] = {0.0f, 0.0f, 0.0f};

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int face_index = BM_elem_index_get(f);
    const int face_set_id = BM_ELEM_CD_GET_INT(f, cd_faceset_offset);

    if (face_set_id == active_face_set_id) {
      BM_elem_select_set(bm, (BMElem *)f, true);
      if (dot_v3v3(f->no, f->no) > 0.0f) {
        add_v3_v3(no, f->no);
      }

      if (ss->bm) {
        BMLoop *l = f->l_first;

        do {
          if (!(BM_elem_flag_test(l->e, tag2))) {
            BM_elem_flag_enable(l->e, tag2);
            BLI_array_append(es, l->e);
          }

          if (!(BM_elem_flag_test(l->v, tag2))) {
            BM_elem_flag_enable(l->v, tag2);
            BLI_array_append(vs, l->v);
          }

        } while ((l = l->next) != f->l_first);

        BKE_pbvh_bmesh_remove_face(ss->pbvh, f, true);
      }
    }
    else {
      BM_elem_select_set(bm, (BMElem *)f, false);
    }

    BM_elem_flag_set(f, BM_ELEM_TAG, face_set_id == active_face_set_id);
  }

  *r_verts = NULL;
  *r_totvert = 0;

  if (ss->bm) {
    for (int i = 0; i < BLI_array_len(es); i++) {
      BMEdge *e = es[i];
      BMLoop *l = e->l;

      bool remove = true;
      do {
        if (!(BM_elem_flag_test(l->f, tag1))) {
          // remove = false;
          if (!BM_elem_flag_test(l->f, tag2)) {
            BKE_pbvh_bmesh_remove_face(ss->pbvh, l->f, true);
            BM_elem_flag_enable(l->f, tag2);
          }
          break;
        }
      } while ((l = l->radial_next) != e->l);

      if (remove) {
        if (!BM_elem_flag_test(e->v1, tag3)) {
          BM_log_vert_removed(ss->bm_log, e->v1, ss->cd_vert_mask_offset);
          // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, e->v1, true);
          BM_elem_flag_enable(e->v1, tag3);
        }

        if (!BM_elem_flag_test(e->v2, tag3)) {
          BM_log_vert_removed(ss->bm_log, e->v2, ss->cd_vert_mask_offset);
          // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, e->v2, true);
          BM_elem_flag_enable(e->v2, tag3);
        }

        BKE_pbvh_bmesh_remove_edge(ss->pbvh, e, true);
        e->head.hflag |= tag1;
      }
    }

    for (int i = 0; i < BLI_array_len(vs); i++) {
      BMVert *v = vs[i];
      BMEdge *e = v->e;
      bool remove = true;

      if (BM_elem_flag_test(v, tag3)) {
        continue;
      }

      BM_elem_flag_enable(v, tag3);

      do {
        if (!BM_elem_flag_test(e, tag1)) {
          // remove = false;
          break;
        }
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      if (remove) {
        // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, v, true);
        BM_log_vert_removed(ss->bm_log, v, ss->cd_vert_mask_offset);
      }
    }
  }

  BM_mesh_select_flush(bm);
  BM_mesh_select_mode_flush(bm);

  BMOperator extop;
  BMO_op_init(bm, &extop, BMO_FLAG_DEFAULTS, "extrude_face_region");
  BMO_slot_bool_set(extop.slots_in, "use_normal_flip", true);
  BMO_slot_bool_set(extop.slots_in, "use_dissolve_ortho_edges", true);
  BMO_slot_bool_set(extop.slots_in, "use_select_history", true);
  char htype = BM_ALL_NOLOOP;
  htype &= ~(BM_VERT | BM_EDGE);
  if (htype & BM_FACE) {
    htype |= BM_EDGE;
  }

  BMO_slot_buffer_from_enabled_hflag(bm, &extop, extop.slots_in, "geom", htype, BM_ELEM_SELECT);

  BMO_op_exec(bm, &extop);
  sculpt_bm_mesh_elem_hflag_disable_all(
      bm, BM_ALL_NOLOOP, BM_ELEM_SELECT | BM_ELEM_TAG_ALT | BM_ELEM_TAG);

  BMOIter siter;
  BMElem *ele;

  for (int step = 0; step < (ss->bm ? 2 : 1); step++) {
    BMO_ITER (ele, &siter, extop.slots_out, step ? "side_geom.out" : "geom.out", BM_ALL_NOLOOP) {
      /*
      if (ele->head.htype == BM_VERT && BM_log_has_vert(ss->bm_log, (BMVert *)ele)) {
        BM_elem_flag_enable(ele, tag1);
      }
      if (ele->head.htype == BM_EDGE && BM_log_has_edge(ss->bm_log, (BMEdge *)ele)) {
        BM_elem_flag_enable(ele, tag1);
      }
      if (ele->head.htype == BM_FACE && BM_log_has_face(ss->bm_log, (BMFace *)ele)) {
        BM_elem_flag_enable(ele, tag1);
      }*/

      if (ele->head.htype == BM_VERT) {
        BM_ELEM_CD_SET_INT(ele, ss->cd_vert_node_offset, DYNTOPO_NODE_NONE);
      }
      else if (ele->head.htype == BM_FACE) {
        BM_ELEM_CD_SET_INT(ele, ss->cd_face_node_offset, DYNTOPO_NODE_NONE);
      }
    }
  }

  BMFace **flipfs = NULL;
  BLI_array_declare(flipfs);

  for (int step = 0; step < (ss->bm ? 2 : 1); step++) {
    BMO_ITER (ele, &siter, extop.slots_out, step ? "side_geom.out" : "geom.out", BM_ALL_NOLOOP) {
      if (step == 0 && ele->head.htype != BM_VERT) {
        BM_elem_flag_set(ele, BM_ELEM_TAG, true);
      }

      if (!ss->bm || BM_elem_flag_test(ele, tag1)) {
        continue;
      }

      BM_elem_flag_enable(ele, tag1);

      switch (ele->head.htype) {
        case BM_VERT:
          BM_log_vert_added(ss->bm_log, (BMVert *)ele, ss->cd_vert_mask_offset);

          if (step == 0) {
            BLI_array_append(retvs, (BMVert *)ele);
          }

          break;
        case BM_EDGE: {
          BMEdge *e = (BMEdge *)ele;
          BM_log_edge_added(ss->bm_log, e);

          if (!BM_elem_flag_test(e->v1, tag1)) {
            BM_elem_flag_enable(e->v1, tag1);
            BM_log_vert_added(ss->bm_log, e->v1, ss->cd_vert_mask_offset);
          }

          if (!BM_elem_flag_test(e->v2, tag1)) {
            BM_elem_flag_enable(e->v2, tag1);
            BM_log_vert_added(ss->bm_log, e->v2, ss->cd_vert_mask_offset);
          }

          if (1 || step == 1) {
            BMLoop *l = e->l;

            if (l) {
              do {
                if (!BM_elem_flag_test(l->f, tag1)) {
                  // BLI_array_append(flipfs, l->f);

                  BKE_pbvh_bmesh_add_face(ss->pbvh, l->f, false, false);
                  BM_log_face_added(ss->bm_log, l->f);

                  BM_elem_flag_enable(l->f, tag1);
                }
              } while ((l = l->radial_next) != e->l);
            }
          }

          break;
        }
        case BM_FACE: {
          BMFace *f = (BMFace *)ele;

          if (dot_v3v3(f->no, f->no) > 0.0f) {
            add_v3_v3(no, f->no);
          }

          BMLoop *l = f->l_first;
          do {
            if (!(l->v->head.hflag & tag1)) {
              l->v->head.hflag |= tag1;
              BM_log_vert_added(ss->bm_log, l->v, ss->cd_vert_mask_offset);
            }

            if (!(l->e->head.hflag & tag1)) {
              l->e->head.hflag |= tag1;
              BM_log_edge_added(ss->bm_log, l->e);
            }
          } while ((l = l->next) != f->l_first);

          BKE_pbvh_bmesh_add_face(ss->pbvh, f, false, false);
          BM_log_face_added(ss->bm_log, f);
          break;
        }
        default:
          break;
      }
    }
  }

#if 0 
  for (int i=0; i<BLI_array_len(flipfs); i++) {
    BMFace *f = flipfs[i];

    //BM_face_normal_flip(bm, f);

    if (ss->bm) {
      BKE_pbvh_bmesh_add_face(ss->pbvh, f, false, false);
      BM_log_face_added(ss->bm_log, f);
    }
  }
#endif

  BMO_op_finish(bm, &extop);

  for (int i = 0; i < BLI_array_len(retvs); i++) {
    BM_elem_flag_enable(retvs[i], BM_ELEM_TAG);
  }

  SmallHash keymap;
  int keyi = 0;
  BLI_smallhash_init(&keymap);

  /* Set the new Face Set ID for the extrusion. */
  const int cd_face_sets_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int face_set_id = BM_ELEM_CD_GET_INT(f, cd_face_sets_offset);
    if (face_set_id == active_face_set_id) {
      continue;
    }

    normalize_v3(no);

    const int cd_dyn_vert = CustomData_get_offset(&bm->vdata, CD_DYNTOPO_VERT);

    BMVert *v;
    BMIter face_iter;
    BM_ITER_ELEM (v, &face_iter, f, BM_VERTS_OF_FACE) {
      BMLoop *l = f->l_first;

      if (cd_dyn_vert >= 0) {
        do {
          MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(l->v, cd_dyn_vert);
          mv->flag |= DYNVERT_NEED_BOUNDARY|DYNVERT_NEED_DISK_SORT|DYNVERT_NEED_TRIANGULATE|DYNVERT_NEED_VALENCE;
        } while ((l = l->next) != f->l_first);
      }

      if (dot_v3v3(f->no, f->no) == 0.0f) {
        float co1[3], co2[3], co3[3];

        copy_v3_v3(co1, l->v->co);
        copy_v3_v3(co2, l->next->v->co);
        copy_v3_v3(co3, l->next->next->v->co);

        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          madd_v3_v3fl(co1, no, 0.01);
        }
        else if (BM_elem_flag_test(l->next->next->v, BM_ELEM_TAG)) {
          madd_v3_v3fl(co3, no, 0.01);
        }

        if (1 || f->len > 3) {
          normal_tri_v3(f->no, co1, co2, co3);
        }
        else {
          normal_quad_v3(
              f->no, l->v->co, l->next->v->co, l->next->next->v->co, l->next->next->next->v->co);
        }
        printf("eek!");
      }

      int dimen = 1;
      int x = (int)((f->no[0] + 1.0f) * (float)dimen);
      int y = (int)((f->no[1] + 1.0f) * (float)dimen);
      int z = (int)((f->no[2] + 1.0f) * (float)dimen);

      int key = x * dimen * dimen + y * dimen + z;
      void **val = NULL;
      if (!BLI_smallhash_ensure_p(&keymap, (uintptr_t)key, &val)) {
        *val = POINTER_FROM_INT(keyi);
        keyi++;
      }
      int fset = next_face_set_id;  // + POINTER_AS_INT(*val);

      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        BM_ELEM_CD_SET_INT(f, cd_face_sets_offset, fset);
        break;
      }
    }
  }

  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

  *r_verts = MEM_malloc_arrayN(BLI_array_len(retvs), sizeof(int), "face set extrude verts");
  *r_totvert = BLI_array_len(retvs);

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  for (int i = 0; i < BLI_array_len(retvs); i++) {
    BMVert *v = retvs[i];
    (*r_verts)[i] = v->head.index;
  }

  BLI_array_free(vs);
  BLI_array_free(es);

  if (!ss->bm) {
    BM_mesh_bm_to_me(NULL,
                     NULL,
                     bm,
                     ob->data,
                     (&(struct BMeshToMeshParams){
                         .calc_object_remap = false,
                     }));
  }

  sculpt_faceset_bm_end(ss, bm);
  if (ss->bm) {
    BKE_pbvh_bmesh_set_toolflags(ss->pbvh, false);
  }
}

static int sculpt_face_set_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const int mode = RNA_enum_get(op->ptr, "mode");
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  const int totvert = SCULPT_vertex_count_get(ss);

  if (mode != SCULPT_FACE_SET_EDIT_EXTRUDE) {
    return OPERATOR_FINISHED;
  }

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    FaceSetExtrudeCD *fsecd = op->customdata;
    MEM_SAFE_FREE(fsecd->orig_co);
    MEM_SAFE_FREE(fsecd->orig_no);
    MEM_SAFE_FREE(fsecd->verts);
    MEM_SAFE_FREE(op->customdata);

    if (ss->bm) {
      SCULPT_undo_push_end();
    }
    else {
      ED_sculpt_undo_geometry_end(ob);
    }

    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  FaceSetExtrudeCD *fsecd = op->customdata;
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  float depth_world_space[3];
  float new_pos[3];

  mul_v3_m4v3(depth_world_space, ob->obmat, fsecd->cursor_location);

  float fmval[2] = {event->mval[0], fsecd->init_mval[1]};

  ED_view3d_win_to_3d(vc.v3d, vc.region, depth_world_space, fmval, new_pos);
  float extrude_disp = len_v3v3(depth_world_space, new_pos);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  if (event->mval[0] <= fsecd->init_mval[0]) {
    // extrude_disp *= -1.0f;
  }

  if (!fsecd->orig_co) {
    fsecd->orig_co = MEM_calloc_arrayN(fsecd->totvert, sizeof(float) * 3, "origco");
    fsecd->orig_no = MEM_calloc_arrayN(fsecd->totvert, sizeof(float) * 3, "origno");

    for (int i = 0; i < fsecd->totvert; i++) {
      int idx = fsecd->verts[i];
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, idx);

      copy_v3_v3(fsecd->orig_co[i], SCULPT_vertex_co_get(ss, vertex));
      SCULPT_vertex_normal_get(ss, vertex, fsecd->orig_no[i]);
    }
  }

  if (!ss->bm) {
    MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
    for (int i = 0; i < fsecd->totvert; i++) {
      int idx = fsecd->verts[i];
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, idx);

      madd_v3_v3v3fl(mvert[idx].co, fsecd->orig_co[i], fsecd->orig_no[i], extrude_disp);
      mvert[idx].flag |= ME_VERT_PBVH_UPDATE;
    }

    PBVHNode **nodes;
    int totnode;
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
    for (int i = 0; i < totnode; i++) {
      BKE_pbvh_node_mark_update(nodes[i]);
    }
    MEM_SAFE_FREE(nodes);
  }
  else {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);

    for (int i = 0; i < fsecd->totvert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, fsecd->verts[i]);

      BMVert *v = (BMVert *)vertex.i;

      int ni = BM_ELEM_CD_GET_INT(v, ss->cd_vert_node_offset);
      if (ni != DYNTOPO_NODE_NONE) {
        BKE_pbvh_node_mark_update(BKE_pbvh_node_from_index(ss->pbvh, ni));
      }

      madd_v3_v3v3fl(v->co, fsecd->orig_co[i], fsecd->orig_no[i], extrude_disp);
    }
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_face_set_extrude(bContext *C,
                                    wmOperator *op,
                                    const wmEvent *event,
                                    Object *ob,
                                    const int active_face_set,
                                    const float cursor_location[3])
{
  FaceSetExtrudeCD *fsecd = MEM_callocN(sizeof(FaceSetExtrudeCD), "face set extrude cd");

  fsecd->active_face_set = active_face_set;
  copy_v3_v3(fsecd->cursor_location, cursor_location);
  float fmval[2] = {event->mval[0], event->mval[1]};
  copy_v2_v2(fsecd->init_mval, fmval);
  op->customdata = fsecd;

  if (!ob->sculpt->bm) {
    ED_sculpt_undo_geometry_begin(ob, "Face Set Extrude");
  }
  else {
    SCULPT_undo_push_begin(ob, "Face Set Extrude");
    SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_COORDS);
  }

  sculpt_face_set_extrude_id(ob, ob->sculpt, active_face_set, &fsecd->verts, &fsecd->totvert);

  if (!ob->sculpt->bm) {
    BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static int sculpt_face_set_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const int mode = RNA_enum_get(op->ptr, "mode");
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  if (!sculpt_face_set_edit_is_operation_valid(ss, mode, modify_hidden)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  /* Update the current active Face Set and Vertex as the operator can be used directly from the
   * tool without brush cursor. */
  SculptCursorGeometryInfo sgi;
  const float mouse[2] = {event->mval[0], event->mval[1]};

  if (!SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false)) {
    /* The cursor is not over the mesh. Cancel to avoid editing the last updated Face Set ID. */
    return OPERATOR_CANCELLED;
  }

  const int active_face_set = SCULPT_active_face_set_get(ss);

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_EXTRUDE:
      sculpt_face_set_extrude(C, op, event, ob, active_face_set, sgi.location);

      SCULPT_tag_update_overlays(C);
      WM_event_add_modal_handler(C, op);
      return OPERATOR_RUNNING_MODAL;
    case SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY:
      sculpt_face_set_edit_modify_geometry(C, ob, active_face_set, mode, modify_hidden);
      break;
    case SCULPT_FACE_SET_EDIT_GROW:
    case SCULPT_FACE_SET_EDIT_SHRINK:
    case SCULPT_FACE_SET_EDIT_FILL_COMPONENT:
      sculpt_face_set_edit_modify_face_sets(ob, active_face_set, mode, modify_hidden);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_POSITIONS:
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
    case SCULPT_FACE_SET_EDIT_FAIR_CURVATURE:
    case SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY:
      sculpt_face_set_edit_modify_coordinates(C, ob, active_face_set, mode);
      break;
  }

  SCULPT_tag_update_overlays(C);

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
  ot->modal = sculpt_face_set_edit_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_edit_types, SCULPT_FACE_SET_EDIT_GROW, "Mode", "");
  ot->prop = RNA_def_boolean(ot->srna,
                             "modify_hidden",
                             true,
                             "Modify Hidden",
                             "Apply the edit operation to hidden Face Sets");
}
