/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"

#include "ED_anim_filter.hh"

#include "ANIM_action.hh"

namespace blender::ed::animfilter {

std::ostream &operator<<(std::ostream &stream, const DataChannelType type)
{
  switch (type) {
    case DataChannelType::FCURVE:
      stream << "FCURVE";
      break;
    case DataChannelType::GREASE_PENCIL:
      stream << "GREASE_PENCIL";
      break;
  }
  return stream;
}

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
      source_(other.source_)
{
}

AnimationChannel::AnimationChannel(AnimationChannel &&other)
    : type_(other.type_),
      flags_(other.flags_),
      label_(std::move(other.label_)),
      source_(std::move(other.source_)),
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

SummaryChannel::SummaryChannel(const StringRef label, const PointerRNA &ptr)
    : AnimationChannel(AnimationChannelType::SUMMARY, label)
{
  this->source_ = ptr;
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
    KeyPoint next_keypoint;

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

DataChannel::DataChannel(const StringRef label, const DataChannelType source_type)
    : AnimationChannel(AnimationChannelType::DATA, label), source_type_(source_type)
{
}

FCurveChannel::FCurveChannel(animrig::Action &containing_action, FCurve &fcurve)
    : DataChannel(fcurve.rna_path(), DataChannelType::FCURVE)
{
  this->source_ = RNA_pointer_create_discrete(&containing_action.id, RNA_FCurve, &fcurve);
}

GreasePencilChannel::GreasePencilChannel(GreasePencil &containing_gp, GreasePencilLayer &gp_layer)
    : DataChannel(gp_layer.wrap().name(), DataChannelType::GREASE_PENCIL)
{
  this->source_ = RNA_pointer_create_discrete(&containing_gp.id, RNA_GreasePencilLayer, &gp_layer);
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
  BLI_assert(this->source_type_ == DataChannelType::FCURVE);
  return static_cast<FCurve *>(this->source_.data);
}

template<> GreasePencilLayer *DataChannel::get_source<GreasePencilLayer *>() const
{
  BLI_assert(this->source_type_ == DataChannelType::GREASE_PENCIL);
  return static_cast<GreasePencilLayer *>(this->source_.data);
}

void FCurveChannel::gather_channeldata()
{
  BLI_assert(this->source_type_ == DataChannelType::FCURVE);
  const FCurve *fcurve = this->get_source<FCurve *>();
  if (!fcurve || !fcurve->bezt) {
    /* Sampled FCurves cannot be edited and don't show up in the dopesheet. */
    return;
  }

  this->datapoints_.resize(fcurve->totvert);
  for (int i = 0; i < fcurve->totvert; i++) {
    BezTriple &key = fcurve->bezt[i];
    KeyPoint p(key.vec[1][0]);
    p.is_selected = BEZT_ISSEL_ANY(&key);
    this->datapoints_[i] = p;
  }
}

void GreasePencilChannel::gather_channeldata()
{
  BLI_assert(this->source_type_ == DataChannelType::GREASE_PENCIL);

  GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer *>();
  if (!gp_layer) {
    return;
  }

  bke::greasepencil::Layer &layer = gp_layer->wrap();
  for (int frame : layer.frames().keys()) {
    KeyPoint p((float(frame)));
    this->datapoints_.append(p);
  }
}

}  // namespace blender::ed::animfilter
