/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "float.h"

#include "BLI_string.hh"

#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"

#include "ED_anim_filter.hh"

#include "ANIM_action.hh"

namespace blender {

WeakRNAPointer::WeakRNAPointer(const PointerRNA &ptr)
    : type(ptr.owner_id->id_type()),
      session_uid(ptr.owner_id->session_uid),
      id_name(ptr.owner_id->name + 2)
{
  BLI_assert(ptr.owner_id != nullptr);
  std::optional<std::string> resolved_path = RNA_path_from_ID_to_struct(&ptr);
  if (!resolved_path) {
    /* Happens if the pointer is pointing to an ID. */
    return;
  }
  path = std::move(resolved_path.value());
}

PointerRNA WeakRNAPointer::dereference(Main &bmain) const
{
  if (this->is_nullptr()) {
    return {};
  }

  /* TODO I think for linking and lib overrides we have to be smarter about which Main to use? */
  ID *id = BKE_libblock_find_session_uid(&bmain, this->type, this->session_uid);
  if (!id) {
    /* Check by name if the session_uid doesn't return a result. */
    id = BKE_libblock_find_name(&bmain, this->type, this->id_name.data());
  }

  if (!id) {
    return {};
  }
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (this->path.empty()) {
    return id_ptr;
  }

  PointerRNA resolved_ptr;
  PropertyRNA *resolved_prop;
  RNA_path_resolve(&id_ptr, this->path.data(), &resolved_ptr, &resolved_prop);
  return resolved_ptr;
}

}  // namespace blender

namespace blender::ed::animfilter {

ChannelTag::ChannelTag(const ChannelTagType type,
                       const StringRef label,
                       const PointerRNA &source_data)
    : tag_type_(type), label_(label), source_data_(source_data)
{
}

AnimationChannel::AnimationChannel(const AnimationChannelType type, const StringRef label)
    : type_(type), label_(label)
{
}

AnimationChannel::AnimationChannel(const AnimationChannel &other)
    : type_(other.type_),
      /* Mark as dirty so that data points don't have to be copied. */
      flags_(other.flags_ | ANIM_CHANNEL_DIRTY),
      label_(other.label_),
      weak_source_(other.weak_source_)
{
}

AnimationChannel::AnimationChannel(AnimationChannel &&other)
    : type_(other.type_),
      flags_(other.flags_),
      label_(std::move(other.label_)),
      weak_source_(std::move(other.weak_source_)),
      datapoints_(std::move(other.datapoints_))
{
  /* Reset the non-std::move'd fields. */
  other.type_ = AnimationChannelType::UNKNOWN;
  other.flags_ = ANIM_CHANNEL_DIRTY;
}

Span<KeyPoint> AnimationChannel::datapoints()
{
  if (this->flags_ & ANIM_CHANNEL_DIRTY) {
    this->gather_channeldata();
    this->flags_ &= ~ANIM_CHANNEL_DIRTY;
  }
  return this->datapoints_;
}

SummaryChannel::SummaryChannel(const StringRef label)
    : AnimationChannel(AnimationChannelType::SUMMARY, label)
{
}

SummaryChannel::SummaryChannel(const StringRef label, const WeakRNAPointer &ptr)
    : AnimationChannel(AnimationChannelType::SUMMARY, label)
{
  this->weak_source_ = ptr;
}

void SummaryChannel::gather_channeldata()
{
  this->datapoints_.clear();

  /* Combine the datapoints of all children by iterating them front to back. Assumes that the data
   * is sorted. */
  Array<int> channel_keypoint_indices(this->children_.size());
  Array<Span<KeyPoint>> channel_keypoints(this->children_.size());
  for (const int channel_index : this->children_.index_range()) {
    channel_keypoints[channel_index] = this->children_[channel_index]->datapoints();
    channel_keypoint_indices[channel_index] = 0;
  }

  /* Used to track if any child channel can be iterated further. */
  bool has_data = true;

  while (has_data) {
    has_data = false;
    /* Store the index of the child channels for which we need to step to the next KeyPoint. */
    Vector<int> channels_to_advance;
    KeyPoint next_keypoint = {FLT_MAX};

    for (const int channel_index : channel_keypoint_indices.index_range()) {
      Span<KeyPoint> child_keypoints = channel_keypoints[channel_index];
      const int keypoint_index = channel_keypoint_indices[channel_index];
      if (keypoint_index >= child_keypoints.size()) {
        continue;
      }

      has_data = true;
      const KeyPoint &child_keypoint = child_keypoints[keypoint_index];
      if (child_keypoint.frame > next_keypoint.frame) {
        continue;
      }

      if (child_keypoint.frame < next_keypoint.frame) {
        next_keypoint = child_keypoint;
        channels_to_advance.clear();
        channels_to_advance.append(channel_index);
      }
      else {
        BLI_assert(child_keypoint.frame == next_keypoint.frame);
        next_keypoint.merge(child_keypoint);
        channels_to_advance.append(channel_index);
      }
    }

    for (const int channel_index : channels_to_advance) {
      channel_keypoint_indices[channel_index]++;
    }

    if (has_data) {
      this->datapoints_.append(std::move(next_keypoint));
    }
  }
}

Span<AnimationChannel *> SummaryChannel::children() const
{
  return this->children_;
}

void SummaryChannel::add_child(AnimationChannel &child)
{
  BLI_assert(&child != this);
  this->children_.append(&child);
}

void SummaryChannel::remove_child(AnimationChannel &child)
{
  BLI_assert(&child != this);
  const int index = this->children_.first_index_of_try(&child);
  if (index == -1) {
    BLI_assert_unreachable();
    return;
  }
  this->children_.remove(index);
}

DataChannel::DataChannel(animrig::Action &containing_action, FCurve &fcurve)
    : AnimationChannel(AnimationChannelType::DATA, fcurve.rna_path()),
      source_type_(DataChannelType::FCURVE)
{
  this->weak_source_ = RNA_pointer_create_discrete(&containing_action.id, RNA_FCurve, &fcurve);
}

DataChannel::DataChannel(GreasePencil &containing_gp, GreasePencilLayer &gp_layer)
    : AnimationChannel(AnimationChannelType::DATA, "todo gp layer name"),
      source_type_(DataChannelType::GREASE_PENCIL)
{
  this->weak_source_ = RNA_pointer_create_discrete(
      &containing_gp.id, RNA_GreasePencilLayer, &gp_layer);
}

void DataChannel::set_tags(const ChannelTagStack &tags)
{
  this->tags_ = tags.as_set();
}

ChannelTag *DataChannel::get_tag(ChannelTagType tag_type)
{
  /* TODO store in map so we don't have to search here. */
  for (const ChannelTag &tag : this->tags_) {
    if (tag.tag_type() == tag_type) {
      return const_cast<ChannelTag *>(&tag);
    }
  }
  return nullptr;
}

template<> FCurve *DataChannel::get_source<FCurve *>() const
{
  /* TODO Remove access to GMAIN!!! */
  /* Ideally this issue is split into 2 steps where we dereference all pointers and clean up
   * anything now invalid. Then in a second step we use that data. The dereferenced data has to be
   * held in a scoped cache so we can control the validity of it. */
  Main *bmain = G.main;
  BLI_assert(this->source_type_ == DataChannelType::FCURVE);
  PointerRNA fcu_ptr = this->weak_source_.dereference(*bmain);
  if (!fcu_ptr) {
    return nullptr;
  }
  return static_cast<FCurve *>(fcu_ptr.data);
}

template<> GreasePencilLayer *DataChannel::get_source<GreasePencilLayer *>() const
{
  /* TODO Remove access to GMAIN!!! */
  Main *bmain = G.main;
  BLI_assert(this->source_type_ == DataChannelType::GREASE_PENCIL);
  PointerRNA gp_ptr = this->weak_source_.dereference(*bmain);
  if (!gp_ptr) {
    return nullptr;
  }
  return static_cast<GreasePencilLayer *>(gp_ptr.data);
}

void DataChannel::gather_channeldata()
{
  switch (this->source_type_) {
    case DataChannelType::FCURVE: {
      const FCurve *fcurve = this->get_source<FCurve *>();
      if (!fcurve || !fcurve->bezt) {
        /* Sampled FCurves cannot be edited and don't show up in the dopesheet. */
        return;
      }
      this->datapoints_.resize(fcurve->totvert);
      for (int i = 0; i < fcurve->totvert; i++) {
        BezTriple &key = fcurve->bezt[i];
        KeyPoint p = {key.vec[1][0]};
        p.is_selected = BEZT_ISSEL_ANY(&key);
        this->datapoints_[i] = p;
      }
      break;
    }

    case DataChannelType::GREASE_PENCIL: {
      GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer *>();
      if (!gp_layer) {
        return;
      }
      bke::greasepencil::Layer &layer = gp_layer->wrap();
      for (int frame : layer.frames().keys()) {
        KeyPoint p = {float(frame)};
        this->datapoints_.append(p);
      }
      break;
    }
  }
}

}  // namespace blender::ed::animfilter
