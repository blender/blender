/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_path.h"

#include "SEQ_sequencer.h"
#include "SEQ_utils.h"

#include "ED_anim_api.hh"

/* **************************** depsgraph tagging ******************************** */

void ANIM_list_elem_update(Main *bmain, Scene *scene, bAnimListElem *ale)
{
  ID *id;
  FCurve *fcu;
  AnimData *adt;

  id = ale->id;
  if (!id) {
    return;
  }

  /* tag AnimData for refresh so that other views will update in realtime with these changes */
  adt = BKE_animdata_from_id(id);
  if (adt) {
    DEG_id_tag_update(id, ID_RECALC_ANIMATION);
    if (adt->action != nullptr) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION);
    }
  }

  /* Tag copy on the main object if updating anything directly inside AnimData */
  if (ELEM(ale->type, ANIMTYPE_ANIMDATA, ANIMTYPE_NLAACTION, ANIMTYPE_NLATRACK, ANIMTYPE_NLACURVE))
  {
    DEG_id_tag_update(id, ID_RECALC_ANIMATION);
    return;
  }

  /* update data */
  fcu = static_cast<FCurve *>((ale->datatype == ALE_FCURVE) ? ale->key_data : nullptr);

  if (fcu && fcu->rna_path) {
    /* If we have an fcurve, call the update for the property we
     * are editing, this is then expected to do the proper redraws
     * and depsgraph updates. */
    PointerRNA id_ptr, ptr;
    PropertyRNA *prop;

    RNA_id_pointer_create(id, &id_ptr);

    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      RNA_property_update_main(bmain, scene, &ptr, prop);
    }
  }
  else {
    /* in other case we do standard depsgraph update, ideally
     * we'd be calling property update functions here too ... */
    DEG_id_tag_update(id, /* XXX: or do we want something more restrictive? */
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  }
}

void ANIM_id_update(Main *bmain, ID *id)
{
  if (id) {
    DEG_id_tag_update_ex(bmain,
                         id, /* XXX: or do we want something more restrictive? */
                         ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  }
}

/* **************************** animation data <-> data syncing ******************************** */
/* This code here is used to synchronize the
 * - selection (to find selected data easier)
 * - ... (insert other relevant items here later)
 * status in relevant Blender data with the status stored in animation channels.
 *
 * This should be called in the refresh() callbacks for various editors in
 * response to appropriate notifiers.
 */

/* perform syncing updates for Action Groups */
static void animchan_sync_group(bAnimContext *ac, bAnimListElem *ale, bActionGroup **active_agrp)
{
  bActionGroup *agrp = (bActionGroup *)ale->data;
  ID *owner_id = ale->id;

  /* major priority is selection status
   * so we need both a group and an owner
   */
  if (ELEM(nullptr, agrp, owner_id)) {
    return;
  }

  /* for standard Objects, check if group is the name of some bone */
  if (GS(owner_id->name) == ID_OB) {
    Object *ob = (Object *)owner_id;

    /* check if there are bones, and whether the name matches any
     * NOTE: this feature will only really work if groups by default contain the F-Curves
     * for a single bone.
     */
    if (ob->pose) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);
      bArmature *arm = static_cast<bArmature *>(ob->data);

      if (pchan) {
        bActionGroup *bgrp;

        /* if one matches, sync the selection status */
        if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
          agrp->flag |= AGRP_SELECTED;
        }
        else {
          agrp->flag &= ~AGRP_SELECTED;
        }

        /* also sync active group status */
        if ((ob == ac->obact) && (pchan->bone == arm->act_bone)) {
          /* if no previous F-Curve has active flag, then we're the first and only one to get it */
          if (*active_agrp == nullptr) {
            agrp->flag |= AGRP_ACTIVE;
            *active_agrp = agrp;
          }
          else {
            /* someone else has already taken it - set as not active */
            agrp->flag &= ~AGRP_ACTIVE;
          }
        }
        else {
          /* this can't possibly be active now */
          agrp->flag &= ~AGRP_ACTIVE;
        }

        /* sync group colors */
        bgrp = (bActionGroup *)BLI_findlink(&ob->pose->agroups, (pchan->agrp_index - 1));
        if (bgrp) {
          agrp->customCol = bgrp->customCol;
          action_group_colors_sync(agrp, bgrp);
        }
      }
    }
  }
}

static void animchan_sync_fcurve_scene(bAnimListElem *ale)
{
  ID *owner_id = ale->id;
  BLI_assert(GS(owner_id->name) == ID_SCE);
  Scene *scene = (Scene *)owner_id;
  FCurve *fcu = (FCurve *)ale->data;
  Sequence *seq = nullptr;

  /* Only affect if F-Curve involves sequence_editor.sequences. */
  char seq_name[sizeof(seq->name)];
  if (!BLI_str_quoted_substr(fcu->rna_path, "sequences_all[", seq_name, sizeof(seq_name))) {
    return;
  }

  /* Check if this strip is selected. */
  Editing *ed = SEQ_editing_get(scene);
  seq = SEQ_get_sequence_by_name(ed->seqbasep, seq_name, false);
  if (seq == nullptr) {
    return;
  }

  /* update selection status */
  if (seq->flag & SELECT) {
    fcu->flag |= FCURVE_SELECTED;
  }
  else {
    fcu->flag &= ~FCURVE_SELECTED;
  }
}

/* perform syncing updates for F-Curves */
static void animchan_sync_fcurve(bAnimListElem *ale)
{
  FCurve *fcu = (FCurve *)ale->data;
  ID *owner_id = ale->id;

  /* major priority is selection status, so refer to the checks done in `anim_filter.cc`
   * #skip_fcurve_selected_data() for reference about what's going on here.
   */
  if (ELEM(nullptr, fcu, fcu->rna_path, owner_id)) {
    return;
  }

  switch (GS(owner_id->name)) {
    case ID_SCE:
      animchan_sync_fcurve_scene(ale);
      break;
    default:
      break;
  }
}

/* perform syncing updates for GPencil Layers */
static void animchan_sync_gplayer(bAnimListElem *ale)
{
  bGPDlayer *gpl = (bGPDlayer *)ale->data;

  /* Make sure the selection flags agree with the "active" flag.
   * The selection flags are used in the Dopesheet only, whereas
   * the active flag is used everywhere else. Hence, we try to
   * sync these here so that it all seems to be have as the user
   * expects - #50184
   *
   * Assume that we only really do this when the active status changes.
   * (NOTE: This may prove annoying if it means selection is always lost)
   */
  if (gpl->flag & GP_LAYER_ACTIVE) {
    gpl->flag |= GP_LAYER_SELECT;
  }
  else {
    gpl->flag &= ~GP_LAYER_SELECT;
  }
}

/* ---------------- */

void ANIM_sync_animchannels_to_data(const bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  bActionGroup *active_agrp = nullptr;

  /* get animation context info for filtering the channels */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* filter data */

  /* NOTE: we want all channels, since we want to be able to set selection status on some of them
   * even when collapsed... however,
   * don't include duplicates so that selection statuses don't override each other.
   */
  filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS;
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* flush settings as appropriate depending on the types of the channels */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_GROUP:
        animchan_sync_group(&ac, ale, &active_agrp);
        break;

      case ANIMTYPE_FCURVE:
        animchan_sync_fcurve(ale);
        break;

      case ANIMTYPE_GPLAYER:
        animchan_sync_gplayer(ale);
        break;
      case ANIMTYPE_GREASE_PENCIL_LAYER:
        GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
        GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ale->data);
        if (grease_pencil->active_layer == layer) {
          layer->base.flag |= GP_LAYER_TREE_NODE_SELECT;
        }
        else {
          layer->base.flag &= ~GP_LAYER_TREE_NODE_SELECT;
        }
        break;
    }
  }

  ANIM_animdata_freelist(&anim_data);
}

void ANIM_animdata_update(bAnimContext *ac, ListBase *anim_data)
{
  LISTBASE_FOREACH (bAnimListElem *, ale, anim_data) {
    if (ale->type == ANIMTYPE_GPLAYER) {
      bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

      if (ale->update & ANIM_UPDATE_ORDER) {
        ale->update &= ~ANIM_UPDATE_ORDER;
        if (gpl) {
          BKE_gpencil_layer_frames_sort(gpl, nullptr);
        }
      }

      if (ale->update & ANIM_UPDATE_DEPS) {
        ale->update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, ale);
      }
      /* disable handles to avoid crash */
      if (ale->update & ANIM_UPDATE_HANDLES) {
        ale->update &= ~ANIM_UPDATE_HANDLES;
      }
    }
    else if (ale->datatype == ALE_MASKLAY) {
      MaskLayer *masklay = static_cast<MaskLayer *>(ale->data);

      if (ale->update & ANIM_UPDATE_ORDER) {
        ale->update &= ~ANIM_UPDATE_ORDER;
        if (masklay) {
          /* While correct & we could enable it: 'posttrans_mask_clean' currently
           * both sorts and removes doubles, so this is not necessary here. */
          // BKE_mask_layer_shape_sort(masklay);
        }
      }

      if (ale->update & ANIM_UPDATE_DEPS) {
        ale->update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, ale);
      }
      /* Disable handles to avoid assert. */
      if (ale->update & ANIM_UPDATE_HANDLES) {
        ale->update &= ~ANIM_UPDATE_HANDLES;
      }
    }
    else if (ale->datatype == ALE_FCURVE) {
      FCurve *fcu = static_cast<FCurve *>(ale->key_data);

      if (ale->update & ANIM_UPDATE_ORDER) {
        ale->update &= ~ANIM_UPDATE_ORDER;
        if (fcu) {
          sort_time_fcurve(fcu);
        }
      }

      if (ale->update & ANIM_UPDATE_HANDLES) {
        ale->update &= ~ANIM_UPDATE_HANDLES;
        if (fcu) {
          BKE_fcurve_handles_recalc(fcu);
        }
      }

      if (ale->update & ANIM_UPDATE_DEPS) {
        ale->update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, ale);
      }
    }
    else if (ELEM(ale->type,
                  ANIMTYPE_ANIMDATA,
                  ANIMTYPE_NLAACTION,
                  ANIMTYPE_NLATRACK,
                  ANIMTYPE_NLACURVE))
    {
      if (ale->update & ANIM_UPDATE_DEPS) {
        ale->update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, ale);
      }
    }
    else if (ale->update) {
#if 0
      if (G.debug & G_DEBUG) {
        printf("%s: Unhandled animchannel updates (%d) for type=%d (%p)\n",
               __func__,
               ale->update,
               ale->type,
               ale->data);
      }
#endif
      /* Prevent crashes in cases where it can't be handled */
      ale->update = 0;
    }

    BLI_assert(ale->update == 0);
  }
}

void ANIM_animdata_freelist(ListBase *anim_data)
{
#ifndef NDEBUG
  bAnimListElem *ale, *ale_next;
  for (ale = static_cast<bAnimListElem *>(anim_data->first); ale; ale = ale_next) {
    ale_next = ale->next;
    BLI_assert(ale->update == 0);
    MEM_freeN(ale);
  }
  BLI_listbase_clear(anim_data);
#else
  BLI_freelistN(anim_data);
#endif
}
