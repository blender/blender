/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */
#pragma once

#include <iostream>
#include <limits>
#include <type_traits>

#include "BLI_linear_allocator.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_ID_enums.h"
#include "DNA_action_types.h"

#include "RNA_path.hh"
#include "RNA_types.hh"

namespace blender {

struct bContext;
struct FCurve;
struct GreasePencil;
struct GreasePencilLayer;
struct Panel;

namespace ed::animfilter {

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
  /* See AnimationChannel::sorting_priority_. */
  int priority_;

 public:
  ChannelTag(ChannelTagType type,
             StringRef label,
             const PointerRNA &source_data,
             int sort_priority_ = 0);
  bool operator==(const ChannelTag &other) const
  {
    return other.tag_type_ == this->tag_type_ &&
           other.source_data_.type == this->source_data_.type &&
           other.source_data_.owner_id == this->source_data_.owner_id &&
           other.source_data_.data == this->source_data_.data && other.label_ == this->label_;
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
  int sort_priority() const
  {
    return this->priority_;
  }

  int64_t hash() const
  {
    return get_default_hash(this->tag_type_,
                            this->label_,
                            this->source_data_.type,
                            this->source_data_.owner_id,
                            this->source_data_.data);
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

/* A point in time with various properties. */
struct KeyPoint {
  float frame = std::numeric_limits<float>::infinity();
  bool is_selected = false;

  /** Construct default KeyPoint at infinity. */
  constexpr KeyPoint() = default;
  /** Construct a KeyPoint at a specific frame. */
  explicit constexpr KeyPoint(const float frame) : frame(frame) {}

  int64_t hash() const
  {
    return get_default_hash(frame);
  }

  bool operator<(const KeyPoint &other) const
  {
    return this->frame < other.frame;
  }

  bool operator==(const KeyPoint &other) const
  {
    /* Equality is given if the keys are on the same frame. */
    return this->frame == other.frame;
  }

  void merge(const KeyPoint &other)
  {
    BLI_assert(other == *this);
    is_selected |= other.is_selected;
  }
};

class AnimationChannel {
 protected:
  AnimationChannelType type_ = AnimationChannelType::UNKNOWN;
  AnimationChannelFlags flags_ = ANIM_CHANNEL_DIRTY;
  /* Mutable so that a call to label() can store its result. Labels should NOT be used as
   * identifier/key for AnimationChannel instances. */
  mutable std::string label_;
  PointerRNA source_;
  Vector<KeyPoint> datapoints_;
  /* Higher numbers will be sorted before lower numbers. In case the values are equal a fallback is
   * used. */
  int sorting_priority_ = 0;

  using ParsedSourcePath = blender::ParsedRNAPath<6>;
  mutable std::optional<ParsedSourcePath> source_rna_path_;

 public:
  AnimationChannel(AnimationChannelType type, StringRef label);
  AnimationChannel(const AnimationChannel &other);
  AnimationChannel(AnimationChannel &&other);
  virtual ~AnimationChannel() = default;

  /* Remove assignment operators. If we need them, they need to be implemented properly. */
  AnimationChannel &operator=(const AnimationChannel &) = delete;
  AnimationChannel &operator=(AnimationChannel &&) = delete;

  virtual int64_t hash() const;

  AnimationChannelType type() const
  {
    return this->type_;
  }
  virtual StringRef label() const
  {
    return this->label_;
  }
  PointerRNA source() const
  {
    return this->source_;
  }

  AnimationChannelFlags *flags_ptr()
  {
    return &this->flags_;
  }
  Span<KeyPoint> datapoints();

  int sorting_priority() const
  {
    return this->sorting_priority_;
  }

  virtual bool is_selected() const = 0;
  virtual void set_selected(bool is_selected) = 0;

 protected:
  virtual void gather_channeldata() = 0;
};

/* This is the class responsible for generating the tree structure. Each channel only tracks its
 * children, which allows us to generate multiple summary trees for the same data channels. */
class SummaryChannel final : public AnimationChannel {
  Vector<AnimationChannel *> children_;

 public:
  SummaryChannel(StringRef label);
  SummaryChannel(StringRef label, const PointerRNA &ptr, int sorting_priority);
  ~SummaryChannel() final = default;

  /**
   * The pointer will always be valid.
   */
  Span<AnimationChannel *> children() const;
  void add_child(AnimationChannel &child);
  void remove_child(AnimationChannel &child);
  /**
   * Sort the children of this summary channel.
   */
  void sort_children();

  bool is_selected() const override;
  void set_selected(bool is_selected) override;

 protected:
  void gather_channeldata() override;
};

enum DataChannelFlags {
  DATACHANNEL_FLAG_PINNED = 1 << 0,
};

enum class DataChannelType {
  FCURVE,
  GREASE_PENCIL,
};

/* Stream operators for debug printing to std::cout. */
std::ostream &operator<<(std::ostream &stream, DataChannelType type);
std::ostream &operator<<(std::ostream &stream, AnimationChannelType type);

class DataChannel : public AnimationChannel {
 protected:
  /* While this could read the AnimationChannel::source_ for its type, the RNA types can't be put
   * into a switch statement which is why I (christoph) ended up keeping this. */
  DataChannelType source_type_;
  Set<ChannelTag> tags_;
  DataChannelFlags flags_ = DataChannelFlags(0);

 public:
  DataChannel(StringRef label, DataChannelType source_type);
  ~DataChannel() override = default;

  bool operator==(const DataChannel &other) const
  {
    /* TODO compare data instead of pointers. */
    return &other == this;
  }

  int64_t hash() const override
  {
    return get_default_hash(AnimationChannel::hash(), this->source_type_, this->flags_);
  }

  DataChannelType source_type() const
  {
    return this->source_type_;
  }

  /**
   * Makes a local copy of the tag elements.
   */
  void set_tags(const ChannelTagStack &tags);
  const ChannelTag *get_tag(ChannelTagType tag_type) const;
  ChannelTag *get_tag(ChannelTagType tag_type);
  template<typename T> T *get_source() const;
};

class FCurveChannel : public DataChannel {
 public:
  FCurveChannel(const PointerRNA &fcurve_ptr);

  bool is_selected() const override;
  void set_selected(bool is_selected) override;

  /** Construct a label for the F-Curve, depending on its tags. */
  StringRef label() const override;

 protected:
  void gather_channeldata() override;
  const ID *find_animated_id_from_tags() const;
};

class GreasePencilChannel : public DataChannel {
 public:
  GreasePencilChannel(GreasePencil &containing_gp, GreasePencilLayer &gp_layer);

  bool is_selected() const override;
  void set_selected(bool is_selected) override;

 protected:
  void gather_channeldata() override;
};

/**
 * Arena Allocator for animation channels.
 *
 */
class ChannelArena {
 private:
  LinearAllocator<> allocator_;

 public:
  Vector<DataChannel *> data_channels;
  Vector<SummaryChannel *> summary_channels;

  ChannelArena() = default;
  ChannelArena(const ChannelArena &) = delete;
  ChannelArena(ChannelArena &&other);
  ~ChannelArena();
  ChannelArena &operator=(const ChannelArena &) = delete;
  ChannelArena &operator=(ChannelArena &&other);

  /**
   * Allocate a data channel.
   *
   * It is returned, and also stored in #data_channels. It is destructed & freed when this
   * ChannelArena is destructed.
   */
  template<typename ChannelType, typename... ConstructorArgs>
  ChannelType *allocate_data_channel(ConstructorArgs &&...args)
  {
    static_assert(std::is_base_of_v<DataChannel, ChannelType>,
                  "ChannelType must be a subclass of DataChannel");

    /* destruct_ptr<> causes the channel to be destructed when it goes out of scope. That's
     * avoided here (hence the call to channel.release()) and instead the channels are all
     * destructed when the ChannelArena itself is destructed. */
    destruct_ptr<ChannelType> channel = allocator_.construct<ChannelType>(
        std::forward<ConstructorArgs>(args)...);
    this->data_channels.append(channel.get());
    return channel.release();
  }

  /**
   * Allocate a summary channel.
   *
   * It is returned, and also stored in #summary_channels. It is destructed & freed when this
   * ChannelArena is destructed.
   */
  template<typename... ConstructorArgs>
  SummaryChannel *allocate_summary_channel(ConstructorArgs &&...args)
  {
    /* destruct_ptr<> causes the channel to be destructed when it goes out of scope. That's
     * avoided here (hence the call to channel.release()) and instead the channels are all
     * destructed when the ChannelArena itself is destructed. */
    destruct_ptr<SummaryChannel> channel = allocator_.construct<SummaryChannel>(
        std::forward<ConstructorArgs>(args)...);
    this->summary_channels.append(channel.get());
    return channel.release();
  }

  /**
   * Return the total number of channels (data + summary).
   */
  int64_t num_channels() const
  {
    return this->summary_channels.size() + this->data_channels.size();
  }

  /**
   * Return the root summary channel, if there is one.
   */
  SummaryChannel *root()
  {
    return this->summary_channels.is_empty() ? nullptr : this->summary_channels[0];
  }
};

struct CollectionFilters {
  bool only_selected;

  /* Limit the returned data to only channels of the given type.
   * If empty, all channel types are returned. */
  Set<AnimationChannelType> channel_type_whitelist;
  /* Any id types put here will be ignored in the channel collection process. */
  Set<ID_Type> id_type_blacklist;
};

/**
 * Collect all channels of the given Action.
 */
void collect_action_channels(ChannelArena &arena);

/**
 * Collect all channels in the given context.
 */
void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                ChannelArena &arena);

/**
 * Group the arena's data channels by setting parent child relationships.
 *
 * The summary channels are stored in `arena.summary_channels`.
 *
 * \param tag_order: determines in which order the groups should be created. Items early in
 * the list are parents to items later in the list.
 * \param use_minimal_grouping: If true, group creation is skipped if it would move all chilren to
 * a subgroup.
 */
void group_channels(ChannelArena &arena,
                    Span<ChannelTagType> tag_order,
                    bool use_minimal_grouping);

/**
 * Draws the channel tree using a Tree View in a Layout.
 */
void draw_channel_tree(const bContext &C, SummaryChannel &root, Panel &panel);
/**
 * Draw the whole tree starting at the root using the legacy UI code.
 * TODO maybe the drawing code should not be in this header?
 * TODO remove this if tree view ends up working.
 */
void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D &v2d);

void draw_dopesheet_key_area(const bContext &C, SummaryChannel &root, View2D &v2d);

}  // namespace ed::animfilter
}  // namespace blender
