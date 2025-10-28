/* SPDX-FileCopyrightText: 2008 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

/* This file contains a system used to provide a layer of abstraction between sources
 * of animation data and tools in Animation Editors. The method used here involves
 * generating a list of edit structures which enable tools to naively perform the actions
 * they require without all the boiler-plate associated with loops within loops and checking
 * for cases to ignore.
 *
 * While this is primarily used for the Action/Dopesheet Editor (and its accessory modes),
 * the Graph Editor also uses this for its channel list and for determining which curves
 * are being edited. Likewise, the NLA Editor also uses this for its channel list and in
 * its operators.
 *
 * NOTE: much of the original system this was based on was built before the creation of the RNA
 * system. In future, it would be interesting to replace some parts of this code with RNA queries,
 * however, RNA does not eliminate some of the boiler-plate reduction benefits presented by this
 * system, so if any such work does occur, it should only be used for the internals used here...
 *
 * -- Joshua Leung, Dec 2008 (Last revision July 2009)
 */

#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_layer_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_userdef_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "ED_anim_api.hh"
#include "ED_markers.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "ANIM_action.hh"
#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "anim_intern.hh"

using namespace blender;

/* ************************************************************ */
/* Blender Context <-> Animation Context mapping */

bAction *ANIM_active_action_from_area(Scene *scene,
                                      ViewLayer *view_layer,
                                      const ScrArea *area,
                                      ID **r_action_user)
{
  if (area->spacetype != SPACE_ACTION) {
    return nullptr;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (!ob) {
    return nullptr;
  }

  const SpaceAction *saction = static_cast<const SpaceAction *>(area->spacedata.first);
  switch (eAnimEdit_Context(saction->mode)) {
    case SACTCONT_ACTION: {
      bAction *active_action = ob->adt ? ob->adt->action : nullptr;
      if (r_action_user) {
        *r_action_user = &ob->id;
      }
      return active_action;
    }

    case SACTCONT_SHAPEKEY: {
      Key *active_key = BKE_key_from_object(ob);
      bAction *active_action = (active_key && active_key->adt) ? active_key->adt->action : nullptr;
      if (r_action_user) {
        *r_action_user = &active_key->id;
      }
      return active_action;
    }

    case SACTCONT_GPENCIL:
    case SACTCONT_DOPESHEET:
    case SACTCONT_MASK:
    case SACTCONT_CACHEFILE:
    case SACTCONT_TIMELINE:
      if (r_action_user) {
        *r_action_user = nullptr;
      }
      return nullptr;
  }

  BLI_assert_unreachable();
  return nullptr;
}

/* ----------- Private Stuff - Action Editor ------------- */

/* Get shapekey data being edited (for Action Editor -> ShapeKey mode) */
/* NOTE: there's a similar function in `key.cc` #BKE_key_from_object. */
static Key *actedit_get_shapekeys(bAnimContext *ac)
{
  Scene *scene = ac->scene;
  ViewLayer *view_layer = ac->view_layer;
  Object *ob;

  BKE_view_layer_synced_ensure(scene, view_layer);
  ob = BKE_view_layer_active_object_get(view_layer);
  if (ob == nullptr) {
    return nullptr;
  }

  /* XXX pinning is not available in 'ShapeKey' mode... */
  // if (saction->pin) { return nullptr; }

  /* shapekey data is stored with geometry data */
  return BKE_key_from_object(ob);
}

/* Get data being edited in Action Editor (depending on current 'mode') */
static bool actedit_get_context(bAnimContext *ac, SpaceAction *saction)
{
  /* get dopesheet */
  ac->ads = &saction->ads;
  ac->dopesheet_mode = eAnimEdit_Context(saction->mode);

  /* Set the default filters. These can be overridden later. */
  ac->filters.flag = eDopeSheet_FilterFlag(ac->ads->filterflag);
  ac->filters.flag2 = eDopeSheet_FilterFlag2(ac->ads->filterflag2);

  ac->active_action = ANIM_active_action_from_area(
      ac->scene, ac->view_layer, ac->area, &ac->active_action_user);

  /* sync settings with current view status, then return appropriate data */
  switch (saction->mode) {
    case SACTCONT_ACTION: /* 'Action Editor' */
      ac->datatype = ANIMCONT_ACTION;
      ac->data = ac->active_action;

      if (saction->flag & SACTION_POSEMARKERS_SHOW) {
        ac->markers = &ac->active_action->markers;
      }

      return true;

    case SACTCONT_SHAPEKEY: /* 'ShapeKey Editor' */
      ac->datatype = ANIMCONT_SHAPEKEY;
      ac->data = actedit_get_shapekeys(ac);

      if (saction->flag & SACTION_POSEMARKERS_SHOW) {
        ac->markers = &ac->active_action->markers;
      }

      return true;

    case SACTCONT_GPENCIL: /* Grease Pencil */ /* XXX review how this mode is handled... */
      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      saction->ads.source = reinterpret_cast<ID *>(ac->scene);

      ac->datatype = ANIMCONT_GPENCIL;
      ac->data = &saction->ads;
      return true;

    case SACTCONT_CACHEFILE: /* Cache File */ /* XXX review how this mode is handled... */
      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      saction->ads.source = reinterpret_cast<ID *>(ac->scene);

      ac->datatype = ANIMCONT_CHANNEL;
      ac->data = &saction->ads;
      return true;

    case SACTCONT_MASK: /* Mask */ /* XXX: review how this mode is handled. */
    {
      /* TODO: other methods to get the mask. */
#if 0
      Strip *strip = SEQ_select_active_get(ac->scene);
      MovieClip *clip = ac->scene->clip;
      struct Mask *mask = strip ? strip->mask : nullptr;
#endif

      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      saction->ads.source = reinterpret_cast<ID *>(ac->scene);

      ac->datatype = ANIMCONT_MASK;
      ac->data = &saction->ads;
      return true;
    }

    case SACTCONT_DOPESHEET: /* DopeSheet */
      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      saction->ads.source = reinterpret_cast<ID *>(ac->scene);

      ac->datatype = ANIMCONT_DOPESHEET;
      ac->data = &saction->ads;
      return true;

    case SACTCONT_TIMELINE: {
      saction->ads.source = reinterpret_cast<ID *>(ac->scene);

      ac->datatype = ANIMCONT_TIMELINE;
      ac->data = &saction->ads;

      /* The 'only show selected' filter has to come from the scene flag, not the dopesheet filter.
       * Most filter flags are hard-coded for the timeline. */
      const eDopeSheet_FilterFlag flag_only_selected = (ac->scene->flag & SCE_KEYS_NO_SELONLY) ?
                                                           eDopeSheet_FilterFlag(0) :
                                                           ADS_FILTER_ONLYSEL;
      const eDopeSheet_FilterFlag flag_only_errors = eDopeSheet_FilterFlag(ac->ads->filterflag &
                                                                           ADS_FILTER_ONLY_ERRORS);
      ac->filters.flag = flag_only_selected | flag_only_errors;
      ac->filters.flag2 = eDopeSheet_FilterFlag2(0);
      return true;
    }
    default: /* unhandled yet */
      ac->datatype = ANIMCONT_NONE;
      ac->data = nullptr;
      return false;
  }
}

/* ----------- Private Stuff - Graph Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static bool graphedit_get_context(bAnimContext *ac, SpaceGraph *sipo)
{
  /* init dopesheet data if non-existent (i.e. for old files) */
  if (sipo->ads == nullptr) {
    sipo->ads = MEM_callocN<bDopeSheet>("GraphEdit DopeSheet");
    sipo->ads->source = reinterpret_cast<ID *>(ac->scene);
  }
  ac->ads = sipo->ads;
  ac->grapheditor_mode = eGraphEdit_Mode(sipo->mode);

  /* set settings for Graph Editor - "Selected = Editable" */
  if (U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) {
    sipo->ads->filterflag |= ADS_FILTER_SELEDIT;
  }
  else {
    sipo->ads->filterflag &= ~ADS_FILTER_SELEDIT;
  }

  bool ok;

  /* sync settings with current view status, then return appropriate data */
  switch (sipo->mode) {
    case SIPO_MODE_ANIMATION: /* Animation F-Curve Editor */
      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      sipo->ads->source = reinterpret_cast<ID *>(ac->scene);
      sipo->ads->filterflag &= ~ADS_FILTER_ONLYDRIVERS;

      ac->datatype = ANIMCONT_FCURVES;
      ac->data = sipo->ads;
      ok = true;
      break;

    case SIPO_MODE_DRIVERS: /* Driver F-Curve Editor */
      /* update scene-pointer (no need to check for pinning yet, as not implemented) */
      sipo->ads->source = reinterpret_cast<ID *>(ac->scene);
      sipo->ads->filterflag |= ADS_FILTER_ONLYDRIVERS;

      ac->datatype = ANIMCONT_DRIVERS;
      ac->data = sipo->ads;
      ok = true;
      break;

    default: /* unhandled yet */
      ac->datatype = ANIMCONT_NONE;
      ac->data = nullptr;
      ok = false;
      break;
  }

  ac->filters.flag = eDopeSheet_FilterFlag(sipo->ads->filterflag);
  ac->filters.flag2 = eDopeSheet_FilterFlag2(sipo->ads->filterflag2);

  return ok;
}

/* ----------- Private Stuff - NLA Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static bool nlaedit_get_context(bAnimContext *ac, SpaceNla *snla)
{
  /* init dopesheet data if non-existent (i.e. for old files) */
  if (snla->ads == nullptr) {
    snla->ads = MEM_callocN<bDopeSheet>("NlaEdit DopeSheet");
  }
  ac->ads = snla->ads;

  /* sync settings with current view status, then return appropriate data */
  /* update scene-pointer (no need to check for pinning yet, as not implemented) */
  snla->ads->source = reinterpret_cast<ID *>(ac->scene);
  ac->filters.flag = eDopeSheet_FilterFlag(snla->ads->filterflag | ADS_FILTER_ONLYNLA);
  ac->filters.flag2 = eDopeSheet_FilterFlag2(snla->ads->filterflag2);

  ac->datatype = ANIMCONT_NLA;
  ac->data = snla->ads;

  return true;
}

/* ----------- Public API --------------- */

bool ANIM_animdata_context_getdata(bAnimContext *ac)
{
  SpaceLink *sl = ac->sl;
  bool ok = false;

  /* context depends on editor we are currently in */
  if (sl) {
    switch (ac->spacetype) {
      case SPACE_ACTION: {
        SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);
        ok = actedit_get_context(ac, saction);
        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(sl);
        ok = graphedit_get_context(ac, sipo);
        break;
      }
      case SPACE_NLA: {
        SpaceNla *snla = reinterpret_cast<SpaceNla *>(sl);
        ok = nlaedit_get_context(ac, snla);
        break;
      }
      case SPACE_EMPTY:
      case SPACE_VIEW3D:
      case SPACE_OUTLINER:
      case SPACE_PROPERTIES:
      case SPACE_FILE:
      case SPACE_IMAGE:
      case SPACE_INFO:
      case SPACE_SEQ:
      case SPACE_TEXT:
      case SPACE_SCRIPT:
      case SPACE_NODE:
      case SPACE_CONSOLE:
      case SPACE_USERPREF:
      case SPACE_CLIP:
      case SPACE_TOPBAR:
      case SPACE_STATUSBAR:
      case SPACE_SPREADSHEET:
        break;
    }
  }

  /* check if there's any valid data */
  return (ok && ac->data);
}

bool ANIM_animdata_get_context(const bContext *C, bAnimContext *ac)
{
  Main *bmain = CTX_data_main(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  SpaceLink *sl = CTX_wm_space_data(C);
  Scene *scene = CTX_data_scene(C);

  /* clear old context info */
  if (ac == nullptr) {
    return false;
  }
  memset(ac, 0, sizeof(bAnimContext));

  /* get useful default context settings from context */
  ac->bmain = bmain;
  ac->scene = scene;
  ac->view_layer = CTX_data_view_layer(C);
  if (scene) {
    /* This may be overwritten by actedit_get_context() when pose markers should be shown. */
    ac->markers = &scene->markers;
  }
  if (scene && ac->view_layer) {
    BKE_view_layer_synced_ensure(scene, ac->view_layer);
    ac->obact = BKE_view_layer_active_object_get(ac->view_layer);
  }
  ac->depsgraph = CTX_data_depsgraph_pointer(C);
  ac->area = area;
  ac->region = region;
  ac->sl = sl;
  ac->spacetype = eSpace_Type((area) ? area->spacetype : 0);
  ac->regiontype = eRegion_Type((region) ? region->regiontype : 0);

  /* get data context info */
  /* XXX: if the below fails, try to grab this info from context instead...
   * (to allow for scripting). */
  return ANIM_animdata_context_getdata(ac);
}

bool ANIM_animdata_can_have_greasepencil(const eAnimCont_Types type)
{
  return ELEM(type, ANIMCONT_GPENCIL, ANIMCONT_DOPESHEET, ANIMCONT_TIMELINE);
}

/* ************************************************************ */
/* Blender Data <-- Filter --> Channels to be operated on */

/* macros to use before/after getting the sub-channels of some channel,
 * to abstract away some of the tricky logic involved
 *
 * cases:
 * 1) Graph Edit main area (just data) OR channels visible in Channel List
 * 2) If not showing channels, we're only interested in the data (Action Editor's editing)
 * 3) We don't care what data, we just care there is some (so that a collapsed
 *    channel can be kept around). No need to clear channels-flag in order to
 *    keep expander channels with no sub-data out, as those cases should get
 *    dealt with by the recursive detection idiom in place.
 *
 * Implementation NOTE:
 *  YES the _doSubChannels variable is NOT read anywhere. BUT, this is NOT an excuse
 *  to go steamrolling the logic into a single-line expression as from experience,
 *  those are notoriously difficult to read + debug when extending later on. The code
 *  below is purposefully laid out so that each case noted above corresponds clearly to
 *  one case below.
 */
#define BEGIN_ANIMFILTER_SUBCHANNELS(expanded_check) \
  { \
    const eAnimFilter_Flags _filter = filter_mode; \
    short _doSubChannels = 0; \
    if (!(filter_mode & ANIMFILTER_LIST_VISIBLE) || (expanded_check)) { \
      _doSubChannels = 1; \
    } \
    else if (!(filter_mode & ANIMFILTER_LIST_CHANNELS)) { \
      _doSubChannels = 2; \
    } \
    else { \
      filter_mode |= ANIMFILTER_TMP_PEEK; \
    } \
\
    { \
      (void)_doSubChannels; \
    }
/* ... standard sub-channel filtering can go on here now ... */
#define END_ANIMFILTER_SUBCHANNELS \
  filter_mode = _filter; \
  } \
  (void)0

/* ............................... */

/* Test whether AnimData has a usable Action. */
#define ANIMDATA_HAS_ACTION_LEGACY(id) \
  ((id)->adt && (id)->adt->action && (id)->adt->action->wrap().is_action_legacy())

#define ANIMDATA_HAS_ACTION_LAYERED(id) \
  ((id)->adt && (id)->adt->action && (id)->adt->action->wrap().is_action_layered())

/* quick macro to test if AnimData is usable for drivers */
#define ANIMDATA_HAS_DRIVERS(id) ((id)->adt && (id)->adt->drivers.first)

/* quick macro to test if AnimData is usable for NLA */
#define ANIMDATA_HAS_NLA(id) ((id)->adt && (id)->adt->nla_tracks.first)

/**
 * Quick macro to test for all three above usability tests, performing the appropriate provided
 * action for each when the AnimData context is appropriate.
 *
 * Priority order for this goes (most important, to least):
 * AnimData blocks, NLA, Drivers, Keyframes.
 *
 * For this to work correctly,
 * a standard set of data needs to be available within the scope that this
 *
 * Gets called in:
 * - ListBase anim_data;
 * - bDopeSheet *ads;
 * - bAnimListElem *ale;
 * - size_t items;
 *
 * - id: ID block which should have an AnimData pointer following it immediately, to use
 * - adtOk: line or block of code to execute for AnimData-blocks case
 *   (usually #ANIMDATA_ADD_ANIMDATA).
 * - nlaOk: line or block of code to execute for NLA tracks+strips case
 * - driversOk: line or block of code to execute for Drivers case
 * - nlaKeysOk: line or block of code for NLA Strip Keyframes case
 * - legacyActionOk: line or block of code for Keyframes from legacy Actions
 * - layeredActionOk: line or block of code for Keyframes from layered Actions
 *
 * The checks for the various cases are as follows:
 * 0) top level: checks for animdata and also that all the F-Curves for the block will be visible
 * 1) animdata check: for filtering animdata blocks only
 * 2A) nla tracks: include animdata block's data as there are NLA tracks+strips there
 * 2B) actions to convert to nla: include animdata block's data as there is an action that can be
 *     converted to a new NLA strip, and the filtering options allow this
 * 2C) allow non-animated data-blocks to be included so that data-blocks can be added
 * 3) drivers: include drivers from animdata block (for Drivers mode in Graph Editor)
 * 4A) nla strip keyframes: these are the per-strip controls for time and influence
 * 4B) normal keyframes: only when there is an active action
 * 4C) normal keyframes: only when there is an Animation assigned
 */
#define ANIMDATA_FILTER_CASES( \
    id, adtOk, nlaOk, driversOk, nlaKeysOk, legacyActionOk, layeredActionOk) \
  { \
    if ((id)->adt) { \
      if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || \
          !((id)->adt->flag & ADT_CURVES_NOT_VISIBLE)) { \
        if (filter_mode & ANIMFILTER_ANIMDATA) { \
          adtOk \
        } \
        else if (ac->filters.flag & ADS_FILTER_ONLYNLA) { \
          if (ANIMDATA_HAS_NLA(id)) { \
            nlaOk \
          } \
          else if (!(ac->filters.flag & ADS_FILTER_NLA_NOACT) || ANIMDATA_HAS_ACTION_LAYERED(id)) \
          { \
            nlaOk \
          } \
        } \
        else if (ac->filters.flag & ADS_FILTER_ONLYDRIVERS) { \
          if (ANIMDATA_HAS_DRIVERS(id)) { \
            driversOk \
          } \
        } \
        else { \
          if (ANIMDATA_HAS_NLA(id)) { \
            nlaKeysOk \
          } \
          if (ANIMDATA_HAS_ACTION_LAYERED(id)) { \
            layeredActionOk \
          } \
          else if (ANIMDATA_HAS_ACTION_LEGACY(id)) { \
            legacyActionOk \
          } \
        } \
      } \
    } \
  } \
  (void)0

/* ............................... */

/**
 * Add a new animation channel, taking into account the "peek" flag, which is used to just check
 * whether any channels will be added (but without needing them to actually get created).
 *
 * \warning This causes the calling function to return early if we're only "peeking" for channels.
 */
#define ANIMCHANNEL_NEW_CHANNEL_FULL( \
    bmain, channel_data, channel_type, owner_id, fcurve_owner_id, ale_statement) \
  if (filter_mode & ANIMFILTER_TMP_PEEK) { \
    return 1; \
  } \
  { \
    bAnimListElem *ale = make_new_animlistelem( \
        bmain, channel_data, channel_type, (ID *)owner_id, fcurve_owner_id); \
    if (ale) { \
      BLI_addtail(anim_data, ale); \
      items++; \
      ale_statement \
    } \
  } \
  (void)0

#define ANIMCHANNEL_NEW_CHANNEL(bmain, channel_data, channel_type, owner_id, fcurve_owner_id) \
  ANIMCHANNEL_NEW_CHANNEL_FULL(bmain, channel_data, channel_type, owner_id, fcurve_owner_id, {})

/* ............................... */

/* quick macro to test if an anim-channel representing an AnimData block is suitably active */
#define ANIMCHANNEL_ACTIVEOK(ale) \
  (!(filter_mode & ANIMFILTER_ACTIVE) || !(ale->adt) || (ale->adt->flag & ADT_UI_ACTIVE))

/* Quick macro to test if an anim-channel (F-Curve, Group, etc.)
 * is selected in an acceptable way. */
#define ANIMCHANNEL_SELOK(test_func) \
  (!(filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)) || \
   ((filter_mode & ANIMFILTER_SEL) && test_func) || \
   ((filter_mode & ANIMFILTER_UNSEL) && test_func == 0))

/**
 * Quick macro to test if an anim-channel (F-Curve) is selected ok for editing purposes
 * - `*_SELEDIT` means that only selected curves will have visible+editable key-frames.
 *
 * checks here work as follows:
 * 1) SELEDIT off - don't need to consider the implications of this option.
 * 2) FOREDIT off - we're not considering editing, so channel is ok still.
 * 3) test_func (i.e. selection test) - only if selected, this test will pass.
 */
#define ANIMCHANNEL_SELEDITOK(test_func) \
  (!(filter_mode & ANIMFILTER_SELEDIT) || !(filter_mode & ANIMFILTER_FOREDIT) || (test_func))

/* ----------- 'Private' Stuff --------------- */

/**
 * Set `ale` so that it points to the top-most 'summary' channel of the given `adt`.
 * So this is either the Animation or the Action, or empty.
 */
static void key_data_from_adt(bAnimListElem &ale, AnimData *adt)
{
  ale.adt = adt;

  if (!adt || !adt->action) {
    ale.key_data = nullptr;
    ale.datatype = ALE_NONE;
    return;
  }

  blender::animrig::Action &action = adt->action->wrap();
  ale.key_data = &action;
  ale.datatype = action.is_action_layered() ? ALE_ACTION_LAYERED : ALE_ACT;
}

/* this function allocates memory for a new bAnimListElem struct for the
 * provided animation channel-data.
 */
static bAnimListElem *make_new_animlistelem(
    Main *bmain, void *data, const eAnim_ChannelType datatype, ID *owner_id, ID *fcurve_owner_id)
{
  /* Only allocate memory if there is data to convert. */
  if (!data) {
    return nullptr;
  }

  /* Allocate and set generic data. */
  bAnimListElem *ale = MEM_callocN<bAnimListElem>("bAnimListElem");

  ale->data = data;
  ale->type = datatype;

  ale->bmain = bmain;
  ale->id = owner_id;
  ale->adt = BKE_animdata_from_id(owner_id);
  ale->fcurve_owner_id = fcurve_owner_id;

  /* do specifics */
  switch (datatype) {
    case ANIMTYPE_SUMMARY: {
      /* Nothing to include for now... this is just a dummy wrapper around
       * all the other channels in the DopeSheet, and gets included at the start of the list. */
      ale->key_data = nullptr;
      ale->datatype = ALE_ALL;
      break;
    }
    case ANIMTYPE_SCENE: {
      Scene *sce = static_cast<Scene *>(data);

      ale->flag = sce->flag;

      ale->key_data = sce;
      ale->datatype = ALE_SCE;

      ale->adt = BKE_animdata_from_id(static_cast<ID *>(data));
      break;
    }
    case ANIMTYPE_OBJECT: {
      Base *base = static_cast<Base *>(data);
      Object *ob = base->object;

      ale->flag = ob->flag;

      ale->key_data = ob;
      ale->datatype = ALE_OB;

      ale->adt = BKE_animdata_from_id(&ob->id);
      break;
    }
    case ANIMTYPE_FILLACT_LAYERED: {
      bAction *action = static_cast<bAction *>(data);

      ale->flag = action->flag;

      ale->key_data = action;
      ale->datatype = ALE_ACTION_LAYERED;
      break;
    }
    case ANIMTYPE_ACTION_SLOT: {
      animrig::Slot *slot = static_cast<animrig::Slot *>(data);
      ale->flag = slot->slot_flags;

      BLI_assert_msg(GS(fcurve_owner_id->name) == ID_AC, "fcurve_owner_id should be an Action");
      /* ale->data = the slot itself, key_data = the Action. */
      ale->key_data = fcurve_owner_id;
      ale->datatype = ALE_ACTION_SLOT;
      break;
    }
    case ANIMTYPE_FILLACTD: {
      bAction *act = static_cast<bAction *>(data);

      ale->flag = act->flag;

      ale->key_data = act;
      ale->datatype = ALE_ACT;
      break;
    }
    case ANIMTYPE_FILLDRIVERS: {
      AnimData *adt = static_cast<AnimData *>(data);

      ale->flag = adt->flag;

      /* XXX drivers don't show summary for now. */
      ale->key_data = nullptr;
      ale->datatype = ALE_NONE;
      break;
    }
    case ANIMTYPE_DSMAT: {
      Material *ma = static_cast<Material *>(data);
      ale->flag = FILTER_MAT_OBJD(ma);
      key_data_from_adt(*ale, ma->adt);
      break;
    }
    case ANIMTYPE_DSLAM: {
      Light *la = static_cast<Light *>(data);
      ale->flag = FILTER_LAM_OBJD(la);
      key_data_from_adt(*ale, la->adt);
      break;
    }
    case ANIMTYPE_DSCAM: {
      Camera *ca = static_cast<Camera *>(data);
      ale->flag = FILTER_CAM_OBJD(ca);
      key_data_from_adt(*ale, ca->adt);
      break;
    }
    case ANIMTYPE_DSCACHEFILE: {
      CacheFile *cache_file = static_cast<CacheFile *>(data);
      ale->flag = FILTER_CACHEFILE_OBJD(cache_file);
      key_data_from_adt(*ale, cache_file->adt);
      break;
    }
    case ANIMTYPE_DSCUR: {
      Curve *cu = static_cast<Curve *>(data);
      ale->flag = FILTER_CUR_OBJD(cu);
      key_data_from_adt(*ale, cu->adt);
      break;
    }
    case ANIMTYPE_DSARM: {
      bArmature *arm = static_cast<bArmature *>(data);
      ale->flag = FILTER_ARM_OBJD(arm);
      key_data_from_adt(*ale, arm->adt);
      break;
    }
    case ANIMTYPE_DSMESH: {
      Mesh *mesh = static_cast<Mesh *>(data);
      ale->flag = FILTER_MESH_OBJD(mesh);
      key_data_from_adt(*ale, mesh->adt);
      break;
    }
    case ANIMTYPE_DSLAT: {
      Lattice *lt = static_cast<Lattice *>(data);
      ale->flag = FILTER_LATTICE_OBJD(lt);
      key_data_from_adt(*ale, lt->adt);
      break;
    }
    case ANIMTYPE_DSSPK: {
      Speaker *spk = static_cast<Speaker *>(data);
      ale->flag = FILTER_SPK_OBJD(spk);
      key_data_from_adt(*ale, spk->adt);
      break;
    }
    case ANIMTYPE_DSHAIR: {
      Curves *curves = static_cast<Curves *>(data);
      ale->flag = FILTER_CURVES_OBJD(curves);
      key_data_from_adt(*ale, curves->adt);
      break;
    }
    case ANIMTYPE_DSPOINTCLOUD: {
      PointCloud *pointcloud = static_cast<PointCloud *>(data);
      ale->flag = FILTER_POINTS_OBJD(pointcloud);
      key_data_from_adt(*ale, pointcloud->adt);
      break;
    }
    case ANIMTYPE_DSVOLUME: {
      Volume *volume = static_cast<Volume *>(data);
      ale->flag = FILTER_VOLUME_OBJD(volume);
      key_data_from_adt(*ale, volume->adt);
      break;
    }
    case ANIMTYPE_DSLIGHTPROBE: {
      LightProbe *probe = static_cast<LightProbe *>(data);
      ale->flag = FILTER_LIGHTPROBE_OBJD(probe);
      key_data_from_adt(*ale, probe->adt);
      break;
    }
    case ANIMTYPE_DSSKEY: {
      Key *key = static_cast<Key *>(data);
      ale->flag = FILTER_SKE_OBJD(key);
      key_data_from_adt(*ale, key->adt);
      break;
    }
    case ANIMTYPE_DSWOR: {
      World *wo = static_cast<World *>(data);
      ale->flag = FILTER_WOR_SCED(wo);
      key_data_from_adt(*ale, wo->adt);
      break;
    }
    case ANIMTYPE_DSNTREE: {
      bNodeTree *ntree = static_cast<bNodeTree *>(data);
      ale->flag = FILTER_NTREE_DATA(ntree);
      key_data_from_adt(*ale, ntree->adt);
      break;
    }
    case ANIMTYPE_DSLINESTYLE: {
      FreestyleLineStyle *linestyle = static_cast<FreestyleLineStyle *>(data);
      ale->flag = FILTER_LS_SCED(linestyle);
      key_data_from_adt(*ale, linestyle->adt);
      break;
    }
    case ANIMTYPE_DSPART: {
      ParticleSettings *part = static_cast<ParticleSettings *>(ale->data);
      ale->flag = FILTER_PART_OBJD(part);
      key_data_from_adt(*ale, part->adt);
      break;
    }
    case ANIMTYPE_DSTEX: {
      Tex *tex = static_cast<Tex *>(data);
      ale->flag = FILTER_TEX_DATA(tex);
      key_data_from_adt(*ale, tex->adt);
      break;
    }
    case ANIMTYPE_DSGPENCIL: {
      bGPdata *gpd = static_cast<bGPdata *>(data);
      /* NOTE: we just reuse the same expand filter for this case */
      ale->flag = EXPANDED_GPD(gpd);

      /* XXX: currently, this is only used for access to its animation data */
      key_data_from_adt(*ale, gpd->adt);
      break;
    }
    case ANIMTYPE_DSMCLIP: {
      MovieClip *clip = static_cast<MovieClip *>(data);
      ale->flag = EXPANDED_MCLIP(clip);
      key_data_from_adt(*ale, clip->adt);
      break;
    }
    case ANIMTYPE_NLACONTROLS: {
      AnimData *adt = static_cast<AnimData *>(data);

      ale->flag = adt->flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_NONE;
      break;
    }
    case ANIMTYPE_GROUP: {
      BLI_assert_msg(GS(fcurve_owner_id->name) == ID_AC, "fcurve_owner_id should be an Action");

      bActionGroup *agrp = static_cast<bActionGroup *>(data);

      ale->flag = agrp->flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_GROUP;
      break;
    }
    case ANIMTYPE_FCURVE:
    case ANIMTYPE_NLACURVE: /* practically the same as ANIMTYPE_FCURVE.
                             * Differences are applied post-creation */
    {
      FCurve *fcu = static_cast<FCurve *>(data);

      ale->flag = fcu->flag;

      ale->key_data = fcu;
      ale->datatype = ALE_FCURVE;
      break;
    }
    case ANIMTYPE_SHAPEKEY: {
      KeyBlock *kb = static_cast<KeyBlock *>(data);
      Key *key = reinterpret_cast<Key *>(ale->id);

      ale->flag = kb->flag;

      /* whether we have keyframes depends on whether there is a Key block to find it from */
      if (key) {
        /* index of shapekey is defined by place in key's list */
        ale->index = BLI_findindex(&key->block, kb);

        /* the corresponding keyframes are from the animdata */
        if (ale->adt && ale->adt->action) {
          /* Try to find the F-Curve which corresponds to this exactly. */
          if (std::optional<std::string> rna_path = BKE_keyblock_curval_rnapath_get(key, kb)) {
            ale->key_data = (void *)blender::animrig::fcurve_find_in_assigned_slot(*ale->adt,
                                                                                   {*rna_path, 0});
          }
        }
        ale->datatype = (ale->key_data) ? ALE_FCURVE : ALE_NONE;
      }
      break;
    }
    case ANIMTYPE_GPLAYER: {
      bGPDlayer *gpl = static_cast<bGPDlayer *>(data);

      ale->flag = gpl->flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_GPFRAME;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_LAYER: {
      GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(data);

      ale->flag = layer->base.flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_GREASE_PENCIL_CEL;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP: {
      GreasePencilLayerTreeGroup *layer_group = static_cast<GreasePencilLayerTreeGroup *>(data);

      ale->flag = layer_group->base.flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_GREASE_PENCIL_GROUP;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_DATABLOCK: {
      GreasePencil *grease_pencil = static_cast<GreasePencil *>(data);

      ale->flag = grease_pencil->flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_GREASE_PENCIL_DATA;
      break;
    }
    case ANIMTYPE_MASKLAYER: {
      MaskLayer *masklay = static_cast<MaskLayer *>(data);

      ale->flag = masklay->flag;

      ale->key_data = nullptr;
      ale->datatype = ALE_MASKLAY;
      break;
    }
    case ANIMTYPE_NLATRACK: {
      NlaTrack *nlt = static_cast<NlaTrack *>(data);

      ale->flag = nlt->flag;

      ale->key_data = &nlt->strips;
      ale->datatype = ALE_NLASTRIP;
      break;
    }
    case ANIMTYPE_NLAACTION: {
      /* nothing to include for now... nothing editable from NLA-perspective here */
      ale->key_data = nullptr;
      ale->datatype = ALE_NONE;
      break;
    }

    case ANIMTYPE_NONE:
    case ANIMTYPE_ANIMDATA:
    case ANIMTYPE_SPECIALDATA__UNUSED:
    case ANIMTYPE_DSMBALL:
    case ANIMTYPE_MASKDATABLOCK:
    case ANIMTYPE_PALETTE:
    case ANIMTYPE_NUM_TYPES:
      break;
  }

  return ale;
}

/* ----------------------------------------- */

/* 'Only Selected' selected data and/or 'Include Hidden' filtering
 * NOTE: when this function returns true, the F-Curve is to be skipped
 */
static bool skip_fcurve_selected_data(bAnimContext *ac,
                                      FCurve *fcu,
                                      ID *owner_id,
                                      const eAnimFilter_Flags filter_mode)
{
  if (fcu->grp != nullptr && fcu->grp->flag & ADT_CURVES_ALWAYS_VISIBLE) {
    return false;
  }
  /* hidden items should be skipped if we only care about visible data,
   * but we aren't interested in hidden stuff */
  const bool skip_hidden = (filter_mode & ANIMFILTER_DATA_VISIBLE) &&
                           !(ac->filters.flag & ADS_FILTER_INCL_HIDDEN);

  if (GS(owner_id->name) == ID_OB) {
    Object *ob = reinterpret_cast<Object *>(owner_id);
    bPoseChannel *pchan = nullptr;
    char bone_name[sizeof(pchan->name)];

    /* Only consider if F-Curve involves `pose.bones`. */
    if (fcu->rna_path &&
        BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name)))
    {
      /* Get bone-name, and check if this bone is selected. */
      pchan = BKE_pose_channel_find_name(ob->pose, bone_name);

      /* check whether to continue or skip */
      if (pchan && pchan->bone) {
        /* If only visible channels,
         * skip if bone not visible unless user wants channels from hidden data too. */
        if (skip_hidden) {
          bArmature *arm = static_cast<bArmature *>(ob->data);

          /* Skipping - is currently hidden. */
          if (!blender::animrig::bone_is_visible(arm, pchan)) {
            return true;
          }
        }

        /* can only add this F-Curve if it is selected */
        if (ac->filters.flag & ADS_FILTER_ONLYSEL) {
          if ((pchan->flag & POSE_SELECTED) == 0) {
            return true;
          }
        }
      }
    }
  }
  else if (GS(owner_id->name) == ID_SCE) {
    Scene *scene = reinterpret_cast<Scene *>(owner_id);
    Strip *strip = nullptr;
    char strip_name[sizeof(strip->name)];

    /* Only consider if F-Curve involves `sequence_editor.strips`. */
    if (fcu->rna_path &&
        BLI_str_quoted_substr(fcu->rna_path, "strips_all[", strip_name, sizeof(strip_name)))
    {
      /* Get strip name, and check if this strip is selected. */
      Editing *ed = blender::seq::editing_get(scene);
      if (ed) {
        strip = blender::seq::get_strip_by_name(ed->current_strips(), strip_name, false);
      }

      /* Can only add this F-Curve if it is selected. */
      if (ac->filters.flag & ADS_FILTER_ONLYSEL) {

        /* NOTE(@ideasman42): The `strip == nullptr` check doesn't look right
         * (compared to other checks in this function which skip data that can't be found).
         *
         * This is done since the search for sequence strips doesn't use a global lookup:
         * - Nested meta-strips are excluded.
         * - When inside a meta-strip - strips outside the meta-strip excluded.
         *
         * Instead, only the strips directly visible to the user are considered for selection.
         * The nullptr check here means everything else is considered unselected and is not shown.
         *
         * There is a subtle difference between nodes, pose-bones ... etc
         * since data-paths that point to missing strips are not shown.
         * If this is an important difference, the nullptr case could perform a global lookup,
         * only returning `true` if the sequence strip exists elsewhere
         * (ignoring its selection state). */
        if (strip == nullptr) {
          return true;
        }

        if ((strip->flag & SELECT) == 0) {
          return true;
        }
      }
    }
  }
  else if (GS(owner_id->name) == ID_NT) {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(owner_id);
    bNode *node = nullptr;
    char node_name[sizeof(node->name)];

    /* Check for selected nodes. */
    if (fcu->rna_path &&
        BLI_str_quoted_substr(fcu->rna_path, "nodes[", node_name, sizeof(node_name)))
    {
      /* Get strip name, and check if this strip is selected. */
      node = bke::node_find_node_by_name(*ntree, node_name);

      /* Can only add this F-Curve if it is selected. */
      if (node) {
        if (ac->filters.flag & ADS_FILTER_ONLYSEL) {
          if ((node->flag & NODE_SELECT) == 0) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

/* Helper for name-based filtering - Perform "partial/fuzzy matches" (as in 80a7efd) */
static bool name_matches_dopesheet_filter(const bDopeSheet *ads, const char *name)
{
  if (ads->flag & ADS_FLAG_FUZZY_NAMES) {
    /* full fuzzy, multi-word, case insensitive matches */
    const size_t str_len = strlen(ads->searchstr);
    const int words_max = BLI_string_max_possible_word_count(str_len);

    int (*words)[2] = static_cast<int (*)[2]>(BLI_array_alloca(words, words_max));
    const int words_len = BLI_string_find_split_words(
        ads->searchstr, str_len, ' ', words, words_max);
    bool found = false;

    /* match name against all search words */
    for (int index = 0; index < words_len; index++) {
      if (BLI_strncasestr(name, ads->searchstr + words[index][0], words[index][1])) {
        found = true;
        break;
      }
    }

    /* if we have a match somewhere, this returns true */
    return ((ads->flag & ADS_FLAG_INVERT_FILTER) == 0) ? found : !found;
  }
  /* fallback/default - just case insensitive, but starts from start of word */
  bool found = BLI_strcasestr(name, ads->searchstr) != nullptr;
  return ((ads->flag & ADS_FLAG_INVERT_FILTER) == 0) ? found : !found;
}

/* (Display-)Name-based F-Curve filtering
 * NOTE: when this function returns true, the F-Curve is to be skipped
 */
static bool skip_fcurve_with_name(
    bAnimContext *ac, FCurve *fcu, eAnim_ChannelType channel_type, void *owner, ID *owner_id)
{
  bAnimListElem ale_dummy = {nullptr};
  const bAnimChannelType *acf;

  /* create a dummy wrapper for the F-Curve, so we can get typeinfo for it */
  ale_dummy.type = channel_type;
  ale_dummy.owner = owner;
  ale_dummy.id = owner_id;
  ale_dummy.data = fcu;

  /* get type info for channel */
  acf = ANIM_channel_get_typeinfo(&ale_dummy);
  if (acf && acf->name) {
    char name[ANIM_CHAN_NAME_SIZE];

    /* get name */
    acf->name(&ale_dummy, name);

    /* check for partial match with the match string, assuming case insensitive filtering
     * if match, this channel shouldn't be ignored!
     */
    return !name_matches_dopesheet_filter(ac->ads, name);
  }

  /* just let this go... */
  return true;
}

static bool ale_name_matches_dopesheet_filter(const bDopeSheet &ads, bAnimListElem &ale)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(&ale);
  if (!acf) {
    BLI_assert_unreachable();
    /* Do not filter out stuff unless we know it can be filtered out. */
    return true;
  }

  char name[ANIM_CHAN_NAME_SIZE];
  acf->name(&ale, name);
  return name_matches_dopesheet_filter(&ads, name);
}

/**
 * Check if F-Curve has errors and/or is disabled
 *
 * \return true if F-Curve has errors/is disabled
 */
static bool fcurve_has_errors(bAnimContext *ac, const FCurve *fcu)
{
  /* F-Curve disabled (path evaluation error). */
  if (fcu->flag & FCURVE_DISABLED) {
    return true;
  }

  /* driver? */
  if (fcu->driver) {
    const ChannelDriver *driver = fcu->driver;

    /* error flag on driver usually means that there is an error
     * BUT this may not hold with PyDrivers as this flag gets cleared
     *     if no critical errors prevent the driver from working...
     */
    if (driver->flag & DRIVER_FLAG_INVALID) {
      return true;
    }

    /* check variables for other things that need linting... */
    /* TODO: maybe it would be more efficient just to have a quick flag for this? */
    LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        if (dtar->flag & DTAR_FLAG_INVALID) {
          return true;
        }

        if ((dtar->flag & DTAR_FLAG_FALLBACK_USED) &&
            (ac->filters.flag2 & ADS_FILTER_DRIVER_FALLBACK_AS_ERROR))
        {
          return true;
        }
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  /* no errors found */
  return false;
}

/* find the next F-Curve that is usable for inclusion */
static FCurve *animfilter_fcurve_next(bAnimContext *ac,
                                      FCurve *first,
                                      eAnim_ChannelType channel_type,
                                      const eAnimFilter_Flags filter_mode,
                                      void *owner,
                                      ID *owner_id)
{
  bActionGroup *grp = (channel_type == ANIMTYPE_FCURVE) ? static_cast<bActionGroup *>(owner) :
                                                          nullptr;
  FCurve *fcu = nullptr;

  /* Loop over F-Curves - assume that the caller of this has already checked
   * that these should be included.
   * NOTE: we need to check if the F-Curves belong to the same group,
   * as this gets called for groups too...
   */
  for (fcu = first; ((fcu) && (fcu->grp == grp)); fcu = fcu->next) {
    /* special exception for Pose-Channel/Sequence-Strip/Node Based F-Curves:
     * - The 'Only Selected' and 'Include Hidden' data filters should be applied to sub-ID data
     *   which can be independently selected/hidden, such as Pose-Channels, Sequence Strips,
     *   and Nodes. Since these checks were traditionally done as first check for objects,
     *   we do the same here.
     * - We currently use an 'approximate' method for getting these F-Curves that doesn't require
     *   carefully checking the entire path.
     * - This will also affect things like Drivers, and also works for Bone Constraints.
     */
    if (ac->ads && owner_id) {
      if ((filter_mode & ANIMFILTER_TMP_IGNORE_ONLYSEL) == 0) {
        if ((ac->filters.flag & ADS_FILTER_ONLYSEL) ||
            (ac->filters.flag & ADS_FILTER_INCL_HIDDEN) == 0)
        {
          if (skip_fcurve_selected_data(ac, fcu, owner_id, filter_mode)) {
            continue;
          }
        }
      }
    }

    /* only include if visible (Graph Editor check, not channels check) */
    if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || (fcu->flag & FCURVE_VISIBLE)) {
      /* only work with this channel and its subchannels if it is editable */
      if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_FCU(fcu)) {
        /* Only include this curve if selected in a way consistent
         * with the filtering requirements. */
        if (ANIMCHANNEL_SELOK(SEL_FCU(fcu)) && ANIMCHANNEL_SELEDITOK(SEL_FCU(fcu))) {
          /* only include if this curve is active */
          if (!(filter_mode & ANIMFILTER_ACTIVE) || (fcu->flag & FCURVE_ACTIVE)) {
            /* name based filtering... */
            if (((ac->ads) && (ac->ads->searchstr[0] != '\0')) && (owner_id)) {
              if (skip_fcurve_with_name(ac, fcu, channel_type, owner, owner_id)) {
                continue;
              }
            }

            /* error-based filtering... */
            if ((ac->ads) && (ac->filters.flag & ADS_FILTER_ONLY_ERRORS)) {
              /* skip if no errors... */
              if (!fcurve_has_errors(ac, fcu)) {
                continue;
              }
            }

            /* this F-Curve can be used, so return it */
            return fcu;
          }
        }
      }
    }
  }

  /* no (more) F-Curves from the list are suitable... */
  return nullptr;
}

static size_t animfilter_fcurves(bAnimContext *ac,
                                 ListBase *anim_data,
                                 FCurve *first,
                                 eAnim_ChannelType fcurve_type,
                                 const eAnimFilter_Flags filter_mode,
                                 void *owner,
                                 ID *owner_id,
                                 ID *fcurve_owner_id)
{
  FCurve *fcu;
  size_t items = 0;

  /* Loop over every F-Curve able to be included.
   *
   * This for-loop works like this:
   * 1) The starting F-Curve is assigned to the fcu pointer
   *    so that we have a starting point to search from.
   * 2) The first valid F-Curve to start from (which may include the one given as 'first')
   *    in the remaining list of F-Curves is found, and verified to be non-null.
   * 3) The F-Curve referenced by fcu pointer is added to the list
   * 4) The fcu pointer is set to the F-Curve after the one we just added,
   *    so that we can keep going through the rest of the F-Curve list without an eternal loop.
   *    Back to step 2 :)
   */
  for (fcu = first;
       (fcu = animfilter_fcurve_next(ac, fcu, fcurve_type, filter_mode, owner, owner_id));
       fcu = fcu->next)
  {
    if (UNLIKELY(fcurve_type == ANIMTYPE_NLACURVE)) {
      /* NLA Control Curve - Basically the same as normal F-Curves,
       * except we need to set some stuff differently */
      ANIMCHANNEL_NEW_CHANNEL_FULL(ac->bmain, fcu, ANIMTYPE_NLACURVE, owner_id, fcurve_owner_id, {
        ale->owner = owner; /* strip */
        /* Since #130440 landed, this should now in theory be something like
         * `ale->adt = BKE_animdata_from_id(owner_id)`, rather than a nullptr.
         * However, at the moment the nullptr doesn't hurt, and it helps us
         * catch bugs like #147803 via the assert in `fcurve_to_keylist()`. If
         * the nullptr does start to hurt at some point, please change it! */
        ale->adt = nullptr;
      });
    }
    else {
      /* Normal FCurve */
      ANIMCHANNEL_NEW_CHANNEL(ac->bmain, fcu, ANIMTYPE_FCURVE, owner_id, fcurve_owner_id);
    }
  }

  /* return the number of items added to the list */
  return items;
}

static inline bool fcurve_span_selection_matters(const eAnimFilter_Flags filter_mode)
{
  /* This means that ANIMFILTER_SELEDIT only works if ANIMFILTER_FOREDIT is also set. Given the
   * description on ANIMFILTER_SELEDIT this seems reasonable. */
  if ((filter_mode & ANIMFILTER_FOREDIT) && (filter_mode & ANIMFILTER_SELEDIT)) {
    return true;
  }
  return filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL);
}

static inline bool fcurve_span_must_be_selected(const eAnimFilter_Flags filter_mode)
{
  if ((filter_mode & ANIMFILTER_FOREDIT) && (filter_mode & ANIMFILTER_SELEDIT)) {
    return true;
  }
  return filter_mode & ANIMFILTER_SEL;
}

/**
 * Add `bAnimListElem`s to `anim_data` for each F-Curve in `fcurves`.
 *
 * \param slot_handle: The slot handle that these F-Curves animate. This is
 *    used later to look up the ID* of the user of the slot, which in turn is
 *    used to construct a suitable F-Curve label for in the channels list.
 *
 * \param owner_id: The ID whose 'animdata->action' pointer was followed to get to
 *    these F-Curves. This ID may be animated by a different slot than referenced by
 *    `slot_handle`, so do _not_ treat this as "the ID animated by these F-Curves".
 *
 * \param fcurve_owner_id: The ID that holds these F-Curves.
 *    Typically an Action, but can be any ID,
 *    for example in the case of drivers.
 */
static size_t animfilter_fcurves_span(bAnimContext *ac,
                                      ListBase * /*bAnimListElem*/ anim_data,
                                      Span<FCurve *> fcurves,
                                      const animrig::slot_handle_t slot_handle,
                                      const eAnimFilter_Flags filter_mode,
                                      ID *animated_id,
                                      ID *fcurve_owner_id)
{
  size_t num_items = 0;
  BLI_assert(animated_id);

  const bool active_matters = filter_mode & ANIMFILTER_ACTIVE;
  const bool selection_matters = fcurve_span_selection_matters(filter_mode);
  const bool must_be_selected = fcurve_span_must_be_selected(filter_mode);
  const bool visibility_matters = filter_mode & ANIMFILTER_CURVE_VISIBLE;
  const bool editability_matters = filter_mode & ANIMFILTER_FOREDIT;
  const bool show_only_errors = ac->ads && (ac->filters.flag & ADS_FILTER_ONLY_ERRORS);
  const bool filter_by_name = ac->ads && (ac->ads->searchstr[0] != '\0');

  for (FCurve *fcu : fcurves) {
    /* make_new_animlistelem will return nullptr when fcu == nullptr, and that's
     * going to cause problems. */
    BLI_assert(fcu);

    if (editability_matters && (fcu->flag & FCURVE_PROTECTED)) {
      continue;
    }

    if (selection_matters && bool(fcu->flag & FCURVE_SELECTED) != must_be_selected) {
      continue;
    }
    if (active_matters && !(fcu->flag & FCURVE_ACTIVE)) {
      continue;
    }
    if (visibility_matters && !(fcu->flag & FCURVE_VISIBLE)) {
      continue;
    }
    if (show_only_errors && !fcurve_has_errors(ac, fcu)) {
      continue;
    }
    if (skip_fcurve_selected_data(ac, fcu, animated_id, filter_mode)) {
      continue;
    }

    bAnimListElem *ale = make_new_animlistelem(
        ac->bmain, fcu, ANIMTYPE_FCURVE, animated_id, fcurve_owner_id);

    /* Filtering by name needs a way to look up the name, which is easiest if
     * there is already an #bAnimListElem. */
    if (filter_by_name && !ale_name_matches_dopesheet_filter(*ac->ads, *ale)) {
      MEM_freeN(ale);
      continue;
    }

    if (filter_mode & ANIMFILTER_TMP_PEEK) {
      /* Found an animation channel, which is good enough for the 'TMP_PEEK' mode. */
      MEM_freeN(ale);
      return 1;
    }

    /* bAnimListElem::slot_handle is exposed as int32_t and not as slot_handle_t, so better
     * ensure that these are still equivalent.
     * TODO: move to another part of the code. */
    static_assert(
        std::is_same_v<decltype(ActionSlot::handle), decltype(bAnimListElem::slot_handle)>);

    /* Note that this might not be the same as ale->adt->slot_handle. The reason this F-Curve is
     * shown could be because it's in the Action editor, showing ale->adt->action with _all_
     * slots, and this F-Curve could be from a different slot than what's used by the owner
     * of `ale->adt`. */
    ale->slot_handle = slot_handle;

    BLI_addtail(anim_data, ale);
    num_items++;
  }

  return num_items;
}

/**
 * Filters a channel group and its children.
 *
 * This works both for channel groups in legacy and in layered actions.
 *
 * Note: `slot_handle` is only used for layered actions, and is ignored for
 * legacy actions.
 */
static size_t animfilter_act_group(bAnimContext *ac,
                                   ListBase *anim_data,
                                   bAction *act,
                                   animrig::slot_handle_t slot_handle,
                                   bActionGroup *agrp,
                                   eAnimFilter_Flags filter_mode,
                                   ID *owner_id)
{
  BLI_assert(act != nullptr);
  BLI_assert(agrp != nullptr);

  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  animrig::Action &action = act->wrap();

  /* if we care about the selection status of the channels,
   * but the group isn't expanded (1)...
   * (1) this only matters if we actually care about the hierarchy though.
   *     - Hierarchy matters: this hack should be applied
   *     - Hierarchy ignored: cases like #21276 won't work properly, unless we skip this hack
   */
  if (
      /* Care about hierarchy but group isn't expanded. */
      ((filter_mode & ANIMFILTER_LIST_VISIBLE) && EXPANDED_AGRP(ac, agrp) == 0) &&
      /* Care about selection status. */
      (filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)))
  {
    /* If the group itself isn't selected appropriately,
     * we shouldn't consider its children either. */
    if (ANIMCHANNEL_SELOK(SEL_AGRP(agrp)) == 0) {
      return 0;
    }

    /* if we're still here,
     * then the selection status of the curves within this group should not matter,
     * since this creates too much overhead for animators (i.e. making a slow workflow).
     *
     * Tools affected by this at time of coding (2010 Feb 09):
     * - Inserting keyframes on selected channels only.
     * - Pasting keyframes.
     * - Creating ghost curves in Graph Editor.
     */
    filter_mode &= ~(ANIMFILTER_SEL | ANIMFILTER_UNSEL | ANIMFILTER_LIST_VISIBLE);
  }

  /* add grouped F-Curves */
  BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_AGRP(ac, agrp)) {
    /* special filter so that we can get just the F-Curves within the active group */
    if (!(filter_mode & ANIMFILTER_ACTGROUPED) || (agrp->flag & AGRP_ACTIVE)) {
      /* for the Graph Editor, curves may be set to not be visible in the view to lessen
       * clutter, but to do this, we need to check that the group doesn't have its
       * not-visible flag set preventing all its sub-curves to be shown
       */
      if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || !(agrp->flag & AGRP_NOTVISIBLE)) {
        /* group must be editable for its children to be editable (if we care about this) */
        if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_AGRP(agrp)) {
          /* Filter the fcurves in this group, adding them to the temporary
           * filter list. */
          if (action.is_action_legacy()) {
            /* get first F-Curve which can be used here */
            FCurve *first_fcu = animfilter_fcurve_next(ac,
                                                       static_cast<FCurve *>(agrp->channels.first),
                                                       ANIMTYPE_FCURVE,
                                                       filter_mode,
                                                       agrp,
                                                       owner_id);

            /* filter list, starting from this F-Curve */
            tmp_items += animfilter_fcurves(
                ac, &tmp_data, first_fcu, ANIMTYPE_FCURVE, filter_mode, agrp, owner_id, &act->id);
          }
          else {
            BLI_assert(agrp->channelbag != nullptr);
            Span<FCurve *> fcurves = agrp->wrap().fcurves();
            tmp_items += animfilter_fcurves_span(
                ac, &tmp_data, fcurves, slot_handle, filter_mode, owner_id, &act->id);
          }
        }
      }
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* add this group as a channel first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* filter selection of channel specially here again,
       * since may be open and not subject to previous test */
      if (ANIMCHANNEL_SELOK(SEL_AGRP(agrp))) {
        if (action.is_action_legacy()) {
          ANIMCHANNEL_NEW_CHANNEL(ac->bmain, agrp, ANIMTYPE_GROUP, owner_id, &act->id);
        }
        else {
          ANIMCHANNEL_NEW_CHANNEL_FULL(ac->bmain, agrp, ANIMTYPE_GROUP, owner_id, &act->id, {
            ale->slot_handle = slot_handle;
          });
        }
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

size_t ANIM_animfilter_action_slot(bAnimContext *ac,
                                   ListBase * /* bAnimListElem */ anim_data,
                                   animrig::Action &action,
                                   animrig::Slot &slot,
                                   const eAnimFilter_Flags filter_mode,
                                   ID *animated_id)
{
  BLI_assert(ac);

  /* In some cases (see `ob_to_keylist()` and friends) fake bDopeSheet and fake bAnimContext are
   * created. These are mostly null-initialized, and so do not have a bmain. This means that
   * lookup of the animated ID is not possible, which can result in failure to look up the proper
   * F-Curve display name. For the `..._to_keylist` functions that doesn't matter, as those are
   * only interested in the key data anyway. So rather than trying to get a reliable `bmain`
   * through the maze, this code just treats it as optional (even though ideally it should always
   * be known). */
  ID *slot_user_id = nullptr;
  if (ac->bmain) {
    slot_user_id = animrig::action_slot_get_id_best_guess(*ac->bmain, slot, animated_id);
  }
  if (!slot_user_id) {
    BLI_assert(animated_id);
    /* At the time of writing this (PR #134922), downstream code (see e.g.
     * `animfilter_fcurves_span()`) assumes this is non-null, so we need to set
     * it to *something*. If it's not an actual user of the slot then channels
     * might not resolve to an actual property and thus be displayed oddly in
     * the channel list, but that's not technically a problem, it's just a
     * little strange for the end user. */
    slot_user_id = animated_id;
  }

  /* Don't include anything from this animation if it is linked in from another
   * file, and we're getting stuff for editing... */
  if ((filter_mode & ANIMFILTER_FOREDIT) &&
      (ID_IS_LINKED(&action) || ID_IS_OVERRIDE_LIBRARY(&action)))
  {
    return 0;
  }

  const bool selection_matters = filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL);
  const bool must_be_selected = filter_mode & ANIMFILTER_SEL;
  const bool selection_ok_for_slot = !selection_matters || slot.is_selected() == must_be_selected;

  int items = 0;

  /* Add a list element for the Slot itself, but only if in Action mode. The Dopesheet mode
   * shouldn't display Slots, as F-Curves are always shown in the context of the animated ID
   * anyway. */
  const bool is_action_mode = (ac->spacetype == SPACE_ACTION &&
                               ac->dopesheet_mode == SACTCONT_ACTION);
  const bool show_active_group_only = filter_mode & ANIMFILTER_ACTGROUPED;
  const bool include_summary_channels = (filter_mode & ANIMFILTER_LIST_CHANNELS);
  const bool show_slot_channel = (is_action_mode && selection_ok_for_slot &&
                                  include_summary_channels);
  if (show_slot_channel) {
    ANIMCHANNEL_NEW_CHANNEL(ac->bmain, &slot, ANIMTYPE_ACTION_SLOT, slot_user_id, &action.id);
    items++;
  }

  /* If the 'list visible' flag is used, the expansion state of the Slot
   * matters. Otherwise the sub-channels can always be listed. */
  const bool visible_only = (filter_mode & ANIMFILTER_LIST_VISIBLE);
  const bool expansion_is_ok = !visible_only || !show_slot_channel || slot.is_expanded();

  animrig::Channelbag *channelbag = animrig::channelbag_for_action_slot(action, slot.handle);
  if (channelbag == nullptr) {
    return items;
  }

  if (!expansion_is_ok) {
    return items;
  }

  /* Add channel groups and their member channels. */
  for (bActionGroup *group : channelbag->channel_groups()) {
    items += animfilter_act_group(
        ac, anim_data, &action, slot.handle, group, filter_mode, slot_user_id);
  }

  /* Add ungrouped channels. */
  if (!show_active_group_only) {
    int first_ungrouped_fcurve_index = 0;
    if (!channelbag->channel_groups().is_empty()) {
      const bActionGroup *last_group = channelbag->channel_groups().last();
      first_ungrouped_fcurve_index = last_group->fcurve_range_start +
                                     last_group->fcurve_range_length;
    }

    Span<FCurve *> fcurves = channelbag->fcurves().drop_front(first_ungrouped_fcurve_index);
    items += animfilter_fcurves_span(
        ac, anim_data, fcurves, slot.handle, filter_mode, slot_user_id, &action.id);
  }

  return items;
}

static size_t animfilter_action_slots(bAnimContext *ac,
                                      ListBase *anim_data,
                                      animrig::Action &action,
                                      const eAnimFilter_Flags filter_mode,
                                      ID *owner_id)
{
  /* Don't include anything from this animation if it is linked in from another
   * file, and we're getting stuff for editing... */
  if ((filter_mode & ANIMFILTER_FOREDIT) &&
      (ID_IS_LINKED(&action) || ID_IS_OVERRIDE_LIBRARY(&action)))
  {
    return 0;
  }

  int num_items = 0;
  for (animrig::Slot *slot : action.slots()) {
    BLI_assert(slot);

    num_items += ANIM_animfilter_action_slot(ac, anim_data, action, *slot, filter_mode, owner_id);
  }

  return num_items;
}

static size_t animfilter_action(bAnimContext *ac,
                                ListBase *anim_data,
                                animrig::Action &action,
                                const animrig::slot_handle_t slot_handle,
                                const eAnimFilter_Flags filter_mode,
                                ID *owner_id)
{
  FCurve *lastchan = nullptr;
  size_t items = 0;

  if (action.is_empty()) {
    return 0;
  }

  /* don't include anything from this action if it is linked in from another file,
   * and we're getting stuff for editing...
   */
  if ((filter_mode & ANIMFILTER_FOREDIT) &&
      (!ID_IS_EDITABLE(&action) || ID_IS_OVERRIDE_LIBRARY(&action)))
  {
    return 0;
  }

  if (action.is_action_legacy()) {
    LISTBASE_FOREACH (bActionGroup *, agrp, &action.groups) {
      /* Store reference to last channel of group. */
      if (agrp->channels.last) {
        lastchan = static_cast<FCurve *>(agrp->channels.last);
      }

      items += animfilter_act_group(
          ac, anim_data, &action, animrig::Slot::unassigned, agrp, filter_mode, owner_id);
    }

    /* Un-grouped F-Curves (only if we're not only considering those channels in
     * the active group) */
    if (!(filter_mode & ANIMFILTER_ACTGROUPED)) {
      FCurve *firstfcu = (lastchan) ? (lastchan->next) :
                                      static_cast<FCurve *>(action.curves.first);
      items += animfilter_fcurves(
          ac, anim_data, firstfcu, ANIMTYPE_FCURVE, filter_mode, nullptr, owner_id, &action.id);
    }

    return items;
  }

  /* For now we don't show layers anywhere, just the contained F-Curves. */

  /* Only show all Slots in Action editor mode. Otherwise the F-Curves ought to be displayed
   * underneath their animated ID anyway. */
  const bool is_action_mode = (ac->spacetype == SPACE_ACTION &&
                               ac->dopesheet_mode == SACTCONT_ACTION);
  const bool show_active_only = (ac->filters.flag & ADS_FILTER_ONLY_SLOTS_OF_ACTIVE);
  if (is_action_mode && !show_active_only) {
    return animfilter_action_slots(ac, anim_data, action, filter_mode, owner_id);
  }

  animrig::Slot *slot = action.slot_for_handle(slot_handle);
  if (!slot) {
    /* Can happen when an Action is assigned, but not a Slot. */
    return 0;
  }
  return ANIM_animfilter_action_slot(ac, anim_data, action, *slot, filter_mode, owner_id);
}

/* Include NLA-Data for NLA-Editor:
 * - When ANIMFILTER_LIST_CHANNELS is used, that means we should be filtering the list for display
 *   Although the evaluation order is from the first track to the last and then apply the
 *   Action on top, we present this in the UI as the Active Action followed by the last track
 *   to the first so that we get the evaluation order presented as per a stack.
 * - For normal filtering (i.e. for editing),
 *   we only need the NLA-tracks but they can be in 'normal' evaluation order, i.e. first to last.
 *   Otherwise, some tools may get screwed up.
 */
static size_t animfilter_nla(bAnimContext *ac,
                             ListBase *anim_data,
                             AnimData *adt,
                             const eAnimFilter_Flags filter_mode,
                             ID *owner_id)
{
  NlaTrack *nlt;
  NlaTrack *first = nullptr, *next = nullptr;
  size_t items = 0;

  /* if showing channels, include active action */
  if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
    /* if NLA action-line filtering is off, don't show unless there are keyframes,
     * in order to keep things more compact for doing transforms
     */
    if (!(ac->filters.flag & ADS_FILTER_NLA_NOACT) || (adt->action)) {
      /* there isn't really anything editable here, so skip if need editable */
      if ((filter_mode & ANIMFILTER_FOREDIT) == 0) {
        /* Just add the action track now (this MUST appear for drawing):
         * - As AnimData may not have an action,
         *   we pass a dummy pointer just to get the list elem created,
         *   then overwrite this with the real value - REVIEW THIS.
         */
        ANIMCHANNEL_NEW_CHANNEL_FULL(
            ac->bmain, (void *)(&adt->action), ANIMTYPE_NLAACTION, owner_id, nullptr, {
              ale->data = adt->action ? adt->action : nullptr;
            });
      }
    }

    /* first track to include will be the last one if we're filtering by channels */
    first = static_cast<NlaTrack *>(adt->nla_tracks.last);
  }
  else {
    /* first track to include will the first one (as per normal) */
    first = static_cast<NlaTrack *>(adt->nla_tracks.first);
  }

  /* loop over NLA Tracks -
   * assume that the caller of this has already checked that these should be included */
  for (nlt = first; nlt; nlt = next) {
    /* 'next' NLA-Track to use depends on whether we're filtering for drawing or not */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      next = nlt->prev;
    }
    else {
      next = nlt->next;
    }

    /* only work with this channel and its subchannels if it is editable */
    if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_NLT(nlt)) {
      /* only include this track if selected in a way consistent with the filtering requirements */
      if (ANIMCHANNEL_SELOK(SEL_NLT(nlt))) {
        /* only include if this track is active */
        if (!(filter_mode & ANIMFILTER_ACTIVE) || (nlt->flag & NLATRACK_ACTIVE)) {
          /* name based filtering... */
          if (((ac->ads) && (ac->ads->searchstr[0] != '\0')) && (owner_id)) {
            bool track_ok = false, strip_ok = false;

            /* check if the name of the track, or the strips it has are ok... */
            track_ok = name_matches_dopesheet_filter(ac->ads, nlt->name);

            if (track_ok == false) {
              LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
                if (name_matches_dopesheet_filter(ac->ads, strip->name)) {
                  strip_ok = true;
                  break;
                }
              }
            }

            /* skip if both fail this test... */
            if (!track_ok && !strip_ok) {
              continue;
            }
          }

          /* add the track now that it has passed all our tests */
          ANIMCHANNEL_NEW_CHANNEL(ac->bmain, nlt, ANIMTYPE_NLATRACK, owner_id, nullptr);
        }
      }
    }
  }

  /* return the number of items added to the list */
  return items;
}

/* Include the control FCurves per NLA Strip in the channel list
 * NOTE: This is includes the expander too...
 */
static size_t animfilter_nla_controls(bAnimContext *ac,
                                      ListBase *anim_data,
                                      AnimData *adt,
                                      eAnimFilter_Flags filter_mode,
                                      ID *owner_id)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add control curves from each NLA strip... */
  /* NOTE: ANIMTYPE_FCURVES are created here, to avoid duplicating the code needed */
  BEGIN_ANIMFILTER_SUBCHANNELS ((adt->flag & ADT_NLA_SKEYS_COLLAPSED) == 0) {
    /* for now, we only go one level deep - so controls on grouped FCurves are not handled */
    LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
      LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
        /* pass strip as the "owner",
         * so that the name lookups (used while filtering) will resolve */
        /* NLA tracks are coming from AnimData, so owner of f-curves
         * is the same as owner of animation data. */
        tmp_items += animfilter_fcurves(ac,
                                        &tmp_data,
                                        static_cast<FCurve *>(strip->fcurves.first),
                                        ANIMTYPE_NLACURVE,
                                        filter_mode,
                                        strip,
                                        owner_id,
                                        owner_id);
      }
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* add the expander as a channel first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* currently these channels cannot be selected, so they should be skipped */
      if ((filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)) == 0) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, adt, ANIMTYPE_NLACONTROLS, owner_id, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* determine what animation data from AnimData block should get displayed */
static size_t animfilter_block_data(bAnimContext *ac,
                                    ListBase *anim_data,
                                    ID *id,
                                    const eAnimFilter_Flags filter_mode)
{
  AnimData *adt = BKE_animdata_from_id(id);
  size_t items = 0;

  /* image object data-blocks have no anim-data so check for nullptr */
  if (adt) {
    IdAdtTemplate *iat = reinterpret_cast<IdAdtTemplate *>(id);

    /* NOTE: this macro is used instead of inlining the logic here,
     * since this sort of filtering is still needed in a few places in the rest of the code still -
     * notably for the few cases where special mode-based
     * different types of data expanders are required.
     */
    ANIMDATA_FILTER_CASES(
        iat,
        { /* AnimData */
          /* specifically filter animdata block */
          if (ANIMCHANNEL_SELOK(SEL_ANIMDATA(adt))) {
            ANIMCHANNEL_NEW_CHANNEL(ac->bmain, adt, ANIMTYPE_ANIMDATA, id, nullptr);
          }
        },
        { /* NLA */
          items += animfilter_nla(ac, anim_data, adt, filter_mode, id);
        },
        { /* Drivers */
          items += animfilter_fcurves(ac,
                                      anim_data,
                                      static_cast<FCurve *>(adt->drivers.first),
                                      ANIMTYPE_FCURVE,
                                      filter_mode,
                                      nullptr,
                                      id,
                                      id);
        },
        { /* NLA Control Keyframes */
          items += animfilter_nla_controls(ac, anim_data, adt, filter_mode, id);
        },
        { /* Keyframes from legacy Action. */
          items += animfilter_action(ac,
                                     anim_data,
                                     adt->action->wrap(),
                                     adt->slot_handle,
                                     eAnimFilter_Flags(filter_mode),
                                     id);
        },
        { /* Keyframes from layered Action. */
          items += animfilter_action(ac,
                                     anim_data,
                                     adt->action->wrap(),
                                     adt->slot_handle,
                                     eAnimFilter_Flags(filter_mode),
                                     id);
        });
  }

  return items;
}

/* Include ShapeKey Data for ShapeKey Editor */
static size_t animdata_filter_shapekey(bAnimContext *ac,
                                       ListBase *anim_data,
                                       Key *key,
                                       const eAnimFilter_Flags filter_mode)
{
  using namespace blender::animrig;

  size_t items = 0;

  /* check if channels or only F-Curves */
  if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
    bDopeSheet *ads = ac->ads;

    if (key->type == KEY_RELATIVE) {
      /* TODO: This currently doesn't take into account the animatable "Range Min/Max" keys on the
       * key-blocks. */

      /* loop through the channels adding ShapeKeys as appropriate */
      LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
        /* skip the first one, since that's the non-animatable basis */
        if (kb == key->block.first) {
          continue;
        }

        /* Skip shapekey if the name doesn't match the filter string. */
        if (ads != nullptr && ads->searchstr[0] != '\0' &&
            name_matches_dopesheet_filter(ads, kb->name) == false)
        {
          continue;
        }

        /* only work with this channel and its subchannels if it is editable */
        if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_SHAPEKEY(kb)) {
          /* Only include this track if selected in a way consistent
           * with the filtering requirements. */
          if (ANIMCHANNEL_SELOK(SEL_SHAPEKEY(kb))) {
            /* TODO: consider 'active' too? */

            /* owner-id here must be key so that the F-Curve can be resolved... */
            ANIMCHANNEL_NEW_CHANNEL(ac->bmain, kb, ANIMTYPE_SHAPEKEY, key, nullptr);
          }
        }
      }
    }
    else {
      /* key->type == KEY_NORMAL */
      if (!key->adt || !key->adt->action) {
        return 0;
      }
      Action &action = key->adt->action->wrap();

      Vector<FCurve *> key_fcurves;
      for (FCurve *fcurve : fcurves_for_action_slot(action, key->adt->slot_handle)) {
        if (STREQ(fcurve->rna_path, "eval_time") ||
            BLI_str_endswith(fcurve->rna_path, ".interpolation"))
        {
          key_fcurves.append(fcurve);
        }
      }

      items += animfilter_fcurves_span(
          ac, anim_data, key_fcurves, key->adt->slot_handle, filter_mode, &key->id, &action.id);
    }
  }
  else {
    /* just use the action associated with the shapekey */
    /* TODO: somehow manage to pass dopesheet info down here too? */
    if (key->adt) {
      if (filter_mode & ANIMFILTER_ANIMDATA) {
        if (ANIMCHANNEL_SELOK(SEL_ANIMDATA(key->adt))) {
          ANIMCHANNEL_NEW_CHANNEL(ac->bmain, key->adt, ANIMTYPE_ANIMDATA, key, nullptr);
        }
      }
      else if (key->adt->action) {
        items = animfilter_action(ac,
                                  anim_data,
                                  key->adt->action->wrap(),
                                  key->adt->slot_handle,
                                  eAnimFilter_Flags(filter_mode),
                                  reinterpret_cast<ID *>(key));
      }
    }
  }

  /* return the number of items added to the list */
  return items;
}

/* Helper for Grease Pencil - layers within a data-block. */

static size_t animdata_filter_grease_pencil_layer(bAnimContext *ac,
                                                  ListBase *anim_data,
                                                  GreasePencil *grease_pencil,
                                                  blender::bke::greasepencil::Layer &layer,
                                                  const eAnimFilter_Flags filter_mode)
{

  size_t items = 0;

  /* Only if the layer is selected. */
  if (!ANIMCHANNEL_SELOK(layer.is_selected())) {
    return items;
  }

  /* Only if the layer is editable. */
  if ((filter_mode & ANIMFILTER_FOREDIT) && layer.is_locked()) {
    return items;
  }

  /* Only if the layer is active. */
  if ((filter_mode & ANIMFILTER_ACTIVE) && grease_pencil->is_layer_active(&layer)) {
    return items;
  }

  /* Skip empty layers. */
  if (layer.is_empty()) {
    return items;
  }

  /* Add layer channel. */
  ANIMCHANNEL_NEW_CHANNEL(ac->bmain,
                          static_cast<void *>(&layer),
                          ANIMTYPE_GREASE_PENCIL_LAYER,
                          grease_pencil,
                          nullptr);

  return items;
}

static size_t animdata_filter_grease_pencil_layer_node_recursive(
    bAnimContext *ac,
    ListBase *anim_data,
    GreasePencil *grease_pencil,
    blender::bke::greasepencil::TreeNode &node,
    eAnimFilter_Flags filter_mode)
{
  using namespace blender::bke::greasepencil;
  size_t items = 0;

  /* Skip node if the name doesn't match the filter string. */
  const bool name_search = (ac->ads->searchstr[0] != '\0');
  const bool skip_node = name_search &&
                         !name_matches_dopesheet_filter(ac->ads, node.name().c_str());

  if (node.is_layer() && !skip_node) {
    items += animdata_filter_grease_pencil_layer(
        ac, anim_data, grease_pencil, node.as_layer(), filter_mode);
  }
  else if (node.is_group()) {
    const LayerGroup &layer_group = node.as_group();

    ListBase tmp_data = {nullptr, nullptr};
    size_t tmp_items = 0;

    /* Add grease pencil layer channels. */
    BEGIN_ANIMFILTER_SUBCHANNELS (layer_group.is_expanded()) {
      LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &layer_group.children) {
        tmp_items += animdata_filter_grease_pencil_layer_node_recursive(
            ac, &tmp_data, grease_pencil, node_->wrap(), filter_mode);
      }
    }
    END_ANIMFILTER_SUBCHANNELS;

    if ((tmp_items == 0) && !name_search) {
      /* If no sub-channels, return early.
       * Except if the search by name is on, because we might want to display the layer group alone
       * in that case. */
      return items;
    }

    if ((filter_mode & ANIMFILTER_LIST_CHANNELS) && !skip_node) {
      /* Add data block container (if for drawing, and it contains sub-channels). */
      ANIMCHANNEL_NEW_CHANNEL(ac->bmain,
                              static_cast<void *>(&node),
                              ANIMTYPE_GREASE_PENCIL_LAYER_GROUP,
                              grease_pencil,
                              nullptr);
    }

    /* Add the list of collected channels. */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }
  return items;
}

static size_t animdata_filter_grease_pencil_layers_data(bAnimContext *ac,
                                                        ListBase *anim_data,
                                                        GreasePencil *grease_pencil,
                                                        const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  LISTBASE_FOREACH_BACKWARD (
      GreasePencilLayerTreeNode *, node, &grease_pencil->root_group_ptr->children)
  {
    items += animdata_filter_grease_pencil_layer_node_recursive(
        ac, anim_data, grease_pencil, node->wrap(), filter_mode);
  }

  return items;
}

/* Helper for Grease Pencil - layers within a data-block. */
static size_t animdata_filter_gpencil_layers_data_legacy(bAnimContext *ac,
                                                         ListBase *anim_data,
                                                         bGPdata *gpd,
                                                         const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  /* loop over layers as the conditions are acceptable (top-Down order) */
  LISTBASE_FOREACH_BACKWARD (bGPDlayer *, gpl, &gpd->layers) {
    /* only if selected */
    if (!ANIMCHANNEL_SELOK(SEL_GPL(gpl))) {
      continue;
    }

    /* only if editable */
    if ((filter_mode & ANIMFILTER_FOREDIT) && !EDITABLE_GPL(gpl)) {
      continue;
    }

    /* active... */
    if ((filter_mode & ANIMFILTER_ACTIVE) && (gpl->flag & GP_LAYER_ACTIVE) == 0) {
      continue;
    }

    /* skip layer if the name doesn't match the filter string */
    if (ac->ads != nullptr && ac->ads->searchstr[0] != '\0' &&
        name_matches_dopesheet_filter(ac->ads, gpl->info) == false)
    {
      continue;
    }

    /* Skip empty layers. */
    if (BLI_listbase_is_empty(&gpl->frames)) {
      continue;
    }

    /* add to list */
    ANIMCHANNEL_NEW_CHANNEL(ac->bmain, gpl, ANIMTYPE_GPLAYER, gpd, nullptr);
  }

  return items;
}

static size_t animdata_filter_grease_pencil_data(bAnimContext *ac,
                                                 ListBase *anim_data,
                                                 GreasePencil *grease_pencil,
                                                 eAnimFilter_Flags filter_mode)
{
  using namespace blender;

  size_t items = 0;

  /* The Grease Pencil mode is not supposed to show channels for regular F-Curves from regular
   * Actions. At some point this might be desirable, but it would also require changing the
   * filtering flags for pretty much all operators running there. */
  const bool show_animdata = grease_pencil->adt && (ac->datatype != ANIMCONT_GPENCIL);

  /* When asked from "AnimData" blocks (i.e. the top-level containers for normal animation),
   * for convenience, this will return grease pencil data-blocks instead.
   * This may cause issues down the track, but for now, this will do.
   */
  if (filter_mode & ANIMFILTER_ANIMDATA) {
    if (show_animdata) {
      items += animfilter_block_data(
          ac, anim_data, reinterpret_cast<ID *>(grease_pencil), filter_mode);
    }
  }
  else {
    ListBase tmp_data = {nullptr, nullptr};
    size_t tmp_items = 0;

    /* Add grease pencil layer channels. */
    BEGIN_ANIMFILTER_SUBCHANNELS (grease_pencil->flag &GREASE_PENCIL_ANIM_CHANNEL_EXPANDED) {
      if (show_animdata) {
        tmp_items += animfilter_block_data(
            ac, &tmp_data, reinterpret_cast<ID *>(grease_pencil), filter_mode);
      }

      if (!(filter_mode & ANIMFILTER_FCURVESONLY)) {
        tmp_items += animdata_filter_grease_pencil_layers_data(
            ac, &tmp_data, grease_pencil, filter_mode);
      }
    }
    END_ANIMFILTER_SUBCHANNELS;

    if (tmp_items == 0) {
      /* If no sub-channels, return early. */
      return items;
    }

    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* Add data block container (if for drawing, and it contains sub-channels). */
      ANIMCHANNEL_NEW_CHANNEL(
          ac->bmain, grease_pencil, ANIMTYPE_GREASE_PENCIL_DATABLOCK, grease_pencil, nullptr);
    }

    /* Add the list of collected channels. */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  return items;
}

static size_t animdata_filter_grease_pencil(bAnimContext *ac,
                                            ListBase *anim_data,
                                            const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;
  Scene *scene = ac->scene;
  ViewLayer *view_layer = ac->view_layer;
  bDopeSheet *ads = ac->ads;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!base->object || (base->object->type != OB_GREASE_PENCIL)) {
      continue;
    }
    Object *ob = base->object;

    if ((filter_mode & ANIMFILTER_DATA_VISIBLE) && !(ac->filters.flag & ADS_FILTER_INCL_HIDDEN)) {
      /* Layer visibility - we check both object and base,
       * since these may not be in sync yet. */
      if ((base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) == 0 ||
          (base->flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0)
      {
        continue;
      }

      /* Outliner restrict-flag */
      if (ob->visibility_flag & OB_HIDE_VIEWPORT) {
        continue;
      }
    }

    /* Check selection and object type filters */
    if ((ac->filters.flag & ADS_FILTER_ONLYSEL) && !(base->flag & BASE_SELECTED)) {
      /* Only selected should be shown */
      continue;
    }

    if (ads->filter_grp != nullptr) {
      if (BKE_collection_has_object_recursive(ads->filter_grp, ob) == 0) {
        continue;
      }
    }

    items += animdata_filter_grease_pencil_data(
        ac, anim_data, static_cast<GreasePencil *>(ob->data), filter_mode);
  }

  /* Return the number of items added to the list */
  return items;
}

/* Helper for Grease Pencil data integrated with main DopeSheet */
static size_t animdata_filter_ds_gpencil(bAnimContext *ac,
                                         ListBase *anim_data,
                                         bGPdata *gpd,
                                         eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add relevant animation channels for Grease Pencil */
  BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_GPD(gpd)) {
    /* add animation channels */
    tmp_items += animfilter_block_data(ac, &tmp_data, &gpd->id, filter_mode);

    /* add Grease Pencil layers */
    if (!(filter_mode & ANIMFILTER_FCURVESONLY)) {
      tmp_items += animdata_filter_gpencil_layers_data_legacy(ac, &tmp_data, gpd, filter_mode);
    }

    /* TODO: do these need a separate expander?
     * XXX:  what order should these go in? */
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      /* XXX: active check here needs checking */
      if (ANIMCHANNEL_ACTIVEOK(gpd)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, gpd, ANIMTYPE_DSGPENCIL, gpd, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* Helper for Cache File data integrated with main DopeSheet */
static size_t animdata_filter_ds_cachefile(bAnimContext *ac,
                                           ListBase *anim_data,
                                           CacheFile *cache_file,
                                           eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add relevant animation channels for Cache File */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_CACHEFILE_OBJD(cache_file)) {
    /* add animation channels */
    tmp_items += animfilter_block_data(ac, &tmp_data, &cache_file->id, filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      /* XXX: active check here needs checking */
      if (ANIMCHANNEL_ACTIVEOK(cache_file)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, cache_file, ANIMTYPE_DSCACHEFILE, cache_file, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* Helper for Mask Editing - mask layers */
static size_t animdata_filter_mask_data(bAnimContext *ac,
                                        ListBase *anim_data,
                                        Mask *mask,
                                        const eAnimFilter_Flags filter_mode)
{
  const MaskLayer *masklay_act = BKE_mask_layer_active(mask);
  size_t items = 0;

  LISTBASE_FOREACH (MaskLayer *, masklay, &mask->masklayers) {
    if (!ANIMCHANNEL_SELOK(SEL_MASKLAY(masklay))) {
      continue;
    }

    if ((filter_mode & ANIMFILTER_FOREDIT) && !EDITABLE_MASK(masklay)) {
      continue;
    }

    if ((filter_mode & ANIMFILTER_ACTIVE) & (masklay_act != masklay)) {
      continue;
    }

    ANIMCHANNEL_NEW_CHANNEL(ac->bmain, masklay, ANIMTYPE_MASKLAYER, mask, nullptr);
  }

  return items;
}

/* Grab all mask data */
static size_t animdata_filter_mask(bAnimContext *ac,
                                   ListBase *anim_data,
                                   void * /*data*/,
                                   eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  /* For now, grab mask data-blocks directly from main. */
  /* XXX: this is not good... */
  LISTBASE_FOREACH (Mask *, mask, &ac->bmain->masks) {
    ListBase tmp_data = {nullptr, nullptr};
    size_t tmp_items = 0;

    /* only show if mask is used by something... */
    if (ID_REAL_USERS(mask) < 1) {
      continue;
    }

    /* add mask animation channels */
    if (!(filter_mode & ANIMFILTER_FCURVESONLY)) {
      BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_MASK(mask)) {
        tmp_items += animdata_filter_mask_data(ac, &tmp_data, mask, filter_mode);
      }
      END_ANIMFILTER_SUBCHANNELS;
    }

    /* did we find anything? */
    if (!tmp_items) {
      continue;
    }

    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* add mask data-block as channel too (if for drawing, and it has layers) */
      ANIMCHANNEL_NEW_CHANNEL(ac->bmain, mask, ANIMTYPE_MASKDATABLOCK, nullptr, nullptr);
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* NOTE: owner_id is scene, material, or texture block,
 * which is the direct owner of the node tree in question. */
static size_t animdata_filter_ds_nodetree_group(bAnimContext *ac,
                                                ListBase *anim_data,
                                                ID *owner_id,
                                                bNodeTree *ntree,
                                                eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add nodetree animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_NTREE_DATA(ntree)) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(ntree), filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(ntree)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, ntree, ANIMTYPE_DSNTREE, owner_id, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_ds_nodetree(bAnimContext *ac,
                                          ListBase *anim_data,
                                          ID *owner_id,
                                          bNodeTree *ntree,
                                          const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  items += animdata_filter_ds_nodetree_group(ac, anim_data, owner_id, ntree, filter_mode);

  for (bNode *node : ntree->all_nodes()) {
    if (node->is_group()) {
      if (node->id) {
        if ((ac->filters.flag & ADS_FILTER_ONLYSEL) && (node->flag & NODE_SELECT) == 0) {
          continue;
        }
        /* Recurse into the node group */
        items += animdata_filter_ds_nodetree(ac,
                                             anim_data,
                                             owner_id,
                                             reinterpret_cast<bNodeTree *>(node->id),
                                             filter_mode | ANIMFILTER_TMP_IGNORE_ONLYSEL);
      }
    }
  }

  return items;
}

static size_t animdata_filter_ds_linestyle(bAnimContext *ac,
                                           ListBase *anim_data,
                                           Scene *sce,
                                           eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    LISTBASE_FOREACH (FreestyleLineSet *, lineset, &view_layer->freestyle_config.linesets) {
      if (lineset->linestyle) {
        lineset->linestyle->id.tag |= ID_TAG_DOIT;
      }
    }
  }

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    /* skip render layers without Freestyle enabled */
    if ((view_layer->flag & VIEW_LAYER_FREESTYLE) == 0) {
      continue;
    }

    /* loop over linesets defined in the render layer */
    LISTBASE_FOREACH (FreestyleLineSet *, lineset, &view_layer->freestyle_config.linesets) {
      FreestyleLineStyle *linestyle = lineset->linestyle;
      ListBase tmp_data = {nullptr, nullptr};
      size_t tmp_items = 0;

      if ((linestyle == nullptr) || !(linestyle->id.tag & ID_TAG_DOIT)) {
        continue;
      }
      linestyle->id.tag &= ~ID_TAG_DOIT;

      /* add scene-level animation channels */
      BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_LS_SCED(linestyle)) {
        /* animation data filtering */
        tmp_items += animfilter_block_data(
            ac, &tmp_data, reinterpret_cast<ID *>(linestyle), filter_mode);
      }
      END_ANIMFILTER_SUBCHANNELS;

      /* did we find anything? */
      if (tmp_items) {
        /* include anim-expand widget first */
        if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
          /* check if filtering by active status */
          if (ANIMCHANNEL_ACTIVEOK(linestyle)) {
            ANIMCHANNEL_NEW_CHANNEL(ac->bmain, linestyle, ANIMTYPE_DSLINESTYLE, sce, nullptr);
          }
        }

        /* now add the list of collected channels */
        BLI_movelisttolist(anim_data, &tmp_data);
        BLI_assert(BLI_listbase_is_empty(&tmp_data));
        items += tmp_items;
      }
    }
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_ds_texture(
    bAnimContext *ac, ListBase *anim_data, Tex *tex, ID *owner_id, eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add texture's animation data to temp collection */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_TEX_DATA(tex)) {
    /* texture animdata */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(tex), filter_mode);

    /* nodes */
    if ((tex->nodetree) && !(ac->filters.flag & ADS_FILTER_NONTREE)) {
      /* owner_id as id instead of texture,
       * since it'll otherwise be impossible to track the depth. */

      /* FIXME: perhaps as a result, textures should NOT be included under materials,
       * but under their own section instead so that free-floating textures can also be animated.
       */
      tmp_items += animdata_filter_ds_nodetree(
          ac, &tmp_data, reinterpret_cast<ID *>(tex), tex->nodetree, filter_mode);
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include texture-expand widget? */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(tex)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, tex, ANIMTYPE_DSTEX, owner_id, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* NOTE: owner_id is the direct owner of the texture stack in question
 *       It used to be Material/Light/World before the Blender Internal removal for 2.8
 */
static size_t animdata_filter_ds_textures(bAnimContext *ac,
                                          ListBase *anim_data,
                                          ID *owner_id,
                                          const eAnimFilter_Flags filter_mode)
{
  MTex **mtex = nullptr;
  size_t items = 0;
  int a = 0;

  /* get datatype specific data first */
  if (owner_id == nullptr) {
    return 0;
  }

  switch (GS(owner_id->name)) {
    case ID_PA: {
      ParticleSettings *part = reinterpret_cast<ParticleSettings *>(owner_id);
      mtex = reinterpret_cast<MTex **>(&part->mtex);
      break;
    }
    default: {
      /* invalid/unsupported option */
      if (G.debug & G_DEBUG) {
        printf("ERROR: Unsupported owner_id (i.e. texture stack) for filter textures - %s\n",
               owner_id->name);
      }
      return 0;
    }
  }

  /* Firstly check that we actually have some textures,
   * by gathering all textures in a temp list. */
  for (a = 0; a < MAX_MTEX; a++) {
    Tex *tex = (mtex[a]) ? mtex[a]->tex : nullptr;

    /* for now, if no texture returned, skip (this shouldn't confuse the user I hope) */
    if (tex == nullptr) {
      continue;
    }

    /* add texture's anim channels */
    items += animdata_filter_ds_texture(ac, anim_data, tex, owner_id, filter_mode);
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_ds_material(bAnimContext *ac,
                                          ListBase *anim_data,
                                          Material *ma,
                                          eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add material's animation data to temp collection */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_MAT_OBJD(ma)) {
    /* material's animation data */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(ma), filter_mode);

    /* nodes */
    if ((ma->nodetree) && !(ac->filters.flag & ADS_FILTER_NONTREE)) {
      tmp_items += animdata_filter_ds_nodetree(
          ac, &tmp_data, reinterpret_cast<ID *>(ma), ma->nodetree, filter_mode);
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include material-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(ma)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, ma, ANIMTYPE_DSMAT, ma, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  return items;
}

static size_t animdata_filter_ds_materials(bAnimContext *ac,
                                           ListBase *anim_data,
                                           Object *ob,
                                           const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;
  int a = 0;

  /* First pass: take the materials referenced via the Material slots of the object. */
  for (a = 1; a <= ob->totcol; a++) {
    Material *ma = BKE_object_material_get(ob, a);

    /* if material is valid, try to add relevant contents from here */
    if (ma) {
      /* add channels */
      items += animdata_filter_ds_material(ac, anim_data, ma, filter_mode);
    }
  }

  /* return the number of items added to the list */
  return items;
}

/* ............ */

/* Temporary context for modifier linked-data channel extraction */
struct tAnimFilterModifiersContext {
  bAnimContext *ac; /* anim editor context */
  bDopeSheet *ads;  /* TODO: Remove this pointer from the struct and just use ac->ads. */

  ListBase tmp_data; /* list of channels created (but not yet added to the main list) */
  size_t items;      /* number of channels created */

  eAnimFilter_Flags filter_mode; /* flags for stuff we want to filter */
};

/* dependency walker callback for modifier dependencies */
static void animfilter_modifier_idpoin_cb(void *afm_ptr,
                                          Object *ob,
                                          ID **idpoin,
                                          LibraryForeachIDCallbackFlag /*cb_flag*/)
{
  tAnimFilterModifiersContext *afm = static_cast<tAnimFilterModifiersContext *>(afm_ptr);
  ID *owner_id = &ob->id;
  ID *id = *idpoin;

  /* NOTE: the walker only guarantees to give us all the ID-ptr *slots*,
   * not just the ones which are actually used, so be careful!
   */
  if (id == nullptr) {
    return;
  }

  /* check if this is something we're interested in... */
  switch (GS(id->name)) {
    case ID_TE: /* Textures */
    {
      Tex *tex = reinterpret_cast<Tex *>(id);
      if (!(afm->ac->filters.flag & ADS_FILTER_NOTEX)) {
        BLI_assert(afm->ac->ads == afm->ads);
        afm->items += animdata_filter_ds_texture(
            afm->ac, &afm->tmp_data, tex, owner_id, afm->filter_mode);
      }
      break;
    }
    case ID_NT: {
      bNodeTree *node_tree = reinterpret_cast<bNodeTree *>(id);
      if (!(afm->ac->filters.flag & ADS_FILTER_NONTREE)) {
        BLI_assert(afm->ac->ads == afm->ads);
        afm->items += animdata_filter_ds_nodetree(
            afm->ac, &afm->tmp_data, owner_id, node_tree, afm->filter_mode);
      }
    }

    /* TODO: images? */
    default:
      break;
  }
}

/* animation linked to data used by modifiers
 * NOTE: strictly speaking, modifier animation is already included under Object level
 *       but for some modifiers (e.g. Displace), there can be linked data that has settings
 *       which would be nice to animate (i.e. texture parameters) but which are not actually
 *       attached to any other objects/materials/etc. in the scene
 */
/* TODO: do we want an expander for this? */
static size_t animdata_filter_ds_modifiers(bAnimContext *ac,
                                           ListBase *anim_data,
                                           Object *ob,
                                           const eAnimFilter_Flags filter_mode)
{
  tAnimFilterModifiersContext afm = {nullptr};
  size_t items = 0;

  /* 1) create a temporary "context" containing all the info we have here to pass to the callback
   *    use to walk through the dependencies of the modifiers
   *
   * Assumes that all other unspecified values (i.e. accumulation buffers)
   * are zeroed out properly!
   */
  afm.ac = ac;
  afm.ads = ac->ads; /* TODO: Remove this pointer from the struct and just use afm.ac->ads. */
  afm.filter_mode = filter_mode;

  /* 2) walk over dependencies */
  BKE_modifiers_foreach_ID_link(ob, animfilter_modifier_idpoin_cb, &afm);

  /* 3) extract data from the context, merging it back into the standard list */
  if (afm.items) {
    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &afm.tmp_data);
    BLI_assert(BLI_listbase_is_empty(&afm.tmp_data));
    items += afm.items;
  }

  return items;
}

/* ............ */

static size_t animdata_filter_ds_particles(bAnimContext *ac,
                                           ListBase *anim_data,
                                           Object *ob,
                                           eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    ListBase tmp_data = {nullptr, nullptr};
    size_t tmp_items = 0;

    /* Note that when psys->part->adt is nullptr the textures can still be
     * animated. */
    if (psys->part == nullptr) {
      continue;
    }

    /* add particle-system's animation data to temp collection */
    BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_PART_OBJD(psys->part)) {
      /* particle system's animation data */
      tmp_items += animfilter_block_data(
          ac, &tmp_data, reinterpret_cast<ID *>(psys->part), filter_mode);

      /* textures */
      if (!(ac->filters.flag & ADS_FILTER_NOTEX)) {
        tmp_items += animdata_filter_ds_textures(
            ac, &tmp_data, reinterpret_cast<ID *>(psys->part), filter_mode);
      }
    }
    END_ANIMFILTER_SUBCHANNELS;

    /* did we find anything? */
    if (tmp_items) {
      /* include particle-expand widget first */
      if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
        /* check if filtering by active status */
        if (ANIMCHANNEL_ACTIVEOK(psys->part)) {
          ANIMCHANNEL_NEW_CHANNEL(ac->bmain, psys->part, ANIMTYPE_DSPART, psys->part, nullptr);
        }
      }

      /* now add the list of collected channels */
      BLI_movelisttolist(anim_data, &tmp_data);
      BLI_assert(BLI_listbase_is_empty(&tmp_data));
      items += tmp_items;
    }
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_ds_obdata(bAnimContext *ac,
                                        ListBase *anim_data,
                                        Object *ob,
                                        eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  IdAdtTemplate *iat = static_cast<IdAdtTemplate *>(ob->data);
  eAnim_ChannelType type = ANIMTYPE_NONE;
  short expanded = 0;
  const eDopeSheet_FilterFlag ads_filterflag = ac->filters.flag;
  const eDopeSheet_FilterFlag2 ads_filterflag2 = ac->filters.flag2;

  /* get settings based on data type */
  switch (ob->type) {
    case OB_CAMERA: /* ------- Camera ------------ */
    {
      Camera *ca = static_cast<Camera *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOCAM) {
        return 0;
      }

      type = ANIMTYPE_DSCAM;
      expanded = FILTER_CAM_OBJD(ca);
      break;
    }
    case OB_LAMP: /* ---------- Light ----------- */
    {
      Light *la = static_cast<Light *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOLAM) {
        return 0;
      }

      type = ANIMTYPE_DSLAM;
      expanded = FILTER_LAM_OBJD(la);
      break;
    }
    case OB_CURVES_LEGACY: /* ------- Curve ---------- */
    case OB_SURF:          /* ------- Nurbs Surface ---------- */
    case OB_FONT:          /* ------- Text Curve ---------- */
    {
      Curve *cu = static_cast<Curve *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOCUR) {
        return 0;
      }

      type = ANIMTYPE_DSCUR;
      expanded = FILTER_CUR_OBJD(cu);
      break;
    }
    case OB_MBALL: /* ------- MetaBall ---------- */
    {
      MetaBall *mb = static_cast<MetaBall *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOMBA) {
        return 0;
      }

      type = ANIMTYPE_DSMBALL;
      expanded = FILTER_MBALL_OBJD(mb);
      break;
    }
    case OB_ARMATURE: /* ------- Armature ---------- */
    {
      bArmature *arm = static_cast<bArmature *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOARM) {
        return 0;
      }

      type = ANIMTYPE_DSARM;
      expanded = FILTER_ARM_OBJD(arm);
      break;
    }
    case OB_MESH: /* ------- Mesh ---------- */
    {
      Mesh *mesh = static_cast<Mesh *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOMESH) {
        return 0;
      }

      type = ANIMTYPE_DSMESH;
      expanded = FILTER_MESH_OBJD(mesh);
      break;
    }
    case OB_LATTICE: /* ---- Lattice ---- */
    {
      Lattice *lt = static_cast<Lattice *>(ob->data);

      if (ads_filterflag & ADS_FILTER_NOLAT) {
        return 0;
      }

      type = ANIMTYPE_DSLAT;
      expanded = FILTER_LATTICE_OBJD(lt);
      break;
    }
    case OB_SPEAKER: /* ---------- Speaker ----------- */
    {
      Speaker *spk = static_cast<Speaker *>(ob->data);

      type = ANIMTYPE_DSSPK;
      expanded = FILTER_SPK_OBJD(spk);
      break;
    }
    case OB_CURVES: /* ---------- Curves ----------- */
    {
      Curves *curves = static_cast<Curves *>(ob->data);

      if (ads_filterflag2 & ADS_FILTER_NOHAIR) {
        return 0;
      }

      type = ANIMTYPE_DSHAIR;
      expanded = FILTER_CURVES_OBJD(curves);
      break;
    }
    case OB_POINTCLOUD: /* ---------- PointCloud ----------- */
    {
      PointCloud *pointcloud = static_cast<PointCloud *>(ob->data);

      if (ads_filterflag2 & ADS_FILTER_NOPOINTCLOUD) {
        return 0;
      }

      type = ANIMTYPE_DSPOINTCLOUD;
      expanded = FILTER_POINTS_OBJD(pointcloud);
      break;
    }
    case OB_VOLUME: /* ---------- Volume ----------- */
    {
      Volume *volume = static_cast<Volume *>(ob->data);

      if (ads_filterflag2 & ADS_FILTER_NOVOLUME) {
        return 0;
      }

      type = ANIMTYPE_DSVOLUME;
      expanded = FILTER_VOLUME_OBJD(volume);
      break;
    }
    case OB_LIGHTPROBE: /* ---------- LightProbe ----------- */
    {
      LightProbe *probe = static_cast<LightProbe *>(ob->data);

      if (ads_filterflag2 & ADS_FILTER_NOLIGHTPROBE) {
        return 0;
      }

      type = ANIMTYPE_DSLIGHTPROBE;
      expanded = FILTER_LIGHTPROBE_OBJD(probe);
      break;
    }
  }

  /* add object data animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (expanded) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(iat), filter_mode);

    /* sub-data filtering... */
    switch (ob->type) {
      case OB_LAMP: /* light - textures + nodetree */
      {
        Light *la = static_cast<Light *>(ob->data);
        bNodeTree *ntree = la->nodetree;

        /* nodetree */
        if ((ntree) && !(ads_filterflag & ADS_FILTER_NONTREE)) {
          tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, &la->id, ntree, filter_mode);
        }
        break;
      }
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(iat)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, iat, type, iat, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* shapekey-level animation */
static size_t animdata_filter_ds_keyanim(
    bAnimContext *ac, ListBase *anim_data, Object *ob, Key *key, eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add shapekey-level animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_SKE_OBJD(key)) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(key), filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include key-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      if (ANIMCHANNEL_ACTIVEOK(key)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, key, ANIMTYPE_DSSKEY, ob, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* object-level animation */
static size_t animdata_filter_ds_obanim(bAnimContext *ac,
                                        ListBase *anim_data,
                                        Object *ob,
                                        eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  AnimData *adt = ob->adt;
  eAnim_ChannelType type = ANIMTYPE_NONE;
  short expanded = 1;
  void *cdata = nullptr;

  /* determine the type of expander channels to use */
  /* this is the best way to do this for now... */
  ANIMDATA_FILTER_CASES(
      ob, /* Some useless long comment to prevent wrapping by old clang-format versions... */
      {/* AnimData - no channel, but consider data */},
      {/* NLA - no channel, but consider data */},
      { /* Drivers */
        type = ANIMTYPE_FILLDRIVERS;
        cdata = adt;
        expanded = EXPANDED_DRVD(adt);
      },
      {/* NLA Strip Controls - no dedicated channel for now (XXX) */},
      { /* Keyframes from legacy Action. */
        type = ANIMTYPE_FILLACTD;
        cdata = adt->action;
        expanded = EXPANDED_ACTC(adt->action);
      },
      { /* Keyframes from layered action. */
        type = ANIMTYPE_FILLACT_LAYERED;
        cdata = adt->action;
        expanded = EXPANDED_ADT(adt);
      });

  /* add object-level animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (expanded) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(ob), filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include anim-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      if (type != ANIMTYPE_NONE) {
        /* NOTE: active-status (and the associated checks) don't apply here... */
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, cdata, type, ob, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

/* get animation channels from object2 */
static size_t animdata_filter_dopesheet_ob(bAnimContext *ac,
                                           ListBase *anim_data,
                                           Base *base,
                                           eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  Object *ob = base->object;
  size_t tmp_items = 0;
  size_t items = 0;
  const eDopeSheet_FilterFlag ads_filterflag = ac->filters.flag;

  /* filter data contained under object first */
  BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_OBJC(ob)) {
    Key *key = BKE_key_from_object(ob);

    /* object-level animation */
    if ((ob->adt) && !(ads_filterflag & ADS_FILTER_NOOBJ)) {
      tmp_items += animdata_filter_ds_obanim(ac, &tmp_data, ob, filter_mode);
    }

    /* particle deflector textures */
    if (ob->pd != nullptr && ob->pd->tex != nullptr && !(ads_filterflag & ADS_FILTER_NOTEX)) {
      tmp_items += animdata_filter_ds_texture(ac, &tmp_data, ob->pd->tex, &ob->id, filter_mode);
    }

    /* shape-key */
    if ((key && key->adt) && !(ads_filterflag & ADS_FILTER_NOSHAPEKEYS)) {
      tmp_items += animdata_filter_ds_keyanim(ac, &tmp_data, ob, key, filter_mode);
    }

    /* modifiers */
    if ((ob->modifiers.first) && !(ads_filterflag & ADS_FILTER_NOMODIFIERS)) {
      tmp_items += animdata_filter_ds_modifiers(ac, &tmp_data, ob, filter_mode);
    }

    /* materials */
    if ((ob->totcol) && !(ads_filterflag & ADS_FILTER_NOMAT)) {
      tmp_items += animdata_filter_ds_materials(ac, &tmp_data, ob, filter_mode);
    }

    /* object data */
    if (ob->data) {
      tmp_items += animdata_filter_ds_obdata(ac, &tmp_data, ob, filter_mode);
    }

    /* particles */
    if ((ob->particlesystem.first) && !(ads_filterflag & ADS_FILTER_NOPART)) {
      tmp_items += animdata_filter_ds_particles(ac, &tmp_data, ob, filter_mode);
    }

    /* grease pencil */
    if (ob->type == OB_GREASE_PENCIL && (ob->data) && !(ads_filterflag & ADS_FILTER_NOGPENCIL)) {
      tmp_items += animdata_filter_grease_pencil_data(
          ac, &tmp_data, static_cast<GreasePencil *>(ob->data), filter_mode);
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* if we collected some channels, add these to the new list... */
  if (tmp_items) {
    /* firstly add object expander if required */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by selection */
      /* XXX: double-check on this -
       * most of the time, a lot of tools need to filter out these channels! */
      if (ANIMCHANNEL_SELOK((base->flag & BASE_SELECTED))) {
        /* check if filtering by active status */
        if (ANIMCHANNEL_ACTIVEOK(ob)) {
          ANIMCHANNEL_NEW_CHANNEL(ac->bmain, base, ANIMTYPE_OBJECT, ob, nullptr);
        }
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added */
  return items;
}

static size_t animdata_filter_ds_world(
    bAnimContext *ac, ListBase *anim_data, Scene *sce, World *wo, eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* add world animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (FILTER_WOR_SCED(wo)) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(wo), filter_mode);

    /* nodes */
    if ((wo->nodetree) && !(ac->filters.flag & ADS_FILTER_NONTREE)) {
      tmp_items += animdata_filter_ds_nodetree(
          ac, &tmp_data, reinterpret_cast<ID *>(wo), wo->nodetree, filter_mode);
    }
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(wo)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, wo, ANIMTYPE_DSWOR, sce, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_ds_scene(bAnimContext *ac,
                                       ListBase *anim_data,
                                       Scene *sce,
                                       eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  AnimData *adt = sce->adt;
  eAnim_ChannelType type = ANIMTYPE_NONE;
  short expanded = 1;
  void *cdata = nullptr;

  /* determine the type of expander channels to use */
  /* this is the best way to do this for now... */
  ANIMDATA_FILTER_CASES(
      sce, /* Some useless long comment to prevent wrapping by old clang-format versions... */
      {/* AnimData - no channel, but consider data */},
      {/* NLA - no channel, but consider data */},
      { /* Drivers */
        type = ANIMTYPE_FILLDRIVERS;
        cdata = adt;
        expanded = EXPANDED_DRVD(adt);
      },
      {/* NLA Strip Controls - no dedicated channel for now (XXX) */},
      { /* Keyframes from legacy Action. */
        type = ANIMTYPE_FILLACTD;
        cdata = adt->action;
        expanded = EXPANDED_ACTC(adt->action);
      },
      { /* Keyframes from layered Action. */
        type = ANIMTYPE_FILLACT_LAYERED;
        cdata = adt->action;
        expanded = EXPANDED_ADT(adt);
      });

  /* add scene-level animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (expanded) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(sce), filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* did we find anything? */
  if (tmp_items) {
    /* include anim-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      if (type != ANIMTYPE_NONE) {
        /* NOTE: active-status (and the associated checks) don't apply here... */
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, cdata, type, sce, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_dopesheet_scene(bAnimContext *ac,
                                              ListBase *anim_data,
                                              Scene *sce,
                                              eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;

  /* filter data contained under object first */
  BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_SCEC(sce)) {
    bNodeTree *ntree = sce->compositing_node_group;
    World *wo = sce->world;
    Editing *ed = sce->ed;

    /* Action, Drivers, or NLA for Scene */
    if ((ac->filters.flag & ADS_FILTER_NOSCE) == 0) {
      tmp_items += animdata_filter_ds_scene(ac, &tmp_data, sce, filter_mode);
    }

    /* world */
    if ((wo) && !(ac->filters.flag & ADS_FILTER_NOWOR)) {
      tmp_items += animdata_filter_ds_world(ac, &tmp_data, sce, wo, filter_mode);
    }

    /* nodetree */
    if ((ntree) && !(ac->filters.flag & ADS_FILTER_NONTREE)) {
      tmp_items += animdata_filter_ds_nodetree(
          ac, &tmp_data, reinterpret_cast<ID *>(sce), ntree, filter_mode);
    }

    /* Strip modifier node trees. */
    if (ed && !(ac->filters.flag & ADS_FILTER_NONTREE)) {
      VectorSet<ID *> node_trees;
      seq::foreach_strip(&ed->seqbase, [&](Strip *strip) {
        seq::foreach_strip_modifier_id(strip, [&](ID *id) {
          if (GS(id->name) == ID_NT) {
            node_trees.add(id);
          }
        });
        return true;
      });
      for (ID *node_tree : node_trees) {
        tmp_items += animdata_filter_ds_nodetree(
            ac, &tmp_data, &sce->id, reinterpret_cast<bNodeTree *>(node_tree), filter_mode);
      }
    }

    /* line styles */
    if ((ac->filters.flag & ADS_FILTER_NOLINESTYLE) == 0) {
      tmp_items += animdata_filter_ds_linestyle(ac, &tmp_data, sce, filter_mode);
    }

    /* TODO: one day, when sequencer becomes its own datatype,
     * perhaps it should be included here. */
  }
  END_ANIMFILTER_SUBCHANNELS;

  /* if we collected some channels, add these to the new list... */
  if (tmp_items) {
    /* firstly add object expander if required */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by selection */
      if (ANIMCHANNEL_SELOK((sce->flag & SCE_DS_SELECTED))) {
        /* NOTE: active-status doesn't matter for this! */
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, sce, ANIMTYPE_SCENE, sce, nullptr);
      }
    }

    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }

  /* return the number of items added */
  return items;
}

static size_t animdata_filter_ds_movieclip(bAnimContext *ac,
                                           ListBase *anim_data,
                                           MovieClip *clip,
                                           eAnimFilter_Flags filter_mode)
{
  ListBase tmp_data = {nullptr, nullptr};
  size_t tmp_items = 0;
  size_t items = 0;
  /* add world animation channels */
  BEGIN_ANIMFILTER_SUBCHANNELS (EXPANDED_MCLIP(clip)) {
    /* animation data filtering */
    tmp_items += animfilter_block_data(ac, &tmp_data, reinterpret_cast<ID *>(clip), filter_mode);
  }
  END_ANIMFILTER_SUBCHANNELS;
  /* did we find anything? */
  if (tmp_items) {
    /* include data-expand widget first */
    if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
      /* check if filtering by active status */
      if (ANIMCHANNEL_ACTIVEOK(clip)) {
        ANIMCHANNEL_NEW_CHANNEL(ac->bmain, clip, ANIMTYPE_DSMCLIP, clip, nullptr);
      }
    }
    /* now add the list of collected channels */
    BLI_movelisttolist(anim_data, &tmp_data);
    BLI_assert(BLI_listbase_is_empty(&tmp_data));
    items += tmp_items;
  }
  /* return the number of items added to the list */
  return items;
}

static size_t animdata_filter_dopesheet_movieclips(bAnimContext *ac,
                                                   ListBase *anim_data,
                                                   const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;
  LISTBASE_FOREACH (MovieClip *, clip, &ac->bmain->movieclips) {
    /* only show if gpd is used by something... */
    if (ID_REAL_USERS(clip) < 1) {
      continue;
    }
    items += animdata_filter_ds_movieclip(ac, anim_data, clip, filter_mode);
  }
  /* return the number of items added to the list */
  return items;
}

/* Helper for animdata_filter_dopesheet() - For checking if an object should be included or not */
static bool animdata_filter_base_is_ok(bAnimContext *ac,
                                       Base *base,
                                       const eObjectMode object_mode,
                                       const eAnimFilter_Flags filter_mode)
{
  Object *ob = base->object;

  if (base->object == nullptr) {
    return false;
  }

  /* firstly, check if object can be included, by the following factors:
   * - if only visible, must check for layer and also viewport visibility
   *   --> while tools may demand only visible, user setting takes priority
   *       as user option controls whether sets of channels get included while
   *       tool-flag takes into account collapsed/open channels too
   * - if only selected, must check if object is selected
   * - there must be animation data to edit (this is done recursively as we
   *   try to add the channels)
   */
  if ((filter_mode & ANIMFILTER_DATA_VISIBLE) && !(ac->filters.flag & ADS_FILTER_INCL_HIDDEN)) {
    /* layer visibility - we check both object and base, since these may not be in sync yet */
    if ((base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) == 0 ||
        (base->flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0)
    {
      return false;
    }

    /* outliner restrict-flag */
    if (ob->visibility_flag & OB_HIDE_VIEWPORT) {
      return false;
    }
  }

  /* if only F-Curves with visible flags set can be shown, check that
   * data-block hasn't been set to invisible.
   */
  if (filter_mode & ANIMFILTER_CURVE_VISIBLE) {
    if ((ob->adt) && (ob->adt->flag & ADT_CURVES_NOT_VISIBLE)) {
      return false;
    }
  }

  /* Pinned curves are visible regardless of selection flags. */
  if ((ob->adt) && (ob->adt->flag & ADT_CURVES_ALWAYS_VISIBLE)) {
    return true;
  }

  /* Special case.
   * We don't do recursive checks for pin, but we need to deal with tricky
   * setup like animated camera lens without (pinned) animated camera location.
   * Without such special handle here we wouldn't be able to pin such
   * camera data animation to the editor.
   */
  if (ob->data != nullptr) {
    AnimData *data_adt = BKE_animdata_from_id(static_cast<ID *>(ob->data));
    if (data_adt != nullptr && (data_adt->flag & ADT_CURVES_ALWAYS_VISIBLE)) {
      return true;
    }
  }

  /* check selection and object type filters */
  if (ac->filters.flag & ADS_FILTER_ONLYSEL) {
    if (object_mode & OB_MODE_POSE) {
      /* When in pose-mode handle all pose-mode objects.
       * This avoids problems with pose-mode where objects may be unselected,
       * where a selected bone of an unselected object would be hidden. see: #81922. */
      if (!(base->object->mode & object_mode)) {
        return false;
      }
    }
    else {
      /* only selected should be shown (ignore active) */
      if (!(base->flag & BASE_SELECTED)) {
        return false;
      }
    }
  }

  /* check if object belongs to the filtering group if option to filter
   * objects by the grouped status is on
   * - used to ease the process of doing multiple-character choreographies
   */
  if (ac->ads->filter_grp != nullptr) {
    if (BKE_collection_has_object_recursive(ac->ads->filter_grp, ob) == 0) {
      return false;
    }
  }

  /* no reason to exclude this object... */
  return true;
}

/* Helper for animdata_filter_ds_sorted_bases() - Comparison callback for two Base pointers... */
static int ds_base_sorting_cmp(const void *base1_ptr, const void *base2_ptr)
{
  const Base *b1 = *((const Base **)base1_ptr);
  const Base *b2 = *((const Base **)base2_ptr);

  return BLI_strcasecmp_natural(b1->object->id.name + 2, b2->object->id.name + 2);
}

/* Get a sorted list of all the bases - for inclusion in dopesheet (when drawing channels) */
static Base **animdata_filter_ds_sorted_bases(bAnimContext *ac,
                                              const Scene *scene,
                                              ViewLayer *view_layer,
                                              const eAnimFilter_Flags filter_mode,
                                              size_t *r_usable_bases)
{
  /* Create an array with space for all the bases, but only containing the usable ones */
  BKE_view_layer_synced_ensure(scene, view_layer);
  ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer);
  size_t tot_bases = BLI_listbase_count(object_bases);
  size_t num_bases = 0;

  Base **sorted_bases = MEM_calloc_arrayN<Base *>(tot_bases, "Dopesheet Usable Sorted Bases");
  LISTBASE_FOREACH (Base *, base, object_bases) {
    const eObjectMode object_mode = eObjectMode(base->object->mode);
    if (animdata_filter_base_is_ok(ac, base, object_mode, filter_mode)) {
      sorted_bases[num_bases++] = base;
    }
  }

  /* Sort this list of pointers (based on the names) */
  qsort(sorted_bases, num_bases, sizeof(Base *), ds_base_sorting_cmp);

  /* Return list of sorted bases */
  *r_usable_bases = num_bases;
  return sorted_bases;
}

/* TODO: implement pinning...
 * (if and when pinning is done, what we need to do is to provide freeing mechanisms -
 * to protect against data that was deleted). */
static size_t animdata_filter_dopesheet(bAnimContext *ac,
                                        ListBase *anim_data,
                                        eAnimFilter_Flags filter_mode)
{
  bDopeSheet *ads = ac->ads;
  Scene *scene = reinterpret_cast<Scene *>(ads->source);
  ViewLayer *view_layer = ac->view_layer;
  size_t items = 0;

  /* check that we do indeed have a scene */
  if ((ads->source == nullptr) || (GS(ads->source->name) != ID_SCE)) {
    printf("Dope Sheet Error: No scene!\n");
    if (G.debug & G_DEBUG) {
      printf("\tPointer = %p, Name = '%s'\n",
             (void *)ads->source,
             (ads->source) ? ads->source->name : nullptr);
    }
    return 0;
  }

  /* augment the filter-flags with settings based on the dopesheet filterflags
   * so that some temp settings can get added automagically...
   */
  if (ac->filters.flag & ADS_FILTER_SELEDIT) {
    /* only selected F-Curves should get their keyframes considered for editability */
    filter_mode |= ANIMFILTER_SELEDIT;
  }

  /* Cache files level animations (frame duration and such). */
  const bool use_only_selected = (ac->filters.flag & ADS_FILTER_ONLYSEL);
  if (!use_only_selected && !(ac->filters.flag2 & ADS_FILTER_NOCACHEFILES)) {
    LISTBASE_FOREACH (CacheFile *, cache_file, &ac->bmain->cachefiles) {
      items += animdata_filter_ds_cachefile(ac, anim_data, cache_file, filter_mode);
    }
  }

  /* Annotations are always shown if "Only Show Selected" is disabled. */
  if (!use_only_selected && !(ac->filters.flag & ADS_FILTER_NOGPENCIL)) {
    LISTBASE_FOREACH (bGPdata *, gp_data, &ac->bmain->gpencils) {
      items += animdata_filter_ds_gpencil(ac, anim_data, gp_data, filter_mode);
    }
  }

  /* movie clip's animation */
  if (!use_only_selected && !(ac->filters.flag2 & ADS_FILTER_NOMOVIECLIPS)) {
    items += animdata_filter_dopesheet_movieclips(ac, anim_data, filter_mode);
  }

  /* Scene-linked animation - e.g. world, compositing nodes, scene anim
   * (including sequencer currently). */
  items += animdata_filter_dopesheet_scene(ac, anim_data, scene, filter_mode);

  /* If filtering for channel drawing, we want the objects in alphabetical order,
   * to make it easier to predict where items are in the hierarchy
   * - This order only really matters
   *   if we need to show all channels in the list (e.g. for drawing).
   *   (XXX: What about lingering "active" flags? The order may now become unpredictable)
   * - Don't do this if this behavior has been turned off (i.e. due to it being too slow)
   * - Don't do this if there's just a single object
   */
  BKE_view_layer_synced_ensure(scene, view_layer);
  ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer);
  if ((filter_mode & ANIMFILTER_LIST_CHANNELS) && !(ads->flag & ADS_FLAG_NO_DB_SORT) &&
      (object_bases->first != object_bases->last))
  {
    /* Filter list of bases (i.e. objects), sort them, then add their contents normally... */
    /* TODO: Cache the old sorted order - if the set of bases hasn't changed, don't re-sort... */
    Base **sorted_bases;
    size_t num_bases;

    sorted_bases = animdata_filter_ds_sorted_bases(ac, scene, view_layer, filter_mode, &num_bases);
    if (sorted_bases) {
      /* Add the necessary channels for these bases... */
      for (size_t i = 0; i < num_bases; i++) {
        items += animdata_filter_dopesheet_ob(ac, anim_data, sorted_bases[i], filter_mode);
      }

      /* TODO: store something to validate whether any changes are needed? */

      /* free temporary data */
      MEM_freeN(sorted_bases);
    }
  }
  else {
    /* Filter and add contents of each base (i.e. object) without them sorting first
     * NOTE: This saves performance in cases where order doesn't matter
     */
    Object *obact = BKE_view_layer_active_object_get(view_layer);
    const eObjectMode object_mode = (obact != nullptr) ? eObjectMode(obact->mode) : OB_MODE_OBJECT;
    LISTBASE_FOREACH (Base *, base, object_bases) {
      if (animdata_filter_base_is_ok(ac, base, object_mode, filter_mode)) {
        /* since we're still here, this object should be usable */
        items += animdata_filter_dopesheet_ob(ac, anim_data, base, filter_mode);
      }
    }
  }

  /* return the number of items in the list */
  return items;
}

/* Summary track for DopeSheet/Action Editor
 * - return code is whether the summary lets the other channels get drawn
 */
static short animdata_filter_dopesheet_summary(bAnimContext *ac,
                                               ListBase *anim_data,
                                               const eAnimFilter_Flags filter_mode,
                                               size_t *items)
{
  bDopeSheet *ads = nullptr;

  /* get the DopeSheet information to use
   * - we should only need to deal with the DopeSheet/Action Editor,
   *   since all the other Animation Editors won't have this concept
   *   being applicable.
   */
  if ((ac && ac->sl) && (ac->spacetype == SPACE_ACTION)) {
    SpaceAction *saction = reinterpret_cast<SpaceAction *>(ac->sl);
    ads = &saction->ads;
  }
  else {
    /* invalid space type - skip this summary channels */
    return 1;
  }

  if ((filter_mode & ANIMFILTER_LIST_CHANNELS) == 0) {
    /* Without ANIMFILTER_LIST_CHANNELS flag, summary channels should not be created.
     * Sub-channels of this summary should still be visited. */
    return 1;
  }

  /* dopesheet summary
   * - only for drawing and/or selecting keyframes in channels, but not for real editing
   * - only useful for DopeSheet/Action/etc. editors where it is actually useful
   */
  const bool is_timeline = ac->dopesheet_mode == SACTCONT_TIMELINE;
  if (is_timeline || (ac->filters.flag & ADS_FILTER_SUMMARY)) {
    bAnimListElem *ale = make_new_animlistelem(ac->bmain, ac, ANIMTYPE_SUMMARY, nullptr, nullptr);
    if (ale) {
      BLI_addtail(anim_data, ale);
      (*items)++;
    }

    /* If summary is collapsed, don't show other channels beneath this - this check is put inside
     * the summary check so that it doesn't interfere with normal operation.
     */
    if (ads->flag & ADS_FLAG_SUMMARY_COLLAPSED) {
      return 0;
    }
  }

  /* the other channels beneath this can be shown */
  return 1;
}

/* ......................... */

/* filter data associated with a channel - usually for handling summary-channels in DopeSheet */
static size_t animdata_filter_animchan(bAnimContext *ac,
                                       ListBase *anim_data,
                                       bAnimListElem *channel,
                                       const eAnimFilter_Flags filter_mode)
{
  size_t items = 0;

  /* data to filter depends on channel type */
  /* NOTE: only common channel-types have been handled for now. More can be added as necessary */
  switch (channel->type) {
    case ANIMTYPE_SUMMARY:
      items += animdata_filter_dopesheet(ac, anim_data, filter_mode);
      break;

    case ANIMTYPE_SCENE:
      items += animdata_filter_dopesheet_scene(
          ac, anim_data, static_cast<Scene *>(channel->data), filter_mode);
      break;

    case ANIMTYPE_OBJECT:
      items += animdata_filter_dopesheet_ob(
          ac, anim_data, static_cast<Base *>(channel->data), filter_mode);
      break;

    case ANIMTYPE_DSCACHEFILE:
      items += animdata_filter_ds_cachefile(
          ac, anim_data, static_cast<CacheFile *>(channel->data), filter_mode);
      break;

    case ANIMTYPE_ANIMDATA:
      items += animfilter_block_data(ac, anim_data, channel->id, filter_mode);
      break;

    default:
      printf("ERROR: Unsupported channel type (%d) in animdata_filter_animchan()\n",
             channel->type);
      break;
  }

  return items;
}

/* ----------- Cleanup API --------------- */

/* Remove entries with invalid types in animation channel list */
static size_t animdata_filter_remove_invalid(ListBase *anim_data)
{
  size_t items = 0;

  /* only keep entries with valid types */
  LISTBASE_FOREACH_MUTABLE (bAnimListElem *, ale, anim_data) {
    if (ale->type == ANIMTYPE_NONE) {
      BLI_freelinkN(anim_data, ale);
    }
    else {
      items++;
    }
  }

  return items;
}

/* Remove duplicate entries in animation channel list */
static size_t animdata_filter_remove_duplis(ListBase *anim_data)
{
  GSet *gs;
  size_t items = 0;

  /* Build new hash-table to efficiently store and retrieve which entries have been
   * encountered already while searching. */
  gs = BLI_gset_ptr_new(__func__);

  /* loop through items, removing them from the list if a similar item occurs already */
  LISTBASE_FOREACH_MUTABLE (bAnimListElem *, ale, anim_data) {
    /* check if hash has any record of an entry like this
     * - just use ale->data for now, though it would be nicer to involve
     *   ale->type in combination too to capture corner cases
     *   (where same data performs differently)
     */
    if (BLI_gset_add(gs, ale->data)) {
      /* this entry is 'unique' and can be kept */
      items++;
    }
    else {
      /* this entry isn't needed anymore */
      BLI_freelinkN(anim_data, ale);
    }
  }

  /* free the hash... */
  BLI_gset_free(gs, nullptr);

  /* return the number of items still in the list */
  return items;
}

/* ----------- Public API --------------- */

size_t ANIM_animdata_filter(bAnimContext *ac,
                            ListBase *anim_data,
                            const eAnimFilter_Flags filter_mode,
                            void *data,
                            const eAnimCont_Types datatype)
{
  if (!data || !anim_data) {
    return 0;
  }

  size_t items = 0;
  switch (datatype) {
    /* Action-Editing Modes */
    case ANIMCONT_ACTION: /* 'Action Editor' */
    {
      Object *obact = ac->obact;
      SpaceAction *saction = reinterpret_cast<SpaceAction *>(ac->sl);
      bDopeSheet *ads = (saction) ? &saction->ads : nullptr;
      BLI_assert(ads == ac->ads);
      UNUSED_VARS_NDEBUG(ads);

      /* specially check for AnimData filter, see #36687. */
      /* TODO: see how this interacts with the new layered Actions. */
      if (UNLIKELY(filter_mode & ANIMFILTER_ANIMDATA)) {
        /* all channels here are within the same AnimData block, hence this special case */
        if (LIKELY(obact->adt)) {
          ANIMCHANNEL_NEW_CHANNEL(
              ac->bmain, obact->adt, ANIMTYPE_ANIMDATA, reinterpret_cast<ID *>(obact), nullptr);
        }
      }
      else {
        /* The check for the DopeSheet summary is included here
         * since the summary works here too. */
        if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
          BLI_assert_msg(
              obact->adt && data == obact->adt->action,
              "This code assumes the Action editor shows the Action of the active object");

          animrig::Action &action = static_cast<bAction *>(data)->wrap();
          const animrig::slot_handle_t slot_handle = obact->adt->slot_handle;
          items += animfilter_action(
              ac, anim_data, action, slot_handle, filter_mode, reinterpret_cast<ID *>(obact));
        }
      }

      break;
    }
    case ANIMCONT_SHAPEKEY: /* 'ShapeKey Editor' */
    {
      Key *key = static_cast<Key *>(data);

      /* specially check for AnimData filter, see #36687. */
      if (UNLIKELY(filter_mode & ANIMFILTER_ANIMDATA)) {
        /* all channels here are within the same AnimData block, hence this special case */
        if (LIKELY(key->adt)) {
          ANIMCHANNEL_NEW_CHANNEL(
              ac->bmain, key->adt, ANIMTYPE_ANIMDATA, reinterpret_cast<ID *>(key), nullptr);
        }
      }
      else {
        /* The check for the DopeSheet summary is included here
         * since the summary works here too. */
        if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
          items = animdata_filter_shapekey(ac, anim_data, key, filter_mode);
        }
      }

      break;
    }

    /* Modes for Specialty Data Types (i.e. not keyframes) */
    case ANIMCONT_GPENCIL: {
      if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
        items = animdata_filter_grease_pencil(ac, anim_data, filter_mode);
      }
      break;
    }
    case ANIMCONT_MASK: {
      if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
        items = animdata_filter_mask(ac, anim_data, data, filter_mode);
      }
      break;
    }

    /* DopeSheet Based Modes */
    case ANIMCONT_DOPESHEET: /* 'DopeSheet Editor' */
    {
      /* Due to code in `actedit_get_context()`, the equation below holds. The `data` pointer is no
       * longer used here, in favor of always passing `ac` down the call chain. The called code
       * can access it via `ac->ads`. Because the anim filtering code is quite complex, I (Sybren)
       * want to keep this assertion in place. */
      BLI_assert_msg(ac->ads == data, "ANIMCONT_DOPESHEET");

      /* the DopeSheet editor is the primary place where the DopeSheet summaries are useful */
      if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
        items += animdata_filter_dopesheet(ac, anim_data, filter_mode);
      }
      break;
    }
    case ANIMCONT_FCURVES: /* Graph Editor -> F-Curves/Animation Editing */
    case ANIMCONT_DRIVERS: /* Graph Editor -> Drivers Editing */
    case ANIMCONT_NLA:     /* NLA Editor */
    {
      /* Due to code in `actedit_get_context()`, the equation below holds. The `data` pointer is no
       * longer used here, in favor of always passing `ac` down the call chain. The called code
       * can access it via `ac->ads`. Because the anim filtering code is quite complex, I (Sybren)
       * want to keep this assertion in place. */
      BLI_assert_msg(ac->ads == data, "ANIMCONT_FCURVES/DRIVERS/NLA");

      /* all of these editors use the basic DopeSheet data for filtering options,
       * but don't have all the same features */
      items = animdata_filter_dopesheet(ac, anim_data, filter_mode);
      break;
    }

    /* Timeline Mode - Basically the same as dopesheet,
     * except we only have the summary for now */
    case ANIMCONT_TIMELINE: {
      /* Due to code in `actedit_get_context()`, the equation below holds. The `data` pointer is no
       * longer used here, in favor of always passing `ac` down the call chain. The called code
       * can access it via `ac->ads`. Because the anim filtering code is quite complex, I (Sybren)
       * want to keep this assertion in place. */
      BLI_assert_msg(ac->ads == data, "ANIMCONT_TIMELINE");
      if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items)) {
        items += animdata_filter_dopesheet(ac, anim_data, filter_mode);
      }
      break;
    }

    /* Special/Internal Use */
    case ANIMCONT_CHANNEL: /* animation channel */
    {
      /* based on the channel type, filter relevant data for this */
      items = animdata_filter_animchan(
          ac, anim_data, static_cast<bAnimListElem *>(data), filter_mode);
      break;
    }

    case ANIMCONT_NONE:
      printf("ANIM_animdata_filter() - Invalid datatype argument ANIMCONT_NONE\n");
      break;
  }

  /* remove any 'weedy' entries */
  items = animdata_filter_remove_invalid(anim_data);

  /* remove duplicates (if required) */
  if (filter_mode & ANIMFILTER_NODUPLIS) {
    items = animdata_filter_remove_duplis(anim_data);
  }

  return items;
}

/* ************************************************************ */
