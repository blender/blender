/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include "BLI_span.hh"
#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
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

#include "BLI_assert.hh"
#include "BLI_listbase.hh"
#include "BLI_set.hh"
#include "BLI_string.hh"
#include "BLI_utildefines.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_annotations.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"

#include "ANIM_action.hh"

namespace blender {

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

  if (fcu && !fcu->rna_path().is_empty()) {
    /* If we have an fcurve, call the update for the property we
     * are editing, this is then expected to do the proper redraws
     * and depsgraph updates. */
    PointerRNA ptr;
    PropertyRNA *prop;

    PointerRNA id_ptr = RNA_id_pointer_create(id);

    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path_parsed(), &ptr, &prop)) {
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
static void ale_chan_sync_group(bAnimContext *ac, bAnimListElem *ale, bActionGroup **active_agrp)
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

      if (pchan) {
        Bone *bone = pchan->bone_get(*ob);
        /* if one matches, sync the selection status */
        if (bone && (pchan->flag & POSE_SELECTED)) {
          agrp->flag |= AGRP_SELECTED;
        }
        else {
          agrp->flag &= ~AGRP_SELECTED;
        }

        /* also sync active group status */
        bArmature *arm = id_cast<bArmature *>(ob->data);
        if ((ob == ac->obact) && (bone == arm->act_bone)) {
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
        action_group_colors_set_from_posebone(agrp, {pchan, bone});
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
  if (!BLI_str_quoted_substr(fcu->rna_path().c_str(), "strips_all[", strip_name)) {
    return;
  }

  /* Check if this strip is selected. */
  Editing *ed = seq::editing_get(scene);
  if (ed == nullptr) {
    /* The existence of the F-Curve doesn't imply the existence of the sequencer
     * strip, or even the sequencer itself. */
    return;
  }
  strip = seq::get_strip_by_name(ed->current_strips(), strip_name, false);
  if (strip == nullptr) {
    return;
  }

  /* update selection status */
  if (strip->flag & SEQ_SELECT) {
    fcu->flag |= FCURVE_SELECTED;
  }
  else {
    fcu->flag &= ~FCURVE_SELECTED;
  }
}

/* perform syncing updates for F-Curves */
static void ale_chan_sync_fcurve(bAnimListElem *ale)
{
  FCurve *fcu = static_cast<FCurve *>(ale->data);
  ID *owner_id = ale->id;

  /* major priority is selection status, so refer to the checks done in `anim_filter.cc`
   * #skip_fcurve_selected_data() for reference about what's going on here.
   */
  if (ELEM(nullptr, fcu, owner_id)) {
    return;
  }

  if (fcu->rna_path().is_empty()) {
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
static void ale_chan_sync_gplayer(bAnimListElem *ale)
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

/* ----- Same functions as above, but implemented for AnimationChannels ----------- */

/**
 * Set ActionGroup select/active state based on the PoseBone select/active state.
 */
static void animchan_sync_group(ed::animfilter::AnimationChannel &channel,
                                bool &has_set_active_group)
{
  using namespace ed::animfilter;
  BLI_assert(channel.source_is_type(RNA_ActionGroup));

  ID *animated_id = channel.tags().data_of_first_tag<ID>(ChannelTagType::ID);

  /* Syncing to pose bones is only possible when the animated ID is an Object. */
  if (animated_id || animated_id->id_type() != ID_OB) {
    return;
  }

  /* Find the pose bone. Since the animation channel system already found it, we just need to get
   * the tag. */
  bPoseChannel *pchan = channel.tags().data_of_first_tag<bPoseChannel>(ChannelTagType::BONE);
  if (!pchan) {
    return;
  }

  /* NOTE: this feature will only really work if groups by default contain the F-Curves
   * for a single bone. It's the old approach that was in use before the new AnimationChannel
   * system, and should get replaced at some point.
   */
  Object *ob = id_cast<Object *>(animated_id);
  bActionGroup *agrp = channel.source().data_as<bActionGroup>();
  flag_update(agrp->flag, AGRP_SELECTED, pchan->flag & POSE_SELECTED);

  /* Sync active group status. */
  bArmature *arm = id_cast<bArmature *>(ob->data);
  Bone *bone = pchan->bone_get(*ob);

  /* If no previous group has active flag, then we're the first and only one to get it. */
  const bool do_set_active = (bone == arm->act_bone && !has_set_active_group);
  flag_update(agrp->flag, AGRP_ACTIVE, do_set_active);
  has_set_active_group |= do_set_active;

  /* Sync bone color. */
  action_group_colors_set_from_posebone(agrp, {pchan, bone});
}

static void animchan_sync_fcurve(ed::animfilter::AnimationChannel &channel)
{
  FCurve *fcu = channel.source().data_as<FCurve>();
  if (fcu->rna_path().is_empty()) {
    return;
  }

  /* Find the ID animated by this channel. */
  ID *animated_id = channel.tags().data_of_first_tag<ID>(ed::animfilter::ChannelTagType::ID);
  if (!animated_id) {
    return;
  }

  /* This function just deals with selecting F-Curves based on their VSE strip selection state, so
   * if this is about anything else, bail out. */
  if (animated_id->id_type() != ID_SCE) {
    return;
  }

  /* Only affect if F-Curve involves sequence_editor.strips. */
  char strip_name[sizeof(Strip::name)];
  if (!BLI_str_quoted_substr(fcu->rna_path().c_str(), "strips_all[", MutableCStr(strip_name))) {
    return;
  }

  /* Check if this strip is selected. */
  Editing *ed = seq::editing_get(id_cast<Scene *>(animated_id));
  if (ed == nullptr) {
    /* The existence of the F-Curve doesn't imply the existence of the sequencer
     * strip, or even the sequencer itself. */
    return;
  }
  Strip *strip = seq::get_strip_by_name(ed->current_strips(), strip_name, false);
  if (strip == nullptr) {
    return;
  }

  flag_update(fcu->flag, FCURVE_SELECTED, strip->flag & SEQ_SELECT);
}

static void animchan_sync_annotation_layer(ed::animfilter::AnimationChannel &channel)
{
  bGPDlayer *gpl = channel.source().data_as<bGPDlayer>();

  /* Make sure the selection flags agree with the "active" flag.
   * The selection flags are used in the Dope-sheet only, whereas
   * the active flag is used everywhere else. Hence, we try to
   * sync these here so that it all seems to be have as the user
   * expects - #50184
   *
   * Assume that we only really do this when the active status changes.
   * (NOTE: This may prove annoying if it means selection is always lost)
   *
   * TODO: move this away from the anim sync code, and into the GreasePencil/Annotation code for
   * setting the selection state.
   */
  flag_update(gpl->flag, GP_LAYER_SELECT, GP_LAYER_ACTIVE);
}

static void animchan_sync_greasepencil_layer(ed::animfilter::AnimationChannel &channel)
{
  using namespace blender::bke::greasepencil;

  /* TODO: move this away from the anim sync code, and into the GreasePencil/Annotation code for
   * setting the selection state. */

  GreasePencil *grease_pencil = id_cast<GreasePencil *>(channel.source().owner_id);
  Layer *layer = channel.source().data_as<Layer>();
  layer->set_selected(grease_pencil->is_layer_active(layer));
}

/* ---------------- */

void ANIM_sync_data_to_animchannels(const bContext *C)
{
  using namespace ed::animfilter;

  if (USER_EXPERIMENTAL_TEST(&U, use_new_animfilter)) {
    /* This is a slightly buggy and indirect way of doing selection syncing, but it's implemented
     * this way to be functionally the same as the non-experimental approach below.
     *
     * - This is done in every animation editor, which means it may be done too often.
     * - It is a very indirect way of doing this, because it is triggered by notifiers & managed by
     *   flags on the space runtime data.
     * - The ActionGroup syncing is mostly a workaround for the lack of actual bone groups.
     */

    ed::animfilter::ChannelArenaHandle channels = ed::animfilter::get_summary_tree_for_context(*C);
    if (!channels->root()) {
      return;
    }

    bool has_set_active_group = false;
    foreach_channel(*channels->root(), [&](AnimationChannel &channel, int /*row_index*/) {
      if (channel.source_is_type(RNA_ActionGroup)) {
        animchan_sync_group(channel, has_set_active_group);
      }
      else if (channel.source_is_type(RNA_FCurve)) {
        animchan_sync_fcurve(channel);
      }
      else if (channel.source_is_type(RNA_AnnotationLayer)) {
        animchan_sync_annotation_layer(channel);
      }
      else if (channel.source_is_type(RNA_GreasePencilLayer)) {
        animchan_sync_greasepencil_layer(channel);
      }
    });

    return;
  }

  bAnimContext ac;
  ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
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
  for (bAnimListElem &ale : anim_data) {
    switch (ale.type) {
      case ANIMTYPE_GROUP:
        ale_chan_sync_group(&ac, &ale, &active_agrp);
        break;

      case ANIMTYPE_FCURVE:
        ale_chan_sync_fcurve(&ale);
        break;

      case ANIMTYPE_GPLAYER:
        ale_chan_sync_gplayer(&ale);
        break;
      case ANIMTYPE_GREASE_PENCIL_LAYER: {
        using namespace blender::bke::greasepencil;
        GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale.id);
        Layer *layer = static_cast<Layer *>(ale.data);
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

void ANIM_animdata_update(bAnimContext *ac, ListBaseT<bAnimListElem> *anim_data)
{
  for (bAnimListElem &ale : *anim_data) {
    if (ale.type == ANIMTYPE_GPLAYER) {
      bGPDlayer *gpl = static_cast<bGPDlayer *>(ale.data);

      if (ale.update & ANIM_UPDATE_ORDER) {
        ale.update &= ~ANIM_UPDATE_ORDER;
        if (gpl) {
          BKE_annotations_layer_frames_sort(gpl, nullptr);
        }
      }

      if (ale.update & ANIM_UPDATE_DEPS) {
        ale.update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, &ale);
      }
      /* disable handles to avoid crash */
      if (ale.update & ANIM_UPDATE_HANDLES) {
        ale.update &= ~ANIM_UPDATE_HANDLES;
      }
    }
    else if (ale.datatype == ALE_MASKLAY) {
      MaskLayer *masklay = static_cast<MaskLayer *>(ale.data);

      if (ale.update & ANIM_UPDATE_ORDER) {
        ale.update &= ~ANIM_UPDATE_ORDER;
        if (masklay) {
          /* While correct & we could enable it: 'posttrans_mask_clean' currently
           * both sorts and removes doubles, so this is not necessary here. */
          // BKE_mask_layer_shape_sort(masklay);
        }
      }

      if (ale.update & ANIM_UPDATE_DEPS) {
        ale.update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, &ale);
      }
      /* Disable handles to avoid assert. */
      if (ale.update & ANIM_UPDATE_HANDLES) {
        ale.update &= ~ANIM_UPDATE_HANDLES;
      }
    }
    else if (ale.datatype == ALE_FCURVE) {
      FCurve *fcu = static_cast<FCurve *>(ale.key_data);

      if (ale.update & ANIM_UPDATE_ORDER) {
        ale.update &= ~ANIM_UPDATE_ORDER;
        if (fcu) {
          sort_time_fcurve(*fcu);
        }
      }

      if (ale.update & ANIM_UPDATE_HANDLES) {
        ale.update &= ~ANIM_UPDATE_HANDLES;
        if (fcu) {
          BKE_fcurve_handles_recalc(*fcu);
        }
      }

      if (ale.update & ANIM_UPDATE_DEPS) {
        ale.update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, &ale);
      }
    }
    else if (ELEM(ale.type,
                  ANIMTYPE_ANIMDATA,
                  ANIMTYPE_NLAACTION,
                  ANIMTYPE_NLATRACK,
                  ANIMTYPE_NLACURVE))
    {
      if (ale.update & ANIM_UPDATE_DEPS) {
        ale.update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, &ale);
      }
    }
    else if (ELEM(ale.type,
                  ANIMTYPE_GREASE_PENCIL_LAYER,
                  ANIMTYPE_GREASE_PENCIL_LAYER_GROUP,
                  ANIMTYPE_GREASE_PENCIL_DATABLOCK))
    {
      if (ale.update & ANIM_UPDATE_DEPS) {
        ale.update &= ~ANIM_UPDATE_DEPS;
        ANIM_list_elem_update(ac->bmain, ac->scene, &ale);
      }
      /* Order appears to be already handled in `grease_pencil_layer_apply_trans_data` when
       * translating. */
      ale.update &= ~(ANIM_UPDATE_HANDLES | ANIM_UPDATE_ORDER);
    }
    else if (ale.update) {
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
      ale.update = eAnim_Update_Flags(0);
    }

    BLI_assert(ale.update == 0);
  }
}

void ANIM_animdata_freelist(ListBaseT<bAnimListElem> *anim_data)
{
#ifndef NDEBUG
  bAnimListElem *ale, *ale_next;
  for (ale = anim_data->first(); ale; ale = ale_next) {
    ale_next = ale->next;
    BLI_assert(ale->update == 0);
    MEM_delete(ale);
  }
  anim_data->clear_no_delete();
#else
  anim_data->free_no_destruct();
#endif
}

void ANIM_deselect_keys_in_animation_editors(bContext *C)
{
  wmWindow *ctx_window = CTX_wm_window(C);
  ScrArea *ctx_area = CTX_wm_area(C);
  ARegion *ctx_region = CTX_wm_region(C);

  Set<bAction *> dna_actions;
  for (wmWindow &win : CTX_wm_manager(C)->windows) {
    bScreen *screen = BKE_workspace_active_screen_get(win.workspace_hook);

    for (ScrArea &area : screen->areabase) {
      if (!ELEM(area.spacetype, SPACE_GRAPH, SPACE_ACTION)) {
        continue;
      }
      ARegion *window_region = BKE_area_find_region_type(&area, RGN_TYPE_WINDOW);

      if (!window_region) {
        continue;
      }

      CTX_wm_window_set(C, &win);
      CTX_wm_area_set(C, &area);
      CTX_wm_region_set(C, window_region);
      bAnimContext ac;
      if (!ANIM_animdata_get_context(C, &ac)) {
        continue;
      }
      ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
      eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY);
      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
      for (bAnimListElem &ale : anim_data) {
        if (!ale.adt || !ale.adt->action) {
          continue;
        }
        dna_actions.add(ale.adt->action);
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

}  // namespace blender
