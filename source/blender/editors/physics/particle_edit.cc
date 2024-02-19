/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_kdtree.h"
#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_rand.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_time_utildefines.h"
#include "BLI_utildefines.h"

#include "BKE_bvhutils.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_particle.hh"
#include "ED_physics.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph_query.hh"

#include "physics_intern.h"

#include "particle_edit_utildefines.h"

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

bool PE_poll(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  if (!scene || !ob || !(ob->mode & OB_MODE_PARTICLE_EDIT)) {
    return false;
  }

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  if (edit == nullptr) {
    return false;
  }
  if (edit->psmd_eval == nullptr || edit->psmd_eval->mesh_final == nullptr) {
    return false;
  }

  return true;
}

bool PE_hair_poll(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  if (!scene || !ob || !(ob->mode & OB_MODE_PARTICLE_EDIT)) {
    return false;
  }

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  if (edit == nullptr || edit->psys == nullptr) {
    return false;
  }
  if (edit->psmd_eval == nullptr || edit->psmd_eval->mesh_final == nullptr) {
    return false;
  }

  return true;
}

bool PE_poll_view3d(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  return (PE_poll(C) && (area && area->spacetype == SPACE_VIEW3D) &&
          (region && region->regiontype == RGN_TYPE_WINDOW));
}

void PE_free_ptcache_edit(PTCacheEdit *edit)
{
  POINT_P;

  if (edit == nullptr) {
    return;
  }

  if (edit->points) {
    LOOP_POINTS {
      if (point->keys) {
        MEM_freeN(point->keys);
      }
    }

    MEM_freeN(edit->points);
  }

  if (edit->mirror_cache) {
    MEM_freeN(edit->mirror_cache);
  }

  if (edit->emitter_cosnos) {
    MEM_freeN(edit->emitter_cosnos);
    edit->emitter_cosnos = nullptr;
  }

  if (edit->emitter_field) {
    BLI_kdtree_3d_free(edit->emitter_field);
    edit->emitter_field = nullptr;
  }

  psys_free_path_cache(edit->psys, edit);

  MEM_freeN(edit);
}

int PE_minmax(Depsgraph *depsgraph,
              Scene *scene,
              ViewLayer *view_layer,
              blender::float3 &min,
              blender::float3 &max)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys;
  ParticleSystemModifierData *psmd_eval = nullptr;
  POINT_P;
  KEY_K;
  float co[3], mat[4][4];
  int ok = 0;

  if (!edit) {
    return ok;
  }

  if ((psys = edit->psys)) {
    psmd_eval = edit->psmd_eval;
  }
  else {
    unit_m4(mat);
  }

  LOOP_VISIBLE_POINTS {
    if (psys) {
      psys_mat_hair_to_global(
          ob, psmd_eval->mesh_final, psys->part->from, psys->particles + p, mat);
    }

    LOOP_SELECTED_KEYS {
      copy_v3_v3(co, key->co);
      mul_m4_v3(mat, co);
      blender::math::min_max(blender::float3(co), min, max);
      ok = 1;
    }
  }

  if (!ok) {
    BKE_object_minmax(ob, min, max);
    ok = 1;
  }

  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Mode Helpers
 * \{ */

int PE_start_edit(PTCacheEdit *edit)
{
  if (edit) {
    edit->edited = 1;
    if (edit->psys) {
      edit->psys->flag |= PSYS_EDITED;
    }
    return 1;
  }

  return 0;
}

ParticleEditSettings *PE_settings(Scene *scene)
{
  return scene->toolsettings ? &scene->toolsettings->particle : nullptr;
}

static float pe_brush_size_get(const Scene * /*scene*/, ParticleBrushData *brush)
{
#if 0 /* TODO: Here we can enable unified brush size, needs more work. */
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  float size = (ups->flag & UNIFIED_PAINT_SIZE) ? ups->size : brush->size;
#endif

  return brush->size;
}

PTCacheEdit *PE_get_current_from_psys(ParticleSystem *psys)
{
  if (psys->part && psys->part->type == PART_HAIR) {
    if ((psys->flag & PSYS_HAIR_DYNAMICS) != 0 && (psys->pointcache->flag & PTCACHE_BAKED) != 0) {
      return psys->pointcache->edit;
    }
    return psys->edit;
  }
  if (psys->pointcache->flag & PTCACHE_BAKED) {
    return psys->pointcache->edit;
  }
  return nullptr;
}

/* NOTE: Similar to creation of edit, but only updates pointers in the
 * existing struct.
 */
static void pe_update_hair_particle_edit_pointers(PTCacheEdit *edit)
{
  ParticleSystem *psys = edit->psys;
  ParticleData *pa = psys->particles;
  for (int p = 0; p < edit->totpoint; p++) {
    PTCacheEditPoint *point = &edit->points[p];
    HairKey *hair_key = pa->hair;
    for (int k = 0; k < point->totkey; k++) {
      PTCacheEditKey *key = &point->keys[k];
      key->co = hair_key->co;
      key->time = &hair_key->time;
      key->flag = hair_key->editflag;
      if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
        key->flag |= PEK_USE_WCO;
        hair_key->editflag |= PEK_USE_WCO;
      }
      hair_key++;
    }
    pa++;
  }
}

/* always gets at least the first particlesystem even if PSYS_CURRENT flag is not set
 *
 * NOTE: this function runs on poll, therefore it can runs many times a second
 * keep it fast! */
static PTCacheEdit *pe_get_current(Depsgraph *depsgraph, Scene *scene, Object *ob, bool create)
{
  ParticleEditSettings *pset = PE_settings(scene);
  PTCacheEdit *edit = nullptr;
  ListBase pidlist;
  PTCacheID *pid;

  if (pset == nullptr || ob == nullptr) {
    return nullptr;
  }

  pset->scene = scene;
  pset->object = ob;

  BKE_ptcache_ids_from_object(&pidlist, ob, nullptr, 0);

  /* in the case of only one editable thing, set pset->edittype accordingly */
  if (BLI_listbase_is_single(&pidlist)) {
    pid = static_cast<PTCacheID *>(pidlist.first);
    switch (pid->type) {
      case PTCACHE_TYPE_PARTICLES:
        pset->edittype = PE_TYPE_PARTICLES;
        break;
      case PTCACHE_TYPE_SOFTBODY:
        pset->edittype = PE_TYPE_SOFTBODY;
        break;
      case PTCACHE_TYPE_CLOTH:
        pset->edittype = PE_TYPE_CLOTH;
        break;
    }
  }

  for (pid = static_cast<PTCacheID *>(pidlist.first); pid; pid = pid->next) {
    if (pset->edittype == PE_TYPE_PARTICLES && pid->type == PTCACHE_TYPE_PARTICLES) {
      ParticleSystem *psys = static_cast<ParticleSystem *>(pid->calldata);

      if (psys->flag & PSYS_CURRENT) {
        if (psys->part && psys->part->type == PART_HAIR) {
          if (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED) {
            if (create && !psys->pointcache->edit) {
              PE_create_particle_edit(depsgraph, scene, ob, pid->cache, nullptr);
            }
            edit = pid->cache->edit;
          }
          else {
            if (create && !psys->edit) {
              if (psys->flag & PSYS_HAIR_DONE) {
                PE_create_particle_edit(depsgraph, scene, ob, nullptr, psys);
              }
            }
            edit = psys->edit;
          }
        }
        else {
          if (create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit) {
            PE_create_particle_edit(depsgraph, scene, ob, pid->cache, psys);
          }
          edit = pid->cache->edit;
        }

        break;
      }
    }
    else if (pset->edittype == PE_TYPE_SOFTBODY && pid->type == PTCACHE_TYPE_SOFTBODY) {
      if (create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit) {
        pset->flag |= PE_FADE_TIME;
        /* Nice to have but doesn't work: `pset->brushtype = PE_BRUSH_COMB;`. */
        PE_create_particle_edit(depsgraph, scene, ob, pid->cache, nullptr);
      }
      edit = pid->cache->edit;
      break;
    }
    else if (pset->edittype == PE_TYPE_CLOTH && pid->type == PTCACHE_TYPE_CLOTH) {
      if (create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit) {
        pset->flag |= PE_FADE_TIME;
        /* Nice to have but doesn't work: `pset->brushtype = PE_BRUSH_COMB;`. */
        PE_create_particle_edit(depsgraph, scene, ob, pid->cache, nullptr);
      }
      edit = pid->cache->edit;
      break;
    }
  }

  /* Don't consider inactive or render dependency graphs, since they might be evaluated for a
   * different number of children. or have different pointer to evaluated particle system or
   * modifier which will also cause troubles. */
  if (edit && DEG_is_active(depsgraph)) {
    edit->pid = *pid;
    if (edit->flags & PT_CACHE_EDIT_UPDATE_PARTICLE_FROM_EVAL) {
      if (edit->psys != nullptr && edit->psys_eval != nullptr) {
        psys_copy_particles(edit->psys, edit->psys_eval);
        pe_update_hair_particle_edit_pointers(edit);
      }
      edit->flags &= ~PT_CACHE_EDIT_UPDATE_PARTICLE_FROM_EVAL;
    }
  }

  BLI_freelistN(&pidlist);

  return edit;
}

PTCacheEdit *PE_get_current(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  return pe_get_current(depsgraph, scene, ob, false);
}

PTCacheEdit *PE_create_current(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  return pe_get_current(depsgraph, scene, ob, true);
}

void PE_current_changed(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  if (ob->mode == OB_MODE_PARTICLE_EDIT) {
    PE_create_current(depsgraph, scene, ob);
  }
}

void PE_hide_keys_time(Scene *scene, PTCacheEdit *edit, float cfra)
{
  ParticleEditSettings *pset = PE_settings(scene);
  POINT_P;
  KEY_K;

  if (pset->flag & PE_FADE_TIME && pset->selectmode == SCE_SELECT_POINT) {
    LOOP_POINTS {
      LOOP_KEYS {
        if (fabsf(cfra - *key->time) < pset->fade_frames) {
          key->flag &= ~PEK_HIDE;
        }
        else {
          key->flag |= PEK_HIDE;
          // key->flag &= ~PEK_SELECT;
        }
      }
    }
  }
  else {
    LOOP_POINTS {
      LOOP_KEYS {
        key->flag &= ~PEK_HIDE;
      }
    }
  }
}

static int pe_x_mirror(Object *ob)
{
  if (ob->type == OB_MESH) {
    return (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X);
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Struct Passed to Callbacks
 * \{ */

struct PEData {
  ViewContext vc;
  ViewDepths *depths;

  const bContext *context;
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  Object *ob;
  Mesh *mesh;
  PTCacheEdit *edit;
  BVHTreeFromMesh shape_bvh;
  Depsgraph *depsgraph;

  RNG *rng;

  const int *mval;
  const rcti *rect;
  float rad;
  float dval;
  int select;
  eSelectOp sel_op;

  float *dvec;
  float combfac;
  float pufffac;
  float cutfac;
  float smoothfac;
  float weightfac;
  float growfac;
  int totrekey;

  int invert;
  int tot;
  float vec[3];

  int select_action;
  int select_toggle_action;
  bool is_changed;

  void *user_data;
};

static void PE_set_data(bContext *C, PEData *data)
{
  *data = {};

  data->context = C;
  data->bmain = CTX_data_main(C);
  data->scene = CTX_data_scene(C);
  data->view_layer = CTX_data_view_layer(C);
  data->ob = CTX_data_active_object(C);
  data->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  data->edit = PE_get_current(data->depsgraph, data->scene, data->ob);
}

static void PE_set_view3d_data(bContext *C, PEData *data)
{
  PE_set_data(C, data);

  data->vc = ED_view3d_viewcontext_init(C, data->depsgraph);

  if (!XRAY_ENABLED(data->vc.v3d)) {
    ED_view3d_depth_override(data->depsgraph,
                             data->vc.region,
                             data->vc.v3d,
                             data->vc.obact,
                             V3D_DEPTH_OBJECT_ONLY,
                             &data->depths);
  }
}

static bool PE_create_shape_tree(PEData *data, Object *shapeob)
{
  Object *shapeob_eval = DEG_get_evaluated_object(data->depsgraph, shapeob);
  const Mesh *mesh = BKE_object_get_evaluated_mesh(shapeob_eval);

  data->shape_bvh = {};

  if (!mesh) {
    return false;
  }

  return (BKE_bvhtree_from_mesh_get(&data->shape_bvh, mesh, BVHTREE_FROM_CORNER_TRIS, 4) !=
          nullptr);
}

static void PE_free_shape_tree(PEData *data)
{
  free_bvhtree_from_mesh(&data->shape_bvh);
}

static void PE_create_random_generator(PEData *data)
{
  uint rng_seed = uint(BLI_time_now_seconds_i() & UINT_MAX);
  rng_seed ^= POINTER_AS_UINT(data->ob);
  rng_seed ^= POINTER_AS_UINT(data->edit);
  data->rng = BLI_rng_new(rng_seed);
}

static void PE_free_random_generator(PEData *data)
{
  if (data->rng != nullptr) {
    BLI_rng_free(data->rng);
    data->rng = nullptr;
  }
}

static void PE_data_free(PEData *data)
{
  PE_free_random_generator(data);
  PE_free_shape_tree(data);
  if (data->depths) {
    ED_view3d_depths_free(data->depths);
    data->depths = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

static bool key_test_depth(const PEData *data, const float co[3], const int screen_co[2])
{
  View3D *v3d = data->vc.v3d;
  ViewDepths *vd = data->depths;
  float depth;

  /* nothing to do */
  if (XRAY_ENABLED(v3d)) {
    return true;
  }

/* used to calculate here but all callers have  the screen_co already, so pass as arg */
#if 0
  if (ED_view3d_project_int_global(data->vc.region,
                                   co,
                                   screen_co,
                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN |
                                       V3D_PROJ_TEST_CLIP_NEAR) != V3D_PROJ_RET_OK)
  {
    return 0;
  }
#endif

  /* check if screen_co is within bounds because brush_cut uses out of screen coords */
  if (screen_co[0] >= 0 && screen_co[0] < vd->w && screen_co[1] >= 0 && screen_co[1] < vd->h) {
    BLI_assert(vd && vd->depths);
    /* we know its not clipped */
    depth = vd->depths[screen_co[1] * vd->w + screen_co[0]];
  }
  else {
    return false;
  }

  float win[3];
  ED_view3d_project_v3(data->vc.region, co, win);

  if (win[2] - 0.00001f > depth) {
    return false;
  }
  return true;
}

static bool key_inside_circle(const PEData *data, float rad, const float co[3], float *distance)
{
  float dx, dy, dist;
  int screen_co[2];

  /* TODO: should this check V3D_PROJ_TEST_CLIP_BB too? */
  if (ED_view3d_project_int_global(data->vc.region, co, screen_co, V3D_PROJ_TEST_CLIP_WIN) !=
      V3D_PROJ_RET_OK)
  {
    return false;
  }

  dx = data->mval[0] - screen_co[0];
  dy = data->mval[1] - screen_co[1];
  dist = sqrtf(dx * dx + dy * dy);

  if (dist > rad) {
    return false;
  }

  if (key_test_depth(data, co, screen_co)) {
    if (distance) {
      *distance = dist;
    }

    return true;
  }

  return false;
}

static bool key_inside_rect(PEData *data, const float co[3])
{
  int screen_co[2];

  if (ED_view3d_project_int_global(data->vc.region, co, screen_co, V3D_PROJ_TEST_CLIP_WIN) !=
      V3D_PROJ_RET_OK)
  {
    return false;
  }

  if (screen_co[0] > data->rect->xmin && screen_co[0] < data->rect->xmax &&
      screen_co[1] > data->rect->ymin && screen_co[1] < data->rect->ymax)
  {
    return key_test_depth(data, co, screen_co);
  }

  return false;
}

static bool key_inside_test(PEData *data, const float co[3])
{
  if (data->mval) {
    return key_inside_circle(data, data->rad, co, nullptr);
  }
  return key_inside_rect(data, co);
}

static bool point_is_selected(PTCacheEditPoint *point)
{
  KEY_K;

  if (point->flag & PEP_HIDE) {
    return false;
  }

  LOOP_SELECTED_KEYS {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterators
 * \{ */

using ForPointFunc = void (*)(PEData *data, int point_index);
using ForHitPointFunc = void (*)(PEData *data, int point_index, float mouse_distance);

using ForKeyFunc = void (*)(PEData *data, int point_index, int key_index, bool is_inside);

using ForKeyMatFunc = void (*)(PEData *data,
                               const float mat[4][4],
                               const float imat[4][4],
                               int point_index,
                               int key_index,
                               PTCacheEditKey *key);
using ForHitKeyMatFunc = void (*)(PEData *data,
                                  float mat[4][4],
                                  float imat[4][4],
                                  int point_index,
                                  int key_index,
                                  PTCacheEditKey *key,
                                  float mouse_distance);

enum eParticleSelectFlag {
  PSEL_NEAREST = (1 << 0),
  PSEL_ALL_KEYS = (1 << 1),
};

static void for_mouse_hit_keys(PEData *data, ForKeyFunc func, const enum eParticleSelectFlag flag)
{
  ParticleEditSettings *pset = PE_settings(data->scene);
  PTCacheEdit *edit = data->edit;
  POINT_P;
  KEY_K;
  int nearest_point, nearest_key;
  float dist = data->rad;

  /* in path select mode we have no keys */
  if (pset->selectmode == SCE_SELECT_PATH) {
    return;
  }

  nearest_point = -1;
  nearest_key = -1;

  LOOP_VISIBLE_POINTS {
    if (pset->selectmode == SCE_SELECT_END) {
      if (point->totkey) {
        /* only do end keys */
        key = point->keys + point->totkey - 1;

        if (flag & PSEL_NEAREST) {
          if (key_inside_circle(data, dist, KEY_WCO, &dist)) {
            nearest_point = p;
            nearest_key = point->totkey - 1;
          }
        }
        else {
          const bool is_inside = key_inside_test(data, KEY_WCO);
          if (is_inside || (flag & PSEL_ALL_KEYS)) {
            func(data, p, point->totkey - 1, is_inside);
          }
        }
      }
    }
    else {
      /* do all keys */
      LOOP_VISIBLE_KEYS {
        if (flag & PSEL_NEAREST) {
          if (key_inside_circle(data, dist, KEY_WCO, &dist)) {
            nearest_point = p;
            nearest_key = k;
          }
        }
        else {
          const bool is_inside = key_inside_test(data, KEY_WCO);
          if (is_inside || (flag & PSEL_ALL_KEYS)) {
            func(data, p, k, is_inside);
          }
        }
      }
    }
  }

  /* do nearest only */
  if (flag & PSEL_NEAREST) {
    if (nearest_point != -1) {
      func(data, nearest_point, nearest_key, true);
    }
  }
}

static void foreach_mouse_hit_point(PEData *data, ForHitPointFunc func, int selected)
{
  ParticleEditSettings *pset = PE_settings(data->scene);
  PTCacheEdit *edit = data->edit;
  POINT_P;
  KEY_K;

  /* all is selected in path mode */
  if (pset->selectmode == SCE_SELECT_PATH) {
    selected = 0;
  }

  LOOP_VISIBLE_POINTS {
    if (pset->selectmode == SCE_SELECT_END) {
      if (point->totkey) {
        /* only do end keys */
        key = point->keys + point->totkey - 1;

        if (selected == 0 || key->flag & PEK_SELECT) {
          float mouse_distance;
          if (key_inside_circle(data, data->rad, KEY_WCO, &mouse_distance)) {
            func(data, p, mouse_distance);
          }
        }
      }
    }
    else {
      /* do all keys */
      LOOP_VISIBLE_KEYS {
        if (selected == 0 || key->flag & PEK_SELECT) {
          float mouse_distance;
          if (key_inside_circle(data, data->rad, KEY_WCO, &mouse_distance)) {
            func(data, p, mouse_distance);
            break;
          }
        }
      }
    }
  }
}

struct KeyIterData {
  PEData *data;
  PTCacheEdit *edit;
  int selected;
  ForHitKeyMatFunc func;
};

static void foreach_mouse_hit_key_iter(void *__restrict iter_data_v,
                                       const int iter,
                                       const TaskParallelTLS *__restrict /*tls*/)
{
  KeyIterData *iter_data = (KeyIterData *)iter_data_v;
  PEData *data = iter_data->data;
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = &edit->points[iter];
  if (point->flag & PEP_HIDE) {
    return;
  }
  ParticleSystem *psys = edit->psys;
  ParticleSystemModifierData *psmd_eval = iter_data->edit->psmd_eval;
  ParticleEditSettings *pset = PE_settings(data->scene);
  const int selected = iter_data->selected;
  float mat[4][4], imat[4][4];
  unit_m4(mat);
  unit_m4(imat);
  if (pset->selectmode == SCE_SELECT_END) {
    if (point->totkey) {
      /* only do end keys */
      PTCacheEditKey *key = point->keys + point->totkey - 1;

      if (selected == 0 || key->flag & PEK_SELECT) {
        float mouse_distance;
        if (key_inside_circle(data, data->rad, KEY_WCO, &mouse_distance)) {
          if (edit->psys && !(edit->psys->flag & PSYS_GLOBAL_HAIR)) {
            psys_mat_hair_to_global(
                data->ob, psmd_eval->mesh_final, psys->part->from, psys->particles + iter, mat);
            invert_m4_m4(imat, mat);
          }
          iter_data->func(data, mat, imat, iter, point->totkey - 1, key, mouse_distance);
        }
      }
    }
  }
  else {
    /* do all keys */
    PTCacheEditKey *key;
    int k;
    LOOP_VISIBLE_KEYS {
      if (selected == 0 || key->flag & PEK_SELECT) {
        float mouse_distance;
        if (key_inside_circle(data, data->rad, KEY_WCO, &mouse_distance)) {
          if (edit->psys && !(edit->psys->flag & PSYS_GLOBAL_HAIR)) {
            psys_mat_hair_to_global(
                data->ob, psmd_eval->mesh_final, psys->part->from, psys->particles + iter, mat);
            invert_m4_m4(imat, mat);
          }
          iter_data->func(data, mat, imat, iter, k, key, mouse_distance);
        }
      }
    }
  }
}

static void foreach_mouse_hit_key(PEData *data, ForHitKeyMatFunc func, int selected)
{
  PTCacheEdit *edit = data->edit;
  ParticleEditSettings *pset = PE_settings(data->scene);
  /* all is selected in path mode */
  if (pset->selectmode == SCE_SELECT_PATH) {
    selected = 0;
  }

  KeyIterData iter_data;
  iter_data.data = data;
  iter_data.edit = edit;
  iter_data.selected = selected;
  iter_data.func = func;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, edit->totpoint, &iter_data, foreach_mouse_hit_key_iter, &settings);
}

static void foreach_selected_point(PEData *data, ForPointFunc func)
{
  PTCacheEdit *edit = data->edit;
  POINT_P;

  LOOP_SELECTED_POINTS {
    func(data, p);
  }
}

static void foreach_selected_key(PEData *data, ForKeyFunc func)
{
  PTCacheEdit *edit = data->edit;
  POINT_P;
  KEY_K;

  LOOP_VISIBLE_POINTS {
    LOOP_SELECTED_KEYS {
      func(data, p, k, true);
    }
  }
}

static void foreach_point(PEData *data, ForPointFunc func)
{
  PTCacheEdit *edit = data->edit;
  POINT_P;

  LOOP_POINTS {
    func(data, p);
  }
}

static int count_selected_keys(Scene *scene, PTCacheEdit *edit)
{
  ParticleEditSettings *pset = PE_settings(scene);
  POINT_P;
  KEY_K;
  int sel = 0;

  LOOP_VISIBLE_POINTS {
    if (pset->selectmode == SCE_SELECT_POINT) {
      LOOP_SELECTED_KEYS {
        sel++;
      }
    }
    else if (pset->selectmode == SCE_SELECT_END) {
      if (point->totkey) {
        key = point->keys + point->totkey - 1;
        if (key->flag & PEK_SELECT) {
          sel++;
        }
      }
    }
  }

  return sel;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Edit Mirroring
 * \{ */

static void PE_update_mirror_cache(Object *ob, ParticleSystem *psys)
{
  PTCacheEdit *edit;
  ParticleSystemModifierData *psmd_eval;
  KDTree_3d *tree;
  KDTreeNearest_3d nearest;
  HairKey *key;
  PARTICLE_P;
  float mat[4][4], co[3];
  int index, totpart;

  edit = psys->edit;
  psmd_eval = edit->psmd_eval;
  totpart = psys->totpart;

  if (!psmd_eval->mesh_final) {
    return;
  }

  tree = BLI_kdtree_3d_new(totpart);

  /* Insert particles into KD-tree. */
  LOOP_PARTICLES
  {
    key = pa->hair;
    psys_mat_hair_to_orco(ob, psmd_eval->mesh_final, psys->part->from, pa, mat);
    copy_v3_v3(co, key->co);
    mul_m4_v3(mat, co);
    BLI_kdtree_3d_insert(tree, p, co);
  }

  BLI_kdtree_3d_balance(tree);

  /* lookup particles and set in mirror cache */
  if (!edit->mirror_cache) {
    edit->mirror_cache = static_cast<int *>(MEM_callocN(sizeof(int) * totpart, "PE mirror cache"));
  }

  LOOP_PARTICLES
  {
    key = pa->hair;
    psys_mat_hair_to_orco(ob, psmd_eval->mesh_final, psys->part->from, pa, mat);
    copy_v3_v3(co, key->co);
    mul_m4_v3(mat, co);
    co[0] = -co[0];

    index = BLI_kdtree_3d_find_nearest(tree, co, &nearest);

    /* this needs a custom threshold still, duplicated for editmode mirror */
    if (index != -1 && index != p && (nearest.dist <= 0.0002f)) {
      edit->mirror_cache[p] = index;
    }
    else {
      edit->mirror_cache[p] = -1;
    }
  }

  /* make sure mirrors are in two directions */
  LOOP_PARTICLES
  {
    if (edit->mirror_cache[p]) {
      index = edit->mirror_cache[p];
      if (edit->mirror_cache[index] != p) {
        edit->mirror_cache[p] = -1;
      }
    }
  }

  BLI_kdtree_3d_free(tree);
}

static void PE_mirror_particle(
    Object *ob, Mesh *mesh, ParticleSystem *psys, ParticleData *pa, ParticleData *mpa)
{
  HairKey *hkey, *mhkey;
  PTCacheEditPoint *point, *mpoint;
  PTCacheEditKey *key, *mkey;
  PTCacheEdit *edit;
  float mat[4][4], mmat[4][4], immat[4][4];
  int i, mi, k;

  edit = psys->edit;
  i = pa - psys->particles;

  /* find mirrored particle if needed */
  if (!mpa) {
    if (!edit->mirror_cache) {
      PE_update_mirror_cache(ob, psys);
    }

    if (!edit->mirror_cache) {
      return; /* something went wrong! */
    }

    mi = edit->mirror_cache[i];
    if (mi == -1) {
      return;
    }
    mpa = psys->particles + mi;
  }
  else {
    mi = mpa - psys->particles;
  }

  point = edit->points + i;
  mpoint = edit->points + mi;

  /* make sure they have the same amount of keys */
  if (pa->totkey != mpa->totkey) {
    if (mpa->hair) {
      MEM_freeN(mpa->hair);
    }
    if (mpoint->keys) {
      MEM_freeN(mpoint->keys);
    }

    mpa->hair = static_cast<HairKey *>(MEM_dupallocN(pa->hair));
    mpa->totkey = pa->totkey;
    mpoint->keys = static_cast<PTCacheEditKey *>(MEM_dupallocN(point->keys));
    mpoint->totkey = point->totkey;

    mhkey = mpa->hair;
    mkey = mpoint->keys;
    for (k = 0; k < mpa->totkey; k++, mkey++, mhkey++) {
      mkey->co = mhkey->co;
      mkey->time = &mhkey->time;
      mkey->flag &= ~PEK_SELECT;
    }
  }

  /* mirror positions and tags */
  psys_mat_hair_to_orco(ob, mesh, psys->part->from, pa, mat);
  psys_mat_hair_to_orco(ob, mesh, psys->part->from, mpa, mmat);
  invert_m4_m4(immat, mmat);

  hkey = pa->hair;
  mhkey = mpa->hair;
  key = point->keys;
  mkey = mpoint->keys;
  for (k = 0; k < pa->totkey; k++, hkey++, mhkey++, key++, mkey++) {
    copy_v3_v3(mhkey->co, hkey->co);
    mul_m4_v3(mat, mhkey->co);
    mhkey->co[0] = -mhkey->co[0];
    mul_m4_v3(immat, mhkey->co);

    if (key->flag & PEK_TAG) {
      mkey->flag |= PEK_TAG;
    }

    mkey->length = key->length;
  }

  if (point->flag & PEP_TAG) {
    mpoint->flag |= PEP_TAG;
  }
  if (point->flag & PEP_EDIT_RECALC) {
    mpoint->flag |= PEP_EDIT_RECALC;
  }
}

static void PE_apply_mirror(Object *ob, ParticleSystem *psys)
{
  PTCacheEdit *edit;
  ParticleSystemModifierData *psmd_eval;
  POINT_P;

  if (!psys) {
    return;
  }

  edit = psys->edit;
  psmd_eval = edit->psmd_eval;

  if (psmd_eval == nullptr || psmd_eval->mesh_final == nullptr) {
    return;
  }

  if (!edit->mirror_cache) {
    PE_update_mirror_cache(ob, psys);
  }

  if (!edit->mirror_cache) {
    return; /* something went wrong */
  }

  /* we delay settings the PARS_EDIT_RECALC for mirrored particles
   * to avoid doing mirror twice */
  LOOP_POINTS {
    if (point->flag & PEP_EDIT_RECALC) {
      PE_mirror_particle(ob, psmd_eval->mesh_final, psys, psys->particles + p, nullptr);

      if (edit->mirror_cache[p] != -1) {
        edit->points[edit->mirror_cache[p]].flag &= ~PEP_EDIT_RECALC;
      }
    }
  }

  LOOP_POINTS {
    if (point->flag & PEP_EDIT_RECALC) {
      if (edit->mirror_cache[p] != -1) {
        edit->points[edit->mirror_cache[p]].flag |= PEP_EDIT_RECALC;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Calculation
 * \{ */

struct DeflectEmitterIter {
  Object *object;
  ParticleSystem *psys;
  PTCacheEdit *edit;
  float dist;
  float emitterdist;
};

static void deflect_emitter_iter(void *__restrict iter_data_v,
                                 const int iter,
                                 const TaskParallelTLS *__restrict /*tls*/)
{
  DeflectEmitterIter *iter_data = (DeflectEmitterIter *)iter_data_v;
  PTCacheEdit *edit = iter_data->edit;
  PTCacheEditPoint *point = &edit->points[iter];
  if ((point->flag & PEP_EDIT_RECALC) == 0) {
    return;
  }
  Object *object = iter_data->object;
  ParticleSystem *psys = iter_data->psys;
  ParticleSystemModifierData *psmd_eval = iter_data->edit->psmd_eval;
  PTCacheEditKey *key;
  int k;
  float hairimat[4][4], hairmat[4][4];
  int index;
  float *vec, *nor, dvec[3], dot, dist_1st = 0.0f;
  const float dist = iter_data->dist;
  const float emitterdist = iter_data->emitterdist;
  psys_mat_hair_to_object(
      object, psmd_eval->mesh_final, psys->part->from, psys->particles + iter, hairmat);

  LOOP_KEYS {
    mul_m4_v3(hairmat, key->co);
  }

  LOOP_KEYS {
    if (k == 0) {
      dist_1st = len_v3v3((key + 1)->co, key->co);
      dist_1st *= dist * emitterdist;
    }
    else {
      index = BLI_kdtree_3d_find_nearest(edit->emitter_field, key->co, nullptr);

      vec = edit->emitter_cosnos + index * 6;
      nor = vec + 3;

      sub_v3_v3v3(dvec, key->co, vec);

      dot = dot_v3v3(dvec, nor);
      copy_v3_v3(dvec, nor);

      if (dot > 0.0f) {
        if (dot < dist_1st) {
          normalize_v3(dvec);
          mul_v3_fl(dvec, dist_1st - dot);
          add_v3_v3(key->co, dvec);
        }
      }
      else {
        normalize_v3(dvec);
        mul_v3_fl(dvec, dist_1st - dot);
        add_v3_v3(key->co, dvec);
      }
      if (k == 1) {
        dist_1st *= 1.3333f;
      }
    }
  }

  invert_m4_m4(hairimat, hairmat);

  LOOP_KEYS {
    mul_m4_v3(hairimat, key->co);
  }
}

/* tries to stop edited particles from going through the emitter's surface */
static void pe_deflect_emitter(Scene *scene, Object *ob, PTCacheEdit *edit)
{
  ParticleEditSettings *pset = PE_settings(scene);
  ParticleSystem *psys;
  const float dist = ED_view3d_select_dist_px() * 0.01f;

  if (edit == nullptr || edit->psys == nullptr || (pset->flag & PE_DEFLECT_EMITTER) == 0 ||
      (edit->psys->flag & PSYS_GLOBAL_HAIR))
  {
    return;
  }

  psys = edit->psys;

  if (edit->psmd_eval == nullptr || edit->psmd_eval->mesh_final == nullptr) {
    return;
  }

  DeflectEmitterIter iter_data;
  iter_data.object = ob;
  iter_data.psys = psys;
  iter_data.edit = edit;
  iter_data.dist = dist;
  iter_data.emitterdist = pset->emitterdist;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, edit->totpoint, &iter_data, deflect_emitter_iter, &settings);
}

struct ApplyLengthsIterData {
  PTCacheEdit *edit;
};

static void apply_lengths_iter(void *__restrict iter_data_v,
                               const int iter,
                               const TaskParallelTLS *__restrict /*tls*/)
{
  ApplyLengthsIterData *iter_data = (ApplyLengthsIterData *)iter_data_v;
  PTCacheEdit *edit = iter_data->edit;
  PTCacheEditPoint *point = &edit->points[iter];
  if ((point->flag & PEP_EDIT_RECALC) == 0) {
    return;
  }
  PTCacheEditKey *key;
  int k;
  LOOP_KEYS {
    if (k) {
      float dv1[3];
      sub_v3_v3v3(dv1, key->co, (key - 1)->co);
      normalize_v3(dv1);
      mul_v3_fl(dv1, (key - 1)->length);
      add_v3_v3v3(key->co, (key - 1)->co, dv1);
    }
  }
}

/* force set distances between neighboring keys */
static void PE_apply_lengths(Scene *scene, PTCacheEdit *edit)
{
  ParticleEditSettings *pset = PE_settings(scene);

  if (edit == nullptr || (pset->flag & PE_KEEP_LENGTHS) == 0) {
    return;
  }

  if (edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR) {
    return;
  }

  ApplyLengthsIterData iter_data;
  iter_data.edit = edit;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, edit->totpoint, &iter_data, apply_lengths_iter, &settings);
}

struct IterateLengthsIterData {
  PTCacheEdit *edit;
  ParticleEditSettings *pset;
};

static void iterate_lengths_iter(void *__restrict iter_data_v,
                                 const int iter,
                                 const TaskParallelTLS *__restrict /*tls*/)
{
  IterateLengthsIterData *iter_data = (IterateLengthsIterData *)iter_data_v;
  PTCacheEdit *edit = iter_data->edit;
  PTCacheEditPoint *point = &edit->points[iter];
  if ((point->flag & PEP_EDIT_RECALC) == 0) {
    return;
  }
  ParticleEditSettings *pset = iter_data->pset;
  float tlen;
  float dv0[3] = {0.0f, 0.0f, 0.0f};
  float dv1[3] = {0.0f, 0.0f, 0.0f};
  float dv2[3] = {0.0f, 0.0f, 0.0f};
  for (int j = 1; j < point->totkey; j++) {
    PTCacheEditKey *key;
    int k;
    float mul = 1.0f / float(point->totkey);
    if (pset->flag & PE_LOCK_FIRST) {
      key = point->keys + 1;
      k = 1;
      dv1[0] = dv1[1] = dv1[2] = 0.0;
    }
    else {
      key = point->keys;
      k = 0;
      dv0[0] = dv0[1] = dv0[2] = 0.0;
    }

    for (; k < point->totkey; k++, key++) {
      if (k) {
        sub_v3_v3v3(dv0, (key - 1)->co, key->co);
        tlen = normalize_v3(dv0);
        mul_v3_fl(dv0, (mul * (tlen - (key - 1)->length)));
      }
      if (k < point->totkey - 1) {
        sub_v3_v3v3(dv2, (key + 1)->co, key->co);
        tlen = normalize_v3(dv2);
        mul_v3_fl(dv2, mul * (tlen - key->length));
      }
      if (k) {
        add_v3_v3((key - 1)->co, dv1);
      }
      add_v3_v3v3(dv1, dv0, dv2);
    }
  }
}

/* try to find a nice solution to keep distances between neighboring keys */
static void pe_iterate_lengths(Scene *scene, PTCacheEdit *edit)
{
  ParticleEditSettings *pset = PE_settings(scene);
  if (edit == nullptr || (pset->flag & PE_KEEP_LENGTHS) == 0) {
    return;
  }
  if (edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR) {
    return;
  }

  IterateLengthsIterData iter_data;
  iter_data.edit = edit;
  iter_data.pset = pset;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, edit->totpoint, &iter_data, iterate_lengths_iter, &settings);
}

void recalc_lengths(PTCacheEdit *edit)
{
  POINT_P;
  KEY_K;

  if (edit == nullptr) {
    return;
  }

  LOOP_EDITED_POINTS {
    key = point->keys;
    for (k = 0; k < point->totkey - 1; k++, key++) {
      key->length = len_v3v3(key->co, (key + 1)->co);
    }
  }
}

void recalc_emitter_field(Depsgraph * /*depsgraph*/, Object * /*ob*/, ParticleSystem *psys)
{
  PTCacheEdit *edit = psys->edit;
  Mesh *mesh = edit->psmd_eval->mesh_final;
  float *vec, *nor;
  int i, totface;

  if (!mesh) {
    return;
  }

  if (edit->emitter_cosnos) {
    MEM_freeN(edit->emitter_cosnos);
  }

  BLI_kdtree_3d_free(edit->emitter_field);

  totface = mesh->totface_legacy;
  // int totvert = dm->getNumVerts(dm); /* UNUSED */

  edit->emitter_cosnos = static_cast<float *>(
      MEM_callocN(sizeof(float[6]) * totface, "emitter cosnos"));

  edit->emitter_field = BLI_kdtree_3d_new(totface);

  vec = edit->emitter_cosnos;
  nor = vec + 3;

  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::Span<blender::float3> vert_normals = mesh->vert_normals();
  const MFace *mfaces = (const MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE);
  for (i = 0; i < totface; i++, vec += 6, nor += 6) {
    const MFace *mface = &mfaces[i];

    copy_v3_v3(vec, positions[mface->v1]);
    copy_v3_v3(nor, vert_normals[mface->v1]);

    add_v3_v3v3(vec, vec, positions[mface->v2]);
    add_v3_v3(nor, vert_normals[mface->v2]);

    add_v3_v3v3(vec, vec, positions[mface->v3]);
    add_v3_v3(nor, vert_normals[mface->v3]);

    if (mface->v4) {
      add_v3_v3v3(vec, vec, positions[mface->v4]);
      add_v3_v3(nor, vert_normals[mface->v4]);

      mul_v3_fl(vec, 0.25);
    }
    else {
      mul_v3_fl(vec, 1.0f / 3.0f);
    }

    normalize_v3(nor);

    BLI_kdtree_3d_insert(edit->emitter_field, i, vec);
  }

  BLI_kdtree_3d_balance(edit->emitter_field);
}

static void PE_update_selection(Depsgraph *depsgraph, Scene *scene, Object *ob, int useflag)
{
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  HairKey *hkey;
  POINT_P;
  KEY_K;

  /* flag all particles to be updated if not using flag */
  if (!useflag) {
    LOOP_POINTS {
      point->flag |= PEP_EDIT_RECALC;
    }
  }

  /* flush edit key flag to hair key flag to preserve selection
   * on save */
  if (edit->psys) {
    LOOP_POINTS {
      hkey = edit->psys->particles[p].hair;
      LOOP_KEYS {
        hkey->editflag = key->flag;
        hkey++;
      }
    }
  }

  psys_cache_edit_paths(depsgraph, scene, ob, edit, scene->r.cfra, G.is_rendering);

  /* disable update flag */
  LOOP_POINTS {
    point->flag &= ~PEP_EDIT_RECALC;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SELECT);
}

void update_world_cos(Object *ob, PTCacheEdit *edit)
{
  ParticleSystem *psys = edit->psys;
  ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
  POINT_P;
  KEY_K;
  float hairmat[4][4];

  if (psys == nullptr || psys->edit == nullptr || psmd_eval == nullptr ||
      psmd_eval->mesh_final == nullptr)
  {
    return;
  }

  LOOP_POINTS {
    if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
      psys_mat_hair_to_global(
          ob, psmd_eval->mesh_final, psys->part->from, psys->particles + p, hairmat);
    }

    LOOP_KEYS {
      copy_v3_v3(key->world_co, key->co);
      if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
        mul_m4_v3(hairmat, key->world_co);
      }
    }
  }
}
static void update_velocities(PTCacheEdit *edit)
{
  /* TODO: get frs_sec properly. */
  float vec1[3], vec2[3], frs_sec, dfra;
  POINT_P;
  KEY_K;

  /* hair doesn't use velocities */
  if (edit->psys || !edit->points || !edit->points->keys->vel) {
    return;
  }

  frs_sec = edit->pid.flag & PTCACHE_VEL_PER_SEC ? 25.0f : 1.0f;

  LOOP_EDITED_POINTS {
    LOOP_KEYS {
      if (k == 0) {
        dfra = *(key + 1)->time - *key->time;

        if (dfra <= 0.0f) {
          continue;
        }

        sub_v3_v3v3(key->vel, (key + 1)->co, key->co);

        if (point->totkey > 2) {
          sub_v3_v3v3(vec1, (key + 1)->co, (key + 2)->co);
          project_v3_v3v3(vec2, vec1, key->vel);
          sub_v3_v3v3(vec2, vec1, vec2);
          madd_v3_v3fl(key->vel, vec2, 0.5f);
        }
      }
      else if (k == point->totkey - 1) {
        dfra = *key->time - *(key - 1)->time;

        if (dfra <= 0.0f) {
          continue;
        }

        sub_v3_v3v3(key->vel, key->co, (key - 1)->co);

        if (point->totkey > 2) {
          sub_v3_v3v3(vec1, (key - 2)->co, (key - 1)->co);
          project_v3_v3v3(vec2, vec1, key->vel);
          sub_v3_v3v3(vec2, vec1, vec2);
          madd_v3_v3fl(key->vel, vec2, 0.5f);
        }
      }
      else {
        dfra = *(key + 1)->time - *(key - 1)->time;

        if (dfra <= 0.0f) {
          continue;
        }

        sub_v3_v3v3(key->vel, (key + 1)->co, (key - 1)->co);
      }
      mul_v3_fl(key->vel, frs_sec / dfra);
    }
  }
}

void PE_update_object(Depsgraph *depsgraph, Scene *scene, Object *ob, int useflag)
{
  /* use this to do partial particle updates, not usable when adding or
   * removing, then a full redo is necessary and calling this may crash */
  ParticleEditSettings *pset = PE_settings(scene);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  POINT_P;

  if (!edit) {
    return;
  }

  /* flag all particles to be updated if not using flag */
  if (!useflag) {
    LOOP_POINTS {
      point->flag |= PEP_EDIT_RECALC;
    }
  }

  /* do post process on particle edit keys */
  pe_iterate_lengths(scene, edit);
  pe_deflect_emitter(scene, ob, edit);
  PE_apply_lengths(scene, edit);
  if (pe_x_mirror(ob)) {
    PE_apply_mirror(ob, edit->psys);
  }
  if (edit->psys) {
    update_world_cos(ob, edit);
  }
  if (pset->flag & PE_AUTO_VELOCITY) {
    update_velocities(edit);
  }

  /* Only do this for emitter particles because drawing PE_FADE_TIME is not respected in 2.8 yet
   * and flagging with PEK_HIDE will prevent selection. This might get restored once this is
   * supported in drawing (but doesn't make much sense for hair anyways). */
  if (edit->psys && edit->psys->part->type == PART_EMITTER) {
    PE_hide_keys_time(scene, edit, scene->r.cfra);
  }

  /* regenerate path caches */
  psys_cache_edit_paths(depsgraph, scene, ob, edit, scene->r.cfra, G.is_rendering);

  /* disable update flag */
  LOOP_POINTS {
    point->flag &= ~PEP_EDIT_RECALC;
  }

  if (edit->psys) {
    edit->psys->flag &= ~PSYS_HAIR_UPDATED;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Selections
 * \{ */

/*-----selection callbacks-----*/

static void select_key(PEData *data, int point_index, int key_index, bool /*is_inside*/)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  PTCacheEditKey *key = point->keys + key_index;

  if (data->select) {
    key->flag |= PEK_SELECT;
  }
  else {
    key->flag &= ~PEK_SELECT;
  }

  point->flag |= PEP_EDIT_RECALC;
  data->is_changed = true;
}

static void select_key_op(PEData *data, int point_index, int key_index, bool is_inside)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  PTCacheEditKey *key = point->keys + key_index;
  const bool is_select = key->flag & PEK_SELECT;
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    SET_FLAG_FROM_TEST(key->flag, sel_op_result, PEK_SELECT);
    point->flag |= PEP_EDIT_RECALC;
    data->is_changed = true;
  }
}

static void select_keys(PEData *data, int point_index, int /*key_index*/, bool /*is_inside*/)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  KEY_K;

  LOOP_KEYS {
    if (data->select) {
      key->flag |= PEK_SELECT;
    }
    else {
      key->flag &= ~PEK_SELECT;
    }
  }

  point->flag |= PEP_EDIT_RECALC;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name De-Select All Operator
 * \{ */

static bool select_action_apply(PTCacheEditPoint *point, PTCacheEditKey *key, int action)
{
  bool changed = false;
  switch (action) {
    case SEL_SELECT:
      if ((key->flag & PEK_SELECT) == 0) {
        key->flag |= PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
      }
      break;
    case SEL_DESELECT:
      if (key->flag & PEK_SELECT) {
        key->flag &= ~PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
      }
      break;
    case SEL_INVERT:
      if ((key->flag & PEK_SELECT) == 0) {
        key->flag |= PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
      }
      else {
        key->flag &= ~PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
      }
      break;
  }
  return changed;
}

static int pe_select_all_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  POINT_P;
  KEY_K;
  int action = RNA_enum_get(op->ptr, "action");

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    LOOP_VISIBLE_POINTS {
      LOOP_SELECTED_KEYS {
        action = SEL_DESELECT;
        break;
      }

      if (action == SEL_DESELECT) {
        break;
      }
    }
  }

  bool changed = false;
  LOOP_VISIBLE_POINTS {
    LOOP_VISIBLE_KEYS {
      changed |= select_action_apply(point, key, action);
    }
  }

  if (changed) {
    PE_update_selection(depsgraph, scene, ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);
  }
  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->idname = "PARTICLE_OT_select_all";
  ot->description = "(De)select all particles' keys";

  /* api callbacks */
  ot->exec = pe_select_all_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pick Select Operator
 * \{ */

struct NearestParticleData {
  PTCacheEditPoint *point;
  PTCacheEditKey *key;
};

static void nearest_key_fn(PEData *data, int point_index, int key_index, bool /*is_inside*/)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  PTCacheEditKey *key = point->keys + key_index;

  NearestParticleData *user_data = static_cast<NearestParticleData *>(data->user_data);
  user_data->point = point;
  user_data->key = key;
  data->is_changed = true;
}

static bool pe_nearest_point_and_key(bContext *C,
                                     const int mval[2],
                                     PTCacheEditPoint **r_point,
                                     PTCacheEditKey **r_key)
{
  NearestParticleData user_data = {nullptr};

  PEData data;
  PE_set_view3d_data(C, &data);
  data.mval = mval;
  data.rad = ED_view3d_select_dist_px();

  data.user_data = &user_data;
  for_mouse_hit_keys(&data, nearest_key_fn, PSEL_NEAREST);
  bool found = data.is_changed;
  PE_data_free(&data);

  *r_point = user_data.point;
  *r_key = user_data.key;
  return found;
}

bool PE_mouse_particles(bContext *C, const int mval[2], const SelectPick_Params *params)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  if (!PE_start_edit(edit)) {
    return false;
  }

  PTCacheEditPoint *point;
  PTCacheEditKey *key;

  bool changed = false;
  bool found = pe_nearest_point_and_key(C, mval, &point, &key);

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && (key->flag & PEK_SELECT)) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      changed |= PE_deselect_all_visible_ex(edit);
    }
  }

  if (found) {
    switch (params->sel_op) {
      case SEL_OP_ADD: {
        if ((key->flag & PEK_SELECT) == 0) {
          key->flag |= PEK_SELECT;
          point->flag |= PEP_EDIT_RECALC;
          changed = true;
        }
        break;
      }
      case SEL_OP_SUB: {
        if ((key->flag & PEK_SELECT) != 0) {
          key->flag &= ~PEK_SELECT;
          point->flag |= PEP_EDIT_RECALC;
          changed = true;
        }
        break;
      }
      case SEL_OP_XOR: {
        key->flag ^= PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
        break;
      }
      case SEL_OP_SET: {
        if ((key->flag & PEK_SELECT) == 0) {
          key->flag |= PEK_SELECT;
          point->flag |= PEP_EDIT_RECALC;
          changed = true;
        }
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }
  }

  if (changed) {
    PE_update_selection(depsgraph, scene, ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);
  }

  return changed || found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Root Operator
 * \{ */

static void select_root(PEData *data, int point_index)
{
  PTCacheEditPoint *point = data->edit->points + point_index;
  PTCacheEditKey *key = point->keys;

  if (point->flag & PEP_HIDE) {
    return;
  }

  if (data->select_action != SEL_TOGGLE) {
    data->is_changed = select_action_apply(point, key, data->select_action);
  }
  else if (key->flag & PEK_SELECT) {
    data->select_toggle_action = SEL_DESELECT;
  }
}

static int select_roots_exec(bContext *C, wmOperator *op)
{
  PEData data;
  int action = RNA_enum_get(op->ptr, "action");

  PE_set_data(C, &data);

  if (action == SEL_TOGGLE) {
    data.select_action = SEL_TOGGLE;
    data.select_toggle_action = SEL_SELECT;

    foreach_point(&data, select_root);

    action = data.select_toggle_action;
  }

  data.select_action = action;
  foreach_point(&data, select_root);

  if (data.is_changed) {
    PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);
  }
  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_roots(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Roots";
  ot->idname = "PARTICLE_OT_select_roots";
  ot->description = "Select roots of all visible particles";

  /* api callbacks */
  ot->exec = select_roots_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_action(ot, SEL_SELECT, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Tip Operator
 * \{ */

static void select_tip(PEData *data, int point_index)
{
  PTCacheEditPoint *point = data->edit->points + point_index;
  PTCacheEditKey *key;

  if (point->totkey == 0) {
    return;
  }

  key = &point->keys[point->totkey - 1];

  if (point->flag & PEP_HIDE) {
    return;
  }

  if (data->select_action != SEL_TOGGLE) {
    data->is_changed = select_action_apply(point, key, data->select_action);
  }
  else if (key->flag & PEK_SELECT) {
    data->select_toggle_action = SEL_DESELECT;
  }
}

static int select_tips_exec(bContext *C, wmOperator *op)
{
  PEData data;
  int action = RNA_enum_get(op->ptr, "action");

  PE_set_data(C, &data);

  if (action == SEL_TOGGLE) {
    data.select_action = SEL_TOGGLE;
    data.select_toggle_action = SEL_SELECT;

    foreach_point(&data, select_tip);

    action = data.select_toggle_action;
  }

  data.select_action = action;
  foreach_point(&data, select_tip);

  if (data.is_changed) {
    PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PARTICLE_OT_select_tips(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Tips";
  ot->idname = "PARTICLE_OT_select_tips";
  ot->description = "Select tips of all visible particles";

  /* api callbacks */
  ot->exec = select_tips_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_action(ot, SEL_SELECT, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Random Operator
 * \{ */

enum { RAN_HAIR, RAN_POINTS };

static const EnumPropertyItem select_random_type_items[] = {
    {RAN_HAIR, "HAIR", 0, "Hair", ""},
    {RAN_POINTS, "POINTS", 0, "Points", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int select_random_exec(bContext *C, wmOperator *op)
{
  PEData data;
  int type;

  /* used by LOOP_VISIBLE_POINTS, LOOP_VISIBLE_KEYS and LOOP_KEYS */
  PTCacheEdit *edit;
  PTCacheEditPoint *point;
  PTCacheEditKey *key;
  int p;
  int k;

  const float randfac = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  RNG *rng;

  type = RNA_enum_get(op->ptr, "type");

  PE_set_data(C, &data);
  data.select_action = SEL_SELECT;
  edit = PE_get_current(data.depsgraph, data.scene, data.ob);

  rng = BLI_rng_new_srandom(seed);

  switch (type) {
    case RAN_HAIR:
      LOOP_VISIBLE_POINTS {
        int flag = ((BLI_rng_get_float(rng) < randfac) == select) ? SEL_SELECT : SEL_DESELECT;
        LOOP_KEYS {
          data.is_changed |= select_action_apply(point, key, flag);
        }
      }
      break;
    case RAN_POINTS:
      LOOP_VISIBLE_POINTS {
        LOOP_VISIBLE_KEYS {
          int flag = ((BLI_rng_get_float(rng) < randfac) == select) ? SEL_SELECT : SEL_DESELECT;
          data.is_changed |= select_action_apply(point, key, flag);
        }
      }
      break;
  }

  BLI_rng_free(rng);

  if (data.is_changed) {
    PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);
  }
  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_random(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Random";
  ot->idname = "PARTICLE_OT_select_random";
  ot->description = "Select a randomly distributed set of hair or points";

  /* api callbacks */
  ot->exec = select_random_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_random(ot);
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          select_random_type_items,
                          RAN_HAIR,
                          "Type",
                          "Select either hair or points");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked operator
 * \{ */

static int select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  PEData data;
  PE_set_data(C, &data);
  data.select = true;

  foreach_selected_key(&data, select_keys);

  PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked All";
  ot->idname = "PARTICLE_OT_select_linked";
  ot->description = "Select all keys linked to already selected ones";

  /* api callbacks */
  ot->exec = select_linked_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
}

static int select_linked_pick_exec(bContext *C, wmOperator *op)
{
  PEData data;
  int mval[2];
  int location[2];

  RNA_int_get_array(op->ptr, "location", location);
  mval[0] = location[0];
  mval[1] = location[1];

  PE_set_view3d_data(C, &data);
  data.mval = mval;
  data.rad = 75.0f;
  data.select = !RNA_boolean_get(op->ptr, "deselect");

  for_mouse_hit_keys(&data, select_keys, PSEL_NEAREST);
  PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);
  PE_data_free(&data);

  return OPERATOR_FINISHED;
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);
  return select_linked_pick_exec(C, op);
}

void PARTICLE_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "PARTICLE_OT_select_linked_pick";
  ot->description = "Select nearest particle from mouse pointer";

  /* api callbacks */
  ot->exec = select_linked_pick_exec;
  ot->invoke = select_linked_pick_invoke;
  ot->poll = PE_poll_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "deselect", false, "Deselect", "Deselect linked keys rather than selecting them");
  RNA_def_int_vector(ot->srna, "location", 2, nullptr, 0, INT_MAX, "Location", "", 0, 16384);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

bool PE_deselect_all_visible_ex(PTCacheEdit *edit)
{
  bool changed = false;
  POINT_P;
  KEY_K;

  LOOP_VISIBLE_POINTS {
    LOOP_SELECTED_KEYS {
      if ((key->flag & PEK_SELECT) != 0) {
        key->flag &= ~PEK_SELECT;
        point->flag |= PEP_EDIT_RECALC;
        changed = true;
      }
    }
  }
  return changed;
}

bool PE_deselect_all_visible(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  if (!PE_start_edit(edit)) {
    return false;
  }
  return PE_deselect_all_visible_ex(edit);
}

bool PE_box_select(bContext *C, const rcti *rect, const int sel_op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  PEData data;

  if (!PE_start_edit(edit)) {
    return false;
  }

  PE_set_view3d_data(C, &data);
  data.rect = rect;
  data.sel_op = eSelectOp(sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed = PE_deselect_all_visible_ex(edit);
  }

  if (BLI_rcti_is_empty(rect)) {
    /* pass */
  }
  else {
    for_mouse_hit_keys(&data, select_key_op, PSEL_ALL_KEYS);
  }

  bool is_changed = data.is_changed;
  PE_data_free(&data);

  if (is_changed) {
    PE_update_selection(depsgraph, scene, ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);
  }
  return is_changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static void pe_select_cache_free_generic_userdata(void *data)
{
  PE_data_free(static_cast<PEData *>(data));
  MEM_freeN(data);
}

static void pe_select_cache_init_with_generic_userdata(bContext *C, wmGenericUserData *wm_userdata)
{
  PEData *data = static_cast<PEData *>(MEM_callocN(sizeof(*data), __func__));
  wm_userdata->data = data;
  wm_userdata->free_fn = pe_select_cache_free_generic_userdata;
  wm_userdata->use_free = true;
  PE_set_view3d_data(C, data);
}

bool PE_circle_select(
    bContext *C, wmGenericUserData *wm_userdata, const int sel_op, const int mval[2], float rad)
{
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  if (!PE_start_edit(edit)) {
    return false;
  }

  if (wm_userdata->data == nullptr) {
    pe_select_cache_init_with_generic_userdata(C, wm_userdata);
  }

  PEData *data = static_cast<PEData *>(wm_userdata->data);
  data->mval = mval;
  data->rad = rad;
  data->select = (sel_op != SEL_OP_SUB);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data->is_changed = PE_deselect_all_visible_ex(edit);
  }
  for_mouse_hit_keys(data, select_key, eParticleSelectFlag(0));

  if (data->is_changed) {
    PE_update_selection(depsgraph, scene, ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);
  }
  return data->is_changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

int PE_lasso_select(bContext *C, const int mcoords[][2], const int mcoords_len, const int sel_op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ARegion *region = CTX_wm_region(C);
  ParticleEditSettings *pset = PE_settings(scene);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  POINT_P;
  KEY_K;
  float co[3], mat[4][4];
  int screen_co[2];

  PEData data;

  unit_m4(mat);

  if (!PE_start_edit(edit)) {
    return OPERATOR_CANCELLED;
  }

  /* only for depths */
  PE_set_view3d_data(C, &data);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= PE_deselect_all_visible_ex(edit);
  }

  ParticleSystem *psys = edit->psys;
  ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
  LOOP_VISIBLE_POINTS {
    if (edit->psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
      psys_mat_hair_to_global(
          ob, psmd_eval->mesh_final, psys->part->from, psys->particles + p, mat);
    }

    if (pset->selectmode == SCE_SELECT_POINT) {
      LOOP_VISIBLE_KEYS {
        copy_v3_v3(co, key->co);
        mul_m4_v3(mat, co);
        const bool is_select = key->flag & PEK_SELECT;
        const bool is_inside =
            ((ED_view3d_project_int_global(region, co, screen_co, V3D_PROJ_TEST_CLIP_WIN) ==
              V3D_PROJ_RET_OK) &&
             BLI_lasso_is_point_inside(
                 mcoords, mcoords_len, screen_co[0], screen_co[1], IS_CLIPPED) &&
             key_test_depth(&data, co, screen_co));
        const int sel_op_result = ED_select_op_action_deselected(
            eSelectOp(sel_op), is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(key->flag, sel_op_result, PEK_SELECT);
          point->flag |= PEP_EDIT_RECALC;
          data.is_changed = true;
        }
      }
    }
    else if (pset->selectmode == SCE_SELECT_END) {
      if (point->totkey) {
        key = point->keys + point->totkey - 1;
        copy_v3_v3(co, key->co);
        mul_m4_v3(mat, co);
        const bool is_select = key->flag & PEK_SELECT;
        const bool is_inside =
            ((ED_view3d_project_int_global(region, co, screen_co, V3D_PROJ_TEST_CLIP_WIN) ==
              V3D_PROJ_RET_OK) &&
             BLI_lasso_is_point_inside(
                 mcoords, mcoords_len, screen_co[0], screen_co[1], IS_CLIPPED) &&
             key_test_depth(&data, co, screen_co));
        const int sel_op_result = ED_select_op_action_deselected(
            eSelectOp(sel_op), is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(key->flag, sel_op_result, PEK_SELECT);
          point->flag |= PEP_EDIT_RECALC;
          data.is_changed = true;
        }
      }
    }
  }

  bool is_changed = data.is_changed;
  PE_data_free(&data);

  if (is_changed) {
    PE_update_selection(depsgraph, scene, ob, 1);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

static int hide_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  POINT_P;
  KEY_K;

  if (RNA_boolean_get(op->ptr, "unselected")) {
    LOOP_UNSELECTED_POINTS {
      point->flag |= PEP_HIDE;
      point->flag |= PEP_EDIT_RECALC;

      LOOP_KEYS {
        key->flag &= ~PEK_SELECT;
      }
    }
  }
  else {
    LOOP_SELECTED_POINTS {
      point->flag |= PEP_HIDE;
      point->flag |= PEP_EDIT_RECALC;

      LOOP_KEYS {
        key->flag &= ~PEK_SELECT;
      }
    }
  }

  PE_update_selection(depsgraph, scene, ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "PARTICLE_OT_hide";
  ot->description = "Hide selected particles";

  /* api callbacks */
  ot->exec = hide_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reveal Operator
 * \{ */

static int reveal_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  const bool select = RNA_boolean_get(op->ptr, "select");
  POINT_P;
  KEY_K;

  LOOP_POINTS {
    if (point->flag & PEP_HIDE) {
      point->flag &= ~PEP_HIDE;
      point->flag |= PEP_EDIT_RECALC;

      LOOP_KEYS {
        SET_FLAG_FROM_TEST(key->flag, select, PEK_SELECT);
      }
    }
  }

  PE_update_selection(depsgraph, scene, ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal";
  ot->idname = "PARTICLE_OT_reveal";
  ot->description = "Show hidden particles";

  /* api callbacks */
  ot->exec = reveal_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static void select_less_keys(PEData *data, int point_index)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  KEY_K;

  LOOP_SELECTED_KEYS {
    if (k == 0) {
      if (((key + 1)->flag & PEK_SELECT) == 0) {
        key->flag |= PEK_TAG;
      }
    }
    else if (k == point->totkey - 1) {
      if (((key - 1)->flag & PEK_SELECT) == 0) {
        key->flag |= PEK_TAG;
      }
    }
    else {
      if ((((key - 1)->flag & (key + 1)->flag) & PEK_SELECT) == 0) {
        key->flag |= PEK_TAG;
      }
    }
  }

  LOOP_KEYS {
    if ((key->flag & PEK_TAG) && (key->flag & PEK_SELECT)) {
      key->flag &= ~(PEK_TAG | PEK_SELECT);
      point->flag |= PEP_EDIT_RECALC; /* redraw selection only */
      data->is_changed = true;
    }
  }
}

static int select_less_exec(bContext *C, wmOperator * /*op*/)
{
  PEData data;

  PE_set_data(C, &data);
  foreach_point(&data, select_less_keys);

  PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "PARTICLE_OT_select_less";
  ot->description = "Deselect boundary selected keys of each particle";

  /* api callbacks */
  ot->exec = select_less_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

static void select_more_keys(PEData *data, int point_index)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  KEY_K;

  LOOP_KEYS {
    if (key->flag & PEK_SELECT) {
      continue;
    }

    if (k == 0) {
      if ((key + 1)->flag & PEK_SELECT) {
        key->flag |= PEK_TAG;
      }
    }
    else if (k == point->totkey - 1) {
      if ((key - 1)->flag & PEK_SELECT) {
        key->flag |= PEK_TAG;
      }
    }
    else {
      if (((key - 1)->flag | (key + 1)->flag) & PEK_SELECT) {
        key->flag |= PEK_TAG;
      }
    }
  }

  LOOP_KEYS {
    if ((key->flag & PEK_TAG) && (key->flag & PEK_SELECT) == 0) {
      key->flag &= ~PEK_TAG;
      key->flag |= PEK_SELECT;
      point->flag |= PEP_EDIT_RECALC; /* redraw selection only */
      data->is_changed = true;
    }
  }
}

static int select_more_exec(bContext *C, wmOperator * /*op*/)
{
  PEData data;

  PE_set_data(C, &data);
  foreach_point(&data, select_more_keys);

  PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_SELECTED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "PARTICLE_OT_select_more";
  ot->description = "Select keys linked to boundary selected keys of each particle";

  /* api callbacks */
  ot->exec = select_more_exec;
  ot->poll = PE_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Re-Key Operator
 * \{ */

static void rekey_particle(PEData *data, int pa_index)
{
  PTCacheEdit *edit = data->edit;
  ParticleSystem *psys = edit->psys;
  ParticleSimulationData sim = {nullptr};
  ParticleData *pa = psys->particles + pa_index;
  PTCacheEditPoint *point = edit->points + pa_index;
  ParticleKey state;
  HairKey *key, *new_keys, *okey;
  PTCacheEditKey *ekey;
  float dval, sta, end;
  int k;

  sim.depsgraph = data->depsgraph;
  sim.scene = data->scene;
  sim.ob = data->ob;
  sim.psys = edit->psys;

  pa->flag |= PARS_REKEY;

  key = new_keys = static_cast<HairKey *>(
      MEM_callocN(data->totrekey * sizeof(HairKey), "Hair re-key keys"));

  okey = pa->hair;
  /* root and tip stay the same */
  copy_v3_v3(key->co, okey->co);
  copy_v3_v3((key + data->totrekey - 1)->co, (okey + pa->totkey - 1)->co);

  sta = key->time = okey->time;
  end = (key + data->totrekey - 1)->time = (okey + pa->totkey - 1)->time;
  dval = (end - sta) / float(data->totrekey - 1);

  /* interpolate new keys from old ones */
  for (k = 1, key++; k < data->totrekey - 1; k++, key++) {
    state.time = float(k) / float(data->totrekey - 1);
    psys_get_particle_on_path(&sim, pa_index, &state, false);
    copy_v3_v3(key->co, state.co);
    key->time = sta + k * dval;
  }

  /* replace keys */
  if (pa->hair) {
    MEM_freeN(pa->hair);
  }
  pa->hair = new_keys;

  point->totkey = pa->totkey = data->totrekey;

  if (point->keys) {
    MEM_freeN(point->keys);
  }
  ekey = point->keys = static_cast<PTCacheEditKey *>(
      MEM_callocN(pa->totkey * sizeof(PTCacheEditKey), "Hair re-key edit keys"));

  for (k = 0, key = pa->hair; k < pa->totkey; k++, key++, ekey++) {
    ekey->co = key->co;
    ekey->time = &key->time;
    ekey->flag |= PEK_SELECT;
    if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
      ekey->flag |= PEK_USE_WCO;
    }
  }

  pa->flag &= ~PARS_REKEY;
  point->flag |= PEP_EDIT_RECALC;
}

static int rekey_exec(bContext *C, wmOperator *op)
{
  PEData data;

  PE_set_data(C, &data);

  data.dval = 1.0f / float(data.totrekey - 1);
  data.totrekey = RNA_int_get(op->ptr, "keys_number");

  foreach_selected_point(&data, rekey_particle);

  recalc_lengths(data.edit);
  PE_update_object(data.depsgraph, data.scene, data.ob, 1);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_rekey(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rekey";
  ot->idname = "PARTICLE_OT_rekey";
  ot->description = "Change the number of keys of selected particles (root and tip keys included)";

  /* api callbacks */
  ot->exec = rekey_exec;
  ot->invoke = WM_operator_props_popup;
  ot->poll = PE_hair_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna, "keys_number", 2, 2, INT_MAX, "Number of Keys", "", 2, 100);
}

static void rekey_particle_to_time(
    const bContext *C, Scene *scene, Object *ob, int pa_index, float path_time)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys;
  ParticleSimulationData sim = {nullptr};
  ParticleData *pa;
  ParticleKey state;
  HairKey *new_keys, *key;
  PTCacheEditKey *ekey;
  int k;

  if (!edit || !edit->psys) {
    return;
  }

  psys = edit->psys;

  sim.depsgraph = depsgraph;
  sim.scene = scene;
  sim.ob = ob;
  sim.psys = psys;

  pa = psys->particles + pa_index;

  pa->flag |= PARS_REKEY;

  key = new_keys = static_cast<HairKey *>(MEM_dupallocN(pa->hair));

  /* interpolate new keys from old ones (roots stay the same) */
  for (k = 1, key++; k < pa->totkey; k++, key++) {
    state.time = path_time * float(k) / float(pa->totkey - 1);
    psys_get_particle_on_path(&sim, pa_index, &state, false);
    copy_v3_v3(key->co, state.co);
  }

  /* replace hair keys */
  if (pa->hair) {
    MEM_freeN(pa->hair);
  }
  pa->hair = new_keys;

  /* update edit pointers */
  for (k = 0, key = pa->hair, ekey = edit->points[pa_index].keys; k < pa->totkey;
       k++, key++, ekey++)
  {
    ekey->co = key->co;
    ekey->time = &key->time;
  }

  pa->flag &= ~PARS_REKEY;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static int remove_tagged_particles(Object *ob, ParticleSystem *psys, int mirror)
{
  PTCacheEdit *edit = psys->edit;
  ParticleData *pa, *npa = nullptr, *new_pars = nullptr;
  POINT_P;
  PTCacheEditPoint *npoint = nullptr, *new_points = nullptr;
  ParticleSystemModifierData *psmd_eval;
  int i, new_totpart = psys->totpart, removed = 0;

  if (mirror) {
    /* mirror tags */
    psmd_eval = edit->psmd_eval;

    LOOP_TAGGED_POINTS {
      PE_mirror_particle(ob, psmd_eval->mesh_final, psys, psys->particles + p, nullptr);
    }
  }

  LOOP_TAGGED_POINTS {
    new_totpart--;
    removed++;
  }

  if (new_totpart != psys->totpart) {
    if (new_totpart) {
      npa = new_pars = static_cast<ParticleData *>(
          MEM_callocN(new_totpart * sizeof(ParticleData), "ParticleData array"));
      npoint = new_points = static_cast<PTCacheEditPoint *>(
          MEM_callocN(new_totpart * sizeof(PTCacheEditPoint), "PTCacheEditKey array"));

      if (ELEM(nullptr, new_pars, new_points)) {
        /* allocation error! */
        if (new_pars) {
          MEM_freeN(new_pars);
        }
        if (new_points) {
          MEM_freeN(new_points);
        }
        return 0;
      }
    }

    pa = psys->particles;
    point = edit->points;
    for (i = 0; i < psys->totpart; i++, pa++, point++) {
      if (point->flag & PEP_TAG) {
        if (point->keys) {
          MEM_freeN(point->keys);
        }
        if (pa->hair) {
          MEM_freeN(pa->hair);
        }
      }
      else {
        memcpy(npa, pa, sizeof(ParticleData));
        memcpy(npoint, point, sizeof(PTCacheEditPoint));
        npa++;
        npoint++;
      }
    }

    if (psys->particles) {
      MEM_freeN(psys->particles);
    }
    psys->particles = new_pars;

    if (edit->points) {
      MEM_freeN(edit->points);
    }
    edit->points = new_points;

    MEM_SAFE_FREE(edit->mirror_cache);

    if (psys->child) {
      MEM_freeN(psys->child);
      psys->child = nullptr;
      psys->totchild = 0;
    }

    edit->totpoint = psys->totpart = new_totpart;
  }

  return removed;
}

static void remove_tagged_keys(Depsgraph *depsgraph, Object *ob, ParticleSystem *psys)
{
  PTCacheEdit *edit = psys->edit;
  ParticleData *pa;
  HairKey *hkey, *nhkey, *new_hkeys = nullptr;
  POINT_P;
  KEY_K;
  PTCacheEditKey *nkey, *new_keys;
  short new_totkey;

  if (pe_x_mirror(ob)) {
    /* mirror key tags */
    ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
    ParticleSystemModifierData *psmd_eval = (ParticleSystemModifierData *)
        BKE_modifier_get_evaluated(depsgraph, ob, &psmd->modifier);

    LOOP_POINTS {
      LOOP_TAGGED_KEYS {
        PE_mirror_particle(ob, psmd_eval->mesh_final, psys, psys->particles + p, nullptr);
        break;
      }
    }
  }

  LOOP_POINTS {
    new_totkey = point->totkey;
    LOOP_TAGGED_KEYS {
      new_totkey--;
    }
    /* We can't have elements with less than two keys. */
    if (new_totkey < 2) {
      point->flag |= PEP_TAG;
    }
  }
  remove_tagged_particles(ob, psys, pe_x_mirror(ob));

  LOOP_POINTS {
    pa = psys->particles + p;
    new_totkey = pa->totkey;

    LOOP_TAGGED_KEYS {
      new_totkey--;
    }

    if (new_totkey != pa->totkey) {
      nhkey = new_hkeys = static_cast<HairKey *>(
          MEM_callocN(new_totkey * sizeof(HairKey), "HairKeys"));
      nkey = new_keys = static_cast<PTCacheEditKey *>(
          MEM_callocN(new_totkey * sizeof(PTCacheEditKey), "particle edit keys"));

      hkey = pa->hair;
      LOOP_KEYS {
        while (key->flag & PEK_TAG && hkey < pa->hair + pa->totkey) {
          key++;
          hkey++;
        }

        if (hkey < pa->hair + pa->totkey) {
          copy_v3_v3(nhkey->co, hkey->co);
          nhkey->editflag = hkey->editflag;
          nhkey->time = hkey->time;
          nhkey->weight = hkey->weight;

          nkey->co = nhkey->co;
          nkey->time = &nhkey->time;
          /* these can be copied from old edit keys */
          nkey->flag = key->flag;
          nkey->ftime = key->ftime;
          nkey->length = key->length;
          copy_v3_v3(nkey->world_co, key->world_co);
        }
        nkey++;
        nhkey++;
        hkey++;
      }

      if (pa->hair) {
        MEM_freeN(pa->hair);
      }

      if (point->keys) {
        MEM_freeN(point->keys);
      }

      pa->hair = new_hkeys;
      point->keys = new_keys;

      point->totkey = pa->totkey = new_totkey;

      /* flag for recalculating length */
      point->flag |= PEP_EDIT_RECALC;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivide Operator
 * \{ */

/* works like normal edit mode subdivide, inserts keys between neighboring selected keys */
static void subdivide_particle(PEData *data, int pa_index)
{
  PTCacheEdit *edit = data->edit;
  ParticleSystem *psys = edit->psys;
  ParticleSimulationData sim = {nullptr};
  ParticleData *pa = psys->particles + pa_index;
  PTCacheEditPoint *point = edit->points + pa_index;
  ParticleKey state;
  HairKey *key, *nkey, *new_keys;
  PTCacheEditKey *ekey, *nekey, *new_ekeys;

  int k;
  short totnewkey = 0;
  float endtime;

  sim.depsgraph = data->depsgraph;
  sim.scene = data->scene;
  sim.ob = data->ob;
  sim.psys = edit->psys;

  for (k = 0, ekey = point->keys; k < pa->totkey - 1; k++, ekey++) {
    if (ekey->flag & PEK_SELECT && (ekey + 1)->flag & PEK_SELECT) {
      totnewkey++;
    }
  }

  if (totnewkey == 0) {
    return;
  }

  pa->flag |= PARS_REKEY;

  nkey = new_keys = static_cast<HairKey *>(
      MEM_callocN((pa->totkey + totnewkey) * sizeof(HairKey), "Hair subdivide keys"));
  nekey = new_ekeys = static_cast<PTCacheEditKey *>(
      MEM_callocN((pa->totkey + totnewkey) * sizeof(PTCacheEditKey), "Hair subdivide edit keys"));

  key = pa->hair;
  endtime = key[pa->totkey - 1].time;

  for (k = 0, ekey = point->keys; k < pa->totkey - 1; k++, key++, ekey++) {

    memcpy(nkey, key, sizeof(HairKey));
    memcpy(nekey, ekey, sizeof(PTCacheEditKey));

    nekey->co = nkey->co;
    nekey->time = &nkey->time;

    nkey++;
    nekey++;

    if (ekey->flag & PEK_SELECT && (ekey + 1)->flag & PEK_SELECT) {
      nkey->time = (key->time + (key + 1)->time) * 0.5f;
      state.time = (endtime != 0.0f) ? nkey->time / endtime : 0.0f;
      psys_get_particle_on_path(&sim, pa_index, &state, false);
      copy_v3_v3(nkey->co, state.co);

      nekey->co = nkey->co;
      nekey->time = &nkey->time;
      nekey->flag |= PEK_SELECT;
      if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
        nekey->flag |= PEK_USE_WCO;
      }

      nekey++;
      nkey++;
    }
  }
  /* Tip still not copied. */
  memcpy(nkey, key, sizeof(HairKey));
  memcpy(nekey, ekey, sizeof(PTCacheEditKey));

  nekey->co = nkey->co;
  nekey->time = &nkey->time;

  if (pa->hair) {
    MEM_freeN(pa->hair);
  }
  pa->hair = new_keys;

  if (point->keys) {
    MEM_freeN(point->keys);
  }
  point->keys = new_ekeys;

  point->totkey = pa->totkey = pa->totkey + totnewkey;
  point->flag |= PEP_EDIT_RECALC;
  pa->flag &= ~PARS_REKEY;
}

static int subdivide_exec(bContext *C, wmOperator * /*op*/)
{
  PEData data;

  PE_set_data(C, &data);
  foreach_point(&data, subdivide_particle);

  recalc_lengths(data.edit);
  PE_update_selection(data.depsgraph, data.scene, data.ob, 1);
  PE_update_object(data.depsgraph, data.scene, data.ob, 1);
  DEG_id_tag_update(&data.ob->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_subdivide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Subdivide";
  ot->idname = "PARTICLE_OT_subdivide";
  ot->description = "Subdivide selected particles segments (adds keys)";

  /* api callbacks */
  ot->exec = subdivide_exec;
  ot->poll = PE_hair_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Doubles Operator
 * \{ */

static int remove_doubles_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys = edit->psys;
  ParticleSystemModifierData *psmd_eval;
  KDTree_3d *tree;
  KDTreeNearest_3d nearest[10];
  POINT_P;
  float mat[4][4], co[3], threshold = RNA_float_get(op->ptr, "threshold");
  int n, totn, removed, totremoved;

  if (psys->flag & PSYS_GLOBAL_HAIR) {
    return OPERATOR_CANCELLED;
  }

  edit = psys->edit;
  psmd_eval = edit->psmd_eval;
  totremoved = 0;

  do {
    removed = 0;

    tree = BLI_kdtree_3d_new(psys->totpart);

    /* Insert particles into KD-tree. */
    LOOP_SELECTED_POINTS {
      psys_mat_hair_to_object(
          ob, psmd_eval->mesh_final, psys->part->from, psys->particles + p, mat);
      copy_v3_v3(co, point->keys->co);
      mul_m4_v3(mat, co);
      BLI_kdtree_3d_insert(tree, p, co);
    }

    BLI_kdtree_3d_balance(tree);

    /* tag particles to be removed */
    LOOP_SELECTED_POINTS {
      psys_mat_hair_to_object(
          ob, psmd_eval->mesh_final, psys->part->from, psys->particles + p, mat);
      copy_v3_v3(co, point->keys->co);
      mul_m4_v3(mat, co);

      totn = BLI_kdtree_3d_find_nearest_n(tree, co, nearest, 10);

      for (n = 0; n < totn; n++) {
        /* this needs a custom threshold still */
        if (nearest[n].index > p && nearest[n].dist < threshold) {
          if (!(point->flag & PEP_TAG)) {
            point->flag |= PEP_TAG;
            removed++;
          }
        }
      }
    }

    BLI_kdtree_3d_free(tree);

    /* remove tagged particles - don't do mirror here! */
    remove_tagged_particles(ob, psys, 0);
    totremoved += removed;
  } while (removed);

  if (totremoved == 0) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Removed %d double particle(s)", totremoved);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_remove_doubles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Doubles";
  ot->idname = "PARTICLE_OT_remove_doubles";
  ot->description = "Remove selected particles close enough of others";

  /* api callbacks */
  ot->exec = remove_doubles_exec;
  ot->poll = PE_hair_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(ot->srna,
                "threshold",
                0.0002f,
                0.0f,
                FLT_MAX,
                "Merge Distance",
                "Threshold distance within which particles are removed",
                0.00001f,
                0.1f);
}

static int weight_set_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  ParticleEditSettings *pset = PE_settings(scene);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys = edit->psys;
  POINT_P;
  KEY_K;
  HairKey *hkey;
  float weight;
  ParticleBrushData *brush = &pset->brush[pset->brushtype];
  float factor = RNA_float_get(op->ptr, "factor");

  weight = brush->strength;
  edit = psys->edit;

  LOOP_SELECTED_POINTS {
    ParticleData *pa = psys->particles + p;

    LOOP_SELECTED_KEYS {
      hkey = pa->hair + k;
      hkey->weight = interpf(weight, hkey->weight, factor);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_weight_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Set";
  ot->idname = "PARTICLE_OT_weight_set";
  ot->description = "Set the weight of selected keys";

  /* api callbacks */
  ot->exec = weight_set_exec;
  ot->poll = PE_hair_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float(ot->srna,
                "factor",
                1,
                0,
                1,
                "Factor",
                "Interpolation factor between current brush weight, and keys' weights",
                0,
                1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Drawing
 * \{ */

static void brush_drawcursor(bContext *C, int x, int y, void * /*customdata*/)
{
  Scene *scene = CTX_data_scene(C);
  ParticleEditSettings *pset = PE_settings(scene);
  ParticleBrushData *brush;

  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return;
  }

  brush = &pset->brush[pset->brushtype];

  if (brush) {
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    immUniformColor4ub(255, 255, 255, 128);

    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    imm_draw_circle_wire_2d(pos, float(x), float(y), pe_brush_size_get(scene, brush), 40);

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);

    immUnbindProgram();
  }
}

static void toggle_particle_cursor(Scene *scene, bool enable)
{
  ParticleEditSettings *pset = PE_settings(scene);

  if (pset->paintcursor && !enable) {
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(pset->paintcursor));
    pset->paintcursor = nullptr;
  }
  else if (enable) {
    pset->paintcursor = WM_paint_cursor_activate(
        SPACE_VIEW3D, RGN_TYPE_WINDOW, PE_poll_view3d, brush_drawcursor, nullptr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

enum { DEL_PARTICLE, DEL_KEY };

static const EnumPropertyItem delete_type_items[] = {
    {DEL_PARTICLE, "PARTICLE", 0, "Particle", ""},
    {DEL_KEY, "KEY", 0, "Key", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void set_delete_particle(PEData *data, int pa_index)
{
  PTCacheEdit *edit = data->edit;

  edit->points[pa_index].flag |= PEP_TAG;
}

static void set_delete_particle_key(PEData *data, int pa_index, int key_index, bool /*is_inside*/)
{
  PTCacheEdit *edit = data->edit;

  edit->points[pa_index].keys[key_index].flag |= PEK_TAG;
}

static int delete_exec(bContext *C, wmOperator *op)
{
  PEData data;
  int type = RNA_enum_get(op->ptr, "type");

  PE_set_data(C, &data);

  if (type == DEL_KEY) {
    foreach_selected_key(&data, set_delete_particle_key);
    remove_tagged_keys(data.depsgraph, data.ob, data.edit->psys);
    recalc_lengths(data.edit);
  }
  else if (type == DEL_PARTICLE) {
    foreach_selected_point(&data, set_delete_particle);
    remove_tagged_particles(data.ob, data.edit->psys, pe_x_mirror(data.ob));
    recalc_lengths(data.edit);
  }

  DEG_id_tag_update(&data.ob->id, ID_RECALC_GEOMETRY);
  BKE_particle_batch_cache_dirty_tag(data.edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, data.ob);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->idname = "PARTICLE_OT_delete";
  ot->description = "Delete selected particles or keys";

  /* api callbacks */
  ot->exec = delete_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = PE_hair_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          delete_type_items,
                          DEL_PARTICLE,
                          "Type",
                          "Delete a full particle or only keys");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mirror Operator
 * \{ */

static void PE_mirror_x(Depsgraph *depsgraph, Scene *scene, Object *ob, int tagged)
{
  Mesh *mesh = (Mesh *)(ob->data);
  ParticleSystemModifierData *psmd_eval;
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys = edit->psys;
  ParticleData *pa, *newpa, *new_pars;
  PTCacheEditPoint *newpoint, *new_points;
  POINT_P;
  KEY_K;
  HairKey *hkey;
  int *mirrorfaces = nullptr;
  int rotation, totpart, newtotpart;

  if (psys->flag & PSYS_GLOBAL_HAIR) {
    return;
  }

  psmd_eval = edit->psmd_eval;
  if (!psmd_eval->mesh_final) {
    return;
  }

  const bool use_dm_final_indices = (psys->part->use_modifier_stack &&
                                     !psmd_eval->mesh_final->runtime->deformed_only);

  /* NOTE: this is not nice to use tessfaces but hard to avoid since pa->num uses tessfaces */
  BKE_mesh_tessface_ensure(mesh);

  /* NOTE: In case psys uses Mesh tessface indices, we mirror final Mesh itself, not orig mesh.
   * Avoids an (impossible) mesh -> orig -> mesh tessface indices conversion. */
  mirrorfaces = mesh_get_x_mirror_faces(
      ob, nullptr, use_dm_final_indices ? psmd_eval->mesh_final : nullptr);

  if (!edit->mirror_cache) {
    PE_update_mirror_cache(ob, psys);
  }

  totpart = psys->totpart;
  newtotpart = psys->totpart;
  LOOP_VISIBLE_POINTS {
    pa = psys->particles + p;

    if (!tagged) {
      if (point_is_selected(point)) {
        if (edit->mirror_cache[p] != -1) {
          /* already has a mirror, don't need to duplicate */
          PE_mirror_particle(ob, psmd_eval->mesh_final, psys, pa, nullptr);
          continue;
        }
        point->flag |= PEP_TAG;
      }
    }

    if ((point->flag & PEP_TAG) && mirrorfaces[pa->num * 2] != -1) {
      newtotpart++;
    }
  }

  if (newtotpart != psys->totpart) {
    const MFace *mtessface = use_dm_final_indices ?
                                 (const MFace *)CustomData_get_layer(
                                     &psmd_eval->mesh_final->fdata_legacy, CD_MFACE) :
                                 (const MFace *)CustomData_get_layer(&mesh->fdata_legacy,
                                                                     CD_MFACE);

    /* allocate new arrays and copy existing */
    new_pars = static_cast<ParticleData *>(
        MEM_callocN(newtotpart * sizeof(ParticleData), "ParticleData new"));
    new_points = static_cast<PTCacheEditPoint *>(
        MEM_callocN(newtotpart * sizeof(PTCacheEditPoint), "PTCacheEditPoint new"));

    if (psys->particles) {
      memcpy(new_pars, psys->particles, totpart * sizeof(ParticleData));
      MEM_freeN(psys->particles);
    }
    psys->particles = new_pars;

    if (edit->points) {
      memcpy(new_points, edit->points, totpart * sizeof(PTCacheEditPoint));
      MEM_freeN(edit->points);
    }
    edit->points = new_points;

    MEM_SAFE_FREE(edit->mirror_cache);

    edit->totpoint = psys->totpart = newtotpart;

    /* create new elements */
    newpa = psys->particles + totpart;
    newpoint = edit->points + totpart;

    for (p = 0, point = edit->points; p < totpart; p++, point++) {
      pa = psys->particles + p;
      const int pa_num = pa->num;

      if (point->flag & PEP_HIDE) {
        continue;
      }

      if (!(point->flag & PEP_TAG) || mirrorfaces[pa_num * 2] == -1) {
        continue;
      }

      /* duplicate */
      *newpa = *pa;
      *newpoint = *point;
      if (pa->hair) {
        newpa->hair = static_cast<HairKey *>(MEM_dupallocN(pa->hair));
      }
      if (point->keys) {
        newpoint->keys = static_cast<PTCacheEditKey *>(MEM_dupallocN(point->keys));
      }

      /* rotate weights according to vertex index rotation */
      rotation = mirrorfaces[pa_num * 2 + 1];
      newpa->fuv[0] = pa->fuv[2];
      newpa->fuv[1] = pa->fuv[1];
      newpa->fuv[2] = pa->fuv[0];
      newpa->fuv[3] = pa->fuv[3];
      while (rotation--) {
        if (mtessface[pa_num].v4) {
          SHIFT4(float, newpa->fuv[0], newpa->fuv[1], newpa->fuv[2], newpa->fuv[3]);
        }
        else {
          SHIFT3(float, newpa->fuv[0], newpa->fuv[1], newpa->fuv[2]);
        }
      }

      /* assign face index */
      /* NOTE: mesh_get_x_mirror_faces generates -1 for non-found mirror,
       * same as DMCACHE_NOTFOUND. */
      newpa->num = mirrorfaces[pa_num * 2];

      if (use_dm_final_indices) {
        newpa->num_dmcache = DMCACHE_ISCHILD;
      }
      else {
        newpa->num_dmcache = psys_particle_dm_face_lookup(
            psmd_eval->mesh_final, psmd_eval->mesh_original, newpa->num, newpa->fuv, nullptr);
      }

      /* update edit key pointers */
      key = newpoint->keys;
      for (k = 0, hkey = newpa->hair; k < newpa->totkey; k++, hkey++, key++) {
        key->co = hkey->co;
        key->time = &hkey->time;
      }

      /* map key positions as mirror over x axis */
      PE_mirror_particle(ob, psmd_eval->mesh_final, psys, pa, newpa);

      newpa++;
      newpoint++;
    }
  }

  LOOP_POINTS {
    point->flag &= ~PEP_TAG;
  }

  MEM_freeN(mirrorfaces);
}

static int mirror_exec(bContext *C, wmOperator * /*op*/)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  PE_mirror_x(depsgraph, scene, ob, 0);

  update_world_cos(ob, edit);
  psys_free_path_cache(nullptr, edit);

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
  BKE_particle_batch_cache_dirty_tag(edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  return OPERATOR_FINISHED;
}

static bool mirror_poll(bContext *C)
{
  if (!PE_hair_poll(C)) {
    return false;
  }

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  /* The operator only works for hairs emitted from faces. */
  return edit->psys->part->from == PART_FROM_FACE;
}

void PARTICLE_OT_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror";
  ot->idname = "PARTICLE_OT_mirror";
  ot->description = "Duplicate and mirror the selected particles along the local X axis";

  /* api callbacks */
  ot->exec = mirror_exec;
  ot->poll = mirror_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Edit Callbacks
 * \{ */

static void brush_comb(PEData *data,
                       float[4][4] /*mat*/,
                       float imat[4][4],
                       int point_index,
                       int key_index,
                       PTCacheEditKey *key,
                       float mouse_distance)
{
  ParticleEditSettings *pset = PE_settings(data->scene);
  float cvec[3], fac;

  if (pset->flag & PE_LOCK_FIRST && key_index == 0) {
    return;
  }

  fac = float(pow(double(1.0f - mouse_distance / data->rad), double(data->combfac)));

  copy_v3_v3(cvec, data->dvec);
  mul_mat3_m4_v3(imat, cvec);
  mul_v3_fl(cvec, fac);
  add_v3_v3(key->co, cvec);

  (data->edit->points + point_index)->flag |= PEP_EDIT_RECALC;
}

static void brush_cut(PEData *data, int pa_index)
{
  PTCacheEdit *edit = data->edit;
  ARegion *region = data->vc.region;
  Object *ob = data->ob;
  ParticleEditSettings *pset = PE_settings(data->scene);
  ParticleCacheKey *key = edit->pathcache[pa_index];
  float rad2, cut_time = 1.0;
  float x0, x1, v0, v1, o0, o1, xo0, xo1, d, dv;
  int k, cut, keys = int(pow(2.0, double(pset->draw_step)));
  int screen_co[2];

  BLI_assert(data->rng != nullptr);
  /* blunt scissors */
  if (BLI_rng_get_float(data->rng) > data->cutfac) {
    return;
  }

  /* don't cut hidden */
  if (edit->points[pa_index].flag & PEP_HIDE) {
    return;
  }

  if (ED_view3d_project_int_global(region, key->co, screen_co, V3D_PROJ_TEST_CLIP_NEAR) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  rad2 = data->rad * data->rad;

  cut = 0;

  x0 = float(screen_co[0]);
  x1 = float(screen_co[1]);

  o0 = float(data->mval[0]);
  o1 = float(data->mval[1]);

  xo0 = x0 - o0;
  xo1 = x1 - o1;

  /* check if root is inside circle */
  if (xo0 * xo0 + xo1 * xo1 < rad2 && key_test_depth(data, key->co, screen_co)) {
    cut_time = -1.0f;
    cut = 1;
  }
  else {
    /* calculate path time closest to root that was inside the circle */
    for (k = 1, key++; k <= keys; k++, key++) {

      if ((ED_view3d_project_int_global(region, key->co, screen_co, V3D_PROJ_TEST_CLIP_NEAR) !=
           V3D_PROJ_RET_OK) ||
          key_test_depth(data, key->co, screen_co) == 0)
      {
        x0 = float(screen_co[0]);
        x1 = float(screen_co[1]);

        xo0 = x0 - o0;
        xo1 = x1 - o1;
        continue;
      }

      v0 = float(screen_co[0]) - x0;
      v1 = float(screen_co[1]) - x1;

      dv = v0 * v0 + v1 * v1;

      d = (v0 * xo1 - v1 * xo0);

      d = dv * rad2 - d * d;

      if (d > 0.0f) {
        d = sqrtf(d);

        cut_time = -(v0 * xo0 + v1 * xo1 + d);

        if (cut_time > 0.0f) {
          cut_time /= dv;

          if (cut_time < 1.0f) {
            cut_time += float(k - 1);
            cut_time /= float(keys);
            cut = 1;
            break;
          }
        }
      }

      x0 = float(screen_co[0]);
      x1 = float(screen_co[1]);

      xo0 = x0 - o0;
      xo1 = x1 - o1;
    }
  }

  if (cut) {
    if (cut_time < 0.0f) {
      edit->points[pa_index].flag |= PEP_TAG;
    }
    else {
      rekey_particle_to_time(data->context, data->scene, ob, pa_index, cut_time);
      edit->points[pa_index].flag |= PEP_EDIT_RECALC;
    }
  }
}

static void brush_length(PEData *data, int point_index, float /*mouse_distance*/)
{
  PTCacheEdit *edit = data->edit;
  PTCacheEditPoint *point = edit->points + point_index;
  KEY_K;
  float dvec[3], pvec[3] = {0.0f, 0.0f, 0.0f};

  LOOP_KEYS {
    if (k == 0) {
      copy_v3_v3(pvec, key->co);
    }
    else {
      sub_v3_v3v3(dvec, key->co, pvec);
      copy_v3_v3(pvec, key->co);
      mul_v3_fl(dvec, data->growfac);
      add_v3_v3v3(key->co, (key - 1)->co, dvec);
    }
  }

  point->flag |= PEP_EDIT_RECALC;
}

static void brush_puff(PEData *data, int point_index, float mouse_distance)
{
  PTCacheEdit *edit = data->edit;
  ParticleSystem *psys = edit->psys;
  PTCacheEditPoint *point = edit->points + point_index;
  KEY_K;
  float mat[4][4], imat[4][4];

  float onor_prev[3];           /* previous normal (particle-space) */
  float ofs_prev[3];            /* accumulate offset for puff_volume (particle-space) */
  float co_root[3], no_root[3]; /* root location and normal (global-space) */
  float co_prev[3], co[3];      /* track key coords as we loop (global-space) */
  float fac = 0.0f, length_accum = 0.0f;
  bool puff_volume = false;
  bool changed = false;

  zero_v3(ofs_prev);

  {
    ParticleEditSettings *pset = PE_settings(data->scene);
    ParticleBrushData *brush = &pset->brush[pset->brushtype];
    puff_volume = (brush->flag & PE_BRUSH_DATA_PUFF_VOLUME) != 0;
  }

  if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
    psys_mat_hair_to_global(
        data->ob, data->mesh, psys->part->from, psys->particles + point_index, mat);
    invert_m4_m4(imat, mat);
  }
  else {
    unit_m4(mat);
    unit_m4(imat);
  }

  LOOP_KEYS {
    float kco[3];

    if (k == 0) {
      /* find root coordinate and normal on emitter */
      copy_v3_v3(co, key->co);
      mul_m4_v3(mat, co);

      /* Use `kco` as the object space version of world-space `co`,
       * `ob->world_to_object` is set before calling. */
      mul_v3_m4v3(kco, data->ob->world_to_object().ptr(), co);

      point_index = BLI_kdtree_3d_find_nearest(edit->emitter_field, kco, nullptr);
      if (point_index == -1) {
        return;
      }

      copy_v3_v3(co_root, co);
      copy_v3_v3(no_root, &edit->emitter_cosnos[point_index * 6 + 3]);
      mul_mat3_m4_v3(data->ob->object_to_world().ptr(), no_root); /* normal into global-space */
      normalize_v3(no_root);

      if (puff_volume) {
        copy_v3_v3(onor_prev, no_root);
        mul_mat3_m4_v3(imat, onor_prev); /* global-space into particle space */
        normalize_v3(onor_prev);
      }

      fac = float(pow(double(1.0f - mouse_distance / data->rad), double(data->pufffac)));
      fac *= 0.025f;
      if (data->invert) {
        fac = -fac;
      }
    }
    else {
      /* Compute position as if hair was standing up straight. */
      float length;
      copy_v3_v3(co_prev, co);
      copy_v3_v3(co, key->co);
      mul_m4_v3(mat, co);
      length = len_v3v3(co_prev, co);
      length_accum += length;

      if ((data->select == 0 || (key->flag & PEK_SELECT)) && !(key->flag & PEK_HIDE)) {
        float dco[3]; /* delta temp var */

        madd_v3_v3v3fl(kco, co_root, no_root, length_accum);

        /* blend between the current and straight position */
        sub_v3_v3v3(dco, kco, co);
        madd_v3_v3fl(co, dco, fac);
        /* keep the same distance from the root or we get glitches #35406. */
        dist_ensure_v3_v3fl(co, co_root, length_accum);

        /* Re-use dco to compare before and after translation and add to the offset. */
        copy_v3_v3(dco, key->co);

        mul_v3_m4v3(key->co, imat, co);

        if (puff_volume) {
          /* accumulate the total distance moved to apply to unselected
           * keys that come after */
          sub_v3_v3v3(ofs_prev, key->co, dco);
        }
        changed = true;
      }
      else {

        if (puff_volume) {
#if 0
          /* this is simple but looks bad, adds annoying kinks */
          add_v3_v3(key->co, ofs);
#else
          /* Translate (not rotate) the rest of the hair if its not selected. */
          {
/* NOLINTNEXTLINE: readability-redundant-preprocessor */
#  if 0 /* Kind of works but looks worse than what's below. */

            /* Move the unselected point on a vector based on the
             * hair direction and the offset */
            float c1[3], c2[3];
            sub_v3_v3v3(dco, lastco, co);
            mul_mat3_m4_v3(imat, dco); /* into particle space */

            /* move the point along a vector perpendicular to the
             * hairs direction, reduces odd kinks, */
            cross_v3_v3v3(c1, ofs, dco);
            cross_v3_v3v3(c2, c1, dco);
            normalize_v3(c2);
            mul_v3_fl(c2, len_v3(ofs));
            add_v3_v3(key->co, c2);
#  else
            /* Move the unselected point on a vector based on the
             * the normal of the closest geometry */
            float oco[3], onor[3];
            copy_v3_v3(oco, key->co);
            mul_m4_v3(mat, oco);

            /* Use `kco` as the object space version of world-space `co`,
             * `ob->world_to_object` is set before calling. */
            mul_v3_m4v3(kco, data->ob->world_to_object().ptr(), oco);

            point_index = BLI_kdtree_3d_find_nearest(edit->emitter_field, kco, nullptr);
            if (point_index != -1) {
              copy_v3_v3(onor, &edit->emitter_cosnos[point_index * 6 + 3]);
              mul_mat3_m4_v3(data->ob->object_to_world().ptr(),
                             onor);       /* Normal into world-space. */
              mul_mat3_m4_v3(imat, onor); /* World-space into particle-space. */
              normalize_v3(onor);
            }
            else {
              copy_v3_v3(onor, onor_prev);
            }

            if (!is_zero_v3(ofs_prev)) {
              mul_v3_fl(onor, len_v3(ofs_prev));

              add_v3_v3(key->co, onor);
            }

            copy_v3_v3(onor_prev, onor);
#  endif
          }
#endif
        }
      }
    }
  }

  if (changed) {
    point->flag |= PEP_EDIT_RECALC;
  }
}

static void BKE_brush_weight_get(PEData *data,
                                 float[4][4] /*mat*/,
                                 float[4][4] /*imat*/,
                                 int point_index,
                                 int key_index,
                                 PTCacheEditKey * /*key*/,
                                 float /*mouse_distance*/)
{
  /* roots have full weight always */
  if (key_index) {
    PTCacheEdit *edit = data->edit;
    ParticleSystem *psys = edit->psys;

    ParticleData *pa = psys->particles + point_index;
    pa->hair[key_index].weight = data->weightfac;

    (data->edit->points + point_index)->flag |= PEP_EDIT_RECALC;
  }
}

static void brush_smooth_get(PEData *data,
                             float mat[4][4],
                             float[4][4] /*imat*/,
                             int /*point_index*/,
                             int key_index,
                             PTCacheEditKey *key,
                             float /*mouse_distance*/)
{
  if (key_index) {
    float dvec[3];

    sub_v3_v3v3(dvec, key->co, (key - 1)->co);
    mul_mat3_m4_v3(mat, dvec);
    add_v3_v3(data->vec, dvec);
    data->tot++;
  }
}

static void brush_smooth_do(PEData *data,
                            float[4][4] /*mat*/,
                            float imat[4][4],
                            int point_index,
                            int key_index,
                            PTCacheEditKey *key,
                            float /*mouse_distance*/)
{
  float vec[3], dvec[3];

  if (key_index) {
    copy_v3_v3(vec, data->vec);
    mul_mat3_m4_v3(imat, vec);

    sub_v3_v3v3(dvec, key->co, (key - 1)->co);

    sub_v3_v3v3(dvec, vec, dvec);
    mul_v3_fl(dvec, data->smoothfac);

    add_v3_v3(key->co, dvec);
  }

  (data->edit->points + point_index)->flag |= PEP_EDIT_RECALC;
}

/* convert from triangle barycentric weights to quad mean value weights */
static void intersect_dm_quad_weights(
    const float v1[3], const float v2[3], const float v3[3], const float v4[3], float w[4])
{
  float co[3], vert[4][3];

  copy_v3_v3(vert[0], v1);
  copy_v3_v3(vert[1], v2);
  copy_v3_v3(vert[2], v3);
  copy_v3_v3(vert[3], v4);

  co[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2] + v4[0] * w[3];
  co[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2] + v4[1] * w[3];
  co[2] = v1[2] * w[0] + v2[2] * w[1] + v3[2] * w[2] + v4[2] * w[3];

  interp_weights_poly_v3(w, vert, 4, co);
}

/** Check intersection with an evaluated mesh. */
static int particle_intersect_mesh(Depsgraph *depsgraph,
                                   Scene * /*scene*/,
                                   Object *ob,
                                   Mesh *mesh,
                                   float *vert_cos,
                                   const float co1[3],
                                   const float co2[3],
                                   float *min_d,
                                   int *min_face,
                                   float *min_w,
                                   float *face_minmax,
                                   float *pa_minmax,
                                   float radius,
                                   float *ipoint)
{
  const MFace *mface = nullptr;
  int i, totface, intersect = 0;
  float cur_d;
  blender::float2 cur_uv;
  blender::float3 v1, v2, v3, v4, min, max, p_min, p_max;
  float cur_ipoint[3];

  if (mesh == nullptr) {
    psys_disable_all(ob);

    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    mesh = (Mesh *)BKE_object_get_evaluated_mesh(ob_eval);
    if (mesh == nullptr) {
      return 0;
    }

    psys_enable_all(ob);

    if (mesh == nullptr) {
      return 0;
    }
  }

  /* BMESH_ONLY, deform dm may not have tessface */
  BKE_mesh_tessface_ensure(mesh);

  if (pa_minmax == nullptr) {
    INIT_MINMAX(p_min, p_max);
    minmax_v3v3_v3(p_min, p_max, co1);
    minmax_v3v3_v3(p_min, p_max, co2);
  }
  else {
    copy_v3_v3(p_min, pa_minmax);
    copy_v3_v3(p_max, pa_minmax + 3);
  }

  totface = mesh->totface_legacy;
  mface = (const MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE);
  blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();

  /* lets intersect the faces */
  for (i = 0; i < totface; i++, mface++) {
    if (vert_cos) {
      copy_v3_v3(v1, vert_cos + 3 * mface->v1);
      copy_v3_v3(v2, vert_cos + 3 * mface->v2);
      copy_v3_v3(v3, vert_cos + 3 * mface->v3);
      if (mface->v4) {
        copy_v3_v3(v4, vert_cos + 3 * mface->v4);
      }
    }
    else {
      copy_v3_v3(v1, positions[mface->v1]);
      copy_v3_v3(v2, positions[mface->v2]);
      copy_v3_v3(v3, positions[mface->v3]);
      if (mface->v4) {
        copy_v3_v3(v4, positions[mface->v4]);
      }
    }

    if (face_minmax == nullptr) {
      INIT_MINMAX(min, max);
      blender::math::min_max(blender::float3(v1), min, max);
      blender::math::min_max(blender::float3(v2), min, max);
      blender::math::min_max(blender::float3(v3), min, max);
      if (mface->v4) {
        blender::math::min_max(blender::float3(v4), min, max);
      }
      if (isect_aabb_aabb_v3(min, max, p_min, p_max) == 0) {
        continue;
      }
    }
    else {
      copy_v3_v3(min, face_minmax + 6 * i);
      copy_v3_v3(max, face_minmax + 6 * i + 3);
      if (isect_aabb_aabb_v3(min, max, p_min, p_max) == 0) {
        continue;
      }
    }

    if (radius > 0.0f) {
      if (isect_sweeping_sphere_tri_v3(co1, co2, radius, v2, v3, v1, &cur_d, cur_ipoint)) {
        if (cur_d < *min_d) {
          *min_d = cur_d;
          copy_v3_v3(ipoint, cur_ipoint);
          *min_face = i;
          intersect = 1;
        }
      }
      if (mface->v4) {
        if (isect_sweeping_sphere_tri_v3(co1, co2, radius, v4, v1, v3, &cur_d, cur_ipoint)) {
          if (cur_d < *min_d) {
            *min_d = cur_d;
            copy_v3_v3(ipoint, cur_ipoint);
            *min_face = i;
            intersect = 1;
          }
        }
      }
    }
    else {
      if (isect_line_segment_tri_v3(co1, co2, v1, v2, v3, &cur_d, cur_uv)) {
        if (cur_d < *min_d) {
          *min_d = cur_d;
          min_w[0] = 1.0f - cur_uv[0] - cur_uv[1];
          min_w[1] = cur_uv[0];
          min_w[2] = cur_uv[1];
          min_w[3] = 0.0f;
          if (mface->v4) {
            intersect_dm_quad_weights(v1, v2, v3, v4, min_w);
          }
          *min_face = i;
          intersect = 1;
        }
      }
      if (mface->v4) {
        if (isect_line_segment_tri_v3(co1, co2, v1, v3, v4, &cur_d, cur_uv)) {
          if (cur_d < *min_d) {
            *min_d = cur_d;
            min_w[0] = 1.0f - cur_uv[0] - cur_uv[1];
            min_w[1] = 0.0f;
            min_w[2] = cur_uv[0];
            min_w[3] = cur_uv[1];
            intersect_dm_quad_weights(v1, v2, v3, v4, min_w);
            *min_face = i;
            intersect = 1;
          }
        }
      }
    }
  }
  return intersect;
}

struct BrushAddCountIterData {
  Depsgraph *depsgraph;
  Scene *scene;
  Object *object;
  Mesh *mesh;
  PEData *data;
  int number;
  short size;
  float imat[4][4];
  ParticleData *add_pars;
};

struct BrushAddCountIterTLSData {
  RNG *rng;
  int num_added;
};

static void brush_add_count_iter(void *__restrict iter_data_v,
                                 const int iter,
                                 const TaskParallelTLS *__restrict tls_v)
{
  BrushAddCountIterData *iter_data = (BrushAddCountIterData *)iter_data_v;
  Depsgraph *depsgraph = iter_data->depsgraph;
  PEData *data = iter_data->data;
  PTCacheEdit *edit = data->edit;
  ParticleSystem *psys = edit->psys;
  ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
  ParticleData *add_pars = iter_data->add_pars;
  BrushAddCountIterTLSData *tls = static_cast<BrushAddCountIterTLSData *>(tls_v->userdata_chunk);
  const int number = iter_data->number;
  const short size = iter_data->size;
  const int size2 = size * size;
  float dmx, dmy;
  if (number > 1) {
    dmx = size;
    dmy = size;
    if (tls->rng == nullptr) {
      tls->rng = BLI_rng_new_srandom(psys->seed + data->mval[0] + data->mval[1] +
                                     BLI_task_parallel_thread_id(tls_v));
    }
    /* rejection sampling to get points in circle */
    while (dmx * dmx + dmy * dmy > size2) {
      dmx = (2.0f * BLI_rng_get_float(tls->rng) - 1.0f) * size;
      dmy = (2.0f * BLI_rng_get_float(tls->rng) - 1.0f) * size;
    }
  }
  else {
    dmx = 0.0f;
    dmy = 0.0f;
  }

  float mco[2];
  mco[0] = data->mval[0] + dmx;
  mco[1] = data->mval[1] + dmy;

  float co1[3], co2[3];
  ED_view3d_win_to_segment_clipped(depsgraph, data->vc.region, data->vc.v3d, mco, co1, co2, true);

  mul_m4_v3(iter_data->imat, co1);
  mul_m4_v3(iter_data->imat, co2);
  float min_d = 2.0;

  /* warning, returns the derived mesh face */
  BLI_assert(iter_data->mesh != nullptr);
  if (particle_intersect_mesh(depsgraph,
                              iter_data->scene,
                              iter_data->object,
                              iter_data->mesh,
                              nullptr,
                              co1,
                              co2,
                              &min_d,
                              &add_pars[iter].num_dmcache,
                              add_pars[iter].fuv,
                              nullptr,
                              nullptr,
                              0,
                              nullptr))
  {
    if (psys->part->use_modifier_stack && !psmd_eval->mesh_final->runtime->deformed_only) {
      add_pars[iter].num = add_pars[iter].num_dmcache;
      add_pars[iter].num_dmcache = DMCACHE_ISCHILD;
    }
    else if (iter_data->mesh == psmd_eval->mesh_original) {
      /* Final DM is not same topology as orig mesh,
       * we have to map num_dmcache to real final dm. */
      add_pars[iter].num = add_pars[iter].num_dmcache;
      add_pars[iter].num_dmcache = psys_particle_dm_face_lookup(psmd_eval->mesh_final,
                                                                psmd_eval->mesh_original,
                                                                add_pars[iter].num,
                                                                add_pars[iter].fuv,
                                                                nullptr);
    }
    else {
      add_pars[iter].num = add_pars[iter].num_dmcache;
    }
    if (add_pars[iter].num != DMCACHE_NOTFOUND) {
      tls->num_added++;
    }
  }
}

static void brush_add_count_iter_reduce(const void *__restrict /*userdata*/,
                                        void *__restrict join_v,
                                        void *__restrict chunk_v)
{
  BrushAddCountIterTLSData *join = (BrushAddCountIterTLSData *)join_v;
  BrushAddCountIterTLSData *tls = (BrushAddCountIterTLSData *)chunk_v;
  join->num_added += tls->num_added;
}

static void brush_add_count_iter_free(const void *__restrict /*userdata_v*/,
                                      void *__restrict chunk_v)
{
  BrushAddCountIterTLSData *tls = (BrushAddCountIterTLSData *)chunk_v;
  if (tls->rng != nullptr) {
    BLI_rng_free(tls->rng);
  }
}

static int brush_add(const bContext *C, PEData *data, short number)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = data->scene;
  Object *ob = data->ob;
  Mesh *mesh;
  PTCacheEdit *edit = data->edit;
  ParticleSystem *psys = edit->psys;
  ParticleData *add_pars;
  ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
  ParticleSimulationData sim = {nullptr};
  ParticleEditSettings *pset = PE_settings(scene);
  int i, k, n = 0, totpart = psys->totpart;
  float co1[3], imat[4][4];
  float framestep, timestep;
  short size = pset->brush[PE_BRUSH_ADD].size;
  RNG *rng;

  invert_m4_m4(imat, ob->object_to_world().ptr());

  if (psys->flag & PSYS_GLOBAL_HAIR) {
    return 0;
  }

  add_pars = static_cast<ParticleData *>(
      MEM_callocN(number * sizeof(ParticleData), "ParticleData add"));

  rng = BLI_rng_new_srandom(psys->seed + data->mval[0] + data->mval[1]);

  sim.depsgraph = depsgraph;
  sim.scene = scene;
  sim.ob = ob;
  sim.psys = psys;
  sim.psmd = psmd_eval;

  timestep = psys_get_timestep(&sim);

  if (psys->part->use_modifier_stack || psmd_eval->mesh_final->runtime->deformed_only) {
    mesh = psmd_eval->mesh_final;
  }
  else {
    mesh = psmd_eval->mesh_original;
  }
  BLI_assert(mesh);

  /* Calculate positions of new particles to add, based on brush intersection
   * with object. New particle data is assigned to a corresponding to check
   * index element of add_pars array. This means, that add_pars is a sparse
   * array.
   */
  BrushAddCountIterData iter_data;
  iter_data.depsgraph = depsgraph;
  iter_data.scene = scene;
  iter_data.object = ob;
  iter_data.mesh = mesh;
  iter_data.data = data;
  iter_data.number = number;
  iter_data.size = size;
  iter_data.add_pars = add_pars;
  copy_m4_m4(iter_data.imat, imat);

  BrushAddCountIterTLSData tls = {nullptr};

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.userdata_chunk = &tls;
  settings.userdata_chunk_size = sizeof(BrushAddCountIterTLSData);
  settings.func_reduce = brush_add_count_iter_reduce;
  settings.func_free = brush_add_count_iter_free;
  BLI_task_parallel_range(0, number, &iter_data, brush_add_count_iter, &settings);

  /* Convert add_parse to a dense array, where all new particles are in the
   * beginning of the array.
   */
  n = tls.num_added;
  for (int current_iter = 0, new_index = 0; current_iter < number; current_iter++) {
    if (add_pars[current_iter].num == DMCACHE_NOTFOUND) {
      continue;
    }
    if (new_index != current_iter) {
      new_index++;
      continue;
    }
    memcpy(add_pars + new_index, add_pars + current_iter, sizeof(ParticleData));
    new_index++;
  }

  /* TODO(sergey): Consider multi-threading this part as well. */
  if (n) {
    int newtotpart = totpart + n;
    float hairmat[4][4], cur_co[3];
    KDTree_3d *tree = nullptr;
    ParticleData *pa, *new_pars = static_cast<ParticleData *>(
                          MEM_callocN(newtotpart * sizeof(ParticleData), "ParticleData new"));
    PTCacheEditPoint *point,
        *new_points = static_cast<PTCacheEditPoint *>(
            MEM_callocN(newtotpart * sizeof(PTCacheEditPoint), "PTCacheEditPoint array new"));
    PTCacheEditKey *key;
    HairKey *hkey;

    /* save existing elements */
    memcpy(new_pars, psys->particles, totpart * sizeof(ParticleData));
    memcpy(new_points, edit->points, totpart * sizeof(PTCacheEditPoint));

    /* change old arrays to new ones */
    if (psys->particles) {
      MEM_freeN(psys->particles);
    }
    psys->particles = new_pars;

    if (edit->points) {
      MEM_freeN(edit->points);
    }
    edit->points = new_points;

    MEM_SAFE_FREE(edit->mirror_cache);

    /* create tree for interpolation */
    if (pset->flag & PE_INTERPOLATE_ADDED && psys->totpart) {
      tree = BLI_kdtree_3d_new(psys->totpart);

      for (i = 0, pa = psys->particles; i < totpart; i++, pa++) {
        psys_particle_on_dm(psmd_eval->mesh_final,
                            psys->part->from,
                            pa->num,
                            pa->num_dmcache,
                            pa->fuv,
                            pa->foffset,
                            cur_co,
                            nullptr,
                            nullptr,
                            nullptr,
                            nullptr);
        BLI_kdtree_3d_insert(tree, i, cur_co);
      }

      BLI_kdtree_3d_balance(tree);
    }

    edit->totpoint = psys->totpart = newtotpart;

    /* create new elements */
    pa = psys->particles + totpart;
    point = edit->points + totpart;

    for (i = totpart; i < newtotpart; i++, pa++, point++) {
      memcpy(pa, add_pars + i - totpart, sizeof(ParticleData));
      pa->hair = static_cast<HairKey *>(
          MEM_callocN(pset->totaddkey * sizeof(HairKey), "BakeKey key add"));
      key = point->keys = static_cast<PTCacheEditKey *>(
          MEM_callocN(pset->totaddkey * sizeof(PTCacheEditKey), "PTCacheEditKey add"));
      point->totkey = pa->totkey = pset->totaddkey;

      for (k = 0, hkey = pa->hair; k < pa->totkey; k++, hkey++, key++) {
        key->co = hkey->co;
        key->time = &hkey->time;

        if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
          key->flag |= PEK_USE_WCO;
        }
      }

      pa->size = 1.0f;
      init_particle(&sim, pa);
      reset_particle(&sim, pa, 0.0, 1.0);
      point->flag |= PEP_EDIT_RECALC;
      if (pe_x_mirror(ob)) {
        point->flag |= PEP_TAG; /* signal for duplicate */
      }

      framestep = pa->lifetime / float(pset->totaddkey - 1);

      if (tree) {
        ParticleData *ppa;
        HairKey *thkey;
        ParticleKey key3[3];
        KDTreeNearest_3d ptn[3];
        int w, maxw;
        float maxd, totw = 0.0, weight[3];

        psys_particle_on_dm(psmd_eval->mesh_final,
                            psys->part->from,
                            pa->num,
                            pa->num_dmcache,
                            pa->fuv,
                            pa->foffset,
                            co1,
                            nullptr,
                            nullptr,
                            nullptr,
                            nullptr);
        maxw = BLI_kdtree_3d_find_nearest_n(tree, co1, ptn, 3);

        maxd = ptn[maxw - 1].dist;

        for (w = 0; w < maxw; w++) {
          weight[w] = float(pow(2.0, double(-6.0f * ptn[w].dist / maxd)));
          totw += weight[w];
        }
        for (; w < 3; w++) {
          weight[w] = 0.0f;
        }

        if (totw > 0.0f) {
          for (w = 0; w < maxw; w++) {
            weight[w] /= totw;
          }
        }
        else {
          for (w = 0; w < maxw; w++) {
            weight[w] = 1.0f / maxw;
          }
        }

        ppa = psys->particles + ptn[0].index;

        for (k = 0; k < pset->totaddkey; k++) {
          thkey = (HairKey *)pa->hair + k;
          thkey->time = pa->time + k * framestep;

          key3[0].time = thkey->time / 100.0f;
          psys_get_particle_on_path(&sim, ptn[0].index, key3, false);
          mul_v3_fl(key3[0].co, weight[0]);

          /* TODO: interpolating the weight would be nicer */
          thkey->weight = (ppa->hair + std::min(k, ppa->totkey - 1))->weight;

          if (maxw > 1) {
            key3[1].time = key3[0].time;
            psys_get_particle_on_path(&sim, ptn[1].index, &key3[1], false);
            mul_v3_fl(key3[1].co, weight[1]);
            add_v3_v3(key3[0].co, key3[1].co);

            if (maxw > 2) {
              key3[2].time = key3[0].time;
              psys_get_particle_on_path(&sim, ptn[2].index, &key3[2], false);
              mul_v3_fl(key3[2].co, weight[2]);
              add_v3_v3(key3[0].co, key3[2].co);
            }
          }

          if (k == 0) {
            sub_v3_v3v3(co1, pa->state.co, key3[0].co);
          }

          add_v3_v3v3(thkey->co, key3[0].co, co1);

          thkey->time = key3[0].time;
        }
      }
      else {
        for (k = 0, hkey = pa->hair; k < pset->totaddkey; k++, hkey++) {
          madd_v3_v3v3fl(hkey->co, pa->state.co, pa->state.vel, k * framestep * timestep);
          hkey->time += k * framestep;
          hkey->weight = 1.0f - float(k) / float(pset->totaddkey - 1);
        }
      }
      for (k = 0, hkey = pa->hair; k < pset->totaddkey; k++, hkey++) {
        psys_mat_hair_to_global(ob, psmd_eval->mesh_final, psys->part->from, pa, hairmat);
        invert_m4_m4(imat, hairmat);
        mul_m4_v3(imat, hkey->co);
      }
    }

    if (tree) {
      BLI_kdtree_3d_free(tree);
    }
  }

  MEM_freeN(add_pars);

  BLI_rng_free(rng);

  return n;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Edit Operator
 * \{ */

struct BrushEdit {
  Scene *scene;
  ViewLayer *view_layer;
  Object *ob;
  PTCacheEdit *edit;

  int first;
  int lastmouse[2];
  float zfac;

  /** Optional cached view settings to avoid setting on every mouse-move. */
  PEData data;
};

static int brush_edit_init(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = CTX_data_active_object(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ARegion *region = CTX_wm_region(C);
  BrushEdit *bedit;
  blender::float3 min, max;

  /* set the 'distance factor' for grabbing (used in comb etc) */
  INIT_MINMAX(min, max);
  PE_minmax(depsgraph, scene, view_layer, min, max);
  mid_v3_v3v3(min, min, max);

  bedit = static_cast<BrushEdit *>(MEM_callocN(sizeof(BrushEdit), "BrushEdit"));
  bedit->first = 1;
  op->customdata = bedit;

  bedit->scene = scene;
  bedit->view_layer = view_layer;
  bedit->ob = ob;
  bedit->edit = edit;

  bedit->zfac = ED_view3d_calc_zfac(static_cast<const RegionView3D *>(region->regiondata), min);

  /* cache view depths and settings for re-use */
  PE_set_view3d_data(C, &bedit->data);
  PE_create_random_generator(&bedit->data);

  return 1;
}

static void brush_edit_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
  BrushEdit *bedit = static_cast<BrushEdit *>(op->customdata);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = bedit->scene;
  Object *ob = bedit->ob;
  PTCacheEdit *edit = bedit->edit;
  ParticleEditSettings *pset = PE_settings(scene);
  ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
  ParticleBrushData *brush = &pset->brush[pset->brushtype];
  ARegion *region = CTX_wm_region(C);
  float vec[3], mousef[2];
  int mval[2];
  int flip, mouse[2], removed = 0, added = 0, selected = 0, tot_steps = 1, step = 1;
  float dx, dy, dmax;
  int lock_root = pset->flag & PE_LOCK_FIRST;

  if (!PE_start_edit(edit)) {
    return;
  }

  RNA_float_get_array(itemptr, "mouse", mousef);
  mouse[0] = mousef[0];
  mouse[1] = mousef[1];
  flip = RNA_boolean_get(itemptr, "pen_flip");

  if (bedit->first) {
    bedit->lastmouse[0] = mouse[0];
    bedit->lastmouse[1] = mouse[1];
  }

  dx = mouse[0] - bedit->lastmouse[0];
  dy = mouse[1] - bedit->lastmouse[1];

  mval[0] = mouse[0];
  mval[1] = mouse[1];

  /* Disable locking temporarily for disconnected hair. */
  if (edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR) {
    pset->flag &= ~PE_LOCK_FIRST;
  }

  if (((pset->brushtype == PE_BRUSH_ADD) ?
           (sqrtf(dx * dx + dy * dy) > pset->brush[PE_BRUSH_ADD].step) :
           (dx != 0 || dy != 0)) ||
      bedit->first)
  {
    PEData data = bedit->data;
    data.context = C; /* TODO(mai): why isn't this set in bedit->data? */

    view3d_operator_needs_opengl(C);
    selected = short(count_selected_keys(scene, edit));

    dmax = max_ff(fabsf(dx), fabsf(dy));
    tot_steps = dmax / (0.2f * pe_brush_size_get(scene, brush)) + 1;

    dx /= float(tot_steps);
    dy /= float(tot_steps);

    for (step = 1; step <= tot_steps; step++) {
      mval[0] = bedit->lastmouse[0] + step * dx;
      mval[1] = bedit->lastmouse[1] + step * dy;

      switch (pset->brushtype) {
        case PE_BRUSH_COMB: {
          const float xy_delta[2] = {dx, dy};
          data.mval = mval;
          data.rad = pe_brush_size_get(scene, brush);

          data.combfac = (brush->strength - 0.5f) * 2.0f;
          if (data.combfac < 0.0f) {
            data.combfac = 1.0f - 9.0f * data.combfac;
          }
          else {
            data.combfac = 1.0f - data.combfac;
          }

          invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

          ED_view3d_win_to_delta(region, xy_delta, bedit->zfac, vec);
          data.dvec = vec;

          foreach_mouse_hit_key(&data, brush_comb, selected);
          break;
        }
        case PE_BRUSH_CUT: {
          if (edit->psys && edit->pathcache) {
            data.mval = mval;
            data.rad = pe_brush_size_get(scene, brush);
            data.cutfac = brush->strength;

            if (selected) {
              foreach_selected_point(&data, brush_cut);
            }
            else {
              foreach_point(&data, brush_cut);
            }

            removed = remove_tagged_particles(ob, edit->psys, pe_x_mirror(ob));
            if (pset->flag & PE_KEEP_LENGTHS) {
              recalc_lengths(edit);
            }
          }
          else {
            removed = 0;
          }

          break;
        }
        case PE_BRUSH_LENGTH: {
          data.mval = mval;

          data.rad = pe_brush_size_get(scene, brush);
          data.growfac = brush->strength / 50.0f;

          if (brush->invert ^ flip) {
            data.growfac = 1.0f - data.growfac;
          }
          else {
            data.growfac = 1.0f + data.growfac;
          }

          foreach_mouse_hit_point(&data, brush_length, selected);

          if (pset->flag & PE_KEEP_LENGTHS) {
            recalc_lengths(edit);
          }
          break;
        }
        case PE_BRUSH_PUFF: {
          if (edit->psys) {
            data.mesh = psmd_eval->mesh_final;
            data.mval = mval;
            data.rad = pe_brush_size_get(scene, brush);
            data.select = selected;

            data.pufffac = (brush->strength - 0.5f) * 2.0f;
            if (data.pufffac < 0.0f) {
              data.pufffac = 1.0f - 9.0f * data.pufffac;
            }
            else {
              data.pufffac = 1.0f - data.pufffac;
            }

            data.invert = (brush->invert ^ flip);
            invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

            foreach_mouse_hit_point(&data, brush_puff, selected);
          }
          break;
        }
        case PE_BRUSH_ADD: {
          if (edit->psys && edit->psys->part->from == PART_FROM_FACE) {
            data.mval = mval;

            added = brush_add(C, &data, brush->count);

            if (pset->flag & PE_KEEP_LENGTHS) {
              recalc_lengths(edit);
            }
          }
          else {
            added = 0;
          }
          break;
        }
        case PE_BRUSH_SMOOTH: {
          data.mval = mval;
          data.rad = pe_brush_size_get(scene, brush);

          data.vec[0] = data.vec[1] = data.vec[2] = 0.0f;
          data.tot = 0;

          data.smoothfac = brush->strength;

          invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

          foreach_mouse_hit_key(&data, brush_smooth_get, selected);

          if (data.tot) {
            mul_v3_fl(data.vec, 1.0f / float(data.tot));
            foreach_mouse_hit_key(&data, brush_smooth_do, selected);
          }

          break;
        }
        case PE_BRUSH_WEIGHT: {
          if (edit->psys) {
            data.mesh = psmd_eval->mesh_final;
            data.mval = mval;
            data.rad = pe_brush_size_get(scene, brush);

            data.weightfac = brush->strength; /* note that this will never be zero */

            foreach_mouse_hit_key(&data, BKE_brush_weight_get, selected);
          }

          break;
        }
      }
      if ((pset->flag & PE_KEEP_LENGTHS) == 0) {
        recalc_lengths(edit);
      }

      if (ELEM(pset->brushtype, PE_BRUSH_ADD, PE_BRUSH_CUT) && (added || removed)) {
        if (pset->brushtype == PE_BRUSH_ADD && pe_x_mirror(ob)) {
          PE_mirror_x(depsgraph, scene, ob, 1);
        }

        update_world_cos(ob, edit);
        psys_free_path_cache(nullptr, edit);
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      else {
        PE_update_object(depsgraph, scene, ob, 1);
      }
    }

    if (edit->psys) {
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
      BKE_particle_batch_cache_dirty_tag(edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
      DEG_id_tag_update(&ob->id, ID_RECALC_PSYS_REDO);
    }
    else {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
    }

    bedit->lastmouse[0] = mouse[0];
    bedit->lastmouse[1] = mouse[1];
    bedit->first = 0;
  }

  pset->flag |= lock_root;
}

static void brush_edit_exit(wmOperator *op)
{
  BrushEdit *bedit = static_cast<BrushEdit *>(op->customdata);

  PE_data_free(&bedit->data);
  MEM_freeN(bedit);
}

static int brush_edit_exec(bContext *C, wmOperator *op)
{
  if (!brush_edit_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    brush_edit_apply(C, op, &itemptr);
  }
  RNA_END;

  brush_edit_exit(op);

  return OPERATOR_FINISHED;
}

static void brush_edit_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
  PointerRNA itemptr;
  float mouse[2];

  copy_v2fl_v2i(mouse, event->mval);

  /* fill in stroke */
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  RNA_float_set_array(&itemptr, "mouse", mouse);
  RNA_boolean_set(&itemptr, "pen_flip", event->modifier & KM_SHIFT); /* XXX hardcoded */

  /* apply */
  brush_edit_apply(C, op, &itemptr);
}

static int brush_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!brush_edit_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  brush_edit_apply_event(C, op, event);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int brush_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE: /* XXX hardcoded */
      if (event->val == KM_RELEASE) {
        brush_edit_exit(op);
        return OPERATOR_FINISHED;
      }
      break;
    case MOUSEMOVE:
      brush_edit_apply_event(C, op, event);
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void brush_edit_cancel(bContext * /*C*/, wmOperator *op)
{
  brush_edit_exit(op);
}

static bool brush_edit_poll(bContext *C)
{
  return PE_poll_view3d(C) && WM_toolsystem_active_tool_is_brush(C);
}

void PARTICLE_OT_brush_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Brush Edit";
  ot->idname = "PARTICLE_OT_brush_edit";
  ot->description = "Apply a stroke of brush to the particles";

  /* api callbacks */
  ot->exec = brush_edit_exec;
  ot->invoke = brush_edit_invoke;
  ot->modal = brush_edit_modal;
  ot->cancel = brush_edit_cancel;
  ot->poll = brush_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut Shape
 * \{ */

static bool shape_cut_poll(bContext *C)
{
  if (PE_hair_poll(C)) {
    Scene *scene = CTX_data_scene(C);
    ParticleEditSettings *pset = PE_settings(scene);

    if (pset->shape_object && (pset->shape_object->type == OB_MESH)) {
      return true;
    }
  }

  return false;
}

struct PointInsideBVH {
  BVHTreeFromMesh bvhdata;
  int num_hits;
};

static void point_inside_bvh_cb(void *userdata,
                                int index,
                                const BVHTreeRay *ray,
                                BVHTreeRayHit *hit)
{
  PointInsideBVH *data = static_cast<PointInsideBVH *>(userdata);

  data->bvhdata.raycast_callback(&data->bvhdata, index, ray, hit);

  if (hit->index != -1) {
    ++data->num_hits;
  }
}

/* true if the point is inside the shape mesh */
static bool shape_cut_test_point(PEData *data, ParticleEditSettings *pset, ParticleCacheKey *key)
{
  BVHTreeFromMesh *shape_bvh = &data->shape_bvh;
  const float dir[3] = {1.0f, 0.0f, 0.0f};
  PointInsideBVH userdata;

  userdata.bvhdata = data->shape_bvh;
  userdata.num_hits = 0;

  float co_shape[3];
  mul_v3_m4v3(co_shape, pset->shape_object->world_to_object().ptr(), key->co);

  BLI_bvhtree_ray_cast_all(
      shape_bvh->tree, co_shape, dir, 0.0f, BVH_RAYCAST_DIST_MAX, point_inside_bvh_cb, &userdata);

  /* for any point inside a watertight mesh the number of hits is uneven */
  return (userdata.num_hits % 2) == 1;
}

static void shape_cut(PEData *data, int pa_index)
{
  PTCacheEdit *edit = data->edit;
  Object *ob = data->ob;
  ParticleEditSettings *pset = PE_settings(data->scene);
  ParticleCacheKey *key;

  bool cut;
  float cut_time = 1.0;
  int k, totkeys = 1 << pset->draw_step;

  /* don't cut hidden */
  if (edit->points[pa_index].flag & PEP_HIDE) {
    return;
  }

  cut = false;

  /* check if root is inside the cut shape */
  key = edit->pathcache[pa_index];
  if (!shape_cut_test_point(data, pset, key)) {
    cut_time = -1.0f;
    cut = true;
  }
  else {
    for (k = 0; k < totkeys; k++, key++) {
      BVHTreeRayHit hit;

      float co_curr_shape[3], co_next_shape[3];
      float dir_shape[3];
      float len_shape;

      mul_v3_m4v3(co_curr_shape, pset->shape_object->world_to_object().ptr(), key->co);
      mul_v3_m4v3(co_next_shape, pset->shape_object->world_to_object().ptr(), (key + 1)->co);

      sub_v3_v3v3(dir_shape, co_next_shape, co_curr_shape);
      len_shape = normalize_v3(dir_shape);

      memset(&hit, 0, sizeof(hit));
      hit.index = -1;
      hit.dist = len_shape;
      BLI_bvhtree_ray_cast(data->shape_bvh.tree,
                           co_curr_shape,
                           dir_shape,
                           0.0f,
                           &hit,
                           data->shape_bvh.raycast_callback,
                           &data->shape_bvh);
      if (hit.index >= 0) {
        if (hit.dist < len_shape) {
          cut_time = ((hit.dist / len_shape) + float(k)) / float(totkeys);
          cut = true;
          break;
        }
      }
    }
  }

  if (cut) {
    if (cut_time < 0.0f) {
      edit->points[pa_index].flag |= PEP_TAG;
    }
    else {
      rekey_particle_to_time(data->context, data->scene, ob, pa_index, cut_time);
      edit->points[pa_index].flag |= PEP_EDIT_RECALC;
    }
  }
}

static int shape_cut_exec(bContext *C, wmOperator * /*op*/)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ParticleEditSettings *pset = PE_settings(scene);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  Object *shapeob = pset->shape_object;
  int selected = count_selected_keys(scene, edit);
  int lock_root = pset->flag & PE_LOCK_FIRST;

  if (!PE_start_edit(edit)) {
    return OPERATOR_CANCELLED;
  }

  /* Disable locking temporarily for disconnected hair. */
  if (edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR) {
    pset->flag &= ~PE_LOCK_FIRST;
  }

  if (edit->psys && edit->pathcache) {
    PEData data;
    int removed;

    PE_set_data(C, &data);
    if (!PE_create_shape_tree(&data, shapeob)) {
      /* shapeob may not have faces... */
      return OPERATOR_CANCELLED;
    }

    if (selected) {
      foreach_selected_point(&data, shape_cut);
    }
    else {
      foreach_point(&data, shape_cut);
    }

    removed = remove_tagged_particles(ob, edit->psys, pe_x_mirror(ob));
    recalc_lengths(edit);

    if (removed) {
      update_world_cos(ob, edit);
      psys_free_path_cache(nullptr, edit);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    else {
      PE_update_object(data.depsgraph, scene, ob, 1);
    }

    if (edit->psys) {
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
      BKE_particle_batch_cache_dirty_tag(edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
      DEG_id_tag_update(&ob->id, ID_RECALC_PSYS_REDO);
    }
    else {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
    }

    PE_free_shape_tree(&data);
  }

  pset->flag |= lock_root;

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_shape_cut(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shape Cut";
  ot->idname = "PARTICLE_OT_shape_cut";
  ot->description = "Cut hair to conform to the set shape object";

  /* api callbacks */
  ot->exec = shape_cut_exec;
  ot->poll = shape_cut_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Edit Toggle Operator
 * \{ */

void PE_create_particle_edit(
    Depsgraph *depsgraph, Scene *scene, Object *ob, PointCache *cache, ParticleSystem *psys)
{
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  PTCacheEdit *edit;
  ParticleSystemModifierData *psmd = (psys) ? psys_get_modifier(ob, psys) : nullptr;
  ParticleSystemModifierData *psmd_eval = nullptr;
  POINT_P;
  KEY_K;
  ParticleData *pa = nullptr;
  HairKey *hkey;
  int totpoint;

  if (psmd != nullptr) {
    psmd_eval = (ParticleSystemModifierData *)BKE_modifiers_findby_name(ob_eval,
                                                                        psmd->modifier.name);
  }

  /* no psmd->dm happens in case particle system modifier is not enabled */
  if (!(psys && psmd && psmd_eval->mesh_final) && !cache) {
    return;
  }

  if (cache && cache->flag & PTCACHE_DISK_CACHE) {
    return;
  }

  if (psys == nullptr && (cache && BLI_listbase_is_empty(&cache->mem_cache))) {
    return;
  }

  edit = (psys) ? psys->edit : cache->edit;

  if (!edit) {
    ParticleSystem *psys_eval = nullptr;
    if (psys) {
      psys_eval = psys_eval_get(depsgraph, ob, psys);
      psys_copy_particles(psys, psys_eval);
    }

    totpoint = psys ? psys->totpart : int(((PTCacheMem *)cache->mem_cache.first)->totpoint);

    edit = static_cast<PTCacheEdit *>(MEM_callocN(sizeof(PTCacheEdit), "PE_create_particle_edit"));
    edit->points = static_cast<PTCacheEditPoint *>(
        MEM_callocN(totpoint * sizeof(PTCacheEditPoint), "PTCacheEditPoints"));
    edit->totpoint = totpoint;

    if (psys && !cache) {
      edit->psmd = psmd;
      edit->psmd_eval = psmd_eval;
      psys->edit = edit;
      edit->psys = psys;
      edit->psys_eval = psys_eval;

      psys->free_edit = PE_free_ptcache_edit;

      edit->pathcache = nullptr;
      BLI_listbase_clear(&edit->pathcachebufs);

      pa = psys->particles;
      LOOP_POINTS {
        point->totkey = pa->totkey;
        point->keys = static_cast<PTCacheEditKey *>(
            MEM_callocN(point->totkey * sizeof(PTCacheEditKey), "ParticleEditKeys"));
        point->flag |= PEP_EDIT_RECALC;

        hkey = pa->hair;
        LOOP_KEYS {
          key->co = hkey->co;
          key->time = &hkey->time;
          key->flag = hkey->editflag;
          if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
            key->flag |= PEK_USE_WCO;
            hkey->editflag |= PEK_USE_WCO;
          }

          hkey++;
        }
        pa++;
      }
      update_world_cos(ob, edit);
    }
    else {
      int totframe = 0;

      cache->edit = edit;
      cache->free_edit = PE_free_ptcache_edit;
      edit->psys = nullptr;

      LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
        totframe++;
      }

      LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
        LOOP_POINTS {
          void *cur[BPHYS_TOT_DATA];
          if (BKE_ptcache_mem_pointers_seek(p, pm, cur) == 0) {
            continue;
          }

          if (!point->totkey) {
            key = point->keys = static_cast<PTCacheEditKey *>(
                MEM_callocN(totframe * sizeof(PTCacheEditKey), "ParticleEditKeys"));
            point->flag |= PEP_EDIT_RECALC;
          }
          else {
            key = point->keys + point->totkey;
          }

          key->co = static_cast<float *>(cur[BPHYS_DATA_LOCATION]);
          key->vel = static_cast<float *>(cur[BPHYS_DATA_VELOCITY]);
          key->rot = static_cast<float *>(cur[BPHYS_DATA_ROTATION]);
          key->ftime = float(pm->frame);
          key->time = &key->ftime;
          BKE_ptcache_mem_pointers_incr(cur);

          point->totkey++;
        }
      }
      psys = nullptr;
    }

    recalc_lengths(edit);
    if (psys && !cache) {
      recalc_emitter_field(depsgraph, ob, psys);
    }

    PE_update_object(depsgraph, scene, ob, 1);
  }
}

static bool particle_edit_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr || ob->type != OB_MESH) {
    return false;
  }
  if (!ob->data || ID_IS_LINKED(ob->data) || ID_IS_OVERRIDE_LIBRARY(ob->data)) {
    return false;
  }

  return ED_object_particle_edit_mode_supported(ob);
}

static void free_all_psys_edit(Object *object)
{
  for (ParticleSystem *psys = static_cast<ParticleSystem *>(object->particlesystem.first);
       psys != nullptr;
       psys = psys->next)
  {
    if (psys->edit != nullptr) {
      BLI_assert(psys->free_edit != nullptr);
      psys->free_edit(psys->edit);
      psys->free_edit = nullptr;
      psys->edit = nullptr;
    }
  }
}

bool ED_object_particle_edit_mode_supported(const Object *ob)
{
  return (ob->particlesystem.first || BKE_modifiers_findby_type(ob, eModifierType_Cloth) ||
          BKE_modifiers_findby_type(ob, eModifierType_Softbody));
}

void ED_object_particle_edit_mode_enter_ex(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Needed so #ParticleSystemModifierData.mesh_final is set. */
  BKE_scene_graph_evaluated_ensure(depsgraph, G_MAIN);

  PTCacheEdit *edit;

  ob->mode |= OB_MODE_PARTICLE_EDIT;

  edit = PE_create_current(depsgraph, scene, ob);

  /* Mesh may have changed since last entering editmode.
   * NOTE: this may have run before if the edit data was just created,
   * so could avoid this and speed up a little. */
  if (edit && edit->psys) {
    /* Make sure pointer to the evaluated modifier data is up to date,
     * with possible changes applied when object was outside of the
     * edit mode. */
    Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
    edit->psmd_eval = (ParticleSystemModifierData *)BKE_modifiers_findby_name(
        object_eval, edit->psmd->modifier.name);
    recalc_emitter_field(depsgraph, ob, edit->psys);
  }

  toggle_particle_cursor(scene, true);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_PARTICLE, nullptr);
}

void ED_object_particle_edit_mode_enter(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ED_object_particle_edit_mode_enter_ex(depsgraph, scene, ob);
}

void ED_object_particle_edit_mode_exit_ex(Scene *scene, Object *ob)
{
  ob->mode &= ~OB_MODE_PARTICLE_EDIT;
  toggle_particle_cursor(scene, false);
  free_all_psys_edit(ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
}

void ED_object_particle_edit_mode_exit(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ED_object_particle_edit_mode_exit_ex(scene, ob);
}

static int particle_edit_toggle_exec(bContext *C, wmOperator *op)
{
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_PARTICLE_EDIT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, eObjectMode(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (!is_mode_set) {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    ED_object_particle_edit_mode_enter_ex(depsgraph, scene, ob);
  }
  else {
    ED_object_particle_edit_mode_exit_ex(scene, ob);
  }

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_particle_edit_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Particle Edit Toggle";
  ot->idname = "PARTICLE_OT_particle_edit_toggle";
  ot->description = "Toggle particle edit mode";

  /* api callbacks */
  ot->exec = particle_edit_toggle_exec;
  ot->poll = particle_edit_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Editable Operator
 * \{ */

static int clear_edited_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  ParticleSystem *psys = psys_get_current(ob);

  if (psys->edit) {
    if (/*psys->edit->edited ||*/ true) {
      PE_free_ptcache_edit(psys->edit);

      psys->edit = nullptr;
      psys->free_edit = nullptr;

      psys->recalc |= ID_RECALC_PSYS_RESET;
      psys->flag &= ~PSYS_GLOBAL_HAIR;
      psys->flag &= ~PSYS_EDITED;

      psys_reset(psys, PSYS_RESET_DEPSGRAPH);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
      BKE_particle_batch_cache_dirty_tag(psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  else { /* some operation might have protected hair from editing so let's clear the flag */
    psys->recalc |= ID_RECALC_PSYS_RESET;
    psys->flag &= ~PSYS_GLOBAL_HAIR;
    psys->flag &= ~PSYS_EDITED;
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_edited_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Edited";
  ot->idname = "PARTICLE_OT_edited_clear";
  ot->description = "Undo all edition performed on the particle system";

  /* api callbacks */
  ot->exec = clear_edited_exec;
  ot->poll = particle_edit_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unify length operator
 * \{ */

static float calculate_point_length(PTCacheEditPoint *point)
{
  float length = 0.0f;
  KEY_K;
  LOOP_KEYS {
    if (k > 0) {
      length += len_v3v3((key - 1)->co, key->co);
    }
  }
  return length;
}

static float calculate_average_length(PTCacheEdit *edit)
{
  int num_selected = 0;
  float total_length = 0;
  POINT_P;
  LOOP_SELECTED_POINTS {
    total_length += calculate_point_length(point);
    num_selected++;
  }
  if (num_selected == 0) {
    return 0.0f;
  }
  return total_length / num_selected;
}

static void scale_point_factor(PTCacheEditPoint *point, float factor)
{
  float orig_prev_co[3], prev_co[3];
  KEY_K;
  LOOP_KEYS {
    if (k == 0) {
      copy_v3_v3(orig_prev_co, key->co);
      copy_v3_v3(prev_co, key->co);
    }
    else {
      float new_co[3];
      float delta[3];

      sub_v3_v3v3(delta, key->co, orig_prev_co);
      mul_v3_fl(delta, factor);
      add_v3_v3v3(new_co, prev_co, delta);

      copy_v3_v3(orig_prev_co, key->co);
      copy_v3_v3(key->co, new_co);
      copy_v3_v3(prev_co, key->co);
    }
  }
  point->flag |= PEP_EDIT_RECALC;
}

static void scale_point_to_length(PTCacheEditPoint *point, float length)
{
  const float point_length = calculate_point_length(point);
  if (point_length != 0.0f) {
    const float factor = length / point_length;
    scale_point_factor(point, factor);
  }
}

static void scale_points_to_length(PTCacheEdit *edit, float length)
{
  POINT_P;
  LOOP_SELECTED_POINTS {
    scale_point_to_length(point, length);
  }
  recalc_lengths(edit);
}

static int unify_length_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  float average_length = calculate_average_length(edit);

  if (average_length == 0.0f) {
    return OPERATOR_CANCELLED;
  }
  scale_points_to_length(edit, average_length);

  PE_update_object(depsgraph, scene, ob, 1);
  if (edit->psys) {
    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
  }
  else {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }

  return OPERATOR_FINISHED;
}

void PARTICLE_OT_unify_length(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unify Length";
  ot->idname = "PARTICLE_OT_unify_length";
  ot->description = "Make selected hair the same length";

  /* api callbacks */
  ot->exec = unify_length_exec;
  ot->poll = PE_poll_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
