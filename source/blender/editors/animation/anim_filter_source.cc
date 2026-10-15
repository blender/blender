/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "anim_filter_source_intern.hh"

#include "BLI_assert.hh"
#include "BLI_listbase.hh"
#include "BLI_span.hh"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_pose.hh"

#include "ANIM_action.hh"
#include "ANIM_armature.hh"
#include "ANIM_rna.hh"

#include "ED_anim_filter.hh"

#include <optional>

namespace blender::ed::animfilter {

#ifndef NDEBUG
/**
 * Debug tool that asserts the number of tags at construction is the same as at destruction.
 *
 * This helps to ensure that functions that receive a ChannelTagSet reference don't forget to pop
 * the tags they push on the stack.
 */
class AssertScopedTagCount final {
  const ChannelTagStack &tags_;
  const int64_t initial_tag_count_;

 public:
  AssertScopedTagCount() = delete;
  explicit AssertScopedTagCount(const ChannelTagStack &tags)
      : tags_(tags), initial_tag_count_(tags.size())
  {
  }
  ~AssertScopedTagCount()
  {
    BLI_assert(tags_.size() == initial_tag_count_);
  }
};
#  define assert_tag_count(tags) AssertScopedTagCount _scoped_tag_counter((tags))
#else
#  define assert_tag_count(tags) ((void)0)
#endif

/* Find the bPoseChannel that this FCurve references. If it does not reference a pose bone, nullptr
 * is returned. The `tags` have to contain a tag of type ID that is an Object. */
static PointerRNA pose_bone_for_fcurve(const FCurve &fcurve, const ChannelTagStack &tags)
{
  assert_tag_count(tags);

  std::optional<std::string> bone_name = animrig::pose_bone_name_from_rna_path(
      fcurve.rna_path_parsed());
  if (!bone_name) {
    return {};
  }
  /* Assuming here that we have an ID that this action is assigned to. We need the object to find
   * the pose bone. */
  const ChannelTag *id_tag = tags.find_type(ChannelTagType::ID);
  if (!id_tag || id_tag->source_data().type != RNA_Object) {
    return {};
  }
  Object *obj = id_tag->source_data().data_as<Object>();
  bPoseChannel *pose_bone = BKE_pose_channel_find_name(obj->pose, bone_name.value().c_str());
  PointerRNA pose_bone_pointer = RNA_pointer_create_discrete(&obj->id, RNA_PoseBone, pose_bone);
  return pose_bone_pointer;
}

/**
 * \param rna_path_items: the RNA path that points to this channelbag.
 */
static void channels_for_channelbag(const PointerRNA &channelbag_ptr,
                                    ChannelTagStack &tags,
                                    Vector<rna_path::Item> rna_path_items,
                                    ChannelArena &arena,
                                    const CollectionFilters &collection_filters)
{
  assert_tag_count(tags);

  BLI_assert(channelbag_ptr.type == RNA_ActionChannelbag);
  animrig::Channelbag &channelbag = *channelbag_ptr.data_as<animrig::Channelbag>();
  BLI_assert(channelbag.slot_handle != animrig::Slot::unassigned);

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

    PointerRNA pose_bone_pointer = pose_bone_for_fcurve(*fcu, tags);

    bool skip_fcurve = false;

    if (pose_bone_pointer.has_data()) {
      bPoseChannel *pose_bone = pose_bone_pointer.data_as<bPoseChannel>();

      Object *object = id_cast<Object *>(pose_bone_pointer.owner_id);
      bArmature *armature = id_cast<bArmature *>(object->data);
      /* We have to check for visibility here. This is not as nice as with objects where we can
       * just skip the object entirely since bones are not the owner of their animation. */
      if (!collection_filters.show_hidden &&
          !animrig::bone_is_visible(armature, {pose_bone, pose_bone->bone_get(*object)}))
      {
        skip_fcurve = true;
      }
      if (collection_filters.only_selected &&
          !animrig::bone_is_selected(armature, {pose_bone, pose_bone->bone_get(*object)}))
      {
        skip_fcurve = true;
      }
      tags.push({ChannelTagType::BONE,
                 pose_bone->name,
                 pose_bone_pointer,
                 fcu->rna_path_parsed().slice(0, 3)});
    }
    if (!skip_fcurve) {
      PointerRNA fcurve_ptr = RNA_pointer_create_with_parent(channelbag_ptr, RNA_FCurve, fcu);
      FCurveChannel *channel = arena.allocate_data_channel<FCurveChannel>(
          fcurve_ptr, ParsedRNAPathRef(rna_path_items));
      channel->tags().set(tags.snapshot());
    }

    /* Pop the FCurve off the RNA path. */
    rna_path_items.pop_last();
    rna_path_items.pop_last();

    if (pose_bone_pointer.has_data()) {
      tags.pop();
    }

    if (fcu->grp) {
      tags.pop();
    }
  }
}

static void channels_for_action_slot(animrig::Action &action,
                                     animrig::Slot &slot,
                                     ChannelTagStack &tags,
                                     ChannelArena &arena,
                                     const CollectionFilters &collection_filters)
{
  using namespace animrig;
  assert_tag_count(tags);

  /* Start building the RNA path from the Action. */
  Vector<rna_path::Item> rna_path_items;

  PointerRNA action_ptr = RNA_pointer_create_discrete(&action.id, RNA_Action, &action);
  tags.push({ChannelTagType::ACTION, action.id.name + 2, action_ptr, ParsedRNAPathRef()});

  PointerRNA slot_ptr = RNA_pointer_create_id_subdata(action.id, RNA_ActionSlot, &slot);

  rna_path_items.append(rna_path::Member{UString("slots")});
  rna_path_items.append(rna_path::LookupKey{UString(slot.identifier)});
  tags.push({ChannelTagType::ACTION_SLOT,
             slot.identifier + 2,
             slot_ptr,
             ParsedRNAPathRef(rna_path_items)});
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
      const int64_t channelbag_index = action_strip.channelbag_index_for_slot(slot.handle);
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

      /* Append the channelbag to the RNA path. */
      rna_path_items.append(rna_path::Member{UString("channelbags")});
      rna_path_items.append(rna_path::LookupIndex{channelbag_index});

      PointerRNA channelbag_ptr = RNA_pointer_create_with_parent(
          strip_ptr, RNA_ActionChannelbag, channelbag);
      channels_for_channelbag(channelbag_ptr, tags, rna_path_items, arena, collection_filters);

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

static void channels_for_action_slot(animrig::Action &action,
                                     const animrig::slot_handle_t slot_handle,
                                     ChannelTagStack &tags,
                                     ChannelArena &arena,
                                     const CollectionFilters &collection_filters)
{
  animrig::Slot *slot = action.slot_for_handle(slot_handle);
  if (!slot) {
    return;
  }
  channels_for_action_slot(action, *slot, tags, arena, collection_filters);
}

static void collect_channels_for_id(ID *animated_id,
                                    const CollectionFilters &collection_filters,
                                    ChannelSourcingContext &ctx)
{
  if (!animated_id) {
    return;
  }

  if (!ctx.visited_ids.add(animated_id)) {
    /* This ID was already visited. */
    return;
  }

  std::optional<std::pair<animrig::Action *, animrig::Slot *>> action_and_slot =
      animrig::get_action_slot_pair(*animated_id);

  if (!action_and_slot) {
    return;
  }

  if (collection_filters.id_type_blocklist.contains(animated_id->id_type())) {
    return;
  }

  ChannelTagStack tags;
  assert_tag_count(tags);

  PointerRNA animated_id_ptr = RNA_pointer_create_discrete(
      animated_id, ID_code_to_RNA_type(animated_id->id_type()), animated_id);
  tags.push({ChannelTagType::ID, animated_id->name + 2, animated_id_ptr, ParsedRNAPathRef()});
  channels_for_action_slot(*action_and_slot->first,
                           action_and_slot->second->handle,
                           tags,
                           ctx.arena,
                           collection_filters);
  tags.pop();
}

struct GreasePencilCollector {
  ChannelTagStack &tags;
  ChannelArena &arena;
  const PointerRNA gp_id_ptr;
  Vector<rna_path::Item> &layer_rna_path_items;
  Vector<rna_path::Item> &group_rna_path_items;

  void visit(bke::greasepencil::TreeNode &node, int &visit_order, const std::string indent)
  {
    assert_tag_count(tags);

    /* The visit order counts downwards, which is linked to the meaning of
     * AnimationChannel::sorting_priority_, which sorts the highest numbers first. Since these
     * channels should be sorted in visit order, the numbers have ot go from high to low, and thus
     * descend. */
    visit_order--;

    std::cout << indent << " - " << visit_order << " " << (node.is_layer() ? "L" : "G") << " "
              << node.name() << "\n";

    if (node.is_layer()) {
      GreasePencilLayer &gp_layer = node.as_layer();
      PointerRNA gp_layer_ptr = RNA_pointer_create_with_parent(
          gp_id_ptr, RNA_GreasePencilLayer, &gp_layer);
      layer_rna_path_items[1] = rna_path::LookupKey{UString(node.name())};

      GreasePencilChannel *gp_channel = arena.allocate_data_channel<GreasePencilChannel>(
          gp_layer_ptr, ParsedRNAPathRef(layer_rna_path_items), visit_order);
      gp_channel->tags().set(tags.snapshot());
      return;
    }

    BLI_assert(node.is_group());
    BLI_assert_msg(node.parent_group() != nullptr,
                   "The root node of the grease pencil layer tree should not be visited");

    bke::greasepencil::LayerGroup &gp_group = node.as_group();

    /* Push the GP group. */
    PointerRNA gp_group_ptr = RNA_pointer_create_with_parent(
        gp_id_ptr, RNA_GreasePencilLayerGroup, &gp_group);
    group_rna_path_items[1] = rna_path::LookupKey{UString(gp_group.name())};
    tags.push({ChannelTagType::GREASEPENCIL_GROUP,
               gp_group.name(),
               gp_group_ptr,
               ParsedRNAPathRef(group_rna_path_items),
               visit_order});

    /* Visit children. */
    for (GreasePencilLayerTreeNode &child : gp_group.children.items_reversed()) {
      this->visit(child.wrap(), visit_order, "  " + indent);
    }

    /* Pop the GP group. */
    tags.pop();
  }
};

static void collect_channels_for_grease_pencil(GreasePencil &gp, ChannelArena &arena)
{
  ChannelTagStack tags;
  assert_tag_count(tags);

  const PointerRNA gp_id_ptr = RNA_id_pointer_create(&gp.id);
  tags.push({ChannelTagType::ID, gp.id.name + 2, gp_id_ptr, ParsedRNAPathRef()});

  Vector<rna_path::Item> layer_rna_path_items = {rna_path::Member{UString("layers")}};
  Vector<rna_path::Item> group_rna_path_items = {rna_path::Member{UString("layer_groups")}};
  layer_rna_path_items.resize(2);
  group_rna_path_items.resize(2);

  GreasePencilCollector collector = {
      tags, arena, gp_id_ptr, layer_rna_path_items, group_rna_path_items};

  /* The root node of the tree doesn't represent any animatable data, so don't visit it, just use
   * it to get to its children. */
  std::cout << "Collecting channels for GP " << gp.id.name + 2 << "\n";
  int visit_order = 0;
  for (GreasePencilLayerTreeNode &child : gp.root_group().children.items_reversed()) {
    collector.visit(child.wrap(), visit_order, "  ");
  }

  /* Pop the GP ID. */
  tags.pop();
}

struct ModifierIDIterArgs {
  const CollectionFilters &collection_filters;
  ChannelSourcingContext &ctx;
};

static void foreach_modifier_ID_callback(void *user_data,
                                         Object * /* ob */,
                                         ID **idpoin,
                                         LibraryForeachIDCallbackFlag /*cb_flag*/)
{
  ID *id = *idpoin;
  if (id == nullptr) {
    return;
  }
  ModifierIDIterArgs *args = reinterpret_cast<ModifierIDIterArgs *>(user_data);
  switch (id->id_type()) {
    case ID_NT:
    case ID_TE:
    case ID_CF:
      collect_channels_for_id(id, args->collection_filters, args->ctx);
      break;
    default:
      break;
  }
}

void collection_channels_for_object(Object &obj,
                                    const CollectionFilters &collection_filters,
                                    ChannelSourcingContext &ctx)
{
  collect_channels_for_id(&obj.id, collection_filters, ctx);
  collect_channels_for_id(obj.data, collection_filters, ctx);

  for (const int i : IndexRange(obj.totcol)) {
    /* The given number is expected to be 1 based... why........ */
    Material *mat = BKE_object_material_get(&obj, i + 1);

    if (mat) {
      collect_channels_for_id(&mat->id, collection_filters, ctx);
      collect_channels_for_id(&mat->nodetree->id, collection_filters, ctx);
    }
  }
  if (obj.data) {
    switch (obj.type) {
      case OB_GREASE_PENCIL:
        if (!collection_filters.id_type_blocklist.contains(ID_GP)) {
          collect_channels_for_grease_pencil(*id_cast<GreasePencil *>(obj.data), ctx.arena);
        }
        break;

      case OB_MESH: {
        Mesh *mesh = id_cast<Mesh *>(obj.data);
        if (mesh->key) {
          collect_channels_for_id(&mesh->key->id, collection_filters, ctx);
        }
        break;
      }

      default:
        break;
    }
  }

  if (!collection_filters.ignore_modifier_ids) {
    ModifierIDIterArgs args(collection_filters, ctx);
    BKE_modifiers_foreach_ID_link(&obj, foreach_modifier_ID_callback, &args);
  }
}

static void collect_action_channels(animrig::Action &action,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena)
{
  using namespace animrig;
  PRF_scope_with_name("ed::animfilter::collect_action_channels", ProfileCategory::Editor);

  ChannelTagStack tags;
  assert_tag_count(tags);

  ChannelSourcingContext ctx(arena);

  /* Start building the RNA path from the Action. */
  Vector<rna_path::Item> rna_path_items;
  PointerRNA action_ptr = RNA_id_pointer_create(&action.id);
  tags.push({ChannelTagType::ACTION, action.id.name + 2, action_ptr, ParsedRNAPathRef()});

  /* Loop over the layers. */
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

    for (const int strip_index : layer->strips().index_range()) {
      Strip *strip = layer->strip(strip_index);
      if (strip->type() != Strip::Type::Keyframe) {
        continue;
      }

      /* Append the strip to the RNA path. */
      rna_path_items.append(rna_path::Member{UString("strips")});
      rna_path_items.append(rna_path::LookupIndex{strip_index});

      /* Add tag for the strip. */
      PointerRNA strip_ptr = RNA_pointer_create_with_parent(
          layer_ptr, RNA_ActionKeyframeStrip, strip);
      tags.push({ChannelTagType::ACTION_STRIP,
                 "strip_name_placeholder",
                 strip_ptr,
                 ParsedRNAPathRef(rna_path_items),
                 strip_index});

      /* Loop over the Channelbags in this Strip. */
      StripKeyframeData &stripdata = strip->data<StripKeyframeData>(action);
      for (const int channelbag_index : stripdata.channelbags().index_range()) {
        Channelbag *channelbag = stripdata.channelbag(channelbag_index);
        Slot *slot = action.slot_for_handle(channelbag->slot_handle);
        BLI_assert(slot); /* A channelbag should always have a matching slot. */

        {
          /* Push the tag for the slot itself. The RNA path starts at the Action directly. */
          PointerRNA slot_ptr = RNA_pointer_create_with_parent(action_ptr, RNA_ActionSlot, slot);
          tags.push({ChannelTagType::ACTION_SLOT,
                     slot->identifier + 2,
                     slot_ptr,
                     ParsedRNAPathRef({
                         rna_path::Member{UString("slots")},
                         rna_path::LookupKey{UString(slot->identifier)},
                     })});
        }

        /* Push a tag for the first ID animated by this Slot. That way, the F-Curve labels
         * can be resolved properly. */
        bool did_push_slot_user_tag = false;
        if (slot && !slot->runtime_users().is_empty()) {
          did_push_slot_user_tag = true;
          ID *slot_user = slot->runtime_users()[0];
          PointerRNA slot_user_id_ptr = RNA_id_pointer_create(slot_user);
          tags.push(
              {ChannelTagType::ID, slot_user->name + 2, slot_user_id_ptr, ParsedRNAPathRef()});
        }
        /* Append the channelbag to the RNA path (no tag for channelbags). */
        rna_path_items.append(rna_path::Member{UString("channelbags")});
        rna_path_items.append(rna_path::LookupIndex{channelbag_index});

        PointerRNA channelbag_ptr = RNA_pointer_create_with_parent(
            strip_ptr, RNA_ActionChannelbag, channelbag);
        channels_for_channelbag(channelbag_ptr, tags, rna_path_items, arena, collection_filters);

        /* Pop the slot & user tags. */
        if (did_push_slot_user_tag) {
          tags.pop();
        }
        tags.pop();

        /* Pop the channelbag off the RNA path. */
        rna_path_items.pop_last();
        rna_path_items.pop_last();
      }

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

  /* Pop the Action off the tags. */
  tags.pop();
  BLI_assert(tags.is_empty());
  BLI_assert(rna_path_items.is_empty());
}

void collect_action_editor_channels(const bContext &C,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena)
{
  /* At this moment, the Action editor always shows the Action assigned to the active Object. */
  Object *active_object = CTX_data_active_object(&C);
  if (!active_object || !active_object->adt || !active_object->adt->action) {
    return;
  }
  animrig::Action &action = active_object->adt->action->wrap();

  collect_action_channels(action, collection_filters, arena);
}

void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                ChannelArena &arena)
{
  PRF_scope_with_name("ed::animfilter::collect_animation_channels", ProfileCategory::Editor);

  Vector<PointerRNA> objects;
  if (collection_filters.only_selected) {
    CTX_data_selected_objects(&C, &objects);
  }
  /* Show *all* objects. */
  else if (collection_filters.show_hidden) {
    /* We can check this after `only_selected` because selected objects have to be visible. */
    Main *bmain = CTX_data_main(&C);
    for (Object &obj : bmain->objects) {
      objects.append(RNA_id_pointer_create(&obj.id));
    }
  }
  else {
    /* Show all visible. */
    CTX_data_visible_objects(&C, &objects);
  }

  ChannelSourcingContext ctx(arena);
  for (PointerRNA &obj_ptr : objects) {
    Object *obj = static_cast<Object *>(obj_ptr.data);
    collection_channels_for_object(*obj, collection_filters, ctx);
  }
}

}  // namespace blender::ed::animfilter
