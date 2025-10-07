/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_defaults.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_session_uid.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_anim_visualization.h"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_asset.hh"
#include "BKE_constraint.h"
#include "BKE_deform.hh"
#include "BKE_fcurve.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_preview_image.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BIK_api.h"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "BLO_read_write.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_bonecolor.hh"
#include "ANIM_versioning.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"anim.action"};

using namespace blender;

/* *********************** NOTE ON POSE AND ACTION **********************
 *
 * - Pose is the local (object level) component of armature. The current
 *   object pose is saved in files, and (will be) is presorted for dependency
 * - Actions have fewer (or other) channels, and write data to a Pose
 * - Currently ob->pose data is controlled in BKE_pose_where_is only. The (recalc)
 *   event system takes care of calling that
 * - The NLA system (here too) uses Poses as interpolation format for Actions
 * - Therefore we assume poses to be static, and duplicates of poses have channels in
 *   same order, for quick interpolation reasons
 *
 * ****************************** (ton) ************************************ */

/**************************** Action Datablock ******************************/

/*********************** Armature Datablock ***********************/
namespace blender::bke {

static void action_init_data(ID *action_id)
{
  BLI_assert(GS(action_id->name) == ID_AC);
  bAction *action = reinterpret_cast<bAction *>(action_id);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(action, id));
  MEMCPY_STRUCT_AFTER(action, DNA_struct_default_get(bAction), id);
}

/**
 * Only copy internal data of Action ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
static void action_copy_data(Main * /*bmain*/,
                             std::optional<Library *> /*owner_library*/,
                             ID *id_dst,
                             const ID *id_src,
                             const int flag)
{
  bAction *dna_action_dst = reinterpret_cast<bAction *>(id_dst);
  animrig::Action &action_dst = dna_action_dst->wrap();

  const bAction *dna_action_src = reinterpret_cast<const bAction *>(id_src);
  const animrig::Action &action_src = dna_action_src->wrap();

  bActionGroup *group_dst, *group_src;
  FCurve *fcurve_dst, *fcurve_src;

  /* Duplicate the lists of groups and markers. */
  BLI_duplicatelist(&action_dst.groups, &action_src.groups);
  BKE_copy_time_markers(action_dst.markers, action_src.markers, flag);

  /* Copy F-Curves, fixing up the links as we go. */
  BLI_listbase_clear(&action_dst.curves);

  for (fcurve_src = static_cast<FCurve *>(action_src.curves.first); fcurve_src;
       fcurve_src = fcurve_src->next)
  {
    /* Duplicate F-Curve. */

    /* XXX TODO: pass sub-data flag?
     * But surprisingly does not seem to be doing any ID reference-counting. */
    fcurve_dst = BKE_fcurve_copy(fcurve_src);

    BLI_addtail(&action_dst.curves, fcurve_dst);

    /* Fix group links (kind of bad list-in-list search, but this is the most reliable way). */
    for (group_dst = static_cast<bActionGroup *>(action_dst.groups.first),
        group_src = static_cast<bActionGroup *>(action_src.groups.first);
         group_dst && group_src;
         group_dst = group_dst->next, group_src = group_src->next)
    {
      if (fcurve_src->grp == group_src) {
        fcurve_dst->grp = group_dst;

        if (group_dst->channels.first == fcurve_src) {
          group_dst->channels.first = fcurve_dst;
        }
        if (group_dst->channels.last == fcurve_src) {
          group_dst->channels.last = fcurve_dst;
        }
        break;
      }
    }
  }

  /* Copy all simple properties. */
  action_dst.layer_array_num = action_src.layer_array_num;
  action_dst.layer_active_index = action_src.layer_active_index;
  action_dst.slot_array_num = action_src.slot_array_num;
  action_dst.last_slot_handle = action_src.last_slot_handle;

  /* Layers, and (recursively) Strips. */
  action_dst.layer_array = MEM_calloc_arrayN<ActionLayer *>(action_src.layer_array_num, __func__);
  for (int i : action_src.layers().index_range()) {
    action_dst.layer_array[i] = action_src.layer(i)->duplicate_with_shallow_strip_copies(__func__);
  }

  /* Strip data. */
  action_dst.strip_keyframe_data_array = MEM_calloc_arrayN<ActionStripKeyframeData *>(
      action_src.strip_keyframe_data_array_num, __func__);
  for (int i : action_src.strip_keyframe_data().index_range()) {
    action_dst.strip_keyframe_data_array[i] = MEM_new<animrig::StripKeyframeData>(
        __func__, *action_src.strip_keyframe_data()[i]);
  }

  /* Slots. */
  action_dst.slot_array = MEM_calloc_arrayN<ActionSlot *>(action_src.slot_array_num, __func__);
  for (int i : action_src.slots().index_range()) {
    action_dst.slot_array[i] = MEM_new<animrig::Slot>(__func__, *action_src.slot(i));
  }

  if (flag & LIB_ID_COPY_NO_PREVIEW) {
    action_dst.preview = nullptr;
  }
  else {
    BKE_previewimg_id_copy(&action_dst.id, &action_src.id);
  }
}

/** Free (or release) any data used by this action (does not free the action itself). */
static void action_free_data(ID *id)
{
  animrig::Action &action = reinterpret_cast<bAction *>(id)->wrap();

  /* Free keyframe data. */
  for (animrig::StripKeyframeData *keyframe_data : action.strip_keyframe_data()) {
    MEM_delete(keyframe_data);
  }
  MEM_SAFE_FREE(action.strip_keyframe_data_array);
  action.strip_keyframe_data_array_num = 0;

  /* Free layers. */
  for (animrig::Layer *layer : action.layers()) {
    MEM_delete(layer);
  }
  MEM_SAFE_FREE(action.layer_array);
  action.layer_array_num = 0;

  /* Free slots. */
  for (animrig::Slot *slot : action.slots()) {
    MEM_delete(slot);
  }
  MEM_SAFE_FREE(action.slot_array);
  action.slot_array_num = 0;

  /* Free legacy F-Curves & groups. */
  BKE_fcurves_free(&action.curves);
  BLI_freelistN(&action.groups);

  /* Free markers & preview. */
  BLI_freelistN(&action.markers);
  BKE_previewimg_free(&action.preview);

  BLI_assert(action.is_empty());
}

static void action_foreach_id(ID *id, LibraryForeachIDData *data)
{
  animrig::Action &action = reinterpret_cast<bAction *>(id)->wrap();

  /* When this function is called without the IDWALK_READONLY flag, calls to
   * BKE_LIB_FOREACHID_PROCESS_... macros can change ID pointers. ID remapping is the main example
   * of such use.
   *
   * Those ID pointer changes are not guaranteed to be valid, though. For example, the remapping
   * can be used to replace one Mesh with another, but that neither means that the new Mesh is
   * animated with the same Action, nor that the old Mesh is no longer animated by that Action. In
   * other words, the best that can be done is to invalidate the cache.
   *
   * NOTE: early-returns by BKE_LIB_FOREACHID_PROCESS_... macros are forbidden in non-readonly
   * cases (see #IDWALK_RET_STOP_ITER documentation). */

  constexpr LibraryForeachIDCallbackFlag idwalk_flags = IDWALK_CB_NEVER_SELF | IDWALK_CB_LOOPBACK;

  /* Note that `bmain` can be `nullptr`. An example is in
   * `deg_eval_copy_on_write.cc`, function `deg_expand_eval_copy_datablock`. */
  Main *bmain = BKE_lib_query_foreachid_process_main_get(data);

  /* This function should not rebuild the slot user map, because that in turn loops over all IDs.
   * It is really up to the caller to ensure things are clean when the slot user pointers should be
   * reported.
   *
   * For things like ID remapping it's fine to skip the pointers when they're dirty. The next time
   * somebody tries to actually use them, they will be rebuilt anyway. */
  const bool slot_user_cache_is_known_clean = bmain && !bmain->is_action_slot_to_id_map_dirty;

  if (slot_user_cache_is_known_clean) {
    bool should_invalidate = false;
    for (animrig::Slot *slot : action.slots()) {
      for (ID *&slot_user : slot->runtime_users()) {
        ID *const old_pointer = slot_user;
        BKE_LIB_FOREACHID_PROCESS_ID(data, slot_user, idwalk_flags);
        /* If slot_user changed, the cache should be invalidated. Not all pointer changes are
         * semantically correct for our use. For example, when ID-remapping is used to replace
         * MECube with MESuzanne. If MECube is animated by some slot before the remap, it will
         * remain animated by that slot after the remap, even when all `object->data` pointers now
         * reference MESuzanne instead. */
        should_invalidate |= (slot_user != old_pointer);
      }
    }

    if (should_invalidate) {
      animrig::Slot::users_invalidate(*bmain);
    }

#ifndef NDEBUG
    const LibraryForeachIDFlag flag = BKE_lib_query_foreachid_process_flags_get(data);
    const bool is_readonly = flag & IDWALK_READONLY;
    if (is_readonly) {
      BLI_assert_msg(!should_invalidate,
                     "pointers were changed while IDWALK_READONLY flag was set");
    }
#endif
  }

  /* Note that, even though `BKE_fcurve_foreach_id()` exists, it is not called here. That function
   * is only relevant for drivers, but the F-Curves stored in an Action are always just animation
   * data, not drivers. */

  LISTBASE_FOREACH (TimeMarker *, marker, &action.markers) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, marker->camera, IDWALK_CB_NOP);
  }
}

static void write_channelbag(BlendWriter *writer, animrig::Channelbag &channelbag)
{
  BLO_write_struct(writer, ActionChannelbag, &channelbag);

  Span<bActionGroup *> groups = channelbag.channel_groups();
  BLO_write_pointer_array(writer, groups.size(), groups.data());
  for (const bActionGroup *group : groups) {
    BLO_write_struct(writer, bActionGroup, group);
  }

  Span<FCurve *> fcurves = channelbag.fcurves();
  BLO_write_pointer_array(writer, fcurves.size(), fcurves.data());
  for (FCurve *fcurve : fcurves) {
    BLO_write_struct(writer, FCurve, fcurve);
    BKE_fcurve_blend_write_data(writer, fcurve);
  }
}

static void write_strip_keyframe_data(BlendWriter *writer,
                                      animrig::StripKeyframeData &strip_keyframe_data)
{
  BLO_write_struct(writer, ActionStripKeyframeData, &strip_keyframe_data);

  auto channelbags = strip_keyframe_data.channelbags();
  BLO_write_pointer_array(writer, channelbags.size(), channelbags.data());

  for (animrig::Channelbag *channelbag : channelbags) {
    write_channelbag(writer, *channelbag);
  }
}

static void write_strip_keyframe_data_array(
    BlendWriter *writer, Span<animrig::StripKeyframeData *> strip_keyframe_data_array)
{
  BLO_write_pointer_array(
      writer, strip_keyframe_data_array.size(), strip_keyframe_data_array.data());

  for (animrig::StripKeyframeData *keyframe_data : strip_keyframe_data_array) {
    write_strip_keyframe_data(writer, *keyframe_data);
  }
}

static void write_strips(BlendWriter *writer, Span<animrig::Strip *> strips)
{
  BLO_write_pointer_array(writer, strips.size(), strips.data());

  for (animrig::Strip *strip : strips) {
    BLO_write_struct(writer, ActionStrip, strip);
  }
}

static void write_layers(BlendWriter *writer, Span<animrig::Layer *> layers)
{
  BLO_write_pointer_array(writer, layers.size(), layers.data());

  for (animrig::Layer *layer : layers) {
    BLO_write_struct(writer, ActionLayer, layer);
    write_strips(writer, layer->strips());
  }
}

static void write_slots(BlendWriter *writer, Span<animrig::Slot *> slots)
{
  BLO_write_pointer_array(writer, slots.size(), slots.data());
  for (animrig::Slot *slot : slots) {
    /* Make a shallow copy using the C type, so that no new runtime struct is
     * allocated for the copy. */
    ActionSlot shallow_copy = *slot;
    shallow_copy.runtime = nullptr;

    BLO_write_struct_at_address(writer, ActionSlot, slot, &shallow_copy);
  }
}

/**
 * Create a listbase from a Span of channel groups.
 *
 * \note this does NOT transfer ownership of the pointers. The ListBase should
 * not be freed, but given to
 * `action_blend_write_clear_legacy_channel_groups_listbase()` below.
 *
 * \warning This code is modifying actual '`Main`' data in-place, which is
 * usually not acceptable (due to risks of unsafe concurrent accesses mainly).
 * The reasons why this is currently seen as 'reasonably safe' are:
 *   - Current blender code is _not_ expected to access the affected bActionGroup data
 *     (`prev`/`next` listbase pointers) in any way, as they are stored in an array.
 *   - The `action.groups` listbase modification is safe/valid, as this is a member of
 *     the Action ID, which is a shallow copy of the actual ID data from Main.
 */
static void action_blend_write_make_legacy_channel_groups_listbase(
    ListBase &listbase, const Span<bActionGroup *> channel_groups)
{
  if (channel_groups.is_empty()) {
    BLI_listbase_clear(&listbase);
    return;
  }

  /* Set the fcurve listbase pointers.
   *
   * Note that the fcurves' own prev/next pointers are hooked up by
   * `action_blend_write_make_legacy_fcurves_listbase()`, so that they function
   * properly as a list. */
  for (bActionGroup *group : channel_groups) {
    Span<FCurve *> fcurves = group->wrap().fcurves();
    if (fcurves.is_empty()) {
      group->channels = {nullptr, nullptr};
    }
    else {
      group->channels = {fcurves.first(), fcurves.last()};
    }
  }

  /* Determine the prev/next pointers on the elements. */
  const int last_index = channel_groups.size() - 1;
  for (int index : channel_groups.index_range()) {
    channel_groups[index]->prev = (index > 0) ? channel_groups[index - 1] : nullptr;
    channel_groups[index]->next = (index < last_index) ? channel_groups[index + 1] : nullptr;
  }

  listbase.first = channel_groups[0];
  listbase.last = channel_groups[last_index];
}

static void action_blend_write_clear_legacy_channel_groups_listbase(ListBase &listbase)
{
  LISTBASE_FOREACH_MUTABLE (bActionGroup *, group, &listbase) {
    group->prev = nullptr;
    group->next = nullptr;
    group->channels = {nullptr, nullptr};
  }

  BLI_listbase_clear(&listbase);
}

/**
 * Create a listbase from a Span of F-Curves.
 *
 * \note this does NOT transfer ownership of the pointers. The ListBase should not be freed,
 * but given to `action_blend_write_clear_legacy_fcurves_listbase()` below.
 *
 * \warning This code is modifying actual '`Main`' data in-place, which is
 * usually not acceptable (due to risks of unsafe concurrent accesses mainly).
 * The reasons why this is currently seen as 'reasonably safe' are:
 *   - Current blender code is _not_ expected to access the affected FCurve data
 *     (`prev`/`next` listbase pointers) in any way, as they are stored in an array.
 *   - The `action.curves` listbase modification is safe/valid, as this is a member of
 *     the Action ID, which is a shallow copy of the actual ID data from Main.
 */
static void action_blend_write_make_legacy_fcurves_listbase(ListBase &listbase,
                                                            const Span<FCurve *> fcurves)
{
  if (fcurves.is_empty()) {
    BLI_listbase_clear(&listbase);
    return;
  }

  /* Determine the prev/next pointers on the elements. */
  const int last_index = fcurves.size() - 1;
  for (int index : fcurves.index_range()) {
    fcurves[index]->prev = (index > 0) ? fcurves[index - 1] : nullptr;
    fcurves[index]->next = (index < last_index) ? fcurves[index + 1] : nullptr;
  }

  listbase.first = fcurves[0];
  listbase.last = fcurves[last_index];
}

static void action_blend_write_clear_legacy_fcurves_listbase(ListBase &listbase)
{
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcurve, &listbase) {
    fcurve->prev = nullptr;
    fcurve->next = nullptr;
  }

  BLI_listbase_clear(&listbase);
}

static void action_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  animrig::Action &action = reinterpret_cast<bAction *>(id)->wrap();

  /* Create legacy data for Layered Actions: the F-Curves from the first Slot,
   * bottom layer, first Keyframe strip. */
  const bool do_write_forward_compat = !BLO_write_is_undo(writer) && action.slot_array_num > 0 &&
                                       action.is_action_layered();
  if (do_write_forward_compat) {
    animrig::assert_baklava_phase_1_invariants(action);
    BLI_assert_msg(BLI_listbase_is_empty(&action.curves),
                   "Layered Action should not have legacy data");
    BLI_assert_msg(BLI_listbase_is_empty(&action.groups),
                   "Layered Action should not have legacy data");

    const animrig::Slot &first_slot = *action.slot(0);

    /* The forward-compat animation data we write is for IDs of the type that
     * the first slot is intended for. Therefore, the Action should have that
     * `idroot` when loaded in old versions of Blender.
     *
     * Note that if there is no slot, this code will never run and therefore the
     * action will be written with `idroot = 0`. Despite that, old
     * pre-slotted-action files are still guaranteed to round-trip losslessly,
     * because old actions (even when empty) are versioned to have one slot with
     * `idtype` set to whatever the old action's `idroot` was. In other words,
     * zero-slot actions can only be created via non-legacy features, and
     * therefore represent animation data that wasn't purely from old files
     * anyway. */
    action.idroot = first_slot.idtype;

    /* Note: channel group forward-compat data requires that fcurve
     * forward-compat legacy data is also written, and vice-versa. Both have
     * pointers to each other that won't resolve properly when loaded in older
     * Blender versions if only one is written. */
    animrig::Channelbag *bag = channelbag_for_action_slot(action, first_slot.handle);
    if (bag) {
      action_blend_write_make_legacy_fcurves_listbase(action.curves, bag->fcurves());
      action_blend_write_make_legacy_channel_groups_listbase(action.groups, bag->channel_groups());
    }
  }

  BLO_write_id_struct(writer, bAction, id_address, &action.id);
  BKE_id_blend_write(writer, &action.id);

  /* Write layered Action data. */
  write_strip_keyframe_data_array(writer, action.strip_keyframe_data());
  write_layers(writer, action.layers());
  write_slots(writer, action.slots());

  if (do_write_forward_compat) {
    /* Set the idroot back to 'unspecified', as it always should be for layered
     * Actions. */
    action.idroot = 0;

    /* The pointers to the first/last FCurve in the `action.curves` have already
     * been written as part of the Action struct data, so they can be cleared
     * here, such that the code writing legacy fcurves below does nothing (as
     * expected). And to leave the Action in a consistent state (it shouldn't
     * have F-Curves in both legacy and layered storage).
     *
     * Note that the FCurves themselves have been written as part of the layered
     * animation writing code called above. Writing them again as part of the
     * handling of the legacy `action.fcurves` ListBase would corrupt the
     * blend-file by generating two `BHead` `DATA` blocks with the same old
     * address for the same ID.
     */
    action_blend_write_clear_legacy_channel_groups_listbase(action.groups);
    action_blend_write_clear_legacy_fcurves_listbase(action.curves);
  }

  /* Write legacy F-Curves & Groups. */
  BKE_fcurve_blend_write_listbase(writer, &action.curves);
  LISTBASE_FOREACH (bActionGroup *, grp, &action.groups) {
    BLO_write_struct(writer, bActionGroup, grp);
  }

  BKE_time_markers_blend_write(writer, action.markers);

  BKE_previewimg_blend_write(writer, action.preview);
}

static void read_channelbag(BlendDataReader *reader, animrig::Channelbag &channelbag)
{
  BLO_read_pointer_array(
      reader, channelbag.group_array_num, reinterpret_cast<void **>(&channelbag.group_array));
  for (int i = 0; i < channelbag.group_array_num; i++) {
    BLO_read_struct(reader, bActionGroup, &channelbag.group_array[i]);
    channelbag.group_array[i]->channelbag = &channelbag;

    /* Clear the legacy channels #ListBase, since it will have been set for some
     * groups for forward compatibility.
     * See #action_blend_write_make_legacy_channel_groups_listbase. */
    channelbag.group_array[i]->channels = {nullptr, nullptr};
  }

  BLO_read_pointer_array(
      reader, channelbag.fcurve_array_num, reinterpret_cast<void **>(&channelbag.fcurve_array));
  for (int i = 0; i < channelbag.fcurve_array_num; i++) {
    BLO_read_struct(reader, FCurve, &channelbag.fcurve_array[i]);
    FCurve *fcurve = channelbag.fcurve_array[i];

    /* Clear the prev/next pointers set by the forward compatibility code in
     * action_blend_write(). */
    fcurve->prev = nullptr;
    fcurve->next = nullptr;

    BKE_fcurve_blend_read_data(reader, fcurve);
  }
}

static void read_strip_keyframe_data(BlendDataReader *reader,
                                     animrig::StripKeyframeData &strip_keyframe_data)
{
  BLO_read_pointer_array(reader,
                         strip_keyframe_data.channelbag_array_num,
                         reinterpret_cast<void **>(&strip_keyframe_data.channelbag_array));

  for (int i = 0; i < strip_keyframe_data.channelbag_array_num; i++) {
    BLO_read_struct(reader, ActionChannelbag, &strip_keyframe_data.channelbag_array[i]);
    ActionChannelbag *channelbag = strip_keyframe_data.channelbag_array[i];
    read_channelbag(reader, channelbag->wrap());
  }
}

static void read_strip_keyframe_data_array(BlendDataReader *reader, animrig::Action &action)
{
  BLO_read_pointer_array(reader,
                         action.strip_keyframe_data_array_num,
                         reinterpret_cast<void **>(&action.strip_keyframe_data_array));

  for (int i = 0; i < action.strip_keyframe_data_array_num; i++) {
    BLO_read_struct(reader, ActionStripKeyframeData, &action.strip_keyframe_data_array[i]);
    ActionStripKeyframeData *keyframe_data = action.strip_keyframe_data_array[i];
    read_strip_keyframe_data(reader, keyframe_data->wrap());
  }
}

static void read_layers(BlendDataReader *reader, animrig::Action &action)
{
  BLO_read_pointer_array(
      reader, action.layer_array_num, reinterpret_cast<void **>(&action.layer_array));

  for (int layer_idx = 0; layer_idx < action.layer_array_num; layer_idx++) {
    BLO_read_struct(reader, ActionLayer, &action.layer_array[layer_idx]);
    ActionLayer *layer = action.layer_array[layer_idx];

    BLO_read_pointer_array(
        reader, layer->strip_array_num, reinterpret_cast<void **>(&layer->strip_array));
    for (int strip_idx = 0; strip_idx < layer->strip_array_num; strip_idx++) {
      BLO_read_struct(reader, ActionStrip, &layer->strip_array[strip_idx]);

      /* This if statement and the code in it is only for a transitional period
       * while we land #126559 and for a while after, to prevent crashes for
       * people that were already playing with slotted actions and have some
       * blend files written with them. This code can be removed after a while.
       * At the very least, if you're reading this and slotted actions are
       * already in an official release of Blender then this code is no longer
       * relevant and can be deleted. */
      if (layer->strip_array[strip_idx] == nullptr) {
        layer->strip_array[strip_idx] = &animrig::Strip::create(action,
                                                                animrig::Strip::Type::Keyframe);
      }
    }
  }
}

static void read_slots(BlendDataReader *reader, animrig::Action &action)
{
  BLO_read_pointer_array(
      reader, action.slot_array_num, reinterpret_cast<void **>(&action.slot_array));

  for (int i = 0; i < action.slot_array_num; i++) {
    BLO_read_struct(reader, ActionSlot, &action.slot_array[i]);

    /* NOTE: this is endianness-sensitive. */
    /* In case of required endian switching, this code would have to undo the generic endian
     * switching, as the ID type values are not numerically the same between little and big endian
     * machines. Due to the way they are defined, they are always in the same byte order,
     * regardless of hardware/platform endianness. */

    action.slot_array[i]->wrap().blend_read_post();
  }
}

static void action_blend_read_data(BlendDataReader *reader, ID *id)
{
  animrig::Action &action = reinterpret_cast<bAction *>(id)->wrap();

  /* NOTE: this is endianness-sensitive. */
  /* In case of required endianness switching, this code would need to undo the generic endian
   * switching (careful, only the two least significant bytes of the int32 must be swapped back
   * here, since this value is actually an int16). */

  read_strip_keyframe_data_array(reader, action);
  read_layers(reader, action);
  read_slots(reader, action);

  if (animrig::versioning::action_is_layered(action)) {
    /* Clear the forward-compatible storage (see action_blend_write_data()). */
    BLI_listbase_clear(&action.curves);
    BLI_listbase_clear(&action.groups);

    /* Layered actions should always have `idroot == 0`, but when writing an
     * action to a blend file `idroot` is typically set otherwise for forward
     * compatibility reasons (see `action_blend_write()`). So we set it to zero
     * here to put it back as it should be. */
    action.idroot = 0;
  }
  else {
    /* Read legacy data. */
    BLO_read_struct_list(reader, FCurve, &action.curves);
    BLO_read_struct_list(reader, bActionGroup, &action.groups);

    BKE_fcurve_blend_read_data_listbase(reader, &action.curves);

    LISTBASE_FOREACH (bActionGroup *, agrp, &action.groups) {
      BLO_read_struct(reader, FCurve, &agrp->channels.first);
      BLO_read_struct(reader, FCurve, &agrp->channels.last);
    }
  }

  BKE_time_markers_blend_read(reader, action.markers);

  /* End of reading legacy data. */

  BLO_read_struct(reader, PreviewImage, &action.preview);
  BKE_previewimg_blend_read(reader, action.preview);
}

static IDProperty *action_asset_type_property(const bAction *action)
{
  using namespace blender;
  const bool is_single_frame = action && action->wrap().has_single_frame();
  return bke::idprop::create("is_single_frame", int(is_single_frame)).release();
}

static void action_asset_metadata_ensure(void *asset_ptr, AssetMetaData *asset_data)
{
  bAction *action = (bAction *)asset_ptr;
  BLI_assert(GS(action->id.name) == ID_AC);

  IDProperty *action_type = action_asset_type_property(action);
  BKE_asset_metadata_idprop_ensure(asset_data, action_type);
}

static AssetTypeInfo AssetType_AC = {
    /*pre_save_fn*/ action_asset_metadata_ensure,
    /*on_mark_asset_fn*/ action_asset_metadata_ensure,
    /*on_clear_asset_fn*/ nullptr,
};

}  // namespace blender::bke

IDTypeInfo IDType_ID_AC = {
    /*id_code*/ bAction::id_type,
    /*id_filter*/ FILTER_ID_AC,

    /* This value will be set dynamically in `BKE_idtype_init()` to only include
     * animatable ID types (see `animrig::Slot::users()`). */
    /*dependencies_id_types*/ FILTER_ID_ALL,

    /*main_listbase_index*/ INDEX_ID_AC,
    /*struct_size*/ sizeof(bAction),
    /*name*/ "Action",
    /*name_plural*/ "actions",
    /*translation_context*/ BLT_I18NCONTEXT_ID_ACTION,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA,
    /*asset_type_info*/ &blender::bke::AssetType_AC,

    /*init_data*/ blender::bke::action_init_data,
    /*copy_data*/ blender::bke::action_copy_data,
    /*free_data*/ blender::bke::action_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ blender::bke::action_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ blender::bke::action_blend_write,
    /*blend_read_data*/ blender::bke::action_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/* ***************** Library data level operations on action ************** */

bAction *BKE_action_add(Main *bmain, const char name[])
{
  bAction *act;

  act = BKE_id_new<bAction>(bmain, name);

  return act;
}

/* .................................. */

/* *************** Action Groups *************** */

bActionGroup *get_active_actiongroup(bAction *act)
{
  /* TODO: move this logic to the animrig::Channelbag struct and unify with code
   * that uses direct access to the flags. */
  for (bActionGroup *agrp : animrig::legacy::channel_groups_all(act)) {
    if (agrp->flag & AGRP_ACTIVE) {
      return agrp;
    }
  }
  return nullptr;
}

void set_active_action_group(bAction *act, bActionGroup *agrp, short select)
{
  /* TODO: move this logic to the animrig::Channelbag struct and unify with code
   * that uses direct access to the flags. */
  for (bActionGroup *grp : animrig::legacy::channel_groups_all(act)) {
    if ((grp == agrp) && (select)) {
      grp->flag |= AGRP_ACTIVE;
    }
    else {
      grp->flag &= ~AGRP_ACTIVE;
    }
  }
}

void action_group_colors_sync(bActionGroup *grp, const bActionGroup *ref_grp)
{
  /* Only do color copying if using a custom color (i.e. not default color). */
  if (grp->customCol) {
    if (grp->customCol > 0) {
      /* copy theme colors on-to group's custom color in case user tries to edit color */
      const bTheme *btheme = static_cast<const bTheme *>(U.themes.first);
      const ThemeWireColor *col_set = &btheme->tarm[(grp->customCol - 1)];

      memcpy(&grp->cs, col_set, sizeof(ThemeWireColor));
    }
    else {
      /* if a reference group is provided, use the custom color from there... */
      if (ref_grp) {
        /* assumption: reference group has a color set */
        memcpy(&grp->cs, &ref_grp->cs, sizeof(ThemeWireColor));
      }
      /* otherwise, init custom color with a generic/placeholder color set if
       * no previous theme color was used that we can just keep using
       */
      else if (grp->cs.solid[0] == 0) {
        /* define for setting colors in theme below */
        rgba_uchar_args_set(grp->cs.solid, 0xff, 0x00, 0x00, 255);
        rgba_uchar_args_set(grp->cs.select, 0x81, 0xe6, 0x14, 255);
        rgba_uchar_args_set(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
      }
    }
  }
}

void action_group_colors_set_from_posebone(bActionGroup *grp, const bPoseChannel *pchan)
{
  BLI_assert_msg(pchan, "cannot 'set action group colors from posebone' without a posebone");
  if (!pchan->bone) {
    /* pchan->bone is only set after leaving editmode. */
    return;
  }

  const BoneColor &color = blender::animrig::ANIM_bonecolor_posebone_get(pchan);
  action_group_colors_set(grp, &color);
}

void action_group_colors_set(bActionGroup *grp, const BoneColor *color)
{
  const blender::animrig::BoneColor &bone_color = color->wrap();

  grp->customCol = int(bone_color.palette_index);

  const ThemeWireColor *effective_color = bone_color.effective_color();
  if (effective_color) {
    /* The drawing code assumes that grp->cs always contains the effective
     * color. This is why the effective color is always written to it, and why
     * the above action_group_colors_sync() function exists: it needs to update
     * grp->cs in case the theme changes. */
    memcpy(&grp->cs, effective_color, sizeof(grp->cs));
  }
}

bActionGroup *action_groups_add_new(bAction *act, const char name[])
{
  bActionGroup *agrp;

  /* sanity check: must have action and name */
  if (ELEM(nullptr, act, name)) {
    return nullptr;
  }

  BLI_assert(act->wrap().is_action_legacy());

  /* allocate a new one */
  agrp = MEM_callocN<bActionGroup>("bActionGroup");

  /* make it selected, with default name */
  agrp->flag = AGRP_SELECTED;
  STRNCPY_UTF8(agrp->name, name[0] ? name : DATA_("Group"));

  /* add to action, and validate */
  BLI_addtail(&act->groups, agrp);
  BLI_uniquename(
      &act->groups, agrp, DATA_("Group"), '.', offsetof(bActionGroup, name), sizeof(agrp->name));

  /* return the new group */
  return agrp;
}

void action_groups_add_channel(bAction *act, bActionGroup *agrp, FCurve *fcurve)
{
  /* sanity checks */
  if (ELEM(nullptr, act, agrp, fcurve)) {
    return;
  }

  BLI_assert(act->wrap().is_action_legacy());

  /* if no channels anywhere, just add to two lists at the same time */
  if (BLI_listbase_is_empty(&act->curves)) {
    fcurve->next = fcurve->prev = nullptr;

    agrp->channels.first = agrp->channels.last = fcurve;
    act->curves.first = act->curves.last = fcurve;
  }

  /* if the group already has channels, the F-Curve can simply be added to the list
   * (i.e. as the last channel in the group)
   */
  else if (agrp->channels.first) {
    /* if the group's last F-Curve is the action's last F-Curve too,
     * then set the F-Curve as the last for the action first so that
     * the lists will be in sync after linking
     */
    if (agrp->channels.last == act->curves.last) {
      act->curves.last = fcurve;
    }

    /* link in the given F-Curve after the last F-Curve in the group,
     * which means that it should be able to fit in with the rest of the
     * list seamlessly
     */
    BLI_insertlinkafter(&agrp->channels, agrp->channels.last, fcurve);
  }

  /* otherwise, need to find the nearest F-Curve in group before/after current to link with */
  else {
    bActionGroup *grp;

    /* firstly, link this F-Curve to the group */
    agrp->channels.first = agrp->channels.last = fcurve;

    /* Step through the groups preceding this one,
     * finding the F-Curve there to attach this one after. */
    for (grp = agrp->prev; grp; grp = grp->prev) {
      /* if this group has F-Curves, we want weave the given one in right after the last channel
       * there, but via the Action's list not this group's list
       * - this is so that the F-Curve is in the right place in the Action,
       *   but won't be included in the previous group.
       */
      if (grp->channels.last) {
        /* once we've added, break here since we don't need to search any further... */
        BLI_insertlinkafter(&act->curves, grp->channels.last, fcurve);
        break;
      }
    }

    /* If grp is nullptr, that means we fell through, and this F-Curve should be added as the new
     * first since group is (effectively) the first group. Thus, the existing first F-Curve becomes
     * the second in the chain, etc. */
    if (grp == nullptr) {
      BLI_insertlinkbefore(&act->curves, act->curves.first, fcurve);
    }
  }

  /* set the F-Curve's new group */
  fcurve->grp = agrp;
}

void BKE_action_groups_reconstruct(bAction *act)
{
  /* Sanity check. */
  if (!act) {
    return;
  }

  if (BLI_listbase_is_empty(&act->groups)) {
    /* NOTE: this also includes layered Actions, as act->groups is the legacy storage for groups.
     * Layered Actions should never have to deal with 'reconstructing' groups, as arbitrarily
     * shuffling of the underlying data isn't allowed, and the available methods for modifying
     * F-Curves/Groups already ensure that the data is valid when they return. */
    return;
  }

  BLI_assert(act->wrap().is_action_legacy());

  /* Clear out all group channels. Channels that are actually in use are
   * reconstructed below; this step is necessary to clear out unused groups. */
  LISTBASE_FOREACH (bActionGroup *, group, &act->groups) {
    BLI_listbase_clear(&group->channels);
  }

  /* Sort the channels into the group lists, destroying the act->curves list. */
  ListBase ungrouped = {nullptr, nullptr};

  LISTBASE_FOREACH_MUTABLE (FCurve *, fcurve, &act->curves) {
    if (fcurve->grp) {
      BLI_assert(BLI_findindex(&act->groups, fcurve->grp) >= 0);

      BLI_addtail(&fcurve->grp->channels, fcurve);
    }
    else {
      BLI_addtail(&ungrouped, fcurve);
    }
  }

  /* Recombine into the main list. */
  BLI_listbase_clear(&act->curves);

  LISTBASE_FOREACH (bActionGroup *, group, &act->groups) {
    /* Copy the list header to preserve the pointers in the group. */
    ListBase tmp = group->channels;
    BLI_movelisttolist(&act->curves, &tmp);
  }

  BLI_movelisttolist(&act->curves, &ungrouped);
}

void action_groups_remove_channel(bAction *act, FCurve *fcu)
{
  /* sanity checks */
  if (ELEM(nullptr, act, fcu)) {
    return;
  }

  BLI_assert(act->wrap().is_action_legacy());

  /* check if any group used this directly */
  if (fcu->grp) {
    bActionGroup *agrp = fcu->grp;

    if (agrp->channels.first == agrp->channels.last) {
      if (agrp->channels.first == fcu) {
        BLI_listbase_clear(&agrp->channels);
      }
    }
    else if (agrp->channels.first == fcu) {
      if ((fcu->next) && (fcu->next->grp == agrp)) {
        agrp->channels.first = fcu->next;
      }
      else {
        agrp->channels.first = nullptr;
      }
    }
    else if (agrp->channels.last == fcu) {
      if ((fcu->prev) && (fcu->prev->grp == agrp)) {
        agrp->channels.last = fcu->prev;
      }
      else {
        agrp->channels.last = nullptr;
      }
    }

    fcu->grp = nullptr;
  }

  /* now just remove from list */
  BLI_remlink(&act->curves, fcu);
}

bActionGroup *BKE_action_group_find_name(bAction *act, const char name[])
{
  /* sanity checks */
  if (ELEM(nullptr, act, act->groups.first, name) || (name[0] == 0)) {
    return nullptr;
  }

  BLI_assert(act->wrap().is_action_legacy());

  /* do string comparisons */
  return static_cast<bActionGroup *>(
      BLI_findstring(&act->groups, name, offsetof(bActionGroup, name)));
}

void action_groups_clear_tempflags(bAction *act)
{
  for (bActionGroup *agrp : animrig::legacy::channel_groups_all(act)) {
    agrp->flag &= ~AGRP_TEMP;
  }
}

/* *************** Pose channels *************** */

void BKE_pose_channel_session_uid_generate(bPoseChannel *pchan)
{
  pchan->runtime.session_uid = BLI_session_uid_generate();
}

bPoseChannel *BKE_pose_channel_find_name(const bPose *pose, const char *name)
{
  if (ELEM(nullptr, pose, name) || (name[0] == '\0')) {
    return nullptr;
  }

  if (pose->chanhash) {
    return static_cast<bPoseChannel *>(BLI_ghash_lookup(pose->chanhash, (const void *)name));
  }

  return static_cast<bPoseChannel *>(
      BLI_findstring(&pose->chanbase, name, offsetof(bPoseChannel, name)));
}

bPoseChannel *BKE_pose_channel_ensure(bPose *pose, const char *name)
{
  bPoseChannel *chan;

  if (pose == nullptr) {
    return nullptr;
  }

  /* See if this channel exists */
  chan = BKE_pose_channel_find_name(pose, name);
  if (chan) {
    return chan;
  }

  /* If not, create it and add it */
  chan = MEM_callocN<bPoseChannel>("verifyPoseChannel");

  BKE_pose_channel_session_uid_generate(chan);

  STRNCPY_UTF8(chan->name, name);

  copy_v3_fl(chan->custom_scale_xyz, 1.0f);
  zero_v3(chan->custom_translation);
  zero_v3(chan->custom_rotation_euler);
  chan->custom_shape_wire_width = 1.0f;

  /* init vars to prevent math errors */
  unit_qt(chan->quat);
  unit_axis_angle(chan->rotAxis, &chan->rotAngle);
  chan->scale[0] = chan->scale[1] = chan->scale[2] = 1.0f;

  copy_v3_fl(chan->scale_in, 1.0f);
  copy_v3_fl(chan->scale_out, 1.0f);

  chan->limitmin[0] = chan->limitmin[1] = chan->limitmin[2] = -M_PI;
  chan->limitmax[0] = chan->limitmax[1] = chan->limitmax[2] = M_PI;
  chan->stiffness[0] = chan->stiffness[1] = chan->stiffness[2] = 0.0f;
  chan->ikrotweight = chan->iklinweight = 0.0f;
  unit_m4(chan->constinv);

  chan->protectflag = OB_LOCK_ROT4D; /* lock by components by default */

  BLI_addtail(&pose->chanbase, chan);
  if (pose->chanhash) {
    BLI_ghash_insert(pose->chanhash, chan->name, chan);
  }

  return chan;
}

#ifndef NDEBUG
bool BKE_pose_channels_is_valid(const bPose *pose)
{
  if (pose->chanhash) {
    bPoseChannel *pchan;
    for (pchan = static_cast<bPoseChannel *>(pose->chanbase.first); pchan; pchan = pchan->next) {
      if (BLI_ghash_lookup(pose->chanhash, pchan->name) != pchan) {
        return false;
      }
    }
  }

  return true;
}

#endif

bool BKE_pose_is_bonecoll_visible(const bArmature *arm, const bPoseChannel *pchan)
{
  return pchan->bone && ANIM_bone_in_visible_collection(arm, pchan->bone);
}

bPoseChannel *BKE_pose_channel_active(Object *ob, const bool check_bonecoll)
{
  bArmature *arm = static_cast<bArmature *>((ob) ? ob->data : nullptr);
  if (ELEM(nullptr, ob, ob->pose, arm)) {
    return nullptr;
  }

  /* find active */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if ((pchan->bone) && (pchan->bone == arm->act_bone)) {
      if (!check_bonecoll || ANIM_bone_in_visible_collection(arm, pchan->bone)) {
        return pchan;
      }
    }
  }

  return nullptr;
}

bPoseChannel *BKE_pose_channel_active_if_bonecoll_visible(Object *ob)
{
  return BKE_pose_channel_active(ob, true);
}

bPoseChannel *BKE_pose_channel_active_or_first_selected(Object *ob)
{
  bArmature *arm = static_cast<bArmature *>((ob) ? ob->data : nullptr);

  if (ELEM(nullptr, ob, ob->pose, arm)) {
    return nullptr;
  }

  bPoseChannel *pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);
  if (pchan && blender::animrig::bone_is_selected(arm, pchan)) {
    return pchan;
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if (pchan->bone != nullptr) {
      if (blender::animrig::bone_is_selected(arm, pchan)) {
        return pchan;
      }
    }
  }
  return nullptr;
}

bPoseChannel *BKE_pose_channel_get_mirrored(const bPose *pose, const char *name)
{
  char name_flip[MAXBONENAME];

  BLI_string_flip_side_name(name_flip, name, false, sizeof(name_flip));

  if (!STREQ(name_flip, name)) {
    return BKE_pose_channel_find_name(pose, name_flip);
  }

  return nullptr;
}

const char *BKE_pose_ikparam_get_name(bPose *pose)
{
  if (pose) {
    switch (pose->iksolver) {
      case IKSOLVER_STANDARD:
        return nullptr;
      case IKSOLVER_ITASC:
        return "bItasc";
    }
  }
  return nullptr;
}

void BKE_pose_copy_data_ex(bPose **dst,
                           const bPose *src,
                           const int flag,
                           const bool copy_constraints)
{
  bPose *outPose;
  ListBase listb;

  if (!src) {
    *dst = nullptr;
    return;
  }

  outPose = MEM_callocN<bPose>("pose");

  BLI_duplicatelist(&outPose->chanbase, &src->chanbase);

  /* Rebuild ghash here too, so that name lookups below won't be too bad...
   * BUT this will have the penalty that the ghash will be built twice
   * if BKE_pose_rebuild() gets called after this...
   */
  if (outPose->chanbase.first != outPose->chanbase.last) {
    outPose->chanhash = nullptr;
    BKE_pose_channels_hash_ensure(outPose);
  }

  outPose->iksolver = src->iksolver;
  outPose->ikdata = nullptr;
  outPose->ikparam = MEM_dupallocN(src->ikparam);
  outPose->avs = src->avs;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &outPose->chanbase) {
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)pchan->custom);
    }

    if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
      BKE_pose_channel_session_uid_generate(pchan);
    }

    /* warning, O(n2) here, if done without the hash, but these are rarely used features. */
    if (pchan->custom_tx) {
      pchan->custom_tx = BKE_pose_channel_find_name(outPose, pchan->custom_tx->name);
    }
    if (pchan->bbone_prev) {
      pchan->bbone_prev = BKE_pose_channel_find_name(outPose, pchan->bbone_prev->name);
    }
    if (pchan->bbone_next) {
      pchan->bbone_next = BKE_pose_channel_find_name(outPose, pchan->bbone_next->name);
    }

    if (copy_constraints) {
      /* #BKE_constraints_copy nullptr's `listb` */
      BKE_constraints_copy_ex(&listb, &pchan->constraints, flag, true);

      pchan->constraints = listb;

      /* XXX: This is needed for motionpath drawing to work.
       * Dunno why it was setting to null before... */
      pchan->mpath = animviz_copy_motionpath(pchan->mpath);
    }

    if (pchan->prop) {
      pchan->prop = IDP_CopyProperty_ex(pchan->prop, flag);
    }
    if (pchan->system_properties) {
      pchan->system_properties = IDP_CopyProperty_ex(pchan->system_properties, flag);
    }

    pchan->draw_data = nullptr; /* Drawing cache, no need to copy. */

    /* Runtime data, no need to copy. */
    BKE_pose_channel_runtime_reset_on_copy(&pchan->runtime);
  }

  /* for now, duplicate Bone Groups too when doing this */
  if (copy_constraints) {
    BLI_duplicatelist(&outPose->agroups, &src->agroups);
  }

  *dst = outPose;
}

void BKE_pose_copy_data(bPose **dst, const bPose *src, const bool copy_constraints)
{
  BKE_pose_copy_data_ex(dst, src, 0, copy_constraints);
}

void BKE_pose_itasc_init(bItasc *itasc)
{
  if (itasc) {
    itasc->iksolver = IKSOLVER_ITASC;
    itasc->minstep = 0.01f;
    itasc->maxstep = 0.06f;
    itasc->numiter = 100;
    itasc->numstep = 4;
    itasc->precision = 0.005f;
    itasc->flag = ITASC_AUTO_STEP | ITASC_INITIAL_REITERATION;
    itasc->feedback = 20.0f;
    itasc->maxvel = 50.0f;
    itasc->solver = ITASC_SOLVER_SDLS;
    itasc->dampmax = 0.5;
    itasc->dampeps = 0.15;
  }
}
void BKE_pose_ikparam_init(bPose *pose)
{
  bItasc *itasc;
  switch (pose->iksolver) {
    case IKSOLVER_ITASC:
      itasc = MEM_callocN<bItasc>("itasc");
      BKE_pose_itasc_init(itasc);
      pose->ikparam = itasc;
      break;
    case IKSOLVER_STANDARD:
    default:
      pose->ikparam = nullptr;
      break;
  }
}

/* only for real IK, not for auto-IK */
static bool pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan, int level)
{
  /* No need to check if constraint is active (has influence),
   * since all constraints with CONSTRAINT_IK_AUTO are active */
  LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
      bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
      if ((data->rootbone == 0) || (data->rootbone > level)) {
        if ((data->flag & CONSTRAINT_IK_AUTO) == 0) {
          return true;
        }
      }
    }
  }
  LISTBASE_FOREACH (Bone *, bone, &pchan->bone->childbase) {
    pchan = BKE_pose_channel_find_name(ob->pose, bone->name);
    if (pchan && pose_channel_in_IK_chain(ob, pchan, level + 1)) {
      return true;
    }
  }
  return false;
}

bool BKE_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
  return pose_channel_in_IK_chain(ob, pchan, 0);
}

static bool transform_follows_custom_tx(const bArmature *arm, const bPoseChannel *pchan)
{
  if (arm->flag & ARM_NO_CUSTOM) {
    return false;
  }

  if (!pchan->custom || !pchan->custom_tx) {
    return false;
  }

  return pchan->flag & POSE_TRANSFORM_AT_CUSTOM_TX;
}

void BKE_pose_channel_transform_orientation(const bArmature *arm,
                                            const bPoseChannel *pose_bone,
                                            float r_pose_orientation[3][3])
{
  if (!transform_follows_custom_tx(arm, pose_bone)) {
    copy_m3_m4(r_pose_orientation, pose_bone->pose_mat);
    return;
  }

  BLI_assert(pose_bone->custom_tx);

  const bPoseChannel *custom_tx_bone = pose_bone->custom_tx;
  copy_m3_m4(r_pose_orientation, custom_tx_bone->pose_mat);
}

void BKE_pose_channel_transform_location(const bArmature *arm,
                                         const bPoseChannel *pose_bone,
                                         float r_pose_space_pivot[3])
{
  if (!transform_follows_custom_tx(arm, pose_bone)) {
    copy_v3_v3(r_pose_space_pivot, pose_bone->pose_mat[3]);
    return;
  }

  copy_v3_v3(r_pose_space_pivot, pose_bone->custom_tx->pose_mat[3]);
}

void BKE_pose_channels_hash_ensure(bPose *pose)
{
  if (!pose->chanhash) {
    pose->chanhash = BLI_ghash_str_new("make_pose_chan gh");
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
      BLI_ghash_insert(pose->chanhash, pchan->name, pchan);
    }
  }
}

void BKE_pose_channels_hash_free(bPose *pose)
{
  if (pose->chanhash) {
    BLI_ghash_free(pose->chanhash, nullptr, nullptr);
    pose->chanhash = nullptr;
  }
}

static void pose_channels_remove_internal_links(Object *ob, bPoseChannel *unlinked_pchan)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if (pchan->bbone_prev == unlinked_pchan) {
      pchan->bbone_prev = nullptr;
    }
    if (pchan->bbone_next == unlinked_pchan) {
      pchan->bbone_next = nullptr;
    }
    if (pchan->custom_tx == unlinked_pchan) {
      pchan->custom_tx = nullptr;
    }
  }
}

void BKE_pose_channels_remove(Object *ob,
                              bool (*filter_fn)(const char *bone_name, void *user_data),
                              void *user_data)
{
  /* Erase any associated pose channel, along with any references to them */
  if (ob->pose) {
    bPoseChannel *pchan, *pchan_next;

    for (pchan = static_cast<bPoseChannel *>(ob->pose->chanbase.first); pchan; pchan = pchan_next)
    {
      pchan_next = pchan->next;

      if (filter_fn(pchan->name, user_data)) {
        /* Bone itself is being removed */
        BKE_pose_channel_free(pchan);
        pose_channels_remove_internal_links(ob, pchan);
        if (ob->pose->chanhash) {
          BLI_ghash_remove(ob->pose->chanhash, pchan->name, nullptr, nullptr);
        }
        BLI_freelinkN(&ob->pose->chanbase, pchan);
      }
      else {
        /* Maybe something the bone references is being removed instead? */
        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          ListBase targets = {nullptr, nullptr};
          if (BKE_constraint_targets_get(con, &targets)) {
            LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
              if (ct->tar == ob) {
                if (ct->subtarget[0]) {
                  if (filter_fn(ct->subtarget, user_data)) {
                    con->flag |= CONSTRAINT_DISABLE;
                    ct->subtarget[0] = 0;
                  }
                }
              }
            }

            BKE_constraint_targets_flush(con, &targets, false);
          }
        }

        if (pchan->bbone_prev) {
          if (filter_fn(pchan->bbone_prev->name, user_data)) {
            pchan->bbone_prev = nullptr;
          }
        }
        if (pchan->bbone_next) {
          if (filter_fn(pchan->bbone_next->name, user_data)) {
            pchan->bbone_next = nullptr;
          }
        }

        if (pchan->custom_tx) {
          if (filter_fn(pchan->custom_tx->name, user_data)) {
            pchan->custom_tx = nullptr;
          }
        }
      }
    }
  }
}

void BKE_pose_channel_free_ex(bPoseChannel *pchan, bool do_id_user)
{
  if (pchan->custom) {
    if (do_id_user) {
      id_us_min(&pchan->custom->id);
    }
    pchan->custom = nullptr;
  }

  if (pchan->mpath) {
    animviz_free_motionpath(pchan->mpath);
    pchan->mpath = nullptr;
  }

  BKE_constraints_free_ex(&pchan->constraints, do_id_user);

  if (pchan->prop) {
    IDP_FreeProperty_ex(pchan->prop, do_id_user);
    pchan->prop = nullptr;
  }
  if (pchan->system_properties) {
    IDP_FreeProperty_ex(pchan->system_properties, do_id_user);
    pchan->system_properties = nullptr;
  }

  /* Cached data, for new draw manager rendering code. */
  MEM_SAFE_FREE(pchan->draw_data);

  /* Cached B-Bone shape and other data. */
  BKE_pose_channel_runtime_free(&pchan->runtime);
}

void BKE_pose_channel_runtime_reset(bPoseChannel_Runtime *runtime)
{
  *runtime = bPoseChannel_Runtime{};
}

void BKE_pose_channel_runtime_reset_on_copy(bPoseChannel_Runtime *runtime)
{
  const SessionUID uid = runtime->session_uid;
  *runtime = bPoseChannel_Runtime{};
  runtime->session_uid = uid;
}

void BKE_pose_channel_runtime_free(bPoseChannel_Runtime *runtime)
{
  BKE_pose_channel_free_bbone_cache(runtime);
}

void BKE_pose_channel_free_bbone_cache(bPoseChannel_Runtime *runtime)
{
  runtime->bbone_segments = 0;
  MEM_SAFE_FREE(runtime->bbone_rest_mats);
  MEM_SAFE_FREE(runtime->bbone_pose_mats);
  MEM_SAFE_FREE(runtime->bbone_deform_mats);
  MEM_SAFE_FREE(runtime->bbone_dual_quats);
  MEM_SAFE_FREE(runtime->bbone_segment_boundaries);
}

void BKE_pose_channel_free(bPoseChannel *pchan)
{
  BKE_pose_channel_free_ex(pchan, true);
}

void BKE_pose_channels_free_ex(bPose *pose, bool do_id_user)
{
  if (!BLI_listbase_is_empty(&pose->chanbase)) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
      BKE_pose_channel_free_ex(pchan, do_id_user);
    }

    BLI_freelistN(&pose->chanbase);
  }

  BKE_pose_channels_hash_free(pose);

  MEM_SAFE_FREE(pose->chan_array);
}

void BKE_pose_channels_free(bPose *pose)
{
  BKE_pose_channels_free_ex(pose, true);
}

void BKE_pose_free_data_ex(bPose *pose, bool do_id_user)
{
  /* free pose-channels */
  BKE_pose_channels_free_ex(pose, do_id_user);

  /* free pose-groups */
  if (pose->agroups.first) {
    BLI_freelistN(&pose->agroups);
  }

  /* free IK solver state */
  BIK_clear_data(pose);

  /* free IK solver param */
  if (pose->ikparam) {
    MEM_freeN(static_cast<bItasc *>(pose->ikparam));
  }
}

void BKE_pose_free_data(bPose *pose)
{
  BKE_pose_free_data_ex(pose, true);
}

void BKE_pose_free_ex(bPose *pose, bool do_id_user)
{
  if (pose) {
    BKE_pose_free_data_ex(pose, do_id_user);
    /* free pose */
    MEM_freeN(pose);
  }
}

void BKE_pose_free(bPose *pose)
{
  BKE_pose_free_ex(pose, true);
}

void BKE_pose_channel_copy_data(bPoseChannel *pchan, const bPoseChannel *pchan_from)
{
  /* copy transform locks */
  pchan->protectflag = pchan_from->protectflag;

  /* copy rotation mode */
  pchan->rotmode = pchan_from->rotmode;

  /* copy bone group */
  pchan->agrp_index = pchan_from->agrp_index;

  /* IK (DOF) settings. */
  pchan->ikflag = pchan_from->ikflag;
  copy_v3_v3(pchan->limitmin, pchan_from->limitmin);
  copy_v3_v3(pchan->limitmax, pchan_from->limitmax);
  copy_v3_v3(pchan->stiffness, pchan_from->stiffness);
  pchan->ikstretch = pchan_from->ikstretch;
  pchan->ikrotweight = pchan_from->ikrotweight;
  pchan->iklinweight = pchan_from->iklinweight;

  /* bbone settings (typically not animated) */
  pchan->bbone_next = pchan_from->bbone_next;
  pchan->bbone_prev = pchan_from->bbone_prev;

  /* constraints */
  BKE_constraints_copy(&pchan->constraints, &pchan_from->constraints, true);

  /* id-properties */
  if (pchan->prop) {
    /* Unlikely, but possible that it exists. */
    IDP_FreeProperty(pchan->prop);
    pchan->prop = nullptr;
  }
  if (pchan_from->prop) {
    pchan->prop = IDP_CopyProperty(pchan_from->prop);
  }
  if (pchan->system_properties) {
    /* Unlikely, but possible that it exists. */
    IDP_FreeProperty(pchan->system_properties);
    pchan->system_properties = nullptr;
  }
  if (pchan_from->system_properties) {
    pchan->system_properties = IDP_CopyProperty(pchan_from->system_properties);
  }

  /* custom shape */
  pchan->custom = pchan_from->custom;
  if (pchan->custom) {
    id_us_plus(&pchan->custom->id);
  }
  copy_v3_v3(pchan->custom_scale_xyz, pchan_from->custom_scale_xyz);
  copy_v3_v3(pchan->custom_translation, pchan_from->custom_translation);
  copy_v3_v3(pchan->custom_rotation_euler, pchan_from->custom_rotation_euler);
  pchan->custom_shape_wire_width = pchan_from->custom_shape_wire_width;

  pchan->color.palette_index = pchan_from->color.palette_index;
  copy_v4_v4_uchar(pchan->color.custom.active, pchan_from->color.custom.active);
  copy_v4_v4_uchar(pchan->color.custom.select, pchan_from->color.custom.select);
  copy_v4_v4_uchar(pchan->color.custom.solid, pchan_from->color.custom.solid);
  pchan->color.custom.flag = pchan_from->color.custom.flag;

  pchan->drawflag = pchan_from->drawflag;
}

void BKE_pose_update_constraint_flags(bPose *pose)
{
  pose->flag &= ~POSE_CONSTRAINTS_TIMEDEPEND;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->constflag = 0;

    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      pchan->constflag |= PCHAN_HAS_CONST;

      switch (con->type) {
        case CONSTRAINT_TYPE_KINEMATIC: {
          bKinematicConstraint *data = (bKinematicConstraint *)con->data;

          pchan->constflag |= PCHAN_HAS_IK;

          if (data->tar == nullptr || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0))
          {
            pchan->constflag |= PCHAN_HAS_NO_TARGET;
          }

          bPoseChannel *chain_tip = (data->flag & CONSTRAINT_IK_TIP) ? pchan : pchan->parent;

          /* negative rootbone = recalc rootbone index. used in do_versions */
          if (data->rootbone < 0) {
            data->rootbone = 0;

            bPoseChannel *parchan = chain_tip;
            while (parchan) {
              data->rootbone++;
              if ((parchan->bone->flag & BONE_CONNECTED) == 0) {
                break;
              }
              parchan = parchan->parent;
            }
          }

          /* Mark the pose bones in the IK chain as influenced by it. */
          {
            bPoseChannel *chain_bone = chain_tip;
            for (short index = 0; chain_bone && (data->rootbone == 0 || index < data->rootbone);
                 index++)
            {
              chain_bone->constflag |= PCHAN_INFLUENCED_BY_IK;
              chain_bone = chain_bone->parent;
            }
          }
          break;
        }

        case CONSTRAINT_TYPE_FOLLOWPATH: {
          bFollowPathConstraint *data = (bFollowPathConstraint *)con->data;

          /* if we have a valid target, make sure that this will get updated on frame-change
           * (needed for when there is no anim-data for this pose)
           */
          if ((data->tar) && (data->tar->type == OB_CURVES_LEGACY)) {
            pose->flag |= POSE_CONSTRAINTS_TIMEDEPEND;
          }
          break;
        }

        case CONSTRAINT_TYPE_SPLINEIK:
          pchan->constflag |= PCHAN_HAS_SPLINEIK;
          break;

        default:
          break;
      }
    }
  }

  pose->flag &= ~POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

void BKE_pose_tag_update_constraint_flags(bPose *pose)
{
  pose->flag |= POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

/* ************************** Bone Groups ************************** */

bActionGroup *BKE_pose_add_group(bPose *pose, const char *name)
{
  bActionGroup *grp;

  if (!name) {
    name = DATA_("Group");
  }

  grp = MEM_callocN<bActionGroup>("PoseGroup");
  STRNCPY_UTF8(grp->name, name);
  BLI_addtail(&pose->agroups, grp);
  BLI_uniquename(&pose->agroups, grp, name, '.', offsetof(bActionGroup, name), sizeof(grp->name));

  pose->active_group = BLI_listbase_count(&pose->agroups);

  return grp;
}

void BKE_pose_remove_group(bPose *pose, bActionGroup *grp, const int index)
{
  int idx = index;

  if (idx < 1) {
    idx = BLI_findindex(&pose->agroups, grp) + 1;
  }

  BLI_assert(idx > 0);

  /* adjust group references (the trouble of using indices!):
   * - firstly, make sure nothing references it
   * - also, make sure that those after this item get corrected
   */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->agrp_index == idx) {
      pchan->agrp_index = 0;
    }
    else if (pchan->agrp_index > idx) {
      pchan->agrp_index--;
    }
  }

  /* now, remove it from the pose */
  BLI_freelinkN(&pose->agroups, grp);
  if (pose->active_group >= idx) {
    const bool has_groups = !BLI_listbase_is_empty(&pose->agroups);
    pose->active_group--;
    if (pose->active_group == 0 && has_groups) {
      pose->active_group = 1;
    }
    else if (pose->active_group < 0 || !has_groups) {
      pose->active_group = 0;
    }
  }
}

void BKE_pose_remove_group_index(bPose *pose, const int index)
{
  bActionGroup *grp = nullptr;

  /* get group to remove */
  grp = static_cast<bActionGroup *>(BLI_findlink(&pose->agroups, index - 1));
  if (grp) {
    BKE_pose_remove_group(pose, grp, index);
  }
}

/* ************** Pose Management Tools ****************** */

void BKE_pose_rest(bPose *pose, bool selected_bones_only)
{
  if (!pose) {
    return;
  }

  memset(pose->stride_offset, 0, sizeof(pose->stride_offset));
  memset(pose->cyclic_offset, 0, sizeof(pose->cyclic_offset));

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (selected_bones_only && pchan->bone != nullptr && (pchan->flag & POSE_SELECTED) == 0) {
      continue;
    }
    zero_v3(pchan->loc);
    zero_v3(pchan->eul);
    unit_qt(pchan->quat);
    unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
    pchan->scale[0] = pchan->scale[1] = pchan->scale[2] = 1.0f;

    pchan->roll1 = pchan->roll2 = 0.0f;
    pchan->curve_in_x = pchan->curve_in_z = 0.0f;
    pchan->curve_out_x = pchan->curve_out_z = 0.0f;
    pchan->ease1 = pchan->ease2 = 0.0f;

    copy_v3_fl(pchan->scale_in, 1.0f);
    copy_v3_fl(pchan->scale_out, 1.0f);

    pchan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SCALE | POSE_BBONE_SHAPE);
  }
}

void BKE_pose_copy_pchan_result(bPoseChannel *pchanto, const bPoseChannel *pchanfrom)
{
  copy_m4_m4(pchanto->pose_mat, pchanfrom->pose_mat);
  copy_m4_m4(pchanto->chan_mat, pchanfrom->chan_mat);

  /* used for local constraints */
  copy_v3_v3(pchanto->loc, pchanfrom->loc);
  copy_qt_qt(pchanto->quat, pchanfrom->quat);
  copy_v3_v3(pchanto->eul, pchanfrom->eul);
  copy_v3_v3(pchanto->scale, pchanfrom->scale);

  copy_v3_v3(pchanto->pose_head, pchanfrom->pose_head);
  copy_v3_v3(pchanto->pose_tail, pchanfrom->pose_tail);

  pchanto->roll1 = pchanfrom->roll1;
  pchanto->roll2 = pchanfrom->roll2;
  pchanto->curve_in_x = pchanfrom->curve_in_x;
  pchanto->curve_in_z = pchanfrom->curve_in_z;
  pchanto->curve_out_x = pchanfrom->curve_out_x;
  pchanto->curve_out_z = pchanfrom->curve_out_z;
  pchanto->ease1 = pchanfrom->ease1;
  pchanto->ease2 = pchanfrom->ease2;

  copy_v3_v3(pchanto->scale_in, pchanfrom->scale_in);
  copy_v3_v3(pchanto->scale_out, pchanfrom->scale_out);

  pchanto->rotmode = pchanfrom->rotmode;
  pchanto->flag = pchanfrom->flag;
  pchanto->protectflag = pchanfrom->protectflag;
}

bool BKE_pose_copy_result(bPose *to, bPose *from)
{
  if (to == nullptr || from == nullptr) {
    CLOG_ERROR(
        &LOG, "Pose copy error, pose to:%p from:%p", (void *)to, (void *)from); /* debug temp */
    return false;
  }

  if (to == from) {
    CLOG_ERROR(&LOG, "source and target are the same");
    return false;
  }

  LISTBASE_FOREACH (bPoseChannel *, pchanfrom, &from->chanbase) {
    bPoseChannel *pchanto = BKE_pose_channel_find_name(to, pchanfrom->name);
    if (pchanto != nullptr) {
      BKE_pose_copy_pchan_result(pchanto, pchanfrom);
    }
  }
  return true;
}

void BKE_pose_tag_recalc(Main *bmain, bPose *pose)
{
  pose->flag |= POSE_RECALC;
  /* Depsgraph components depends on actual pose state,
   * if pose was changed depsgraph is to be updated as well.
   */
  DEG_relations_tag_update(bmain);
}

void what_does_obaction(Object *ob,
                        Object *workob,
                        bPose *pose,
                        bAction *act,
                        const int32_t action_slot_handle,
                        char groupname[],
                        const AnimationEvalContext *anim_eval_context)
{
  using namespace blender::animrig;
  BLI_assert(act);

  bActionGroup *agrp = nullptr;
  if (groupname && groupname[0]) {
    /* Find the named channel group. */
    Action &action = act->wrap();
    if (action.is_action_layered()) {
      Channelbag *cbag = channelbag_for_action_slot(action, action_slot_handle);
      agrp = cbag ? cbag->channel_group_find(groupname) : nullptr;
    }
    else {
      agrp = BKE_action_group_find_name(act, groupname);
    }
  }

  /* clear workob */
  blender::bke::ObjectRuntime workob_runtime;
  BKE_object_workob_clear(workob);
  workob->runtime = &workob_runtime;

  /* init workob */
  copy_m4_m4(workob->runtime->object_to_world.ptr(), ob->object_to_world().ptr());
  copy_m4_m4(workob->parentinv, ob->parentinv);
  copy_m4_m4(workob->constinv, ob->constinv);
  workob->parent = ob->parent;

  workob->rotmode = ob->rotmode;

  workob->trackflag = ob->trackflag;
  workob->upflag = ob->upflag;

  workob->partype = ob->partype;
  workob->par1 = ob->par1;
  workob->par2 = ob->par2;
  workob->par3 = ob->par3;

  workob->constraints.first = ob->constraints.first;
  workob->constraints.last = ob->constraints.last;

  /* Need to set pose too, since this is used for both types of Action Constraint. */
  workob->pose = pose;
  if (pose) {
    /* This function is most likely to be used with a temporary pose with a single bone in there.
     * For such cases it makes no sense to create hash since it'll only waste CPU ticks on memory
     * allocation and also will make lookup slower.
     */
    if (pose->chanbase.first != pose->chanbase.last) {
      BKE_pose_channels_hash_ensure(pose);
    }
    if (pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
      BKE_pose_update_constraint_flags(pose);
    }
  }

  STRNCPY_UTF8(workob->parsubstr, ob->parsubstr);

  /* we don't use real object name, otherwise RNA screws with the real thing */
  STRNCPY_UTF8(workob->id.name, "OB<ConstrWorkOb>");

  /* If we're given a group to use, it's likely to be more efficient
   * (though a bit more dangerous). */
  if (agrp) {
    /* specifically evaluate this group only */

    /* get RNA-pointer for the workob's ID */
    PointerRNA id_ptr = RNA_id_pointer_create(&workob->id);

    /* execute action for this group only */
    animsys_evaluate_action_group(&id_ptr, act, agrp, anim_eval_context);
  }
  else {
    AnimData adt = {nullptr};

    /* init animdata, and attach to workob */
    workob->adt = &adt;

    adt.action = act;
    adt.slot_handle = action_slot_handle;
    BKE_animdata_action_ensure_idroot(&workob->id, act);

    /* execute effects of Action on to workob (or its PoseChannels) */
    BKE_animsys_evaluate_animdata(&workob->id, &adt, anim_eval_context, ADT_RECALC_ANIM, false);

    /* Ensure stack memory set here isn't accessed later, relates to !118847. */
    workob->adt = nullptr;
  }
  /* Ensure stack memory set here isn't accessed later, see !118847. */
  workob->runtime = nullptr;
}

void BKE_pose_check_uids_unique_and_report(const bPose *pose)
{
  if (pose == nullptr) {
    return;
  }

  GSet *used_uids = BLI_gset_new(
      BLI_session_uid_ghash_hash, BLI_session_uid_ghash_compare, "sequencer used uids");

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    const SessionUID *session_uid = &pchan->runtime.session_uid;
    if (!BLI_session_uid_is_generated(session_uid)) {
      printf("Pose channel %s does not have UID generated.\n", pchan->name);
      continue;
    }

    if (BLI_gset_lookup(used_uids, session_uid) != nullptr) {
      printf("Pose channel %s has duplicate UID generated.\n", pchan->name);
      continue;
    }

    BLI_gset_insert(used_uids, (void *)session_uid);
  }

  BLI_gset_free(used_uids, nullptr);
}

void BKE_pose_blend_write(BlendWriter *writer, bPose *pose)
{
#ifndef __GNUC__
  BLI_assert(pose != nullptr);
#endif

  /* Write channels */
  LISTBASE_FOREACH (bPoseChannel *, chan, &pose->chanbase) {
    /* Write ID Properties -- and copy this comment EXACTLY for easy finding
     * of library blocks that implement this. */
    if (chan->prop) {
      IDP_BlendWrite(writer, chan->prop);
    }
    if (chan->system_properties) {
      IDP_BlendWrite(writer, chan->system_properties);
    }

    BKE_constraint_blend_write(writer, &chan->constraints);

    animviz_motionpath_blend_write(writer, chan->mpath);

    BLO_write_struct(writer, bPoseChannel, chan);
  }

  /* Write groups */
  LISTBASE_FOREACH (bActionGroup *, grp, &pose->agroups) {
    BLO_write_struct(writer, bActionGroup, grp);
  }

  /* write IK param */
  if (pose->ikparam) {
    const char *structname = BKE_pose_ikparam_get_name(pose);
    if (structname) {
      BLO_write_struct_by_name(writer, structname, pose->ikparam);
    }
  }

  /* Write this pose */
  BLO_write_struct(writer, bPose, pose);
}

void BKE_pose_blend_read_data(BlendDataReader *reader, ID *id_owner, bPose *pose)
{
  if (!pose) {
    return;
  }

  BLO_read_struct_list(reader, bPoseChannel, &pose->chanbase);
  BLO_read_struct_list(reader, bActionGroup, &pose->agroups);

  pose->chanhash = nullptr;
  pose->chan_array = nullptr;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    BKE_pose_channel_runtime_reset(&pchan->runtime);
    BKE_pose_channel_session_uid_generate(pchan);

    pchan->bone = nullptr;
    BLO_read_struct(reader, bPoseChannel, &pchan->parent);
    BLO_read_struct(reader, bPoseChannel, &pchan->child);
    BLO_read_struct(reader, bPoseChannel, &pchan->custom_tx);

    BLO_read_struct(reader, bPoseChannel, &pchan->bbone_prev);
    BLO_read_struct(reader, bPoseChannel, &pchan->bbone_next);

    BKE_constraint_blend_read_data(reader, id_owner, &pchan->constraints);

    BLO_read_struct(reader, IDProperty, &pchan->prop);
    IDP_BlendDataRead(reader, &pchan->prop);
    BLO_read_struct(reader, IDProperty, &pchan->system_properties);
    IDP_BlendDataRead(reader, &pchan->system_properties);

    BLO_read_struct(reader, bMotionPath, &pchan->mpath);
    if (pchan->mpath) {
      animviz_motionpath_blend_read_data(reader, pchan->mpath);
    }

    BLI_listbase_clear(&pchan->iktree);
    BLI_listbase_clear(&pchan->siktree);

    /* in case this value changes in future, clamp else we get undefined behavior */
    CLAMP(pchan->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

    pchan->draw_data = nullptr;
  }
  pose->ikdata = nullptr;
  if (pose->ikparam != nullptr) {
    const char *structname = BKE_pose_ikparam_get_name(pose);
    if (structname) {
      pose->ikparam = BLO_read_struct_by_name_array(reader, structname, 1, pose->ikparam);
    }
    else {
      pose->ikparam = nullptr;
    }
  }
}

void BKE_pose_blend_read_after_liblink(BlendLibReader *reader, Object *ob, bPose *pose)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);

  if (!pose || !arm) {
    return;
  }

  /* Always rebuild to match library changes, except on Undo. */
  bool rebuild = false;

  if (!BLO_read_lib_is_undo(reader)) {
    if (ob->id.lib != arm->id.lib) {
      rebuild = true;
    }
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->bone = BKE_armature_find_bone_name(arm, pchan->name);

    if (UNLIKELY(pchan->bone == nullptr)) {
      rebuild = true;
    }

    /* At some point in history, bones could have an armature object as custom shape, which caused
     * all kinds of wonderful issues. This is now avoided in RNA, but through the magic of linking
     * and editing the library file, the situation can still occur. Better to just reset the
     * pointer in those cases. */
    if (pchan->custom && pchan->custom->type == OB_ARMATURE) {
      pchan->custom = nullptr;
    }
  }

  if (rebuild) {
    Main *bmain = BLO_read_lib_get_main(reader);
    DEG_id_tag_update_ex(
        bmain, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
    BKE_pose_tag_recalc(bmain, pose);
  }
}

void BKE_action_fcurves_clear(bAction *act)
{
  if (!act) {
    return;
  }

  BLI_assert(act->wrap().is_action_legacy());

  while (act->curves.first) {
    FCurve *fcu = static_cast<FCurve *>(act->curves.first);
    action_groups_remove_channel(act, fcu);
    BKE_fcurve_free(fcu);
  }
  DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
}
