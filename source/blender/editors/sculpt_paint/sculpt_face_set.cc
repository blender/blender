/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_smallhash.h"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_types.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_sculpt.h"

#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"
#include "bmesh_log.h"

#include <math.h>
#include <stdlib.h>

using blender::IndexRange;
using blender::Vector;

static int sculpt_face_material_get(SculptSession *ss, PBVHFaceRef face)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;
      return f->mat_nr;
    }
    case PBVH_GRIDS:
    case PBVH_FACES:
      return ss->material_index[face.i];
  }

  return -1;
}

int SCULPT_face_set_get(SculptSession *ss, PBVHFaceRef face)
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

void SCULPT_face_check_origdata(SculptSession *ss, PBVHFaceRef face)
{
  if (!ss->attrs.orig_fsets) {
    return;
  }

  short *s = BKE_sculpt_face_attr_get<short *>(face, ss->attrs.orig_fsets);

  // pack ss->stroke_id in higher 16 bits
  if (s[1] != ss->stroke_id) {
    s[0] = SCULPT_face_set_get(ss, face);
    s[1] = ss->stroke_id;
  }
}

int SCULPT_face_set_original_get(SculptSession *ss, PBVHFaceRef face)
{
  if (!ss->attrs.orig_fsets) {
    return SCULPT_face_set_get(ss, face);
  }

  short *s = BKE_sculpt_face_attr_get<short *>(face, ss->attrs.orig_fsets);

  if (s[1] != ss->stroke_id) {
    s[0] = SCULPT_face_set_get(ss, face);
    s[1] = ss->stroke_id;
  }

  return s[0];
}

void SCULPT_face_ensure_original(SculptSession *ss, Object *ob)
{
  SculptAttributeParams params = {0};

  ss->attrs.orig_fsets = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_FACE, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(orig_fsets), &params);
}

bool SCULPT_face_select_get(SculptSession *ss, PBVHFaceRef face)
{
  if (ss->bm) {
    BMFace *f = (BMFace *)face.i;

    return f->head.hflag & BM_ELEM_SELECT;
  }
  else {
    return ss->select_poly ? ss->select_poly[face.i] : false;
  }
}

using blender::float3;
using blender::Vector;

/* Utils. */

int ED_sculpt_face_sets_find_next_available_id(struct Mesh *mesh)
{
  const int *face_sets = static_cast<const int *>(
      CustomData_get_layer_named(&mesh->pdata, CD_PROP_INT32, ".sculpt_face_set"));
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
  int *face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
      &mesh->pdata, CD_PROP_INT32, ".sculpt_face_set", mesh->totpoly));
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

static BMesh *sculpt_faceset_bm_begin(Object *ob, SculptSession *ss, Mesh *mesh)
{
  if (ss->bm) {
    return ss->bm;
  }

  BMeshCreateParams params = {0};
  params.use_toolflags = true;

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
  BMesh *bm = BM_mesh_create(&allocsize, &params);

  BMeshFromMeshParams cparams = {0};

  cparams.calc_face_normal = true;
  cparams.active_shapekey = ob->shapenr;
  cparams.use_shapekey = true;
  cparams.create_shapekey_layers = true;

  BM_mesh_bm_from_me(bm, mesh, &cparams);
  return bm;
}

static void sculpt_faceset_bm_end(SculptSession *ss, BMesh *bm)
{
  if (bm != ss->bm) {
    BM_mesh_free(bm);
  }
}

/* Draw Face Sets Brush. */

static int new_fset_apply_curve(SculptSession *ss,
                                SculptFaceSetDrawData *data,
                                int new_fset,
                                float poly_center[3],
                                float no[3],
                                SculptBrushTest *test,
                                CurveMapping *curve,
                                int count)
{
  float fade2;
  float tmp[3];
  float n[3];

  sub_v3_v3v3(tmp, poly_center, test->location);

  cross_v3_v3v3(n, no, data->stroke_direction);
  normalize_v3(n);

  // find t along brush line
  float t = dot_v3v3(data->stroke_direction, tmp) / ss->cache->radius;
  CLAMP(t, -1.0f, 1.0f);
  t = t * 0.5 + 0.5;

  // find start and end points;
  float start[3], end[3];
  copy_v3_v3(start, ss->cache->last_location);
  copy_v3_v3(end, ss->cache->location);

  madd_v3_v3fl(start, data->prev_stroke_direction, 0.5f * ss->cache->radius);
  madd_v3_v3fl(end, data->next_stroke_direction, 0.5f * ss->cache->radius);

  float co[3];

  // interpolate direction and pos across stroke line
  float dir[3];
  if (t < 0.5) {
    interp_v3_v3v3(co, start, test->location, t * 2.0f);
    interp_v3_v3v3(dir, data->prev_stroke_direction, data->stroke_direction, t * 2.0f);
  }
  else {
    interp_v3_v3v3(co, test->location, end, (t - 0.5f) * 2.0f);
    interp_v3_v3v3(dir, data->stroke_direction, data->next_stroke_direction, (t - 0.5f) * 2.0f);
  }

  sub_v3_v3v3(tmp, poly_center, co);
  normalize_v3(dir);

  // get final distance from stroke curve
  cross_v3_v3v3(n, no, dir);
  normalize_v3(n);

  fade2 = fabsf(dot_v3v3(n, tmp)) / ss->cache->radius;
  CLAMP(fade2, 0.0f, 1.0f);

  if (curve) {
    fade2 = BKE_curvemapping_evaluateF(curve, 0, fade2);
  }

  new_fset += (int)((1.0f - fade2) * (float)count);

  return new_fset;
}

ATTR_NO_OPT void do_draw_face_sets_brush_task_cb_ex(void *__restrict userdata,
                                                    const int n,
                                                    const TaskParallelTLS *__restrict tls)
{
  SculptFaceSetDrawData *data = (SculptFaceSetDrawData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = data->bstrength;

  const bool use_fset_strength = data->use_fset_strength;
  const bool use_fset_curve = data->use_fset_curve;
  const int count = data->count;
  const int active_fset = data->faceset;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init(ss, &test);

  const int thread_id = BLI_task_parallel_thread_id(tls);

  float(*vert_positions)[3] = SCULPT_mesh_deformed_positions_get(ss);
  const float test_limit = 0.05f;
  int cd_mask = -1;

  if (ss->bm) {
    cd_mask = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);
  }

  /*check if we need to sample the current face set*/

  bool set_active_faceset = ss->cache->automasking &&
                            (ss->cache->automasking->settings.flags & BRUSH_AUTOMASKING_FACE_SETS);
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

  bool modified = false;

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
      MeshElemMap *vert_map = &ss->pmap->pmap[vd.index];
      for (int j = 0; j < ss->pmap->pmap[vd.index].count; j++) {
        const MPoly *p = &ss->polys[vert_map->indices[j]];

        float poly_center[3];
        BKE_mesh_calc_poly_center(
            &ss->loops[p->loopstart], p->totloop, vert_positions, ss->totvert, poly_center);

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
                                                                    thread_id,
                                                                    &automask_data);

        int new_fset = active_fset;

        if (use_fset_curve) {
          float no[3];
          SCULPT_vertex_normal_get(ss, vd.vertex, no);

          new_fset = new_fset_apply_curve(
              ss, data, new_fset, poly_center, no, &test, data->curve, count);
        }

        if (fade > test_limit && ss->face_sets[vert_map->indices[j]] > 0) {
          bool ok = true;

          int fset = abs(ss->face_sets[vert_map->indices[j]]);

          /* Sample faces that are within
           * 8 pixels of the center of the brush.
           */
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

          const MLoop *ml = &ss->loops[p->loopstart];

          for (int i = 0; i < p->totloop; i++, ml++) {
            float *v = vert_positions[ml->v];
            float fno[3];

            *SCULPT_vertex_attr_get<int *>(
                BKE_pbvh_make_vref(ml->v),
                ss->attrs.boundary_flags) |= SCULPT_BOUNDARY_NEEDS_UPDATE;

            copy_v3_v3(fno, ss->vert_normals[ml->v]);
            float mask = ss->vmask ? ss->vmask[ml->v] : 0.0f;

            const float fade2 = bstrength *
                                SCULPT_brush_strength_factor(ss,
                                                             brush,
                                                             v,
                                                             sqrtf(test.dist),
                                                             ss->vert_normals[ml->v],
                                                             fno,
                                                             mask,
                                                             BKE_pbvh_make_vref((intptr_t)ml->v),
                                                             thread_id,
                                                             &automask_data);

            if (fade2 < test_limit) {
              ok = false;
              break;
            }
          }

          if (ok) {
            ss->face_sets[vert_map->indices[j]] = new_fset;
            modified = true;
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
                                                                      thread_id,
                                                                      &automask_data);

          int new_fset = active_fset;

          if (use_fset_curve) {
            float no[3];
            SCULPT_vertex_normal_get(ss, vd.vertex, no);

            new_fset = new_fset_apply_curve(
                ss, data, new_fset, poly_center, no, &test, data->curve, count);
          }

          int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

          if ((!use_fset_strength || fade > test_limit) && fset > 0) {
            BMLoop *l = f->l_first;

            bool ok = true;

            /* Sample faces that are within
             * 8 pixels of the center of the brush.
             */
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
              float mask = cd_mask >= 0 ? BM_ELEM_CD_GET_FLOAT(l->v, cd_mask) : 0.0f;

              const float fade2 = bstrength *
                                  SCULPT_brush_strength_factor(ss,
                                                               brush,
                                                               l->v->co,
                                                               sqrtf(test.dist),
                                                               l->v->no,
                                                               l->f->no,
                                                               mask,
                                                               BKE_pbvh_make_vref((intptr_t)l->v),
                                                               thread_id,
                                                               &automask_data);

              if (fade2 < test_limit) {
                ok = false;
                break;
              }

              *SCULPT_vertex_attr_get<int *>(
                  BKE_pbvh_make_vref((intptr_t)l->v),
                  ss->attrs.boundary_flags) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
            } while ((l = l->next) != f->l_first);

            if (ok) {
              BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, new_fset);
              modified = true;
            }
          }
        }
      }
    }
    else if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
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
                                                                  thread_id,
                                                                  &automask_data);
      int new_fset = active_fset;

      if (use_fset_curve) {
        float no[3];
        SCULPT_vertex_normal_get(ss, vd.vertex, no);

        new_fset = new_fset_apply_curve(
            ss, data, new_fset, ss->cache->location, no, &test, data->curve, count);
      }

      if (!use_fset_strength || fade > test_limit) {
        SCULPT_vertex_face_set_set(ss, vd.vertex, new_fset);
        modified = true;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_update_index_buffer(ss->pbvh, data->nodes[n]);
  }

  // restore automasking flag
  if (set_active_faceset) {
    ss->cache->automasking->settings.flags |= automasking_fset_flag;
  }
}

static void do_relax_face_sets_brush_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptFaceSetDrawData *data = (SculptFaceSetDrawData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init(ss, &test);

  const bool relax_face_sets = !(ss->cache->iteration_count % 3 == 0);
  /* This operations needs a strength tweak as the relax deformation is too weak by default. */
  if (relax_face_sets && data->iteration < 2) {
    bstrength *= 1.5f;
  }

  const int thread_id = BLI_task_parallel_thread_id(tls);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  bool do_reproject = SCULPT_need_reproject(ss);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    if (relax_face_sets == SCULPT_vertex_has_unique_face_set(ss, vd.vertex)) {
      continue;
    }

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          sqrtf(test.dist),
                                                          vd.no,
                                                          vd.fno,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.vertex,
                                                          thread_id,
                                                          &automask_data);

    CLAMP(fade, 0.0f, 1.0f);

    float oldco[3], oldno[3];

    copy_v3_v3(oldco, vd.co);
    SCULPT_vertex_normal_get(ss, vd.vertex, oldno);

    SCULPT_relax_vertex(ss,
                        &vd,
                        fade * bstrength,
                        (eSculptBoundary)(SCULPT_BOUNDARY_DEFAULT | SCULPT_BOUNDARY_FACE_SET),
                        vd.co);
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
    if (do_reproject) {
      SCULPT_reproject_cdata(ss, vd.vertex, oldco, oldno);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

ATTR_NO_OPT void SCULPT_do_draw_face_sets_brush(Sculpt *sd,
                                                Object *ob,
                                                PBVHNode **nodes,
                                                int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache->brush ? ss->cache->brush : BKE_paint_brush(&sd->paint);

  BKE_sculpt_face_sets_ensure(ob);
  if (ss->pbvh) {
    Mesh *mesh = BKE_mesh_from_object(ob);
    BKE_pbvh_face_sets_color_set(
        ss->pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);
  }

  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptFaceSetDrawData data;

  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.faceset = abs(ss->cache->paint_face_set);
  data.use_fset_curve = false;
  data.use_fset_strength = true;
  data.bstrength = ss->cache->bstrength;
  data.count = 1;

  bool threaded = true;

  /*for ctrl invert mode we have to set the automasking initial_face_set
    to the first non-current faceset that is found*/
  int automasking_flags = brush->automasking_flags | (sd ? sd->automasking_flags : 0);

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    if (ss->cache->invert && ss->cache->automasking &&
        (automasking_flags & BRUSH_AUTOMASKING_FACE_SETS)) {
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
      data.iteration = i;
      BLI_task_parallel_range(0, totnode, &data, do_relax_face_sets_brush_task_cb_ex, &settings);
    }
  }
  else {
    BLI_task_parallel_range(0, totnode, &data, do_draw_face_sets_brush_task_cb_ex, &settings);
  }
}

/* Face Sets Operators */

enum eSculptFaceGroupsCreateModes {
  SCULPT_FACE_SET_MASKED = 0,
  SCULPT_FACE_SET_VISIBLE = 1,
  SCULPT_FACE_SET_ALL = 2,
  SCULPT_FACE_SET_SELECTION = 3,
};

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
    {0, nullptr, 0, nullptr, nullptr},
};

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
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
  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, op);
  for (const int i : blender::IndexRange(totnode)) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_FACE_SETS);
  }

  const int next_face_set = SCULPT_face_set_next_available_get(ss);

  if (mode == SCULPT_FACE_SET_MASKED) {
    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

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
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (!SCULPT_vertex_visible_get(ss, vertex)) {
        all_visible = false;
        break;
      }
    }

    if (all_visible) {
      Mesh *mesh = (Mesh *)ob->data;
      mesh->face_sets_color_default = next_face_set;
      BKE_pbvh_face_sets_color_set(
          ss->pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);
    }

    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (SCULPT_vertex_visible_get(ss, vertex)) {
        SCULPT_vertex_face_set_set(ss, vertex, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_ALL) {
    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_face_set_set(ss, vertex, next_face_set);
    }
  }

  if (mode == SCULPT_FACE_SET_SELECTION) {
    const int totface = ss->totfaces;

    for (int i = 0; i < totface; i++) {
      PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);

      // XXX check hidden?
      bool ok = true;

      if (ss->attrs.hide_poly) {
        ok = *BKE_sculpt_face_attr_get<bool *>(fref, ss->attrs.hide_poly);
      }

      ok = ok && SCULPT_face_select_get(ss, fref);

      if (ok) {
        SCULPT_face_set_set(ss, fref, next_face_set);
      }
    }
  }

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);

  SCULPT_undo_push_end(ob);

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

enum eSculptFaceSetsInitMode {
  SCULPT_FACE_SETS_FROM_LOOSE_PARTS = 0,
  SCULPT_FACE_SETS_FROM_MATERIALS = 1,
  SCULPT_FACE_SETS_FROM_NORMALS = 2,
  SCULPT_FACE_SETS_FROM_UV_SEAMS = 3,
  SCULPT_FACE_SETS_FROM_CREASES = 4,
  SCULPT_FACE_SETS_FROM_SHARP_EDGES = 5,
  SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT = 6,
  SCULPT_FACE_SETS_FROM_FACE_MAPS = 7,
  SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES = 8,
};

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
    {0, nullptr, 0, nullptr, nullptr},
};

typedef bool (*face_sets_flood_fill_test)(
    BMesh *bm, BMFace *from_f, BMEdge *from_e, BMFace *to_f, const float threshold);

static bool sculpt_face_sets_init_loose_parts_test(BMesh * /*bm*/,
                                                   BMFace * /*from_f*/,
                                                   BMEdge * /*from_e*/,
                                                   BMFace * /*UNUSED(to_f)*/,
                                                   const float /*UNUSED(threshold)*/)
{
  return true;
}

static bool sculpt_face_sets_init_normals_test(BMesh * /*bm*/,
                                               BMFace *from_f,
                                               BMEdge * /*UNUSED(from_e)*/,
                                               BMFace *to_f,
                                               const float threshold)
{
  return fabsf(dot_v3v3(from_f->no, to_f->no)) > threshold;
}

static bool sculpt_face_sets_init_uv_seams_test(BMesh * /*UNUSED(bm)*/,
                                                BMFace * /*UNUSED(from_f)*/,
                                                BMEdge *from_e,
                                                BMFace * /*UNUSED(to_f)*/,
                                                const float /*UNUSED(threshold)*/)
{
  return !BM_elem_flag_test(from_e, BM_ELEM_SEAM);
}

static bool sculpt_face_sets_init_crease_test(BMesh *bm,
                                              BMFace * /*UNUSED(from_f)*/,
                                              BMEdge *from_e,
                                              BMFace * /*UNUSED(to_f)*/,
                                              const float threshold)
{
  return BM_elem_float_data_get(&bm->edata, from_e, CD_CREASE) < threshold;
}

static bool sculpt_face_sets_init_bevel_weight_test(BMesh *bm,
                                                    BMFace * /*UNUSED(from_f)*/,
                                                    BMEdge *from_e,
                                                    BMFace * /*UNUSED(to_f)*/,
                                                    const float threshold)
{
  return BM_elem_float_data_get(&bm->edata, from_e, CD_BWEIGHT) < threshold;
}

static bool sculpt_face_sets_init_sharp_edges_test(BMesh * /*UNUSED(bm)*/,
                                                   BMFace * /*UNUSED(from_f)*/,
                                                   BMEdge *from_e,
                                                   BMFace * /*UNUSED(to_f)*/,
                                                   const float /*UNUSED(threshold)*/)
{
  return BM_elem_flag_test(from_e, BM_ELEM_SMOOTH);
}

static bool sculpt_face_sets_init_face_set_boundary_test(BMesh *bm,
                                                         BMFace *from_f,
                                                         BMEdge * /*UNUSED(from_e)*/,
                                                         BMFace *to_f,
                                                         const float /*UNUSED(threshold)*/)
{
  const int cd_face_sets_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  return BM_ELEM_CD_GET_INT(from_f, cd_face_sets_offset) ==
         BM_ELEM_CD_GET_INT(to_f, cd_face_sets_offset);
}

static void sculpt_face_sets_init_flood_fill(Object *ob,
                                             face_sets_flood_fill_test test,
                                             const float threshold)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = (Mesh *)ob->data;
  BMesh *bm;

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  bm = sculpt_faceset_bm_begin(ob, ss, mesh);

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

    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);
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

          PBVHFaceRef fref2 = BKE_pbvh_index_to_face(ss->pbvh, neighbor_face_index);
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

  Mesh *me = nullptr;
  int *fmaps = nullptr;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    me = (Mesh *)ob->data;
    fmaps = (int *)CustomData_get_layer(&me->pdata, CD_FACEMAP);
  }
  else if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    fmaps = (int *)CustomData_get_layer(ss->pdata, CD_FACEMAP);
  }

  for (int i = 0; i < ss->totfaces; i++) {
    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);

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
  ss->face_sets = BKE_sculpt_face_sets_ensure(ob);

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);

  if (!nodes) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, op);
  for (const int i : blender::IndexRange(totnode)) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_FACE_SETS);
  }

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

  SCULPT_undo_push_end(ob);

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_redraw(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  MEM_SAFE_FREE(nodes);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts((Mesh *)ob->data);
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

enum eSculptFaceGroupVisibilityModes {
  SCULPT_FACE_SET_VISIBILITY_TOGGLE = 0,
  SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE = 1,
  SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE = 2,
  SCULPT_FACE_SET_VISIBILITY_INVERT = 3,
};

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
    {0, nullptr, 0, nullptr, nullptr},
};

void SCULPT_face_sets_visibility_all_set(SculptSession *ss, bool state)
{
  for (int i = 0; i < ss->totfaces; i++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

    *BKE_sculpt_face_attr_get<bool *>(face, ss->attrs.hide_poly) = !state;
  }
}

void SCULPT_face_sets_visibility_invert(SculptSession *ss)
{
  for (int i = 0; i < ss->totfaces; i++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

    *BKE_sculpt_face_attr_get<bool *>(face, ss->attrs.hide_poly) ^= true;
  }
}

bool sculpt_has_face_sets(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm) {
    return CustomData_get_offset_named(&ss->bm->pdata, CD_PROP_INT32, ".sculpt_face_set") != -1;
  }
  else {
    Mesh *mesh = BKE_object_get_original_mesh(ob);

    return CustomData_get_layer_named(&mesh->pdata, CD_PROP_INT32, ".sculpt_face_set");
  }
}

static int sculpt_face_sets_change_visibility_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  const int mode = RNA_enum_get(op->ptr, "mode");
  const int tot_vert = SCULPT_vertex_count_get(ss);

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);

  if (totnode == 0) {
    MEM_SAFE_FREE(nodes);
    return OPERATOR_CANCELLED;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  const int active_face_set = SCULPT_active_face_set_get(ss);

  SCULPT_undo_push_begin(ob, op);
  for (int i = 0; i < totnode; i++) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_HIDDEN);
  }

  switch (mode) {
    case SCULPT_FACE_SET_VISIBILITY_TOGGLE: {
      bool hidden_vertex = false;

      /* This can fail with regular meshes with non-manifold geometry as the visibility state can't
       * be synced from face sets to non-manifold vertices. */
      if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
        for (int i = 0; i < tot_vert; i++) {
          PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

          if (!SCULPT_vertex_visible_get(ss, vertex)) {
            hidden_vertex = true;
            break;
          }
        }
      }

      if (!hidden_vertex && ss->attrs.hide_poly) {
        for (int i = 0; i < ss->totfaces; i++) {
          PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

          if (*BKE_sculpt_face_attr_get<bool *>(face, ss->attrs.hide_poly)) {
            hidden_vertex = true;
            break;
          }
        }
      }

      if (hidden_vertex) {
        SCULPT_face_visibility_all_set(ss, true);
      }
      else {
        if (sculpt_has_face_sets(ob)) {
          SCULPT_face_visibility_all_set(ss, false);
          SCULPT_face_set_visibility_set(ss, active_face_set, true);
        }
        else {
          SCULPT_face_visibility_all_set(ss, true);
        }
      }
      break;
    }
    case SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE:
      ss->hide_poly = BKE_sculpt_hide_poly_ensure(ob);

      if (sculpt_has_face_sets(ob)) {
        SCULPT_face_visibility_all_set(ss, false);
        SCULPT_face_set_visibility_set(ss, active_face_set, true);
      }
      else {
        SCULPT_face_set_visibility_set(ss, active_face_set, true);
      }
      break;
    case SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE:
      ss->hide_poly = BKE_sculpt_hide_poly_ensure(ob);

      if (sculpt_has_face_sets(ob)) {
        SCULPT_face_set_visibility_set(ss, active_face_set, false);
      }
      else {
        SCULPT_face_visibility_all_set(ss, false);
      }

      break;
    case SCULPT_FACE_SET_VISIBILITY_INVERT:
      ss->hide_poly = BKE_sculpt_hide_poly_ensure(ob);
      SCULPT_face_visibility_all_invert(ss);
      break;
  }

  /* For modes that use the cursor active vertex, update the rotation origin for viewport
   * navigation.
   */
  if (ELEM(mode, SCULPT_FACE_SET_VISIBILITY_TOGGLE, SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE)) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    float location[3];
    copy_v3_v3(location, SCULPT_active_vertex_co_get(ss));
    mul_m4_v3(ob->object_to_world, location);
    copy_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter = 1;
    ups->last_stroke_valid = true;
  }

  /* Sync face sets visibility and vertex visibility. */
  SCULPT_visibility_sync_all_from_faces(ob);

  SCULPT_undo_push_end(ob);
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_visibility(nodes[i]);
    BKE_pbvh_bmesh_check_tris(ss->pbvh, nodes[i]);
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
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false);

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

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_face_sets_change_visibility_types,
               SCULPT_FACE_SET_VISIBILITY_TOGGLE,
               "Mode",
               "");
}

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator * /*op*/)
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  Mesh *mesh = (Mesh *)ob->data;

  SCULPT_face_random_access_ensure(ss);

  mesh->face_sets_color_seed += 1;
  if (ss->face_sets || (ss->bm && ss->cd_faceset_offset >= 0)) {
    const int random_index = clamp_i(ss->totfaces * BLI_hash_int_01(mesh->face_sets_color_seed),
                                     0,
                                     max_ii(0, ss->totfaces - 1));

    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, random_index);
    mesh->face_sets_color_default = SCULPT_face_set_get(ss, fref);
  }
  BKE_pbvh_face_sets_color_set(pbvh, mesh->face_sets_color_seed, mesh->face_sets_color_default);

  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);
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

enum eSculptFaceSetEditMode {
  SCULPT_FACE_SET_EDIT_GROW = 0,
  SCULPT_FACE_SET_EDIT_SHRINK = 1,
  SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY = 2,
  SCULPT_FACE_SET_EDIT_FAIR_POSITIONS = 3,
  SCULPT_FACE_SET_EDIT_FAIR_TANGENCY = 4,
  SCULPT_FACE_SET_EDIT_FAIR_CURVATURE = 5,
  SCULPT_FACE_SET_EDIT_FILL_COMPONENT = 6,
  SCULPT_FACE_SET_EDIT_EXTRUDE = 7,
  SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY = 8,
};

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
    {0, nullptr, 0, nullptr, nullptr},
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
  Vector<BMFace *> faces;

  if (ss->cd_faceset_offset < 0) {
    return;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) && !modify_hidden) {
      continue;
    }

    int fset = abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset));

    if (fset == active_face_set_id) {
      faces.append(f);
    }
  }

  for (BMFace *f : faces) {
    int ni = BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset);
    if (ni != DYNTOPO_NODE_NONE) {
      PBVHNode *node = BKE_pbvh_node_from_index(ss->pbvh, ni);

      if (node) {
        BKE_pbvh_node_mark_update(node);
        BKE_pbvh_node_mark_rebuild_draw(node);
      }
    }

    BMLoop *l = f->l_first;

    do {
      if (l->radial_next != l) {
        BM_ELEM_CD_SET_INT(l->radial_next->f, ss->cd_faceset_offset, active_face_set_id);
      }
      l = l->next;
    } while (l != f->l_first);
  }
}

static void rebuild_pbvh_draw_buffers(PBVH *pbvh)
{
  PBVHNode **nodes;
  int nodes_num;

  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &nodes_num);
  for (int i : IndexRange(nodes_num)) {
    BKE_pbvh_node_mark_update(nodes[i]);
    BKE_pbvh_node_mark_rebuild_draw(nodes[i]);
  }

  MEM_SAFE_FREE(nodes);
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
  const blender::Span<MPoly> polys = mesh->polys();
  const blender::Span<MLoop> loops = mesh->loops();

  for (const int p : polys.index_range()) {
    if (!modify_hidden && prev_face_sets[p] <= 0) {
      continue;
    }
    const MPoly *c_poly = &polys[p];
    for (int l = 0; l < c_poly->totloop; l++) {
      const MLoop *c_loop = &loops[c_poly->loopstart + l];
      const MeshElemMap *vert_map = &ss->pmap->pmap[c_loop->v];
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

  rebuild_pbvh_draw_buffers(ss->pbvh);
}

static void sculpt_face_set_fill_component(Object *ob,
                                           SculptSession *ss,
                                           const int active_face_set_id,
                                           const bool /*UNUSED(modify_hidden)*/)
{
  SCULPT_topology_islands_ensure(ob);
  GSet *connected_components = BLI_gset_int_new("affected_components");

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_face_set(ss, vertex, active_face_set_id)) {
      continue;
    }
    const int vertex_connected_component = SCULPT_vertex_island_get(ss, vertex);
    if (BLI_gset_haskey(connected_components, POINTER_FROM_INT(vertex_connected_component))) {
      continue;
    }
    BLI_gset_add(connected_components, POINTER_FROM_INT(vertex_connected_component));
  }

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    const int vertex_connected_component = SCULPT_vertex_island_get(ss, vertex);
    if (!BLI_gset_haskey(connected_components, POINTER_FROM_INT(vertex_connected_component))) {
      continue;
    }

    SCULPT_vertex_face_set_set(ss, vertex, active_face_set_id);
  }

  BLI_gset_free(connected_components, nullptr);
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
  Vector<BMFace *> faces;

  if (ss->cd_faceset_offset < 0) {
    return;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) && !modify_hidden) {
      continue;
    }

    int fset = abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset));

    if (fset == active_face_set_id) {
      faces.append(f);
    }
  }

  for (BMFace *f : faces) {
    int ni = BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset);
    if (ni != DYNTOPO_NODE_NONE) {
      PBVHNode *node = BKE_pbvh_node_from_index(ss->pbvh, ni);

      if (node) {
        BKE_pbvh_node_mark_update(node);
        BKE_pbvh_node_mark_rebuild_draw(node);
      }
    }

    BMLoop *l = f->l_first;
    do {
      if (!modify_hidden && BM_elem_flag_test(l->radial_next->f, BM_ELEM_HIDDEN)) {
        l = l->next;
        continue;
      }

      if (l->radial_next != l &&
          abs(BM_ELEM_CD_GET_INT(l->radial_next->f, ss->cd_faceset_offset)) !=
              abs(active_face_set_id)) {
        int fset = BM_ELEM_CD_GET_INT(l->radial_next->f, ss->cd_faceset_offset);
        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
        break;
      }
      l = l->next;
    } while (l != f->l_first);
  }
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
  const blender::Span<MPoly> polys = mesh->polys();
  const blender::Span<MLoop> loops = mesh->loops();
  for (const int p : polys.index_range()) {
    if (!modify_hidden && prev_face_sets[p] <= 0) {
      continue;
    }
    if (abs(prev_face_sets[p]) == active_face_set_id) {
      const MPoly *c_poly = &polys[p];
      for (int l = 0; l < c_poly->totloop; l++) {
        const MLoop *c_loop = &loops[c_poly->loopstart + l];
        const MeshElemMap *vert_map = &ss->pmap->pmap[c_loop->v];
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

  rebuild_pbvh_draw_buffers(ss->pbvh);
}

static bool check_single_face_set(SculptSession *ss, const bool check_visible_only)
{
  if (!ss->totfaces) {
    return true;
  }

  int first_face_set = SCULPT_FACE_SET_NONE;

  if (check_visible_only) {
    for (int f = 0; f < ss->totfaces; f++) {
      PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, f);
      int fset = SCULPT_face_set_get(ss, fref);

      if (fset > 0) {
        first_face_set = fset;
        break;
      }
    }
  }
  else {
    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, 0);
    first_face_set = abs(SCULPT_face_set_get(ss, fref));
  }

  if (first_face_set == SCULPT_FACE_SET_NONE) {
    return true;
  }

  for (int f = 0; f < ss->totfaces; f++) {
    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, f);

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

  Mesh *mesh = (Mesh *)ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  if (ss->bm) {
    Vector<BMFace *> faces;

    BMIter iter;
    BMFace *f;

    BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
      const int face_set_id = modify_hidden ? abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset)) :
                                              BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
      if (face_set_id == active_face_set_id) {
        faces.append(f);
      }
    }

    for (BMFace *f : faces) {
      BKE_pbvh_bmesh_remove_face(ss->pbvh, f, true);
    }
  }
  else {
    BMeshCreateParams params = {0};
    params.use_toolflags = true;

    BMesh *bm = BM_mesh_create(&allocsize, &params);

    BMeshFromMeshParams cparams = {0};
    cparams.calc_face_normal = true;
    cparams.active_shapekey = ob->shapenr;
    cparams.use_shapekey = true;
    cparams.create_shapekey_layers = true;

    BM_mesh_bm_from_me(bm, mesh, &cparams);

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

    BMeshToMeshParams tparams = {0};

    BM_mesh_bm_to_me(nullptr, bm, (Mesh *)ob->data, &tparams);

    BM_mesh_free(bm);
  }
}

static void sculpt_face_set_edit_fair_face_set(Object *ob,
                                               const int active_face_set_id,
                                               const int fair_order,
                                               float strength)
{
  SculptSession *ss = ob->sculpt;

  const int totvert = SCULPT_vertex_count_get(ss);

  Mesh *mesh = (Mesh *)ob->data;
  Vector<float3> orig_positions;
  Vector<bool> fair_verts;

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  orig_positions.resize(totvert);
  fair_verts.resize(totvert);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    orig_positions[i] = SCULPT_vertex_co_get(ss, vertex);
    fair_verts[i] = !SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_MESH) &&
                    SCULPT_vertex_has_face_set(ss, vertex, active_face_set_id) &&
                    SCULPT_vertex_has_unique_face_set(ss, vertex);
  }

  if (ss->bm) {
    BKE_bmesh_prefair_and_fair_verts(ss->bm, fair_verts.data(), (eMeshFairingDepth)fair_order);
  }
  else {
    float(*vert_positions)[3] = SCULPT_mesh_deformed_positions_get(ss);
    BKE_mesh_prefair_and_fair_verts(
        mesh, vert_positions, fair_verts.data(), (eMeshFairingDepth)fair_order);
  }

  for (int i = 0; i < totvert; i++) {
    if (fair_verts[i]) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
      float3 co = SCULPT_vertex_co_get(ss, vertex);

      interp_v3_v3v3(co, orig_positions[i], co, strength);

      SCULPT_vertex_co_set(ss, vertex, co);
    }
  }
}

static void sculpt_face_set_apply_edit(Object *ob,
                                       const int active_face_set_id,
                                       const int mode,
                                       const bool modify_hidden,
                                       const float strength = 1.0f)
{
  SculptSession *ss = ob->sculpt;

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_GROW: {
      int *prev_face_sets = ss->face_sets ? (int *)MEM_dupallocN(ss->face_sets) : nullptr;
      sculpt_face_set_grow(ob, ss, prev_face_sets, active_face_set_id, modify_hidden);
      MEM_SAFE_FREE(prev_face_sets);
      break;
    }
    case SCULPT_FACE_SET_EDIT_SHRINK: {
      int *prev_face_sets = ss->face_sets ? (int *)MEM_dupallocN(ss->face_sets) : nullptr;
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
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set_id, MESH_FAIRING_DEPTH_POSITION, strength);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set_id, MESH_FAIRING_DEPTH_TANGENCY, strength);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY: {
      GSet *face_sets_ids = BLI_gset_int_new("ids");
      for (int i = 0; i < ss->totfaces; i++) {
        BLI_gset_add(face_sets_ids, POINTER_FROM_INT(ss->face_sets[i]));
      }

      GSetIterator gs_iter;
      GSET_ITER (gs_iter, face_sets_ids) {
        const int face_set_id = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
        sculpt_face_set_edit_fair_face_set(ob, face_set_id, MESH_FAIRING_DEPTH_TANGENCY, strength);
      }

      BLI_gset_free(face_sets_ids, nullptr);
    } break;
    case SCULPT_FACE_SET_EDIT_FAIR_CURVATURE:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set_id, MESH_FAIRING_DEPTH_CURVATURE, strength);
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
      /* TODO: Multi-resolution topology representation using grids and duplicates can't be used
       * directly by the fair algorithm. Multi-resolution topology needs to be exposed in a
       * different way or converted to a mesh for this operation. */
      return false;
    }
  }

  return true;
}

static void sculpt_face_set_edit_modify_geometry(bContext *C,
                                                 Object *ob,
                                                 const int active_face_set,
                                                 const eSculptFaceSetEditMode mode,
                                                 const bool modify_hidden,
                                                 wmOperator *op)
{
  ED_sculpt_undo_geometry_begin(ob, op);
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, modify_hidden);
  ED_sculpt_undo_geometry_end(ob);
  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static void face_set_edit_do_post_visibility_updates(Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts((Mesh *)ob->data);
  }
}

static void sculpt_face_set_edit_modify_face_sets(Object *ob,
                                                  const int active_face_set,
                                                  const eSculptFaceSetEditMode mode,
                                                  const bool modify_hidden,
                                                  wmOperator *op)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);

  if (!nodes) {
    return;
  }
  SCULPT_undo_push_begin(ob, op);
  for (const int i : blender::IndexRange(totnode)) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_FACE_SETS);
  }
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, modify_hidden);
  SCULPT_undo_push_end(ob);
  face_set_edit_do_post_visibility_updates(ob, nodes, totnode);
  MEM_freeN(nodes);
}

static void sculpt_face_set_edit_modify_coordinates(bContext *C,
                                                    Object *ob,
                                                    const int active_face_set,
                                                    const eSculptFaceSetEditMode mode,
                                                    wmOperator *op)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(pbvh, nullptr, nullptr, &nodes, &totnode);

  const float strength = RNA_float_get(op->ptr, "strength");

  SCULPT_undo_push_begin(ob, op);
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update(nodes[i]);
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_COORDS);
  }
  sculpt_face_set_apply_edit(ob, abs(active_face_set), mode, false, strength);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
  SCULPT_undo_push_end(ob);
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
  float start_no[3];
} FaceSetExtrudeCD;

static void sculpt_bm_mesh_elem_hflag_disable_all(BMesh *bm, char htype, char hflag)
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

ATTR_NO_OPT static void sculpt_face_set_extrude_id(Object *ob,
                                                   bool no_islands,
                                                   SculptSession *ss,
                                                   const int active_face_set_id,
                                                   FaceSetExtrudeCD *fsecd)
{

  Mesh *mesh = (Mesh *)ob->data;
  int next_face_set_id = SCULPT_face_set_next_available_get(ss) + 1;

  SculptFaceSetIsland *island = nullptr;

  if (no_islands && ss->active_face.i != PBVH_REF_NONE) {
    island = SCULPT_face_set_island_get(ss, ss->active_face, active_face_set_id);

    /* convert PBVHFaceRef list into simple integers, only need to do for pbvh_bmesh*/
    if (island && ss->bm) {
      SCULPT_face_random_access_ensure(ss);

      for (int i = 0; i < island->totface; i++) {
        BMFace *f = (BMFace *)island->faces[i].i;
        island->faces[i].i = BM_elem_index_get(f);
      }
    }
  }

  no_islands = no_islands && island != nullptr;

  BMesh *bm = sculpt_faceset_bm_begin(ob, ss, mesh);
  if (ss->bm) {
    BKE_pbvh_bmesh_set_toolflags(ss->pbvh, true);
    BKE_sculptsession_update_attr_refs(ob);
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

  int mupdateflag = SCULPTVERT_NEED_DISK_SORT | SCULPTVERT_NEED_TRIANGULATE |
                    SCULPTVERT_NEED_VALENCE;

  Vector<BMVert *> retvs, vs;
  Vector<BMEdge *> es;

  int cd_faceset_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  const int tag1 = BM_ELEM_SELECT;
  const int tag2 = BM_ELEM_TAG_ALT;
  const int tag3 = BM_ELEM_TAG;

  int totface = no_islands ? island->totface : bm->totface;
  for (int i = 0; i < totface; i++) {
    BMFace *f = no_islands ? bm->ftable[island->faces[i].i] : bm->ftable[i];

    const int face_set_id = BM_ELEM_CD_GET_INT(f, cd_faceset_offset);

    if (face_set_id == active_face_set_id) {
      BM_elem_select_set(bm, (BMElem *)f, true);

      if (ss->bm) {
        BMLoop *l = f->l_first;

        do {
          if (!(BM_elem_flag_test(l->e, tag2))) {
            BM_elem_flag_enable(l->e, tag2);
            es.append(l->e);
          }

          if (!(BM_elem_flag_test(l->v, tag2))) {
            BM_elem_flag_enable(l->v, tag2);
            vs.append(l->v);
          }

        } while ((l = l->next) != f->l_first);

        if (ss->bm) {
          BKE_pbvh_bmesh_remove_face(ss->pbvh, f, true);
        }
      }
    }
    else {
      BM_elem_select_set(bm, (BMElem *)f, false);
    }

    BM_elem_flag_set(f, BM_ELEM_TAG, face_set_id == active_face_set_id);
  }

  Vector<BMFace *> borderfs;
  Vector<BMEdge *> borderes;
  Vector<BMVert *> bordervs;

  if (ss->bm) {
    for (BMEdge *e : es) {
      BMLoop *l = e->l;

      bool remove = true;
      do {
        if (!(BM_elem_flag_test(l->f, tag1))) {
          // remove = false;
          borderes.append(e);
          break;
        }
      } while ((l = l->radial_next) != e->l);

      if (remove) {
        if (!BM_elem_flag_test(e->v1, tag3)) {
          BM_log_vert_removed(ss->bm, ss->bm_log, e->v1);
          // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, e->v1, true);
          BM_elem_flag_enable(e->v1, tag3);
        }

        if (!BM_elem_flag_test(e->v2, tag3)) {
          BM_log_vert_removed(ss->bm, ss->bm_log, e->v2);
          // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, e->v2, true);
          BM_elem_flag_enable(e->v2, tag3);
        }

        BKE_pbvh_bmesh_remove_edge(ss->pbvh, e, true);
        e->head.hflag |= tag1;
      }
    }

    for (BMVert *v : vs) {
      BMEdge *e = v->e;
      bool remove = true;

      do {
        if (!BM_elem_flag_test(e, tag1)) {
          // remove = false;
          bordervs.append(v);
          break;
        }
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      if (BM_elem_flag_test(v, tag3)) {
        continue;
      }

      BM_elem_flag_enable(v, tag3);

      if (remove) {
        // BKE_pbvh_bmesh_remove_vertex(ss->pbvh, v, true);
        BM_log_vert_removed(ss->bm, ss->bm_log, v);
      }
    }
  }

  for (BMVert *v : bordervs) {
    BMFace *f2;
    BMIter iter;

    BM_ITER_ELEM (f2, &iter, v, BM_FACES_OF_VERT) {
      if (BM_elem_flag_test(f2, tag1) || BM_elem_flag_test(f2, tag2)) {
        continue;
      }

      if (ss->bm) {
        BKE_pbvh_bmesh_remove_face(ss->pbvh, f2, true);
      }

      BM_elem_flag_enable(f2, tag2);
      borderfs.append(f2);
    }
  }

  BM_mesh_select_flush(bm);
  BM_mesh_select_mode_flush(bm);

  BMOperator extop;
  BMO_op_init(bm, &extop, BMO_FLAG_DEFAULTS, "extrude_face_region");
  BMO_slot_bool_set(extop.slots_in, "use_normal_from_adjacent", true);
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

  int cd_sculpt_vert = CustomData_get_offset(&bm->vdata, CD_DYNTOPO_VERT);
  cd_faceset_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");  // recalc in case bmop changed it

  const int cd_boundary_flag = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(boundary_flags));

  BMOIter siter;
  BMElem *ele;

  if (ss->bm) { /* handle some pbvh stuff */
    for (int step = 0; step < 2; step++) {
      BMO_ITER (ele, &siter, extop.slots_out, step ? "side_geom.out" : "geom.out", BM_ALL_NOLOOP) {
        if (ele->head.htype == BM_VERT) {
          BM_ELEM_CD_SET_INT(ele, ss->cd_vert_node_offset, DYNTOPO_NODE_NONE);
        }
        else if (ele->head.htype == BM_FACE) {
          BM_ELEM_CD_SET_INT(ele, ss->cd_face_node_offset, DYNTOPO_NODE_NONE);
        }
      }
    }

    /*push a log subentry*/
    BM_log_entry_add_ex(bm, ss->bm_log, true);
  }

  for (int step = 0; step < 2; step++) {
    BMO_ITER (ele, &siter, extop.slots_out, step ? "side_geom.out" : "geom.out", BM_ALL_NOLOOP) {
      if (step == 0 && ele->head.htype != BM_VERT) {
        BM_elem_flag_set(ele, BM_ELEM_TAG, true);
      }

      if (step == 1 && ele->head.htype == BM_FACE) {
        BM_ELEM_CD_SET_INT(ele, cd_faceset_offset, next_face_set_id);
      }

      if (BM_elem_flag_test(ele, tag1)) {
        continue;
      }

      BM_elem_flag_enable(ele, tag1);

      switch (ele->head.htype) {
        case BM_VERT:
          if (ss->bm) {
            BM_log_vert_added(ss->bm, ss->bm_log, (BMVert *)ele);
          }

          if (step == 0) {
            retvs.append((BMVert *)ele);
          }

          break;
        case BM_EDGE: {
          BMEdge *e = (BMEdge *)ele;

          if (ss->bm) {
            BM_log_edge_added(ss->bm, ss->bm_log, e);

            if (!BM_elem_flag_test(e->v1, tag1)) {
              BM_elem_flag_enable(e->v1, tag1);
              BM_log_vert_added(ss->bm, ss->bm_log, e->v1);
            }

            if (!BM_elem_flag_test(e->v2, tag1)) {
              BM_elem_flag_enable(e->v2, tag1);
              BM_log_vert_added(ss->bm, ss->bm_log, e->v2);
            }

            if (1 || step == 1) {
              BMLoop *l = e->l;

              if (l) {
                do {
                  if (!BM_elem_flag_test(l->f, tag1)) {
                    BKE_pbvh_bmesh_add_face(ss->pbvh, l->f, false, false);
                    BM_log_face_added(ss->bm, ss->bm_log, l->f);
                  }

                  BM_elem_flag_enable(l->f, tag1);
                } while ((l = l->radial_next) != e->l);
              }
            }
          }
          break;
        }
        case BM_FACE: {
          BMFace *f = (BMFace *)ele;

          if (cd_sculpt_vert != -1) {
            BMLoop *l = f->l_first;
            do {
              *(int *)BM_ELEM_CD_GET_VOID_P(l->v,
                                            cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
              MSculptVert *mv = BKE_PBVH_SCULPTVERT(cd_sculpt_vert, l->v);
              MV_ADD_FLAG(mv, mupdateflag);
            } while ((l = l->next) != f->l_first);
          }

          if (ss->bm) {
            BKE_pbvh_bmesh_add_face(ss->pbvh, f, true, false);
          }

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
      BM_log_face_added(ss->bm, ss->bm_log, f);
    }
  }
#endif

  BMO_op_finish(bm, &extop);

  for (BMFace *f : borderfs) {
    if (BM_elem_is_free((BMElem *)f, BM_FACE)) {
      continue;
    }

    if (cd_sculpt_vert >= 0) {
      BMLoop *l = f->l_first;
      do {
        *(int *)BM_ELEM_CD_GET_VOID_P(l->v, cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;

        MSculptVert *mv = BKE_PBVH_SCULPTVERT(cd_sculpt_vert, l->v);
        MV_ADD_FLAG(mv, mupdateflag);
      } while ((l = l->next) != f->l_first);
    }

    if (ss->bm && !BM_elem_flag_test(f, tag1)) {
      BKE_pbvh_bmesh_add_face(ss->pbvh, f, true, false);
    }

    BM_elem_flag_enable(f, tag1);
  }

  for (BMVert *v : retvs) {
    BM_elem_flag_enable(v, BM_ELEM_TAG);
  }

  /* Set the new Face Set ID for the extrusion. */
  const int cd_face_sets_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  BM_mesh_elem_table_ensure(bm, BM_FACE);
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int face_set_id = BM_ELEM_CD_GET_INT(f, cd_face_sets_offset);
    if (abs(face_set_id) == active_face_set_id) {
      continue;
    }

    const int cd_sculpt_vert = CustomData_get_offset(&bm->vdata, CD_DYNTOPO_VERT);

    BMLoop *l = f->l_first;

    do {
      if (cd_boundary_flag != -1) {
        *(int *)BM_ELEM_CD_GET_VOID_P(l->v, cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
      }

      if (cd_sculpt_vert != -1) {
        MSculptVert *mv = (MSculptVert *)BM_ELEM_CD_GET_VOID_P(l->v, cd_sculpt_vert);

        MV_ADD_FLAG(mv, mupdateflag);
      }
    } while ((l = l->next) != f->l_first);
  }

  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  /*
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  */

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

  fsecd->verts = (int *)MEM_malloc_arrayN(retvs.size(), sizeof(int), "face set extrude verts");
  fsecd->totvert = retvs.size();

  fsecd->orig_co = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(retvs.size(), sizeof(float) * 3, "face set extrude verts"));
  fsecd->orig_no = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(retvs.size(), sizeof(float) * 3, "face set extrude verts"));

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  for (int i = 0; i < retvs.size(); i++) {
    BMVert *v = retvs[i];

    fsecd->verts[i] = v->head.index;
    copy_v3_v3(fsecd->orig_co[i], v->co);

    BMIter iter;
    BMFace *f;

    float no[3] = {0.0f, 0.0f, 0.0f};

    BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
      int fset = BM_ELEM_CD_GET_INT(f, cd_faceset_offset);
      if (fset == active_face_set_id) {
        add_v3_v3(no, f->no);
      }
    }

    normalize_v3(no);
    copy_v3_v3(fsecd->orig_no[i], no);
  }

  if (island) {
    SCULPT_face_set_island_free(island);
  }

  if (!ss->bm) {
    BMeshToMeshParams params = {0};

    BM_mesh_bm_to_me(nullptr, bm, (Mesh *)ob->data, &params);
  }

  sculpt_faceset_bm_end(ss, bm);

  if (ss->bm) {
    // slow! BKE_pbvh_bmesh_set_toolflags(ss->pbvh, false);
    BKE_sculptsession_update_attr_refs(ob);
  }
}

static void island_stack_bmesh_do(
    SculptSession *ss, int fset, PBVHFaceRef face, Vector<PBVHFaceRef> &faces, BLI_bitmap *visit)
{
  BMFace *f = (BMFace *)face.i;

  BMLoop *l = f->l_first;
  do {
    BMLoop *l2 = l;

    do {
      int index = BM_elem_index_get(l2->f);

      bool ok = !BLI_BITMAP_TEST(visit, index);
      ok = ok && abs(BM_ELEM_CD_GET_INT(l2->f, ss->cd_faceset_offset)) == fset;

      if (ok) {
        BLI_BITMAP_SET(visit, index, true);

        faces.append(BKE_pbvh_make_fref((intptr_t)l2->f));
      }
    } while ((l2 = l2->radial_next) != l);
  } while ((l = l->next) != f->l_first);
}

static void island_stack_mesh_do(
    SculptSession *ss, int fset, PBVHFaceRef face, Vector<PBVHFaceRef> &faces, BLI_bitmap *visit)
{
  const MPoly *mp = ss->polys + face.i;
  const MLoop *ml = ss->loops + mp->loopstart;

  for (int i = 0; i < mp->totloop; i++, ml++) {
    MeshElemMap *ep = ss->epmap + ml->e;

    for (int j = 0; j < ep->count; j++) {
      int f2 = ep->indices[j];

      if (abs(ss->face_sets[f2]) == fset && !BLI_BITMAP_TEST(visit, f2)) {
        BLI_BITMAP_SET(visit, f2, true);
        PBVHFaceRef face2 = {f2};

        faces.append(face2);
      }
    }
  }
}

SculptFaceSetIslands *SCULPT_face_set_islands_get(SculptSession *ss, int fset)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH && !ss->epmap) {
    BKE_mesh_edge_poly_map_create(&ss->epmap,
                                  &ss->epmap_mem,
                                  ss->totedges,
                                  ss->polys,
                                  ss->totfaces,
                                  ss->loops,
                                  ss->totloops);
  }

  SculptFaceSetIslands *ret = (SculptFaceSetIslands *)MEM_callocN(sizeof(*ret), "fset islands");
  Vector<SculptFaceSetIsland> islands;

  int totface = ss->totfaces;
  BLI_bitmap *visit = BLI_BITMAP_NEW(totface, __func__);
  Vector<PBVHFaceRef> stack;

  SCULPT_face_random_access_ensure(ss);

  for (int i = 0; i < totface; i++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

    if (abs(SCULPT_face_set_get(ss, face)) != fset) {
      continue;
    }

    if (BLI_BITMAP_TEST(visit, i)) {
      continue;
    }

    BLI_BITMAP_SET(visit, i, true);

    stack.resize(0);
    stack.append(face);

    Vector<PBVHFaceRef> faces;

    while (stack.size() > 0) {
      // can't use BLI_array_pop since it doesn't work with popping structures
      PBVHFaceRef face2 = stack.pop_last();
      faces.append(face2);

      if (ss->bm) {
        island_stack_bmesh_do(ss, fset, face2, stack, visit);
      }
      else {
        island_stack_mesh_do(ss, fset, face2, stack, visit);
      }
    }

    PBVHFaceRef *cfaces = (PBVHFaceRef *)MEM_malloc_arrayN(
        faces.size(), sizeof(PBVHFaceRef), __func__);
    memcpy((void *)cfaces, (void *)faces.data(), sizeof(PBVHFaceRef) * faces.size());

    SculptFaceSetIsland island;

    island.faces = cfaces;
    island.totface = faces.size();

    islands.append(island);
  }

  SculptFaceSetIsland *cislands = (SculptFaceSetIsland *)MEM_malloc_arrayN(
      islands.size(), sizeof(SculptFaceSetIsland *), __func__);
  memcpy((void *)cislands, (void *)islands.data(), sizeof(SculptFaceSetIsland *) * islands.size());

  ret->islands = cislands;
  ret->totisland = islands.size();

  MEM_SAFE_FREE(visit);
  return ret;
}

void SCULPT_face_set_islands_free(SculptSession *ss, SculptFaceSetIslands *islands)
{
  for (int i = 0; i < islands->totisland; i++) {
    MEM_SAFE_FREE(islands->islands[i].faces);
  }

  MEM_SAFE_FREE(islands->islands);
  MEM_SAFE_FREE(islands);
}

SculptFaceSetIsland *SCULPT_face_set_island_get(SculptSession *ss, PBVHFaceRef face, int fset)
{
  SculptFaceSetIslands *islands = SCULPT_face_set_islands_get(ss, fset);

  for (int i = 0; i < islands->totisland; i++) {
    SculptFaceSetIsland *island = islands->islands + i;

    for (int j = 0; j < island->totface; j++) {
      if (island->faces[j].i == face.i) {
        SculptFaceSetIsland *ret = (SculptFaceSetIsland *)MEM_callocN(sizeof(SculptFaceSetIsland),
                                                                      "SculptFaceSetIsland");

        *ret = *island;

        // prevent faces from freeing
        island->faces = nullptr;

        SCULPT_face_set_islands_free(ss, islands);
        return ret;
      }
    }
  }

  SCULPT_face_set_islands_free(ss, islands);
  return nullptr;
}

void SCULPT_face_set_island_free(SculptFaceSetIsland *island)
{
  if (island) {
    MEM_SAFE_FREE(island->faces);
    MEM_freeN(island);
  }
}

ATTR_NO_OPT static int sculpt_face_set_edit_modal(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const int mode = RNA_enum_get(op->ptr, "mode");
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  if (mode != SCULPT_FACE_SET_EDIT_EXTRUDE) {
    return OPERATOR_FINISHED;
  }

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    FaceSetExtrudeCD *fsecd = (FaceSetExtrudeCD *)op->customdata;
    MEM_SAFE_FREE(fsecd->orig_co);
    MEM_SAFE_FREE(fsecd->orig_no);
    MEM_SAFE_FREE(fsecd->verts);
    MEM_SAFE_FREE(op->customdata);

    if (ss->bm) {
      SCULPT_undo_push_end(ob);
    }
    else {
      ED_sculpt_undo_geometry_end(ob);
    }

    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  FaceSetExtrudeCD *fsecd = (FaceSetExtrudeCD *)op->customdata;
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  float depth_world_space[3];
  float new_pos[3];

  mul_v3_m4v3(depth_world_space, ob->object_to_world, fsecd->cursor_location);

  float fmval[2] = {(float)event->mval[0], (float)event->mval[1]};

  ED_view3d_win_to_3d(vc.v3d, vc.region, depth_world_space, fmval, new_pos);
  float extrude_disp = len_v3v3(depth_world_space, new_pos);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  if (dot_v3v3(fsecd->start_no, fsecd->start_no) == 0.0f && ss->active_face.i != PBVH_REF_NONE) {
    float fno[4];

    SCULPT_face_normal_get(ss, ss->active_face, fno);
    fno[3] = 0.0f;

    mul_v4_m4v4(fno, ob->object_to_world, fno);
    copy_v3_v3(fsecd->start_no, fno);
    // extrude_disp *= -1.0f;
  }

  float grabtan[3];
  sub_v3_v3v3(grabtan, new_pos, depth_world_space);
  if (dot_v3v3(fsecd->start_no, fsecd->start_no) > 0.0f &&
      dot_v3v3(grabtan, fsecd->start_no) < 0) {
    extrude_disp *= -1.0f;
  }

  RNA_float_set(op->ptr, "extrude_disp", extrude_disp);

  if (!ss->bm) {
    float(*vert_positions)[3] = SCULPT_mesh_deformed_positions_get(ss);
    for (int i = 0; i < fsecd->totvert; i++) {
      int idx = fsecd->verts[i];

      madd_v3_v3v3fl(vert_positions[idx], fsecd->orig_co[i], fsecd->orig_no[i], extrude_disp);
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, BKE_pbvh_make_vref(idx));
    }

    rebuild_pbvh_draw_buffers(ss->pbvh);
  }
  else {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);

    for (int i = 0; i < fsecd->totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, fsecd->verts[i]);

      BMVert *v = (BMVert *)vertex.i;

      int ni = BM_ELEM_CD_GET_INT(v, ss->cd_vert_node_offset);
      if (ni != DYNTOPO_NODE_NONE) {
        PBVHNode *node = BKE_pbvh_node_from_index(ss->pbvh, ni);

        if (node) {
          BKE_pbvh_node_mark_update(node);
          BKE_pbvh_node_mark_rebuild_draw(node);
        }
      }

      madd_v3_v3v3fl(v->co, fsecd->orig_co[i], fsecd->orig_no[i], extrude_disp);
    }
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_tag_update_overlays(C);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_face_set_extrude(bContext *C,
                                    wmOperator *op,
                                    const int mval[2],
                                    Object *ob,
                                    const int active_face_set,
                                    const float cursor_location[3])
{
  FaceSetExtrudeCD *fsecd = (FaceSetExtrudeCD *)MEM_callocN(sizeof(FaceSetExtrudeCD),
                                                            "face set extrude cd");

  fsecd->active_face_set = active_face_set;
  copy_v3_v3(fsecd->cursor_location, cursor_location);
  float fmval[2] = {(float)mval[0], (float)mval[1]};
  copy_v2_v2(fsecd->init_mval, fmval);
  op->customdata = fsecd;

  bool no_islands = RNA_boolean_get(op->ptr, "single_island_only");

  if (!ob->sculpt->bm) {
    ED_sculpt_undo_geometry_begin(ob, op);
  }
  else {
    SCULPT_undo_push_begin(ob, op);
    SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_COORDS);
  }

  sculpt_face_set_extrude_id(ob, no_islands, ob->sculpt, active_face_set, fsecd);

  if (!ob->sculpt->bm) {
    BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static bool sculpt_face_set_edit_init(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const eSculptFaceSetEditMode mode = static_cast<eSculptFaceSetEditMode>(
      RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  if (!sculpt_face_set_edit_is_operation_valid(ss, mode, modify_hidden)) {
    return false;
  }

  ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  return true;
}

static int sculpt_face_set_edit_exec(bContext *C, wmOperator *op)
{
  if (!sculpt_face_set_edit_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  Object *ob = CTX_data_active_object(C);

  const int active_face_set = RNA_int_get(op->ptr, "active_face_set");
  const eSculptFaceSetEditMode mode = static_cast<eSculptFaceSetEditMode>(
      RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  int mval[2];
  float location[3];

  RNA_int_get_array(op->ptr, "mouse", mval);
  RNA_float_get_array(op->ptr, "location", location);

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_EXTRUDE:
      sculpt_face_set_extrude(C, op, mval, ob, active_face_set, location);

      SCULPT_tag_update_overlays(C);
      return OPERATOR_RUNNING_MODAL;
    case SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY:
      sculpt_face_set_edit_modify_geometry(C, ob, active_face_set, mode, modify_hidden, op);
      break;
    case SCULPT_FACE_SET_EDIT_GROW:
    case SCULPT_FACE_SET_EDIT_SHRINK:
    case SCULPT_FACE_SET_EDIT_FILL_COMPONENT:
      sculpt_face_set_edit_modify_face_sets(ob, active_face_set, mode, modify_hidden, op);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_POSITIONS:
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
    case SCULPT_FACE_SET_EDIT_FAIR_CURVATURE:
    case SCULPT_FACE_SET_EDIT_FAIR_ALL_TANGENCY:
      sculpt_face_set_edit_modify_coordinates(C, ob, active_face_set, mode, op);
      break;
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static int sculpt_face_set_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  /* Update the current active Face Set and Vertex as the operator can be used directly from the
   * tool without brush cursor. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  if (!SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false)) {
    /* The cursor is not over the mesh. Cancel to avoid editing the last updated Face Set ID. */
    return OPERATOR_CANCELLED;
  }

  RNA_int_set(op->ptr, "active_face_set", SCULPT_active_face_set_get(ss));
  RNA_int_set_array(op->ptr, "mouse", event->mval);
  RNA_float_set_array(op->ptr, "location", sgi.location);

  const int mode = RNA_enum_get(op->ptr, "mode");

  if (mode == SCULPT_FACE_SET_EDIT_EXTRUDE) {
    const int active_face_set = RNA_int_get(op->ptr, "active_face_set");

    WM_event_add_modal_handler(C, op);
    sculpt_face_set_extrude(C, op, event->mval, ob, active_face_set, sgi.location);

    return OPERATOR_RUNNING_MODAL;
  }

  return sculpt_face_set_edit_exec(C, op);
}

void SCULPT_OT_face_sets_edit(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Edit Face Set";
  ot->idname = "SCULPT_OT_face_set_edit";
  ot->description = "Edits the current active Face Set";

  /* Api callbacks. */
  ot->invoke = sculpt_face_set_edit_invoke;
  ot->exec = sculpt_face_set_edit_exec;
  ot->poll = SCULPT_mode_poll;
  ot->modal = sculpt_face_set_edit_modal;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR | OPTYPE_BLOCKING;

  PropertyRNA *prop = RNA_def_int(
      ot->srna, "active_face_set", 1, 0, INT_MAX, "Active Face Set", "", 0, 64);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_edit_types, SCULPT_FACE_SET_EDIT_GROW, "Mode", "");
  RNA_def_float(ot->srna, "strength", 1.0f, 0.0f, 1.0f, "Strength", "", 0.0f, 1.0f);

  ot->prop = RNA_def_boolean(ot->srna,
                             "modify_hidden",
                             true,
                             "Modify Hidden",
                             "Apply the edit operation to hidden Face Sets");
  ot->prop = RNA_def_boolean(ot->srna,
                             "single_island_only",
                             false,
                             "Ignore Disconnected",
                             "Apply the edit operation to a single island only");

  prop = RNA_def_float_array(
      ot->srna, "location", 3, nullptr, FLT_MIN, FLT_MAX, "Location", "", -100000, 100000);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  prop = RNA_def_int_array(ot->srna, "mouse", 2, nullptr, 0, 16000, "Mouse", "", 0, 4000);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  prop = RNA_def_float(ot->srna, "extrude_disp", 0.0, FLT_MIN, FLT_MAX, "", "", 0.0, 1.0);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}
