/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Operators and API's for renaming bones both in and out of Edit Mode.
 *
 * This file contains functions/API's for renaming bones and/or working with them.
 */

#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_screen.hh"

#include "ANIM_armature.hh"

#include "armature_intern.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Unique Bone Name Utility (Edit Mode)
 * \{ */

/* NOTE: there's a ed_armature_bone_unique_name() too! */
static bool editbone_unique_check(ListBase *ebones, const StringRefNull name, EditBone *bone)
{
  if (bone) {
    /* This indicates that there is a bone to ignore. This means ED_armature_ebone_find_name()
     * cannot be used, as it might return the bone we should be ignoring. */
    for (EditBone *ebone : ListBaseWrapper<EditBone>(ebones)) {
      if (ebone->name == name && ebone != bone) {
        return true;
      }
    }
    return false;
  }

  EditBone *dupli = ED_armature_ebone_find_name(ebones, name.c_str());
  return dupli && dupli != bone;
}

void ED_armature_ebone_unique_name(ListBase *ebones, char *name, EditBone *bone)
{
  BLI_uniquename_cb(
      [&](const StringRefNull check_name) {
        return editbone_unique_check(ebones, check_name, bone);
      },
      DATA_("Bone"),
      '.',
      name,
      sizeof(bone->name));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unique Bone Name Utility (Object Mode)
 * \{ */

static void ed_armature_bone_unique_name(bArmature *arm, char *name)
{
  BLI_uniquename_cb(
      [&](const StringRefNull check_name) {
        return BKE_armature_find_bone_name(arm, check_name.c_str()) != nullptr;
      },
      DATA_("Bone"),
      '.',
      name,
      sizeof(Bone::name));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Renaming (Object & Edit Mode API)
 * \{ */

/**
 * Helper call for `armature_bone_rename()`.
 *
 * \param rename_ob: The object whose bone was renamed.
 * \param constraint_ob: The object that owns the constraints in `conlist`.
 */
static void constraint_bone_name_fix(Object *rename_ob,
                                     Object *constraint_ob,
                                     ListBase *conlist,
                                     const char *oldname,
                                     const char *newname)
{
  LISTBASE_FOREACH (bConstraint *, curcon, conlist) {
    ListBase targets = {nullptr, nullptr};

    /* constraint targets */
    if (BKE_constraint_targets_get(curcon, &targets)) {
      LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
        if (ct->tar == rename_ob) {
          if (STREQ(ct->subtarget, oldname)) {
            STRNCPY_UTF8(ct->subtarget, newname);
          }
        }
      }

      BKE_constraint_targets_flush(curcon, &targets, false);
    }

    /* Actions from action constraints.
     *
     * We only rename channels in the action if the action constraint and the
     * bone rename are from the same object. This is because the action of an
     * action constraint animates the constrained object/bone, it does not
     * animate the constraint target. */
    if (curcon->type == CONSTRAINT_TYPE_ACTION && constraint_ob == rename_ob) {
      bActionConstraint *actcon = static_cast<bActionConstraint *>(curcon->data);
      BKE_action_fix_paths_rename(&rename_ob->id,
                                  actcon->act,
                                  actcon->action_slot_handle,
                                  "pose.bones",
                                  oldname,
                                  newname,
                                  0,
                                  0,
                                  true);
    }
  }
}

void ED_armature_bone_rename(Main *bmain,
                             bArmature *arm,
                             const char *oldnamep,
                             const char *newnamep)
{
  Object *ob;
  char newname[MAXBONENAME];
  char oldname[MAXBONENAME];

  /* names better differ! */
  if (!STREQLEN(oldnamep, newnamep, MAXBONENAME)) {

    /* we alter newname string... so make copy */
    STRNCPY_UTF8(newname, newnamep);
    /* we use oldname for search... so make copy */
    STRNCPY(oldname, oldnamep); /* Allow non UTF8 encoding for the old name. */

    /* now check if we're in editmode, we need to find the unique name */
    if (arm->edbo) {
      EditBone *eBone = ED_armature_ebone_find_name(arm->edbo, oldname);

      if (eBone) {
        ED_armature_ebone_unique_name(arm->edbo, newname, nullptr);
        STRNCPY_UTF8(eBone->name, newname);
      }
      else {
        return;
      }
    }
    else {
      Bone *bone = BKE_armature_find_bone_name(arm, oldname);

      if (bone) {
        ed_armature_bone_unique_name(arm, newname);

        if (arm->bonehash) {
          BLI_assert(BLI_ghash_haskey(arm->bonehash, bone->name));
          BLI_ghash_remove(arm->bonehash, bone->name, nullptr, nullptr);
        }

        STRNCPY_UTF8(bone->name, newname);

        if (arm->bonehash) {
          BLI_ghash_insert(arm->bonehash, bone->name, bone);
        }
      }
      else {
        return;
      }
    }

    /* force evaluation copy to update database */
    DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);

    /* do entire dbase - objects */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {

      /* we have the object using the armature */
      if (arm == ob->data) {
        Object *cob;

        /* Rename the pose channel, if it exists */
        if (ob->pose) {
          bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, oldname);
          if (pchan) {
            GHash *gh = ob->pose->chanhash;

            /* remove the old hash entry, and replace with the new name */
            if (gh) {
              BLI_assert(BLI_ghash_haskey(gh, pchan->name));
              BLI_ghash_remove(gh, pchan->name, nullptr, nullptr);
            }

            STRNCPY_UTF8(pchan->name, newname);

            if (gh) {
              BLI_ghash_insert(gh, pchan->name, pchan);
            }
          }

          BLI_assert(BKE_pose_channels_is_valid(ob->pose) == true);
        }

        /* Update any object constraints to use the new bone name */
        for (cob = static_cast<Object *>(bmain->objects.first); cob;
             cob = static_cast<Object *>(cob->id.next))
        {
          if (cob->constraints.first) {
            constraint_bone_name_fix(ob, cob, &cob->constraints, oldname, newname);
          }
          if (cob->pose) {
            LISTBASE_FOREACH (bPoseChannel *, pchan, &cob->pose->chanbase) {
              constraint_bone_name_fix(ob, cob, &pchan->constraints, oldname, newname);
            }
          }
        }
      }

      /* See if an object is parented to this armature */
      if (ob->parent && (ob->parent->data == arm)) {
        if (ob->partype == PARBONE) {
          /* bone name in object */
          if (STREQ(ob->parsubstr, oldname)) {
            STRNCPY_UTF8(ob->parsubstr, newname);
          }
        }
      }

      if (BKE_modifiers_uses_armature(ob, arm) && BKE_object_supports_vertex_groups(ob)) {
        if (BKE_object_defgroup_find_name(ob, newname)) {
          WM_global_reportf(eReportType::RPT_WARNING,
                            "New bone name collides with an existing vertex "
                            "group name, vertex group "
                            "names are unchanged. (%s::%s)",
                            &ob->id.name[2],
                            newname);
          /* Not renaming vertex group could cause bone to bind to other vertex group, in this case
           * deformation could change, so we tag this object for depsgraph update. */
          DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
        }
        else if (bDeformGroup *dg = BKE_object_defgroup_find_name(ob, oldname)) {
          STRNCPY_UTF8(dg->name, newname);

          if (ob->type == OB_GREASE_PENCIL) {
            /* Update vgroup names stored in CurvesGeometry */
            BKE_grease_pencil_vgroup_name_update(ob, oldname, dg->name);
          }

          DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
        }
      }

      /* fix modifiers that might be using this name */
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        switch (md->type) {
          case eModifierType_Hook: {
            HookModifierData *hmd = reinterpret_cast<HookModifierData *>(md);

            if (hmd->object && (hmd->object->data == arm)) {
              if (STREQ(hmd->subtarget, oldname)) {
                STRNCPY_UTF8(hmd->subtarget, newname);
              }
            }
            break;
          }
          case eModifierType_UVWarp: {
            UVWarpModifierData *umd = reinterpret_cast<UVWarpModifierData *>(md);

            if (umd->object_src && (umd->object_src->data == arm)) {
              if (STREQ(umd->bone_src, oldname)) {
                STRNCPY_UTF8(umd->bone_src, newname);
              }
            }
            if (umd->object_dst && (umd->object_dst->data == arm)) {
              if (STREQ(umd->bone_dst, oldname)) {
                STRNCPY_UTF8(umd->bone_dst, newname);
              }
            }
            break;
          }
          default:
            break;
        }
      }

      /* fix camera focus */
      if (ob->type == OB_CAMERA) {
        Camera *cam = static_cast<Camera *>(ob->data);
        if ((cam->dof.focus_object != nullptr) && (cam->dof.focus_object->data == arm)) {
          if (STREQ(cam->dof.focus_subtarget, oldname)) {
            STRNCPY_UTF8(cam->dof.focus_subtarget, newname);
            DEG_id_tag_update(&cam->id, ID_RECALC_SYNC_TO_EVAL);
          }
        }
      }

      if (ob->type == OB_GREASE_PENCIL) {
        using namespace blender;
        GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
        for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
          Object *parent = layer->parent;
          if (parent == nullptr) {
            continue;
          }
          StringRefNull bone_name = layer->parent_bone_name();
          if (!bone_name.is_empty() && bone_name == StringRef(oldname)) {
            layer->set_parent_bone_name(newname);
          }
        }
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
    }

    /* Fix all animdata that may refer to this bone -
     * we can't just do the ones attached to objects,
     * since other ID-blocks may have drivers referring to this bone #29822. */

    /* XXX: the ID here is for armatures,
     * but most bone drivers are actually on the object instead. */
    {

      BKE_animdata_fix_paths_rename_all(&arm->id, "pose.bones", oldname, newname);
    }

    /* correct view locking */
    {
      bScreen *screen;
      for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
           screen = static_cast<bScreen *>(screen->id.next))
      {
        /* add regions */
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = reinterpret_cast<View3D *>(sl);
              if (v3d->ob_center && v3d->ob_center->data == arm) {
                if (STREQ(v3d->ob_center_bone, oldname)) {
                  STRNCPY_UTF8(v3d->ob_center_bone, newname);
                }
              }
            }
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Flipping (Object & Edit Mode API)
 * \{ */

struct BoneFlipNameData {
  BoneFlipNameData *next, *prev;
  char *name;
  char name_flip[MAXBONENAME];
};

void ED_armature_bones_flip_names(Main *bmain,
                                  bArmature *arm,
                                  ListBase *bones_names,
                                  const bool do_strip_numbers)
{
  ListBase bones_names_conflicts = {nullptr};
  BoneFlipNameData *bfn;

  /* First pass: generate flip names, and blindly rename.
   * If rename did not yield expected result,
   * store both bone's name and expected flipped one into temp list for second pass. */
  LISTBASE_FOREACH (LinkData *, link, bones_names) {
    char name_flip[MAXBONENAME];
    char *name = static_cast<char *>(link->data);

    /* WARNING: if do_strip_numbers is set, expect completely mismatched names in cases like
     * Bone.R, Bone.R.001, Bone.R.002, etc. */
    BLI_string_flip_side_name(name_flip, name, do_strip_numbers, sizeof(name_flip));

    ED_armature_bone_rename(bmain, arm, name, name_flip);

    if (!STREQ(name, name_flip)) {
      bfn = static_cast<BoneFlipNameData *>(alloca(sizeof(BoneFlipNameData)));
      bfn->name = name;
      STRNCPY_UTF8(bfn->name_flip, name_flip);
      BLI_addtail(&bones_names_conflicts, bfn);
    }
  }

  /* Second pass to handle the bones that have naming conflicts with other bones.
   * Note that if the other bone was not selected, its name was not flipped,
   * so conflict remains and that second rename simply generates a new numbered alternative name.
   */
  LISTBASE_FOREACH (BoneFlipNameData *, bfn, &bones_names_conflicts) {
    ED_armature_bone_rename(bmain, arm, bfn->name, bfn->name_flip);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flip Bone Names (Edit Mode Operator)
 * \{ */

static wmOperatorStatus armature_flip_names_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_edit_object(C);

  const bool do_strip_numbers = RNA_boolean_get(op->ptr, "do_strip_numbers");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);

    /* Paranoia check. */
    if (ob_active->pose == nullptr) {
      continue;
    }

    ListBase bones_names = {nullptr};

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (blender::animrig::bone_is_selected(arm, ebone)) {
        BLI_addtail(&bones_names, BLI_genericNodeN(ebone->name));

        if (arm->flag & ARM_MIRROR_EDIT) {
          EditBone *flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if ((flipbone) && !(flipbone->flag & BONE_SELECTED)) {
            BLI_addtail(&bones_names, BLI_genericNodeN(flipbone->name));
          }
        }
      }
    }

    if (BLI_listbase_is_empty(&bones_names)) {
      continue;
    }

    ED_armature_bones_flip_names(bmain, arm, &bones_names, do_strip_numbers);

    BLI_freelistN(&bones_names);

    /* since we renamed stuff... */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

    /* copied from #rna_Bone_update_renamed */
    /* Redraw Outliner / Dope-sheet. */
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_RENAME, ob->data);

    /* update animation channels */
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, ob->data);
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_flip_names(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Names";
  ot->idname = "ARMATURE_OT_flip_names";
  ot->description = "Flips (and corrects) the axis suffixes of the names of selected bones";

  /* API callbacks. */
  ot->exec = armature_flip_names_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "do_strip_numbers",
                  false,
                  "Strip Numbers",
                  "Try to remove right-most dot-number from flipped names.\n"
                  "Warning: May result in incoherent naming in some cases");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Auto Side Names (Edit Mode Operator)
 * \{ */

static wmOperatorStatus armature_autoside_names_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  char newname[MAXBONENAME];
  const short axis = RNA_enum_get(op->ptr, "type");
  bool changed_multi = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    /* Paranoia checks. */
    if (ELEM(nullptr, ob, ob->pose)) {
      continue;
    }

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_EDITABLE(ebone)) {

        /* We first need to do the flipped bone, then the original one.
         * Otherwise we can't find the flipped one because of the bone name change. */
        if (arm->flag & ARM_MIRROR_EDIT) {
          EditBone *flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if ((flipbone) && !(flipbone->flag & BONE_SELECTED)) {
            STRNCPY_UTF8(newname, flipbone->name);
            if (bone_autoside_name(newname, 1, axis, flipbone->head[axis], flipbone->tail[axis])) {
              ED_armature_bone_rename(bmain, arm, flipbone->name, newname);
              changed = true;
            }
          }
        }

        STRNCPY_UTF8(newname, ebone->name);
        if (bone_autoside_name(newname, 1, axis, ebone->head[axis], ebone->tail[axis])) {
          ED_armature_bone_rename(bmain, arm, ebone->name, newname);
          changed = true;
        }
      }
    }

    if (!changed) {
      continue;
    }

    changed_multi = true;

    /* Since we renamed stuff... */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

    /* NOTE: notifier might evolve. */
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }
  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ARMATURE_OT_autoside_names(wmOperatorType *ot)
{
  static const EnumPropertyItem axis_items[] = {
      {0, "XAXIS", 0, "X-Axis", "Left/Right"},
      {1, "YAXIS", 0, "Y-Axis", "Front/Back"},
      {2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Auto-Name by Axis";
  ot->idname = "ARMATURE_OT_autoside_names";
  ot->description =
      "Automatically renames the selected bones according to which side of the target axis they "
      "fall on";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = armature_autoside_names_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* settings */
  ot->prop = RNA_def_enum(ot->srna, "type", axis_items, 0, "Axis", "Axis to tag names with");
}

/** \} */
