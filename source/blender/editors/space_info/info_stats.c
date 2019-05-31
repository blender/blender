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

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim.h"
#include "BKE_blender_version.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_gpencil.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph_query.h"

#include "ED_info.h"
#include "ED_armature.h"

#include "GPU_extensions.h"

#define MAX_INFO_LEN 512
#define MAX_INFO_NUM_LEN 16

typedef struct SceneStats {
  uint64_t totvert, totvertsel;
  uint64_t totedge, totedgesel;
  uint64_t totface, totfacesel;
  uint64_t totbone, totbonesel;
  uint64_t totobj, totobjsel;
  uint64_t totlamp, totlampsel;
  uint64_t tottri;
  uint64_t totgplayer, totgpframe, totgpstroke, totgppoint;

  char infostr[MAX_INFO_LEN];
} SceneStats;

typedef struct SceneStatsFmt {
  /* Totals */
  char totvert[MAX_INFO_NUM_LEN], totvertsel[MAX_INFO_NUM_LEN];
  char totface[MAX_INFO_NUM_LEN], totfacesel[MAX_INFO_NUM_LEN];
  char totedge[MAX_INFO_NUM_LEN], totedgesel[MAX_INFO_NUM_LEN];
  char totbone[MAX_INFO_NUM_LEN], totbonesel[MAX_INFO_NUM_LEN];
  char totobj[MAX_INFO_NUM_LEN], totobjsel[MAX_INFO_NUM_LEN];
  char totlamp[MAX_INFO_NUM_LEN], totlampsel[MAX_INFO_NUM_LEN];
  char tottri[MAX_INFO_NUM_LEN];
  char totgplayer[MAX_INFO_NUM_LEN], totgpframe[MAX_INFO_NUM_LEN];
  char totgpstroke[MAX_INFO_NUM_LEN], totgppoint[MAX_INFO_NUM_LEN];
} SceneStatsFmt;

static bool stats_mesheval(Mesh *me_eval, bool is_selected, SceneStats *stats)
{
  if (me_eval == NULL) {
    return false;
  }

  int totvert, totedge, totface, totloop;
  if (me_eval->runtime.subdiv_ccg != NULL) {
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

static void stats_object(Object *ob, SceneStats *stats, GSet *objects_gset)
{
  const bool is_selected = (ob->base_flag & BASE_SELECTED) != 0;

  stats->totobj++;
  if (is_selected) {
    stats->totobjsel++;
  }

  switch (ob->type) {
    case OB_MESH: {
      /* we assume evaluated mesh is already built, this strictly does stats now. */
      Mesh *me_eval = ob->runtime.mesh_eval;
      if (!BLI_gset_add(objects_gset, me_eval)) {
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
      Mesh *me_eval = ob->runtime.mesh_eval;

      if ((me_eval != NULL) && !BLI_gset_add(objects_gset, me_eval)) {
        break;
      }

      if (stats_mesheval(me_eval, is_selected, stats)) {
        break;
      }
      ATTR_FALLTHROUGH; /* Falltrough to displist. */
    }
    case OB_MBALL: {
      int totv = 0, totf = 0, tottri = 0;

      if (ob->runtime.curve_cache && ob->runtime.curve_cache->disp.first) {
        /* Note: We only get the same curve_cache for instances of the same curve/font/...
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
    bArmature *arm = obedit->data;
    EditBone *ebo;

    for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
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

      /* if this is a connected child and it's parent is being moved, remove our root */
      if ((ebo->flag & BONE_CONNECTED) && (ebo->flag & BONE_ROOTSEL) && ebo->parent &&
          (ebo->parent->flag & BONE_TIPSEL)) {
        stats->totvertsel--;
      }

      stats->totvert += 2;
    }
  }
  else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) { /* OB_FONT has no cu->editnurb */
    /* Curve Edit */
    Curve *cu = obedit->data;
    Nurb *nu;
    BezTriple *bezt;
    BPoint *bp;
    int a;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);

    for (nu = nurbs->first; nu; nu = nu->next) {
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
    MetaBall *mball = obedit->data;
    MetaElem *ml;

    for (ml = mball->editelems->first; ml; ml = ml->next) {
      stats->totvert++;
      if (ml->flag & SELECT) {
        stats->totvertsel++;
      }
    }
  }
  else if (obedit->type == OB_LATTICE) {
    /* Lattice Edit */
    Lattice *lt = obedit->data;
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

static void stats_object_pose(Object *ob, SceneStats *stats)
{
  if (ob->pose) {
    bArmature *arm = ob->data;
    bPoseChannel *pchan;

    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      stats->totbone++;
      if (pchan->bone && (pchan->bone->flag & BONE_SELECTED)) {
        if (pchan->bone->layer & arm->layer) {
          stats->totbonesel++;
        }
      }
    }
  }
}

static void stats_object_sculpt_dynamic_topology(Object *ob, SceneStats *stats)
{
  stats->totvert = ob->sculpt->bm->totvert;
  stats->tottri = ob->sculpt->bm->totface;
}

static bool stats_is_object_dynamic_topology_sculpt(Object *ob, const eObjectMode object_mode)
{
  return (ob && (object_mode & OB_MODE_SCULPT) && ob->sculpt && ob->sculpt->bm);
}

/* Statistics displayed in info header. Called regularly on scene changes. */
static void stats_update(Depsgraph *depsgraph, ViewLayer *view_layer)
{
  SceneStats stats = {0};
  Object *ob = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);

  if (obedit) {
    /* Edit Mode */
    FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, ((View3D *)NULL), ob->type, ob->mode, ob_iter) {
      stats_object_edit(ob_iter, &stats);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    /* Pose Mode */
    stats_object_pose(ob, &stats);
  }
  else if (ob && stats_is_object_dynamic_topology_sculpt(ob, ob->mode)) {
    /* Dynamic-topology sculpt mode */
    stats_object_sculpt_dynamic_topology(ob, &stats);
  }
  else {
    /* Objects */
    GSet *objects_gset = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob_iter) {
      stats_object(ob_iter, &stats, objects_gset);
    }
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;
    BLI_gset_free(objects_gset, NULL);
  }

  if (!view_layer->stats) {
    view_layer->stats = MEM_callocN(sizeof(SceneStats), "SceneStats");
  }

  *(view_layer->stats) = stats;
}

static void stats_string(ViewLayer *view_layer)
{
#define MAX_INFO_MEM_LEN 64
  SceneStats *stats = view_layer->stats;
  SceneStatsFmt stats_fmt;
  LayerCollection *layer_collection = view_layer->active_collection;
  Object *ob = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  eObjectMode object_mode = ob ? ob->mode : OB_MODE_OBJECT;
  uintptr_t mem_in_use, mmap_in_use;
  char memstr[MAX_INFO_MEM_LEN];
  char gpumemstr[MAX_INFO_MEM_LEN] = "";
  char formatted_mem[15];
  char *s;
  size_t ofs = 0;

  mem_in_use = MEM_get_memory_in_use();
  mmap_in_use = MEM_get_mapped_memory_in_use();

  /* Generate formatted numbers */
#define SCENE_STATS_FMT_INT(_id) BLI_str_format_uint64_grouped(stats_fmt._id, stats->_id)

  SCENE_STATS_FMT_INT(totvert);
  SCENE_STATS_FMT_INT(totvertsel);

  SCENE_STATS_FMT_INT(totedge);
  SCENE_STATS_FMT_INT(totedgesel);

  SCENE_STATS_FMT_INT(totface);
  SCENE_STATS_FMT_INT(totfacesel);

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

  /* get memory statistics */
  BLI_str_format_byte_unit(formatted_mem, mem_in_use - mmap_in_use, true);
  ofs = BLI_snprintf(memstr, MAX_INFO_MEM_LEN, IFACE_(" | Mem: %s"), formatted_mem);

  if (mmap_in_use) {
    BLI_str_format_byte_unit(formatted_mem, mmap_in_use, true);
    BLI_snprintf(memstr + ofs, MAX_INFO_MEM_LEN - ofs, IFACE_(" (%s)"), formatted_mem);
  }

  if (GPU_mem_stats_supported()) {
    int gpu_free_mem, gpu_tot_memory;

    GPU_mem_stats_get(&gpu_tot_memory, &gpu_free_mem);

    BLI_str_format_byte_unit(formatted_mem, gpu_free_mem, true);
    ofs = BLI_snprintf(gpumemstr, MAX_INFO_MEM_LEN, IFACE_(" | Free GPU Mem: %s"), formatted_mem);

    if (gpu_tot_memory) {
      BLI_str_format_byte_unit(formatted_mem, gpu_tot_memory, true);
      BLI_snprintf(gpumemstr + ofs, MAX_INFO_MEM_LEN - ofs, IFACE_("/%s"), formatted_mem);
    }
  }

  s = stats->infostr;
  ofs = 0;

  if (object_mode == OB_MODE_OBJECT) {
    ofs += BLI_snprintf(s + ofs,
                        MAX_INFO_LEN - ofs,
                        "%s | ",
                        BKE_collection_ui_name_get(layer_collection->collection));
  }

  if (ob) {
    ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, "%s | ", ob->id.name + 2);
  }

  if (obedit) {
    if (BKE_keyblock_from_object(obedit)) {
      ofs += BLI_strncpy_rlen(s + ofs, IFACE_("(Key) "), MAX_INFO_LEN - ofs);
    }

    if (obedit->type == OB_MESH) {
      ofs += BLI_snprintf(s + ofs,
                          MAX_INFO_LEN - ofs,
                          IFACE_("Verts:%s/%s | Edges:%s/%s | Faces:%s/%s | Tris:%s"),
                          stats_fmt.totvertsel,
                          stats_fmt.totvert,
                          stats_fmt.totedgesel,
                          stats_fmt.totedge,
                          stats_fmt.totfacesel,
                          stats_fmt.totface,
                          stats_fmt.tottri);
    }
    else if (obedit->type == OB_ARMATURE) {
      ofs += BLI_snprintf(s + ofs,
                          MAX_INFO_LEN - ofs,
                          IFACE_("Verts:%s/%s | Bones:%s/%s"),
                          stats_fmt.totvertsel,
                          stats_fmt.totvert,
                          stats_fmt.totbonesel,
                          stats_fmt.totbone);
    }
    else {
      ofs += BLI_snprintf(s + ofs,
                          MAX_INFO_LEN - ofs,
                          IFACE_("Verts:%s/%s"),
                          stats_fmt.totvertsel,
                          stats_fmt.totvert);
    }

    ofs += BLI_strncpy_rlen(s + ofs, memstr, MAX_INFO_LEN - ofs);
    ofs += BLI_strncpy_rlen(s + ofs, gpumemstr, MAX_INFO_LEN - ofs);
  }
  else if (ob && (object_mode & OB_MODE_POSE)) {
    ofs += BLI_snprintf(s + ofs,
                        MAX_INFO_LEN - ofs,
                        IFACE_("Bones:%s/%s %s%s"),
                        stats_fmt.totbonesel,
                        stats_fmt.totbone,
                        memstr,
                        gpumemstr);
  }
  else if ((ob) && (ob->type == OB_GPENCIL)) {
    ofs += BLI_snprintf(s + ofs,
                        MAX_INFO_LEN - ofs,
                        IFACE_("Layers:%s | Frames:%s | Strokes:%s | Points:%s | Objects:%s/%s"),
                        stats_fmt.totgplayer,
                        stats_fmt.totgpframe,
                        stats_fmt.totgpstroke,
                        stats_fmt.totgppoint,
                        stats_fmt.totobjsel,
                        stats_fmt.totobj);

    ofs += BLI_strncpy_rlen(s + ofs, memstr, MAX_INFO_LEN - ofs);
    ofs += BLI_strncpy_rlen(s + ofs, gpumemstr, MAX_INFO_LEN - ofs);
  }
  else if (stats_is_object_dynamic_topology_sculpt(ob, object_mode)) {
    ofs += BLI_snprintf(s + ofs,
                        MAX_INFO_LEN - ofs,
                        IFACE_("Verts:%s | Tris:%s%s"),
                        stats_fmt.totvert,
                        stats_fmt.tottri,
                        gpumemstr);
  }
  else {
    ofs += BLI_snprintf(s + ofs,
                        MAX_INFO_LEN - ofs,
                        IFACE_("Verts:%s | Faces:%s | Tris:%s | Objects:%s/%s%s%s"),
                        stats_fmt.totvert,
                        stats_fmt.totface,
                        stats_fmt.tottri,
                        stats_fmt.totobjsel,
                        stats_fmt.totobj,
                        memstr,
                        gpumemstr);
  }

  ofs += BLI_snprintf(s + ofs, MAX_INFO_LEN - ofs, " | %s", versionstr);
#undef MAX_INFO_MEM_LEN
}

#undef MAX_INFO_LEN

void ED_info_stats_clear(ViewLayer *view_layer)
{
  if (view_layer->stats) {
    MEM_freeN(view_layer->stats);
    view_layer->stats = NULL;
  }
}

const char *ED_info_stats_string(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  /* Loopin through dependency graph when interface is locked in not safe.
   * Thew interface is marked as locked when jobs wants to modify the
   * dependency graph. */
  wmWindowManager *wm = bmain->wm.first;
  if (wm->is_interface_locked) {
    return "";
  }
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
  if (!view_layer->stats) {
    stats_update(depsgraph, view_layer);
  }
  stats_string(view_layer);
  return view_layer->stats->infostr;
}
