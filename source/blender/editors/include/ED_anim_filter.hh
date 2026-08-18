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
struct PointerRNA;

namespace ed::animfilter {

enum class ChannelType {
  DATA,
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
  PointerRNA source_data_;

 public:
  ChannelTag(ChannelTagType type, StringRef label, const PointerRNA &source_data);
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
  const PointerRNA &source_data() const
  {
    return this->source_data_;
  }

  int64_t hash() const
  {
    return get_default_hash(this->tag_type_, this->label_, this->source_data_.data);
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

enum AnimationChannelFlags : int8_t {
  ANIM_CHANNEL_DIRTY = 1 << 0,
  SUMMARY_EXPANDED = 1 << 1,
};
ENUM_OPERATORS(AnimationChannelFlags);

class AnimationChannel {
 protected:
  ChannelType type_;
  std::string label_;
  PointerRNA source_;
  AnimationChannelFlags flags_ = AnimationChannelFlags(ANIM_CHANNEL_DIRTY | SUMMARY_EXPANDED);
  Vector<float> datapoints_;

 public:
  AnimationChannel(ChannelType type, StringRef label);

  ChannelType type() const
  {
    return this->type_;
  }
  StringRef label() const
  {
    return this->label_;
  }
  Span<float> datapoints();

 protected:
  virtual void gather_channeldata() = 0;
};

/* This is the class responsible for generating the tree structure. We only track children, which
 * allows us to generate multiple summary trees for the same data channels. */
class SummaryChannel : public AnimationChannel {
  Vector<AnimationChannel *> children_;

 public:
  SummaryChannel(StringRef label);
  SummaryChannel(StringRef label, const PointerRNA &ptr);
  /**
   * The pointer will always be valid.
   */
  Span<AnimationChannel *> children() const;
  void add_child(AnimationChannel &child);
  void remove_child(AnimationChannel &child);
  bool is_expanded()
  {
    return this->flags_ & SUMMARY_EXPANDED;
  }

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
  /* While this could read the AnimationChannel::source_ for its type, the RNA types can't be put
   * into a switch statement which is why I (christoph) ended up keeping this. */
  DataChannelType source_type_;
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

  DataChannelType source_type() const
  {
    return this->source_type_;
  }

  int64_t hash() const
  {
    /* TODO hash tags too. */
    return get_default_hash(this->type_, this->source_.data);
  }

  /**
   * Makes a local copy of the tag elements.
   */
  void set_tags(const ChannelTagStack &tags);
  ChannelTag *get_tag(ChannelTagType tag_type);
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
 * This owns the memory.
 */
class SummaryTree {
  Vector<SummaryChannel *> summaries_;

 public:
  SummaryTree() = default;
  ~SummaryTree()
  {
    for (SummaryChannel *summary : summaries_) {
      MEM_delete(summary);
    }
  }

  SummaryChannel &allocate_summary(StringRef label, const PointerRNA *ptr);
  SummaryChannel *root() const;

  int summary_count() const
  {
    return summaries_.size();
  }
};

/**
 * Group the given channels by setting parent child relationships.
 *
 * \param tag_order: determines in which order the groups should be created. Items early in
 * the list are parents to items later in the list.
 * \param use_minimal_grouping: If true, group creation is skipped if it would move all chilren to
 * a subgroup.
 *
 * \returns: all summary channels that were created. The root is always index 0.
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
