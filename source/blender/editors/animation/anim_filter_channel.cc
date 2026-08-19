/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

AnimationChannel::AnimationChannel(const DNAChannelType type, const StringRef label)
{
  type_ = type;
  label_ = BLI_strdup(label.data());
}

AnimationChannel::~AnimationChannel()
{
  /* TODO should this be handled differently? Currently this is only freed via the C++ route but
   * the property is on the C base class. */
  // MEM_delete(label_);
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
    : AnimationChannel(DNAChannelType::SUMMARY, label)
{
}

SummaryChannel::SummaryChannel(const StringRef label, const WeakRNAPointer &ptr)
    : AnimationChannel(DNAChannelType::SUMMARY, label)
{
  this->weak_source_ = ptr;
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

DataChannel::DataChannel(animrig::Action &containing_action, FCurve &fcurve)
    : AnimationChannel(DNAChannelType::DATA, fcurve.rna_path())
{
  this->weak_source_ = RNA_pointer_create_discrete(&containing_action.id, RNA_FCurve, &fcurve);
}

DataChannel::DataChannel(GreasePencil &containing_gp, GreasePencilLayer &gp_layer)
    : AnimationChannel(DNAChannelType::DATA, "todo gp layer name")
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
        this->datapoints_[i] = fcurve->bezt[i].vec[1][0];
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
        this->datapoints_.append(float(frame));
      }
      break;
    }
  }
}

}  // namespace blender::ed::animfilter
