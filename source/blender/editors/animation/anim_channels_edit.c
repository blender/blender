/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation, Joshua Leung. All rights reserved. */

/** \file
 * \ingroup edanimation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mask.h"
#include "BKE_nla.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_keyframes_edit.h" /* XXX move the select modes out of there! */
#include "ED_markers.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "WM_api.h"
#include "WM_types.h"

/* -------------------------------------------------------------------- */
/** \name Channel helper functions
 * \{ */

static bool get_normalized_fcurve_bounds(FCurve *fcu,
                                         bAnimContext *ac,
                                         const bAnimListElem *ale,
                                         const bool include_handles,
                                         const float range[2],
                                         rctf *r_bounds)
{
  const bool fcu_selection_only = false;
  const bool found_bounds = BKE_fcurve_calc_bounds(
      fcu, fcu_selection_only, include_handles, range, r_bounds);

  if (!found_bounds) {
    return false;
  }

  const short mapping_flag = ANIM_get_normalization_flags(ac);

  float offset;
  const float unit_fac = ANIM_unit_mapping_get_factor(
      ac->scene, ale->id, fcu, mapping_flag, &offset);

  r_bounds->ymin = (r_bounds->ymin + offset) * unit_fac;
  r_bounds->ymax = (r_bounds->ymax + offset) * unit_fac;

  const float min_height = 0.01f;
  const float height = BLI_rctf_size_y(r_bounds);
  if (height < min_height) {
    r_bounds->ymin -= (min_height - height) / 2;
    r_bounds->ymax += (min_height - height) / 2;
  }
  return true;
}

static bool get_gpencil_bounds(bGPDlayer *gpl, const float range[2], rctf *r_bounds)
{
  bool found_start = false;
  int start_frame = 0;
  int end_frame = 1;
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->framenum < range[0]) {
      continue;
    }
    if (gpf->framenum > range[1]) {
      break;
    }
    if (!found_start) {
      start_frame = gpf->framenum;
      found_start = true;
    }
    end_frame = gpf->framenum;
  }
  r_bounds->xmin = start_frame;
  r_bounds->xmax = end_frame;
  r_bounds->ymin = 0;
  r_bounds->ymax = 1;

  return found_start;
}

static bool get_channel_bounds(bAnimContext *ac,
                               bAnimListElem *ale,
                               const float range[2],
                               const bool include_handles,
                               rctf *r_bounds)
{
  bool found_bounds = false;
  switch (ale->datatype) {
    case ALE_GPFRAME: {
      bGPDlayer *gpl = (bGPDlayer *)ale->data;
      found_bounds = get_gpencil_bounds(gpl, range, r_bounds);
      break;
    }
    case ALE_FCURVE: {
      FCurve *fcu = (FCurve *)ale->key_data;
      found_bounds = get_normalized_fcurve_bounds(fcu, ac, ale, include_handles, range, r_bounds);
      break;
    }
  }
  return found_bounds;
}

/* Pad the given rctf with regions that could block the view.
 * For example Markers and Time Scrubbing. */
static void add_region_padding(bContext *C, ARegion *region, rctf *bounds)
{
  BLI_rctf_scale(bounds, 1.1f);

  const float pad_top = UI_TIME_SCRUB_MARGIN_Y;
  const float pad_bottom = BLI_listbase_is_empty(ED_context_get_markers(C)) ?
                               V2D_SCROLL_HANDLE_HEIGHT :
                               UI_MARKER_MARGIN_Y;
  BLI_rctf_pad_y(bounds, region->winy, pad_bottom, pad_top);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Channel Selection API
 * \{ */

void ANIM_set_active_channel(bAnimContext *ac,
                             void *data,
                             eAnimCont_Types datatype,
                             eAnimFilter_Flags filter,
                             void *channel_data,
                             eAnim_ChannelType channel_type)
{
  /* TODO: extend for animdata types. */

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;

  /* try to build list of filtered items */
  ANIM_animdata_filter(ac, &anim_data, filter, data, datatype);
  if (BLI_listbase_is_empty(&anim_data)) {
    return;
  }

  /* only clear the 'active' flag for the channels of the same type */
  for (ale = anim_data.first; ale; ale = ale->next) {
    /* skip if types don't match */
    if (channel_type != ale->type) {
      continue;
    }

    /* flag to set depends on type */
    switch (ale->type) {
      case ANIMTYPE_GROUP: {
        bActionGroup *agrp = (bActionGroup *)ale->data;

        ACHANNEL_SET_FLAG(agrp, ACHANNEL_SETFLAG_CLEAR, AGRP_ACTIVE);
        break;
      }
      case ANIMTYPE_FCURVE:
      case ANIMTYPE_NLACURVE: {
        FCurve *fcu = (FCurve *)ale->data;

        ACHANNEL_SET_FLAG(fcu, ACHANNEL_SETFLAG_CLEAR, FCURVE_ACTIVE);
        break;
      }
      case ANIMTYPE_NLATRACK: {
        NlaTrack *nlt = (NlaTrack *)ale->data;

        ACHANNEL_SET_FLAG(nlt, ACHANNEL_SETFLAG_CLEAR, NLATRACK_ACTIVE);
        break;
      }
      case ANIMTYPE_FILLACTD: /* Action Expander */
      case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
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
      case ANIMTYPE_NLAACTION:
      case ANIMTYPE_DSSIMULATION: {
        /* need to verify that this data is valid for now */
        if (ale->adt) {
          ACHANNEL_SET_FLAG(ale->adt, ACHANNEL_SETFLAG_CLEAR, ADT_UI_ACTIVE);
        }
        break;
      }
      case ANIMTYPE_GPLAYER: {
        bGPDlayer *gpl = (bGPDlayer *)ale->data;

        ACHANNEL_SET_FLAG(gpl, ACHANNEL_SETFLAG_CLEAR, GP_LAYER_ACTIVE);
        break;
      }
    }
  }

  /* set active flag */
  if (channel_data) {
    switch (channel_type) {
      case ANIMTYPE_GROUP: {
        bActionGroup *agrp = (bActionGroup *)channel_data;
        agrp->flag |= AGRP_ACTIVE;
        break;
      }
      case ANIMTYPE_FCURVE:
      case ANIMTYPE_NLACURVE: {
        FCurve *fcu = (FCurve *)channel_data;
        fcu->flag |= FCURVE_ACTIVE;
        break;
      }
      case ANIMTYPE_NLATRACK: {
        NlaTrack *nlt = (NlaTrack *)channel_data;
        nlt->flag |= NLATRACK_ACTIVE;
        break;
      }
      case ANIMTYPE_FILLACTD: /* Action Expander */
      case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
      case ANIMTYPE_DSPART:
      case ANIMTYPE_DSMBALL:
      case ANIMTYPE_DSARM:
      case ANIMTYPE_DSMESH:
      case ANIMTYPE_DSLAT:
      case ANIMTYPE_DSLINESTYLE:
      case ANIMTYPE_DSSPK:
      case ANIMTYPE_DSNTREE:
      case ANIMTYPE_DSTEX:
      case ANIMTYPE_DSGPENCIL:
      case ANIMTYPE_DSMCLIP:
      case ANIMTYPE_DSHAIR:
      case ANIMTYPE_DSPOINTCLOUD:
      case ANIMTYPE_DSVOLUME:
      case ANIMTYPE_NLAACTION:
      case ANIMTYPE_DSSIMULATION: {
        /* need to verify that this data is valid for now */
        if (ale && ale->adt) {
          ale->adt->flag |= ADT_UI_ACTIVE;
        }
        break;
      }

      case ANIMTYPE_GPLAYER: {
        bGPDlayer *gpl = (bGPDlayer *)channel_data;
        gpl->flag |= GP_LAYER_ACTIVE;
        break;
      }

      /* unhandled currently, but may be interesting */
      case ANIMTYPE_MASKLAYER:
      case ANIMTYPE_SHAPEKEY:
        break;

      /* other types */
      default:
        break;
    }
  }

  /* clean up */
  ANIM_animdata_freelist(&anim_data);
}

bool ANIM_is_active_channel(bAnimListElem *ale)
{
  switch (ale->type) {
    case ANIMTYPE_FILLACTD: /* Action Expander */
    case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
    case ANIMTYPE_DSLAM:
    case ANIMTYPE_DSCAM:
    case ANIMTYPE_DSCACHEFILE:
    case ANIMTYPE_DSCUR:
    case ANIMTYPE_DSSKEY:
    case ANIMTYPE_DSWOR:
    case ANIMTYPE_DSPART:
    case ANIMTYPE_DSMBALL:
    case ANIMTYPE_DSARM:
    case ANIMTYPE_DSMESH:
    case ANIMTYPE_DSNTREE:
    case ANIMTYPE_DSTEX:
    case ANIMTYPE_DSLAT:
    case ANIMTYPE_DSLINESTYLE:
    case ANIMTYPE_DSSPK:
    case ANIMTYPE_DSGPENCIL:
    case ANIMTYPE_DSMCLIP:
    case ANIMTYPE_DSHAIR:
    case ANIMTYPE_DSPOINTCLOUD:
    case ANIMTYPE_DSVOLUME:
    case ANIMTYPE_NLAACTION:
    case ANIMTYPE_DSSIMULATION: {
      return ale->adt && (ale->adt->flag & ADT_UI_ACTIVE);
    }
    case ANIMTYPE_GROUP: {
      bActionGroup *argp = (bActionGroup *)ale->data;
      return argp->flag & AGRP_ACTIVE;
    }
    case ANIMTYPE_FCURVE:
    case ANIMTYPE_NLACURVE: {
      FCurve *fcu = (FCurve *)ale->data;
      return fcu->flag & FCURVE_ACTIVE;
    }
    case ANIMTYPE_GPLAYER: {
      bGPDlayer *gpl = (bGPDlayer *)ale->data;
      return gpl->flag & GP_LAYER_ACTIVE;
    }
    /* These channel types do not have active flags. */
    case ANIMTYPE_MASKLAYER:
    case ANIMTYPE_SHAPEKEY:
      break;
  }
  return false;
}

/* change_active determines whether to change the active bone of the armature when selecting pose
 * channels. It is false during range selection otherwise true. */
static void select_pchan_for_action_group(bAnimContext *ac,
                                          bActionGroup *agrp,
                                          bAnimListElem *ale,
                                          const bool change_active)
{
  /* Armatures-Specific Feature:
   * See mouse_anim_channels() -> ANIMTYPE_GROUP case for more details (#38737)
   */
  if ((ac->ads->filterflag & ADS_FILTER_ONLYSEL) == 0) {
    if ((ale->id) && (GS(ale->id->name) == ID_OB)) {
      Object *ob = (Object *)ale->id;
      if (ob->type == OB_ARMATURE) {
        /* Assume for now that any group with corresponding name is what we want
         * (i.e. for an armature whose location is animated, things would break
         * if the user were to add a bone named "Location").
         *
         * TODO: check the first F-Curve or so to be sure...
         */
        bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);

        if (agrp->flag & AGRP_SELECTED) {
          ED_pose_bone_select(ob, pchan, true, change_active);
        }
        else {
          ED_pose_bone_select(ob, pchan, false, change_active);
        }
      }
    }
  }
}

static ListBase /*bAnimListElem*/ anim_channels_for_selection(bAnimContext *ac)
{
  ListBase anim_data = {NULL, NULL};

  /* filter data */
  /* NOTE: no list visible, otherwise, we get dangling */
  const int filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  return anim_data;
}

static eAnimChannels_SetFlag anim_channels_selection_flag_for_toggle(const ListBase anim_data)
{
  /* See if we should be selecting or deselecting. */
  for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
    switch (ale->type) {
      case ANIMTYPE_SCENE:
        if (ale->flag & SCE_DS_SELECTED) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      case ANIMTYPE_OBJECT:
#if 0 /* for now, do not take object selection into account, since it gets too annoying */
          if (ale->flag & SELECT) {
            return ACHANNEL_SETFLAG_CLEAR;
          }
#endif
        break;
      case ANIMTYPE_GROUP:
        if (ale->flag & AGRP_SELECTED) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      case ANIMTYPE_FCURVE:
      case ANIMTYPE_NLACURVE:
        if (ale->flag & FCURVE_SELECTED) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      case ANIMTYPE_SHAPEKEY:
        if (ale->flag & KEYBLOCK_SEL) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      case ANIMTYPE_NLATRACK:
        if (ale->flag & NLATRACK_SELECTED) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;

      case ANIMTYPE_FILLACTD: /* Action Expander */
      case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
      case ANIMTYPE_DSPART:
      case ANIMTYPE_DSMBALL:
      case ANIMTYPE_DSARM:
      case ANIMTYPE_DSMESH:
      case ANIMTYPE_DSNTREE:
      case ANIMTYPE_DSTEX:
      case ANIMTYPE_DSLAT:
      case ANIMTYPE_DSLINESTYLE:
      case ANIMTYPE_DSSPK:
      case ANIMTYPE_DSGPENCIL:
      case ANIMTYPE_DSMCLIP:
      case ANIMTYPE_DSHAIR:
      case ANIMTYPE_DSPOINTCLOUD:
      case ANIMTYPE_DSVOLUME:
      case ANIMTYPE_NLAACTION:
      case ANIMTYPE_DSSIMULATION: {
        if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED)) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      }
      case ANIMTYPE_GPLAYER:
        if (ale->flag & GP_LAYER_SELECT) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
      case ANIMTYPE_MASKLAYER:
        if (ale->flag & MASK_LAYERFLAG_SELECT) {
          return ACHANNEL_SETFLAG_CLEAR;
        }
        break;
    }
  }

  return ACHANNEL_SETFLAG_ADD;
}

static void anim_channels_select_set(bAnimContext *ac,
                                     const ListBase anim_data,
                                     eAnimChannels_SetFlag sel)
{
  bAnimListElem *ale;
  /* Boolean to keep active channel status during range selection. */
  const bool change_active = (sel != ACHANNEL_SETFLAG_EXTEND_RANGE);

  for (ale = anim_data.first; ale; ale = ale->next) {
    switch (ale->type) {
      case ANIMTYPE_SCENE: {
        if (change_active) {
          break;
        }
        Scene *scene = (Scene *)ale->data;

        ACHANNEL_SET_FLAG(scene, sel, SCE_DS_SELECTED);

        if (scene->adt) {
          ACHANNEL_SET_FLAG(scene, sel, ADT_UI_SELECTED);
        }
        break;
      }
      case ANIMTYPE_OBJECT: {
#if 0 /* for now, do not take object selection into account, since it gets too annoying */
        Base *base = (Base *)ale->data;
        Object *ob = base->object;

        ACHANNEL_SET_FLAG(base, sel, SELECT);
        ACHANNEL_SET_FLAG(ob, sel, SELECT);

        if (ob->adt) {
          ACHANNEL_SET_FLAG(ob, sel, ADT_UI_SELECTED);
        }
#endif
        break;
      }
      case ANIMTYPE_GROUP: {
        bActionGroup *agrp = (bActionGroup *)ale->data;
        ACHANNEL_SET_FLAG(agrp, sel, AGRP_SELECTED);
        select_pchan_for_action_group(ac, agrp, ale, change_active);
        if (change_active) {
          agrp->flag &= ~AGRP_ACTIVE;
        }
        break;
      }
      case ANIMTYPE_FCURVE:
      case ANIMTYPE_NLACURVE: {
        FCurve *fcu = (FCurve *)ale->data;

        ACHANNEL_SET_FLAG(fcu, sel, FCURVE_SELECTED);
        if (!(fcu->flag & FCURVE_SELECTED) && change_active) {
          /* Only erase the ACTIVE flag when deselecting. This ensures that "select all curves"
           * retains the currently active curve. */
          fcu->flag &= ~FCURVE_ACTIVE;
        }
        break;
      }
      case ANIMTYPE_SHAPEKEY: {
        KeyBlock *kb = (KeyBlock *)ale->data;

        ACHANNEL_SET_FLAG(kb, sel, KEYBLOCK_SEL);
        break;
      }
      case ANIMTYPE_NLATRACK: {
        NlaTrack *nlt = (NlaTrack *)ale->data;

        ACHANNEL_SET_FLAG(nlt, sel, NLATRACK_SELECTED);
        nlt->flag &= ~NLATRACK_ACTIVE;
        break;
      }
      case ANIMTYPE_FILLACTD: /* Action Expander */
      case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
      case ANIMTYPE_DSPART:
      case ANIMTYPE_DSMBALL:
      case ANIMTYPE_DSARM:
      case ANIMTYPE_DSMESH:
      case ANIMTYPE_DSNTREE:
      case ANIMTYPE_DSTEX:
      case ANIMTYPE_DSLAT:
      case ANIMTYPE_DSLINESTYLE:
      case ANIMTYPE_DSSPK:
      case ANIMTYPE_DSGPENCIL:
      case ANIMTYPE_DSMCLIP:
      case ANIMTYPE_DSHAIR:
      case ANIMTYPE_DSPOINTCLOUD:
      case ANIMTYPE_DSVOLUME:
      case ANIMTYPE_NLAACTION:
      case ANIMTYPE_DSSIMULATION: {
        /* need to verify that this data is valid for now */
        if (ale->adt) {
          ACHANNEL_SET_FLAG(ale->adt, sel, ADT_UI_SELECTED);
          if (change_active) {
            ale->adt->flag &= ~ADT_UI_ACTIVE;
          }
        }
        break;
      }
      case ANIMTYPE_GPLAYER: {
        bGPDlayer *gpl = (bGPDlayer *)ale->data;

        ACHANNEL_SET_FLAG(gpl, sel, GP_LAYER_SELECT);
        break;
      }
      case ANIMTYPE_MASKLAYER: {
        MaskLayer *masklay = (MaskLayer *)ale->data;

        ACHANNEL_SET_FLAG(masklay, sel, MASK_LAYERFLAG_SELECT);
        break;
      }
    }
  }
}

void ANIM_anim_channels_select_set(bAnimContext *ac, eAnimChannels_SetFlag sel)
{
  ListBase anim_data = anim_channels_for_selection(ac);
  anim_channels_select_set(ac, anim_data, sel);
  ANIM_animdata_freelist(&anim_data);
}

void ANIM_anim_channels_select_toggle(bAnimContext *ac)
{
  ListBase anim_data = anim_channels_for_selection(ac);
  const eAnimChannels_SetFlag sel = anim_channels_selection_flag_for_toggle(anim_data);
  anim_channels_select_set(ac, anim_data, sel);
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Graph Editor API
 * \{ */

/* Copy a certain channel setting to parents of the modified channel. */
static void anim_flush_channel_setting_up(bAnimContext *ac,
                                          const eAnimChannel_Settings setting,
                                          const eAnimChannels_SetFlag mode,
                                          bAnimListElem *const match,
                                          const int matchLevel)
{
  /* flush up?
   *
   * For Visibility:
   * - only flush up if the current state is now enabled (positive 'on' state is default)
   *   (otherwise, it's too much work to force the parents to be inactive too)
   *
   * For everything else:
   * - only flush up if the current state is now disabled (negative 'off' state is default)
   *   (otherwise, it's too much work to force the parents to be active too)
   */
  if (setting == ACHANNEL_SETTING_VISIBLE) {
    if (mode == ACHANNEL_SETFLAG_CLEAR) {
      return;
    }
  }
  else {
    if (mode != ACHANNEL_SETFLAG_CLEAR) {
      return;
    }
  }

  /* Go backwards in the list, until the highest-ranking element
   * (by indentation has been covered). */
  int prevLevel = matchLevel;
  for (bAnimListElem *ale = match->prev; ale; ale = ale->prev) {
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

    /* if no channel info was found, skip, since this type might not have any useful info */
    if (acf == NULL) {
      continue;
    }

    /* Get the level of the current channel traversed
     *   - we define the level as simply being the offset for the start of the channel
     */
    const int level = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;

    if (level == prevLevel) {
      /* Don't influence siblings. */
      continue;
    }

    if (level > prevLevel) {
      /* If previous level was a base-level (i.e. 0 offset / root of one hierarchy), stop here. */
      if (prevLevel == 0) {
        return;
      }

      /* Otherwise, this level weaves into another sibling hierarchy to the previous one just
       * finished, so skip until we get to the parent of this level. */
      continue;
    }

    /* The level is 'less than' (i.e. more important) the level we're matching but also 'less
     * than' the level just tried (i.e. only the 1st group above grouped F-Curves, when toggling
     * visibility of F-Curves, gets flushed, which should happen if we don't let prevLevel get
     * updated below once the first 1st group is found). */
    ANIM_channel_setting_set(ac, ale, setting, mode);

    /* store this level as the 'old' level now */
    prevLevel = level;
  }
}

/* Copy a certain channel setting to children of the modified channel. */
static void anim_flush_channel_setting_down(bAnimContext *ac,
                                            const eAnimChannel_Settings setting,
                                            const eAnimChannels_SetFlag mode,
                                            bAnimListElem *const match,
                                            const int matchLevel)
{
  /* go forwards in the list, until the lowest-ranking element (by indentation has been covered) */
  for (bAnimListElem *ale = match->next; ale; ale = ale->next) {
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

    /* if no channel info was found, skip, since this type might not have any useful info */
    if (acf == NULL) {
      continue;
    }

    /* get the level of the current channel traversed
     *   - we define the level as simply being the offset for the start of the channel
     */
    const int level = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;

    /* if the level is 'greater than' (i.e. less important) the channel that was changed,
     * flush the new status...
     */
    if (level > matchLevel) {
      ANIM_channel_setting_set(ac, ale, setting, mode);
      /* however, if the level is 'less than or equal to' the channel that was changed,
       * (i.e. the current channel is as important if not more important than the changed
       * channel) then we should stop, since we've found the last one of the children we should
       * flush
       */
    }
    else {
      break;
    }
  }
}

void ANIM_flush_setting_anim_channels(bAnimContext *ac,
                                      ListBase *anim_data,
                                      bAnimListElem *ale_setting,
                                      eAnimChannel_Settings setting,
                                      eAnimChannels_SetFlag mode)
{
  bAnimListElem *ale, *match = NULL;
  int matchLevel = 0;

  /* sanity check */
  if (ELEM(NULL, anim_data, anim_data->first)) {
    return;
  }

  if (setting == ACHANNEL_SETTING_ALWAYS_VISIBLE) {
    return;
  }

  /* find the channel that got changed */
  for (ale = anim_data->first; ale; ale = ale->next) {
    /* compare data, and type as main way of identifying the channel */
    if ((ale->data == ale_setting->data) && (ale->type == ale_setting->type)) {
      /* We also have to check the ID, this is assigned to,
       * since a block may have multiple users. */
      /* TODO: is the owner-data more revealing? */
      if (ale->id == ale_setting->id) {
        match = ale;
        break;
      }
    }
  }
  if (match == NULL) {
    printf("ERROR: no channel matching the one changed was found\n");
    return;
  }

  {
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale_setting);
    if (acf == NULL) {
      printf("ERROR: no channel info for the changed channel\n");
      return;
    }

    /* get the level of the channel that was affected
     *   - we define the level as simply being the offset for the start of the channel
     */
    matchLevel = (acf->get_offset) ? acf->get_offset(ac, ale_setting) : 0;
  }

  anim_flush_channel_setting_up(ac, setting, mode, match, matchLevel);
  anim_flush_channel_setting_down(ac, setting, mode, match, matchLevel);
}

void ANIM_frame_channel_y_extents(bContext *C, bAnimContext *ac)
{

  ARegion *window_region = BKE_area_find_region_type(ac->area, RGN_TYPE_WINDOW);

  if (!window_region) {
    return;
  }

  ListBase anim_data = {NULL, NULL};
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS |
                      ANIMFILTER_FCURVESONLY | ANIMFILTER_CURVE_VISIBLE);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  rctf bounds = {.xmin = FLT_MAX, .xmax = -FLT_MAX, .ymin = FLT_MAX, .ymax = -FLT_MAX};
  const bool include_handles = false;
  float frame_range[2] = {window_region->v2d.cur.xmin, window_region->v2d.cur.xmax};
  if (ac->scene->r.flag & SCER_PRV_RANGE) {
    frame_range[0] = ac->scene->r.psfra;
    frame_range[1] = ac->scene->r.pefra;
  }

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    rctf channel_bounds;
    const bool found_bounds = get_channel_bounds(
        ac, ale, frame_range, include_handles, &channel_bounds);
    if (found_bounds) {
      BLI_rctf_union(&bounds, &channel_bounds);
    }
  }

  if (!BLI_rctf_is_valid(&bounds)) {
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  add_region_padding(C, window_region, &bounds);

  window_region->v2d.cur.ymin = bounds.ymin;
  window_region->v2d.cur.ymax = bounds.ymax;

  ANIM_animdata_freelist(&anim_data);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Public F-Curves API
 * \{ */

void ANIM_fcurve_delete_from_animdata(bAnimContext *ac, AnimData *adt, FCurve *fcu)
{
  /* - if no AnimData, we've got nowhere to remove the F-Curve from
   *   (this doesn't guarantee that the F-Curve is in there, but at least we tried
   * - if no F-Curve, there is nothing to remove
   */
  if (ELEM(NULL, adt, fcu)) {
    return;
  }

  /* remove from whatever list it came from
   * - Action Group
   * - Action
   * - Drivers
   * - TODO... some others?
   */
  if ((ac) && (ac->datatype == ANIMCONT_DRIVERS)) {
    /* driver F-Curve */
    BLI_remlink(&adt->drivers, fcu);
  }
  else if (adt->action) {
    bAction *act = adt->action;

    /* remove from group or action, whichever one "owns" the F-Curve */
    if (fcu->grp) {
      bActionGroup *agrp = fcu->grp;

      /* remove F-Curve from group+action */
      action_groups_remove_channel(act, fcu);

      /* if group has no more channels, remove it too,
       * otherwise can have many dangling groups #33541.
       */
      if (BLI_listbase_is_empty(&agrp->channels)) {
        BLI_freelinkN(&act->groups, agrp);
      }
    }
    else {
      BLI_remlink(&act->curves, fcu);
    }

    /* if action has no more F-Curves as a result of this, unlink it from
     * AnimData if it did not come from a NLA Strip being tweaked.
     *
     * This is done so that we don't have dangling Object+Action entries in
     * channel list that are empty, and linger around long after the data they
     * are for has disappeared (and probably won't come back).
     */
    ANIM_remove_empty_action_from_animdata(adt);
  }

  /* free the F-Curve itself */
  BKE_fcurve_free(fcu);
}

bool ANIM_remove_empty_action_from_animdata(struct AnimData *adt)
{
  if (adt->action != NULL) {
    bAction *act = adt->action;

    if (BLI_listbase_is_empty(&act->curves) && (adt->flag & ADT_NLA_EDIT_ON) == 0) {
      id_us_min(&act->id);
      adt->action = NULL;
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Utilities
 * \{ */

/* poll callback for being in an Animation Editor channels list region */
static bool animedit_poll_channels_active(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* channels region test */
  /* TODO: could enhance with actually testing if channels region? */
  if (ELEM(NULL, area, CTX_wm_region(C))) {
    return false;
  }
  /* animation editor test */
  if (ELEM(area->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA) == 0) {
    return false;
  }

  return true;
}

/* Poll callback for Animation Editor channels list region + not in NLA-tweak-mode for NLA. */
static bool animedit_poll_channels_nla_tweakmode_off(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);

  /* channels region test */
  /* TODO: could enhance with actually testing if channels region? */
  if (ELEM(NULL, area, CTX_wm_region(C))) {
    return false;
  }
  /* animation editor test */
  if (ELEM(area->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA) == 0) {
    return false;
  }

  /* NLA tweak-mode test. */
  if (area->spacetype == SPACE_NLA) {
    if ((scene == NULL) || (scene->flag & SCE_NLA_EDIT_ON)) {
      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move (Rearrange) Channels Operator
 * \{ */

/* constants for channel rearranging */
/* WARNING: don't change existing ones without modifying rearrange func accordingly */
typedef enum eRearrangeAnimChan_Mode {
  REARRANGE_ANIMCHAN_TOP = -2,
  REARRANGE_ANIMCHAN_UP = -1,
  REARRANGE_ANIMCHAN_DOWN = 1,
  REARRANGE_ANIMCHAN_BOTTOM = 2,
} eRearrangeAnimChan_Mode;

/* defines for rearranging channels */
static const EnumPropertyItem prop_animchannel_rearrange_types[] = {
    {REARRANGE_ANIMCHAN_TOP, "TOP", 0, "To Top", ""},
    {REARRANGE_ANIMCHAN_UP, "UP", 0, "Up", ""},
    {REARRANGE_ANIMCHAN_DOWN, "DOWN", 0, "Down", ""},
    {REARRANGE_ANIMCHAN_BOTTOM, "BOTTOM", 0, "To Bottom", ""},
    {0, NULL, 0, NULL, NULL},
};

/* Reordering "Islands" Defines ----------------------------------- */

/* Island definition - just a listbase container */
typedef struct tReorderChannelIsland {
  struct tReorderChannelIsland *next, *prev;

  ListBase channels; /* channels within this region with the same state */
  int flag;          /* eReorderIslandFlag */
} tReorderChannelIsland;

/* flags for channel reordering islands */
typedef enum eReorderIslandFlag {
  REORDER_ISLAND_SELECTED = (1 << 0),    /* island is selected */
  REORDER_ISLAND_UNTOUCHABLE = (1 << 1), /* island should be ignored */
  REORDER_ISLAND_MOVED = (1 << 2),       /* island has already been moved */
  REORDER_ISLAND_HIDDEN = (1 << 3),      /* island is not visible */
} eReorderIslandFlag;

/* Rearrange Methods --------------------------------------------- */

static bool rearrange_island_ok(tReorderChannelIsland *island)
{
  /* island must not be untouchable */
  if (island->flag & REORDER_ISLAND_UNTOUCHABLE) {
    return false;
  }

  /* island should be selected to be moved */
  return (island->flag & REORDER_ISLAND_SELECTED) && !(island->flag & REORDER_ISLAND_MOVED);
}

/* ............................. */

static bool rearrange_island_top(ListBase *list, tReorderChannelIsland *island)
{
  if (rearrange_island_ok(island)) {
    /* remove from current position */
    BLI_remlink(list, island);

    /* make it first element */
    BLI_insertlinkbefore(list, list->first, island);

    return true;
  }

  return false;
}

static bool rearrange_island_up(ListBase *list, tReorderChannelIsland *island)
{
  if (rearrange_island_ok(island)) {
    /* moving up = moving before the previous island, otherwise we're in the same place */
    tReorderChannelIsland *prev = island->prev;

    /* Skip hidden islands! */
    while (prev && prev->flag & REORDER_ISLAND_HIDDEN) {
      prev = prev->prev;
    }

    if (prev) {
      /* remove from current position */
      BLI_remlink(list, island);

      /* push it up */
      BLI_insertlinkbefore(list, prev, island);

      return true;
    }
  }

  return false;
}

static bool rearrange_island_down(ListBase *list, tReorderChannelIsland *island)
{
  if (rearrange_island_ok(island)) {
    /* moving down = moving after the next island, otherwise we're in the same place */
    tReorderChannelIsland *next = island->next;

    /* Skip hidden islands! */
    while (next && next->flag & REORDER_ISLAND_HIDDEN) {
      next = next->next;
    }

    if (next) {
      /* can only move past if next is not untouchable (i.e. nothing can go after it) */
      if ((next->flag & REORDER_ISLAND_UNTOUCHABLE) == 0) {
        /* remove from current position */
        BLI_remlink(list, island);

        /* push it down */
        BLI_insertlinkafter(list, next, island);

        return true;
      }
    }
    /* else: no next channel, so we're at the bottom already, so can't move */
  }

  return false;
}

static bool rearrange_island_bottom(ListBase *list, tReorderChannelIsland *island)
{
  if (rearrange_island_ok(island)) {
    tReorderChannelIsland *last = list->last;

    /* remove island from current position */
    BLI_remlink(list, island);

    /* add before or after the last channel? */
    if ((last->flag & REORDER_ISLAND_UNTOUCHABLE) == 0) {
      /* can add after it */
      BLI_addtail(list, island);
    }
    else {
      /* can at most go just before it, since last cannot be moved */
      BLI_insertlinkbefore(list, last, island);
    }

    return true;
  }

  return false;
}

/* ............................. */

/**
 * typedef for channel rearranging function
 *
 * \param list: List of tReorderChannelIsland's that channels belong to
 * \param island: Island to be moved
 * \return Whether operation was a success
 */
typedef bool (*AnimChanRearrangeFp)(ListBase *list, tReorderChannelIsland *island);

/* get rearranging function, given 'rearrange' mode */
static AnimChanRearrangeFp rearrange_get_mode_func(eRearrangeAnimChan_Mode mode)
{
  switch (mode) {
    case REARRANGE_ANIMCHAN_TOP:
      return rearrange_island_top;
    case REARRANGE_ANIMCHAN_UP:
      return rearrange_island_up;
    case REARRANGE_ANIMCHAN_DOWN:
      return rearrange_island_down;
    case REARRANGE_ANIMCHAN_BOTTOM:
      return rearrange_island_bottom;
    default:
      return NULL;
  }
}

/* get rearranging function, given 'rearrange' mode (grease pencil is inverted) */
static AnimChanRearrangeFp rearrange_gpencil_get_mode_func(eRearrangeAnimChan_Mode mode)
{
  switch (mode) {
    case REARRANGE_ANIMCHAN_TOP:
      return rearrange_island_bottom;
    case REARRANGE_ANIMCHAN_UP:
      return rearrange_island_down;
    case REARRANGE_ANIMCHAN_DOWN:
      return rearrange_island_up;
    case REARRANGE_ANIMCHAN_BOTTOM:
      return rearrange_island_top;
    default:
      return NULL;
  }
}

/* Rearrange Islands Generics ------------------------------------- */

/* add channel into list of islands */
static void rearrange_animchannel_add_to_islands(ListBase *islands,
                                                 ListBase *srcList,
                                                 Link *channel,
                                                 eAnim_ChannelType type,
                                                 const bool is_hidden)
{
  /* always try to add to last island if possible */
  tReorderChannelIsland *island = islands->last;
  bool is_sel = false, is_untouchable = false;

  /* get flags - selected and untouchable from the channel */
  switch (type) {
    case ANIMTYPE_GROUP: {
      bActionGroup *agrp = (bActionGroup *)channel;

      is_sel = SEL_AGRP(agrp);
      is_untouchable = (agrp->flag & AGRP_TEMP) != 0;
      break;
    }
    case ANIMTYPE_FCURVE:
    case ANIMTYPE_NLACURVE: {
      FCurve *fcu = (FCurve *)channel;

      is_sel = SEL_FCU(fcu);
      break;
    }
    case ANIMTYPE_NLATRACK: {
      NlaTrack *nlt = (NlaTrack *)channel;

      is_sel = SEL_NLT(nlt);
      break;
    }
    case ANIMTYPE_GPLAYER: {
      bGPDlayer *gpl = (bGPDlayer *)channel;

      is_sel = SEL_GPL(gpl);
      break;
    }
    default:
      printf(
          "rearrange_animchannel_add_to_islands(): don't know how to handle channels of type %u\n",
          type);
      return;
  }

  /* do we need to add to a new island? */
  if (/* 1) no islands yet */
      (island == NULL) ||
      /* 2) unselected islands have single channels only - to allow up/down movement */
      ((island->flag & REORDER_ISLAND_SELECTED) == 0) ||
      /* 3) if channel is unselected, stop existing island
       * (it was either wrong sel status, or full already) */
      (is_sel == 0) ||
      /* 4) hidden status changes */
      ((island->flag & REORDER_ISLAND_HIDDEN) != is_hidden))
  {
    /* create a new island now */
    island = MEM_callocN(sizeof(tReorderChannelIsland), "tReorderChannelIsland");
    BLI_addtail(islands, island);

    if (is_sel) {
      island->flag |= REORDER_ISLAND_SELECTED;
    }
    if (is_untouchable) {
      island->flag |= REORDER_ISLAND_UNTOUCHABLE;
    }
    if (is_hidden) {
      island->flag |= REORDER_ISLAND_HIDDEN;
    }
  }

  /* add channel to island - need to remove it from its existing list first though */
  BLI_remlink(srcList, channel);
  BLI_addtail(&island->channels, channel);
}

/* flatten islands out into a single list again */
static void rearrange_animchannel_flatten_islands(ListBase *islands, ListBase *srcList)
{
  tReorderChannelIsland *island, *isn = NULL;

  /* make sure srcList is empty now */
  BLI_assert(BLI_listbase_is_empty(srcList));

  /* go through merging islands */
  for (island = islands->first; island; island = isn) {
    isn = island->next;

    /* merge island channels back to main list, then delete the island */
    BLI_movelisttolist(srcList, &island->channels);
    BLI_freelinkN(islands, island);
  }
}

/* ............................. */

/* get a list of all bAnimListElem's of a certain type which are currently visible */
static void rearrange_animchannels_filter_visible(ListBase *anim_data_visible,
                                                  bAnimContext *ac,
                                                  eAnim_ChannelType type)
{
  ListBase anim_data = {NULL, NULL};
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                              ANIMFILTER_LIST_CHANNELS);

  /* get all visible channels */
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* now, only keep the ones that are of the types we are interested in */
  LISTBASE_FOREACH_MUTABLE (bAnimListElem *, ale, &anim_data) {
    if (ale->type != type) {
      BLI_freelinkN(&anim_data, ale);
      continue;
    }

    if (type == ANIMTYPE_NLATRACK) {
      NlaTrack *nlt = (NlaTrack *)ale->data;

      if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
        /* No re-arrangement of non-local tracks of override data. */
        BLI_freelinkN(&anim_data, ale);
        continue;
      }
    }
  }

  /* return cleaned up list */
  *anim_data_visible = anim_data;
}

/* performing rearranging of channels using islands */
static bool rearrange_animchannel_islands(ListBase *list,
                                          AnimChanRearrangeFp rearrange_func,
                                          eRearrangeAnimChan_Mode mode,
                                          eAnim_ChannelType type,
                                          ListBase *anim_data_visible)
{
  ListBase islands = {NULL, NULL};
  Link *channel, *chanNext = NULL;
  bool done = false;

  /* don't waste effort on an empty list */
  if (BLI_listbase_is_empty(list)) {
    return false;
  }

  /* group channels into islands */
  for (channel = list->first; channel; channel = chanNext) {
    /* find out whether this channel is present in anim_data_visible or not! */
    const bool is_hidden =
        (BLI_findptr(anim_data_visible, channel, offsetof(bAnimListElem, data)) == NULL);
    chanNext = channel->next;
    rearrange_animchannel_add_to_islands(&islands, list, channel, type, is_hidden);
  }

  /* Perform moving of selected islands now, but only if there is more than one of them
   * so that something will happen:
   *
   * - Scanning of the list is performed in the opposite direction
   *   to the direction we're moving things,
   *   so that we shouldn't need to encounter items we've moved already.
   */
  if (islands.first != islands.last) {
    tReorderChannelIsland *first = (mode > 0) ? islands.last : islands.first;
    tReorderChannelIsland *island, *isn = NULL;

    for (island = first; island; island = isn) {
      isn = (mode > 0) ? island->prev : island->next;

      /* perform rearranging */
      if (rearrange_func(&islands, island)) {
        island->flag |= REORDER_ISLAND_MOVED;
        done = true;
      }
    }
  }

  /* ungroup islands */
  rearrange_animchannel_flatten_islands(&islands, list);

  /* did we do anything? */
  return done;
}

/* NLA Specific Stuff ----------------------------------------------------- */

/* Change the order NLA Tracks within NLA Stack
 * ! NLA tracks are displayed in opposite order, so directions need care
 * mode: REARRANGE_ANIMCHAN_*
 */
static void rearrange_nla_channels(bAnimContext *ac, AnimData *adt, eRearrangeAnimChan_Mode mode)
{
  AnimChanRearrangeFp rearrange_func;
  ListBase anim_data_visible = {NULL, NULL};
  const bool is_liboverride = (ac->obact != NULL) ? ID_IS_OVERRIDE_LIBRARY(ac->obact) : false;

  /* hack: invert mode so that functions will work in right order */
  mode *= -1;

  /* get rearranging function */
  rearrange_func = rearrange_get_mode_func(mode);
  if (rearrange_func == NULL) {
    return;
  }

  /* In liboverride case, we need to extract non-local NLA tracks from current anim data before we
   * can perform the move, and add then back afterwards. It's the only way to prevent them from
   * being affected by the reordering.
   *
   * Note that both override apply code for NLA tracks collection, and NLA editing code, are
   * responsible to ensure that non-local tracks always remain first in the list. */
  ListBase extracted_nonlocal_nla_tracks = {NULL, NULL};
  if (is_liboverride) {
    NlaTrack *nla_track;
    for (nla_track = adt->nla_tracks.first; nla_track != NULL; nla_track = nla_track->next) {
      if (!BKE_nlatrack_is_nonlocal_in_liboverride(&ac->obact->id, nla_track)) {
        break;
      }
    }
    if (nla_track != NULL && nla_track->prev != NULL) {
      extracted_nonlocal_nla_tracks.first = adt->nla_tracks.first;
      extracted_nonlocal_nla_tracks.last = nla_track->prev;
      adt->nla_tracks.first = nla_track;
      nla_track->prev->next = NULL;
      nla_track->prev = NULL;
    }
  }

  /* Filter visible data. */
  rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_NLATRACK);

  /* perform rearranging on tracks list */
  rearrange_animchannel_islands(
      &adt->nla_tracks, rearrange_func, mode, ANIMTYPE_NLATRACK, &anim_data_visible);

  /* Add back non-local NLA tracks at the beginning of the animation data's list. */
  if (!BLI_listbase_is_empty(&extracted_nonlocal_nla_tracks)) {
    BLI_assert(is_liboverride);
    ((NlaTrack *)extracted_nonlocal_nla_tracks.last)->next = adt->nla_tracks.first;
    ((NlaTrack *)adt->nla_tracks.first)->prev = extracted_nonlocal_nla_tracks.last;
    adt->nla_tracks.first = extracted_nonlocal_nla_tracks.first;
  }

  /* free temp data */
  BLI_freelistN(&anim_data_visible);
}

/* Drivers Specific Stuff ------------------------------------------------- */

/* Change the order drivers within AnimData block
 * mode: REARRANGE_ANIMCHAN_*
 */
static void rearrange_driver_channels(bAnimContext *ac,
                                      AnimData *adt,
                                      eRearrangeAnimChan_Mode mode)
{
  /* get rearranging function */
  AnimChanRearrangeFp rearrange_func = rearrange_get_mode_func(mode);
  ListBase anim_data_visible = {NULL, NULL};

  if (rearrange_func == NULL) {
    return;
  }

  /* only consider drivers if they're accessible */
  if (EXPANDED_DRVD(adt) == 0) {
    return;
  }

  /* Filter visible data. */
  rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_FCURVE);

  /* perform rearranging on drivers list (drivers are really just F-Curves) */
  rearrange_animchannel_islands(
      &adt->drivers, rearrange_func, mode, ANIMTYPE_FCURVE, &anim_data_visible);

  /* free temp data */
  BLI_freelistN(&anim_data_visible);
}

/* Action Specific Stuff ------------------------------------------------- */

/* make sure all action-channels belong to a group (and clear action's list) */
static void split_groups_action_temp(bAction *act, bActionGroup *tgrp)
{
  FCurve *fcu;

  if (act == NULL) {
    return;
  }

  /* Separate F-Curves into lists per group */
  LISTBASE_FOREACH (bActionGroup *, agrp, &act->groups) {
    FCurve *const group_fcurves_first = agrp->channels.first;
    FCurve *const group_fcurves_last = agrp->channels.last;
    if (group_fcurves_first == NULL) {
      /* Empty group. */
      continue;
    }

    if (group_fcurves_first == act->curves.first) {
      /* First of the action curves, update the start of the action curves. */
      BLI_assert(group_fcurves_first->prev == NULL);
      act->curves.first = group_fcurves_last->next;
    }
    else {
      group_fcurves_first->prev->next = group_fcurves_last->next;
    }

    if (group_fcurves_last == act->curves.last) {
      /* Last of the action curves, update the end of the action curves. */
      BLI_assert(group_fcurves_last->next == NULL);
      act->curves.last = group_fcurves_first->prev;
    }
    else {
      group_fcurves_last->next->prev = group_fcurves_first->prev;
    }

    /* Clear links pointing outside the per-group list. */
    group_fcurves_first->prev = group_fcurves_last->next = NULL;
  }

  /* Initialize memory for temp-group */
  memset(tgrp, 0, sizeof(bActionGroup));
  tgrp->flag |= (AGRP_EXPANDED | AGRP_TEMP);
  STRNCPY(tgrp->name, "#TempGroup");

  /* Move any action-channels not already moved, to the temp group */
  if (act->curves.first) {
    /* start of list */
    fcu = act->curves.first;
    fcu->prev = NULL;
    tgrp->channels.first = fcu;
    act->curves.first = NULL;

    /* end of list */
    fcu = act->curves.last;
    fcu->next = NULL;
    tgrp->channels.last = fcu;
    act->curves.last = NULL;

    /* ensure that all of these get their group set to this temp group
     * (so that visibility filtering works)
     */
    for (fcu = tgrp->channels.first; fcu; fcu = fcu->next) {
      fcu->grp = tgrp;
    }
  }

  /* Add temp-group to list */
  BLI_addtail(&act->groups, tgrp);
}

/* link lists of channels that groups have */
static void join_groups_action_temp(bAction *act)
{
  bActionGroup *agrp;

  for (agrp = act->groups.first; agrp; agrp = agrp->next) {
    /* add list of channels to action's channels */
    const ListBase group_channels = agrp->channels;
    BLI_movelisttolist(&act->curves, &agrp->channels);
    agrp->channels = group_channels;

    /* clear moved flag */
    agrp->flag &= ~AGRP_MOVED;

    /* if group was temporary one:
     * - unassign all FCurves which were temporarily added to it
     * - remove from list (but don't free as it's on the stack!)
     */
    if (agrp->flag & AGRP_TEMP) {
      LISTBASE_FOREACH (FCurve *, fcu, &agrp->channels) {
        fcu->grp = NULL;
      }

      BLI_remlink(&act->groups, agrp);
      break;
    }
  }
}

/* Change the order of anim-channels within action
 * mode: REARRANGE_ANIMCHAN_*
 */
static void rearrange_action_channels(bAnimContext *ac, bAction *act, eRearrangeAnimChan_Mode mode)
{
  bActionGroup tgrp;
  ListBase anim_data_visible = {NULL, NULL};
  bool do_channels;

  /* get rearranging function */
  AnimChanRearrangeFp rearrange_func = rearrange_get_mode_func(mode);

  if (rearrange_func == NULL) {
    return;
  }

  /* make sure we're only operating with groups (vs a mixture of groups+curves) */
  split_groups_action_temp(act, &tgrp);

  /* Filter visible data. */
  rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_GROUP);

  /* Rearrange groups first:
   * - The group's channels will only get considered
   *   if nothing happened when rearranging the groups
   *   i.e. the rearrange function returned 0.
   */
  do_channels = (rearrange_animchannel_islands(
                     &act->groups, rearrange_func, mode, ANIMTYPE_GROUP, &anim_data_visible) == 0);

  /* free temp data */
  BLI_freelistN(&anim_data_visible);

  if (do_channels) {
    bActionGroup *agrp;

    /* Filter visible data. */
    rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_FCURVE);

    for (agrp = act->groups.first; agrp; agrp = agrp->next) {
      /* only consider F-Curves if they're visible (group expanded) */
      if (EXPANDED_AGRP(ac, agrp)) {
        rearrange_animchannel_islands(
            &agrp->channels, rearrange_func, mode, ANIMTYPE_FCURVE, &anim_data_visible);
      }
    }

    /* free temp data */
    BLI_freelistN(&anim_data_visible);
  }

  /* assemble lists into one list (and clear moved tags) */
  join_groups_action_temp(act);
}

/* ------------------- */

static void rearrange_nla_control_channels(bAnimContext *ac,
                                           AnimData *adt,
                                           eRearrangeAnimChan_Mode mode)
{
  ListBase anim_data_visible = {NULL, NULL};

  NlaTrack *nlt;
  NlaStrip *strip;

  /* get rearranging function */
  AnimChanRearrangeFp rearrange_func = rearrange_get_mode_func(mode);

  if (rearrange_func == NULL) {
    return;
  }

  /* skip if these curves aren't being shown */
  if (adt->flag & ADT_NLA_SKEYS_COLLAPSED) {
    return;
  }

  /* Filter visible data. */
  rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_NLACURVE);

  /* we cannot rearrange between strips, but within each strip, we can rearrange those curves */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      rearrange_animchannel_islands(
          &strip->fcurves, rearrange_func, mode, ANIMTYPE_NLACURVE, &anim_data_visible);
    }
  }

  /* free temp data */
  BLI_freelistN(&anim_data_visible);
}

/* ------------------- */

static void rearrange_gpencil_channels(bAnimContext *ac, eRearrangeAnimChan_Mode mode)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get rearranging function */
  AnimChanRearrangeFp rearrange_func = rearrange_gpencil_get_mode_func(mode);

  if (rearrange_func == NULL) {
    return;
  }

  /* get Grease Pencil datablocks */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA |
            ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    /* only consider grease pencil container channels */
    if (!ELEM(ale->type, ANIMTYPE_GPDATABLOCK, ANIMTYPE_DSGPENCIL)) {
      continue;
    }

    ListBase anim_data_visible = {NULL, NULL};
    bGPdata *gpd = ale->data;

    /* only consider layers if this datablock is open */
    if ((gpd->flag & GP_DATA_EXPAND) == 0) {
      continue;
    }

    /* Filter visible data. */
    rearrange_animchannels_filter_visible(&anim_data_visible, ac, ANIMTYPE_GPLAYER);

    /* Rearrange data-block's layers. */
    rearrange_animchannel_islands(
        &gpd->layers, rearrange_func, mode, ANIMTYPE_GPLAYER, &anim_data_visible);

    /* free visible layers data */
    BLI_freelistN(&anim_data_visible);

    /* Tag to recalc geometry */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* free GPD channel data */
  ANIM_animdata_freelist(&anim_data);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

/* ------------------- */

static int animchannels_rearrange_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  eRearrangeAnimChan_Mode mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get mode */
  mode = RNA_enum_get(op->ptr, "direction");

  /* method to move channels depends on the editor */
  if (ac.datatype == ANIMCONT_GPENCIL) {
    /* Grease Pencil channels */
    rearrange_gpencil_channels(&ac, mode);
  }
  else if (ac.datatype == ANIMCONT_MASK) {
    /* Grease Pencil channels */
    printf("Mask does not supported for moving yet\n");
  }
  else if (ac.datatype == ANIMCONT_ACTION) {
    /* Directly rearrange action's channels */
    rearrange_action_channels(&ac, ac.data, mode);
  }
  else {
    ListBase anim_data = {NULL, NULL};
    bAnimListElem *ale;
    int filter;

    if (ELEM(ac.datatype, ANIMCONT_DOPESHEET, ANIMCONT_TIMELINE)) {
      rearrange_gpencil_channels(&ac, mode);
    }

    /* get animdata blocks */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA |
              ANIMFILTER_FCURVESONLY);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    for (ale = anim_data.first; ale; ale = ale->next) {
      AnimData *adt = ale->data;

      switch (ac.datatype) {
        case ANIMCONT_NLA: /* NLA-tracks only */
          rearrange_nla_channels(&ac, adt, mode);
          DEG_id_tag_update(ale->id, ID_RECALC_ANIMATION);
          break;

        case ANIMCONT_DRIVERS: /* Drivers list only */
          rearrange_driver_channels(&ac, adt, mode);
          break;

        case ANIMCONT_ACTION:   /* Single Action only... */
        case ANIMCONT_SHAPEKEY: /* DOUBLE CHECK ME... */
        {
          if (adt->action) {
            rearrange_action_channels(&ac, adt->action, mode);
          }
          else if (G.debug & G_DEBUG) {
            printf("Animdata has no action\n");
          }
          break;
        }

        default: /* DopeSheet/Graph Editor - Some Actions + NLA Control Curves */
        {
          /* NLA Control Curves */
          if (adt->nla_tracks.first) {
            rearrange_nla_control_channels(&ac, adt, mode);
          }

          /* Action */
          if (adt->action) {
            rearrange_action_channels(&ac, adt->action, mode);
          }
          else if (G.debug & G_DEBUG) {
            printf("Animdata has no action\n");
          }
          break;
        }
      }
    }

    /* free temp data */
    ANIM_animdata_freelist(&anim_data);
  }

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ORDER, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Channels";
  ot->idname = "ANIM_OT_channels_move";
  ot->description = "Rearrange selected animation channels";

  /* api callbacks */
  ot->exec = animchannels_rearrange_exec;
  ot->poll = animedit_poll_channels_nla_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(ot->srna,
                          "direction",
                          prop_animchannel_rearrange_types,
                          REARRANGE_ANIMCHAN_DOWN,
                          "Direction",
                          "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group Channel Operator
 * \{ */

static bool animchannels_grouping_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceLink *sl;

  /* channels region test */
  /* TODO: could enhance with actually testing if channels region? */
  if (ELEM(NULL, area, CTX_wm_region(C))) {
    return false;
  }

  /* animation editor test - must be suitable modes only */
  sl = CTX_wm_space_data(C);

  switch (area->spacetype) {
    /* supported... */
    case SPACE_ACTION: {
      SpaceAction *saction = (SpaceAction *)sl;

      /* dopesheet and action only - all others are for other datatypes or have no groups */
      if (ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_DOPESHEET) == 0) {
        return false;
      }

      break;
    }
    case SPACE_GRAPH: {
      SpaceGraph *sipo = (SpaceGraph *)sl;

      /* drivers can't have groups... */
      if (sipo->mode != SIPO_MODE_ANIMATION) {
        return false;
      }

      break;
    }
    /* unsupported... */
    default:
      return false;
  }

  return true;
}

/* ----------------------------------------------------------- */

static void animchannels_group_channels(bAnimContext *ac,
                                        bAnimListElem *adt_ref,
                                        const char name[])
{
  AnimData *adt = adt_ref->adt;
  bAction *act = adt->action;

  if (act) {
    ListBase anim_data = {NULL, NULL};
    int filter;

    /* find selected F-Curves to re-group */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
              ANIMFILTER_FCURVESONLY);
    ANIM_animdata_filter(ac, &anim_data, filter, adt_ref, ANIMCONT_CHANNEL);

    if (anim_data.first) {
      bActionGroup *agrp;
      bAnimListElem *ale;

      /* create new group, which should now be part of the action */
      agrp = action_groups_add_new(act, name);
      BLI_assert(agrp != NULL);

      /* Transfer selected F-Curves across to new group. */
      for (ale = anim_data.first; ale; ale = ale->next) {
        FCurve *fcu = (FCurve *)ale->data;
        bActionGroup *grp = fcu->grp;

        /* remove F-Curve from group, then group too if it is now empty */
        action_groups_remove_channel(act, fcu);

        if ((grp) && BLI_listbase_is_empty(&grp->channels)) {
          BLI_freelinkN(&act->groups, grp);
        }

        /* add F-Curve to group */
        action_groups_add_channel(act, agrp, fcu);
      }
    }

    /* cleanup */
    ANIM_animdata_freelist(&anim_data);
  }
}

static int animchannels_group_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  char name[MAX_NAME];

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get name for new group */
  RNA_string_get(op->ptr, "name", name);

  /* XXX: name for group should never be empty... */
  if (name[0]) {
    ListBase anim_data = {NULL, NULL};
    bAnimListElem *ale;
    int filter;

    /* Handle each animdata block separately, so that the regrouping doesn't flow into blocks. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA |
              ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    for (ale = anim_data.first; ale; ale = ale->next) {
      animchannels_group_channels(&ac, ale, name);
    }

    /* free temp data */
    ANIM_animdata_freelist(&anim_data);

    /* Updates. */
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  }

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_group(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Group Channels";
  ot->idname = "ANIM_OT_channels_group";
  ot->description = "Add selected F-Curves to a new group";

  /* callbacks */
  ot->invoke = WM_operator_props_popup;
  ot->exec = animchannels_group_exec;
  ot->poll = animchannels_grouping_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_string(ot->srna,
                            "name",
                            "New Group",
                            sizeof(((bActionGroup *)NULL)->name),
                            "Name",
                            "Name of newly created group");
  /* XXX: still not too sure about this - keeping same text is confusing... */
  // RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ungroup Channels Operator
 * \{ */

static int animchannels_ungroup_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* just selected F-Curves... */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
            ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    /* find action for this F-Curve... */
    if (ale->adt && ale->adt->action) {
      FCurve *fcu = (FCurve *)ale->data;
      bAction *act = ale->adt->action;

      /* only proceed to remove if F-Curve is in a group... */
      if (fcu->grp) {
        bActionGroup *agrp = fcu->grp;

        /* remove F-Curve from group and add at tail (ungrouped) */
        action_groups_remove_channel(act, fcu);
        BLI_addtail(&act->curves, fcu);

        /* delete group if it is now empty */
        if (BLI_listbase_is_empty(&agrp->channels)) {
          BLI_freelinkN(&act->groups, agrp);
        }
      }
    }
  }

  /* cleanup */
  ANIM_animdata_freelist(&anim_data);

  /* updates */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_ungroup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Ungroup Channels";
  ot->idname = "ANIM_OT_channels_ungroup";
  ot->description = "Remove selected F-Curves from their current groups";

  /* callbacks */
  ot->exec = animchannels_ungroup_exec;
  ot->poll = animchannels_grouping_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Channel Operator
 * \{ */

static void tag_update_animation_element(bAnimListElem *ale)
{
  ID *id = ale->id;
  AnimData *adt = BKE_animdata_from_id(id);
  /* TODO(sergey): Technically, if the animation element is being deleted
   * from a driver we don't have to tag action. This is something we can check
   * for in the future. For now just do most reliable tag which was always happening. */
  if (adt != NULL) {
    DEG_id_tag_update(id, ID_RECALC_ANIMATION);
    if (adt->action != NULL) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION);
    }
  }
  /* Deals with NLA and drivers.
   * Doesn't cause overhead for action updates, since object will receive
   * animation update after dependency graph flushes update from action to
   * all its users. */
  DEG_id_tag_update(id, ID_RECALC_ANIMATION);
}

static int animchannels_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* cannot delete in shapekey */
  if (ac.datatype == ANIMCONT_SHAPEKEY) {
    return OPERATOR_CANCELLED;
  }

  /* do groups only first (unless in Drivers mode, where there are none) */
  if (ac.datatype != ANIMCONT_DRIVERS) {
    /* filter data */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
              ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    /* delete selected groups and their associated channels */
    for (ale = anim_data.first; ale; ale = ale->next) {
      /* only groups - don't check other types yet, since they may no-longer exist */
      if (ale->type == ANIMTYPE_GROUP) {
        bActionGroup *agrp = (bActionGroup *)ale->data;
        AnimData *adt = ale->adt;
        FCurve *fcu, *fcn;

        /* skip this group if no AnimData available, as we can't safely remove the F-Curves */
        if (adt == NULL) {
          continue;
        }

        /* delete all of the Group's F-Curves, but no others */
        for (fcu = agrp->channels.first; fcu && fcu->grp == agrp; fcu = fcn) {
          fcn = fcu->next;

          /* remove from group and action, then free */
          action_groups_remove_channel(adt->action, fcu);
          BKE_fcurve_free(fcu);
        }

        /* free the group itself */
        if (adt->action) {
          BLI_freelinkN(&adt->action->groups, agrp);
          DEG_id_tag_update_ex(CTX_data_main(C), &adt->action->id, ID_RECALC_ANIMATION);
        }
        else {
          MEM_freeN(agrp);
        }
      }
    }

    /* cleanup */
    ANIM_animdata_freelist(&anim_data);
  }

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* delete selected data channels */
  for (ale = anim_data.first; ale; ale = ale->next) {
    switch (ale->type) {
      case ANIMTYPE_FCURVE: {
        /* F-Curves if we can identify its parent */
        AnimData *adt = ale->adt;
        FCurve *fcu = (FCurve *)ale->data;

        /* try to free F-Curve */
        ANIM_fcurve_delete_from_animdata(&ac, adt, fcu);
        tag_update_animation_element(ale);
        break;
      }
      case ANIMTYPE_NLACURVE: {
        /* NLA Control Curve - Deleting it should disable the corresponding setting... */
        NlaStrip *strip = (NlaStrip *)ale->owner;
        FCurve *fcu = (FCurve *)ale->data;

        if (STREQ(fcu->rna_path, "strip_time")) {
          strip->flag &= ~NLASTRIP_FLAG_USR_TIME;
        }
        else if (STREQ(fcu->rna_path, "influence")) {
          strip->flag &= ~NLASTRIP_FLAG_USR_INFLUENCE;
        }
        else {
          printf("ERROR: Trying to delete NLA Control Curve for unknown property '%s'\n",
                 fcu->rna_path);
        }

        /* unlink and free the F-Curve */
        BLI_remlink(&strip->fcurves, fcu);
        BKE_fcurve_free(fcu);
        tag_update_animation_element(ale);
        break;
      }
      case ANIMTYPE_GPLAYER: {
        /* Grease Pencil layer */
        bGPdata *gpd = (bGPdata *)ale->id;
        bGPDlayer *gpl = (bGPDlayer *)ale->data;

        /* try to delete the layer's data and the layer itself */
        BKE_gpencil_layer_delete(gpd, gpl);
        ale->update = ANIM_UPDATE_DEPS;
        break;
      }
      case ANIMTYPE_MASKLAYER: {
        /* Mask layer */
        Mask *mask = (Mask *)ale->id;
        MaskLayer *masklay = (MaskLayer *)ale->data;

        /* try to delete the layer's data and the layer itself */
        BKE_mask_layer_remove(mask, masklay);
        break;
      }
    }
  }

  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Channels";
  ot->idname = "ANIM_OT_channels_delete";
  ot->description = "Delete all selected animation channels";

  /* api callbacks */
  ot->exec = animchannels_delete_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set/Toggle Channel Flags Operator Utilities
 * \{ */

/* defines for setting animation-channel flags */
static const EnumPropertyItem prop_animchannel_setflag_types[] = {
    {ACHANNEL_SETFLAG_TOGGLE, "TOGGLE", 0, "Toggle", ""},
    {ACHANNEL_SETFLAG_CLEAR, "DISABLE", 0, "Disable", ""},
    {ACHANNEL_SETFLAG_ADD, "ENABLE", 0, "Enable", ""},
    {ACHANNEL_SETFLAG_INVERT, "INVERT", 0, "Invert", ""},
    {0, NULL, 0, NULL, NULL},
};

/* defines for set animation-channel settings */
/* TODO: could add some more types, but those are really quite dependent on the mode... */
static const EnumPropertyItem prop_animchannel_settings_types[] = {
    {ACHANNEL_SETTING_PROTECT, "PROTECT", 0, "Protect", ""},
    {ACHANNEL_SETTING_MUTE, "MUTE", 0, "Mute", ""},
    {0, NULL, 0, NULL, NULL},
};

/* ------------------- */

/**
 * Set/clear a particular flag (setting) for all selected + visible channels
 * \param setting: the setting to modify.
 * \param mode: eAnimChannels_SetFlag.
 * \param onlysel: only selected channels get the flag set.
 *
 * TODO: enable a setting which turns flushing on/off?
 */
static void setflag_anim_channels(bAnimContext *ac,
                                  eAnimChannel_Settings setting,
                                  eAnimChannels_SetFlag mode,
                                  bool onlysel,
                                  bool flush)
{
  ListBase anim_data = {NULL, NULL};
  ListBase all_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* filter data that we need if flush is on */
  if (flush) {
    /* get list of all channels that selection may need to be flushed to
     * - hierarchy visibility needs to be ignored so that settings can get flushed
     *   "down" inside closed containers
     */
    filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
    ANIM_animdata_filter(ac, &all_data, filter, ac->data, ac->datatype);
  }

  /* filter data that we're working on
   * - hierarchy matters if we're doing this from the channels region
   *   since we only want to apply this to channels we can "see",
   *   and have these affect their relatives
   * - but for Graph Editor, this gets used also from main region
   *   where hierarchy doesn't apply #21276.
   */
  if ((ac->spacetype == SPACE_GRAPH) && (ac->regiontype != RGN_TYPE_CHANNELS)) {
    /* graph editor (case 2) */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_CURVE_VISIBLE |
              ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  }
  else {
    /* standard case */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS |
              ANIMFILTER_NODUPLIS);
  }
  if (onlysel) {
    filter |= ANIMFILTER_SEL;
  }
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* if toggling, check if disable or enable */
  if (mode == ACHANNEL_SETFLAG_TOGGLE) {
    /* default to turn all on, unless we encounter one that's on... */
    mode = ACHANNEL_SETFLAG_ADD;

    /* see if we should turn off instead... */
    for (ale = anim_data.first; ale; ale = ale->next) {
      /* set the setting in the appropriate way (if available) */
      if (ANIM_channel_setting_get(ac, ale, setting) > 0) {
        mode = ACHANNEL_SETFLAG_CLEAR;
        break;
      }
    }
  }

  /* apply the setting */
  for (ale = anim_data.first; ale; ale = ale->next) {
    /* skip channel if setting is not available */
    if (ANIM_channel_setting_get(ac, ale, setting) == -1) {
      continue;
    }

    /* set the setting in the appropriate way */
    ANIM_channel_setting_set(ac, ale, setting, mode);
    tag_update_animation_element(ale);

    /* if flush status... */
    if (flush) {
      ANIM_flush_setting_anim_channels(ac, &all_data, ale, setting, mode);
    }
  }

  ANIM_animdata_freelist(&anim_data);
  BLI_freelistN(&all_data);
}

/* ------------------- */

static int animchannels_setflag_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  eAnimChannel_Settings setting;
  eAnimChannels_SetFlag mode;
  bool flush = true;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* mode (eAnimChannels_SetFlag), setting (eAnimChannel_Settings) */
  mode = RNA_enum_get(op->ptr, "mode");
  setting = RNA_enum_get(op->ptr, "type");

  /* check if setting is flushable */
  if (setting == ACHANNEL_SETTING_EXPAND) {
    flush = false;
  }

  /* modify setting
   * - only selected channels are affected
   */
  setflag_anim_channels(&ac, setting, mode, true, flush);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

/* duplicate of 'ANIM_OT_channels_setting_toggle' for menu title only, weak! */
static void ANIM_OT_channels_setting_enable(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Enable Channel Setting";
  ot->idname = "ANIM_OT_channels_setting_enable";
  ot->description = "Enable specified setting on all selected animation channels";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = animchannels_setflag_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* flag-setting mode */
  prop = RNA_def_enum(
      ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_ADD, "Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  /* setting to set */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}
/* duplicate of 'ANIM_OT_channels_setting_toggle' for menu title only, weak! */
static void ANIM_OT_channels_setting_disable(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Disable Channel Setting";
  ot->idname = "ANIM_OT_channels_setting_disable";
  ot->description = "Disable specified setting on all selected animation channels";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = animchannels_setflag_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* flag-setting mode */
  prop = RNA_def_enum(
      ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_CLEAR, "Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN); /* internal hack - don't expose */
  /* setting to set */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

static void ANIM_OT_channels_setting_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Toggle Channel Setting";
  ot->idname = "ANIM_OT_channels_setting_toggle";
  ot->description = "Toggle specified setting on all selected animation channels";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = animchannels_setflag_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* flag-setting mode */
  prop = RNA_def_enum(
      ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_TOGGLE, "Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN); /* internal hack - don't expose */
  /* setting to set */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_animchannel_settings_types, 0, "Type", "");
}

static void ANIM_OT_channels_editable_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Toggle Channel Editability";
  ot->idname = "ANIM_OT_channels_editable_toggle";
  ot->description = "Toggle editability of selected channels";

  /* api callbacks */
  ot->exec = animchannels_setflag_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* flag-setting mode */
  RNA_def_enum(
      ot->srna, "mode", prop_animchannel_setflag_types, ACHANNEL_SETFLAG_TOGGLE, "Mode", "");
  /* setting to set */
  prop = RNA_def_enum(
      ot->srna, "type", prop_animchannel_settings_types, ACHANNEL_SETTING_PROTECT, "Type", "");
  RNA_def_property_flag(prop, PROP_HIDDEN); /* internal hack - don't expose */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Expand Channels Operator
 * \{ */

static int animchannels_expand_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bool onlysel = true;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* only affect selected channels? */
  if (RNA_boolean_get(op->ptr, "all")) {
    onlysel = false;
  }

  /* modify setting */
  setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_ADD, onlysel, false);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_expand(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Expand Channels";
  ot->idname = "ANIM_OT_channels_expand";
  ot->description = "Expand (open) all selected expandable animation channels";

  /* api callbacks */
  ot->exec = animchannels_expand_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(
      ot->srna, "all", 1, "All", "Expand all channels (not just selected ones)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collapse Channels Operator
 * \{ */

static int animchannels_collapse_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bool onlysel = true;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* only affect selected channels? */
  if (RNA_boolean_get(op->ptr, "all")) {
    onlysel = false;
  }

  /* modify setting */
  setflag_anim_channels(&ac, ACHANNEL_SETTING_EXPAND, ACHANNEL_SETFLAG_CLEAR, onlysel, false);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_collapse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Collapse Channels";
  ot->idname = "ANIM_OT_channels_collapse";
  ot->description = "Collapse (close) all selected expandable animation channels";

  /* api callbacks */
  ot->exec = animchannels_collapse_exec;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(
      ot->srna, "all", true, "All", "Collapse all channels (not just selected ones)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove All "Empty" AnimData Blocks Operator
 *
 * We define "empty" AnimData blocks here as those which have all 3 of criteria:
 *
 * 1) No active action OR that active actions are empty
 *    Assuming that all legitimate entries will have an action,
 *    and that empty actions
 * 2) No NLA Tracks + NLA Strips
 *    Assuming that users haven't set up any of these as "placeholders"
 *    for convenience sake, and that most that exist were either unintentional
 *    or are no longer wanted
 * 3) No drivers
 * \{ */

static int animchannels_clean_empty_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get animdata blocks */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ANIMDATA |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    ID *id = ale->id;
    AnimData *adt = ale->data;

    bool action_empty = false;
    bool nla_empty = false;
    bool drivers_empty = false;

    /* sanity checks */
    BLI_assert((id != NULL) && (adt != NULL));

    /* check if this is "empty" and can be deleted */
    /* (For now, there are only these 3 criteria) */

    /* 1) Active Action is missing or empty */
    if (ELEM(NULL, adt->action, adt->action->curves.first)) {
      action_empty = true;
    }
    else {
      /* TODO: check for keyframe + fmodifier data on these too */
    }

    /* 2) No NLA Tracks and/or NLA Strips */
    if (adt->nla_tracks.first == NULL) {
      nla_empty = true;
    }
    else {
      NlaTrack *nlt;

      /* empty tracks? */
      for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
        if (nlt->strips.first) {
          /* stop searching, as we found one that actually had stuff we don't want lost
           * NOTE: nla_empty gets reset to false, as a previous track may have been empty
           */
          nla_empty = false;
          break;
        }
        if (nlt->strips.first == NULL) {
          /* this track is empty, but another one may still have stuff in it, so can't break yet */
          nla_empty = true;
        }
      }
    }

    /* 3) Drivers */
    drivers_empty = (adt->drivers.first == NULL);

    /* remove AnimData? */
    if (action_empty && nla_empty && drivers_empty) {
      BKE_animdata_free(id, true);
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_clean_empty(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Empty Animation Data";
  ot->idname = "ANIM_OT_channels_clean_empty";
  ot->description = "Delete all empty animation data containers from visible data-blocks";

  /* api callbacks */
  ot->exec = animchannels_clean_empty_exec;
  ot->poll = animedit_poll_channels_nla_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Re-enable Disabled Operator
 * \{ */

static bool animchannels_enable_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* channels region test */
  /* TODO: could enhance with actually testing if channels region? */
  if (ELEM(NULL, area, CTX_wm_region(C))) {
    return false;
  }

  /* animation editor test - Action/Dopesheet/etc. and Graph only */
  if (ELEM(area->spacetype, SPACE_ACTION, SPACE_GRAPH) == 0) {
    return false;
  }

  return true;
}

static int animchannels_enable_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop through filtered data and clean curves */
  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->data;

    /* remove disabled flags from F-Curves */
    fcu->flag &= ~FCURVE_DISABLED;

    /* for drivers, let's do the same too */
    if (fcu->driver) {
      fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
    }

    /* tag everything for updates - in particular, this is needed to get drivers working again */
    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_fcurves_enable(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Revive Disabled F-Curves";
  ot->idname = "ANIM_OT_channels_fcurves_enable";
  ot->description = "Clear 'disabled' tag from all F-Curves to get broken F-Curves working again";

  /* api callbacks */
  ot->exec = animchannels_enable_exec;
  ot->poll = animchannels_enable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Filter Text-box Operator
 * \{ */

/* XXX: make this generic? */
static bool animchannels_select_filter_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  if (area == NULL) {
    return false;
  }

  /* animation editor with dopesheet */
  return ELEM(area->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA);
}

static int animchannels_select_filter_invoke(struct bContext *C,
                                             struct wmOperator *op,
                                             const struct wmEvent *UNUSED(event))
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region_ctx = CTX_wm_region(C);
  ARegion *region_channels = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);

  CTX_wm_region_set(C, region_channels);

  /* Show the channel region if it's hidden. This means that direct activation of the input field
   * is impossible, as it may not exist yet. For that reason, the actual activation is deferred to
   * the modal callback function; by the time it runs, the screen has been redrawn and the UI
   * element is there to activate. */
  if (region_channels->flag & RGN_FLAG_HIDDEN) {
    ED_region_toggle_hidden(C, region_channels);
    ED_region_tag_redraw(region_channels);
  }

  WM_event_add_modal_handler(C, op);

  CTX_wm_region_set(C, region_ctx);
  return OPERATOR_RUNNING_MODAL;
}

static int animchannels_select_filter_modal(bContext *C,
                                            wmOperator *UNUSED(op),
                                            const wmEvent *UNUSED(event))
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  ARegion *region = CTX_wm_region(C);
  if (UI_textbutton_activate_rna(C, region, ac.ads, "filter_text")) {
    /* Redraw to make sure it shows the cursor after activating */
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  }

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_select_filter(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter Channels";
  ot->idname = "ANIM_OT_channels_select_filter";
  ot->description =
      "Start entering text which filters the set of channels shown to only include those with "
      "matching names";

  /* callbacks */
  ot->invoke = animchannels_select_filter_invoke;
  ot->modal = animchannels_select_filter_modal;
  ot->poll = animchannels_select_filter_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */

static int animchannels_selectall_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* 'standard' behavior - check if selected, then apply relevant selection */
  const int action = RNA_enum_get(op->ptr, "action");
  switch (action) {
    case SEL_TOGGLE:
      ANIM_anim_channels_select_toggle(&ac);
      break;
    case SEL_SELECT:
      ANIM_anim_channels_select_set(&ac, ACHANNEL_SETFLAG_ADD);
      break;
    case SEL_DESELECT:
      ANIM_anim_channels_select_set(&ac, ACHANNEL_SETFLAG_CLEAR);
      break;
    case SEL_INVERT:
      ANIM_anim_channels_select_set(&ac, ACHANNEL_SETFLAG_INVERT);
      break;
    default:
      BLI_assert(0);
      break;
  }

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->idname = "ANIM_OT_channels_select_all";
  ot->description = "Toggle selection of all animation channels";

  /* api callbacks */
  ot->exec = animchannels_selectall_exec;
  ot->poll = animedit_poll_channels_nla_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static void box_select_anim_channels(bAnimContext *ac, rcti *rect, short selectmode)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  SpaceNla *snla = (SpaceNla *)ac->sl;
  View2D *v2d = &ac->region->v2d;
  rctf rectf;

  /* convert border-region to view coordinates */
  UI_view2d_region_to_view(v2d, rect->xmin, rect->ymin + 2, &rectf.xmin, &rectf.ymin);
  UI_view2d_region_to_view(v2d, rect->xmax, rect->ymax - 2, &rectf.xmax, &rectf.ymax);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  float ymax;
  if (ac->datatype == ANIMCONT_NLA) {
    ymax = NLACHANNEL_FIRST_TOP(ac);
  }
  else {
    ymax = ANIM_UI_get_first_channel_top(v2d);
  }

  /* loop over data, doing box select */
  for (ale = anim_data.first; ale; ale = ale->next) {
    float ymin;

    if (ale->type == ANIMTYPE_GPDATABLOCK) {
      ymax -= ANIM_UI_get_channel_step();
      continue;
    }

    if (ac->datatype == ANIMCONT_NLA) {
      ymin = ymax - NLACHANNEL_STEP(snla);
    }
    else {
      ymin = ymax - ANIM_UI_get_channel_step();
    }

    /* if channel is within border-select region, alter it */
    if (ymax >= rectf.ymin && ymin <= rectf.ymax) {
      /* set selection flags only */
      ANIM_channel_setting_set(ac, ale, ACHANNEL_SETTING_SELECT, selectmode);

      /* type specific actions */
      switch (ale->type) {
        case ANIMTYPE_GROUP: {
          bActionGroup *agrp = (bActionGroup *)ale->data;
          select_pchan_for_action_group(ac, agrp, ale, true);
          /* always clear active flag after doing this */
          agrp->flag &= ~AGRP_ACTIVE;
          break;
        }
        case ANIMTYPE_NLATRACK: {
          NlaTrack *nlt = (NlaTrack *)ale->data;

          /* for now, it's easier just to do this here manually, as defining a new type
           * currently adds complications when doing other stuff
           */
          ACHANNEL_SET_FLAG(nlt, selectmode, NLATRACK_SELECTED);
          break;
        }
      }
    }

    /* set minimum extent to be the maximum of the next channel */
    ymax = ymin;
  }

  /* cleanup */
  ANIM_animdata_freelist(&anim_data);
}

static int animchannels_box_select_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  rcti rect;
  short selectmode = 0;
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get settings from operator */
  WM_operator_properties_border_to_rcti(op, &rect);

  if (!extend) {
    ANIM_anim_channels_select_set(&ac, ACHANNEL_SETFLAG_CLEAR);
  }

  if (select) {
    selectmode = ACHANNEL_SETFLAG_ADD;
  }
  else {
    selectmode = ACHANNEL_SETFLAG_CLEAR;
  }

  /* apply box_select animation channels */
  box_select_anim_channels(&ac, &rect, selectmode);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, NULL);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channels_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "ANIM_OT_channels_select_box";
  ot->description = "Select all animation channels within the specified region";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = animchannels_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = animedit_poll_channels_nla_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  WM_operator_properties_gesture_box_select(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rename Channel Operator
 *
 * Allow renaming some channels by clicking on them.
 * \{ */

static bool rename_anim_channels(bAnimContext *ac, int channel_index)
{
  ListBase anim_data = {NULL, NULL};
  const bAnimChannelType *acf;
  bAnimListElem *ale;
  int filter;
  bool success = false;

  /* Filter relevant channels (note that grease-pencil/annotations are not displayed in Graph
   * Editor). */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  if (ELEM(ac->datatype, ANIMCONT_FCURVES, ANIMCONT_NLA)) {
    filter |= ANIMFILTER_FCURVESONLY;
  }
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Get channel that was clicked on from index. */
  ale = BLI_findlink(&anim_data, channel_index);
  if (ale == NULL) {
    /* channel not found */
    if (G.debug & G_DEBUG) {
      printf("Error: animation channel (index = %d) not found in rename_anim_channels()\n",
             channel_index);
    }

    ANIM_animdata_freelist(&anim_data);
    return false;
  }

  /* Don't allow renaming linked/liboverride channels. */
  if (ale->fcurve_owner_id != NULL &&
      (ID_IS_LINKED(ale->fcurve_owner_id) || ID_IS_OVERRIDE_LIBRARY(ale->fcurve_owner_id)))
  {
    ANIM_animdata_freelist(&anim_data);
    return false;
  }
  if (ale->id != NULL) {
    if (ID_IS_LINKED(ale->id)) {
      ANIM_animdata_freelist(&anim_data);
      return false;
    }
    /* There is one exception to not allowing renaming on liboverride channels: locally-inserted
     * NLA tracks. */
    if (ID_IS_OVERRIDE_LIBRARY(ale->id)) {
      switch (ale->type) {
        case ANIMTYPE_NLATRACK: {
          NlaTrack *nlt = (NlaTrack *)ale->data;
          if ((nlt->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0) {
            ANIM_animdata_freelist(&anim_data);
            return false;
          }
          break;
        }
        default:
          ANIM_animdata_freelist(&anim_data);
          return false;
      }
    }
  }

  /* check that channel can be renamed */
  acf = ANIM_channel_get_typeinfo(ale);
  if (acf && acf->name_prop) {
    PointerRNA ptr;
    PropertyRNA *prop;

    /* ok if we can get name property to edit from this channel */
    if (acf->name_prop(ale, &ptr, &prop)) {
      /* Actually showing the rename text-field is done on redraw,
       * so here we just store the index of this channel in the
       * dope-sheet data, which will get utilized when drawing the channel.
       *
       * +1 factor is for backwards compatibility issues. */
      if (ac->ads) {
        ac->ads->renameIndex = channel_index + 1;
        success = true;
      }
    }
  }

  /* free temp data and tag for refresh */
  ANIM_animdata_freelist(&anim_data);
  ED_region_tag_redraw(ac->region);
  return success;
}

static int animchannels_channel_get(bAnimContext *ac, const int mval[2])
{
  ARegion *region;
  View2D *v2d;
  int channel_index;
  float x, y;

  /* get useful pointers from animation context data */
  region = ac->region;
  v2d = &region->v2d;

  /* Figure out which channel user clicked in. */
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);

  if (ac->datatype == ANIMCONT_NLA) {
    SpaceNla *snla = (SpaceNla *)ac->sl;
    UI_view2d_listview_view_to_cell(NLACHANNEL_NAMEWIDTH,
                                    NLACHANNEL_STEP(snla),
                                    0,
                                    NLACHANNEL_FIRST_TOP(ac),
                                    x,
                                    y,
                                    NULL,
                                    &channel_index);
  }
  else {
    UI_view2d_listview_view_to_cell(ANIM_UI_get_channel_name_width(),
                                    ANIM_UI_get_channel_step(),
                                    0,
                                    ANIM_UI_get_first_channel_top(v2d),
                                    x,
                                    y,
                                    NULL,
                                    &channel_index);
  }

  return channel_index;
}

static int animchannels_rename_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  bAnimContext ac;
  int channel_index;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  channel_index = animchannels_channel_get(&ac, event->mval);

  /* handle click */
  if (rename_anim_channels(&ac, channel_index)) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_RENAME, NULL);
    return OPERATOR_FINISHED;
  }

  /* allow event to be handled by selectall operator */
  return OPERATOR_PASS_THROUGH;
}

static void ANIM_OT_channels_rename(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rename Channels";
  ot->idname = "ANIM_OT_channels_rename";
  ot->description = "Rename animation channel under mouse";

  /* api callbacks */
  ot->invoke = animchannels_rename_invoke;
  ot->poll = animedit_poll_channels_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Channel Keyframes Operator (Internal Logic)
 * \{ */

/* Handle selection changes due to clicking on channels. Settings will get caught by UI code... */

static int click_select_channel_scene(bAnimListElem *ale,
                                      const short /* eEditKeyframes_Select or -1 */ selectmode)
{
  Scene *sce = (Scene *)ale->data;
  AnimData *adt = sce->adt;

  /* set selection status */
  if (selectmode == SELECT_INVERT) {
    /* swap select */
    sce->flag ^= SCE_DS_SELECTED;
    if (adt) {
      adt->flag ^= ADT_UI_SELECTED;
    }
  }
  else {
    sce->flag |= SCE_DS_SELECTED;
    if (adt) {
      adt->flag |= ADT_UI_SELECTED;
    }
  }
  return (ND_ANIMCHAN | NA_SELECTED);
}

/* Return whether active channel of given type is present. */
static bool animchannel_has_active_of_type(bAnimContext *ac, const eAnim_ChannelType type)
{
  ListBase anim_data = anim_channels_for_selection(ac);
  bool is_active_found = false;

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type != type) {
      continue;
    }
    is_active_found = ANIM_is_active_channel(ale);
    if (is_active_found) {
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);
  return is_active_found;
}

/* Select channels that lies between active channel and cursor_elem. */
static void animchannel_select_range(bAnimContext *ac, bAnimListElem *cursor_elem)
{
  ListBase anim_data = anim_channels_for_selection(ac);
  bool in_selection_range = false;

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {

    /* Allow selection when active channel and `cursor_elem` are of same type. */
    if (ale->type != cursor_elem->type) {
      continue;
    }

    const bool is_cursor_elem = (ale->data == cursor_elem->data);
    const bool is_active_elem = ANIM_is_active_channel(ale);

    /* Restrict selection when active element is not found and group-channels are excluded from the
     * selection. */
    if (is_active_elem || is_cursor_elem) {
      /* Select first and last element from the range. Reverse selection status on extremes. */
      ANIM_channel_setting_set(ac, ale, ACHANNEL_SETTING_SELECT, ACHANNEL_SETFLAG_ADD);
      in_selection_range = !in_selection_range;
      if (ale->type == ANIMTYPE_GROUP) {
        select_pchan_for_action_group(ac, (bActionGroup *)ale->data, ale, false);
      }
    }
    else if (in_selection_range) {
      /* Select elements between the range. */
      ANIM_channel_setting_set(ac, ale, ACHANNEL_SETTING_SELECT, ACHANNEL_SETFLAG_ADD);
      if (ale->type == ANIMTYPE_GROUP) {
        select_pchan_for_action_group(ac, (bActionGroup *)ale->data, ale, false);
      }
    }

    if (is_active_elem && is_cursor_elem) {
      /* Selection range is only one element when active channel and clicked channel are same. So
       * exit out of the loop when this condition is hit. */
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);
}

static int click_select_channel_object(bContext *C,
                                       bAnimContext *ac,
                                       bAnimListElem *ale,
                                       const short /* eEditKeyframes_Select or -1 */ selectmode)
{
  Scene *scene = ac->scene;
  ViewLayer *view_layer = ac->view_layer;
  Base *base = (Base *)ale->data;
  Object *ob = base->object;
  AnimData *adt = ob->adt;

  if ((base->flag & BASE_SELECTABLE) == 0) {
    return 0;
  }

  if (selectmode == SELECT_INVERT) {
    /* swap select */
    ED_object_base_select(base, BA_INVERT);

    if (adt) {
      adt->flag ^= ADT_UI_SELECTED;
    }
  }
  else if (selectmode == SELECT_EXTEND_RANGE) {
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_EXTEND_RANGE);
    animchannel_select_range(ac, ale);
  }
  else {
    /* deselect all */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    BKE_view_layer_synced_ensure(scene, view_layer);
    /* TODO: should this deselect all other types of channels too? */
    LISTBASE_FOREACH (Base *, b, BKE_view_layer_object_bases_get(view_layer)) {
      ED_object_base_select(b, BA_DESELECT);
      if (b->object->adt) {
        b->object->adt->flag &= ~(ADT_UI_SELECTED | ADT_UI_ACTIVE);
      }
    }

    /* select object now */
    ED_object_base_select(base, BA_SELECT);
    if (adt) {
      adt->flag |= ADT_UI_SELECTED;
    }
  }

  /* Change active object - regardless of whether it is now selected, see: #37883.
   *
   * Ensure we exit edit-mode on whatever object was active before
   * to avoid getting stuck there, see: #48747. */
  ED_object_base_activate_with_mode_exit_if_needed(C, base); /* adds notifier */

  /* Similar to outliner, do not change active element when selecting elements in range.*/
  if ((adt) && (adt->flag & ADT_UI_SELECTED) && (selectmode != SELECT_EXTEND_RANGE)) {
    adt->flag |= ADT_UI_ACTIVE;
  }

  return (ND_ANIMCHAN | NA_SELECTED);
}

static int click_select_channel_dummy(bAnimContext *ac,
                                      bAnimListElem *ale,
                                      const short /* eEditKeyframes_Select or -1 */ selectmode)
{
  if (ale->adt == NULL) {
    return 0;
  }

  /* select/deselect */
  if (selectmode == SELECT_INVERT) {
    /* inverse selection status of this AnimData block only */
    ale->adt->flag ^= ADT_UI_SELECTED;
  }
  else if (selectmode == SELECT_EXTEND_RANGE) {
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_EXTEND_RANGE);
    animchannel_select_range(ac, ale);
  }
  else {
    /* select AnimData block by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    ale->adt->flag |= ADT_UI_SELECTED;
  }

  /* Similar to outliner, do not change active element when selecting elements in range. */
  if ((ale->adt->flag & ADT_UI_SELECTED) && (selectmode != SELECT_EXTEND_RANGE)) {
    ale->adt->flag |= ADT_UI_ACTIVE;
  }

  return (ND_ANIMCHAN | NA_SELECTED);
}

static int click_select_channel_group(bAnimContext *ac,
                                      bAnimListElem *ale,
                                      const short /* eEditKeyframes_Select or -1 */ selectmode,
                                      const int filter)
{
  bActionGroup *agrp = (bActionGroup *)ale->data;
  Object *ob = NULL;
  bPoseChannel *pchan = NULL;

  /* Armatures-Specific Feature:
   * Since groups are used to collect F-Curves of the same Bone by default
   * (via Keying Sets) so that they can be managed better, we try to make
   * things here easier for animators by mapping group selection to bone
   * selection.
   *
   * Only do this if "Only Selected" dopesheet filter is not active, or else it
   * becomes too unpredictable/tricky to manage
   */
  if ((ac->ads->filterflag & ADS_FILTER_ONLYSEL) == 0) {
    if ((ale->id) && (GS(ale->id->name) == ID_OB)) {
      ob = (Object *)ale->id;

      if (ob->type == OB_ARMATURE) {
        /* Assume for now that any group with corresponding name is what we want
         * (i.e. for an armature whose location is animated, things would break
         * if the user were to add a bone named "Location").
         *
         * TODO: check the first F-Curve or so to be sure...
         */
        pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);
      }
    }
  }

  /* select/deselect group */
  if (selectmode == SELECT_INVERT) {
    /* inverse selection status of this group only */
    agrp->flag ^= AGRP_SELECTED;
  }
  else if (selectmode == SELECT_EXTEND_RANGE) {
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_EXTEND_RANGE);
    animchannel_select_range(ac, ale);
  }
  else if (selectmode == -1) {
    /* select all in group (and deselect everything else) */
    FCurve *fcu;

    /* deselect all other channels */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    if (pchan) {
      ED_pose_deselect_all(ob, SEL_DESELECT, false);
    }

    /* only select channels in group and group itself */
    for (fcu = agrp->channels.first; fcu && fcu->grp == agrp; fcu = fcu->next) {
      fcu->flag |= FCURVE_SELECTED;
    }
    agrp->flag |= AGRP_SELECTED;
  }
  else {
    /* select group by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    if (pchan) {
      ED_pose_deselect_all(ob, SEL_DESELECT, false);
    }

    agrp->flag |= AGRP_SELECTED;
  }

  /* if group is selected now, make group the 'active' one in the visible list.
   * Similar to outliner, do not change active element when selecting elements in range. */
  if (agrp->flag & AGRP_SELECTED) {
    if (selectmode != SELECT_EXTEND_RANGE) {
      ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, agrp, ANIMTYPE_GROUP);
      if (pchan) {
        ED_pose_bone_select(ob, pchan, true, true);
      }
    }
  }
  else {
    if (selectmode != SELECT_EXTEND_RANGE) {
      ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, NULL, ANIMTYPE_GROUP);
      if (pchan) {
        ED_pose_bone_select(ob, pchan, false, true);
      }
    }
  }

  return (ND_ANIMCHAN | NA_SELECTED);
}

static int click_select_channel_fcurve(bAnimContext *ac,
                                       bAnimListElem *ale,
                                       const short /* eEditKeyframes_Select or -1 */ selectmode,
                                       const int filter)
{
  FCurve *fcu = (FCurve *)ale->data;

  /* select/deselect */
  if (selectmode == SELECT_INVERT) {
    /* inverse selection status of this F-Curve only */
    fcu->flag ^= FCURVE_SELECTED;
  }
  else if (selectmode == SELECT_EXTEND_RANGE) {
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_EXTEND_RANGE);
    animchannel_select_range(ac, ale);
  }
  else {
    /* select F-Curve by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    fcu->flag |= FCURVE_SELECTED;
  }

  /* if F-Curve is selected now, make F-Curve the 'active' one in the visible list.
   * Similar to outliner, do not change active element when selecting elements in range. */
  if ((fcu->flag & FCURVE_SELECTED) && (selectmode != SELECT_EXTEND_RANGE)) {
    ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, fcu, ale->type);
  }

  return (ND_ANIMCHAN | NA_SELECTED);
}

static int click_select_channel_shapekey(bAnimContext *ac,
                                         bAnimListElem *ale,
                                         const short /* eEditKeyframes_Select or -1 */ selectmode)
{
  KeyBlock *kb = (KeyBlock *)ale->data;

  /* select/deselect */
  if (selectmode == SELECT_INVERT) {
    /* inverse selection status of this ShapeKey only */
    kb->flag ^= KEYBLOCK_SEL;
  }
  else {
    /* select ShapeKey by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    kb->flag |= KEYBLOCK_SEL;
  }

  return (ND_ANIMCHAN | NA_SELECTED);
}

static int click_select_channel_nlacontrols(bAnimListElem *ale)
{
  AnimData *adt = (AnimData *)ale->data;

  /* Toggle expand:
   * - Although the triangle widget already allows this,
   *   since there's nothing else that can be done here now,
   *   let's just use it for easier expand/collapse for now.
   */
  adt->flag ^= ADT_NLA_SKEYS_COLLAPSED;

  return (ND_ANIMCHAN | NA_EDITED);
}

static int click_select_channel_gpdatablock(bAnimListElem *ale)
{
  bGPdata *gpd = (bGPdata *)ale->data;

  /* Toggle expand:
   * - Although the triangle widget already allows this,
   *   the whole channel can also be used for this purpose.
   */
  gpd->flag ^= GP_DATA_EXPAND;

  return (ND_ANIMCHAN | NA_EDITED);
}

static int click_select_channel_gplayer(bContext *C,
                                        bAnimContext *ac,
                                        bAnimListElem *ale,
                                        const short /* eEditKeyframes_Select or -1 */ selectmode,
                                        const int filter)
{
  bGPdata *gpd = (bGPdata *)ale->id;
  bGPDlayer *gpl = (bGPDlayer *)ale->data;

  /* select/deselect */
  if (selectmode == SELECT_INVERT) {
    /* invert selection status of this layer only */
    gpl->flag ^= GP_LAYER_SELECT;
  }
  else if (selectmode == SELECT_EXTEND_RANGE) {
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_EXTEND_RANGE);
    animchannel_select_range(ac, ale);
  }
  else {
    /* select layer by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    gpl->flag |= GP_LAYER_SELECT;
  }

  /* change active layer, if this is selected (since we must always have an active layer).
   * Similar to outliner, do not change active element when selecting elements in range. */
  if ((gpl->flag & GP_LAYER_SELECT) && (selectmode != SELECT_EXTEND_RANGE)) {
    ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, gpl, ANIMTYPE_GPLAYER);
    /* update other layer status */
    BKE_gpencil_layer_active_set(gpd, gpl);
    BKE_gpencil_layer_autolock_set(gpd, false);
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
  }

  /* Grease Pencil updates */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);
  return (ND_ANIMCHAN | NA_EDITED); /* Animation Editors updates */
}

static int click_select_channel_maskdatablock(bAnimListElem *ale)
{
  Mask *mask = (Mask *)ale->data;

  /* Toggle expand
   * - Although the triangle widget already allows this,
   *   the whole channel can also be used for this purpose.
   */
  mask->flag ^= MASK_ANIMF_EXPAND;

  return (ND_ANIMCHAN | NA_EDITED);
}

static int click_select_channel_masklayer(bAnimContext *ac,
                                          bAnimListElem *ale,
                                          const short /* eEditKeyframes_Select or -1 */ selectmode)
{
  MaskLayer *masklay = (MaskLayer *)ale->data;

  /* select/deselect */
  if (selectmode == SELECT_INVERT) {
    /* invert selection status of this layer only */
    masklay->flag ^= MASK_LAYERFLAG_SELECT;
  }
  else {
    /* select layer by itself */
    ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    masklay->flag |= MASK_LAYERFLAG_SELECT;
  }

  return (ND_ANIMCHAN | NA_EDITED);
}

static int mouse_anim_channels(bContext *C,
                               bAnimContext *ac,
                               const int channel_index,
                               short /* eEditKeyframes_Select or -1 */ selectmode)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  int notifierFlags = 0;
  ScrArea *area = CTX_wm_area(C);

  /* get the channel that was clicked on */
  /* filter channels */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  if (ELEM(area->spacetype, SPACE_NLA, SPACE_GRAPH)) {
    filter |= ANIMFILTER_FCURVESONLY;
  }
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* get channel from index */
  ale = BLI_findlink(&anim_data, channel_index);
  if (ale == NULL) {
    /* channel not found */
    if (G.debug & G_DEBUG) {
      printf("Error: animation channel (index = %d) not found in mouse_anim_channels()\n",
             channel_index);
    }

    ANIM_animdata_freelist(&anim_data);
    return 0;
  }

  /* selectmode -1 is a special case for ActionGroups only,
   * which selects all of the channels underneath it only. */
  /* TODO: should this feature be extended to work with other channel types too? */
  if ((selectmode == -1) && (ale->type != ANIMTYPE_GROUP)) {
    /* normal channels should not behave normally in this case */
    ANIM_animdata_freelist(&anim_data);
    return 0;
  }

  /* Change selection mode to single when no active element is found. */
  if ((selectmode == SELECT_EXTEND_RANGE) && !animchannel_has_active_of_type(ac, ale->type)) {
    selectmode = SELECT_INVERT;
  }

  /* action to take depends on what channel we've got */
  /* WARNING: must keep this in sync with the equivalent function in nla_channels.c */
  switch (ale->type) {
    case ANIMTYPE_SCENE:
      notifierFlags |= click_select_channel_scene(ale, selectmode);
      break;
    case ANIMTYPE_OBJECT:
      notifierFlags |= click_select_channel_object(C, ac, ale, selectmode);
      break;
    case ANIMTYPE_FILLACTD: /* Action Expander */
    case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
    case ANIMTYPE_DSLAM:
    case ANIMTYPE_DSCAM:
    case ANIMTYPE_DSCACHEFILE:
    case ANIMTYPE_DSCUR:
    case ANIMTYPE_DSSKEY:
    case ANIMTYPE_DSWOR:
    case ANIMTYPE_DSPART:
    case ANIMTYPE_DSMBALL:
    case ANIMTYPE_DSARM:
    case ANIMTYPE_DSMESH:
    case ANIMTYPE_DSNTREE:
    case ANIMTYPE_DSTEX:
    case ANIMTYPE_DSLAT:
    case ANIMTYPE_DSLINESTYLE:
    case ANIMTYPE_DSSPK:
    case ANIMTYPE_DSGPENCIL:
    case ANIMTYPE_DSMCLIP:
    case ANIMTYPE_DSHAIR:
    case ANIMTYPE_DSPOINTCLOUD:
    case ANIMTYPE_DSVOLUME:
    case ANIMTYPE_DSSIMULATION:
      notifierFlags |= click_select_channel_dummy(ac, ale, selectmode);
      break;
    case ANIMTYPE_GROUP:
      notifierFlags |= click_select_channel_group(ac, ale, selectmode, filter);
      break;
    case ANIMTYPE_FCURVE:
    case ANIMTYPE_NLACURVE:
      notifierFlags |= click_select_channel_fcurve(ac, ale, selectmode, filter);
      break;
    case ANIMTYPE_SHAPEKEY:
      notifierFlags |= click_select_channel_shapekey(ac, ale, selectmode);
      break;
    case ANIMTYPE_NLACONTROLS:
      notifierFlags |= click_select_channel_nlacontrols(ale);
      break;
    case ANIMTYPE_GPDATABLOCK:
      notifierFlags |= click_select_channel_gpdatablock(ale);
      break;
    case ANIMTYPE_GPLAYER:
      notifierFlags |= click_select_channel_gplayer(C, ac, ale, selectmode, filter);
      break;
    case ANIMTYPE_MASKDATABLOCK:
      notifierFlags |= click_select_channel_maskdatablock(ale);
      break;
    case ANIMTYPE_MASKLAYER:
      notifierFlags |= click_select_channel_masklayer(ac, ale, selectmode);
      break;
    default:
      if (G.debug & G_DEBUG) {
        printf("Error: Invalid channel type in mouse_anim_channels()\n");
      }
      break;
  }

  /* free channels */
  ANIM_animdata_freelist(&anim_data);

  /* return notifier flags */
  return notifierFlags;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Channel Keyframes Operator
 * \{ */

/** Handle picking logic. */
static int animchannels_mouseclick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;
  ARegion *region;
  View2D *v2d;
  int channel_index;
  int notifierFlags = 0;
  short selectmode;
  float x, y;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get useful pointers from animation context data */
  region = ac.region;
  v2d = &region->v2d;

  /* select mode is either replace (deselect all, then add) or add/extend */
  if (RNA_boolean_get(op->ptr, "extend")) {
    selectmode = SELECT_INVERT;
  }
  else if (RNA_boolean_get(op->ptr, "extend_range")) {
    selectmode = SELECT_EXTEND_RANGE;
  }
  else if (RNA_boolean_get(op->ptr, "children_only")) {
    /* this is a bit of a special case for ActionGroups only...
     * should it be removed or extended to all instead? */
    selectmode = -1;
  }
  else {
    selectmode = SELECT_REPLACE;
  }

  /* figure out which channel user clicked in */
  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, &y);
  UI_view2d_listview_view_to_cell(ANIM_UI_get_channel_name_width(),
                                  ANIM_UI_get_channel_step(),
                                  0,
                                  ANIM_UI_get_first_channel_top(v2d),
                                  x,
                                  y,
                                  NULL,
                                  &channel_index);

  /* handle mouse-click in the relevant channel then */
  notifierFlags = mouse_anim_channels(C, &ac, channel_index, selectmode);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | notifierFlags, NULL);

  return WM_operator_flag_only_pass_through_on_press(OPERATOR_FINISHED | OPERATOR_PASS_THROUGH,
                                                     event);
}

static void ANIM_OT_channels_click(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Mouse Click on Channels";
  ot->idname = "ANIM_OT_channels_click";
  ot->description = "Handle mouse clicks over animation channels";

  /* api callbacks */
  ot->invoke = animchannels_mouseclick_invoke;
  ot->poll = animedit_poll_channels_active;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  /* NOTE: don't save settings, otherwise, can end up with some weird behavior (sticky extend)
   *
   * Key-map: Enable with `Shift`. */
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend Select", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "extend_range",
                         false,
                         "Extend Range",
                         "Selection of active channel to clicked channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Key-map: Enable with `Ctrl-Shift`. */
  prop = RNA_def_boolean(ot->srna, "children_only", false, "Select Children Only", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static bool select_anim_channel_keys(bAnimContext *ac, int channel_index, bool extend)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  bool success = false;
  FCurve *fcu;
  int i;

  /* get the channel that was clicked on */
  /* filter channels */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* get channel from index */
  ale = BLI_findlink(&anim_data, channel_index);
  if (ale == NULL) {
    /* channel not found */
    if (G.debug & G_DEBUG) {
      printf("Error: animation channel (index = %d) not found in rename_anim_channels()\n",
             channel_index);
    }

    ANIM_animdata_freelist(&anim_data);
    return false;
  }

  fcu = (FCurve *)ale->key_data;
  success = (fcu != NULL);

  ANIM_animdata_freelist(&anim_data);

  /* F-Curve may not have any keyframes */
  if (fcu && fcu->bezt) {
    BezTriple *bezt;

    if (!extend) {
      filter = (ANIMFILTER_DATA_VISIBLE);
      ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
      for (ale = anim_data.first; ale; ale = ale->next) {
        FCurve *fcu_inner = (FCurve *)ale->key_data;

        if (fcu_inner != NULL && fcu_inner->bezt != NULL) {
          for (i = 0, bezt = fcu_inner->bezt; i < fcu_inner->totvert; i++, bezt++) {
            bezt->f2 = bezt->f1 = bezt->f3 = 0;
          }
        }
      }

      ANIM_animdata_freelist(&anim_data);
    }

    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      bezt->f2 = bezt->f1 = bezt->f3 = SELECT;
    }
  }

  /* free temp data and tag for refresh */
  ED_region_tag_redraw(ac->region);
  return success;
}

static int animchannels_channel_select_keys_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  bAnimContext ac;
  int channel_index;
  bool extend = RNA_boolean_get(op->ptr, "extend");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  channel_index = animchannels_channel_get(&ac, event->mval);

  /* handle click */
  if (select_anim_channel_keys(&ac, channel_index, extend)) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
    return OPERATOR_FINISHED;
  }

  /* allow event to be handled by selectall operator */
  return OPERATOR_PASS_THROUGH;
}

static void ANIM_OT_channel_select_keys(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Channel Keyframes";
  ot->idname = "ANIM_OT_channel_select_keys";
  ot->description = "Select all keyframes of channel under mouse";

  /* api callbacks */
  ot->invoke = animchannels_channel_select_keys_invoke;
  ot->poll = animedit_poll_channels_active;

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Channel Operator
 * \{ */

static void get_view_range(Scene *scene, const bool use_preview_range, float r_range[2])
{
  if (use_preview_range && scene->r.flag & SCER_PRV_RANGE) {
    r_range[0] = scene->r.psfra;
    r_range[1] = scene->r.pefra;
  }
  else {
    r_range[0] = scene->r.sfra;
    r_range[1] = scene->r.efra;
  }
}

static int graphkeys_view_selected_channels_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  ARegion *window_region = BKE_area_find_region_type(ac.area, RGN_TYPE_WINDOW);

  if (!window_region) {
    return OPERATOR_CANCELLED;
  }

  ListBase anim_data = {NULL, NULL};
  const int filter = (ANIMFILTER_SEL | ANIMFILTER_NODUPLIS | ANIMFILTER_DATA_VISIBLE |
                      ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  size_t anim_data_length = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  if (anim_data_length == 0) {
    WM_report(RPT_WARNING, "No channels to operate on");
    return OPERATOR_CANCELLED;
  }

  float range[2];
  const bool use_preview_range = RNA_boolean_get(op->ptr, "use_preview_range");
  get_view_range(ac.scene, use_preview_range, range);

  rctf bounds = {.xmin = FLT_MAX, .xmax = -FLT_MAX, .ymin = FLT_MAX, .ymax = -FLT_MAX};

  bAnimListElem *ale;
  const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");

  bool valid_bounds = false;
  for (ale = anim_data.first; ale; ale = ale->next) {
    rctf channel_bounds;
    const bool found_bounds = get_channel_bounds(
        &ac, ale, range, include_handles, &channel_bounds);
    if (found_bounds) {
      BLI_rctf_union(&bounds, &channel_bounds);
      valid_bounds = true;
    }
  }

  if (!valid_bounds) {
    ANIM_animdata_freelist(&anim_data);
    WM_report(RPT_WARNING, "No keyframes to focus on");
    return OPERATOR_CANCELLED;
  }

  add_region_padding(C, window_region, &bounds);

  if (ac.spacetype == SPACE_ACTION) {
    bounds.ymin = window_region->v2d.cur.ymin;
    bounds.ymax = window_region->v2d.cur.ymax;
  }

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  UI_view2d_smooth_view(C, window_region, &bounds, smooth_viewtx);

  ANIM_animdata_freelist(&anim_data);

  return OPERATOR_FINISHED;
}

static bool channel_view_poll(bContext *C)
{
  return ED_operator_action_active(C) || ED_operator_graphedit_active(C);
}

static void ANIM_OT_channels_view_selected(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Frame Selected Channels";
  ot->idname = "ANIM_OT_channels_view_selected";
  ot->description = "Reset viewable area to show the selected channels";

  /* API callbacks */
  ot->exec = graphkeys_view_selected_channels_exec;
  ot->poll = channel_view_poll;

  ot->flag = 0;

  ot->prop = RNA_def_boolean(ot->srna,
                             "include_handles",
                             true,
                             "Include Handles",
                             "Include handles of keyframes when calculating extents");

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_preview_range",
                             true,
                             "Use Preview Range",
                             "Ignore frames outside of the preview range");
}

static int graphkeys_channel_view_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  ARegion *window_region = BKE_area_find_region_type(ac.area, RGN_TYPE_WINDOW);

  if (!window_region) {
    return OPERATOR_CANCELLED;
  }

  ListBase anim_data = {NULL, NULL};
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS |
                      ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  bAnimListElem *ale;
  const int channel_index = animchannels_channel_get(&ac, event->mval);
  ale = BLI_findlink(&anim_data, channel_index);
  if (ale == NULL) {
    ANIM_animdata_freelist(&anim_data);
    return OPERATOR_CANCELLED;
  }

  float range[2];
  const bool use_preview_range = RNA_boolean_get(op->ptr, "use_preview_range");

  get_view_range(ac.scene, use_preview_range, range);

  rctf bounds;
  const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");
  const bool found_bounds = get_channel_bounds(&ac, ale, range, include_handles, &bounds);

  if (!found_bounds) {
    ANIM_animdata_freelist(&anim_data);
    WM_report(RPT_WARNING, "No keyframes to focus on");
    return OPERATOR_CANCELLED;
  }

  add_region_padding(C, window_region, &bounds);

  if (ac.spacetype == SPACE_ACTION) {
    bounds.ymin = window_region->v2d.cur.ymin;
    bounds.ymax = window_region->v2d.cur.ymax;
  }

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  UI_view2d_smooth_view(C, window_region, &bounds, smooth_viewtx);

  ANIM_animdata_freelist(&anim_data);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_channel_view_pick(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Frame Channel Under Cursor";
  ot->idname = "ANIM_OT_channel_view_pick";
  ot->description = "Reset viewable area to show the channel under the cursor";

  /* API callbacks */
  ot->invoke = graphkeys_channel_view_pick_invoke;
  ot->poll = channel_view_poll;

  ot->flag = 0;

  ot->prop = RNA_def_boolean(ot->srna,
                             "include_handles",
                             true,
                             "Include Handles",
                             "Include handles of keyframes when calculating extents");

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_preview_range",
                             true,
                             "Use Preview Range",
                             "Ignore frames outside of the preview range");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Registration
 * \{ */

void ED_operatortypes_animchannels(void)
{
  WM_operatortype_append(ANIM_OT_channels_select_all);
  WM_operatortype_append(ANIM_OT_channels_select_box);

  WM_operatortype_append(ANIM_OT_channels_click);
  WM_operatortype_append(ANIM_OT_channel_select_keys);
  WM_operatortype_append(ANIM_OT_channels_rename);

  WM_operatortype_append(ANIM_OT_channels_select_filter);

  WM_operatortype_append(ANIM_OT_channels_setting_enable);
  WM_operatortype_append(ANIM_OT_channels_setting_disable);
  WM_operatortype_append(ANIM_OT_channels_setting_toggle);

  WM_operatortype_append(ANIM_OT_channel_view_pick);
  WM_operatortype_append(ANIM_OT_channels_view_selected);

  WM_operatortype_append(ANIM_OT_channels_delete);

  /* XXX does this need to be a separate operator? */
  WM_operatortype_append(ANIM_OT_channels_editable_toggle);

  WM_operatortype_append(ANIM_OT_channels_move);

  WM_operatortype_append(ANIM_OT_channels_expand);
  WM_operatortype_append(ANIM_OT_channels_collapse);

  WM_operatortype_append(ANIM_OT_channels_fcurves_enable);

  WM_operatortype_append(ANIM_OT_channels_clean_empty);

  WM_operatortype_append(ANIM_OT_channels_group);
  WM_operatortype_append(ANIM_OT_channels_ungroup);
}

void ED_keymap_animchannels(wmKeyConfig *keyconf)
{
  /* TODO: check on a poll callback for this, to get hotkeys into menus. */

  WM_keymap_ensure(keyconf, "Animation Channels", 0, 0);
}

/** \} */
