/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_types.h"

#include "BKE_grease_pencil.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

ChannelTag::ChannelTag(const ChannelTagType type, const StringRef label, void *source_data)
    : tag_type_(type), label_(label), source_data(source_data)
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

DataChannel::DataChannel(ChannelType type, void *source, const StringRef label)
    : AnimationChannel(type, label), source_(source)
{
}

/* TODO pass label */
FCurveChannel::FCurveChannel(FCurve &fcurve) : DataChannel(ChannelType::FCURVE, &fcurve, "") {}

void FCurveChannel::gather_channeldata()
{
  const FCurve *fcurve = this->get_source();
  if (!fcurve->bezt) {
    /* Sampled FCurves cannot be edited and don't show up in the dopesheet. */
    return;
  }
  this->datapoints_.resize(fcurve->totvert);
  for (int i = 0; i < fcurve->totvert; i++) {
    this->datapoints_[i] = fcurve->bezt[i].vec[1][0];
  }
}

/* TODO pass label */
GreasePencilChannel::GreasePencilChannel(GreasePencilLayer &gp_layer)
    : DataChannel(ChannelType::GREASE_PENCIL, &gp_layer, "")
{
}

void GreasePencilChannel::gather_channeldata()
{
  bke::greasepencil::Layer &layer = this->get_source()->wrap();
  for (int frame : layer.frames().keys()) {
    this->datapoints_.append(float(frame));
  }
}
}  // namespace blender::ed::animfilter
