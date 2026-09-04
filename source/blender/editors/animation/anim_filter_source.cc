/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "anim_filter_source_intern.hh"

#include "BLI_span.hh"
#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"

#include "ANIM_action.hh"

#include "ED_anim_filter.hh"
#include <optional>

namespace blender::ed::animfilter {

static bool has_assigned_action(AnimData &anim_data)
{
  return anim_data.action != nullptr && anim_data.slot_handle != animrig::Slot::unassigned;
}

/**
 * \param rna_path_items: the RNA path that points to this channelbag.
 */
static void channels_for_channelbag(const PointerRNA &channelbag_ptr,
                                    animrig::Channelbag &channelbag,
                                    ChannelTagStack &tags,
                                    Vector<rna_path::Item> rna_path_items,
                                    ChannelArena &arena)
{

  for (const int64_t fcurve_index : channelbag.fcurves().index_range()) {
    FCurve *fcu = channelbag.fcurve(fcurve_index);

    if (fcu->grp) {
      PointerRNA group_pointer = RNA_pointer_create_with_parent(
          channelbag_ptr, RNA_ActionGroup, fcu->grp);
      rna_path_items.append(rna_path::Member{UString("groups")});
      rna_path_items.append(rna_path::LookupKey{UString{fcu->grp->name}});
      tags.push({ChannelTagType::FCURVE_GROUP,
                 fcu->grp->name,
                 group_pointer,
                 ParsedRNAPathRef(rna_path_items)});
      rna_path_items.pop_last();
      rna_path_items.pop_last();
    }

    /* Append the FCurve to the RNA path. */
    rna_path_items.append(rna_path::Member{UString("fcurves")});
    rna_path_items.append(rna_path::LookupIndex{fcurve_index});

    PointerRNA fcurve_ptr = RNA_pointer_create_with_parent(channelbag_ptr, RNA_FCurve, fcu);
    FCurveChannel *channel = arena.allocate_data_channel<FCurveChannel>(
        fcurve_ptr, ParsedRNAPathRef(rna_path_items));
    channel->tags().set(tags.as_set());

    /* Pop the FCurve off the RNA path. */
    rna_path_items.pop_last();
    rna_path_items.pop_last();

    if (fcu->grp) {
      tags.pop();
    }
  }
}

static void channels_for_action_slot(animrig::Action &action,
                                     const animrig::slot_handle_t slot_handle,
                                     ChannelTagStack &tags,
                                     ChannelArena &arena)
{
  using namespace animrig;

  /* Start building the RNA path from the Action. */
  Vector<rna_path::Item> rna_path_items;

  PointerRNA action_ptr = RNA_pointer_create_discrete(&action.id, RNA_Action, &action);
  tags.push({ChannelTagType::ACTION, action.id.name + 2, action_ptr, ParsedRNAPathRef()});

  Slot *slot = action.slot_for_handle(slot_handle);
  PointerRNA slot_ptr = RNA_pointer_create_id_subdata(action.id, RNA_ActionSlot, slot);

  rna_path_items.append(rna_path::Member{UString("slots")});
  rna_path_items.append(rna_path::LookupKey{UString(slot->identifier)});
  tags.push(
      {ChannelTagType::ACTION_SLOT, slot->identifier, slot_ptr, ParsedRNAPathRef(rna_path_items)});
  rna_path_items.pop_last();
  rna_path_items.pop_last();

  for (const int layer_index : action.layers().index_range()) {
    Layer *layer = action.layer(layer_index);

    /* Append the layer to the RNA path. */
    rna_path_items.append(rna_path::Member{UString("layers")});
    rna_path_items.append(rna_path::LookupKey{UString(layer->name)});

    PointerRNA layer_ptr = RNA_pointer_create_id_subdata(action.id, RNA_ActionLayer, layer);
    /* Passing the layer index to guarantee layers are always sorted by their hierarchy. */
    tags.push({ChannelTagType::ACTION_LAYER,
               layer->name,
               layer_ptr,
               ParsedRNAPathRef(rna_path_items),
               layer_index});

    for (const int64_t strip_index : layer->strips().index_range()) {
      Strip *strip = layer->strip(strip_index);
      if (strip->type() != Strip::Type::Keyframe) {
        continue;
      }
      StripKeyframeData &action_strip = strip->data<StripKeyframeData>(action);
      const int64_t channelbag_index = action_strip.channelbag_index_for_slot(slot_handle);
      if (channelbag_index < 0) {
        continue;
      }

      /* Append the strip to the RNA path. */
      rna_path_items.append(rna_path::Member{UString("strips")});
      rna_path_items.append(rna_path::LookupIndex{strip_index});

      Channelbag *channelbag = action_strip.channelbag(channelbag_index);
      PointerRNA strip_ptr = RNA_pointer_create_with_parent(
          layer_ptr, RNA_ActionKeyframeStrip, strip);
      tags.push({ChannelTagType::ACTION_STRIP,
                 "strip_name_placeholder",
                 strip_ptr,
                 ParsedRNAPathRef(rna_path_items)});

      rna_path_items.append(rna_path::Member{UString("channelbags")});
      rna_path_items.append(rna_path::LookupIndex{channelbag_index});

      PointerRNA channelbag_ptr = RNA_pointer_create_with_parent(
          strip_ptr, RNA_ActionChannelbag, strip);
      channels_for_channelbag(channelbag_ptr, *channelbag, tags, rna_path_items, arena);

      /* Pop the channelbag off the RNA path. */
      rna_path_items.pop_last();
      rna_path_items.pop_last();

      /* Pop the strip off the tags & RNA path. */
      tags.pop();
      rna_path_items.pop_last();
      rna_path_items.pop_last();
    }

    /* Pop the layer off the tags & RNA path. */
    tags.pop();
    rna_path_items.pop_last();
    rna_path_items.pop_last();
  }

  tags.pop();
  tags.pop();
}

static void collect_channels_for_id(ID *animated_id,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena)
{
  if (!animated_id) {
    return;
  }

  AnimData *adt = BKE_animdata_from_id(animated_id);
  if (!adt || !has_assigned_action(*adt)) {
    return;
  }

  if (collection_filters.id_type_blocklist.contains(animated_id->id_type())) {
    return;
  }

  ChannelTagStack tags;
  PointerRNA animated_id_ptr = RNA_pointer_create_discrete(
      animated_id, ID_code_to_RNA_type(animated_id->id_type()), animated_id);
  tags.push({ChannelTagType::ID, animated_id->name + 2, animated_id_ptr, ParsedRNAPathRef()});
  channels_for_action_slot(adt->action->wrap(), adt->slot_handle, tags, arena);
  tags.pop();
}

void collection_channels_for_object(Object &obj,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena)
{
  collect_channels_for_id(&obj.id, collection_filters, arena);
  collect_channels_for_id(obj.data, collection_filters, arena);
  for (int i = 0; i < obj.totcol; i++) {
    collect_channels_for_id(&obj.mat[i]->id, collection_filters, arena);
  }
}

void collect_action_channels(ChannelArena & /* arena */)
{
  /* TODO */
}

void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                ChannelArena &arena)
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
    collection_channels_for_object(*obj, collection_filters, arena);
  }
}

}  // namespace blender::ed::animfilter
