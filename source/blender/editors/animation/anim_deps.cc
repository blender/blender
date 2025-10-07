/* SPDX-FileCopyrightText: 2008 Blender Authors
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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "ED_anim_api.hh"

#include "ANIM_action.hh"

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
    PointerRNA ptr;
    PropertyRNA *prop;

    PointerRNA id_ptr = RNA_id_pointer_create(id);

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
  bActionGroup *agrp = static_cast<bActionGroup *>(ale->data);
  ID *owner_id = ale->id;

  /* major priority is selection status
   * so we need both a group and an owner
   */
  if (ELEM(nullptr, agrp, owner_id)) {
    return;
  }

  /* for standard Objects, check if group is the name of some bone */
  if (GS(owner_id->name) == ID_OB) {
    Object *ob = reinterpret_cast<Object *>(owner_id);

    /* check if there are bones, and whether the name matches any
     * NOTE: this feature will only really work if groups by default contain the F-Curves
     * for a single bone.
     */
    if (ob->pose) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);
      bArmature *arm = static_cast<bArmature *>(ob->data);

      if (pchan) {
        /* if one matches, sync the selection status */
        if ((pchan->bone) && (pchan->flag & POSE_SELECTED)) {
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

        /* sync bone color */
        action_group_colors_set_from_posebone(agrp, pchan);
      }
    }
  }
}

static void animchan_sync_fcurve_scene(bAnimListElem *ale)
{
  ID *owner_id = ale->id;
  BLI_assert(GS(owner_id->name) == ID_SCE);
  Scene *scene = reinterpret_cast<Scene *>(owner_id);
  FCurve *fcu = static_cast<FCurve *>(ale->data);
  Strip *strip = nullptr;

  /* Only affect if F-Curve involves sequence_editor.strips. */
  char strip_name[sizeof(strip->name)];
  if (!BLI_str_quoted_substr(fcu->rna_path, "strips_all[", strip_name, sizeof(strip_name))) {
    return;
  }

  /* Check if this strip is selected. */
  Editing *ed = blender::seq::editing_get(scene);
  if (ed == nullptr) {
    /* The existence of the F-Curve doesn't imply the existence of the sequencer
     * strip, or even the sequencer itself. */
    return;
  }
  strip = blender::seq::get_strip_by_name(ed->current_strips(), strip_name, false);
  if (strip == nullptr) {
    return;
  }

  /* update selection status */
  if (strip->flag & SELECT) {
    fcu->flag |= FCURVE_SELECTED;
  }
  else {
    fcu->flag &= ~FCURVE_SELECTED;
  }
}

/* perform syncing updates for F-Curves */
static void animchan_sync_fcurve(bAnimListElem *ale)
{
  FCurve *fcu = static_cast<FCurve *>(ale->data);
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
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

  /* Make sure the selection flags agree with the "active" flag.
   * The selection flags are used in the Dope-sheet only, whereas
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
      case ANIMTYPE_GREASE_PENCIL_LAYER: {
        using namespace blender::bke::greasepencil;
        GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
        Layer *layer = static_cast<Layer *>(ale->data);
        layer->set_selected(grease_pencil->is_layer_active(layer));
        break;
      }

      case ANIMTYPE_NONE:
      case ANIMTYPE_ANIMDATA:
      case ANIMTYPE_SPECIALDATA__UNUSED:
      case ANIMTYPE_SUMMARY:
      case ANIMTYPE_SCENE:
      case ANIMTYPE_OBJECT:
      case ANIMTYPE_NLACONTROLS:
      case ANIMTYPE_NLACURVE:
      case ANIMTYPE_FILLACT_LAYERED:
      case ANIMTYPE_ACTION_SLOT:
      case ANIMTYPE_FILLACTD:
      case ANIMTYPE_FILLDRIVERS:
      case ANIMTYPE_DSMAT:
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
      case ANIMTYPE_DSNTREE:
      case ANIMTYPE_DSPART:
      case ANIMTYPE_DSMBALL:
      case ANIMTYPE_DSARM:
      case ANIMTYPE_DSMESH:
      case ANIMTYPE_DSTEX:
      case ANIMTYPE_DSLAT:
      case ANIMTYPE_DSLINESTYLE:
      case ANIMTYPE_DSSPK:
      case ANIMTYPE_DSGPENCIL:
      case ANIMTYPE_DSMCLIP:
      case ANIMTYPE_DSHAIR:
      case ANIMTYPE_DSPOINTCLOUD:
      case ANIMTYPE_DSVOLUME:
      case ANIMTYPE_DSLIGHTPROBE:
      case ANIMTYPE_SHAPEKEY:
      case ANIMTYPE_GREASE_PENCIL_DATABLOCK:
      case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP:
      case ANIMTYPE_MASKDATABLOCK:
      case ANIMTYPE_MASKLAYER:
      case ANIMTYPE_NLATRACK:
      case ANIMTYPE_NLAACTION:
      case ANIMTYPE_PALETTE:
      case ANIMTYPE_NUM_TYPES:
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
    else if (ELEM(ale->type,
                  ANIMTYPE_GREASE_PENCIL_LAYER,
                  ANIMTYPE_GREASE_PENCIL_LAYER_GROUP,
                  ANIMTYPE_GREASE_PENCIL_DATABLOCK))
    {
      if (ale->update & ANIM_UPDATE_DEPS) {
        ale->update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, ale);
      }
      /* Order appears to be already handled in `grease_pencil_layer_apply_trans_data` when
       * translating. */
      ale->update &= ~(ANIM_UPDATE_HANDLES | ANIM_UPDATE_ORDER);
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
      ale->update = eAnim_Update_Flags(0);
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

void ANIM_deselect_keys_in_animation_editors(bContext *C)
{
  using namespace blender;

  wmWindow *ctx_window = CTX_wm_window(C);
  ScrArea *ctx_area = CTX_wm_area(C);
  ARegion *ctx_region = CTX_wm_region(C);

  Set<bAction *> dna_actions;
  LISTBASE_FOREACH (wmWindow *, win, &CTX_wm_manager(C)->windows) {
    bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (!ELEM(area->spacetype, SPACE_GRAPH, SPACE_ACTION)) {
        continue;
      }
      ARegion *window_region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

      if (!window_region) {
        continue;
      }

      CTX_wm_window_set(C, win);
      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, window_region);
      bAnimContext ac;
      if (!ANIM_animdata_get_context(C, &ac)) {
        continue;
      }
      ListBase anim_data = {nullptr, nullptr};
      eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY);
      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
      LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
        if (!ale->adt || !ale->adt->action) {
          continue;
        }
        dna_actions.add(ale->adt->action);
      }
      ANIM_animdata_freelist(&anim_data);
    }
  }

  CTX_wm_window_set(C, ctx_window);
  CTX_wm_area_set(C, ctx_area);
  CTX_wm_region_set(C, ctx_region);

  for (bAction *dna_action : dna_actions) {
    animrig::action_deselect_keys(dna_action->wrap());
  }
}
