/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_grease_pencil.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

ChannelTag::ChannelTag(const ChannelTagType type,
                       const StringRef label,
                       const PointerRNA &source_data)
    : tag_type_(type), label_(label), source_data_(source_data)
{
}

AnimationChannel::AnimationChannel(const ChannelType type, const StringRef label)
    : type_(type), label_(label)
{
}

Span<float> AnimationChannel::datapoints()
{
  if (this->flags_ & ANIM_CHANNEL_DIRTY) {
    this->gather_channeldata();
    this->flags_ &= ~ANIM_CHANNEL_DIRTY;
  }
  return this->datapoints_;
}

SummaryChannel::SummaryChannel(const StringRef label)
    : AnimationChannel(ChannelType::SUMMARY, label)
{
}

SummaryChannel::SummaryChannel(const StringRef label, const PointerRNA &ptr)
    : AnimationChannel(ChannelType::SUMMARY, label)
{
  this->source_ = ptr;
}

void SummaryChannel::gather_channeldata()
{
  for (AnimationChannel *child : this->children_) {
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

DataChannel::DataChannel(FCurve &fcurve) : AnimationChannel(ChannelType::DATA, fcurve.rna_path())
{
  this->source_ = RNA_pointer_create_discrete(nullptr, RNA_FCurve, &fcurve);
}

DataChannel::DataChannel(GreasePencilLayer &gp_layer)
    : AnimationChannel(ChannelType::DATA, "todo gp layer name")
{
  this->source_ = RNA_pointer_create_discrete(nullptr, RNA_GreasePencilLayer, &gp_layer);
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

void DataChannel::gather_channeldata()
{
  switch (this->source_type_) {
    case DataChannelType::FCURVE: {
      const FCurve *fcurve = this->get_source<FCurve *>();
      if (!fcurve->bezt) {
        /* Sampled FCurves cannot be edited and don't show up in the dopesheet. */
        return;
      }
      this->datapoints_.resize(fcurve->totvert);
      for (int i = 0; i < fcurve->totvert; i++) {
        this->datapoints_[i] = fcurve->bezt[i].vec[1][0];
      }
      break;
    }

    case DataChannelType::GREASE_PENCIL: {
      bke::greasepencil::Layer &layer = this->get_source<GreasePencilLayer *>()->wrap();
      for (int frame : layer.frames().keys()) {
        this->datapoints_.append(float(frame));
      }
      break;
    }
  }
}

}  // namespace blender::ed::animfilter
