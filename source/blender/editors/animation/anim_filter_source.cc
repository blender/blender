/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"

#include "ANIM_action.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

static bool has_assigned_action(AnimData &anim_data)
{
  return anim_data.action != nullptr && anim_data.slot_handle != animrig::Slot::unassigned;
}

static void channels_for_channelbag(animrig::Action &containing_action,
                                    animrig::Channelbag &channelbag,
                                    ChannelTagStack &tags,
                                    Vector<DataChannel *> &r_channels)
{

  for (FCurve *fcu : channelbag.fcurves()) {
    if (fcu->grp) {
      PointerRNA group_pointer = RNA_pointer_create_discrete(
          &containing_action.id, RNA_ActionGroup, fcu->grp);
      tags.push({ChannelTagType::FCURVE_GROUP, fcu->grp->name, group_pointer});
    }

    DataChannel *channel = MEM_new<FCurveChannel>(__func__, containing_action, *fcu);
    channel->set_tags(tags);
    r_channels.append(channel);

    if (fcu->grp) {
      tags.pop();
    }
  }
}

static void channels_for_action_slot(animrig::Action &action,
                                     const animrig::slot_handle_t slot_handle,
                                     ChannelTagStack &tags,
                                     Vector<DataChannel *> &r_channels)
{
  using namespace animrig;
  PointerRNA action_ptr = RNA_pointer_create_discrete(&action.id, RNA_Action, &action);
  tags.push({ChannelTagType::ACTION, action.id.name, action_ptr});

  Slot *slot = action.slot_for_handle(slot_handle);
  PointerRNA slot_ptr = RNA_pointer_create_discrete(&action.id, RNA_ActionSlot, slot);
  tags.push({ChannelTagType::ACTION_SLOT, slot->identifier, slot_ptr});

  for (Layer *layer : action.layers()) {
    PointerRNA layer_ptr = RNA_pointer_create_discrete(&action.id, RNA_ActionLayer, layer);
    tags.push({ChannelTagType::ACTION_LAYER, layer->name, layer_ptr});
    for (Strip *strip : layer->strips()) {
      if (strip->type() != Strip::Type::Keyframe) {
        continue;
      }
      StripKeyframeData &action_strip = strip->data<StripKeyframeData>(action);
      Channelbag *channelbag = action_strip.channelbag_for_slot(slot_handle);
      if (!channelbag) {
        continue;
      }
      PointerRNA strip_ptr = RNA_pointer_create_discrete(&action.id, RNA_ActionStrip, strip);
      tags.push({ChannelTagType::ACTION_STRIP, "strip_name_placeholder", strip_ptr});
      channels_for_channelbag(action, *channelbag, tags, r_channels);
      tags.pop();
    }
    tags.pop();
  }

  tags.pop();
  tags.pop();
}

static void collect_channels_for_id(ID *animated_id,
                                    const CollectionFilters &collection_filters,
                                    Vector<DataChannel *> &r_channels)
{
  if (!animated_id) {
    return;
  }

  AnimData *adt = BKE_animdata_from_id(animated_id);
  if (!adt || !has_assigned_action(*adt)) {
    return;
  }

  if (collection_filters.id_type_blacklist.contains(animated_id->id_type())) {
    return;
  }

  ChannelTagStack tags;
  PointerRNA ptr = RNA_pointer_create_discrete(
      animated_id, ID_code_to_RNA_type(animated_id->id_type()), animated_id);
  tags.push({ChannelTagType::ID, animated_id->name, ptr});
  channels_for_action_slot(adt->action->wrap(), adt->slot_handle, tags, r_channels);
  tags.pop();
}

static void collection_channels_for_object(Object &obj,
                                           const CollectionFilters &collection_filters,
                                           Vector<DataChannel *> &r_channels)
{
  collect_channels_for_id(&obj.id, collection_filters, r_channels);
  collect_channels_for_id(obj.data, collection_filters, r_channels);
  for (int i = 0; i < obj.totcol; i++) {
    collect_channels_for_id(&obj.mat[i]->id, collection_filters, r_channels);
  }
}

void collect_action_channels(Vector<DataChannel *> & /* r_channels */)
{
  /* TODO */
}

void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                Vector<DataChannel *> &r_channels)
{
  Vector<PointerRNA> objects;
  if (collection_filters.only_selected) {
    CTX_data_selected_objects(&C, &objects);
  }
  else {
    /* TODO review if this is the correct list. */
    CTX_data_visible_objects(&C, &objects);
  }

  for (PointerRNA &obj_ptr : objects) {
    Object *obj = static_cast<Object *>(obj_ptr.data);
    collection_channels_for_object(*obj, collection_filters, r_channels);
  }
}

}  // namespace blender::ed::animfilter
