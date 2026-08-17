/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_types.h"

#include "BKE_grease_pencil.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

ChannelTag::ChannelTag(const ChannelTagType type, const StringRef label, void *source_data)
    : tag_type_(type), label_(label), source_data_(source_data)
{
}

AnimationChannel::AnimationChannel(const ChannelType type, const StringRef label)
    : type_(type), label_(label)
{
}

SummaryChannel::SummaryChannel(const StringRef label)
    : AnimationChannel(ChannelType::SUMMARY, label)
{
}

void SummaryChannel::gather_channeldata()
{
  /* TODO implement. */
}

Span<AnimationChannel *> SummaryChannel::children() const
{
  return this->children_;
}

void SummaryChannel::add_child(AnimationChannel &child)
{
  BLI_assert(&child != this);
  this->children_.append(&child);
  child.parent = this;
}

void SummaryChannel::remove_child(AnimationChannel &child)
{
  BLI_assert(&child != this);
  const int index = this->children_.first_index_of(&child);
  if (index == -1) {
    return;
  }
  this->children_.remove(index);
  child.parent = nullptr;
}

DataChannel::DataChannel(FCurve &fcurve)
    : AnimationChannel(ChannelType::FCURVE, fcurve.rna_path()),
      type_(DatachannelType::FCURVE),
      source_(&fcurve)
{
}

DataChannel::DataChannel(GreasePencilLayer &gp_layer)
    : AnimationChannel(ChannelType::GREASE_PENCIL, ""),
      type_(DatachannelType::GREASE_PENCIL),
      source_(&gp_layer)
{
}

template<> FCurve *DataChannel::get_source<FCurve *>() const
{
  BLI_assert(this->type_ == DatachannelType::FCURVE);
  return static_cast<FCurve *>(this->source_);
}

template<> GreasePencilLayer *DataChannel::get_source<GreasePencilLayer *>() const
{
  BLI_assert(this->type_ == DatachannelType::GREASE_PENCIL);
  return static_cast<GreasePencilLayer *>(this->source_);
}

void DataChannel::gather_channeldata()
{
  switch (this->type_) {
    case DatachannelType::FCURVE: {
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

    case DatachannelType::GREASE_PENCIL: {
      bke::greasepencil::Layer &layer = this->get_source<GreasePencilLayer *>()->wrap();
      for (int frame : layer.frames().keys()) {
        this->datapoints_.append(float(frame));
      }
      break;
    }
  }
}

}  // namespace blender::ed::animfilter
