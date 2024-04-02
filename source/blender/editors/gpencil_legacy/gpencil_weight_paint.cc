/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Brush based operators for editing Grease Pencil strokes.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_action.h"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_modifier.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "DNA_meshdata_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "UI_view2d.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.hh"

/* ************************************************ */
/* General Brush Editing Context */
#define GP_SELECT_BUFFER_CHUNK 256
#define GP_FIND_NEAREST_BUFFER_CHUNK 1024
#define GP_FIND_NEAREST_EPSILON 1e-6f
#define GP_STROKE_HASH_BITSHIFT 16

/* Grid of Colors for Smear. */
struct tGP_Grid {
  /** Lower right corner of rectangle of grid cell. */
  float bottom[2];
  /** Upper left corner of rectangle of grid cell. */
  float top[2];
  /** Average Color */
  float color[4];
  /** Total points included. */
  int totcol;
};

/* List of points affected by brush. */
struct tGP_Selected {
  /** Referenced stroke. */
  bGPDstroke *gps;
  /** Point index in points array. */
  int pt_index;
  /** Position. */
  int pc[2];
  /** Color. */
  float color[4];
  /** Weight. */
  float weight;
};

/* Context for brush operators */
struct tGP_BrushWeightpaintData {
  Main *bmain;
  Scene *scene;
  Object *object;

  ARegion *region;

  /* Current GPencil datablock */
  bGPdata *gpd;

  Brush *brush;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /* Is the brush currently painting? */
  bool is_painting;

  /* Start of new paint */
  bool first;

  /* Is multi-frame editing enabled, and are we using falloff for that? */
  bool is_multiframe;
  bool use_multiframe_falloff;

  /* Draw tool: add or subtract? */
  bool subtract;

  /* Auto-normalize weights of bone-deformed vertices? */
  bool auto_normalize;

  /* Active vertex group */
  int vrgroup;

  /* Brush Runtime Data: */
  /* - position and pressure
   * - the *_prev variants are the previous values
   */
  float mouse[2], mouse_prev[2];
  float pressure;

  /* - Brush direction. */
  float brush_dir[2];
  bool brush_dir_is_set;

  /* - Multi-frame falloff factor. */
  float mf_falloff;

  /* Brush geometry (bounding box). */
  rcti brush_rect;

  /* Temp data to save selected points */
  /** Stroke buffer. */
  tGP_Selected *pbuffer;
  /** Number of elements currently used in cache. */
  int pbuffer_used;
  /** Number of total elements available in cache. */
  int pbuffer_size;
  /** Average weight of elements in cache (used for average tool). */
  float pbuffer_avg_weight;

  /* Temp data for find-nearest-points, used by blur and smear tool. */
  bool use_find_nearest;
  /** Buffer of stroke points during one mouse swipe. */
  tGP_Selected *fn_pbuffer;
  /** Hash table of added points (to avoid duplicate entries). */
  GHash *fn_added;
  /** KDtree for finding nearest points. */
  KDTree_2d *fn_kdtree;
  /** Number of points used in find-nearest set. */
  uint fn_used;
  /** Number of points available in find-nearest set. */
  uint fn_size;
  /** Flag for balancing kdtree. */
  bool fn_do_balance;

  /* Temp data for auto-normalize weights used by deforming bones. */
  /** Boolean array of locked vertex groups. */
  bool *vgroup_locked;
  /** Boolean array of vertex groups deformed by bones. */
  bool *vgroup_bone_deformed;
  /** Number of vertex groups in object. */
  int vgroup_tot;
};

/* Ensure the buffer to hold temp selected point size is enough to save all points selected. */
static void gpencil_select_buffer_ensure(tGP_BrushWeightpaintData *gso, const bool clear)
{
  /* By default a buffer is created with one block with a predefined number of free slots,
   * if the size is not enough, the cache is reallocated adding a new block of free slots.
   * This is done in order to keep cache small and improve speed. */
  if ((gso->pbuffer_used + 1) > gso->pbuffer_size) {
    if ((gso->pbuffer_size == 0) || (gso->pbuffer == nullptr)) {
      gso->pbuffer = static_cast<tGP_Selected *>(
          MEM_callocN(sizeof(tGP_Selected) * GP_SELECT_BUFFER_CHUNK, __func__));
      gso->pbuffer_size = GP_SELECT_BUFFER_CHUNK;
    }
    else {
      gso->pbuffer_size += GP_SELECT_BUFFER_CHUNK;
      gso->pbuffer = static_cast<tGP_Selected *>(
          MEM_recallocN(gso->pbuffer, sizeof(tGP_Selected) * gso->pbuffer_size));
    }
  }

  /* Clear old data. */
  if (clear) {
    gso->pbuffer_used = 0;
    if (gso->pbuffer != nullptr) {
      memset(gso->pbuffer, 0, sizeof(tGP_Selected) * gso->pbuffer_size);
    }
  }

  /* Create or enlarge buffer for find-nearest-points. */
  if (gso->use_find_nearest && ((gso->fn_used + 1) > gso->fn_size)) {
    gso->fn_size += GP_FIND_NEAREST_BUFFER_CHUNK;

    /* Stroke point buffer. */
    if (gso->fn_pbuffer == nullptr) {
      gso->fn_pbuffer = static_cast<tGP_Selected *>(
          MEM_callocN(sizeof(tGP_Selected) * gso->fn_size, __func__));
    }
    else {
      gso->fn_pbuffer = static_cast<tGP_Selected *>(
          MEM_recallocN(gso->fn_pbuffer, sizeof(tGP_Selected) * gso->fn_size));
    }

    /* Stroke point hash table (for duplicate checking.) */
    if (gso->fn_added == nullptr) {
      gso->fn_added = BLI_ghash_int_new("GP weight paint find nearest");
    }

    /* KDtree of stroke points. */
    bool do_tree_rebuild = false;
    if (gso->fn_kdtree != nullptr) {
      BLI_kdtree_2d_free(gso->fn_kdtree);
      do_tree_rebuild = true;
    }
    gso->fn_kdtree = BLI_kdtree_2d_new(gso->fn_size);

    if (do_tree_rebuild) {
      for (int i = 0; i < gso->fn_used; i++) {
        float pc_f[2];
        copy_v2fl_v2i(pc_f, gso->fn_pbuffer[i].pc);
        BLI_kdtree_2d_insert(gso->fn_kdtree, i, pc_f);
      }
    }
  }
}

/* Ensure a vertex group for the weight paint brush. */
static void gpencil_vertex_group_ensure(tGP_BrushWeightpaintData *gso)
{
  if (gso->vrgroup >= 0) {
    return;
  }
  if (gso->object == nullptr) {
    return;
  }

  DEG_relations_tag_update(gso->bmain);
  gso->vrgroup = 0;

  Object *ob_armature = BKE_modifiers_is_deformed_by_armature(gso->object);
  if (ob_armature == nullptr) {
    BKE_object_defgroup_add(gso->object);
    return;
  }
  Bone *actbone = ((bArmature *)ob_armature->data)->act_bone;
  if (actbone == nullptr) {
    return;
  }
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob_armature->pose, actbone->name);
  if (pchan == nullptr) {
    return;
  }
  bDeformGroup *dg = BKE_object_defgroup_find_name(gso->object, pchan->name);
  if (dg == nullptr) {
    BKE_object_defgroup_add_name(gso->object, pchan->name);
  }
}

static void gpencil_select_buffer_avg_weight_set(tGP_BrushWeightpaintData *gso)
{
  if (gso->pbuffer_used == 0) {
    gso->pbuffer_avg_weight = 0.0f;
    return;
  }
  float sum = 0;
  for (int i = 0; i < gso->pbuffer_used; i++) {
    tGP_Selected *selected = &gso->pbuffer[i];
    sum += selected->weight;
  }
  gso->pbuffer_avg_weight = sum / gso->pbuffer_used;
  CLAMP(gso->pbuffer_avg_weight, 0.0f, 1.0f);
}

/* Get boolean array of vertex groups deformed by GP armature modifiers.
 * GP equivalent of `BKE_object_defgroup_validmap_get`.
 */
static bool *gpencil_vgroup_bone_deformed_map_get(Object *ob, const int defbase_tot)
{
  bDeformGroup *dg;
  bool *vgroup_bone_deformed;
  GHash *gh;
  int i;
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  if (BLI_listbase_is_empty(defbase)) {
    return nullptr;
  }

  /* Add all vertex group names to a hash table. */
  gh = BLI_ghash_str_new_ex(__func__, defbase_tot);
  LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
    BLI_ghash_insert(gh, dg->name, nullptr);
  }
  BLI_assert(BLI_ghash_len(gh) == defbase_tot);

  /* Now loop through the armature modifiers and identify deform bones. */
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type != eGpencilModifierType_Armature) {
      continue;
    }

    ArmatureGpencilModifierData *amd = (ArmatureGpencilModifierData *)md;

    if (amd->object && amd->object->pose) {
      bPose *pose = amd->object->pose;

      LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
        void **val_p;
        if (chan->bone->flag & BONE_NO_DEFORM) {
          continue;
        }

        val_p = BLI_ghash_lookup_p(gh, chan->name);
        if (val_p) {
          *val_p = POINTER_FROM_INT(1);
        }
      }
    }
  }

  /* Mark vertex groups with reference in the bone hash table. */
  vgroup_bone_deformed = static_cast<bool *>(
      MEM_mallocN(sizeof(*vgroup_bone_deformed) * defbase_tot, __func__));
  for (dg = static_cast<bDeformGroup *>(defbase->first), i = 0; dg; dg = dg->next, i++) {
    vgroup_bone_deformed[i] = (BLI_ghash_lookup(gh, dg->name) != nullptr);
  }

  BLI_assert(i == BLI_ghash_len(gh));

  BLI_ghash_free(gh, nullptr, nullptr);

  return vgroup_bone_deformed;
}

/* ************************************************ */
/* Auto-normalize Operations
 * This section defines the functions for auto-normalizing the sum of weights to 1.0
 * for points in vertex groups deformed by bones.
 * The logic is copied from `editors/sculpt_paint/paint_vertex.cc`. */

static bool do_weight_paint_normalize(MDeformVert *dvert,
                                      const int defbase_tot,
                                      const bool *vgroup_bone_deformed,
                                      const bool *vgroup_locked,
                                      const bool lock_active_vgroup,
                                      const int active_vgroup)
{
  float sum = 0.0f, fac;
  float sum_unlock = 0.0f;
  float sum_lock = 0.0f;
  uint lock_tot = 0, unlock_tot = 0;
  MDeformWeight *dw;

  if (dvert->totweight <= 1) {
    return true;
  }

  dw = dvert->dw;
  for (int i = dvert->totweight; i != 0; i--, dw++) {
    /* Auto-normalize is only applied on bone-deformed vertex groups that have weight already. */
    if (dw->def_nr < defbase_tot && vgroup_bone_deformed[dw->def_nr] && dw->weight > FLT_EPSILON) {
      sum += dw->weight;

      if (vgroup_locked[dw->def_nr] || (lock_active_vgroup && active_vgroup == dw->def_nr)) {
        lock_tot++;
        sum_lock += dw->weight;
      }
      else {
        unlock_tot++;
        sum_unlock += dw->weight;
      }
    }
  }

  if (sum == 1.0f) {
    return true;
  }

  if (unlock_tot == 0) {
    /* There are no unlocked vertex groups to normalize. We don't need
     * a second pass when there is only one locked group (the active group). */
    return (lock_tot == 1);
  }

  if (sum_lock >= 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
    /* Locked groups make it impossible to fully normalize,
     * zero out what we can and return false. */
    dw = dvert->dw;
    for (int i = dvert->totweight; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_bone_deformed[dw->def_nr]) {
        if ((vgroup_locked[dw->def_nr] == false) &&
            !(lock_active_vgroup && active_vgroup == dw->def_nr))
        {
          dw->weight = 0.0f;
        }
      }
    }

    return (sum_lock == 1.0f);
  }
  if (sum_unlock != 0.0f) {
    fac = (1.0f - sum_lock) / sum_unlock;

    dw = dvert->dw;
    for (int i = dvert->totweight; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_bone_deformed[dw->def_nr] && dw->weight > FLT_EPSILON)
      {
        if ((vgroup_locked[dw->def_nr] == false) &&
            !(lock_active_vgroup && active_vgroup == dw->def_nr))
        {
          dw->weight *= fac;
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }
  }
  else {
    fac = (1.0f - sum_lock) / unlock_tot;
    CLAMP(fac, 0.0f, 1.0f);

    dw = dvert->dw;
    for (int i = dvert->totweight; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_bone_deformed[dw->def_nr] && dw->weight > FLT_EPSILON)
      {
        if ((vgroup_locked[dw->def_nr] == false) &&
            !(lock_active_vgroup && active_vgroup == dw->def_nr))
        {
          dw->weight = fac;
        }
      }
    }
  }

  return true;
}

/**
 * A version of #do_weight_paint_normalize that only changes unlocked weights
 * and does a second pass without the active vertex group locked when the first pass fails.
 */
static void do_weight_paint_normalize_try(MDeformVert *dvert, tGP_BrushWeightpaintData *gso)
{
  /* First pass with both active and explicitly locked vertex groups restricted from change. */
  bool succes = do_weight_paint_normalize(
      dvert, gso->vgroup_tot, gso->vgroup_bone_deformed, gso->vgroup_locked, true, gso->vrgroup);

  if (!succes) {
    /* Second pass with active vertex group unlocked. */
    do_weight_paint_normalize(
        dvert, gso->vgroup_tot, gso->vgroup_bone_deformed, gso->vgroup_locked, false, -1);
  }
}

/* Brush Operations ------------------------------- */

/* Compute strength of effect. */
static float brush_influence_calc(tGP_BrushWeightpaintData *gso, const int radius, const int co[2])
{
  Brush *brush = gso->brush;

  /* basic strength factor from brush settings */
  float influence = brush->alpha;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mouse_i[2];
  round_v2i_v2fl(mouse_i, gso->mouse);
  float distance = float(len_v2v2_int(mouse_i, co));

  /* Apply Brush curve. */
  float brush_falloff = BKE_brush_curve_strength(brush, distance, float(radius));
  influence *= brush_falloff;

  /* apply multi-frame falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}

/* Compute effect vector for directional brushes. */
static void brush_calc_brush_dir_2d(tGP_BrushWeightpaintData *gso)
{
  sub_v2_v2v2(gso->brush_dir, gso->mouse, gso->mouse_prev);

  /* Skip tiny changes in direction, we want the bigger movements only. */
  if (len_squared_v2(gso->brush_dir) < 9.0f) {
    return;
  }

  normalize_v2(gso->brush_dir);

  gso->brush_dir_is_set = true;
  copy_v2_v2(gso->mouse_prev, gso->mouse);
}

/* ************************************************ */
/* Brush Callbacks
 * This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius. */

/* Draw Brush */
static bool brush_draw_apply(tGP_BrushWeightpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == nullptr) {
    return false;
  }

  /* Apply brush weight. */
  float bweight = (gso->subtract) ? -gso->brush->weight : gso->brush->weight;
  dw->weight = interpf(bweight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Average Brush */
static bool brush_average_apply(tGP_BrushWeightpaintData *gso,
                                bGPDstroke *gps,
                                int pt_index,
                                const int radius,
                                const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == nullptr) {
    return false;
  }

  /* Blend weight with average weight under the brush. */
  dw->weight = interpf(gso->pbuffer_avg_weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Blur Brush */
static bool brush_blur_apply(tGP_BrushWeightpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == nullptr) {
    return false;
  }

  /* Find the 5 nearest points (this includes the to-be-blurred point itself). */
  KDTreeNearest_2d nearest[5];
  float pc_f[2];
  copy_v2fl_v2i(pc_f, co);
  const int tot = BLI_kdtree_2d_find_nearest_n(gso->fn_kdtree, pc_f, nearest, 5);

  /* Calculate the average (=blurred) weight. */
  float blur_weight = 0.0f, dist_sum = 0.0f;
  int count = 0;
  for (int i = 0; i < tot; i++) {
    dist_sum += nearest[i].dist;
    count++;
  }
  if (count <= 1) {
    return false;
  }
  for (int i = 0; i < tot; i++) {
    /* Weighted average, based on distance to point. */
    blur_weight += (1.0f - nearest[i].dist / dist_sum) * gso->fn_pbuffer[nearest[i].index].weight;
  }
  blur_weight /= (count - 1);

  /* Blend weight with blurred weight. */
  dw->weight = interpf(blur_weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Smear Brush */
static bool brush_smear_apply(tGP_BrushWeightpaintData *gso,
                              bGPDstroke *gps,
                              int pt_index,
                              const int radius,
                              const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == nullptr) {
    return false;
  }

  /* Find the 8 nearest points (this includes the to-be-blurred point itself). */
  KDTreeNearest_2d nearest[8];
  float pc_f[2];
  copy_v2fl_v2i(pc_f, co);
  const int tot = BLI_kdtree_2d_find_nearest_n(gso->fn_kdtree, pc_f, nearest, 8);

  /* For smearing a weight from point A to point B, we look for a point A 'behind' the brush,
   * matching the brush angle best and with the shortest distance to B. */
  float point_dot[8] = {0};
  float point_dir[2];
  float score_max = 0.0f, dist_min = FLT_MAX, dist_max = 0.0f;
  int i_max = -1, count = 0;

  for (int i = 0; i < tot; i++) {
    /* Skip the point we are about to smear. */
    if (nearest[i].dist > GP_FIND_NEAREST_EPSILON) {
      sub_v2_v2v2(point_dir, pc_f, nearest[i].co);
      normalize_v2(point_dir);

      /* Match A-B direction with brush direction. */
      point_dot[i] = dot_v2v2(point_dir, gso->brush_dir);
      if (point_dot[i] > 0.0f) {
        count++;
        float dist = nearest[i].dist;
        if (dist < dist_min) {
          dist_min = dist;
        }
        if (dist > dist_max) {
          dist_max = dist;
        }
      }
    }
  }
  if (count == 0) {
    return false;
  }

  /* Find best match in angle and distance. */
  float dist_f = (dist_min == dist_max) ? 1.0f : 0.95f / (dist_max - dist_min);
  for (int i = 0; i < tot; i++) {
    if (point_dot[i] > 0.0f) {
      float score = point_dot[i] * (1.0f - (nearest[i].dist - dist_min) * dist_f);
      if (score > score_max) {
        score_max = score;
        i_max = i;
      }
    }
  }
  if (i_max == -1) {
    return false;
  }

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Smear the weight. */
  dw->weight = interpf(gso->fn_pbuffer[nearest[i_max].index].weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* ************************************************ */
/* Header Info */
static void gpencil_weightpaint_brush_header_set(bContext *C, tGP_BrushWeightpaintData *gso)
{
  switch (gso->brush->gpencil_weight_tool) {
    case GPWEIGHT_TOOL_DRAW:
      ED_workspace_status_text(C,
                               IFACE_("GPencil Weight Paint: LMB to paint | RMB/Escape to Exit"));
      break;
    case GPWEIGHT_TOOL_BLUR:
      ED_workspace_status_text(C, IFACE_("GPencil Weight Blur: LMB to blur | RMB/Escape to Exit"));
      break;
    case GPWEIGHT_TOOL_AVERAGE:
      ED_workspace_status_text(
          C, IFACE_("GPencil Weight Average: LMB to set average | RMB/Escape to Exit"));
      break;
    case GPWEIGHT_TOOL_SMEAR:
      ED_workspace_status_text(C,
                               IFACE_("GPencil Weight Smear: LMB to smear | RMB/Escape to Exit"));
      break;
  }
}

/* ************************************************ */
/* Grease Pencil Weight Paint Operator */

/* Init/Exit ----------------------------------------------- */

static bool gpencil_weightpaint_brush_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  Paint *paint = &ts->gp_weightpaint->paint;

  /* set the brush using the tool */
  tGP_BrushWeightpaintData *gso;

  /* setup operator data */
  gso = static_cast<tGP_BrushWeightpaintData *>(
      MEM_callocN(sizeof(tGP_BrushWeightpaintData), "tGP_BrushWeightpaintData"));
  op->customdata = gso;

  gso->bmain = CTX_data_main(C);

  gso->brush = BKE_paint_brush(paint);
  BKE_curvemapping_init(gso->brush->curve);

  gso->is_painting = false;
  gso->first = true;

  gso->pbuffer = nullptr;
  gso->pbuffer_size = 0;
  gso->pbuffer_used = 0;

  gso->fn_pbuffer = nullptr;
  gso->fn_added = nullptr;
  gso->fn_kdtree = nullptr;
  gso->fn_used = 0;
  gso->fn_size = 0;
  gso->use_find_nearest = ELEM(
      gso->brush->gpencil_weight_tool, GPWEIGHT_TOOL_BLUR, GPWEIGHT_TOOL_SMEAR);

  gso->gpd = ED_gpencil_data_get_active(C);
  gso->scene = scene;
  gso->object = ob;
  if (ob) {
    gso->vrgroup = gso->gpd->vertex_group_active_index - 1;
    if (!BLI_findlink(&gso->gpd->vertex_group_names, gso->vrgroup)) {
      gso->vrgroup = -1;
    }
  }
  else {
    gso->vrgroup = -1;
  }

  gso->region = CTX_wm_region(C);

  /* Multi-frame settings. */
  gso->is_multiframe = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gso->gpd));
  gso->use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;

  /* Init multi-edit falloff curve data before doing anything,
   * so we won't have to do it again later. */
  if (gso->is_multiframe) {
    BKE_curvemapping_init(ts->gp_sculpt.cur_falloff);
  }

  /* Draw tool: add or subtract weight? */
  gso->subtract = (gso->brush->flag & BRUSH_DIR_IN);

  /* Setup auto-normalize. */
  gso->auto_normalize = (ts->auto_normalize && gso->vrgroup != -1);
  if (gso->auto_normalize) {
    gso->vgroup_tot = BLI_listbase_count(&gso->gpd->vertex_group_names);
    /* Get boolean array of vertex groups deformed by bones. */
    gso->vgroup_bone_deformed = gpencil_vgroup_bone_deformed_map_get(ob, gso->vgroup_tot);
    if (gso->vgroup_bone_deformed != nullptr) {
      /* Get boolean array of locked vertex groups. */
      gso->vgroup_locked = BKE_object_defgroup_lock_flags_get(ob, gso->vgroup_tot);
      if (gso->vgroup_locked == nullptr) {
        gso->vgroup_locked = (bool *)MEM_callocN(sizeof(bool) * gso->vgroup_tot, __func__);
      }
    }
    else {
      gso->auto_normalize = false;
    }
  }

  /* Setup space conversions. */
  gpencil_point_conversion_init(C, &gso->gsc);

  /* Update header. */
  gpencil_weightpaint_brush_header_set(C, gso);

  return true;
}

static void gpencil_weightpaint_brush_exit(bContext *C, wmOperator *op)
{
  tGP_BrushWeightpaintData *gso = static_cast<tGP_BrushWeightpaintData *>(op->customdata);

  /* Clear status bar text. */
  ED_workspace_status_text(C, nullptr);

  /* Free operator data */
  MEM_SAFE_FREE(gso->vgroup_bone_deformed);
  MEM_SAFE_FREE(gso->vgroup_locked);
  if (gso->fn_kdtree != nullptr) {
    BLI_kdtree_2d_free(gso->fn_kdtree);
  }
  if (gso->fn_added != nullptr) {
    BLI_ghash_free(gso->fn_added, nullptr, nullptr);
  }
  MEM_SAFE_FREE(gso->fn_pbuffer);
  MEM_SAFE_FREE(gso->pbuffer);
  MEM_SAFE_FREE(gso);
  op->customdata = nullptr;
}

/* Poll callback for stroke weight paint operator. */
static bool gpencil_weightpaint_brush_poll(bContext *C)
{
  if (!ED_operator_regionactive(C)) {
    CTX_wm_operator_poll_msg_set(C, "Active region not set");
    return false;
  }

  ScrArea *area = CTX_wm_area(C);
  if (area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  bGPdata *gpd = ED_gpencil_data_get_active(C);
  if ((gpd == nullptr) || !GPENCIL_WEIGHT_MODE(gpd)) {
    return false;
  }

  ToolSettings *ts = CTX_data_scene(C)->toolsettings;
  Brush *brush = BKE_paint_brush(&ts->gp_weightpaint->paint);
  if (brush == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Grease Pencil has no active paint tool");
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* Helper to save the points selected by the brush. */
static void gpencil_save_selected_point(tGP_BrushWeightpaintData *gso,
                                        bGPDstroke *gps,
                                        const int gps_index,
                                        const int index,
                                        const int pc[2],
                                        const bool within_brush)
{
  tGP_Selected *selected;
  bGPDspoint *pt = &gps->points[index];

  /* Ensure the array to save the list of selected points is big enough. */
  gpencil_select_buffer_ensure(gso, false);

  /* Copy point data. */
  if (within_brush) {
    selected = &gso->pbuffer[gso->pbuffer_used];
    selected->gps = gps;
    selected->pt_index = index;
    copy_v2_v2_int(selected->pc, pc);
    copy_v4_v4(selected->color, pt->vert_color);
    gso->pbuffer_used++;
  }

  /* Ensure vertex group and dvert. */
  gpencil_vertex_group_ensure(gso);
  BKE_gpencil_dvert_ensure(gps);

  /* Copy current weight. */
  MDeformVert *dvert = gps->dvert + index;
  MDeformWeight *dw = BKE_defvert_find_index(dvert, gso->vrgroup);
  if (within_brush && (dw != nullptr)) {
    selected->weight = dw->weight;
  }

  /* Store point for finding nearest points (blur, smear). */
  if (gso->use_find_nearest) {
    /* Create hash key, assuming there are no more than 65536 strokes in a frame
     * and 65536 points in a stroke. */
    const int point_hash = (gps_index << GP_STROKE_HASH_BITSHIFT) + index;

    /* Prevent duplicate points in buffer. */
    if (!BLI_ghash_haskey(gso->fn_added, POINTER_FROM_INT(point_hash))) {
      /* Add stroke point to find-nearest buffer. */
      selected = &gso->fn_pbuffer[gso->fn_used];
      copy_v2_v2_int(selected->pc, pc);
      selected->weight = (dw == nullptr) ? 0.0f : dw->weight;

      BLI_ghash_insert(
          gso->fn_added, POINTER_FROM_INT(point_hash), POINTER_FROM_INT(gso->fn_used));

      float pc_f[2];
      copy_v2_fl2(pc_f, float(pc[0]), float(pc[1]));
      BLI_kdtree_2d_insert(gso->fn_kdtree, gso->fn_used, pc_f);

      gso->fn_do_balance = true;
      gso->fn_used++;
    }
    else {
      /* Update weight of point in buffer. */
      int *idx = static_cast<int *>(BLI_ghash_lookup(gso->fn_added, POINTER_FROM_INT(point_hash)));
      selected = &gso->fn_pbuffer[POINTER_AS_INT(idx)];
      selected->weight = (dw == nullptr) ? 0.0f : dw->weight;
    }
  }
}

/* Select points in this stroke and add to an array to be used later. */
static void gpencil_weightpaint_select_stroke(tGP_BrushWeightpaintData *gso,
                                              bGPDstroke *gps,
                                              const int gps_index,
                                              const float diff_mat[4][4],
                                              const float bound_mat[4][4])
{
  GP_SpaceConversion *gsc = &gso->gsc;
  const rcti *rect = &gso->brush_rect;
  Brush *brush = gso->brush;
  /* For the blur tool, look a bit wider than the brush itself,
   * because we need the weight of surrounding points to perform the blur. */
  const bool widen_brush = (gso->brush->gpencil_weight_tool == GPWEIGHT_TOOL_BLUR);
  int radius_brush = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  int radius_wide = (widen_brush) ? radius_brush * 1.3f : radius_brush;
  bool within_brush = true;

  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = nullptr;

  bGPDspoint *pt1, *pt2;
  bGPDspoint *pt = nullptr;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int index;
  bool include_last = false;

  /* Check if the stroke collides with brush. */
  if (!ED_gpencil_stroke_check_collision(gsc, gps, gso->mouse, radius_wide, bound_mat)) {
    return;
  }

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    pt = &gps->points[0];
    gpencil_point_to_world_space(gps->points, diff_mat, &pt_temp);
    gpencil_point_to_xy(gsc, gps, &pt_temp, &pc1[0], &pc1[1]);

    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
    /* Do bound-box check first. */
    if (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
      /* only check if point is inside */
      int mouse_i[2];
      round_v2i_v2fl(mouse_i, gso->mouse);
      int mlen = len_v2v2_int(mouse_i, pc1);
      if (mlen <= radius_wide) {
        /* apply operation to this point */
        if (pt_active != nullptr) {
          if (widen_brush) {
            within_brush = (mlen <= radius_brush);
          }
          gpencil_save_selected_point(gso, gps_active, gps_index, 0, pc1, within_brush);
        }
      }
    }
  }
  else {
    /* Loop over the points in the stroke, checking for intersections
     * - an intersection means that we touched the stroke
     */
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* Get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      bGPDspoint npt;
      gpencil_point_to_world_space(pt1, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc1[0], &pc1[1]);

      gpencil_point_to_world_space(pt2, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc2[0], &pc2[1]);

      /* Check that point segment of the bound-box of the selection stroke */
      if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          (!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1]) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1])))
      {
        /* Check if point segment of stroke had anything to do with
         * brush region  (either within stroke painted, or on its lines)
         * - this assumes that line-width is irrelevant.
         */
        if (gpencil_stroke_inside_circle(gso->mouse, radius_wide, pc1[0], pc1[1], pc2[0], pc2[1]))
        {
          if (widen_brush) {
            within_brush = gpencil_stroke_inside_circle(
                gso->mouse, radius_brush, pc1[0], pc1[1], pc2[0], pc2[1]);
          }

          /* To each point individually... */
          pt = &gps->points[i];
          pt_active = pt->runtime.pt_orig;
          if (pt_active != nullptr) {
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
            gpencil_save_selected_point(gso, gps_active, gps_index, index, pc1, within_brush);
          }

          /* Only do the second point if this is the last segment,
           * and it is unlikely that the point will get handled
           * otherwise.
           *
           * NOTE: There is a small risk here that the second point wasn't really
           *       actually in-range. In that case, it only got in because
           *       the line linking the points was!
           */
          if (i + 1 == gps->totpoints - 1) {
            pt = &gps->points[i + 1];
            pt_active = pt->runtime.pt_orig;
            if (pt_active != nullptr) {
              index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i + 1;
              gpencil_save_selected_point(gso, gps_active, gps_index, index, pc2, within_brush);
              include_last = false;
            }
          }
          else {
            include_last = true;
          }
        }
        else if (include_last) {
          /* This case is for cases where for whatever reason the second vert (1st here)
           * doesn't get included because the whole edge isn't in bounds,
           * but it would've qualified since it did with the previous step
           * (but wasn't added then, to avoid double-ups).
           */
          pt = &gps->points[i];
          pt_active = pt->runtime.pt_orig;
          if (pt_active != nullptr) {
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
            gpencil_save_selected_point(gso, gps_active, gps_index, index, pc1, true);
            include_last = false;
          }
        }
      }
    }
  }
}

/* Apply weight paint brushes to strokes in the given frame. */
static bool gpencil_weightpaint_brush_do_frame(bContext *C,
                                               tGP_BrushWeightpaintData *gso,
                                               bGPDframe *gpf,
                                               const float diff_mat[4][4],
                                               const float bound_mat[4][4])
{
  Object *ob = CTX_data_active_object(C);
  char tool = gso->brush->gpencil_weight_tool;
  const int radius = (gso->brush->flag & GP_BRUSH_USE_PRESSURE) ?
                         gso->brush->size * gso->pressure :
                         gso->brush->size;
  tGP_Selected *selected = nullptr;
  gso->fn_do_balance = false;

  /*---------------------------------------------------------------------
   * First step: select the points affected. This step is required to have
   * all selected points before apply the effect, because it could be
   * required to do some step. Now is not used, but the operator is ready.
   *--------------------------------------------------------------------- */
  int gps_index;
  LISTBASE_FOREACH_INDEX (bGPDstroke *, gps, &gpf->strokes, gps_index) {
    /* Skip strokes that are invalid for current view. */
    if (ED_gpencil_stroke_can_use(C, gps) == false) {
      continue;
    }
    /* Check if the color is visible. */
    if (ED_gpencil_stroke_material_visible(ob, gps) == false) {
      continue;
    }

    /* Check points below the brush. */
    gpencil_weightpaint_select_stroke(gso, gps, gps_index, diff_mat, bound_mat);
  }

  bDeformGroup *defgroup = static_cast<bDeformGroup *>(
      BLI_findlink(&gso->gpd->vertex_group_names, gso->vrgroup));
  if ((defgroup == nullptr) || (defgroup->flag & DG_LOCK_WEIGHT)) {
    return false;
  }

  /*---------------------------------------------------------------------
   * Second step: Calculations on selected points.
   *--------------------------------------------------------------------- */
  /* For average tool, get average weight of affected points. */
  if (tool == GPWEIGHT_TOOL_AVERAGE) {
    gpencil_select_buffer_avg_weight_set(gso);
  }
  /* Balance find-nearest kdtree. */
  if (gso->use_find_nearest && gso->fn_do_balance) {
    BLI_kdtree_2d_balance(gso->fn_kdtree);
  }

  /*---------------------------------------------------------------------
   * Third step: Apply effect.
   *--------------------------------------------------------------------- */
  bool changed = false;
  for (int i = 0; i < gso->pbuffer_used; i++) {
    selected = &gso->pbuffer[i];

    switch (tool) {
      case GPWEIGHT_TOOL_DRAW: {
        changed |= brush_draw_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      }
      case GPWEIGHT_TOOL_AVERAGE: {
        changed |= brush_average_apply(
            gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      }
      case GPWEIGHT_TOOL_BLUR: {
        changed |= brush_blur_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      }
      case GPWEIGHT_TOOL_SMEAR: {
        changed |= brush_smear_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      }
      default:
        printf("ERROR: Unknown type of GPencil Weight Paint brush\n");
        break;
    }
  }

  /* Clear the selected array, but keep the memory allocation. */
  gpencil_select_buffer_ensure(gso, true);

  return changed;
}

/* Apply brush effect to all layers. */
static bool gpencil_weightpaint_brush_apply_to_layers(bContext *C, tGP_BrushWeightpaintData *gso)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = gso->object;
  bool changed = false;

  Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph, &obact->id);
  bGPdata *gpd = (bGPdata *)ob_eval->data;

  /* Find visible strokes, and perform operations on those if hit */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* If locked or no active frame, don't do anything. */
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == nullptr)) {
      continue;
    }

    /* Calculate transform matrix. */
    float diff_mat[4][4], bound_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
    copy_m4_m4(bound_mat, diff_mat);
    mul_m4_m4m4(diff_mat, diff_mat, gpl->layer_invmat);

    /* Active Frame or MultiFrame? */
    if (gso->is_multiframe) {
      /* init multi-frame falloff options */
      int f_init = 0;
      int f_end = 0;

      if (gso->use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        /* Always do active frame; Otherwise, only include selected frames */
        if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
          /* Compute multi-frame falloff factor. */
          if (gso->use_multiframe_falloff) {
            /* Falloff depends on distance to active frame
             * (relative to the overall frame range). */
            gso->mf_falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }
          else {
            /* No falloff */
            gso->mf_falloff = 1.0f;
          }

          /* affect strokes in this frame */
          changed |= gpencil_weightpaint_brush_do_frame(C, gso, gpf, diff_mat, bound_mat);
        }
      }
    }
    else {
      /* Apply to active frame's strokes */
      gso->mf_falloff = 1.0f;
      changed |= gpencil_weightpaint_brush_do_frame(C, gso, gpl->actframe, diff_mat, bound_mat);
    }
  }

  return changed;
}

/* Calculate settings for applying brush */
static void gpencil_weightpaint_brush_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
  tGP_BrushWeightpaintData *gso = static_cast<tGP_BrushWeightpaintData *>(op->customdata);
  Brush *brush = gso->brush;
  const int radius = ((brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                              gso->brush->size);
  float mousef[2];
  int mouse[2];
  bool changed = false;

  /* Get latest mouse coordinates */
  RNA_float_get_array(itemptr, "mouse", mousef);
  gso->mouse[0] = mouse[0] = int(mousef[0]);
  gso->mouse[1] = mouse[1] = int(mousef[1]);

  gso->pressure = RNA_float_get(itemptr, "pressure");

  /* Store coordinates as reference, if operator just started running */
  if (gso->first) {
    gso->mouse_prev[0] = gso->mouse[0];
    gso->mouse_prev[1] = gso->mouse[1];
    gso->brush_dir_is_set = false;
  }
  gso->first = false;

  /* Update brush_rect, so that it represents the bounding rectangle of brush. */
  gso->brush_rect.xmin = mouse[0] - radius;
  gso->brush_rect.ymin = mouse[1] - radius;
  gso->brush_rect.xmax = mouse[0] + radius;
  gso->brush_rect.ymax = mouse[1] + radius;

  /* Calculate brush direction. */
  if (gso->brush->gpencil_weight_tool == GPWEIGHT_TOOL_SMEAR) {
    brush_calc_brush_dir_2d(gso);

    if (!gso->brush_dir_is_set) {
      return;
    }
  }

  /* Apply brush to layers. */
  changed = gpencil_weightpaint_brush_apply_to_layers(C, gso);

  /* Updates. */
  if (changed) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }
}

/* Running --------------------------------------------- */

/* helper - a record stroke, and apply paint event */
static void gpencil_weightpaint_brush_apply_event(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = static_cast<tGP_BrushWeightpaintData *>(op->customdata);
  PointerRNA itemptr;
  float mouse[2];

  mouse[0] = event->mval[0] + 1;
  mouse[1] = event->mval[1] + 1;

  /* fill in stroke */
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  RNA_float_set_array(&itemptr, "mouse", mouse);
  RNA_boolean_set(&itemptr, "is_start", gso->first);

  /* Handle pressure sensitivity (which is supplied by tablets). */
  float pressure = event->tablet.pressure;
  CLAMP(pressure, 0.0f, 1.0f);
  RNA_float_set(&itemptr, "pressure", pressure);

  /* apply */
  gpencil_weightpaint_brush_apply(C, op, &itemptr);
}

/* reapply */
static int gpencil_weightpaint_brush_exec(bContext *C, wmOperator *op)
{
  if (!gpencil_weightpaint_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    gpencil_weightpaint_brush_apply(C, op, &itemptr);
  }
  RNA_END;

  gpencil_weightpaint_brush_exit(C, op);

  return OPERATOR_FINISHED;
}

/* start modal painting */
static int gpencil_weightpaint_brush_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = nullptr;
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  const bool is_playing = ED_screen_animation_playing(CTX_wm_manager(C)) != nullptr;

  /* the operator cannot work while play animation */
  if (is_playing) {
    BKE_report(op->reports, RPT_ERROR, "Cannot Paint while play animation");

    return OPERATOR_CANCELLED;
  }

  /* init painting data */
  if (!gpencil_weightpaint_brush_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  gso = static_cast<tGP_BrushWeightpaintData *>(op->customdata);

  /* register modal handler */
  WM_event_add_modal_handler(C, op);

  /* start drawing immediately? */
  if (is_modal == false) {
    ARegion *region = CTX_wm_region(C);

    /* apply first dab... */
    gso->is_painting = true;
    gpencil_weightpaint_brush_apply_event(C, op, event);

    /* redraw view with feedback */
    ED_region_tag_redraw(region);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* painting - handle events. */
static int gpencil_weightpaint_brush_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGP_BrushWeightpaintData *gso = static_cast<tGP_BrushWeightpaintData *>(op->customdata);
  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");
  bool redraw_region = false;
  bool redraw_toolsettings = false;

  /* The operator can be in 2 states: Painting and Idling. */
  if (gso->is_painting) {
    /* Painting. */
    switch (event->type) {
      /* Mouse Move: Apply somewhere else. */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        /* Apply brush effect at new position. */
        gpencil_weightpaint_brush_apply_event(C, op, event);

        /* Force redraw, so that the cursor will at least be valid. */
        redraw_region = true;
        break;

      /* Painting mouse-button release: Stop painting (back to idle). */
      case LEFTMOUSE:
        if (is_modal) {
          /* go back to idling... */
          gso->is_painting = false;
        }
        else {
          /* end painting, since we're not modal. */
          gso->is_painting = false;

          gpencil_weightpaint_brush_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;

      /* Abort painting if any of the usual things are tried. */
      case MIDDLEMOUSE:
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gpencil_weightpaint_brush_exit(C, op);
        return OPERATOR_FINISHED;
    }
  }
  else {
    /* Idling. */
    BLI_assert(is_modal == true);

    switch (event->type) {
      /* Painting mouse-button press = Start painting (switch to painting state). */
      case LEFTMOUSE:
        /* do initial "click" apply. */
        gso->is_painting = true;
        gso->first = true;

        gpencil_weightpaint_brush_apply_event(C, op, event);
        break;

      /* Exit modal operator, based on the "standard" ops. */
      case RIGHTMOUSE:
      case EVT_ESCKEY:
        gpencil_weightpaint_brush_exit(C, op);
        return OPERATOR_FINISHED;

      /* MMB is often used for view manipulations. */
      case MIDDLEMOUSE:
        return OPERATOR_PASS_THROUGH;

      /* Mouse movements should update the brush cursor - Just redraw the active region. */
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE:
        redraw_region = true;
        break;

      /* Change Frame - Allowed. */
      case EVT_LEFTARROWKEY:
      case EVT_RIGHTARROWKEY:
      case EVT_UPARROWKEY:
      case EVT_DOWNARROWKEY:
        return OPERATOR_PASS_THROUGH;

      /* Camera/View Gizmo's - Allowed. */
      /* See rationale in `gpencil_paint.cc`, #gpencil_draw_modal(). */
      case EVT_PAD0:
      case EVT_PAD1:
      case EVT_PAD2:
      case EVT_PAD3:
      case EVT_PAD4:
      case EVT_PAD5:
      case EVT_PAD6:
      case EVT_PAD7:
      case EVT_PAD8:
      case EVT_PAD9:
        return OPERATOR_PASS_THROUGH;

      /* Unhandled event. */
      default:
        break;
    }
  }

  /* Redraw region? */
  if (redraw_region) {
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  /* Redraw toolsettings (brush settings)? */
  if (redraw_toolsettings) {
    DEG_id_tag_update(&gso->gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }

  return OPERATOR_RUNNING_MODAL;
}

void GPENCIL_OT_weight_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stroke Weight Paint";
  ot->idname = "GPENCIL_OT_weight_paint";
  ot->description = "Draw weight on stroke points";

  /* api callbacks */
  ot->exec = gpencil_weightpaint_brush_exec;
  ot->invoke = gpencil_weightpaint_brush_invoke;
  ot->modal = gpencil_weightpaint_brush_modal;
  ot->cancel = gpencil_weightpaint_brush_exit;
  ot->poll = gpencil_weightpaint_brush_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* -------------------------------------------------------------------- */
/*  Weight Toggle Add/Subtract Operator */
static int gpencil_weight_toggle_direction_invoke(bContext *C,
                                                  wmOperator * /*op*/,
                                                  const wmEvent * /*event*/)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Paint *paint = &ts->gp_weightpaint->paint;
  Brush *brush = BKE_paint_brush(paint);

  /* Toggle Add/Subtract flag. */
  brush->flag ^= BRUSH_DIR_IN;

  /* Update tool settings. */
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_weight_toggle_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint Toggle Direction";
  ot->idname = "GPENCIL_OT_weight_toggle_direction";
  ot->description = "Toggle Add/Subtract for the weight paint draw tool";

  /* api callbacks */
  ot->invoke = gpencil_weight_toggle_direction_invoke;
  ot->poll = gpencil_weightpaint_brush_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */
/*  Weight Sample Operator */
static int gpencil_weight_sample_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* Get mouse position. */
  int mouse[2];
  mouse[0] = event->mval[0] + 1;
  mouse[1] = event->mval[1] + 1;
  float mouse_f[2];
  copy_v2fl_v2i(mouse_f, mouse);

  /* Get active GP object. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if ((ob == nullptr) || (gpd == nullptr)) {
    return OPERATOR_CANCELLED;
  }

  /* Get active vertex group. */
  int vgroup = gpd->vertex_group_active_index - 1;
  bDeformGroup *defgroup = static_cast<bDeformGroup *>(
      BLI_findlink(&gpd->vertex_group_names, vgroup));
  if (!defgroup) {
    return OPERATOR_CANCELLED;
  }

  /* Init space conversion. */
  GP_SpaceConversion gsc = {nullptr};
  gpencil_point_conversion_init(C, &gsc);

  /* Get evaluated GP object. */
  Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph, &ob->id);
  bGPdata *gpd_eval = (bGPdata *)ob_eval->data;

  /* Get brush radius. */
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->gp_weightpaint->paint);
  const int radius = brush->size;

  /* Init closest points. */
  float closest_dist[2] = {FLT_MAX, FLT_MAX};
  float closest_weight[2] = {0.0f, 0.0f};
  int closest_count = 0;
  int pc[2] = {0};

  /* Inspect all layers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_eval->layers) {
    /* If no active frame, don't do anything. */
    if (gpl->actframe == nullptr) {
      continue;
    }

    /* Calculate transform matrix. */
    float diff_mat[4][4], bound_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);
    copy_m4_m4(bound_mat, diff_mat);
    mul_m4_m4m4(diff_mat, diff_mat, gpl->layer_invmat);

    /* Inspect all strokes in active frame. */
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      /* Look for strokes that collide with the brush. */
      if (!ED_gpencil_stroke_check_collision(&gsc, gps, mouse_f, radius, bound_mat)) {
        continue;
      }
      if (gps->dvert == nullptr) {
        continue;
      }

      /* Look for two closest points. */
      for (int i = 0; i < gps->totpoints; i++) {
        bGPDspoint *pt = &gps->points[i];
        bGPDspoint npt;

        gpencil_point_to_world_space(pt, diff_mat, &npt);
        gpencil_point_to_xy(&gsc, gps, &npt, &pc[0], &pc[1]);

        float dist = len_v2v2_int(pc, mouse);

        if ((dist < closest_dist[0]) || (dist < closest_dist[1])) {
          /* Get weight. */
          MDeformVert *dvert = &gps->dvert[i];
          MDeformWeight *dw = BKE_defvert_find_index(dvert, vgroup);
          if (dw == nullptr) {
            continue;
          }
          if (dist < closest_dist[0]) {
            closest_dist[1] = closest_dist[0];
            closest_weight[1] = closest_weight[0];
            closest_dist[0] = dist;
            closest_weight[0] = dw->weight;
            closest_count++;
          }
          else if (dist < closest_dist[1]) {
            closest_dist[1] = dist;
            closest_weight[1] = dw->weight;
            closest_count++;
          }
        }
      }
    }
  }

  /* Set brush weight, based on points found. */
  if (closest_count > 0) {
    if (closest_count == 1) {
      brush->weight = closest_weight[0];
    }
    else {
      CLAMP_MIN(closest_dist[1], 1e-6f);
      float dist_sum = closest_dist[0] + closest_dist[1];
      brush->weight = (1.0f - closest_dist[0] / dist_sum) * closest_weight[0] +
                      (1.0f - closest_dist[1] / dist_sum) * closest_weight[1];
    }

    /* Update tool settings. */
    WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_weight_sample(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint Sample Weight";
  ot->idname = "GPENCIL_OT_weight_sample";
  ot->description = "Use the mouse to sample a weight in the 3D view";

  /* api callbacks */
  ot->invoke = gpencil_weight_sample_invoke;
  ot->poll = gpencil_weightpaint_brush_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;
}
