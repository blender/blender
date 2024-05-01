/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 *
 * Operators for dealing with armatures and GP data-blocks.
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_action.h"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_layer.hh"
#include "BKE_object_deform.h"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"

#include "ANIM_bone_collections.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.hh"

enum {
  GP_ARMATURE_NAME = 0,
  GP_ARMATURE_AUTO = 1,
};

#define DEFAULT_RATIO 0.10f
#define DEFAULT_DECAY 0.8f

static int gpencil_bone_looper(Object *ob,
                               Bone *bone,
                               void *data,
                               int (*bone_func)(Object *, Bone *, void *))
{
  /* We want to apply the function bone_func to every bone
   * in an armature -- feed bone_looper the first bone and
   * a pointer to the bone_func and watch it go! The int count
   * can be useful for counting bones with a certain property
   * (e.g. skinnable)
   */
  int count = 0;

  if (bone) {
    /* only do bone_func if the bone is non null */
    count += bone_func(ob, bone, data);

    /* try to execute bone_func for the first child */
    count += gpencil_bone_looper(ob, static_cast<Bone *>(bone->childbase.first), data, bone_func);

    /* try to execute bone_func for the next bone at this
     * depth of the recursion.
     */
    count += gpencil_bone_looper(ob, bone->next, data, bone_func);
  }

  return count;
}

static int gpencil_bone_skinnable_cb(Object * /*ob*/, Bone *bone, void *datap)
{
  /* Bones that are deforming
   * are regarded to be "skinnable" and are eligible for
   * auto-skinning.
   *
   * This function performs 2 functions:
   *
   *   a) It returns 1 if the bone is skinnable.
   *      If we loop over all bones with this
   *      function, we can count the number of
   *      skinnable bones.
   *   b) If the pointer data is non null,
   *      it is treated like a handle to a
   *      bone pointer -- the bone pointer
   *      is set to point at this bone, and
   *      the pointer the handle points to
   *      is incremented to point to the
   *      next member of an array of pointers
   *      to bones. This way we can loop using
   *      this function to construct an array of
   *      pointers to bones that point to all
   *      skinnable bones.
   */
  Bone ***hbone;
  int a, segments;
  struct Data {
    Object *armob;
    void *list;
    int heat;
  } *data = static_cast<Data *>(datap);

  if (!(bone->flag & BONE_HIDDEN_P)) {
    if (!(bone->flag & BONE_NO_DEFORM)) {
      if (data->heat && data->armob->pose &&
          BKE_pose_channel_find_name(data->armob->pose, bone->name))
      {
        segments = bone->segments;
      }
      else {
        segments = 1;
      }

      if (data->list != nullptr) {
        hbone = (Bone ***)&data->list;

        for (a = 0; a < segments; a++) {
          **hbone = bone;
          (*hbone)++;
        }
      }
      return segments;
    }
  }
  return 0;
}

static int vgroup_add_unique_bone_cb(Object *ob, Bone *bone, void * /*ptr*/)
{
  /* This group creates a vertex group to ob that has the
   * same name as bone (provided the bone is skinnable).
   * If such a vertex group already exist the routine exits.
   */
  if (!(bone->flag & BONE_NO_DEFORM)) {
    if (!BKE_object_defgroup_find_name(ob, bone->name)) {
      BKE_object_defgroup_add_name(ob, bone->name);
      return 1;
    }
  }
  return 0;
}

static int dgroup_skinnable_cb(Object *ob, Bone *bone, void *datap)
{
  /* Bones that are deforming
   * are regarded to be "skinnable" and are eligible for
   * auto-skinning.
   *
   * This function performs 2 functions:
   *
   *   a) If the bone is skinnable, it creates
   *      a vertex group for ob that has
   *      the name of the skinnable bone
   *      (if one doesn't exist already).
   *   b) If the pointer data is non null,
   *      it is treated like a handle to a
   *      bDeformGroup pointer -- the
   *      bDeformGroup pointer is set to point
   *      to the deform group with the bone's
   *      name, and the pointer the handle
   *      points to is incremented to point to the
   *      next member of an array of pointers
   *      to bDeformGroups. This way we can loop using
   *      this function to construct an array of
   *      pointers to bDeformGroups, all with names
   *      of skinnable bones.
   */
  bDeformGroup ***hgroup, *defgroup = nullptr;
  int a, segments;
  struct Data {
    Object *armob;
    void *list;
    int heat;
  } *data = static_cast<Data *>(datap);
  bArmature *arm = static_cast<bArmature *>(data->armob->data);

  if (!(bone->flag & BONE_HIDDEN_P)) {
    if (!(bone->flag & BONE_NO_DEFORM)) {
      if (data->heat && data->armob->pose &&
          BKE_pose_channel_find_name(data->armob->pose, bone->name))
      {
        segments = bone->segments;
      }
      else {
        segments = 1;
      }

      if (ANIM_bone_in_visible_collection(arm, bone)) {
        if (!(defgroup = BKE_object_defgroup_find_name(ob, bone->name))) {
          defgroup = BKE_object_defgroup_add_name(ob, bone->name);
        }
        else if (defgroup->flag & DG_LOCK_WEIGHT) {
          /* In case vgroup already exists and is locked, do not modify it here. See #43814. */
          defgroup = nullptr;
        }
      }

      if (data->list != nullptr) {
        hgroup = (bDeformGroup ***)&data->list;

        for (a = 0; a < segments; a++) {
          **hgroup = defgroup;
          (*hgroup)++;
        }
      }
      return segments;
    }
  }
  return 0;
}

/* get weight value depending of distance and decay value */
static float get_weight(float dist, float decay_rad, float dif_rad)
{
  float weight = 1.0f;
  if (dist < decay_rad) {
    weight = 1.0f;
  }
  else {
    weight = interpf(0.0f, 0.9f, (dist - decay_rad) / dif_rad);
  }

  return weight;
}

/* This functions implements the automatic computation of vertex group weights */
static void gpencil_add_verts_to_dgroups(
    const bContext *C, Object *ob, Object *ob_arm, const float ratio, const float decay)
{
  bArmature *arm = static_cast<bArmature *>(ob_arm->data);
  Bone **bonelist, *bone;
  bDeformGroup **dgrouplist;
  bPoseChannel *pchan;
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  Mat4 bbone_array[MAX_BBONE_SUBDIV], *bbone = nullptr;
  float(*root)[3], (*tip)[3], (*verts)[3];
  float *radsqr;
  int *selected;
  float weight;
  int numbones, i, j, segments = 0;
  struct {
    Object *armob;
    void *list;
    int heat;
  } looper_data;

  looper_data.armob = ob_arm;
  looper_data.heat = true;
  looper_data.list = nullptr;

  /* count the number of skinnable bones */
  numbones = gpencil_bone_looper(
      ob, static_cast<Bone *>(arm->bonebase.first), &looper_data, gpencil_bone_skinnable_cb);

  if (numbones == 0) {
    return;
  }

  /* create an array of pointer to bones that are skinnable
   * and fill it with all of the skinnable bones */
  bonelist = static_cast<Bone **>(MEM_callocN(numbones * sizeof(Bone *), "bonelist"));
  looper_data.list = bonelist;
  gpencil_bone_looper(
      ob, static_cast<Bone *>(arm->bonebase.first), &looper_data, gpencil_bone_skinnable_cb);

  /* create an array of pointers to the deform groups that
   * correspond to the skinnable bones (creating them
   * as necessary. */
  dgrouplist = static_cast<bDeformGroup **>(
      MEM_callocN(numbones * sizeof(bDeformGroup *), "dgrouplist"));

  looper_data.list = dgrouplist;
  gpencil_bone_looper(
      ob, static_cast<Bone *>(arm->bonebase.first), &looper_data, dgroup_skinnable_cb);

  /* create an array of root and tip positions transformed into
   * global coords */
  root = static_cast<float(*)[3]>(MEM_callocN(sizeof(float[3]) * numbones, "root"));
  tip = static_cast<float(*)[3]>(MEM_callocN(sizeof(float[3]) * numbones, "tip"));
  selected = static_cast<int *>(MEM_callocN(sizeof(int) * numbones, "selected"));
  radsqr = static_cast<float *>(MEM_callocN(sizeof(float) * numbones, "radsqr"));

  for (j = 0; j < numbones; j++) {
    bone = bonelist[j];

    /* handle bbone */
    if (segments == 0) {
      segments = 1;
      bbone = nullptr;

      if ((ob_arm->pose) && (pchan = BKE_pose_channel_find_name(ob_arm->pose, bone->name))) {
        if (bone->segments > 1) {
          segments = bone->segments;
          BKE_pchan_bbone_spline_setup(pchan, true, false, bbone_array);
          bbone = bbone_array;
        }
      }
    }

    segments--;

    /* compute root and tip */
    if (bbone) {
      mul_v3_m4v3(root[j], bone->arm_mat, bbone[segments].mat[3]);
      if ((segments + 1) < bone->segments) {
        mul_v3_m4v3(tip[j], bone->arm_mat, bbone[segments + 1].mat[3]);
      }
      else {
        copy_v3_v3(tip[j], bone->arm_tail);
      }
    }
    else {
      copy_v3_v3(root[j], bone->arm_head);
      copy_v3_v3(tip[j], bone->arm_tail);
    }

    mul_m4_v3(ob_arm->object_to_world().ptr(), root[j]);
    mul_m4_v3(ob_arm->object_to_world().ptr(), tip[j]);

    selected[j] = 1;

    /* calculate radius squared */
    radsqr[j] = len_squared_v3v3(root[j], tip[j]) * ratio;
  }

  /* loop all strokes */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    bGPDspoint *pt = nullptr;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          BKE_gpencil_dvert_ensure(gps);

          /* create verts array */
          verts = static_cast<float(*)[3]>(MEM_callocN(gps->totpoints * sizeof(*verts), __func__));

          /* transform stroke points to global space */
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            copy_v3_v3(verts[i], &pt->x);
            mul_m4_v3(ob->object_to_world().ptr(), verts[i]);
          }

          /* loop groups and assign weight */
          for (j = 0; j < numbones; j++) {
            int def_nr = BLI_findindex(&gpd->vertex_group_names, dgrouplist[j]);
            if (def_nr < 0) {
              continue;
            }

            float decay_rad = radsqr[j] - (radsqr[j] * decay);
            float dif_rad = radsqr[j] - decay_rad;

            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              MDeformVert *dvert = &gps->dvert[i];
              float dist = dist_squared_to_line_segment_v3(verts[i], root[j], tip[j]);
              if (dist > radsqr[j]) {
                /* if not in cylinder, check if inside extreme spheres */
                weight = 0.0f;
                dist = len_squared_v3v3(root[j], verts[i]);
                if (dist < radsqr[j]) {
                  weight = get_weight(dist, decay_rad, dif_rad);
                }
                else {
                  dist = len_squared_v3v3(tip[j], verts[i]);
                  if (dist < radsqr[j]) {
                    weight = get_weight(dist, decay_rad, dif_rad);
                  }
                }
              }
              else {
                /* inside bone cylinder */
                weight = get_weight(dist, decay_rad, dif_rad);
              }

              /* assign weight */
              MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
              if (dw) {
                dw->weight = weight;
              }
            }
          }
          MEM_SAFE_FREE(verts);
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }

  /* free the memory allocated */
  MEM_SAFE_FREE(bonelist);
  MEM_SAFE_FREE(dgrouplist);
  MEM_SAFE_FREE(root);
  MEM_SAFE_FREE(tip);
  MEM_SAFE_FREE(radsqr);
  MEM_SAFE_FREE(selected);
}

static void gpencil_object_vgroup_calc_from_armature(const bContext *C,
                                                     Object *ob,
                                                     Object *ob_arm,
                                                     const int mode,
                                                     const float ratio,
                                                     const float decay)
{
  /* Lets try to create some vertex groups
   * based on the bones of the parent armature.
   */
  bArmature *arm = static_cast<bArmature *>(ob_arm->data);

  /* always create groups */
  const int defbase_tot = BKE_object_defgroup_count(ob);
  int defbase_add;
  /* Traverse the bone list, trying to create empty vertex
   * groups corresponding to the bone.
   */
  defbase_add = gpencil_bone_looper(
      ob, static_cast<Bone *>(arm->bonebase.first), nullptr, vgroup_add_unique_bone_cb);

  if (defbase_add) {
    /* It's possible there are DWeights outside the range of the current
     * object's deform groups. In this case the new groups won't be empty */
    blender::ed::object::vgroup_data_clamp_range(static_cast<ID *>(ob->data), defbase_tot);
  }

  if (mode == GP_ARMATURE_AUTO) {
    /* Traverse the bone list, trying to fill vertex groups
     * with the corresponding vertex weights for which the
     * bone is closest.
     */
    gpencil_add_verts_to_dgroups(C, ob, ob_arm, ratio, decay);
  }

  DEG_relations_tag_update(CTX_data_main(C));
}

bool ED_gpencil_add_armature(const bContext *C, ReportList *reports, Object *ob, Object *ob_arm)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  if (ob == nullptr) {
    return false;
  }

  /* if no armature modifier, add a new one */
  GpencilModifierData *md = BKE_gpencil_modifiers_findby_type(ob, eGpencilModifierType_Armature);
  if (md == nullptr) {
    md = blender::ed::object::gpencil_modifier_add(
        reports, bmain, scene, ob, "Armature", eGpencilModifierType_Armature);
    if (md == nullptr) {
      BKE_report(reports, RPT_ERROR, "Unable to add a new Armature modifier to object");
      return false;
    }
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* verify armature */
  ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;
  if (mmd->object == nullptr) {
    mmd->object = ob_arm;
  }
  else {
    if (ob_arm != mmd->object) {
      BKE_report(reports,
                 RPT_ERROR,
                 "The existing Armature modifier is already using a different Armature object");
      return false;
    }
  }
  return true;
}

bool ED_gpencil_add_armature_weights(
    const bContext *C, ReportList *reports, Object *ob, Object *ob_arm, int mode)
{
  if (ob == nullptr) {
    return false;
  }

  bool success = ED_gpencil_add_armature(C, reports, ob, ob_arm);

  /* add weights */
  if (success) {
    gpencil_object_vgroup_calc_from_armature(C, ob, ob_arm, mode, DEFAULT_RATIO, DEFAULT_DECAY);
  }

  return success;
}
/* ***************** Generate armature weights ************************** */
static bool gpencil_generate_weights_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr) {
    return false;
  }

  if (ob->type != OB_GPENCIL_LEGACY) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  if (BLI_listbase_count(&gpd->layers) == 0) {
    return false;
  }

  /* need some armature in the view layer */
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->object->type == OB_ARMATURE) {
      return true;
    }
  }

  return false;
}

static int gpencil_generate_weights_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  bGPdata *gpd = (bGPdata *)ob->data;
  Object *ob_arm = nullptr;

  const int mode = RNA_enum_get(op->ptr, "mode");
  const float ratio = RNA_float_get(op->ptr, "ratio");
  const float decay = RNA_float_get(op->ptr, "decay");

  /* sanity checks */
  if (ELEM(nullptr, ob, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* get armature */
  const int arm_idx = RNA_enum_get(op->ptr, "armature");
  if (arm_idx > 0) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = static_cast<Base *>(
        BLI_findlink(BKE_view_layer_object_bases_get(view_layer), arm_idx - 1));
    ob_arm = base->object;
  }
  else {
    /* get armature from modifier */
    GpencilModifierData *md = BKE_gpencil_modifiers_findby_type(ob_eval,
                                                                eGpencilModifierType_Armature);
    if (md == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "The grease pencil object needs an Armature modifier");
      return OPERATOR_CANCELLED;
    }

    ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;
    if (mmd->object == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "The Armature modifier is invalid");
      return OPERATOR_CANCELLED;
    }

    ob_arm = mmd->object;
  }

  if (ob_arm == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Armature object in the view layer");
    return OPERATOR_CANCELLED;
  }

  gpencil_object_vgroup_calc_from_armature(C, ob, ob_arm, mode, ratio, decay);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

/* Dynamically populate an enum of Armatures */
static const EnumPropertyItem *gpencil_armatures_enum_itemf(bContext *C,
                                                            PointerRNA * /*ptr*/,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (C == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* add default */
  item_tmp.identifier = "DEFAULT";
  item_tmp.name = "Default";
  item_tmp.value = 0;
  RNA_enum_item_add(&item, &totitem, &item_tmp);
  i++;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *ob = base->object;
    if (ob->type == OB_ARMATURE) {
      item_tmp.identifier = item_tmp.name = ob->id.name + 2;
      item_tmp.value = i;
      RNA_enum_item_add(&item, &totitem, &item_tmp);
    }
    i++;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void GPENCIL_OT_generate_weights(wmOperatorType *ot)
{
  static const EnumPropertyItem mode_type[] = {
      {GP_ARMATURE_NAME, "NAME", 0, "Empty Groups", ""},
      {GP_ARMATURE_AUTO, "AUTO", 0, "Automatic Weights", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Generate Automatic Weights";
  ot->idname = "GPENCIL_OT_generate_weights";
  ot->description = "Generate automatic weights for armatures (requires armature modifier)";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_generate_weights_exec;
  ot->poll = gpencil_generate_weights_poll;

  ot->prop = RNA_def_enum(ot->srna, "mode", mode_type, 0, "Mode", "");

  prop = RNA_def_enum(
      ot->srna, "armature", rna_enum_dummy_DEFAULT_items, 0, "Armature", "Armature to use");
  RNA_def_enum_funcs(prop, gpencil_armatures_enum_itemf);

  RNA_def_float(ot->srna,
                "ratio",
                DEFAULT_RATIO,
                0.0f,
                2.0f,
                "Ratio",
                "Ratio between bone length and influence radius",
                0.001f,
                1.0f);

  RNA_def_float(ot->srna,
                "decay",
                DEFAULT_DECAY,
                0.0f,
                1.0f,
                "Decay",
                "Factor to reduce influence depending of distance to bone axis",
                0.0f,
                1.0f);
}
