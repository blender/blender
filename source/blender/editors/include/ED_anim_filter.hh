/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */
#pragma once

#include <iostream>
#include <limits>
#include <optional>
#include <type_traits>

#include "BLI_bounds.hh"
#include "BLI_function_ref.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
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
struct IDNameLib_Map;
struct Panel;

namespace ed::animfilter {

struct SAction_AnimationChannelState;

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
  GREASEPENCIL_GROUP,
};

using ParsedSourcePath = blender::ParsedRNAPath<6>;

class ChannelTag {
  ChannelTagType tag_type_;
  std::string label_;
  PointerRNA source_data_;

  /* RNA path to the source data, relative to the source's owner_id. If the source is an ID the
   * path is an empty ParsedSourcePath. This is to distinguish "unknown" from "empty". */
  std::optional<ParsedSourcePath> source_rna_path_;

  /* See AnimationChannel::sorting_priority_. */
  int priority_;

 public:
  ChannelTag(ChannelTagType type,
             StringRef label,
             const PointerRNA &source_data,
             std::optional<ParsedSourcePath> source_rna_path,
             int sort_priority_ = 0 /*AnimationChannel::DEFAULT_SORTING*/);

  bool operator==(const ChannelTag &other) const
  {
    return other.tag_type_ == this->tag_type_ &&
           other.source_data_.type == this->source_data_.type &&
           other.source_data_.owner_id == this->source_data_.owner_id &&
           other.source_data_.data == this->source_data_.data && other.label_ == this->label_;
  }
  bool operator!=(const ChannelTag &other) const
  {
    return !(*this == other);
  }

  /* This operator has to be consistent with the above operator==, to ensure that the sorting by
   * tags, and the equality comparison of tags in the channel grouping code, are consistent with
   * each other. */
  bool operator<(const ChannelTag &other) const
  {
    if (tag_type_ != other.tag_type_) {
      return tag_type_ < other.tag_type_;
    }
    if (label_ != other.label_) {
      return label_ < other.label_;
    }
    /* Order the RNA pointer. This is a bit arbitrary, as those are all pointers. The most
     * important part is that it gives a total ordering where unequal (according to operator==)
     * values are ordered consistently.*/
    return std::tie(source_data_.type, source_data_.owner_id, source_data_.data) <
           std::tie(other.source_data_.type, other.source_data_.owner_id, other.source_data_.data);
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
  std::optional<ParsedRNAPathRef> source_rna_path() const
  {
    return this->source_rna_path_;
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

class ChannelTagStack final {
  /* In typical blend files, the stack grows to 7 tags:
   *   1. ACTION 'Mikassa_on2s_wiggle_SHRDS'
   *   2. ACTION_LAYER 'Legacy Layer'
   *   3. ACTION_STRIP 'strip_name_placeholder'
   *   4. ACTION_SLOT 'Legacy Slot'
   *   5. ID 'RIG-mikassa'
   *   6. FCURVE_GROUP 'Shard_321352'
   *   7. BONE 'Shard_321352'
   *
   * Tests with an example file (Mikasa with lots of small objects (shards) animated by one bone
   * each). Times are in milliseconds, counting only the collection of data channels. 'avg' is
   * average, and 'med' is the median.
   *
   * Without reserving : 188, 134, 135, 132, 127; avg 143 med 134
   * With reserving    : 147, 184, 143, 135, 141; avg 150 med 143
   * With 8 inline     : 139, 140, 127, 127, 125, 127; avg 131 med 127
   *
   * So actively reserving 7 items in the vector doesn't actually seem to help performance.
   * Increasing the vector's inline buffer to 8 items (which is needed with the normal allocation
   * strategy of doubling the buffer) there is a slight performance increase of 5%.
   */
  Vector<ChannelTag, 8> tags_;

 public:
  void push(const ChannelTag &tag);
  void pop();
  bool is_empty() const;
  /** \return the number of tags in the set. */
  int64_t size() const;

  const ChannelTag *find_type(const ChannelTagType t) const;
  Span<ChannelTag> snapshot() const;
};

/* See eBezTriple_Interpolation. */
enum class InterpolationSummary : uint8_t {
  NONE,
  MIXED,
  LINEAR,
  CONSTANT,
  BEZIER,
};

enum class KeyframeHandleSummary : uint8_t {
  /* Used when the data does not support handles or the KeyPoint is a mix of various
   * handle types. For legacy reasons this has to stay in the order it is in. `KeyList` assumes
   * this to be sorted for draw priority. */
  UNSPECIFIED,
  AUTO_CLAMP,
  AUTO,
  VECTOR,
  ALIGNED,
  FREE,
};

/* See eKeyframeExtremeDrawOpts. Note that we use fewer values here. The old enum has many values
 * that are not actually used anymore. */
enum class KeyframeExtremeSummary : uint8_t {
  MIXED,
  MAXIMUM,
  MINIMUM,
};

/* A point in time with various properties. */
struct KeyPoint {
  float frame = std::numeric_limits<float>::infinity();
  bool is_selected = false;
  /* Records if there is motion to the *next* KeyPoint. */
  bool has_motion = false;
  InterpolationSummary interpolation = InterpolationSummary::NONE;
  eBezTriple_KeyframeType key_type = BEZT_KEYTYPE_KEYFRAME;
  /* Not using eBezTriple_Handle because that has no "None" case. */
  KeyframeHandleSummary handle_type = KeyframeHandleSummary::UNSPECIFIED;
  KeyframeExtremeSummary extreme_type = KeyframeExtremeSummary::MIXED;

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

  /**
   * Combine the values of the given point with the values of this point. The points don't need to
   * be on the same frame. In case `other` has a lower frame, that frame value is copied.
   */
  void merge(const KeyPoint &other);
};

class ChannelTagSet {
 private:
  Vector<ChannelTag> tags_;
  Vector<bool> used_for_grouping_;

 public:
  ChannelTagSet() = default;
  ChannelTagSet(const ChannelTagSet &) = default;
  ChannelTagSet(ChannelTagSet &&) noexcept = default;
  ChannelTagSet &operator=(const ChannelTagSet &) = default;
  ChannelTagSet &operator=(ChannelTagSet &&) = default;

  void set(const Span<ChannelTag> tags);
  void update_intersect(const ChannelTagSet &other);

  Span<ChannelTag> all() const;
  bool is_empty() const;

  const ChannelTag *first_by_type(const ChannelTagType tag_type) const;
  ChannelTag *first_by_type(const ChannelTagType tag_type);
  const ChannelTag *nth_by_type(ChannelTagType tag_type, int n) const;
  /* Returns true if there is another tag in this set after the given tag of the same type. */
  bool has_another_of_type(const ChannelTag *tag) const;
  Span<const ChannelTag> by_type(const ChannelTagType tag_type) const;
  /* Returns all `ChannelTag`s of the given type. */
  Span<ChannelTag> by_type(const ChannelTagType tag_type);

  /**
   * Return the first not-yet-returned tag with the given tag type.
   * The returned boolean indicates whether there is a subsequent tag of the same type.
   *
   * So in a case of tags [A1, L1, L2, L3, G1], calling this function for tag type 'L' will first
   * return (L1, true), the subsequent call will return (L2, true), followed by (L3, false).
   *
   * \see reset_bookkeeping()
   */
  std::pair<const ChannelTag *, bool> by_type_for_grouping(const ChannelTagType tag_type);

  /**
   * Reset the bookkeeping of which tag has been used for grouping.
   *
   * This is implicitly called by #set() and #update_intersect().
   */
  void reset_bookkeeping();

 private:
  const ChannelTagSet &const_this() const;
};

/**
 * Identifier for an AnimationChannel.
 *
 * Data is copied into the identifier, because these identifiers are designed to out-live the
 * channels they identify. This copying should be fast, as it just consists of some numbers and a
 * bunch of UString instances.
 */
class ChannelIdentifier {
 public:
  /** Owner ID of the channel source. */
  ID_Type owner_id_type = ID_Type(0);
  /* It would be tempting to use an ID* here directly. However, such pointers _must_ be remappable,
   * for example to set to nullptr when the ID is deleted. This means that any struct using an ID*
   * is inherently mutable, but this struct is meant to serve as a mapping key and thus MUST be
   * immutable. */
  unsigned int owner_session_uid = 0;
  /** Path from the ID to the channel source data. */
  ParsedSourcePath source_rna_path = {};

  /**
   * Hash of the channels' parent.
   * This is only set when data channels have been grouped into a hierarchy.
   */
  int64_t parent_hash = 0;

  bool operator==(const ChannelIdentifier &other) const;
  int64_t hash() const;

  /** Stream operator, which needs a bmain for looking up the session UID. */
  void stream_to(std::ostream &stream, Main *bmain) const;
  void stream_to(std::ostream &stream, IDNameLib_Map *idmap) const;
};

/**
 * SpaceAction runtime data containing animation channel UI state.
 *
 * Fields that need persisting to the blend file need to be mirrored in #SpaceActionChannelUIState
 * in source/blender/makesdna/DNA_action_types.h.
 */
struct SAction_AnimationChannelState {
  /** Whether the channel should be collapsed or not.
   *
   * Uses 'is_collapsed' (and not 'is_expanded') following AbstractTreeViewItem.
   */
  bool is_collapsed = false;

  /**
   * A 'timestamp' that's set from #SpaceAction_Runtime::last_used, used to detect the
   * least-recently-used ones for culling the remembered states.
   */
  int32_t last_used = 0;
};

/* Stream operators for debugging. */
std::ostream &operator<<(std::ostream &stream, ChannelTagType type);
std::ostream &operator<<(std::ostream &stream, const ChannelTag &tag);
std::ostream &operator<<(std::ostream &stream, const ChannelTagSet &tagset);
std::ostream &operator<<(std::ostream &stream, const ChannelTagStack &tagstack);
std::ostream &operator<<(std::ostream &stream, const ParsedSourcePath &rna_path);

enum class SelectionMode {
  ADD = 0,
  SUBTRACT = 1,
  INVERT = 2,
  /* There is no REPLACE mode since that is a 2 step procedure of SUBTRACT on all and then ADD the
     relevant keys. This is also not particularly useful to do on a per channel basis. */
};

/* Abstracts data access to a struct type. This avoids checking the RNA type in various
 * functions. To add a new channel type, implement a class inheriting `AbstractChannelAPI` and add
 * a new return path in `create_channel_api`. */
class AbstractChannelAPI {
 public:
  virtual ~AbstractChannelAPI() = default;
  virtual void set_selected(bool is_selected) = 0;
  virtual bool is_selected() const = 0;
};

/**
 * Allocates a channel api for the given type.
 * Memory has to be handled by the caller.
 */
std::unique_ptr<AbstractChannelAPI> create_channel_api(PointerRNA &ptr);

class AnimationChannel {
 public:
  /**
   * Sentinel value for #sorting_priority_, indicating that this channel should not be sorted.
   *
   * This intentionally has the lowest possible value, so that all sortable channels are sorted
   * above unsortable channels.
   */
  constexpr static int NO_SORTING = std::numeric_limits<int>::lowest();
  constexpr static int DEFAULT_SORTING = 0;
  constexpr static ID_Type IDTYPE_UNKNOWN = ID_Type(0);

 protected:
  AnimationChannelType type_ = AnimationChannelType::UNKNOWN;
  AnimationChannelFlags flags_ = ANIM_CHANNEL_DIRTY;
  /* Mutable so that a call to label() can store its result. Labels should NOT be used as
   * identifier/key for AnimationChannel instances. */
  mutable std::string label_;

  /* The RNA path that identifies the source of this channel, relative to source_.owner_id. */
  std::optional<ParsedSourcePath> source_rna_path_;

  Vector<KeyPoint> datapoints_;
  /* Higher numbers will be sorted before lower numbers. In case the values are equal a fallback is
   * used. A special value #NO_SORTING indicates that the channel should not be sorted relative to
   * other NO_SORTING channels. */
  int sorting_priority_ = DEFAULT_SORTING;

  /**
   * The tags of a DataChannel are defined by its source.
   * The tags of a SummaryChannel are the intersection of its child channels.
   */
  ChannelTagSet tags_;

 private:
  /* The source of this channel. */
  PointerRNA source_;
  mutable ID_Type source_id_type_ = IDTYPE_UNKNOWN;

  /* Provides a function interface for the `source_` data.*/
  std::unique_ptr<AbstractChannelAPI> channel_api_;

  /**
   * For channels that have a parent channel, this is set to the hash of that parent.
   *
   * Since #parent_hash_ is also used to compute this channel's hash and its identifier, multiple
   * channels for the same data can appear in different sub-hierarchies and still retain unique UI
   * data. An example is an Action nested underneath each object in animates.
   */
  int64_t parent_hash_ = 0;

 public:
  AnimationChannel(AnimationChannelType type, StringRef label);
  AnimationChannel(const AnimationChannel &other);
  AnimationChannel(AnimationChannel &&other);
  virtual ~AnimationChannel() = default;

  /* Remove assignment operators. If we need them, they need to be implemented properly. */
  AnimationChannel &operator=(const AnimationChannel &) = delete;
  AnimationChannel &operator=(AnimationChannel &&) = delete;

  virtual int64_t hash() const;

  ChannelIdentifier identifier() const
  {
    if (!source_.has_owner_id() || !source_.has_data()) {
      return {};
    }
    return {
        source_.owner_id->id_type(),
        source_.owner_id->session_uid,
        /* The identifier makes no distinction between 'no path' or 'empty path'. */
        /* TODO: maybe this class should also remove this distinction? */
        source_rna_path_.value_or(ParsedSourcePath()),
        parent_hash_,
    };
  }

  void parent_hash_set(const int64_t hash)
  {
    this->parent_hash_ = hash;
  }

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
  bool source_is_id() const;
  ID_Type source_id_type() const;

  AnimationChannelFlags *flags_ptr()
  {
    return &this->flags_;
  }
  Span<KeyPoint> datapoints();

  int sorting_priority() const
  {
    return this->sorting_priority_;
  }

  const ChannelTagSet &tags() const
  {
    return this->tags_;
  }
  ChannelTagSet &tags()
  {
    return this->tags_;
  }

  std::optional<ParsedRNAPathRef> source_rna_path() const
  {
    return this->source_rna_path_;
  }

  bool is_selected() const
  {
    if (!this->channel_api_) {
      return false;
    }
    return this->channel_api_->is_selected();
  };
  /**
   * Selects the channel itself. If selection syncing is used this can also select the data if that
   * is selectable.
   */
  void set_selected(bool is_selected)
  {
    if (!this->channel_api_) {
      return;
    }
    this->channel_api_->set_selected(is_selected);
  };
  /**
   * Change the selection of datapoints in the given range.
   * Range is inclusive/inclusive.
   */
  virtual void change_selection(SelectionMode mode,
                                Bounds<float> range = {-INFINITY, INFINITY}) = 0;

  /**
   * Return the closest datapoint or a nullptr if no datapoint is in the given
   * range.
   */
  const KeyPoint *closest_datapoint(float frame, Bounds<float> range = {-INFINITY, INFINITY});

 protected:
  virtual void gather_channeldata() = 0;

  /* Set source_ and source_id_type_ from this RNA pointer. */
  void source_set(const PointerRNA &source_ptr);
  void source_set(PointerRNA &&source_ptr);
};

using ChannelUIStateMap =
    Map<ed::animfilter::ChannelIdentifier, ed::animfilter::SAction_AnimationChannelState>;

/* This is the class responsible for generating the tree structure. Each channel only tracks its
 * children, which allows us to generate multiple summary trees for the same data channels. */
class SummaryChannel final : public AnimationChannel {
  Vector<AnimationChannel *> children_;

  /**
   * Pointer to the space's UI state map. This is used to lazily look up this channel's UI state.
   */
  ChannelUIStateMap *ui_state_map_ = nullptr;
  bool is_ui_state_map_used_ = false;
  /* The tag type for which this channel was created. Not every summary channel is created for a
   * tag so this is not always set. */
  std::optional<ChannelTagType> tag_type_;

  /** Collapsed/expanded state, if known. */
  std::optional<bool> is_collapsed_ = {};

 public:
  SummaryChannel(StringRef label);
  SummaryChannel(const ChannelTag *tag);
  ~SummaryChannel() final = default;

  /**
   * The pointer will always be valid.
   */
  Span<AnimationChannel *> children() const;
  void add_child(AnimationChannel &child);
  void insert_child(int insert_index, AnimationChannel &child);
  void remove_child(AnimationChannel &child);

  /**
   * Recursively push this channel's hash to its children's #parent_hash_.
   *
   * This ensures that any channel identifiers built for these channels include a value that is
   * unique to the chain of parents that leads to a channel.
   */
  void push_hash_to_children();

  /**
   * Sort the children of this summary channel.
   */
  void sort_children();

  void change_selection(SelectionMode mode, Bounds<float> range = {-INFINITY, INFINITY}) override;

  /** Let the channel know where to find its UI state. */
  void ui_state_map_set(ChannelUIStateMap *ui_state_map);
  /**
   * Return the collapsed/expanded state.
   * \note this is not thread-safe, as it can lazily load the UI state.
   */
  bool is_collapsed() const;

  std::optional<ChannelTagType> tag_type() const
  {
    return tag_type_;
  }

 protected:
  void gather_channeldata() override;

  /** Update the properties of this channel from the UI state. */
  void ui_state_ensure();
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
  DataChannel(StringRef label,
              DataChannelType source_type,
              const ParsedSourcePath &source_rna_path);
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

  template<typename T> T *get_source() const;
};

class FCurveChannel : public DataChannel {
 public:
  FCurveChannel(const PointerRNA &fcurve_ptr, ParsedSourcePath source_rna_path);

  /** Construct a label for the F-Curve, depending on its tags. */
  StringRef label() const override;
  void change_selection(SelectionMode mode, Bounds<float> range = {-INFINITY, INFINITY}) override;

 protected:
  void gather_channeldata() override;
  const ID *find_animated_id_from_tags() const;
};

class GreasePencilChannel : public DataChannel {
 public:
  GreasePencilChannel(const PointerRNA &gp_layer_ptr,
                      ParsedSourcePath source_rna_path,
                      int sorting_priority);

  void change_selection(SelectionMode mode, Bounds<float> range = {-INFINITY, INFINITY}) override;

 protected:
  void gather_channeldata() override;
};

/* The row_index is the depth first number of the row accounting for child channels. */
using ForEachChannelCallback = FunctionRef<void(AnimationChannel &channel, int row_index)>;

/**
 * Arena Allocator for animation channels.
 *
 */
class ChannelArena {
 private:
  LinearAllocator<> allocator_;

 public:
  /* Heap allocated data channels. */
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
  bool only_selected = true;
  /* Show keys of hidden objects/bones. This is only used if `only_selected` is false. */
  bool show_hidden = false;
  bool only_errors = false;
  /**
   * Skip any IDs that an object modifier points to.
   * See the comment on ADS_FILTER_NOMODIFIERS. I (christoph) don't think this is a useful
   * setting, but it is kept for backwards compatibility. */
  bool ignore_modifier_ids = false;

  /* Limit the returned data to only channels of the given type.
   * If empty, all channel types are returned. */
  Set<AnimationChannelType> channel_type_allowlist;
  /* Any id types put here will be ignored in the channel collection process. */
  Set<ID_Type> id_type_blocklist;
};

/**
 * Collect all channels for the Action Editor.
 */
void collect_action_editor_channels(const bContext &C,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena);
/**
 * Collect all channels in the given context, for Dope Sheet and Graph Editor.
 */
void collect_animation_channels(const bContext &C,
                                const CollectionFilters &collection_filters,
                                ChannelArena &arena);

/**
 * Group the arena's data channels by setting parent child relationships.
 *
 * The summary channels are stored in `arena.summary_channels`.
 *
 * \note that this can change the order of datachannels in `arena`.
 *
 * \param tag_order: determines in which order the groups should be created. Items early in
 * the list are parents to items later in the list.
 * \param use_minimal_grouping: If true, group creation is skipped if it would move all chilren
 * to a subgroup.
 */
void group_channels(ChannelArena &arena,
                    Span<ChannelTagType> tag_order,
                    bool use_minimal_grouping);

/**
 * Draw the whole tree starting at the root using the legacy UI code.
 * TODO maybe the drawing code should not be in this header?
 */
void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D &v2d);

void draw_dopesheet_key_area(SummaryChannel &root, View2D &v2d);

/**
 * Iterate all expanded channels and their children.
 */
void foreach_expanded(SummaryChannel &root, ForEachChannelCallback callback);

}  // namespace ed::animfilter
}  // namespace blender
