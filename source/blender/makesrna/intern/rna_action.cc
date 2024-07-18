/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_action.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "ANIM_action.hh"

#include "WM_types.hh"

using namespace blender;

#ifdef WITH_ANIM_BAKLAVA
const EnumPropertyItem rna_enum_layer_mix_mode_items[] = {
    {int(animrig::Layer::MixMode::Replace),
     "REPLACE",
     0,
     "Replace",
     "Channels in this layer override the same channels from underlying layers"},
    {int(animrig::Layer::MixMode::Offset),
     "OFFSET",
     0,
     "Offset",
     "Channels in this layer are added to underlying layers as sequential operations"},
    {int(animrig::Layer::MixMode::Add),
     "ADD",
     0,
     "Add",
     "Channels in this layer are added to underlying layers on a per-channel basis"},
    {int(animrig::Layer::MixMode::Subtract),
     "SUBTRACT",
     0,
     "Subtract",
     "Channels in this layer are subtracted to underlying layers on a per-channel basis"},
    {int(animrig::Layer::MixMode::Multiply),
     "MULTIPLY",
     0,
     "Multiply",
     "Channels in this layer are multiplied with underlying layers on a per-channel basis"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_type_items[] = {
    {int(animrig::Strip::Type::Keyframe),
     "KEYFRAME",
     0,
     "Keyframe",
     "Strip containing keyframes on F-Curves"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif  // WITH_ANIM_BAKLAVA

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include "BLI_math_base.h"

#  include "BKE_fcurve.hh"

#  include "DEG_depsgraph.hh"

#  include "ANIM_action.hh"
#  include "ANIM_animdata.hh"
#  include "ED_anim_api.hh"

#  include "WM_api.hh"

#  include "UI_interface_icons.hh"

#  include "DEG_depsgraph.hh"

#  include "ANIM_keyframing.hh"

#  include <fmt/format.h>

#  ifdef WITH_ANIM_BAKLAVA

static animrig::Action &rna_action(const PointerRNA *ptr)
{
  return reinterpret_cast<bAction *>(ptr->owner_id)->wrap();
}

static animrig::Slot &rna_data_slot(const PointerRNA *ptr)
{
  BLI_assert(ptr->type == &RNA_ActionSlot);
  return reinterpret_cast<ActionSlot *>(ptr->data)->wrap();
}

static animrig::Layer &rna_data_layer(const PointerRNA *ptr)
{
  return reinterpret_cast<ActionLayer *>(ptr->data)->wrap();
}

static animrig::Strip &rna_data_strip(const PointerRNA *ptr)
{
  return reinterpret_cast<ActionStrip *>(ptr->data)->wrap();
}

static void rna_Action_tag_animupdate(Main *, Scene *, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
}

static animrig::KeyframeStrip &rna_data_keyframe_strip(const PointerRNA *ptr)
{
  animrig::Strip &strip = reinterpret_cast<ActionStrip *>(ptr->data)->wrap();
  return strip.as<animrig::KeyframeStrip>();
}

static animrig::ChannelBag &rna_data_channelbag(const PointerRNA *ptr)
{
  return reinterpret_cast<ActionChannelBag *>(ptr->data)->wrap();
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter, Span<T *> items)
{
  rna_iterator_array_begin(iter, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter, MutableSpan<T *> items)
{
  rna_iterator_array_begin(iter, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

static PointerRNA rna_ActionSlots_active_get(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot *active_slot = action.slot_active_get();

  if (!active_slot) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create(&action.id, &RNA_ActionSlot, active_slot);
}

static void rna_ActionSlots_active_set(PointerRNA *ptr,
                                       PointerRNA value,
                                       struct ReportList * /*reports*/)
{
  animrig::Action &action = rna_action(ptr);

  if (value.data) {
    animrig::Slot &slot = rna_data_slot(&value);
    action.slot_active_set(slot.handle);
  }
  else {
    action.slot_active_set(animrig::Slot::unassigned);
  }
}

static ActionSlot *rna_Action_slots_new(bAction *dna_action,
                                        bContext *C,
                                        ReportList *reports,
                                        ID *id_for_slot)
{
  animrig::Action &action = dna_action->wrap();
  animrig::Slot *slot;

  if (!action.is_action_layered()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot add slots to a legacy Action '%s'. Convert it to a layered Action first.",
                action.id.name + 2);
    return nullptr;
  }

  if (id_for_slot) {
    slot = &action.slot_add_for_id(*id_for_slot);
  }
  else {
    slot = &action.slot_add();
  }

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return slot;
}

static void rna_iterator_action_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  rna_iterator_array_begin(iter, action.layers());
}

static int rna_iterator_action_layers_length(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  return action.layers().size();
}

static ActionLayer *rna_Action_layers_new(bAction *dna_action,
                                          bContext *C,
                                          ReportList *reports,
                                          const char *name)
{
  animrig::Action &action = dna_action->wrap();

  if (!action.is_action_layered()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot add layers to a legacy Action '%s'. Convert it to a layered Action first.",
                action.id.name + 2);
    return nullptr;
  }

  if (action.layers().size() >= 1) {
    /* Not allowed to have more than one layer, for now. This limitation is in
     * place until working with multiple animated IDs is fleshed out better. */
    BKE_report(reports, RPT_ERROR, "An Action may not have more than one layer");
    return nullptr;
  }

  animrig::Layer &layer = action.layer_add(name);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &layer;
}

void rna_Action_layers_remove(bAction *dna_action,
                              bContext *C,
                              ReportList *reports,
                              PointerRNA *layer_ptr)
{
  animrig::Action &action = dna_action->wrap();
  animrig::Layer &layer = rna_data_layer(layer_ptr);
  if (!action.layer_remove(layer)) {
    BKE_report(reports, RPT_ERROR, "This layer does not belong to this Action");
    return;
  }

  RNA_POINTER_INVALIDATE(layer_ptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
}

static void rna_iterator_animation_slots_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  rna_iterator_array_begin(iter, action.slots());
}

static int rna_iterator_animation_slots_length(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  return action.slots().size();
}

static std::optional<std::string> rna_ActionSlot_path(const PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);

  char name_esc[sizeof(slot.name) * 2];
  BLI_str_escape(name_esc, slot.name, sizeof(name_esc));
  return fmt::format("slots[\"{}\"]", name_esc);
}

int rna_ActionSlot_idtype_icon_get(PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  return UI_icon_from_idcode(slot.idtype);
  ;
}

/* Name functions that ignore the first two ID characters */
void rna_ActionSlot_name_display_get(PointerRNA *ptr, char *value)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  slot.name_without_prefix().unsafe_copy(value);
}

int rna_ActionSlot_name_display_length(PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  return slot.name_without_prefix().size();
}

static void rna_ActionSlot_name_display_set(PointerRNA *ptr, const char *name)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  const StringRef name_ref(name);

  if (name_ref.is_empty()) {
    WM_report(RPT_ERROR, "Action slot display names cannot be empty");
    return;
  }

  /* Construct the new internal name, from the slot's type and the given name. */
  const std::string internal_name = slot.name_prefix_for_idtype() + name_ref;
  action.slot_name_define(slot, internal_name);
}

static void rna_ActionSlot_name_set(PointerRNA *ptr, const char *name)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  const StringRef name_ref(name);

  if (name_ref.size() < animrig::Slot::name_length_min) {
    WM_report(RPT_ERROR, "Action slot names should be at least three characters");
    return;
  }

  if (slot.has_idtype()) {
    /* Check if the new name is going to be compatible with the already-established ID type. */
    const std::string expect_prefix = slot.name_prefix_for_idtype();

    if (!name_ref.startswith(expect_prefix)) {
      const std::string new_prefix = name_ref.substr(0, 2);
      WM_reportf(RPT_WARNING,
                 "Action slot renamed to unexpected prefix \"%s\" (expected \"%s\").\n",
                 new_prefix.c_str(),
                 expect_prefix.c_str());
    }
  }

  action.slot_name_define(slot, name);
}

static void rna_ActionSlot_name_update(Main *bmain, Scene *, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  action.slot_name_propagate(*bmain, slot);
}

static std::optional<std::string> rna_ActionLayer_path(const PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);

  char name_esc[sizeof(layer.name) * 2];
  BLI_str_escape(name_esc, layer.name, sizeof(name_esc));
  return fmt::format("layers[\"{}\"]", name_esc);
}

static void rna_iterator_ActionLayer_strips_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);
  rna_iterator_array_begin(iter, layer.strips());
}

static int rna_iterator_ActionLayer_strips_length(PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);
  return layer.strips().size();
}

ActionStrip *rna_ActionStrips_new(ActionLayer *dna_layer,
                                  bContext *C,
                                  ReportList *reports,
                                  const int type)
{
  const animrig::Strip::Type strip_type = animrig::Strip::Type(type);

  animrig::Layer &layer = dna_layer->wrap();

  if (layer.strips().size() >= 1) {
    /* Not allowed to have more than one strip, for now. This limitation is in
     * place until working with layers is fleshed out better. */
    BKE_report(reports, RPT_ERROR, "A layer may not have more than one strip");
    return nullptr;
  }

  animrig::Strip &strip = layer.strip_add(strip_type);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &strip;
}

void rna_ActionStrips_remove(
    ID *action, ActionLayer *dna_layer, bContext *C, ReportList *reports, PointerRNA *strip_ptr)
{
  animrig::Layer &layer = dna_layer->wrap();
  animrig::Strip &strip = rna_data_strip(strip_ptr);
  if (!layer.strip_remove(strip)) {
    BKE_report(reports, RPT_ERROR, "This strip does not belong to this layer");
    return;
  }

  RNA_POINTER_INVALIDATE(strip_ptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(action, ID_RECALC_ANIMATION);
}

static StructRNA *rna_ActionStrip_refine(PointerRNA *ptr)
{
  animrig::Strip &strip = rna_data_strip(ptr);
  switch (strip.type()) {
    case animrig::Strip::Type::Keyframe:
      return &RNA_KeyframeActionStrip;
  }
  return &RNA_UnknownType;
}

static std::optional<std::string> rna_ActionStrip_path(const PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Strip &strip_to_find = rna_data_strip(ptr);

  for (animrig::Layer *layer : action.layers()) {
    Span<animrig::Strip *> strips = layer->strips();
    const int index = strips.first_index_try(&strip_to_find);
    if (index < 0) {
      continue;
    }

    PointerRNA layer_ptr = RNA_pointer_create(&action.id, &RNA_ActionLayer, layer);
    const std::optional<std::string> layer_path = rna_ActionLayer_path(&layer_ptr);
    BLI_assert_msg(layer_path, "Every animation layer should have a valid RNA path.");
    const std::string strip_path = fmt::format("{}.strips[{}]", *layer_path, index);
    return strip_path;
  }

  return std::nullopt;
}

static void rna_iterator_keyframestrip_channelbags_begin(CollectionPropertyIterator *iter,
                                                         PointerRNA *ptr)
{
  animrig::KeyframeStrip &key_strip = rna_data_keyframe_strip(ptr);
  rna_iterator_array_begin(iter, key_strip.channelbags());
}

static int rna_iterator_keyframestrip_channelbags_length(PointerRNA *ptr)
{
  animrig::KeyframeStrip &key_strip = rna_data_keyframe_strip(ptr);
  return key_strip.channelbags().size();
}

static ActionChannelBag *rna_ChannelBags_new(KeyframeActionStrip *dna_strip,
                                             bContext *C,
                                             ReportList *reports,
                                             ActionSlot *dna_slot)
{
  animrig::KeyframeStrip &key_strip = dna_strip->wrap();
  animrig::Slot &slot = dna_slot->wrap();

  if (key_strip.channelbag_for_slot(slot) != nullptr) {
    BKE_report(reports, RPT_ERROR, "A channelbag for this slot already exists");
    return nullptr;
  }

  ChannelBag &channelbag = key_strip.channelbag_for_slot_add(slot);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  /* No need to tag the depsgraph, as there is no new animation yet. */

  return &channelbag;
}

static void rna_ChannelBags_remove(ID *action,
                                   KeyframeActionStrip *dna_strip,
                                   bContext *C,
                                   ReportList *reports,
                                   PointerRNA *channelbag_ptr)
{
  animrig::KeyframeStrip &key_strip = dna_strip->wrap();
  animrig::ChannelBag &channelbag = rna_data_channelbag(channelbag_ptr);

  if (!key_strip.channelbag_remove(channelbag)) {
    BKE_report(reports, RPT_ERROR, "This channelbag does not belong to this strip");
    return;
  }

  RNA_POINTER_INVALIDATE(channelbag_ptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(action, ID_RECALC_ANIMATION);
}

static bool rna_KeyframeActionStrip_key_insert(ID *id,
                                               KeyframeActionStrip *dna_strip,
                                               Main *bmain,
                                               ReportList *reports,
                                               ActionSlot *dna_slot,
                                               const char *rna_path,
                                               const int array_index,
                                               const float value,
                                               const float time)
{
  if (dna_slot == nullptr) {
    BKE_report(reports, RPT_ERROR, "Slot cannot be None");
    return false;
  }

  animrig::KeyframeStrip &key_strip = dna_strip->wrap();
  const animrig::Slot &slot = dna_slot->wrap();
  const animrig::KeyframeSettings settings = animrig::get_keyframe_settings(true);

  const animrig::SingleKeyingResult result = key_strip.keyframe_insert(
      slot, {rna_path, array_index}, {time, value}, settings, INSERTKEY_NOFLAGS);

  const bool ok = result == animrig::SingleKeyingResult::SUCCESS;
  if (ok) {
    DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION);
  }

  return ok;
}

static std::optional<std::string> rna_ChannelBag_path(const PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::ChannelBag &cbag_to_find = rna_data_channelbag(ptr);

  for (animrig::Layer *layer : action.layers()) {
    for (int64_t strip_index : layer->strips().index_range()) {
      const animrig::Strip *strip = layer->strip(strip_index);
      if (!strip->is<animrig::KeyframeStrip>()) {
        continue;
      }

      const animrig::KeyframeStrip &key_strip = strip->as<animrig::KeyframeStrip>();
      const int64_t index = key_strip.find_channelbag_index(cbag_to_find);
      if (index < 0) {
        continue;
      }

      PointerRNA layer_ptr = RNA_pointer_create(&action.id, &RNA_ActionLayer, layer);
      const std::optional<std::string> layer_path = rna_ActionLayer_path(&layer_ptr);
      BLI_assert_msg(layer_path, "Every animation layer should have a valid RNA path.");
      return fmt::format("{}.strips[{}].channelbags[{}]", *layer_path, strip_index, index);
    }
  }

  return std::nullopt;
}

static void rna_iterator_ChannelBag_fcurves_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  animrig::ChannelBag &bag = rna_data_channelbag(ptr);
  rna_iterator_array_begin(iter, bag.fcurves());
}

static int rna_iterator_ChannelBag_fcurves_length(PointerRNA *ptr)
{
  animrig::ChannelBag &bag = rna_data_channelbag(ptr);
  return bag.fcurves().size();
}

static FCurve *rna_ChannelBag_fcurve_new(ActionChannelBag *dna_channelbag,
                                         ReportList *reports,
                                         const char *data_path,
                                         const int index)
{
  BLI_assert(data_path != nullptr);
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  animrig::ChannelBag &self = dna_channelbag->wrap();
  FCurve *fcurve = self.fcurve_create_unique({data_path, index});
  if (!fcurve) {
    BKE_reportf(reports,
                RPT_ERROR,
                "F-Curve '%s[%d]' already exists in this channelbag",
                data_path,
                index);
    return nullptr;
  }
  return fcurve;
}

static FCurve *rna_ChannelBag_fcurve_find(ActionChannelBag *dna_channelbag,
                                          ReportList *reports,
                                          const char *data_path,
                                          const int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  animrig::ChannelBag &self = dna_channelbag->wrap();
  return self.fcurve_find({data_path, index});
}

static void rna_ChannelBag_fcurve_remove(ID *dna_action_id,
                                         ActionChannelBag *dna_channelbag,
                                         bContext *C,
                                         ReportList *reports,
                                         PointerRNA *fcurve_ptr)
{
  animrig::ChannelBag &self = dna_channelbag->wrap();
  FCurve *fcurve = static_cast<FCurve *>(fcurve_ptr->data);

  if (!self.fcurve_remove(*fcurve)) {
    BKE_reportf(reports, RPT_ERROR, "F-Curve not found");
    return;
  }

  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static void rna_ChannelBag_fcurve_clear(ID *dna_action_id,
                                        ActionChannelBag *dna_channelbag,
                                        bContext *C)
{
  dna_channelbag->wrap().fcurves_clear();
  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static ActionChannelBag *rna_KeyframeActionStrip_channels(KeyframeActionStrip *self,
                                                          const animrig::slot_handle_t slot_handle)
{
  animrig::KeyframeStrip &key_strip = self->wrap();
  return key_strip.channelbag_for_slot(slot_handle);
}

#  endif  // WITH_ANIM_BAKLAVA

static void rna_ActionGroup_channels_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  FCurve *fcu = (FCurve *)internal->link;
  bActionGroup *grp = fcu->grp;

  /* only continue if the next F-Curve (if existent) belongs in the same group */
  if ((fcu->next) && (fcu->next->grp == grp)) {
    internal->link = (Link *)fcu->next;
  }
  else {
    internal->link = nullptr;
  }

  iter->valid = (internal->link != nullptr);
}

static bActionGroup *rna_Action_groups_new(bAction *act, ReportList *reports, const char name[])
{
  if (!act->wrap().is_action_legacy()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot add legacy Action Groups to a layered Action '%s'. Convert it to a legacy "
                "Action first.",
                act->id.name + 2);
    return nullptr;
  }

  return action_groups_add_new(act, name);
}

static void rna_Action_groups_remove(bAction *act, ReportList *reports, PointerRNA *agrp_ptr)
{
  bActionGroup *agrp = static_cast<bActionGroup *>(agrp_ptr->data);
  FCurve *fcu, *fcn;

  /* try to remove the F-Curve from the action */
  if (BLI_remlink_safe(&act->groups, agrp) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Action group '%s' not found in action '%s'",
                agrp->name,
                act->id.name + 2);
    return;
  }

  /* Move every one of the group's F-Curves out into the Action again. */
  for (fcu = static_cast<FCurve *>(agrp->channels.first); (fcu) && (fcu->grp == agrp); fcu = fcn) {
    fcn = fcu->next;

    /* remove from group */
    action_groups_remove_channel(act, fcu);

    /* tack onto the end */
    BLI_addtail(&act->curves, fcu);
  }

  MEM_freeN(agrp);
  RNA_POINTER_INVALIDATE(agrp_ptr);

  DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static FCurve *rna_Action_fcurve_new(bAction *act,
                                     Main *bmain,
                                     ReportList *reports,
                                     const char *data_path,
                                     int index,
                                     const char *group)
{
  if (!act->wrap().is_action_legacy()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot add legacy F-Curves to a layered Action '%s'. Convert it to a legacy "
                "Action first.",
                act->id.name + 2);
    return nullptr;
  }

  if (group && group[0] == '\0') {
    group = nullptr;
  }

  BLI_assert(data_path != nullptr);
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  /* Annoying, check if this exists. */
  if (blender::animrig::action_fcurve_find(act, {data_path, index})) {
    BKE_reportf(reports,
                RPT_ERROR,
                "F-Curve '%s[%d]' already exists in action '%s'",
                data_path,
                index,
                act->id.name + 2);
    return nullptr;
  }
  return blender::animrig::action_fcurve_ensure(bmain, act, group, nullptr, {data_path, index});
}

static FCurve *rna_Action_fcurve_find(bAction *act,
                                      ReportList *reports,
                                      const char *data_path,
                                      int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  /* Returns nullptr if not found. */
  return BKE_fcurve_find(&act->curves, data_path, index);
}

static void rna_Action_fcurve_remove(bAction *act, ReportList *reports, PointerRNA *fcu_ptr)
{
  FCurve *fcu = static_cast<FCurve *>(fcu_ptr->data);
  if (fcu->grp) {
    if (BLI_findindex(&act->groups, fcu->grp) == -1) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "F-Curve's action group '%s' not found in action '%s'",
                  fcu->grp->name,
                  act->id.name + 2);
      return;
    }

    action_groups_remove_channel(act, fcu);
    BKE_fcurve_free(fcu);
    RNA_POINTER_INVALIDATE(fcu_ptr);
  }
  else {
    if (BLI_findindex(&act->curves, fcu) == -1) {
      BKE_reportf(reports, RPT_ERROR, "F-Curve not found in action '%s'", act->id.name + 2);
      return;
    }

    BLI_remlink(&act->curves, fcu);
    BKE_fcurve_free(fcu);
    RNA_POINTER_INVALIDATE(fcu_ptr);
  }

  DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static void rna_Action_fcurve_clear(bAction *act)
{
  BKE_action_fcurves_clear(act);
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static TimeMarker *rna_Action_pose_markers_new(bAction *act, const char name[])
{
  TimeMarker *marker = static_cast<TimeMarker *>(MEM_callocN(sizeof(TimeMarker), "TimeMarker"));
  marker->flag = SELECT;
  marker->frame = 1;
  STRNCPY_UTF8(marker->name, name);
  BLI_addtail(&act->markers, marker);
  return marker;
}

static void rna_Action_pose_markers_remove(bAction *act,
                                           ReportList *reports,
                                           PointerRNA *marker_ptr)
{
  TimeMarker *marker = static_cast<TimeMarker *>(marker_ptr->data);
  if (!BLI_remlink_safe(&act->markers, marker)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Timeline marker '%s' not found in action '%s'",
                marker->name,
                act->id.name + 2);
    return;
  }

  MEM_freeN(marker);
  RNA_POINTER_INVALIDATE(marker_ptr);
}

static PointerRNA rna_Action_active_pose_marker_get(PointerRNA *ptr)
{
  bAction *act = (bAction *)ptr->data;
  return rna_pointer_inherit_refine(
      ptr, &RNA_TimelineMarker, BLI_findlink(&act->markers, act->active_marker - 1));
}

static void rna_Action_active_pose_marker_set(PointerRNA *ptr,
                                              PointerRNA value,
                                              ReportList * /*reports*/)
{
  bAction *act = (bAction *)ptr->data;
  act->active_marker = BLI_findindex(&act->markers, value.data) + 1;
}

static int rna_Action_active_pose_marker_index_get(PointerRNA *ptr)
{
  bAction *act = (bAction *)ptr->data;
  return std::max(act->active_marker - 1, 0);
}

static void rna_Action_active_pose_marker_index_set(PointerRNA *ptr, int value)
{
  bAction *act = (bAction *)ptr->data;
  act->active_marker = value + 1;
}

static void rna_Action_active_pose_marker_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  bAction *act = (bAction *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&act->markers) - 1);
}

#  ifdef WITH_ANIM_BAKLAVA
static bool rna_Action_is_empty_get(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  return action.is_empty();
}
static bool rna_Action_is_action_legacy_get(PointerRNA *ptr)
{
  return rna_action(ptr).is_action_legacy();
}
static bool rna_Action_is_action_layered_get(PointerRNA *ptr)
{
  return rna_action(ptr).is_action_layered();
}
#  endif  // WITH_ANIM_BAKLAVA

static void rna_Action_frame_range_get(PointerRNA *ptr, float *r_values)
{
  BKE_action_frame_range_get((bAction *)ptr->owner_id, &r_values[0], &r_values[1]);
}

static void rna_Action_frame_range_set(PointerRNA *ptr, const float *values)
{
  bAction *data = (bAction *)ptr->owner_id;

  data->flag |= ACT_FRAME_RANGE;
  data->frame_start = values[0];
  data->frame_end = values[1];
  CLAMP_MIN(data->frame_end, data->frame_start);
}

static void rna_Action_curve_frame_range_get(PointerRNA *ptr, float *values)
{ /* don't include modifiers because they too easily can have very large
   * ranges: MINAFRAMEF to MAXFRAMEF. */
  BKE_action_frame_range_calc((bAction *)ptr->owner_id, false, values, values + 1);
}

static void rna_Action_use_frame_range_set(PointerRNA *ptr, bool value)
{
  bAction *data = (bAction *)ptr->owner_id;

  if (value) {
    /* If the frame range is blank, initialize it by scanning F-Curves. */
    if ((data->frame_start == data->frame_end) && (data->frame_start == 0)) {
      BKE_action_frame_range_calc(data, false, &data->frame_start, &data->frame_end);
    }

    data->flag |= ACT_FRAME_RANGE;
  }
  else {
    data->flag &= ~ACT_FRAME_RANGE;
  }
}

static void rna_Action_start_frame_set(PointerRNA *ptr, float value)
{
  bAction *data = (bAction *)ptr->owner_id;

  data->frame_start = value;
  CLAMP_MIN(data->frame_end, data->frame_start);
}

static void rna_Action_end_frame_set(PointerRNA *ptr, float value)
{
  bAction *data = (bAction *)ptr->owner_id;

  data->frame_end = value;
  CLAMP_MAX(data->frame_start, data->frame_end);
}

static void rna_Action_deselect_keys(bAction *act)
{
  animrig::action_deselect_keys(act->wrap());
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

/* Used to check if an action (value pointer)
 * is suitable to be assigned to the ID-block that is ptr. */
bool rna_Action_id_poll(PointerRNA *ptr, PointerRNA value)
{
  ID *srcId = ptr->owner_id;
  bAction *act = (bAction *)value.owner_id;

  if (act) {
    /* there can still be actions that will have undefined id-root
     * (i.e. floating "action-library" members) which we will not
     * be able to resolve an idroot for automatically, so let these through
     */
    if (act->idroot == 0) {
      return 1;
    }
    else if (srcId) {
      return GS(srcId->name) == act->idroot;
    }
  }

  return 0;
}

/* Used to check if an action (value pointer)
 * can be assigned to Action Editor given current mode. */
bool rna_Action_actedit_assign_poll(PointerRNA *ptr, PointerRNA value)
{
  SpaceAction *saction = (SpaceAction *)ptr->data;
  bAction *action = (bAction *)value.owner_id;

  if (!saction) {
    /* Unable to determine what this Action is going to be assigned to, so
     * reject it for now. This is mostly to have a non-functional refactor of
     * this code; personally I (Sybren) wouldn't mind to always return `true` in
     * this case. */
    return false;
  }

  switch (saction->mode) {
    case SACTCONT_ACTION:
      return blender::animrig::is_action_assignable_to(action, ID_OB);
    case SACTCONT_SHAPEKEY:
      return blender::animrig::is_action_assignable_to(action, ID_KE);
    case SACTCONT_GPENCIL:
    case SACTCONT_DOPESHEET:
    case SACTCONT_MASK:
    case SACTCONT_CACHEFILE:
    case SACTCONT_TIMELINE:
      break;
  }

  /* Same as above, I (Sybren) wouldn't mind returning `true` here to just
   * always show all Actions in an unexpected place. */
  return false;
}

/* All FCurves need to be validated when the "show_only_errors" button is enabled. */
static void rna_Action_show_errors_update(bContext *C, PointerRNA * /*ptr*/)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  if (!(ac.ads->filterflag & ADS_FILTER_ONLY_ERRORS)) {
    return;
  }

  blender::animrig::reevaluate_fcurve_errors(&ac);
}

static std::optional<std::string> rna_DopeSheet_path(const PointerRNA * /*ptr*/)
{
  return "dopesheet";
}

#else

static void rna_def_dopesheet(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DopeSheet", nullptr);
  RNA_def_struct_sdna(srna, "bDopeSheet");
  RNA_def_struct_path_func(srna, "rna_DopeSheet_path");
  RNA_def_struct_ui_text(
      srna, "Dope Sheet", "Settings for filtering the channels shown in animation editors");

  /* Source of DopeSheet data */
  /* XXX: make this obsolete? */
  prop = RNA_def_property(srna, "source", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_ui_text(
      prop, "Source", "ID-Block representing source data, usually ID_SCE (i.e. Scene)");

  /* Show data-block filters */
  prop = RNA_def_property(srna, "show_datablock_filters", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADS_FLAG_SHOW_DBFILTERS);
  RNA_def_property_ui_text(
      prop,
      "Show Data-Block Filters",
      "Show options for whether channels related to certain types of data are included");
  RNA_def_property_ui_icon(prop, ICON_RIGHTARROW, 1);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, nullptr);

  /* General Filtering Settings */
  prop = RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_ONLYSEL);
  RNA_def_property_ui_text(
      prop, "Only Show Selected", "Only include channels relating to selected objects and data");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_all_slots", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_ALL_SLOTS);
  RNA_def_property_ui_text(prop, "Show All Slots", "Show all the Action's Slots");
  RNA_def_property_ui_icon(prop, ICON_LINKED, 0); /* TODO: select icon for Slots. */
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_INCL_HIDDEN);
  RNA_def_property_ui_text(
      prop, "Show Hidden", "Include channels from objects/bone that are not visible");
  RNA_def_property_ui_icon(prop, ICON_OBJECT_HIDDEN, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_datablock_sort", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", ADS_FLAG_NO_DB_SORT);
  RNA_def_property_ui_text(prop,
                           "Sort Data-Blocks",
                           "Alphabetically sorts data-blocks - mainly objects in the scene "
                           "(disable to increase viewport speed)");
  RNA_def_property_ui_icon(prop, ICON_SORTALPHA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_filter_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADS_FLAG_INVERT_FILTER);
  RNA_def_property_ui_text(prop, "Invert", "Invert filter search");
  RNA_def_property_ui_icon(prop, ICON_ZOOM_IN, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* Debug Filtering Settings */
  prop = RNA_def_property(srna, "show_only_errors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_ONLY_ERRORS);
  RNA_def_property_ui_text(prop,
                           "Only Show Errors",
                           "Only include F-Curves and drivers that are disabled or have errors");
  RNA_def_property_ui_icon(prop, ICON_ERROR, 0);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, "rna_Action_show_errors_update");

  /* Object Collection Filtering Settings */
  prop = RNA_def_property(srna, "filter_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "filter_grp");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Filtering Collection", "Collection that included object should be a member of");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* FCurve Display Name Search Settings */
  prop = RNA_def_property(srna, "filter_fcurve_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "searchstr");
  RNA_def_property_ui_text(prop, "F-Curve Name Filter", "F-Curve live filtering string");
  RNA_def_property_ui_icon(prop, ICON_VIEWZOOM, 0);
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* NLA Name Search Settings (Shared with FCurve setting, but with different labels) */
  prop = RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "searchstr");
  RNA_def_property_ui_text(prop, "Name Filter", "Live filtering string");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_ui_icon(prop, ICON_VIEWZOOM, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* Multi-word fuzzy search option for name/text filters */
  prop = RNA_def_property(srna, "use_multi_word_filter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADS_FLAG_FUZZY_NAMES);
  RNA_def_property_ui_text(prop,
                           "Multi-Word Fuzzy Filter",
                           "Perform fuzzy/multi-word matching.\n"
                           "Warning: May be slow");
  RNA_def_property_ui_icon(prop, ICON_SORTALPHA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* NLA Specific Settings */
  prop = RNA_def_property(srna, "show_missing_nla", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NLA_NOACT);
  RNA_def_property_ui_text(prop,
                           "Include Missing NLA",
                           "Include animation data-blocks with no NLA data (NLA editor only)");
  RNA_def_property_ui_icon(prop, ICON_ACTION, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* Summary Settings (DopeSheet editors only) */
  prop = RNA_def_property(srna, "show_summary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_SUMMARY);
  RNA_def_property_ui_text(
      prop, "Display Summary", "Display an additional 'summary' line (Dope Sheet editors only)");
  RNA_def_property_ui_icon(prop, ICON_BORDERMOVE, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_expanded_summary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", ADS_FLAG_SUMMARY_COLLAPSED);
  RNA_def_property_ui_text(
      prop,
      "Collapse Summary",
      "Collapse summary when shown, so all other channels get hidden (Dope Sheet editors only)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* General DataType Filtering Settings */
  prop = RNA_def_property(srna, "show_transforms", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOOBJ);
  RNA_def_property_ui_text(
      prop,
      "Display Transforms",
      "Include visualization of object-level animation data (mostly transforms)");
  RNA_def_property_ui_icon(prop, ICON_ORIENTATION_GLOBAL, 0); /* XXX? */
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_shapekeys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOSHAPEKEYS);
  RNA_def_property_ui_text(
      prop, "Display Shape Keys", "Include visualization of shape key related animation data");
  RNA_def_property_ui_icon(prop, ICON_SHAPEKEY_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_modifiers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOMODIFIERS);
  RNA_def_property_ui_text(
      prop,
      "Display Modifier Data",
      "Include visualization of animation data related to data-blocks linked to modifiers");
  RNA_def_property_ui_icon(prop, ICON_MODIFIER_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_meshes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOMESH);
  RNA_def_property_ui_text(
      prop, "Display Meshes", "Include visualization of mesh related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_MESH, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_lattices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOLAT);
  RNA_def_property_ui_text(
      prop, "Display Lattices", "Include visualization of lattice related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_LATTICE, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_cameras", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOCAM);
  RNA_def_property_ui_text(
      prop, "Display Camera", "Include visualization of camera related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_CAMERA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOMAT);
  RNA_def_property_ui_text(
      prop, "Display Material", "Include visualization of material related animation data");
  RNA_def_property_ui_icon(prop, ICON_MATERIAL_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOLAM);
  RNA_def_property_ui_text(
      prop, "Display Light", "Include visualization of light related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_LIGHT, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_linestyles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOLINESTYLE);
  RNA_def_property_ui_text(
      prop, "Display Line Style", "Include visualization of Line Style related Animation data");
  RNA_def_property_ui_icon(prop, ICON_LINE_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_textures", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOTEX);
  RNA_def_property_ui_text(
      prop, "Display Texture", "Include visualization of texture related animation data");
  RNA_def_property_ui_icon(prop, ICON_TEXTURE_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOCUR);
  RNA_def_property_ui_text(
      prop, "Display Curve", "Include visualization of curve related animation data");
  RNA_def_property_ui_icon(prop, ICON_CURVE_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_worlds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOWOR);
  RNA_def_property_ui_text(
      prop, "Display World", "Include visualization of world related animation data");
  RNA_def_property_ui_icon(prop, ICON_WORLD_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_scenes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOSCE);
  RNA_def_property_ui_text(
      prop, "Display Scene", "Include visualization of scene related animation data");
  RNA_def_property_ui_icon(prop, ICON_SCENE_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOPART);
  RNA_def_property_ui_text(
      prop, "Display Particle", "Include visualization of particle related animation data");
  RNA_def_property_ui_icon(prop, ICON_PARTICLE_DATA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_metaballs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOMBA);
  RNA_def_property_ui_text(
      prop, "Display Metaball", "Include visualization of metaball related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_META, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_armatures", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOARM);
  RNA_def_property_ui_text(
      prop, "Display Armature", "Include visualization of armature related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_ARMATURE, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NONTREE);
  RNA_def_property_ui_text(
      prop, "Display Node", "Include visualization of node related animation data");
  RNA_def_property_ui_icon(prop, ICON_NODETREE, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_speakers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOSPK);
  RNA_def_property_ui_text(
      prop, "Display Speaker", "Include visualization of speaker related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_SPEAKER, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_cache_files", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOCACHEFILES);
  RNA_def_property_ui_text(
      prop, "Display Cache Files", "Include visualization of cache file related animation data");
  RNA_def_property_ui_icon(prop, ICON_FILE, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_hair_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOHAIR);
  RNA_def_property_ui_text(
      prop, "Display Hair", "Include visualization of hair related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_CURVES, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_pointclouds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOPOINTCLOUD);
  RNA_def_property_ui_text(
      prop, "Display Point Cloud", "Include visualization of point cloud related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_POINTCLOUD, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_volumes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOVOLUME);
  RNA_def_property_ui_text(
      prop, "Display Volume", "Include visualization of volume related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_VOLUME, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_gpencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag", ADS_FILTER_NOGPENCIL);
  RNA_def_property_ui_text(
      prop,
      "Display Grease Pencil",
      "Include visualization of Grease Pencil related animation data and frames");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_GREASEPENCIL, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_movieclips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOMOVIECLIPS);
  RNA_def_property_ui_text(
      prop, "Display Movie Clips", "Include visualization of movie clip related animation data");
  RNA_def_property_ui_icon(prop, ICON_TRACKER, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_driver_fallback_as_error", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag2", ADS_FILTER_DRIVER_FALLBACK_AS_ERROR);
  RNA_def_property_ui_text(
      prop,
      "Variable Fallback As Error",
      "Include drivers that relied on any fallback values for their evaluation "
      "in the Only Show Errors filter, even if the driver evaluation succeeded");
  RNA_def_property_ui_icon(prop, ICON_RNA, 0);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
}

/* =========================== Layered Action interface =========================== */

#  ifdef WITH_ANIM_BAKLAVA

static void rna_def_action_slots(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionSlots");
  srna = RNA_def_struct(brna, "ActionSlots", nullptr);
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action Slots", "Collection of action slots");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_ActionSlots_active_get", "rna_ActionSlots_active_set", nullptr, nullptr);
  RNA_def_property_update_notifier(prop, NC_ANIMATION | ND_ANIMCHAN);

  RNA_def_property_ui_text(prop, "Active Slot", "Active slot for this action");

  /* Animation.slots.new(...) */
  func = RNA_def_function(srna, "new", "rna_Action_slots_new");
  RNA_def_function_ui_description(func, "Add a slot to the animation");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func,
      "for_id",
      "ID",
      "Data-Block",
      "If given, the new slot will be named after this data-block, and limited to animating "
      "data-blocks of its type. If ommitted, limiting the ID type will happen as soon as the "
      "slot is assigned");
  /* Clear out the PARM_REQUIRED flag, which is set by default for pointer parameters. */
  RNA_def_parameter_flags(parm, PropertyFlag(0), ParameterFlag(0));

  parm = RNA_def_pointer(func, "slot", "ActionSlot", "", "Newly created action slot");
  RNA_def_function_return(func, parm);
}

static void rna_def_action_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionLayers");
  srna = RNA_def_struct(brna, "ActionLayers", nullptr);
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action Layers", "Collection of animation layers");

  /* Animation.layers.new(...) */
  func = RNA_def_function(srna, "new", "rna_Action_layers_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func,
      "Add a layer to the Animation. Currently an Animation can only have at most one layer");
  parm = RNA_def_string(func,
                        "name",
                        nullptr,
                        sizeof(ActionLayer::name) - 1,
                        "Name",
                        "Name of the layer, will be made unique within the Action");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layer", "ActionLayer", "", "Newly created animation layer");
  RNA_def_function_return(func, parm);

  /* Animation.layers.remove(layer) */
  func = RNA_def_function(srna, "remove", "rna_Action_layers_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the layer from the animation");
  parm = RNA_def_pointer(
      func, "anim_layer", "ActionLayer", "Animation Layer", "The layer to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_action_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionSlot", nullptr);
  RNA_def_struct_path_func(srna, "rna_ActionSlot_path");
  RNA_def_struct_ui_text(
      srna,
      "Action slot",
      "Identifier for a set of channels in this Action, that can be used by a data-block "
      "to specify what it gets animated by");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ActionSlot_name_set");
  RNA_def_property_string_maxlength(prop, sizeof(ActionSlot::name) - 2);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_ActionSlot_name_update");
  RNA_def_property_ui_text(
      prop,
      "Slot Name",
      "Used when connecting an Action to a data-block, to find the correct slot handle");

  prop = RNA_def_property(srna, "idtype_icon", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_ActionSlot_idtype_icon_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name_display", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ActionSlot_name_display_get",
                                "rna_ActionSlot_name_display_length",
                                "rna_ActionSlot_name_display_set");
  RNA_def_property_string_maxlength(prop, sizeof(ActionSlot::name) - 2);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_ActionSlot_name_update");
  RNA_def_property_ui_text(
      prop,
      "Slot Display Name",
      "Name of the slot for showing in the interface. It is the name, without the first two "
      "characters that identify what kind of data-block it animates");

  prop = RNA_def_property(srna, "handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Slot Handle",
                           "Number specific to this Slot, unique within the Action"
                           "This is used, for example, on a KeyframeActionStrip to look up the "
                           "ActionChannelBag for this Slot");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "slot_flags", int(animrig::Slot::Flags::Active));
  RNA_def_property_ui_text(
      prop,
      "Active",
      "Whether this is the active slot, can be set by assigning to action.slots.active");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_update_notifier(prop, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED);

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "slot_flags", int(animrig::Slot::Flags::Selected));
  RNA_def_property_ui_text(prop, "Select", "Selection state of the slot");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update_notifier(prop, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "slot_flags", int(animrig::Slot::Flags::Expanded));
  RNA_def_property_ui_text(prop, "Show Expanded", "Expanded state of the slot");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update_notifier(prop, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED);
}

static void rna_def_ActionLayer_strips(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionStrips");
  srna = RNA_def_struct(brna, "ActionStrips", nullptr);
  RNA_def_struct_sdna(srna, "ActionLayer");
  RNA_def_struct_ui_text(srna, "Action Strips", "Collection of animation strips");

  /* Layer.strips.new(type='...') */
  func = RNA_def_function(srna, "new", "rna_ActionStrips_new");
  RNA_def_function_ui_description(func,
                                  "Add a new strip to the layer. Currently a layer can only have "
                                  "one strip, with infinite boundaries");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_strip_type_items,
                      int(animrig::Strip::Type::Keyframe),
                      "Type",
                      "The type of strip to create");
  /* Return value. */
  parm = RNA_def_pointer(func, "strip", "ActionStrip", "", "Newly created animation strip");
  RNA_def_function_return(func, parm);

  /* Layer.strips.remove(strip) */
  func = RNA_def_function(srna, "remove", "rna_ActionStrips_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the strip from the animation layer");
  parm = RNA_def_pointer(
      func, "anim_strip", "ActionStrip", "Animation Strip", "The strip to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_action_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionLayer", nullptr);
  RNA_def_struct_ui_text(srna, "Action Layer", "");
  RNA_def_struct_path_func(srna, "rna_ActionLayer_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Influence", "How much of this layer is used when blending into the lower layers");
  RNA_def_property_ui_range(prop, 0.0, 1.0, 3, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_Action_tag_animupdate");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "layer_mix_mode");
  RNA_def_property_ui_text(
      prop, "Mix Mode", "How animation of this layer is blended into the lower layers");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_enum_items(prop, rna_enum_layer_mix_mode_items);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_Action_tag_animupdate");

  /* Collection properties .*/
  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionStrip");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_ActionLayer_strips_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_ActionLayer_strips_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Strips", "The list of strips that are on this animation layer");

  rna_def_ActionLayer_strips(brna, prop);
}

static void rna_def_keyframestrip_channelbags(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionChannelBags");
  srna = RNA_def_struct(brna, "ActionChannelBags", nullptr);
  RNA_def_struct_sdna(srna, "KeyframeActionStrip");
  RNA_def_struct_ui_text(
      srna,
      "Animation Channels for Slots",
      "For each action slot, a list of animation channels that are meant for that slot");

  /* KeyframeStrip.channelbags.new(slot=...) */
  func = RNA_def_function(srna, "new", "rna_ChannelBags_new");
  RNA_def_function_ui_description(
      func,
      "Add a new channelbag to the strip, to contain animation channels for a specific slot");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func,
                         "slot",
                         "ActionSlot",
                         "Action Slot",
                         "The slot that should be animated by this channelbag");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Return value. */
  parm = RNA_def_pointer(func, "channelbag", "ActionChannelBag", "", "Newly created channelbag");
  RNA_def_function_return(func, parm);

  /* KeyframeStrip.channelbags.remove(strip) */
  func = RNA_def_function(srna, "remove", "rna_ChannelBags_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the channelbag from the strip");
  parm = RNA_def_pointer(func, "channelbag", "ActionChannelBag", "", "The channelbag to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_action_keyframe_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyframeActionStrip", "ActionStrip");
  RNA_def_struct_ui_text(
      srna, "Keyframe Animation Strip", "Strip with a set of F-Curves for each action slot");

  prop = RNA_def_property(srna, "channelbags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionChannelBag");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_keyframestrip_channelbags_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_keyframestrip_channelbags_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_keyframestrip_channelbags(brna, prop);

  {
    FunctionRNA *func;
    PropertyRNA *parm;

    /* KeyframeStrip.channels(...). */
    func = RNA_def_function(srna, "channels", "rna_KeyframeActionStrip_channels");
    RNA_def_function_ui_description(func, "Find the ActionChannelBag for a specific Slot");
    parm = RNA_def_int(func,
                       "slot_handle",
                       0,
                       0,
                       INT_MAX,
                       "Slot Handle",
                       "Number that identifies a specific action slot",
                       0,
                       INT_MAX);
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
    parm = RNA_def_pointer(func, "channels", "ActionChannelBag", "Channels", "");
    RNA_def_function_return(func, parm);

    /* KeyframeStrip.key_insert(...). */

    func = RNA_def_function(srna, "key_insert", "rna_KeyframeActionStrip_key_insert");
    RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
    parm = RNA_def_pointer(func,
                           "slot",
                           "ActionSlot",
                           "Slot",
                           "The slot that identifies which 'thing' should be keyed");
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

    parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

    parm = RNA_def_int(
        func,
        "array_index",
        -1,
        -INT_MAX,
        INT_MAX,
        "Array Index",
        "Index of the animated array element, or -1 if the property is not an array",
        -1,
        4);
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

    parm = RNA_def_float(func,
                         "value",
                         0.0,
                         -FLT_MAX,
                         FLT_MAX,
                         "Value to key",
                         "Value of the animated property",
                         -FLT_MAX,
                         FLT_MAX);
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

    parm = RNA_def_float(func,
                         "time",
                         0.0,
                         -FLT_MAX,
                         FLT_MAX,
                         "Time of the key",
                         "Time, in frames, of the key",
                         -FLT_MAX,
                         FLT_MAX);
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

    parm = RNA_def_boolean(
        func, "success", true, "Success", "Whether the key was successfully inserted");

    RNA_def_function_return(func, parm);
  }
}

static void rna_def_action_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionStrip", nullptr);
  RNA_def_struct_ui_text(srna, "Action Strip", "");
  RNA_def_struct_path_func(srna, "rna_ActionStrip_path");
  RNA_def_struct_refine_func(srna, "rna_ActionStrip_refine");

  static const EnumPropertyItem prop_type_items[] = {
      {int(animrig::Strip::Type::Keyframe),
       "KEYFRAME",
       0,
       "Keyframe",
       "Strip with a set of F-Curves for each action slot"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "strip_type");
  RNA_def_property_enum_items(prop, prop_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Define Strip subclasses. */
  rna_def_action_keyframe_strip(brna);
}

static void rna_def_channelbag_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionChannelBagFCurves");
  srna = RNA_def_struct(brna, "ActionChannelBagFCurves", nullptr);
  RNA_def_struct_sdna(srna, "ActionChannelBag");
  RNA_def_struct_ui_text(
      srna, "F-Curves", "Collection of F-Curves for a specific action slot, on a specific strip");

  /* ChannelBag.fcurves.new(...) */
  extern struct FCurve *ActionChannelBagFCurves_new_func(struct ID * _selfid,
                                                         struct ActionChannelBag * _self,
                                                         Main * bmain,
                                                         ReportList * reports,
                                                         const char *data_path,
                                                         int index);

  func = RNA_def_function(srna, "new", "rna_ChannelBag_fcurve_new");
  RNA_def_function_ui_description(func, "Add an F-Curve to the channelbag");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);

  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Newly created F-Curve");
  RNA_def_function_return(func, parm);

  /* ChannelBag.fcurves.find(...) */
  func = RNA_def_function(srna, "find", "rna_ChannelBag_fcurve_find");
  RNA_def_function_ui_description(
      func,
      "Find an F-Curve. Note that this function performs a linear scan "
      "of all F-Curves in the channelbag.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it doesn't exist");
  RNA_def_function_return(func, parm);

  /* ChannelBag.fcurves.remove(...) */
  func = RNA_def_function(srna, "remove", "rna_ChannelBag_fcurve_remove");
  RNA_def_function_ui_description(func, "Remove F-Curve");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "F-Curve to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* ChannelBag.fcurves.clear() */
  func = RNA_def_function(srna, "clear", "rna_ChannelBag_fcurve_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove all F-Curves from this channelbag");
}

static void rna_def_action_channelbag(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionChannelBag", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Animation Channel Bag",
      "Collection of animation channels, typically associated with an action slot");
  RNA_def_struct_path_func(srna, "rna_ChannelBag_path");

  prop = RNA_def_property(srna, "slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_ChannelBag_fcurves_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_ChannelBag_fcurves_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that animate the slot");
  rna_def_channelbag_fcurves(brna, prop);
}
#  endif  // WITH_ANIM_BAKLAVA

/* =========================== Legacy Action interface =========================== */

static void rna_def_action_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionGroup", nullptr);
  RNA_def_struct_sdna(srna, "bActionGroup");
  RNA_def_struct_ui_text(srna, "Action Group", "Groups of F-Curves");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* WARNING: be very careful when working with this list, since the endpoint is not
   * defined like a standard ListBase. Adding/removing channels from this list needs
   * extreme care, otherwise the F-Curve list running through adjacent groups does
   * not match up with the one stored in the Action, resulting in curves which do not
   * show up in animation editors. In extreme cases, animation may also selectively
   * fail to play back correctly.
   *
   * If such changes are required, these MUST go through the API functions for manipulating
   * these F-Curve groupings. Also, note that groups only apply in actions ONLY.
   */
  prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "channels", nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    "rna_ActionGroup_channels_next",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Channels", "F-Curves in this group");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", AGRP_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "Action group is selected");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", AGRP_PROTECTED);
  RNA_def_property_ui_text(prop, "Lock", "Action group is locked");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", AGRP_MUTED);
  RNA_def_property_ui_text(prop, "Mute", "Action group is muted");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", AGRP_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "Action group is expanded except in graph editor");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "show_expanded_graph", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", AGRP_EXPANDED_G);
  RNA_def_property_ui_text(
      prop, "Expanded in Graph Editor", "Action group is expanded in graph editor");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADT_CURVES_ALWAYS_VISIBLE);
  RNA_def_property_ui_text(prop, "Pin in Graph Editor", "");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* color set */
  rna_def_actionbone_group_common(srna, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
}

/* fcurve.keyframe_points */
static void rna_def_action_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionGroups");
  srna = RNA_def_struct(brna, "ActionGroups", nullptr);
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action Groups", "Collection of action groups");

  func = RNA_def_function(srna, "new", "rna_Action_groups_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Create a new action group and add it to the action");
  parm = RNA_def_string(func, "name", "Group", 0, "", "New name for the action group");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Newly created action group");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Action_groups_remove");
  RNA_def_function_ui_description(func, "Remove action group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Action group to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_action_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionFCurves");
  srna = RNA_def_struct(brna, "ActionFCurves", nullptr);
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action F-Curves", "Collection of action F-Curves");

  /* Action.fcurves.new(...) */
  func = RNA_def_function(srna, "new", "rna_Action_fcurve_new");
  RNA_def_function_ui_description(func, "Add an F-Curve to the action");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  RNA_def_string(
      func, "action_group", nullptr, 0, "Action Group", "Acton group to add this F-Curve into");

  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Newly created F-Curve");
  RNA_def_function_return(func, parm);

  /* Action.fcurves.find(...) */
  func = RNA_def_function(srna, "find", "rna_Action_fcurve_find");
  RNA_def_function_ui_description(
      func,
      "Find an F-Curve. Note that this function performs a linear scan "
      "of all F-Curves in the action.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it doesn't exist");
  RNA_def_function_return(func, parm);

  /* Action.fcurves.remove(...) */
  func = RNA_def_function(srna, "remove", "rna_Action_fcurve_remove");
  RNA_def_function_ui_description(func, "Remove F-Curve");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "F-Curve to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Action.fcurves.clear() */
  func = RNA_def_function(srna, "clear", "rna_Action_fcurve_clear");
  RNA_def_function_ui_description(func, "Remove all F-Curves");
}

static void rna_def_action_pose_markers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionPoseMarkers");
  srna = RNA_def_struct(brna, "ActionPoseMarkers", nullptr);
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action Pose Markers", "Collection of timeline markers");

  func = RNA_def_function(srna, "new", "rna_Action_pose_markers_new");
  RNA_def_function_ui_description(func, "Add a pose marker to the action");
  parm = RNA_def_string(
      func, "name", "Marker", 0, nullptr, "New name for the marker (not unique)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Newly created marker");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Action_pose_markers_remove");
  RNA_def_function_ui_description(func, "Remove a timeline marker");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Timeline marker to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "TimelineMarker");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_LIB_EXCEPTION);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Action_active_pose_marker_get",
                                 "rna_Action_active_pose_marker_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop, "Active Pose Marker", "Active pose marker for this action");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_marker");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_int_funcs(prop,
                             "rna_Action_active_pose_marker_index_get",
                             "rna_Action_active_pose_marker_index_set",
                             "rna_Action_active_pose_marker_index_range");
  RNA_def_property_ui_text(prop, "Active Pose Marker Index", "Index of active pose marker");
}

/* Access to 'legacy' Action features, like the top-level F-Curves, the corresponding F-Curve
 * groups, and the top-level id_root. */
static void rna_def_action_legacy(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  /* collections */
  prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "curves", nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that make up the action");
  rna_def_action_fcurves(brna, prop);

  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "groups", nullptr);
  RNA_def_property_struct_type(prop, "ActionGroup");
  RNA_def_property_ui_text(prop, "Groups", "Convenient groupings of F-Curves");
  rna_def_action_groups(brna, prop);

  /* special "type" limiter - should not really be edited in general,
   * but is still available/editable in 'emergencies' */
  prop = RNA_def_property(srna, "id_root", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "idroot");
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_ui_text(prop,
                           "ID Root Type",
                           "Type of ID block that action can be used on - "
                           "DO NOT CHANGE UNLESS YOU KNOW WHAT YOU ARE DOING");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
}

static void rna_def_action(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Action", "ID");
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action", "A collection of F-Curves for animation");
  RNA_def_struct_ui_icon(srna, ICON_ACTION);

#  ifdef WITH_ANIM_BAKLAVA
  /* Properties. */
  prop = RNA_def_property(srna, "last_slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_empty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Empty", "False when there is any Layer, Slot, or legacy F-Curve");
  RNA_def_property_boolean_funcs(prop, "rna_Action_is_empty_get", nullptr);

  prop = RNA_def_property(srna, "is_action_legacy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Is Legacy Action",
      "Return whether this is a legacy Action. Legacy Actions have no layers or slots. An "
      "empty Action considered as both a 'legacy' and a 'layered' Action");
  RNA_def_property_boolean_funcs(prop, "rna_Action_is_action_legacy_get", nullptr);

  prop = RNA_def_property(srna, "is_action_layered", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Is Layered Action",
                           "Return whether this is a layered Action. An empty Action considered "
                           "as both a 'layered' and a 'layered' Action");
  RNA_def_property_boolean_funcs(prop, "rna_Action_is_action_layered_get", nullptr);
#  endif  // WITH_ANIM_BAKLAVA

  /* Collection properties. */

#  ifdef WITH_ANIM_BAKLAVA
  prop = RNA_def_property(srna, "slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_animation_slots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_animation_slots_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Slots", "The list of slots in this Action");
  rna_def_action_slots(brna, prop);

  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionLayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_action_layers_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_action_layers_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layers", "The list of layers that make up this Action");
  rna_def_action_layers(brna, prop);
#  endif  // WITH_ANIM_BAKLAVA

  prop = RNA_def_property(srna, "pose_markers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "markers", nullptr);
  RNA_def_property_struct_type(prop, "TimelineMarker");
  /* Use lib exception so the list isn't grayed out;
   * adding/removing is still banned though, see #45689. */
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_ui_text(
      prop, "Pose Markers", "Markers specific to this action, for labeling poses");
  rna_def_action_pose_markers(brna, prop);

  /* properties */
  prop = RNA_def_property(srna, "use_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ACT_FRAME_RANGE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Action_use_frame_range_set");
  RNA_def_property_ui_text(
      prop,
      "Manual Frame Range",
      "Manually specify the intended playback frame range for the action "
      "(this range is used by some tools, but does not affect animation evaluation)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ACT_CYCLIC);
  RNA_def_property_ui_text(
      prop,
      "Cyclic Animation",
      "The action is intended to be used as a cycle looping over its manually set "
      "playback frame range (enabling this doesn't automatically make it loop)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "frame_start");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Action_start_frame_set", nullptr);
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Start Frame", "The start frame of the manually set intended playback range");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "frame_end");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Action_end_frame_set", nullptr);
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "End Frame", "The end frame of the manually set intended playback range");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_float_vector(
      srna,
      "frame_range",
      2,
      nullptr,
      0,
      0,
      "Frame Range",
      "The intended playback frame range of this action, using the manually set range "
      "if available, or the combined frame range of all F-Curves within this action "
      "if not (assigning sets the manual frame range)",
      0,
      0);
  RNA_def_property_float_funcs(
      prop, "rna_Action_frame_range_get", "rna_Action_frame_range_set", nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  prop = RNA_def_float_vector(srna,
                              "curve_frame_range",
                              2,
                              nullptr,
                              0,
                              0,
                              "Curve Frame Range",
                              "The combined frame range of all F-Curves within this action",
                              0,
                              0);
  RNA_def_property_float_funcs(prop, "rna_Action_curve_frame_range_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  FunctionRNA *func = RNA_def_function(srna, "deselect_keys", "rna_Action_deselect_keys");
  RNA_def_function_ui_description(
      func, "Deselects all keys of the Action. The selection status of F-Curves is unchanged");

  rna_def_action_legacy(brna, srna);

  /* API calls */
  RNA_api_action(srna);
}

/* --------- */

void RNA_def_action(BlenderRNA *brna)
{
  rna_def_action(brna);
  rna_def_action_group(brna);
  rna_def_dopesheet(brna);

#  ifdef WITH_ANIM_BAKLAVA
  rna_def_action_slot(brna);
  rna_def_action_layer(brna);
  rna_def_action_strip(brna);
  rna_def_action_channelbag(brna);
#  endif
}

#endif
