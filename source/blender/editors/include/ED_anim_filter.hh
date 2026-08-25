/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#include <iostream>

#include "float.h"

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_ID_enums.h"
#include "DNA_action_types.h"

namespace blender {

struct bContext;
struct FCurve;
struct GreasePencil;
struct GreasePencilLayer;
struct PointerRNA;
struct Panel;

/**
 * Stores data that can resolve to any PointerRNA.
 * Keeping this out of the ed::animfilter namespace because it likely doesn't belong here.
 */
struct WeakRNAPointer {
  ID_Type type;
  uint32_t session_uid;
  /* Only used as fallback in case the session_uid has changed which happens on file load. Has to
   * be a copy of the name so we don't access memory of the original struct. */
  std::string id_name;
  /* `path` can be empty when pointing at IDs
   * Could this use ParsedRNAPath? So far there seems to be no API to actually use
   * this anywhere. */
  std::string path = "";

  /* Default constructor to initialize as null pointer. */
  WeakRNAPointer() = default;
  WeakRNAPointer(const PointerRNA &ptr);
  bool operator==(const WeakRNAPointer &other) const
  {
    /* Not comparing  */
    return this->type == other.type && this->session_uid == other.session_uid &&
           this->path == other.path;
  }

  int64_t hash() const
  {
    return get_default_hash(this->type, this->session_uid, this->id_name, this->path);
  }

  bool is_nullptr() const
  {
    return this->id_name.empty();
  }

  /**
   * Return the data this ptr represents. This can be expensive and should only be done sparingly.
   *
   * \warning The returned PointerRNA may not have data if the underlying data no longer exists.
   */
  PointerRNA dereference(Main &bmain) const;
};

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
  WeakRNAPointer source_data_;

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
  const WeakRNAPointer &source_data() const
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

/* A point in time with various properties. */
struct KeyPoint {
  float frame = FLT_MAX;
  bool is_selected = false;

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

class AnimationChannel : public DNAAnimationChannel {
 protected:
  /* For source data lookups during runtime where the data may be removed at any moment. */
  WeakRNAPointer weak_source_;
  /* Datapoints is not declared in the dna struct since it can just be calculated at runtime. */
  Vector<KeyPoint> datapoints_;

 public:
  AnimationChannel(DNAChannelType type, StringRef label);
  AnimationChannel(const AnimationChannel &other);
  AnimationChannel(AnimationChannel &&other);
  virtual ~AnimationChannel();

  int64_t hash() const
  {
    /* TODO maybe special function for DataChannel to also hash tags? */
    return get_default_hash(this->type_, this->weak_source_);
  }

  DNAChannelType type() const
  {
    return this->type_;
  }
  StringRef label() const
  {
    return this->label_;
  }
  AnimationChannelFlags *flags_ptr()
  {
    return &this->flags_;
  }
  Span<KeyPoint> datapoints();

 protected:
  virtual void gather_channeldata() = 0;
};

/* This is the class responsible for generating the tree structure. Each channel only tracks its
 * children, which allows us to generate multiple summary trees for the same data channels. */
class SummaryChannel final : public AnimationChannel {
  Vector<AnimationChannel *> children_;

 public:
  SummaryChannel(StringRef label);
  SummaryChannel(StringRef label, const WeakRNAPointer &ptr);
  ~SummaryChannel() final = default;

  /**
   * The pointer will always be valid.
   */
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

class DataChannel final : public AnimationChannel {
  /* While this could read the AnimationChannel::source_ for its type, the RNA types can't be put
   * into a switch statement which is why I (christoph) ended up keeping this. */
  DataChannelType source_type_;
  Set<ChannelTag> tags_;
  DataChannelFlags flags_ = DataChannelFlags(0);

 public:
  DataChannel(animrig::Action &containing_action, FCurve &fcurve);
  DataChannel(GreasePencil &containing_gp, GreasePencilLayer &gp_layer);
  ~DataChannel() final = default;

  bool operator==(const DataChannel &other) const
  {
    /* TODO compare data instead of pointers. */
    return &other == this;
  }

  DataChannelType source_type() const
  {
    return this->source_type_;
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
  Set<DNAChannelType> channel_type_whitelist;
  /* Any id types put here will be ignored in the channel collection process. */
  Set<ID_Type> id_type_blacklist;
};

/**
 * Collect all channels of the given Action.
 */
void collect_action_channels(Vector<DataChannel *> &r_channels);

/**
 * Collect all channels in the given context.
 */
void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                Vector<DataChannel *> &r_channels);

class ChannelTree {
 public:
  /* TODO make this a reference, at least the summary channel should be created, even if there are
   * no children. */
  SummaryChannel *root;
  int channel_count;
};

/**
 * Group the given channels by setting parent child relationships.
 *
 * \param tag_order: determines in which order the groups should be created. Items early in
 * the list are parents to items later in the list.
 * \param use_minimal_grouping: If true, group creation is skipped if it would move all chilren to
 * a subgroup.
 *
 * \returns: all summary channels that were created. The root is always index 0. They are heap
 * allocated and the caller takes ownership of the data.
 */
Vector<SummaryChannel *> group_channels(MutableSpan<DataChannel *> channels,
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
