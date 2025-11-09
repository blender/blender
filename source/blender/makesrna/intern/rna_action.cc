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

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_blender.hh"
#include "BKE_fcurve.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "ANIM_action.hh"

#include "WM_types.hh"

using namespace blender;

/* Disabled for now, see comment in `rna_def_action_layer()` for more info. */
#if 0
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
#endif

const EnumPropertyItem rna_enum_strip_type_items[] = {
    {int(animrig::Strip::Type::Keyframe),
     "KEYFRAME",
     0,
     "Keyframe",
     "Strip containing keyframes on F-Curves"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Cannot use rna_enum_dummy_DEFAULT_items because the UNSPECIFIED entry needs
 * to exist as it is the default. */
const EnumPropertyItem default_ActionSlot_target_id_type_items[] = {
    {0,
     "UNSPECIFIED",
     ICON_NONE,
     "Unspecified",
     "Not yet specified. When this slot is first assigned to a data-block, this will be set to "
     "the type of that data-block"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include "BLI_math_base.h"
#  include "BLI_string.h"
#  include "BLI_string_utf8.h"

#  include "BKE_fcurve.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"

#  include "DEG_depsgraph.hh"

#  include "ANIM_action.hh"
#  include "ANIM_animdata.hh"
#  include "ED_anim_api.hh"

#  include "WM_api.hh"

#  include "UI_interface_icons.hh"

#  include "ANIM_action_legacy.hh"
#  include "ANIM_fcurve.hh"
#  include "ANIM_keyframing.hh"

#  include <fmt/format.h>

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

/* Disabled for now, see comment in `rna_def_action_layer()` for more info. */
#  if 0
static void rna_Action_tag_animupdate(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
}
#  endif

static animrig::Channelbag &rna_data_channelbag(const PointerRNA *ptr)
{
  return reinterpret_cast<ActionChannelbag *>(ptr->data)->wrap();
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter,
                                     PointerRNA *ptr,
                                     Span<T *> items)
{
  rna_iterator_array_begin(iter, ptr, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter,
                                     PointerRNA *ptr,
                                     MutableSpan<T *> items)
{
  rna_iterator_array_begin(iter, ptr, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

static PointerRNA rna_ActionSlots_active_get(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot *active_slot = action.slot_active_get();

  if (!active_slot) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&action.id, &RNA_ActionSlot, active_slot);
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

static ActionSlot *rna_Action_slots_new(
    bAction *dna_action, Main *bmain, bContext *C, ReportList *reports, int type, const char *name)
{
  animrig::Action &action = dna_action->wrap();

  if (!action.is_action_layered()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot add slots to a legacy Action '%s'. Convert it to a layered Action first.",
                action.id.name + 2);
    return nullptr;
  }

  if (name[0] == 0) {
    BKE_reportf(reports, RPT_ERROR, "Invalid slot name '%s': name must not be empty.", name);
    return nullptr;
  }

  animrig::Slot *slot = &action.slot_add_for_id_type(ID_Type(type));
  action.slot_display_name_set(*bmain, *slot, name);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return slot;
}

void rna_Action_slots_remove(bAction *dna_action,
                             bContext *C,
                             ReportList *reports,
                             PointerRNA *slot_ptr)
{
  animrig::Action &action = dna_action->wrap();
  animrig::Slot &slot = rna_data_slot(slot_ptr);
  if (!action.slot_remove(slot)) {
    BKE_report(reports, RPT_ERROR, "This slot does not belong to this Action");
    return;
  }

  slot_ptr->invalidate();
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
}

static void rna_iterator_action_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  rna_iterator_array_begin(iter, ptr, action.layers());
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

  layer_ptr->invalidate();
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
}

static void rna_iterator_animation_slots_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  rna_iterator_array_begin(iter, ptr, action.slots());
}

static int rna_iterator_animation_slots_length(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  return action.slots().size();
}

static std::optional<std::string> rna_ActionSlot_path(const PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);

  char identifier_esc[sizeof(slot.identifier) * 2];
  BLI_str_escape(identifier_esc, slot.identifier, sizeof(identifier_esc));
  return fmt::format("slots[\"{}\"]", identifier_esc);
}

int rna_ActionSlot_target_id_type_icon_get(PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  return UI_icon_from_idcode(slot.idtype);
}

/* Name functions that ignore the first two ID characters */
void rna_ActionSlot_name_display_get(PointerRNA *ptr, char *value)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  slot.identifier_without_prefix().copy_unsafe(value);
}

int rna_ActionSlot_name_display_length(PointerRNA *ptr)
{
  animrig::Slot &slot = rna_data_slot(ptr);
  return slot.identifier_without_prefix().size();
}

static void rna_ActionSlot_name_display_set(PointerRNA *ptr, const char *name)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  const StringRef name_ref(name);

  if (name_ref.is_empty()) {
    WM_global_report(RPT_ERROR, "Action slot display names cannot be empty");
    return;
  }

  action.slot_display_name_define(slot, name);
}

static void rna_ActionSlot_identifier_set(PointerRNA *ptr, const char *identifier)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  const StringRef identifier_ref(identifier);

  if (identifier_ref.size() < animrig::Slot::identifier_length_min) {
    WM_global_report(RPT_ERROR, "Action slot identifiers should be at least three characters");
    return;
  }

  /* Sanity check. These should never be out of sync in higher-level code. */
  BLI_assert(slot.idtype_string() == slot.identifier_prefix());

  const std::string identifier_with_correct_prefix = slot.idtype_string() +
                                                     identifier_ref.substr(2);

  if (identifier_with_correct_prefix != identifier_ref) {
    WM_global_reportf(
        RPT_WARNING,
        "Attempted to set slot identifier to \"%s\", but the type prefix does not match the "
        "slot's 'target_id_type' \"%s\". Setting to \"%s\" instead.\n",
        identifier,
        slot.idtype_string().c_str(),
        identifier_with_correct_prefix.c_str());
  }

  action.slot_identifier_define(slot, identifier_with_correct_prefix);
}

static void rna_ActionSlot_identifier_update(Main *bmain, Scene *, PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Slot &slot = rna_data_slot(ptr);
  action.slot_identifier_propagate(*bmain, slot);
}

static CollectionVector rna_ActionSlot_users(struct ActionSlot *self, Main *bmain)
{
  animrig::Slot &slot = self->wrap();
  const Span<ID *> slot_users = slot.users(*bmain);

  CollectionVector vector{};
  vector.items.resize(slot_users.size());
  for (const int i : slot_users.index_range()) {
    vector.items[i] = RNA_id_pointer_create(slot_users[i]);
  }

  return vector;
}

static ActionSlot *rna_ActionSlot_duplicate(ID *action_id, const ActionSlot *self)
{
  animrig::Action &action = reinterpret_cast<bAction *>(action_id)->wrap();
  const animrig::Slot &source_slot = self->wrap();

  animrig::Slot &dupli_slot = animrig::duplicate_slot(action, source_slot);
  return &dupli_slot;
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
  rna_iterator_array_begin(iter, ptr, layer.strips());
}

static int rna_iterator_ActionLayer_strips_length(PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);
  return layer.strips().size();
}

static StructRNA *rna_ActionStrip_refine(PointerRNA *ptr)
{
  animrig::Strip &strip = static_cast<ActionStrip *>(ptr->data)->wrap();

  switch (strip.type()) {
    case animrig::Strip::Type::Keyframe:
      return &RNA_ActionKeyframeStrip;
  }
  return &RNA_UnknownType;
}

ActionStrip *rna_ActionStrips_new(
    ID *dna_action_id, ActionLayer *dna_layer, bContext *C, ReportList *reports, const int type)
{
  const animrig::Strip::Type strip_type = animrig::Strip::Type(type);

  animrig::Layer &layer = dna_layer->wrap();

  if (layer.strips().size() >= 1) {
    /* Not allowed to have more than one strip, for now. This limitation is in
     * place until working with layers is fleshed out better. */
    BKE_report(reports, RPT_ERROR, "A layer may not have more than one strip");
    return nullptr;
  }

  animrig::Action &action = reinterpret_cast<bAction *>(dna_action_id)->wrap();
  animrig::Strip &strip = layer.strip_add(action, strip_type);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &strip;
}

void rna_ActionStrips_remove(
    ID *action_id, ActionLayer *dna_layer, bContext *C, ReportList *reports, PointerRNA *strip_ptr)
{
  animrig::Action &action = reinterpret_cast<bAction *>(action_id)->wrap();
  animrig::Layer &layer = dna_layer->wrap();
  animrig::Strip &strip = rna_data_strip(strip_ptr);
  if (!layer.strip_remove(action, strip)) {
    BKE_report(reports, RPT_ERROR, "This strip does not belong to this layer");
    return;
  }

  strip_ptr->invalidate();
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(action_id, ID_RECALC_ANIMATION);
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

    PointerRNA layer_ptr = RNA_pointer_create_discrete(&action.id, &RNA_ActionLayer, layer);
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
  animrig::Action &action = reinterpret_cast<bAction *>(ptr->owner_id)->wrap();
  animrig::Strip &strip = rna_data_strip(ptr);
  rna_iterator_array_begin(
      iter, ptr, strip.data<animrig::StripKeyframeData>(action).channelbags());
}

static int rna_iterator_keyframestrip_channelbags_length(PointerRNA *ptr)
{
  animrig::Action &action = reinterpret_cast<bAction *>(ptr->owner_id)->wrap();
  animrig::Strip &strip = rna_data_strip(ptr);
  return strip.data<animrig::StripKeyframeData>(action).channelbags().size();
}

static ActionChannelbag *rna_Channelbags_new(ID *dna_action_id,
                                             ActionStrip *dna_strip,
                                             bContext *C,
                                             ReportList *reports,
                                             ActionSlot *dna_slot)
{
  animrig::Action &action = reinterpret_cast<bAction *>(dna_action_id)->wrap();
  animrig::Strip &strip = dna_strip->wrap();
  animrig::StripKeyframeData &strip_data = strip.data<animrig::StripKeyframeData>(action);
  animrig::Slot &slot = dna_slot->wrap();

  if (strip_data.channelbag_for_slot(slot) != nullptr) {
    BKE_report(reports, RPT_ERROR, "A channelbag for this slot already exists");
    return nullptr;
  }

  animrig::Channelbag &channelbag = strip_data.channelbag_for_slot_add(slot);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  /* No need to tag the depsgraph, as there is no new animation yet. */

  return &channelbag;
}

static void rna_Channelbags_remove(ID *dna_action_id,
                                   ActionStrip *dna_strip,
                                   bContext *C,
                                   ReportList *reports,
                                   PointerRNA *channelbag_ptr)
{
  animrig::Action &action = reinterpret_cast<bAction *>(dna_action_id)->wrap();
  animrig::StripKeyframeData &strip_data = dna_strip->wrap().data<animrig::StripKeyframeData>(
      action);
  animrig::Channelbag &channelbag = rna_data_channelbag(channelbag_ptr);

  if (!strip_data.channelbag_remove(channelbag)) {
    BKE_report(reports, RPT_ERROR, "This channelbag does not belong to this strip");
    return;
  }

  channelbag_ptr->invalidate();
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION);
}

static bool rna_ActionStrip_key_insert(ID *dna_action_id,
                                       ActionStrip *dna_strip,
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

  animrig::Action &action = reinterpret_cast<bAction *>(dna_action_id)->wrap();
  animrig::StripKeyframeData &strip_data = dna_strip->wrap().data<animrig::StripKeyframeData>(
      action);
  const animrig::Slot &slot = dna_slot->wrap();
  const animrig::KeyframeSettings settings = animrig::get_keyframe_settings(true);

  const animrig::SingleKeyingResult result = strip_data.keyframe_insert(
      bmain, slot, {rna_path, array_index}, {time, value}, settings, INSERTKEY_NOFLAGS);

  const bool ok = result == animrig::SingleKeyingResult::SUCCESS;
  if (ok) {
    DEG_id_tag_update_ex(bmain, dna_action_id, ID_RECALC_ANIMATION);
  }

  return ok;
}

std::optional<std::string> rna_Channelbag_path(const PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Channelbag &cbag_to_find = rna_data_channelbag(ptr);

  for (animrig::Layer *layer : action.layers()) {
    for (int64_t strip_index : layer->strips().index_range()) {
      const animrig::Strip *strip = layer->strip(strip_index);
      if (strip->type() != animrig::Strip::Type::Keyframe) {
        continue;
      }

      const animrig::StripKeyframeData &strip_data = strip->data<animrig::StripKeyframeData>(
          action);
      const int64_t index = strip_data.find_channelbag_index(cbag_to_find);
      if (index < 0) {
        continue;
      }

      PointerRNA layer_ptr = RNA_pointer_create_discrete(&action.id, &RNA_ActionLayer, layer);
      const std::optional<std::string> layer_path = rna_ActionLayer_path(&layer_ptr);
      BLI_assert_msg(layer_path, "Every animation layer should have a valid RNA path.");
      return fmt::format("{}.strips[{}].channelbags[{}]", *layer_path, strip_index, index);
    }
  }

  return std::nullopt;
}

static PointerRNA rna_Channelbag_slot_get(PointerRNA *ptr)
{
  animrig::Action &action = rna_action(ptr);
  animrig::Channelbag &channelbag = rna_data_channelbag(ptr);
  animrig::Slot *slot = action.slot_for_handle(channelbag.slot_handle);
  BLI_assert(slot);

  return RNA_pointer_create_with_parent(*ptr, &RNA_ActionSlot, slot);
}

static void rna_iterator_Channelbag_fcurves_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  animrig::Channelbag &bag = rna_data_channelbag(ptr);
  rna_iterator_array_begin(iter, ptr, bag.fcurves());
}

static int rna_iterator_Channelbag_fcurves_length(PointerRNA *ptr)
{
  animrig::Channelbag &bag = rna_data_channelbag(ptr);
  return bag.fcurves().size();
}

static FCurve *rna_Channelbag_fcurve_new(ActionChannelbag *dna_channelbag,
                                         Main *bmain,
                                         ReportList *reports,
                                         const char *data_path,
                                         const int index,
                                         const char *group_name)
{
  BLI_assert(data_path != nullptr);
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  blender::animrig::FCurveDescriptor descr = {data_path, index};
  if (group_name && group_name[0]) {
    descr.channel_group = {group_name};
  }

  animrig::Channelbag &self = dna_channelbag->wrap();
  FCurve *fcurve = self.fcurve_create_unique(bmain, descr);
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

static FCurve *rna_Channelbag_fcurve_new_from_fcurve(ID *dna_action_id,
                                                     ActionChannelbag *dna_channelbag,
                                                     ReportList *reports,
                                                     FCurve *source,
                                                     const char *data_path)
{
  animrig::Channelbag &self = dna_channelbag->wrap();

  if (!data_path) {
    data_path = source->rna_path;
  }

  if (self.fcurve_find({data_path, source->array_index})) {
    BKE_reportf(reports,
                RPT_ERROR,
                "F-Curve '%s[%d]' already exists in this channelbag",
                data_path,
                source->array_index);
    return nullptr;
  }
  FCurve *copy = BKE_fcurve_copy(source);
  MEM_SAFE_FREE(copy->rna_path);
  copy->rna_path = BLI_strdupn(data_path, strlen(data_path));
  self.fcurve_append(*copy);

  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION_NO_FLUSH);

  return copy;
}

static FCurve *rna_Channelbag_fcurve_ensure(ActionChannelbag *dna_channelbag,
                                            Main *bmain,
                                            ReportList *reports,
                                            const char *data_path,
                                            const int index,
                                            const char *group_name)
{
  BLI_assert(data_path != nullptr);
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  blender::animrig::FCurveDescriptor descr = {data_path, index};
  if (group_name && group_name[0]) {
    descr.channel_group = {group_name};
  }

  animrig::Channelbag &self = dna_channelbag->wrap();
  FCurve &fcurve = self.fcurve_ensure(bmain, descr);
  return &fcurve;
}

static FCurve *rna_Channelbag_fcurve_find(ActionChannelbag *dna_channelbag,
                                          ReportList *reports,
                                          const char *data_path,
                                          const int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  animrig::Channelbag &self = dna_channelbag->wrap();
  return self.fcurve_find({data_path, index});
}

static void rna_Channelbag_fcurve_remove(ID *dna_action_id,
                                         ActionChannelbag *dna_channelbag,
                                         bContext *C,
                                         ReportList *reports,
                                         PointerRNA *fcurve_ptr)
{
  animrig::Channelbag &self = dna_channelbag->wrap();
  FCurve *fcurve = static_cast<FCurve *>(fcurve_ptr->data);

  if (!self.fcurve_remove(*fcurve)) {
    BKE_reportf(reports, RPT_ERROR, "F-Curve not found");
    return;
  }

  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static void rna_Channelbag_fcurve_clear(ID *dna_action_id,
                                        ActionChannelbag *dna_channelbag,
                                        bContext *C)
{
  dna_channelbag->wrap().fcurves_clear();
  DEG_id_tag_update(dna_action_id, ID_RECALC_ANIMATION_NO_FLUSH);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static void rna_iterator_Channelbag_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Channelbag &bag = rna_data_channelbag(ptr);
  rna_iterator_array_begin(iter, ptr, bag.channel_groups());
}

static int rna_iterator_Channelbag_groups_length(PointerRNA *ptr)
{
  animrig::Channelbag &bag = rna_data_channelbag(ptr);
  return bag.channel_groups().size();
}

static bActionGroup *rna_Channelbag_group_new(ActionChannelbag *dna_channelbag, const char *name)
{
  BLI_assert(name != nullptr);

  animrig::Channelbag &self = dna_channelbag->wrap();
  return &self.channel_group_create(name);
}

static void rna_Channelbag_group_remove(ActionChannelbag *dna_channelbag,
                                        ReportList *reports,
                                        PointerRNA *agrp_ptr)
{
  animrig::Channelbag &self = dna_channelbag->wrap();

  bActionGroup *agrp = static_cast<bActionGroup *>(agrp_ptr->data);

  if (!self.channel_group_remove(*agrp)) {
    BKE_report(reports,
               RPT_ERROR,
               "Could not remove the F-Curve Group from the collection because it does not exist "
               "in the collection");
    return;
  }

  agrp_ptr->invalidate();
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static ActionChannelbag *rna_ActionStrip_channelbag(ID *dna_action_id,
                                                    ActionStrip *self,
                                                    ReportList *reports,
                                                    const ActionSlot *dna_slot,
                                                    const bool ensure)
{
  if (!dna_slot) {
    BKE_report(reports, RPT_ERROR, "Cannot return channelbag when slot is None");
    return nullptr;
  }

  animrig::Action &action = reinterpret_cast<bAction *>(dna_action_id)->wrap();
  animrig::StripKeyframeData &strip_data = self->wrap().data<animrig::StripKeyframeData>(action);
  const animrig::Slot &slot = dna_slot->wrap();

  if (ensure) {
    return &strip_data.channelbag_for_slot_ensure(slot);
  }
  return strip_data.channelbag_for_slot(slot);
}

/**
 * Iterator for the fcurves in a channel group.
 *
 * We need a custom iterator for this because legacy actions store their fcurves
 * in a listbase, whereas layered actions store them in an array.  Therefore
 * this iterator needs to handle both kinds of iteration.
 *
 * In the future when legacy actions are fully deprecated this can be changed to
 * a simple array iterator.
 */
struct ActionGroupChannelsIterator {
  /* Which kind of iterator it is. */
  enum {
    ARRAY,
    LISTBASE,
  } tag;

  union {
    ArrayIterator array;
    ListBaseIterator listbase;
  };
};

static void rna_ActionGroup_channels_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bActionGroup *group = (bActionGroup *)ptr->data;

  ActionGroupChannelsIterator *custom_iter = MEM_callocN<ActionGroupChannelsIterator>(__func__);

  iter->internal.custom = custom_iter;

  /* We handle both the listbase (legacy action) and array (layered action)
   * cases below. The code for each is based on the code in
   * `rna_iterator_listbase_begin()` and `rna_iterator_array_begin()`,
   * respectively. */

  /* Group from a legacy action. */
  if (group->wrap().is_legacy()) {
    custom_iter->tag = ActionGroupChannelsIterator::LISTBASE;
    custom_iter->listbase.link = static_cast<Link *>(group->channels.first);

    iter->valid = custom_iter->listbase.link != nullptr;
    return;
  }

  /* Group from a layered action. */
  animrig::Channelbag &cbag = group->channelbag->wrap();

  custom_iter->tag = ActionGroupChannelsIterator::ARRAY;
  custom_iter->array.ptr = reinterpret_cast<char *>(cbag.fcurve_array + group->fcurve_range_start);
  custom_iter->array.endptr = reinterpret_cast<char *>(
      cbag.fcurve_array + group->fcurve_range_start + group->fcurve_range_length);
  custom_iter->array.itemsize = sizeof(FCurve *);
  custom_iter->array.length = group->fcurve_range_length;

  iter->valid = group->fcurve_range_length > 0;
}

static void rna_ActionGroup_channels_end(CollectionPropertyIterator *iter)
{
  MEM_freeN(iter->internal.custom);
}

static void rna_ActionGroup_channels_next(CollectionPropertyIterator *iter)
{
  BLI_assert(iter->internal.custom != nullptr);
  BLI_assert(iter->valid);

  ActionGroupChannelsIterator *custom_iter = static_cast<ActionGroupChannelsIterator *>(
      iter->internal.custom);

  /* The code for both cases here is written based on the code in
   * `rna_iterator_array_next()` and `rna_iterator_listbase_next()`,
   * respectively. */
  switch (custom_iter->tag) {
    case ActionGroupChannelsIterator::ARRAY: {
      custom_iter->array.ptr += custom_iter->array.itemsize;
      iter->valid = (custom_iter->array.ptr != custom_iter->array.endptr);
      break;
    }
    case ActionGroupChannelsIterator::LISTBASE: {
      FCurve *fcurve = (FCurve *)custom_iter->listbase.link;
      bActionGroup *grp = fcurve->grp;
      /* Only continue if the next F-Curve (if existent) belongs in the same
       * group. */
      if ((fcurve->next) && (fcurve->next->grp == grp)) {
        custom_iter->listbase.link = custom_iter->listbase.link->next;
        iter->valid = (custom_iter->listbase.link != nullptr);
      }
      else {
        custom_iter->listbase.link = nullptr;
        iter->valid = false;
      }
      break;
    }
  }
}

static PointerRNA rna_ActionGroup_channels_get(CollectionPropertyIterator *iter)
{
  BLI_assert(iter->internal.custom != nullptr);
  BLI_assert(iter->valid);
  ActionGroupChannelsIterator *custom_iter = static_cast<ActionGroupChannelsIterator *>(
      iter->internal.custom);

  FCurve *fcurve;
  switch (custom_iter->tag) {
    case ActionGroupChannelsIterator::ARRAY:
      fcurve = *reinterpret_cast<FCurve **>(custom_iter->array.ptr);
      break;
    case ActionGroupChannelsIterator::LISTBASE:
      fcurve = reinterpret_cast<FCurve *>(custom_iter->listbase.link);
      break;
  }

  return RNA_pointer_create_with_parent(iter->parent, &RNA_FCurve, fcurve);
}

static TimeMarker *rna_Action_pose_markers_new(bAction *act, const char name[])
{
  TimeMarker *marker = MEM_callocN<TimeMarker>("TimeMarker");
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
  marker_ptr->invalidate();
}

static PointerRNA rna_Action_active_pose_marker_get(PointerRNA *ptr)
{
  bAction *act = (bAction *)ptr->data;
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_TimelineMarker, BLI_findlink(&act->markers, act->active_marker - 1));
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

static void rna_Action_frame_range_get(PointerRNA *ptr, float *r_values)
{
  const float2 frame_range = rna_action(ptr).get_frame_range();
  r_values[0] = frame_range[0];
  r_values[1] = frame_range[1];
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
  const float2 frame_range = rna_action(ptr).get_frame_range_of_keys(false);
  values[0] = frame_range[0];
  values[1] = frame_range[1];
}

static void rna_Action_use_frame_range_set(PointerRNA *ptr, bool value)
{
  animrig::Action &action = rna_action(ptr);

  if (value) {
    /* If the frame range is blank, initialize it by scanning F-Curves. */
    if ((action.frame_start == action.frame_end) && (action.frame_start == 0)) {
      const float2 frame_range = action.get_frame_range_of_keys(false);
      action.frame_start = frame_range[0];
      action.frame_end = frame_range[1];
    }

    action.flag |= ACT_FRAME_RANGE;
  }
  else {
    action.flag &= ~ACT_FRAME_RANGE;
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

static FCurve *rna_Action_fcurve_ensure_for_datablock(bAction *_self,
                                                      Main *bmain,
                                                      ReportList *reports,
                                                      ID *datablock,
                                                      const char *data_path,
                                                      const int array_index,
                                                      const char *group_name)
{
  /* Precondition checks. */
  {
    if (blender::animrig::get_action(*datablock) != _self) {
      BKE_reportf(reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Assign action \"%s\" to \"%s\" before calling this function",
                  _self->id.name + 2,
                  datablock->name + 2);
      return nullptr;
    }

    BLI_assert(data_path != nullptr);
    if (data_path[0] == '\0') {
      BKE_report(reports, RPT_ERROR_INVALID_INPUT, "F-Curve data path empty, invalid argument");
      return nullptr;
    }
  }

  blender::animrig::FCurveDescriptor descriptor = {data_path, array_index};
  if (group_name && group_name[0]) {
    descriptor.channel_group = group_name;
  }

  FCurve &fcurve = blender::animrig::action_fcurve_ensure(bmain, *_self, *datablock, descriptor);

  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
  return &fcurve;
}

/**
 * Used to check if an action (value pointer)
 * is suitable to be assigned to the ID-block that is ptr.
 */
bool rna_Action_id_poll(PointerRNA *ptr, PointerRNA value)
{
  ID *srcId = ptr->owner_id;
  bAction *dna_action = (bAction *)value.owner_id;

  if (!dna_action) {
    return false;
  }

  animrig::Action &action = dna_action->wrap();
  if (animrig::legacy::action_treat_as_legacy(action)) {
    /* there can still be actions that will have undefined id-root
     * (i.e. floating "action-library" members) which we will not
     * be able to resolve an idroot for automatically, so let these through
     */
    if (action.idroot == 0) {
      return true;
    }
    if (srcId) {
      return GS(srcId->name) == action.idroot;
    }
  }

  /* Layered Actions can always be assigned. */
  BLI_assert(action.idroot == 0);
  return true;
}

/**
 * Used to check if an action (value pointer)
 * can be assigned to Action Editor given current mode.
 */
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
      break;
  }

  /* Same as above, I (Sybren) wouldn't mind returning `true` here to just
   * always show all Actions in an unexpected place. */
  return false;
}

/**
 * Iterate the FCurves of the given bAnimContext and validate the RNA path. Sets the flag
 * #FCURVE_DISABLED if the path can't be resolved.
 */
static void reevaluate_fcurve_errors(bAnimContext *ac)
{
  /* Need to take off the flag before filtering, else the filter code would skip the FCurves, which
   * have not yet been validated. */
  const bool filtering_enabled = ac->filters.flag & ADS_FILTER_ONLY_ERRORS;
  if (filtering_enabled) {
    ac->filters.flag &= ~ADS_FILTER_ONLY_ERRORS;
  }
  ListBase anim_data = {nullptr, nullptr};
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    PointerRNA ptr;
    PropertyRNA *prop;
    PointerRNA id_ptr = RNA_id_pointer_create(ale->id);
    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      fcu->flag &= ~FCURVE_DISABLED;
    }
    else {
      fcu->flag |= FCURVE_DISABLED;
    }
  }

  ANIM_animdata_freelist(&anim_data);
  if (filtering_enabled) {
    ac->filters.flag |= ADS_FILTER_ONLY_ERRORS;
  }
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

  reevaluate_fcurve_errors(&ac);
}

static std::optional<std::string> rna_DopeSheet_path(const PointerRNA *ptr)
{
  if (GS(ptr->owner_id->name) == ID_SCR) {
    const bScreen *screen = reinterpret_cast<bScreen *>(ptr->owner_id);
    const bDopeSheet *ads = static_cast<bDopeSheet *>(ptr->data);
    int area_index;
    int space_index;
    LISTBASE_FOREACH_INDEX (ScrArea *, area, &screen->areabase, area_index) {
      LISTBASE_FOREACH_INDEX (SpaceLink *, sl, &area->spacedata, space_index) {
        if (sl->spacetype == SPACE_GRAPH) {
          SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(sl);
          if (sipo->ads == ads) {
            return fmt::format("areas[{}].spaces[{}].dopesheet", area_index, space_index);
          }
        }
        else if (sl->spacetype == SPACE_NLA) {
          SpaceNla *snla = reinterpret_cast<SpaceNla *>(sl);
          if (snla->ads == ads) {
            return fmt::format("areas[{}].spaces[{}].dopesheet", area_index, space_index);
          }
        }
        else if (sl->spacetype == SPACE_ACTION) {
          SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);
          if (&saction->ads == ads) {
            return fmt::format("areas[{}].spaces[{}].dopesheet", area_index, space_index);
          }
        }
      }
    }
  }
  return "dopesheet";
}

/**
 * Used for both `action.id_root` and `slot.target_id_type`.
 *
 * Note that `action.id_root` is deprecated, as it is only relevant to legacy
 * Animato actions. So in practice this function is primarily here for
 * `slot.target_id_type`.
 */
static const EnumPropertyItem *rna_ActionSlot_target_id_type_itemf(bContext * /* C */,
                                                                   PointerRNA * /* ptr */,
                                                                   PropertyRNA * /* prop */,
                                                                   bool *r_free)
{
  /* These items don't change, as the ID types are hard-coded. So better to
   * cache the list of enum items. */
  static EnumPropertyItem *_rna_ActionSlot_target_id_type_items = nullptr;

  if (_rna_ActionSlot_target_id_type_items) {
    return _rna_ActionSlot_target_id_type_items;
  }

  int totitem = 0;
  EnumPropertyItem *items = nullptr;

  int i = 0;
  while (rna_enum_id_type_items[i].identifier != nullptr) {
    EnumPropertyItem item = {0};
    item.value = rna_enum_id_type_items[i].value;
    item.name = rna_enum_id_type_items[i].name;
    item.identifier = rna_enum_id_type_items[i].identifier;
    item.icon = rna_enum_id_type_items[i].icon;
    item.description = rna_enum_id_type_items[i].description;
    RNA_enum_item_add(&items, &totitem, &item);
    i++;
  }

  RNA_enum_item_add(&items, &totitem, &default_ActionSlot_target_id_type_items[0]);

  RNA_enum_item_end(&items, &totitem);

  /* Don't free, but keep a reference to the created list. This is necessary
   * because of the PROP_ENUM_NO_CONTEXT flag. Without it, this will make
   * Blender use memory after it is freed:
   *
   * >>> slot = C.object.animation_data.action_slot
   * >>> enum_item = s.bl_rna.properties['target_id_type'].enum_items[slot.target_id_type]
   * >>> print(enum_item.name)
   */
  *r_free = false;
  _rna_ActionSlot_target_id_type_items = items;

  BKE_blender_atexit_register(MEM_freeN, items);

  return items;
}

static void rna_ActionSlot_target_id_type_set(PointerRNA *ptr, int value)
{
  animrig::Action &action = reinterpret_cast<bAction *>(ptr->owner_id)->wrap();
  animrig::Slot &slot = reinterpret_cast<ActionSlot *>(ptr->data)->wrap();

  if (slot.idtype != 0) {
    /* Ignore the assignment. */
    printf(
        "WARNING: ignoring assignment to target_id_type of Slot '%s' in Action '%s'. A Slot's "
        "target_id_type can only be changed when currently 'UNSPECIFIED'.\n",
        slot.identifier,
        action.id.name);
    return;
  }

  action.slot_idtype_define(slot, ID_Type(value));
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

  prop = RNA_def_property(srna, "show_only_slot_of_active_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filterflag", ADS_FILTER_ONLY_SLOTS_OF_ACTIVE);
  RNA_def_property_ui_text(
      prop,
      "Only Show Slot of Active Object",
      "Only show the slot of the active Object. Otherwise show all the Action's Slots");
  RNA_def_property_ui_icon(prop, ICON_ACTION_SLOT, 0);
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

  prop = RNA_def_property(srna, "show_lightprobes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filterflag2", ADS_FILTER_NOLIGHTPROBE);
  RNA_def_property_ui_text(
      prop, "Display Light Probe", "Include visualization of lightprobe related animation data");
  RNA_def_property_ui_icon(prop, ICON_OUTLINER_OB_LIGHTPROBE, 0);
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
  RNA_def_function_ui_description(func, "Add a slot to the Action");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_enum(
      func,
      "id_type",
      rna_enum_id_type_items,
      ID_OB,
      "Data-block Type",
      "The data-block type that the slot is intended for. This is combined with the slot name to "
      "create the slot's unique identifier, and is also used to limit (on a best-effort basis) "
      "which data-blocks the slot can be assigned to.");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(
      func,
      "name",
      nullptr,
      /* Minus 2 for the ID-type prefix. */
      sizeof(ActionSlot::identifier) - 2,
      "Name",
      "Name of the slot. This will be made unique within the Action among slots of the same type");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_pointer(func, "slot", "ActionSlot", "", "Newly created action slot");
  RNA_def_function_return(func, parm);

  /* Animation.slots.remove(layer) */
  func = RNA_def_function(srna, "remove", "rna_Action_slots_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func,
                                  "Remove the slot from the Action, including all animation that "
                                  "is associated with that slot");
  parm = RNA_def_pointer(func, "action_slot", "ActionSlot", "Action Slot", "The slot to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
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
      "Add a layer to the Animation. Currently an Animation can only have at most one layer.");
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
  PropertyRNA *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "ActionSlot", nullptr);
  RNA_def_struct_path_func(srna, "rna_ActionSlot_path");
  RNA_def_struct_ui_icon(srna, ICON_ACTION_SLOT);
  RNA_def_struct_ui_text(
      srna,
      "Action slot",
      "Identifier for a set of channels in this Action, that can be used by a data-block "
      "to specify what it gets animated by");

  RNA_define_lib_overridable(false);

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ActionSlot_identifier_set");
  RNA_def_property_string_maxlength(prop, sizeof(ActionSlot::identifier));
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_ActionSlot_identifier_update");
  RNA_def_property_ui_text(
      prop,
      "Slot Identifier",
      "Used when connecting an Action to a data-block, to find the correct slot handle. This is "
      "the display name, prefixed by two characters determined by the slot's ID type");

  prop = RNA_def_property(srna, "target_id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "idtype");
  RNA_def_property_enum_items(prop, default_ActionSlot_target_id_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_ActionSlot_target_id_type_set", "rna_ActionSlot_target_id_type_itemf");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_ActionSlot_identifier_update");
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_ui_text(prop,
                           "Target ID Type",
                           "Type of data-block that this slot is intended to animate; can be set "
                           "when 'UNSPECIFIED' but is otherwise read-only");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_property(srna, "target_id_type_icon", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_ActionSlot_target_id_type_icon_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name_display", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ActionSlot_name_display_get",
                                "rna_ActionSlot_name_display_length",
                                "rna_ActionSlot_name_display_set");
  RNA_def_property_string_maxlength(prop, sizeof(ActionSlot::identifier) - 2);
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_ANIMCHAN | NA_RENAME, "rna_ActionSlot_identifier_update");
  RNA_def_property_ui_text(
      prop,
      "Slot Display Name",
      "Name of the slot, for display in the user interface. This name combined with the slot's "
      "data-block type is unique within its Action");

  prop = RNA_def_property(srna, "handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Slot Handle",
                           "Number specific to this Slot, unique within the Action.\n"
                           "This is used, for example, on a ActionKeyframeStrip to look up the "
                           "ActionChannelbag for this Slot");

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

  func = RNA_def_function(srna, "users", "rna_ActionSlot_users");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(
      func, "Return the data-blocks that are animated by this slot of this action");
  /* Return value. */
  parm = RNA_def_property(func, "users", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(parm, "ID");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "duplicate", "rna_ActionSlot_duplicate");
  RNA_def_function_ui_description(
      func, "Duplicate this slot, including all the animation data associated with it");
  /* Return value. */
  parm = RNA_def_property(func, "slot", PROP_POINTER, PROP_NONE);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_property_struct_type(parm, "ActionSlot");
  RNA_def_property_ui_text(parm, "Duplicated Slot", "The slot created by duplicating this one");
  RNA_def_function_return(func, parm);
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
                                  "one strip, with infinite boundaries.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
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

  /* Disabled in RNA until layered animation is actually implemented.
   *
   * The animation evaluation already takes these into account, but there is no guarantee that the
   * mixing that is currently implemented is going to be mathematically identical to the eventual
   * implementation. */
#  if 0
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
#  endif

  /* Collection properties. */
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

  RNA_def_property_srna(cprop, "ActionChannelbags");
  srna = RNA_def_struct(brna, "ActionChannelbags", nullptr);
  RNA_def_struct_sdna(srna, "ActionStrip");
  RNA_def_struct_ui_text(
      srna,
      "Animation Channels for Slots",
      "For each action slot, a list of animation channels that are meant for that slot");

  /* Strip.channelbags.new(slot=...) */
  func = RNA_def_function(srna, "new", "rna_Channelbags_new");
  RNA_def_function_ui_description(
      func,
      "Add a new channelbag to the strip, to contain animation channels for a specific slot");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func,
                         "slot",
                         "ActionSlot",
                         "Action Slot",
                         "The slot that should be animated by this channelbag");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Return value. */
  parm = RNA_def_pointer(func, "channelbag", "ActionChannelbag", "", "Newly created channelbag");
  RNA_def_function_return(func, parm);

  /* Strip.channelbags.remove(strip) */
  func = RNA_def_function(srna, "remove", "rna_Channelbags_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the channelbag from the strip");
  parm = RNA_def_pointer(func, "channelbag", "ActionChannelbag", "", "The channelbag to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
}

/**
 * Define the ActionKeyframeStrip subtype of ActionStrip.
 */
static void rna_def_action_keyframe_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionKeyframeStrip", "ActionStrip");
  RNA_def_struct_ui_text(
      srna, "Keyframe Animation Strip", "Strip with a set of F-Curves for each action slot");
  RNA_def_struct_sdna_from(srna, "ActionStrip", nullptr);

  prop = RNA_def_property(srna, "channelbags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionChannelbag");
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

    /* Strip.channelbag(...). */
    func = RNA_def_function(srna, "channelbag", "rna_ActionStrip_channelbag");
    RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
    RNA_def_function_ui_description(func, "Find the ActionChannelbag for a specific Slot");
    parm = RNA_def_pointer(
        func, "slot", "ActionSlot", "Slot", "The slot for which to find the channelbag");
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
    RNA_def_boolean(func,
                    "ensure",
                    false,
                    "Create if necessary",
                    "Ensure the channelbag exists for this slot, creating it if necessary");
    parm = RNA_def_pointer(func, "channels", "ActionChannelbag", "Channels", "");
    RNA_def_function_return(func, parm);

    /* Strip.key_insert(...). */

    func = RNA_def_function(srna, "key_insert", "rna_ActionStrip_key_insert");
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

  /* Define Strip subtypes. */
  rna_def_action_keyframe_strip(brna);
}

static void rna_def_channelbag_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionChannelbagFCurves");
  srna = RNA_def_struct(brna, "ActionChannelbagFCurves", nullptr);
  RNA_def_struct_sdna(srna, "ActionChannelbag");
  RNA_def_struct_ui_text(
      srna, "F-Curves", "Collection of F-Curves for a specific action slot, on a specific strip");

  /* Channelbag.fcurves.new(...) */
  func = RNA_def_function(srna, "new", "rna_Channelbag_fcurve_new");
  RNA_def_function_ui_description(func, "Add an F-Curve to the channelbag");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  parm = RNA_def_string(
      func,
      "group_name",
      nullptr,
      sizeof(bActionGroup::name),
      "Group Name",
      "Name of the Group for this F-Curve, will be created if it does not exist yet");
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Newly created F-Curve");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_fcurve", "rna_Channelbag_fcurve_new_from_fcurve");
  RNA_def_function_ui_description(
      func, "Copy an F-Curve into the channelbag. The original F-Curve is unchanged");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "source", "FCurve", "Source F-Curve", "The F-Curve to copy");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func,
                        "data_path",
                        nullptr,
                        0,
                        "Data Path",
                        "F-Curve data path to use. If not provided, this will use the same data "
                        "path as the given F-Curve");
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Newly created F-Curve");
  RNA_def_function_return(func, parm);

  /* Channelbag.fcurves.ensure(...) */
  func = RNA_def_function(srna, "ensure", "rna_Channelbag_fcurve_ensure");
  RNA_def_function_ui_description(
      func, "Returns the F-Curve if it already exists, and creates it if necessary");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  parm = RNA_def_string(func,
                        "group_name",
                        nullptr,
                        sizeof(bActionGroup::name),
                        "Group Name",
                        "Name of the Group for this F-Curve, will be created if it does not exist "
                        "yet. This parameter is ignored if the F-Curve already exists");
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Found or newly created F-Curve");
  RNA_def_function_return(func, parm);

  /* Channelbag.fcurves.find(...) */
  func = RNA_def_function(srna, "find", "rna_Channelbag_fcurve_find");
  RNA_def_function_ui_description(
      func,
      "Find an F-Curve. Note that this function performs a linear scan "
      "of all F-Curves in the channelbag.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it does not exist");
  RNA_def_function_return(func, parm);

  /* Channelbag.fcurves.remove(...) */
  func = RNA_def_function(srna, "remove", "rna_Channelbag_fcurve_remove");
  RNA_def_function_ui_description(func, "Remove F-Curve");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "F-Curve to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Channelbag.fcurves.clear() */
  func = RNA_def_function(srna, "clear", "rna_Channelbag_fcurve_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove all F-Curves from this channelbag");
}

static void rna_def_channelbag_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ActionChannelbagGroups");
  srna = RNA_def_struct(brna, "ActionChannelbagGroups", nullptr);
  RNA_def_struct_sdna(srna, "ActionChannelbag");
  RNA_def_struct_ui_text(srna, "F-Curve Groups", "Collection of f-curve groups");

  func = RNA_def_function(srna, "new", "rna_Channelbag_group_new");
  RNA_def_function_flag(func, FunctionFlag(0));
  RNA_def_function_ui_description(func, "Create a new action group and add it to the action");
  parm = RNA_def_string(func, "name", "Group", 0, "", "New name for the action group");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Newly created action group");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Channelbag_group_remove");
  RNA_def_function_ui_description(func, "Remove action group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Action group to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_action_channelbag(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ActionChannelbag", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Animation Channel Bag",
      "Collection of animation channels, typically associated with an action slot");
  RNA_def_struct_path_func(srna, "rna_Channelbag_path");

  prop = RNA_def_property(srna, "slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_ui_text(prop, "Slot", "The Slot that the Channelbag's animation data is for");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_Channelbag_slot_get", nullptr, nullptr, nullptr);

  /* Channelbag.fcurves */
  prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_Channelbag_fcurves_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_Channelbag_fcurves_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that animate the slot");
  rna_def_channelbag_fcurves(brna, prop);

  /* Channelbag.groups */
  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_Channelbag_groups_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_Channelbag_groups_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "ActionGroup");
  RNA_def_property_ui_text(
      prop,
      "F-Curve Groups",
      "Groupings of F-Curves for display purposes, in e.g. the dopesheet and graph editor");
  rna_def_channelbag_groups(brna, prop);
}

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
                                    "rna_ActionGroup_channels_begin",
                                    "rna_ActionGroup_channels_next",
                                    "rna_ActionGroup_channels_end",
                                    "rna_ActionGroup_channels_get",
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

/* =========================== Legacy Action interface =========================== */

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

static void rna_def_action(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "Action", "ID");
  RNA_def_struct_sdna(srna, "bAction");
  RNA_def_struct_ui_text(srna, "Action", "A collection of F-Curves for animation");
  RNA_def_struct_ui_icon(srna, ICON_ACTION);

  /* Properties. */
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
      "empty Action is considered as both a 'legacy' and a 'layered' Action. Since Blender 4.4 "
      "actions are automatically updated to layered actions, and thus this will only return True "
      "when the action is empty");
  RNA_def_property_boolean_funcs(prop, "rna_Action_is_action_legacy_get", nullptr);

  prop = RNA_def_property(srna, "is_action_layered", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Is Layered Action",
      "Return whether this is a layered Action. An empty Action is considered "
      "as both a 'legacy' and a 'layered' Action.");
  RNA_def_property_boolean_funcs(prop, "rna_Action_is_action_layered_get", nullptr);

  /* Collection properties. */
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
      "playback frame range (enabling this does not automatically make it loop)");
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

  func = RNA_def_function(srna, "deselect_keys", "rna_Action_deselect_keys");
  RNA_def_function_ui_description(
      func, "Deselects all keys of the Action. The selection status of F-Curves is unchanged.");

  /* action.fcurve_ensure_for_datablock() */
  func = RNA_def_function(
      srna, "fcurve_ensure_for_datablock", "rna_Action_fcurve_ensure_for_datablock");
  RNA_def_function_ui_description(
      func,
      "Ensure that an F-Curve exists, with the given data path and array index, for the given "
      "data-block. This action must already be assigned to the data-block. This function will "
      "also create the layer, keyframe strip, and action slot if necessary, and take care of "
      "assigning the action slot too");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);

  parm = RNA_def_pointer(func,
                         "datablock",
                         "ID",
                         "",
                         "The data-block animated by this action, for which to ensure the F-Curve "
                         "exists. This action must already be assigned to the data-block");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  RNA_def_string(func,
                 "group_name",
                 nullptr,
                 0,
                 "Group Name",
                 "Name of the group for this F-Curve, if any. If the F-Curve already exists, this "
                 "parameter is ignored");
  parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "The found or created F-Curve");
  RNA_def_function_return(func, parm);

  /* API calls */
  RNA_api_action(srna);
}

/* --------- */

void RNA_def_action(BlenderRNA *brna)
{
  rna_def_action(brna);
  rna_def_action_group(brna);
  rna_def_dopesheet(brna);

  rna_def_action_slot(brna);
  rna_def_action_layer(brna);
  rna_def_action_strip(brna);
  rna_def_action_channelbag(brna);
}

#endif
