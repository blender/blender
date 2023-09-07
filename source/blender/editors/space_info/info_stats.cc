/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLF_api.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_modifier.hh"

#include "DEG_depsgraph_query.h"

#include "ED_info.hh"

#include "WM_api.hh"

#include "UI_resources.hh"

#include "GPU_capabilities.h"

ENUM_OPERATORS(eUserpref_StatusBar_Flag, STATUSBAR_SHOW_VERSION)

struct SceneStats {
  uint64_t totvert, totvertsel, totvertsculpt;
  uint64_t totedge, totedgesel;
  uint64_t totface, totfacesel, totfacesculpt;
  uint64_t totbone, totbonesel;
  uint64_t totobj, totobjsel;
  uint64_t totlamp, totlampsel;
  uint64_t tottri, tottrisel;
  uint64_t totgplayer, totgpframe, totgpstroke, totgppoint;
};

struct SceneStatsFmt {
  /* Totals */
  char totvert[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totvertsel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE],
      totvertsculpt[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totface[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totfacesel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totedge[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totedgesel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE],
      totfacesculpt[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totbone[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totbonesel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totobj[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totobjsel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totlamp[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], totlampsel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char tottri[BLI_STR_FORMAT_UINT64_GROUPED_SIZE], tottrisel[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totgplayer[BLI_STR_FORMAT_UINT64_GROUPED_SIZE],
      totgpframe[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
  char totgpstroke[BLI_STR_FORMAT_UINT64_GROUPED_SIZE],
      totgppoint[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
};

static bool stats_mesheval(const Mesh *me_eval, bool is_selected, SceneStats *stats)
{
  if (me_eval == nullptr) {
    return false;
  }

  int totvert, totedge, totface, totloop;

  const SubdivCCG *subdiv_ccg = me_eval->runtime->subdiv_ccg;
  const SubsurfRuntimeData *subsurf_runtime_data = me_eval->runtime->subsurf_runtime_data;

  if (subdiv_ccg != nullptr) {
    BKE_subdiv_ccg_topology_counters(subdiv_ccg, &totvert, &totedge, &totface, &totloop);
  }
  else if (subsurf_runtime_data && subsurf_runtime_data->resolution != 0) {
    totvert = subsurf_runtime_data->stats_totvert;
    totedge = subsurf_runtime_data->stats_totedge;
    totface = subsurf_runtime_data->stats_faces_num;
    totloop = subsurf_runtime_data->stats_totloop;
  }
  else {
    totvert = me_eval->totvert;
    totedge = me_eval->totedge;
    totface = me_eval->faces_num;
    totloop = me_eval->totloop;
  }

  stats->totvert += totvert;
  stats->totedge += totedge;
  stats->totface += totface;

  const int tottri = poly_to_tri_count(totface, totloop);
  stats->tottri += tottri;

  if (is_selected) {
    stats->totvertsel += totvert;
    stats->totedgesel += totedge;
    stats->totfacesel += totface;
    stats->tottrisel += tottri;
  }
  return true;
}

static void stats_object(Object *ob,
                         const View3D *v3d_local,
                         SceneStats *stats,
                         GSet *objects_gset)
{
  if ((ob->base_flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0) {
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
      const Mesh *me_eval = BKE_object_get_evaluated_mesh_no_subsurf(ob);
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
    case OB_GPENCIL_LEGACY: {
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
    case OB_CURVES:
    case OB_POINTCLOUD:
    case OB_VOLUME:
    case OB_GREASE_PENCIL: {
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
          (ebo->parent->flag & BONE_TIPSEL))
      {
        stats->totvertsel--;
      }

      stats->totvert += 2;
    }
  }
  else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) { /* OB_FONT has no cu->editnurb */
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
        if (BKE_pose_is_bonecoll_visible(arm, pchan)) {
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
      if (ob->sculpt->bm) {
        stats->totvertsculpt = ob->sculpt->bm->totvert;
        stats->tottri = ob->sculpt->bm->totface;
      }
      break;
    case PBVH_GRIDS:
      stats->totvertsculpt = BKE_pbvh_get_grid_num_verts(ss->pbvh);
      stats->totfacesculpt = BKE_pbvh_get_grid_num_faces(ss->pbvh);
      break;
  }
}

/* Statistics displayed in info header. Called regularly on scene changes. */
static void stats_update(Depsgraph *depsgraph,
                         const Scene *scene,
                         ViewLayer *view_layer,
                         View3D *v3d_local,
                         SceneStats *stats)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Object *ob = BKE_view_layer_active_object_get(view_layer);
  const Object *obedit = BKE_view_layer_edit_object_get(view_layer);

  memset(stats, 0x0, sizeof(*stats));

  if (obedit) {
    /* Edit Mode. */
    FOREACH_OBJECT_BEGIN (scene, view_layer, ob_iter) {
      if (ob_iter->base_flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) {
        if (ob_iter->mode & OB_MODE_EDIT) {
          stats_object_edit(ob_iter, stats);
          stats->totobjsel++;
        }
        else {
          /* Skip hidden objects in local view that are not in edit-mode,
           * an exception for edit-mode, in most other modes these would be considered hidden. */
          if (v3d_local && !BKE_object_is_visible_in_viewport(v3d_local, ob_iter)) {
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
    FOREACH_OBJECT_BEGIN (scene, view_layer, ob_iter) {
      if (ob_iter->base_flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) {
        if (ob_iter->mode & OB_MODE_POSE) {
          stats_object_pose(ob_iter, stats);
          stats->totobjsel++;
        }
        else {
          /* See comment for edit-mode. */
          if (v3d_local && !BKE_object_is_visible_in_viewport(v3d_local, ob_iter)) {
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
    stats_object_sculpt(ob, stats);
  }
  else {
    /* Objects. */
    GSet *objects_gset = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
    DEGObjectIterSettings deg_iter_settings{};
    deg_iter_settings.depsgraph = depsgraph;
    deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
    DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob_iter) {
      stats_object(ob_iter, v3d_local, stats, objects_gset);
    }
    DEG_OBJECT_ITER_END;
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
    stats_update(depsgraph, scene, view_layer, v3d_local, *stats_p);
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
  SCENE_STATS_FMT_INT(tottrisel);

  SCENE_STATS_FMT_INT(totgplayer);
  SCENE_STATS_FMT_INT(totgpframe);
  SCENE_STATS_FMT_INT(totgpstroke);
  SCENE_STATS_FMT_INT(totgppoint);

#undef SCENE_STATS_FMT_INT
  return true;
}

static void get_stats_string(char *info,
                             int len,
                             size_t *ofs,
                             const Scene *scene,
                             ViewLayer *view_layer,
                             SceneStatsFmt *stats_fmt)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  eObjectMode object_mode = ob ? (eObjectMode)ob->mode : OB_MODE_OBJECT;
  LayerCollection *layer_collection = BKE_view_layer_active_collection_get(view_layer);

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
  else if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
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
                                stats_fmt->totvertsculpt,
                                stats_fmt->tottri);
    }
    else {
      *ofs += BLI_snprintf_rlen(info + *ofs,
                                len - *ofs,
                                TIP_("Verts:%s | Faces:%s"),
                                stats_fmt->totvertsculpt,
                                stats_fmt->totfacesculpt);
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

  if (!STREQ(&stats_fmt->totobj[0], "0")) {
    *ofs += BLI_snprintf_rlen(info + *ofs,
                              len - *ofs,
                              TIP_(" | Objects:%s/%s"),
                              stats_fmt->totobjsel,
                              stats_fmt->totobj);
  }
}

const char *ED_info_statusbar_string_ex(Main *bmain,
                                        Scene *scene,
                                        ViewLayer *view_layer,
                                        const char statusbar_flag)
{
  char formatted_mem[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
  size_t ofs = 0;
  static char info[256];
  int len = sizeof(info);

  info[0] = '\0';

  /* Scene statistics. */
  if (statusbar_flag & STATUSBAR_SHOW_STATS) {
    SceneStatsFmt stats_fmt;
    if (format_stats(bmain, scene, view_layer, nullptr, &stats_fmt)) {
      get_stats_string(info + ofs, len, &ofs, scene, view_layer, &stats_fmt);
    }
  }

  /* Scene Duration. */
  if (statusbar_flag & STATUSBAR_SHOW_SCENE_DURATION) {
    if (info[0]) {
      ofs += BLI_snprintf_rlen(info + ofs, len - ofs, " | ");
    }
    const int relative_current_frame = (scene->r.cfra - scene->r.sfra) + 1;
    const int frame_count = (scene->r.efra - scene->r.sfra) + 1;
    char timecode[32];
    BLI_timecode_string_from_time(
        timecode, sizeof(timecode), -2, FRA2TIME(frame_count), FPS, U.timecode_style);
    ofs += BLI_snprintf_rlen(info + ofs,
                             len - ofs,
                             TIP_("Duration: %s (Frame %i/%i)"),
                             timecode,
                             relative_current_frame,
                             frame_count);
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
  if ((statusbar_flag & STATUSBAR_SHOW_VRAM) && GPU_mem_stats_supported()) {
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
  return ED_info_statusbar_string_ex(bmain, scene, view_layer, U.statusbar_flag);
}

const char *ED_info_statistics_string(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  const eUserpref_StatusBar_Flag statistics_status_bar_flag = STATUSBAR_SHOW_STATS |
                                                              STATUSBAR_SHOW_MEMORY |
                                                              STATUSBAR_SHOW_VERSION |
                                                              STATUSBAR_SHOW_SCENE_DURATION;

  return ED_info_statusbar_string_ex(bmain, scene, view_layer, statistics_status_bar_flag);
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
  SNPRINTF(values, (value2) ? "%s / %s" : "%s", value1, value2);
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

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
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

  STRNCPY_UTF8(labels[OBJ], IFACE_("Objects"));
  STRNCPY_UTF8(labels[VERTS], IFACE_("Vertices"));
  STRNCPY_UTF8(labels[EDGES], IFACE_("Edges"));
  STRNCPY_UTF8(labels[FACES], IFACE_("Faces"));
  STRNCPY_UTF8(labels[TRIS], IFACE_("Triangles"));
  STRNCPY_UTF8(labels[JOINTS], IFACE_("Joints"));
  STRNCPY_UTF8(labels[BONES], IFACE_("Bones"));
  STRNCPY_UTF8(labels[LAYERS], IFACE_("Layers"));
  STRNCPY_UTF8(labels[FRAMES], IFACE_("Frames"));
  STRNCPY_UTF8(labels[STROKES], IFACE_("Strokes"));
  STRNCPY_UTF8(labels[POINTS], IFACE_("Points"));
  STRNCPY_UTF8(labels[LIGHTS], IFACE_("Lights"));

  int longest_label = 0;
  for (int i = 0; i < MAX_LABELS_COUNT; ++i) {
    longest_label = max_ii(longest_label, BLF_width(font_id, labels[i], sizeof(labels[i])));
  }

  int col1 = x;
  int col2 = x + longest_label + (0.5f * U.widget_unit);

  /* Add some extra margin above this section. */
  *y -= (0.6f * height);

  bool any_objects = !STREQ(&stats_fmt.totobj[0], "0");
  bool any_selected = !STREQ(&stats_fmt.totobjsel[0], "0");

  if (any_selected) {
    stats_row(col1, labels[OBJ], col2, stats_fmt.totobjsel, stats_fmt.totobj, y, height);
  }
  else if (any_objects) {
    stats_row(col1, labels[OBJ], col2, stats_fmt.totobj, nullptr, y, height);
  }

  if (obedit) {
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
    else if (obedit->type != OB_FONT) {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsel, stats_fmt.totvert, y, height);
    }
  }
  else if (ob && (object_mode & OB_MODE_SCULPT)) {
    if (stats_is_object_dynamic_topology_sculpt(ob)) {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsculpt, nullptr, y, height);
      stats_row(col1, labels[TRIS], col2, stats_fmt.tottri, nullptr, y, height);
    }
    else {
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsculpt, nullptr, y, height);
      stats_row(col1, labels[FACES], col2, stats_fmt.totfacesculpt, nullptr, y, height);
    }
  }
  else if (!any_selected) {
    if (any_objects) {
      /* Show scene totals if nothing is selected. */
      stats_row(col1, labels[VERTS], col2, stats_fmt.totvert, nullptr, y, height);
      stats_row(col1, labels[EDGES], col2, stats_fmt.totedge, nullptr, y, height);
      stats_row(col1, labels[FACES], col2, stats_fmt.totface, nullptr, y, height);
      stats_row(col1, labels[TRIS], col2, stats_fmt.tottri, nullptr, y, height);
    }
    else {
      /* No objects in scene. */
      stats_row(col1, labels[OBJ], col2, stats_fmt.totobj, nullptr, y, height);
    }
  }
  else if (ob && (object_mode & OB_MODE_POSE)) {
    stats_row(col1, labels[BONES], col2, stats_fmt.totbonesel, stats_fmt.totbone, y, height);
  }
  else if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    stats_row(col1, labels[LAYERS], col2, stats_fmt.totgplayer, nullptr, y, height);
    stats_row(col1, labels[FRAMES], col2, stats_fmt.totgpframe, nullptr, y, height);
    stats_row(col1, labels[STROKES], col2, stats_fmt.totgpstroke, nullptr, y, height);
    stats_row(col1, labels[POINTS], col2, stats_fmt.totgppoint, nullptr, y, height);
  }
  else if ((ob) && (ob->type == OB_LAMP)) {
    stats_row(col1, labels[LIGHTS], col2, stats_fmt.totlampsel, stats_fmt.totlamp, y, height);
  }
  else if ((object_mode == OB_MODE_OBJECT) && ob && (ELEM(ob->type, OB_MESH, OB_FONT))) {
    /* Object mode with the active object a mesh or text object. */
    stats_row(col1, labels[VERTS], col2, stats_fmt.totvertsel, stats_fmt.totvert, y, height);
    stats_row(col1, labels[EDGES], col2, stats_fmt.totedgesel, stats_fmt.totedge, y, height);
    stats_row(col1, labels[FACES], col2, stats_fmt.totfacesel, stats_fmt.totface, y, height);
    stats_row(col1, labels[TRIS], col2, stats_fmt.tottrisel, stats_fmt.tottri, y, height);
  }

  BLF_disable(font_id, BLF_SHADOW);
}
