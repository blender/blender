/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#include <iostream>

#include "BLI_set.hh"
#include "BLI_vector.hh"

namespace blender {

struct bContext;

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
};

enum DataChannelFlags {
  DATACHANNEL_FLAG_PINNED,
};

class DataChannel : public AnimationChannel {
 protected:
  void *source_;
  Set<ChannelTag> tags_;
  DataChannelFlags flags_;
};

class FCurveChannel : public DataChannel {
 protected:
  void gather_channeldata() override;
};

class GreasePencilChannel : public DataChannel {
 protected:
  void gather_channeldata() override;
};

/**
 * Iterate all channels of the given Action.
 */
void iter_action_channels();

/**
 * Iterate all channels in the given context.
 */
void iter_animation_channels(bContext &C);

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
