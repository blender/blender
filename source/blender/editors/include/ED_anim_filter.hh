/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#include <iostream>

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_ID_enums.h"

namespace blender {

struct bContext;
struct FCurve;
struct GreasePencilLayer;

namespace ed::animfilter {

enum class ChannelType {
  FCURVE,
  GREASE_PENCIL,
  SUMMARY,
};

enum class ChannelTagType {
  FCURVE_GROUP,
  ACTION_STRIP,
  ACTION_LAYER,
  ACTION_SLOT,
  ACTION,
  BONE,
  ID,
  /* While this is technically an ID, it is embedded and thus can't be
   re-used so it can never be shared between meshes. */
  SHAPE_KEY,
};

class ChannelTag {
  ChannelTagType tag_type_;
  std::string label_;

  /* This could be a PointerRNA. */
  void *source_data;
};

class AnimationChannel {
 protected:
  ChannelType type_;
  std::string label_;
  void *source_;
  bool is_dirty_;
  Vector<float> datapoints_;

 public:
  AnimationChannel(ChannelType type, StringRef label);

  AnimationChannel *parent;

  Span<float> datapoints()
  {
    if (this->is_dirty_) {
      this->gather_channeldata();
      this->is_dirty_ = false;
    }
    return this->datapoints_;
  };

 protected:
  virtual void gather_channeldata() = 0;
};

class SummaryChannel : public AnimationChannel {
  Vector<AnimationChannel *> children_;

 public:
  SummaryChannel(StringRef label);
  Span<AnimationChannel *> children() const;
  void add_child(AnimationChannel &child);
  void remove_child(AnimationChannel &child);

 protected:
  void gather_channeldata() override;
};

enum DataChannelFlags {
  DATACHANNEL_FLAG_PINNED,
};

class DataChannel : public AnimationChannel {
 public:
  DataChannel(ChannelType type, void *source, StringRef label);

 protected:
  /* The datatype of this is determined by the inheriting class. */
  void *source_;
  Set<ChannelTag> tags_;
  DataChannelFlags flags_;
};

class FCurveChannel : public DataChannel {
 public:
  FCurveChannel(FCurve &fcurve);

  FCurve *get_source()
  {
    return reinterpret_cast<FCurve *>(this->source_);
  }

 protected:
  void gather_channeldata() override;
};

class GreasePencilChannel : public DataChannel {
 public:
  GreasePencilChannel(GreasePencilLayer &gp_layer);

  GreasePencilLayer *get_source()
  {
    return reinterpret_cast<GreasePencilLayer *>(this->source_);
  }

 protected:
  void gather_channeldata() override;
};

struct CollectionFilters {
  bool only_selected;

  /* Limit the returned data to only channels of the given type.
   * If empty, all channel types are returned. */
  Set<ChannelType> channel_type_whitelist;
  /* Any id types put here will be ignored in the query process. */
  Set<ID_Type> id_type_blacklist;
};

/**
 * Iterate all channels of the given Action.
 */
void collect_action_channels(Set<DataChannel> &r_channels);

/**
 * Iterate all channels in the given context.
 */
void collect_animation_channels(bContext &C,
                                const CollectionFilters &collection_filters,
                                Set<DataChannel> &r_channels);

/**
 * Data structure that holds all summary channels and the root of the tree.
 */
struct SummaryTree {
  SummaryChannel *root;
  Vector<SummaryChannel> summaries;
};

/**
 * Group the given channels by setting parent child relationships.
 *
 * \param tag_order: determines in which order the groups should be created. Items early in
 * the list are parents to items later in the list.
 * \param use_minimal_grouping: If true, group creation is skipped if it would move all chilren to
 * a subgroup.
 *
 * \returns: the root channel.
 */
SummaryTree group_channels(MutableSpan<DataChannel> channels,
                           Span<ChannelTagType> tag_order,
                           bool use_minimal_grouping);

}  // namespace ed::animfilter
}  // namespace blender
