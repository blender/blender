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
  void *source_data_;

 public:
  ChannelTag(ChannelTagType type, StringRef label, void *source_data);
  bool operator==(const ChannelTag &other) const
  {
    return other.tag_type() == this->tag_type_ && other.source_data() == this->source_data_ &&
           other.label() == this->label_;
  }

  ChannelTagType tag_type() const
  {
    return this->tag_type_;
  }
  StringRef label() const
  {
    return this->label_;
  }
  void *source_data() const
  {
    return this->source_data_;
  }

  int64_t hash() const
  {
    return get_default_hash(this->tag_type_, this->label_, this->source_data_);
  }
};

/* Not using BLI_set because that can't be iterated thus not turned into a set. */
class ChannelTagStack {
  Vector<ChannelTag> tags_;

 public:
  void push(const ChannelTag &tag)
  {
    tags_.append(tag);
  }
  void pop()
  {
    tags_.pop_last();
  };

  Set<ChannelTag> as_set() const
  {
    return {tags_};
  };
};

class AnimationChannel {
 protected:
  ChannelType type_;
  std::string label_;
  void *source_;
  bool is_dirty_;
  Vector<float> datapoints_;

 public:
  AnimationChannel *parent;

  AnimationChannel(ChannelType type, StringRef label);

  ChannelType type() const
  {
    return this->type_;
  }
  StringRef label() const
  {
    return this->label_;
  }
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

enum class DataChannelType {
  FCURVE,
  GREASE_PENCIL,
};

class DataChannel : public AnimationChannel {
  DataChannelType type_;
  /* The datatype of this is determined by the type. */
  void *source_;
  Set<ChannelTag> tags_;
  DataChannelFlags flags_;

 public:
  DataChannel(FCurve &fcurve);
  DataChannel(GreasePencilLayer &gp_layer);
  bool operator==(const DataChannel &other) const
  {
    /* TODO compare data instead of pointers. */
    return &other == this;
  }

  DataChannelType type() const
  {
    return this->type_;
  }

  int64_t hash() const
  {
    /* TODO hash tags too. */
    return get_default_hash(this->type_, this->source_);
  }

  /**
   * Makes a local copy of the tag elements.
   */
  void set_tags(const ChannelTagStack &tags);
  template<typename T> T get_source() const;

 protected:
  void gather_channeldata() override;
};

struct CollectionFilters {
  bool only_selected;

  /* Limit the returned data to only channels of the given type.
   * If empty, all channel types are returned. */
  Set<ChannelType> channel_type_whitelist;
  /* Any id types put here will be ignored in the channel collection process. */
  Set<ID_Type> id_type_blacklist;
};

/**
 * Collect all channels of the given Action.
 */
void collect_action_channels(Vector<DataChannel> &r_channels);

/**
 * Collect all channels in the given context.
 */
void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                Vector<DataChannel> &r_channels);

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

/**
 * Draw the whole tree starting at the root.
 */
void draw_channel_tree(SummaryChannel &root, View2D &v2d);

}  // namespace ed::animfilter
}  // namespace blender
