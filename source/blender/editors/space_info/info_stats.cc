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
 * \ingroup spinfo
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLF_api.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph_query.h"

#include "ED_info.h"

#include "WM_api.h"

#include "UI_resources.h"

#include "GPU_capabilities.h"

ENUM_OPERATORS(eUserpref_StatusBar_Flag, STATUSBAR_SHOW_VERSION)

#define MAX_INFO_NUM_LEN 16

struct SceneStats {
  uint64_t totvert, totvertsel, totvertsculpt;
  uint64_t totedge, totedgesel;
  uint64_t totface, totfacesel, totfacesculpt;
  uint64_t totbone, totbonesel;
  uint64_t totobj, totobjsel;
  uint64_t totlamp, totlampsel;
  uint64_t tottri;
  uint64_t totgplayer, totgpframe, totgpstroke, totgppoint;
};

struct SceneStatsFmt {
  /* Totals */
  char totvert[MAX_INFO_NUM_LEN], totvertsel[MAX_INFO_NUM_LEN], totvertsculpt[MAX_INFO_NUM_LEN];
  char totface[MAX_INFO_NUM_LEN], totfacesel[MAX_INFO_NUM_LEN];
  char totedge[MAX_INFO_NUM_LEN], totedgesel[MAX_INFO_NUM_LEN], totfacesculpt[MAX_INFO_NUM_LEN];
  char totbone[MAX_INFO_NUM_LEN], totbonesel[MAX_INFO_NUM_LEN];
  char totobj[MAX_INFO_NUM_LEN], totobjsel[MAX_INFO_NUM_LEN];
  char totlamp[MAX_INFO_NUM_LEN], totlampsel[MAX_INFO_NUM_LEN];
  char tottri[MAX_INFO_NUM_LEN];
  char totgplayer[MAX_INFO_NUM_LEN], totgpframe[MAX_INFO_NUM_LEN];
  char totgpstroke[MAX_INFO_NUM_LEN], totgppoint[MAX_INFO_NUM_LEN];
};

static bool stats_mesheval(const Mesh *me_eval, bool is_selected, SceneStats *stats)
{
  if (me_eval == nullptr) {
    return false;
  }

  int totvert, totedge, totface, totloop;
  if (me_eval->runtime.subdiv_ccg != nullptr) {
    const SubdivCCG *subdiv_ccg = me_eval->runtime.subdiv_ccg;
    BKE_subdiv_ccg_topology_counters(subdiv_ccg, &totvert, &totedge, &totface, &totloop);
  }
  else {
    totvert = me_eval->totvert;
    totedge = me_eval->totedge;
    totface = me_eval->totpoly;
    totloop = me_eval->totloop;
  }

  stats->totvert += totvert;
  stats->totedge += totedge;
  stats->totface += totface;
  stats->tottri += poly_to_tri_count(totface, totloop);

  if (is_selected) {
    stats->totvertsel += totvert;
    stats->totfacesel += totface;
  }
  return true;
}

static void stats_object(Object *ob,
                         const View3D *v3d_local,
                         SceneStats *stats,
                         GSet *objects_gset)
{
  if ((ob->base_flag & BASE_VISIBLE_VIEWLAYER) == 0) {
    return;
  }

  if (v3d_local && !BKE_object_is_visible_in_viewport(v3d_local, ob)) {
    return;
  }

  const bool is_selected = (ob->base_flag & BASE_SELECTED) != 0;

  stats->totobj++;
  if (is_selected) {
    stats->totobjsel++;
  }

  switch (ob->type) {
    case OB_MESH: {
      /* we assume evaluated mesh is already built, this strictly does stats now. */
      const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob);
      if (!BLI_gset_add(objects_gset, (void *)me_eval)) {
        break;
      }
      stats_mesheval(me_eval, is_selected, stats);
      break;
    }
    case OB_LAMP:
      stats->totlamp++;
      if (is_selected) {
        stats->totlampsel++;
      }
      break;
    case OB_SURF:
    case OB_CURVE:
    case OB_FONT: {
      const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob);
      if ((me_eval != nullptr) && !BLI_gset_add(objects_gset, (void *)me_eval)) {
        break;
      }

      if (stats_mesheval(me_eval, is_selected, stats)) {
        break;
      }
      ATTR_FALLTHROUGH; /* Fall-through to displist. */
    }
    case OB_MBALL: {
      int totv = 0, totf = 0, tottri = 0;

      if (ob->runtime.curve_cache && ob->runtime.curve_cache->disp.first) {
        /* NOTE: We only get the same curve_cache for instances of the same curve/font/...
         * For simple linked duplicated objects, each has its own dispList. */
        if (!BLI_gset_add(objects_gset, ob->runtime.curve_cache)) {
          break;
        }

        BKE_displist_count(&ob->runtime.curve_cache->disp, &totv, &totf, &tottri);
      }

      stats->totvert += totv;
      stats->totface += totf;
      stats->tottri += tottri;

      if (is_selected) {
        stats->totvertsel += totv;
        stats->totfacesel += totf;
      }
      break;
    }
    case OB_GPENCIL: {
      if (is_selected) {
        bGPdata *gpd = (bGPdata *)ob->data;
        if (!BLI_gset_add(objects_gset, gpd)) {
          break;
        }
        /* GPXX Review if we can move to other place when object change
         * maybe to depsgraph evaluation
         */
        BKE_gpencil_stats_update(gpd);

        stats->totgplayer += gpd->totlayer;
        stats->totgpframe += gpd->totframe;
        stats->totgpstroke += gpd->totstroke;
        stats->totgppoint += gpd->totpoint;
      }
      break;
    }
    case OB_HAIR:
    case OB_POINTCLOUD:
    case OB_VOLUME: {
      break;
    }
  }
}

static void stats_object_edit(Object *obedit, SceneStats *stats)
{
  if (obedit->type == OB_MESH) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    stats->totvert += em->bm->totvert;
    stats->totvertsel += em->bm->totvertsel;

    stats->totedge += em->bm->totedge;
    stats->totedgesel += em->bm->totedgesel;

    stats->totface += em->bm->totface;
    stats->totfacesel += em->bm->totfacesel;

    stats->tottri += em->tottri;
  }
  else if (obedit->type == OB_ARMATURE) {
    /* Armature Edit */
    bArmature *arm = static_cast<bArmature *>(obedit->data);

    LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
      stats->totbone++;

      if ((ebo->flag & BONE_CONNECTED) && ebo->parent) {
        stats->totvert--;
      }

      if (ebo->flag & BONE_TIPSEL) {
        stats->totvertsel++;
      }
      if (ebo->flag & BONE_ROOTSEL) {
        stats->totvertsel++;
      }

      if (ebo->flag & BONE_SELECTED) {
        stats->totbonesel++;
      }

      /* if this is a connected child and its parent is being moved, remove our root */
      if ((ebo->flag & BONE_CONNECTED) && (ebo->flag & BONE_ROOTSEL) && ebo->parent &&
          (ebo->parent->flag & BONE_TIPSEL)) {
        stats->totvertsel--;
      }

      stats->totvert += 2;
    }
  }
  else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) { /* OB_FONT has no cu->editnurb */
    /* Curve Edit */
    Curve *cu = static_cast<Curve *>(obedit->data);
    BezTriple *bezt;
    BPoint *bp;
    int a;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);

    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          stats->totvert += 3;
          if (bezt->f1 & SELECT) {
            stats->totvertsel++;
          }
          if (bezt->f2 & SELECT) {
            stats->totvertsel++;
          }
          if (bezt->f3 & SELECT) {
            stats->totvertsel++;
          }
          bezt++;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          stats->totvert++;
          if (bp->f1 & SELECT) {
            stats->totvertsel++;
          }
          bp++;
        }
      }
    }
  }
  else if (obedit->type == OB_MBALL) {
    /* MetaBall Edit */
    MetaBall *mball = static_cast<MetaBall *>(obedit->data);

    LISTBASE_FOREACH (MetaElem *, ml, mball->editelems) {
      stats->totvert++;
      if (ml->flag & SELECT) {
        stats->totvertsel++;
      }
    }
  }
  else if (obedit->type == OB_LATTICE) {
    /* Lattice Edit */
    Lattice *lt = static_cast<Lattice *>(obedit->data);
    Lattice *editlatt = lt->editlatt->latt;
    BPoint *bp;
    int a;

    bp = editlatt->def;

    a = editlatt->pntsu * editlatt->pntsv * editlatt->pntsw;
    while (a--) {
      stats->totvert++;
      if (bp->f1 & SELECT) {
        stats->totvertsel++;
      }
      bp++;
    }
  }
}

static void stats_object_pose(const Object *ob, SceneStats *stats)
{
  if (ob->pose) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      stats->totbone++;
      if (pchan->bone && (pchan->bone->flag & BONE_SELECTED)) {
        if (BKE_pose_is_layer_visible(arm, pchan)) {
          stats->totbonesel++;
        }
      }
    }
  }
}

static bool stats_is_object_dynamic_topology_sculpt(const Object *ob)
{
  BLI_assert(ob->mode & OB_MODE_SCULPT);
  return (ob->sculpt && ob->sculpt->bm);
}

static void stats_object_sculpt(const Object *ob, SceneStats *stats)
{

  SculptSession *ss = ob->sculpt;

  if (ss == nullptr || ss->pbvh == nullptr) {
    return;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      stats->totvertsculpt = ss->totvert;
      stats->totfacesculpt = ss->totfaces;
      break;
    case PBVH_BMESH:
      stats->totvertsculpt = ob->sculpt->bm->totvert;
      stats->tottri = ob->sculpt->bm->totface;
      break;
    case PBVH_GRIDS:
      stats->totvertsculpt = BKE_pbvh_get_grid_num_vertices(ss->pbvh);
      stats->totfacesculpt = BKE_pbvh_get_grid_num_faces(ss->pbvh);
      break;
  }
}

/* Statistics displayed in info header. Called regularly on scene changes. */
static void stats_update(Depsgraph *depsgraph,
                         ViewLayer *view_layer,
                         View3D *v3d_local,
                         SceneStats *stats)
{
  const Object *ob = OBACT(view_layer);
  const Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);

  memset(stats, 0x0, sizeof(*stats));

  if (obedit) {
    /* Edit Mode. */
    FOREACH_OBJECT_BEGIN (view_layer, ob_iter) {
      if (ob_iter->base_flag & BASE_VISIBLE_VIEWLAYER) {
        if (ob_iter->mode & OB_MODE_EDIT) {
          stats_object_edit(ob_iter, stats);
          stats->totobjsel++;
        }
        else {
          /* Skip hidden objects in local view that are not in edit-mode,
           * an exception for edit-mode, in most other modes these would be considered hidden. */
          if ((v3d_local && !BKE_object_is_visible_in_viewport(v3d_local, ob_iter))) {
            continue;
          }
        }
        stats->totobj++;
      }
    }
    FOREACH_OBJECT_END;
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    /* Pose Mode. */
    FOREACH_OBJECT_BEGIN (view_layer, ob_iter) {
      if (ob_iter->base_flag & BASE_VISIBLE_VIEWLAYER) {
        if (ob_iter->mode & OB_MODE_POSE) {
          stats_object_pose(ob_iter, stats);
          stats->totobjsel++;
        }
        else {
          /* See comment for edit-mode. */
          if ((v3d_local && !BKE_object_is_visible_in_viewport(v3d_local, ob_iter))) {
            continue;
          }
        }
        stats->totobj++;
      }
    }
    FOREACH_OBJECT_END;
  }
  else if (ob && (ob->mode & OB_MODE_SCULPT)) {
    /* Sculpt Mode. */
    if (stats_is_object_dynamic_topology_sculpt(ob)) {
      /* Dynamic topology. Do not count all vertices,
       * dynamic topology stats are initialized later as part of sculpt stats. */
    }
    else {
      /* When dynamic topology is not enabled both sculpt stats and scene stats are collected. */
      stats_object_sculpt(ob, stats);
    }
  }
  else {
    /* Objects. */
    GSet *objects_gset = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob_iter) {
      stats_object(ob_iter, v3d_local, stats, objects_gset);
    }
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;
    BLI_gset_free(objects_gset, nullptr);
  }
}

void ED_info_stats_clear(wmWindowManager *wm, ViewLayer *view_layer)
{
  MEM_SAFE_FREE(view_layer->stats);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    ViewLayer *view_layer_test = WM_window_get_active_view_layer(win);
    if (view_layer != view_layer_test) {
      continue;
    }
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)area->spacedata.first;
        if (v3d->localvd) {
          MEM_SAFE_FREE(v3d->runtime.local_stats);
        }
      }
    }
  }
}

static bool format_stats(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d_local, SceneStatsFmt *stats_fmt)
{
  /* Create stats if they don't already exist. */
  SceneStats **stats_p = (v3d_local) ? &v3d_local->runtime.local_stats : &view_layer->stats;
  if (*stats_p == nullptr) {
    /* Don't access dependency graph if interface is marked as locked. */
    wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
    if (wm->is_interface_locked) {
      return false;
    }
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    *stats_p = (SceneStats *)MEM_mallocN(sizeof(SceneStats), __func__);
    stats_update(depsgraph, view_layer, v3d_local, *stats_p);
  }

  SceneStats *stats = *stats_p;

  /* Generate formatted numbers. */
#define SCENE_STATS_FMT_INT(_id) BLI_str_format_uint64_grouped(stats_fmt->_id, stats->_id)

  SCENE_STATS_FMT_INT(totvert);
  SCENE_STATS_FMT_INT(totvertsel);
  SCENE_STATS_FMT_INT(totvertsculpt);

  SCENE_STATS_FMT_INT(totedge);
  SCENE_STATS_FMT_INT(totedgesel);

  SCENE_STATS_FMT_INT(totface);
  SCENE_STATS_FMT_INT(totfacesel);
  SCENE_STATS_FMT_INT(totfacesculpt);

  SCENE_STATS_FMT_INT(totbone);
  SCENE_STATS_FMT_INT(totbonesel);

  SCENE_STATS_FMT_INT(totobj);
  SCENE_STATS_FMT_INT(totobjsel);

  SCENE_STATS_FMT_INT(totlamp);
  SCENE_STATS_FMT_INT(totlampsel);

  SCENE_STATS_FMT_INT(tottri);

  SCENE_STATS_FMT_INT(totgplayer);
  SCENE_STATS_FMT_INT(totgpframe);
  SCENE_STATS_FMT_INT(totgpstroke);
  SCENE_STATS_FMT_INT(totgppoint);

#undef SCENE_STATS_FMT_INT
  return true;
}

static void get_stats_string(
    char *info, int len, size_t *ofs, ViewLayer *view_layer, SceneStatsFmt *stats_fmt)
{
  Object *ob = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  eObjectMode object_mode = ob ? (eObjectMode)ob->mode : OB_MODE_OBJECT;
  LayerCollection *layer_collection = view_layer->active_collection;

  if (object_mode == OB_MODE_OBJECT) {
    *ofs += BLI_snprintf_rlen(info + *ofs,
                              len - *ofs,
                              "%s | ",
                              BKE_collection_ui_name_get(layer_collection->collection));
  }

  if (ob) {
    *ofs += BLI_snprintf_rlen(info + *ofs, len - *ofs, "%s | ", ob->id.name + 2);
  }

  if (obedit) {
    if (BKE_keyblock_from_object(obedit)) {
      *ofs += BLI_strncpy_rlen(info + *ofs, TIP_("(Key) "), len - *ofs);
    }

    if (obedit->type == OB_MESH) {
      *ofs += BLI_snprintf_rlen(info + *ofs,
                                len - *ofs,
                                TIP_("Verts:%s/%s | Edges:%s/%s | Faces:%s/%s | Tris:%s"),
                                stats_fmt->totvertsel,
                                stats_fmt->totvert,
                                stats_fmt->totedgesel,
                                stats_fmt->totedge,
                                stats_fmt->totfacesel,
                                stats_fmt->totface,
                                stats_fmt->tottri);
    }
    else if (obedit->type == OB_ARMATURE) {
      *ofs += BLI_snprintf_rlen(info + *ofs,
                                len - *ofs,
                                TIP_("Joints:%s/%s | Bones:%s/%s"),
                                stats_fmt->totvertsel,
                                stats_fmt->totvert,
                                stats_fmt->totbonesel,
                                stats_fmt->totbone);
    }
    else {
      *ofs += BLI_snprintf_rlen(
          info + *ofs, len - *ofs, TIP_("Verts:%s/%s"), stats_fmt->totvertsel, stats_fmt->totvert);
    }
  }
  else if (ob && (object_mode & OB_MODE_POSE)) {
    *ofs += BLI_snprintf_rlen(
        info + *ofs, len - *ofs, TIP_("Bones:%s/%s"), stats_fmt->totbonesel, stats_fmt->totbone);
  }
  else if ((ob) && (ob->type == OB_GPENCIL)) {
    *ofs += BLI_snprintf_rlen(info + *ofs,
                              len - *ofs,
                              TIP_("Layers:%s | Frames:%s | Strokes:%s | Points:%s"),
                              stats_fmt->totgplayer,
                              stats_fmt->totgpframe,
                              stats_fmt->totgpstroke,
                              stats_fmt->totgppoint);
  }
  else if (ob && (object_mode & OB_MODE_SCULPT)) {
    if (stats_is_object_dynamic_topology_sculpt(ob)) {
      *ofs += BLI_snprintf_rlen(info + *ofs,
                                len - *ofs,
                                TIP_("Verts:%s | Tris:%s"),
                                stats_fmt->totvert,
                                stats_fmt->tottri);
    }
    else {
      *ofs += BLI_snprintf_rlen(info + *ofs,
                                len - *ofs,
                                TIP_("Verts:%s/%s | Faces:%s/%s"),
                                stats_fmt->totvertsculpt,
                                stats_fmt->totvert,
                                stats_fmt->totfacesculpt,
                                stats_fmt->totface);
    }
  }
  else {
    *ofs += BLI_snprintf_rlen(info + *ofs,
                              len - *ofs,
                              TIP_("Verts:%s | Faces:%s | Tris:%s"),
                              stats_fmt->totvert,
                              stats_fmt->totface,
                              stats_fmt->tottri);
  }

  *ofs += BLI_snprintf_rlen(
      info + *ofs, len - *ofs, TIP_(" | Objects:%s/%s"), stats_fmt->totobjsel, stats_fmt->totobj);
}

static const char *info_statusbar_string(Main *bmain,
                                         Scene *scene,
                                         ViewLayer *view_layer,
                                         char statusbar_flag)
{
  char formatted_mem[15];
  size_t ofs = 0;
  static char info[256];
  int len = sizeof(info);

  info[0] = '\0';

  /* Scene statistics. */
  if (statusbar_flag & STATUSBAR_SHOW_STATS) {
    SceneStatsFmt stats_fmt;
    if (format_stats(bmain, scene, view_layer, nullptr, &stats_fmt)) {
      get_stats_string(info + ofs, len, &ofs, view_layer, &stats_fmt);
    }
  }

  /* Memory status. */
  if (statusbar_flag & STATUSBAR_SHOW_MEMORY) {
    if (info[0]) {
      ofs += BLI_snprintf_rlen(info + ofs, len - ofs, " | ");
    }
    uintptr_t mem_in_use = MEM_get_memory_in_use();
    BLI_str_format_byte_unit(formatted_mem, mem_in_use, false);
    ofs += BLI_snprintf_rlen(info + ofs, len, TIP_("Memory: %s"), formatted_mem);
  }

  /* GPU VRAM status. */
  if ((statusbar_flag & STATUSBAR_SHOW_VRAM) && (GPU_mem_stats_supported())) {
    int gpu_free_mem_kb, gpu_tot_mem_kb;
    GPU_mem_stats_get(&gpu_tot_mem_kb, &gpu_free_mem_kb);
    float gpu_total_gb = gpu_tot_mem_kb / 1048576.0f;
    float gpu_free_gb = gpu_free_mem_kb / 1048576.0f;
    if (info[0]) {
      ofs += BLI_snprintf_rlen(info + ofs, len - ofs, " | ");
    }
    if (gpu_free_mem_kb && gpu_tot_mem_kb) {
      ofs += BLI_snprintf_rlen(info + ofs,
                               len - ofs,
                               TIP_("VRAM: %.1f/%.1f GiB"),
                               gpu_total_gb - gpu_free_gb,
                               gpu_total_gb);
    }
    else {
      /* Can only show amount of GPU VRAM available. */
      ofs += BLI_snprintf_rlen(info + ofs, len - ofs, TIP_("VRAM: %.1f GiB Free"), gpu_free_gb);
    }
  }

  /* Blender version. */
  if (statusbar_flag & STATUSBAR_SHOW_VERSION) {
    if (info[0]) {
      ofs += BLI_snprintf_rlen(info + ofs, len - ofs, " | ");
    }
    ofs += BLI_snprintf_rlen(info + ofs, len - ofs, TIP_("%s"), BKE_blender_version_string());
  }

  return info;
}

const char *ED_info_statusbar_string(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  return info_statusbar_string(bmain, scene, view_layer, U.statusbar_flag);
}

const char *ED_info_statistics_string(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  const eUserpref_StatusBar_Flag statistics_status_bar_flag = STATUSBAR_SHOW_STATS |
                                                              STATUSBAR_SHOW_MEMORY |
                                                              STATUSBAR_SHOW_VERSION;

  return info_statusbar_string(bmain, scene, view_layer, statistics_status_bar_flag);
}

static void stats_row(int col1,
                      const char *key,
                      int col2,
                      const char *value1,
                      const char *value2,
                      int *y,
                      int height)
{
  *y -= height;
  BLF_draw_default(col1, *y, 0.0f, key, 128);
  char values[128];
  BLI_snprintf(values, sizeof(values), (value2) ? "%s / %s" : "%s", value1, value2);
  BLF_draw_default(col2, *y, 0.0f, values, sizeof(values));
}

void ED_info_draw_stats(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d_local, int x, int *y, int height)
{
  BLI_assert(v3d_local == nullptr || v3d_local->localvd != nullptr);
  SceneStatsFmt stats_fmt;
  if (!format_stats(bmain, scene, view_layer, v3d_local, &stats_fmt)) {
    return;
  }

  Object *ob = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  eObjectMode object_mode = ob ? (eObjectMode)ob->mode : OB_MODE_OBJECT;
  const int font_id = BLF_set_default();

  UI_FontThemeColor(font_id, TH_TEXT_HI);
  BLF_enable(font_id, BLF_SHADOW);
  const float shadow_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  BLF_shadow(font_id, 5, shadow_color);
  BLF_shadow_offset(font_id, 1, -1);

  /* Translated labels for each stat row. */
  enum {
    OBJ,
    VERTS,
    EDGES,
    FACES,
    TRIS,
    JOINTS,
    BONES,
    LAYERS,
    FRAMES,
    STROKES,
    POINTS,
    LIGHTS,
    MAX_LABELS_COUNT
  };
  char labels[MAX_LABELS_COUNT][64];

  STRNCPY(labels[OBJ], IFACE_("Objects"));
  STRNCPY(labels[VERTS], IFACE_("Vertices"));
  STRNCPY(labels[EDGES], IFACE_("Edges"));
  STRNCPY(labels[FACES], IFACE_("Faces"));
  STRNCPY(labels[TRIS], IFACE_("Triangles"));
  STRNCPY(labels[JOINTS], IFACE_("Joints"));
  STRNCPY(labels[BONES], IFACE_("Bones"));
  STRNCPY(labels[LAYERS], IFACE_("Layers"));
  STRNCPY(labels[FRAMES], IFACE_("Frames"));
  STRNCPY(labels[STROKES], IFACE_("Strokes"));
  STRNCPY(labels[POINTS], IFACE_("Points"));
  STRNCPY(labels[LIGHTS], IFACE_("Lights"));

  int longest_label = 0;
  int i;
  for (i = 0; i < MAX_LABELS_COUNT; ++i) {
    longest_label = max_ii(longest_label, BLF_width(font_id, labels[i], sizeof(labels[i])));
  }

  int col1 = x;
  int col2 = x + longest_label + (0.5f * U.widget_unit);

  /* Add some extra margin above this section. */
  *y -= (0.6f * height);

  if (object_mode == OB_MODE_OBJECT) {
    stats_row(col1, labels[OBJ], col2, stats_fmt.totobjsel, stats_fmt.totobj, y, height);
  }

  if (obedit) {
    stats_row(col1, labels[OBJ], col2, stats_fmt.totobjsel, stats_fmt.totobj, y, height);
    if (obedit->type == OB_MESH) {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsel, stats_fmt.totvert, y, height);
      stats_row(col1, labels[EDGES], col2, stats_fmt.totedgesel, stats_fmt.totedge, y, height);
      stats_row(col1, labels[FACES], col2, stats_fmt.totfacesel, stats_fmt.totface, y, height);
      stats_row(col1, labels[TRIS], col2, stats_fmt.tottri, nullptr, y, height);
    }
    else if (obedit->type == OB_ARMATURE) {
      stats_row(col1, labels[JOINTS], col2, stats_fmt.totvertsel, stats_fmt.totvert, y, height);
      stats_row(col1, labels[BONES], col2, stats_fmt.totbonesel, stats_fmt.totbone, y, height);
    }
    else {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsel, stats_fmt.totvert, y, height);
    }
  }
  else if (ob && (object_mode & OB_MODE_POSE)) {
    stats_row(col1, labels[OBJ], col2, stats_fmt.totobjsel, stats_fmt.totobj, y, height);
    stats_row(col1, labels[BONES], col2, stats_fmt.totbonesel, stats_fmt.totbone, y, height);
  }
  else if ((ob) && (ob->type == OB_GPENCIL)) {
    stats_row(col1, labels[LAYERS], col2, stats_fmt.totgplayer, nullptr, y, height);
    stats_row(col1, labels[FRAMES], col2, stats_fmt.totgpframe, nullptr, y, height);
    stats_row(col1, labels[STROKES], col2, stats_fmt.totgpstroke, nullptr, y, height);
    stats_row(col1, labels[POINTS], col2, stats_fmt.totgppoint, nullptr, y, height);
  }
  else if (ob && (object_mode & OB_MODE_SCULPT)) {
    if (stats_is_object_dynamic_topology_sculpt(ob)) {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsculpt, nullptr, y, height);
      stats_row(col1, labels[TRIS], col2, stats_fmt.tottri, nullptr, y, height);
    }
    else {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsculpt, stats_fmt.totvert, y, height);
      stats_row(col1, labels[FACES], col2, stats_fmt.totfacesculpt, stats_fmt.totface, y, height);
    }
  }
  else if ((ob) && (ob->type == OB_LAMP)) {
    stats_row(col1, labels[LIGHTS], col2, stats_fmt.totlampsel, stats_fmt.totlamp, y, height);
  }
  else {
    stats_row(col1, labels[VERTS], col2, stats_fmt.totvert, nullptr, y, height);
    stats_row(col1, labels[EDGES], col2, stats_fmt.totedge, nullptr, y, height);
    stats_row(col1, labels[FACES], col2, stats_fmt.totface, nullptr, y, height);
    stats_row(col1, labels[TRIS], col2, stats_fmt.tottri, nullptr, y, height);
  }

  BLF_disable(font_id, BLF_SHADOW);
}
