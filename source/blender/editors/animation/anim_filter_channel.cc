/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <ostream>
#include <string>

#include "BLI_assert.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"
#include "ED_grease_pencil.hh"

#include "ANIM_action.hh"
#include "ANIM_rna.hh"

namespace blender::ed::animfilter {

std::ostream &operator<<(std::ostream &stream, const DataChannelType type)
{
  switch (type) {
    case DataChannelType::FCURVE:
      stream << "FCURVE";
      break;
    case DataChannelType::GREASE_PENCIL:
      stream << "GREASE_PENCIL";
      break;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const AnimationChannelType type)
{
  switch (type) {
    case AnimationChannelType::UNKNOWN:
      stream << "UNKNOWN";
      break;
    case AnimationChannelType::DATA:
      stream << "DATA";
      break;
    case AnimationChannelType::SUMMARY:
      stream << "SUMMARY";
      break;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChannelTagType type)
{
  switch (type) {
    case ChannelTagType::FCURVE_GROUP:
      stream << "FCURVE_GROUP";
      break;
    case ChannelTagType::ACTION_STRIP:
      stream << "ACTION_STRIP";
      break;
    case ChannelTagType::ACTION_LAYER:
      stream << "ACTION_LAYER";
      break;
    case ChannelTagType::ACTION_SLOT:
      stream << "ACTION_SLOT";
      break;
    case ChannelTagType::ACTION:
      stream << "ACTION";
      break;
    case ChannelTagType::BONE:
      stream << "BONE";
      break;
    case ChannelTagType::ID:
      stream << "ID";
      break;
    case ChannelTagType::SHAPE_KEY:
      stream << "SHAPE_KEY";
      break;
    case ChannelTagType::GREASEPENCIL_GROUP:
      stream << "GREASEPENCIL_GROUP";
      break;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChannelTag &tag)
{
  stream << tag.tag_type() << " '" << tag.label() << "'";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChannelTagSet &tagset)
{
  stream << "[";
  bool is_first = true;
  for (const ChannelTag &tag : tagset.all()) {
    if (!is_first) {
      stream << "; ";
    }
    is_first = false;
    stream << tag;
  }
  stream << "]";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChannelTagStack &tagstack)
{
  stream << "[";
  bool is_first = true;
  for (const ChannelTag &tag : tagstack.snapshot()) {
    if (!is_first) {
      stream << "; ";
    }
    is_first = false;
    stream << tag;
  }
  stream << "]";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ParsedSourcePath &rna_path)
{
  if (rna_path.items.is_empty()) {
    stream << "-empty path-";
  }
  else {
    stream << rna_path.to_string();
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChannelIdentifier &chan_ident)
{
  Main *bmain = G_MAIN;

  if (chan_ident.owner_id_type == ID_Type(0) ||
      chan_ident.owner_session_uid == MAIN_ID_SESSION_UID_UNSET)
  {
    stream << "-no-owner-";
  }
  else {
    const ID *owner_id = BKE_libblock_find_session_uid(
        bmain, chan_ident.owner_id_type, chan_ident.owner_session_uid);
    stream << "[";
    if (owner_id) {
      stream << owner_id->name;
    }
    else {
      const char *id_type = reinterpret_cast<const char *>(&chan_ident.owner_id_type);
      stream << "-stale-" << id_type[0] << id_type[1] << "-";
    }
  }

  stream << " ";
  stream << chan_ident.source_rna_path;
  stream << "]";
  return stream;
}

void debug_stream_hierarchy(std::ostream &stream,
                            const AnimationChannel &channel,
                            const StringRef indent)
{
  /* Show the channel itself. */
  stream << indent << "\033[90m" << channel.type() << "\033[0m "
         << "\033[97m" << channel.label() << "\033[0m";
  if (channel.type() == AnimationChannelType::DATA) {
    const DataChannel &data = dynamic_cast<const DataChannel &>(channel);
    stream << " (\033[95m" << data.source_type() << "\033[0m)";
  }
  stream << std::endl;

  /* Show source and data type. */
  if (channel.source().has_data()) {
    const PointerRNA source_ptr = channel.source();
    stream << indent << "   Source: " << RNA_struct_ui_name(source_ptr.type);
    stream << std::endl;
  }
  stream << indent << "   Path  : ";
  if (const std::optional<ParsedRNAPathRef> source_rna_path = channel.source_rna_path()) {
    if (source_rna_path->is_empty()) {
      stream << "\033[90m-empty-\033[0m";
    }
    else {
      stream << "\033[38;5;214m\033[97m" << rna_path::to_string(*source_rna_path) << "\033[0m";
    }
  }
  else {
    stream << "\033[90m-none-\033[0m";
  }
  stream << std::endl;

  /* Show the tags. */
  stream << indent << "   Tags  : " << channel.tags() << std::endl;

  /* Recurse into children. */
  if (channel.type() == AnimationChannelType::SUMMARY) {
    const SummaryChannel &summary = dynamic_cast<const SummaryChannel &>(channel);
    std::string child_indent = indent + "  ";
    for (const AnimationChannel *child : summary.children()) {
      debug_stream_hierarchy(stream, *child, child_indent);
    }
  }
}

/**
 * Assert that the path constructed for this PointerRNA actually matches the RNA path Blender
 * would generate for it.
 */
static void assert_same_rna_path(const ParsedSourcePath &parsed_rna_path,
                                 const PointerRNA &rna_pointer)
{
  UNUSED_VARS_NDEBUG(parsed_rna_path, rna_pointer);
#ifndef NDEBUG
  std::optional<std::string> rna_path_given = parsed_rna_path.to_string();
  std::optional<std::string> rna_path_generated = RNA_path_from_ID_to_struct(&rna_pointer);

  if (rna_path_given == rna_path_generated) {
    return;
  }

  /* A given path that has an empty value will result in a 'no value' generated path, which is a
   * valid case. */
  if (rna_path_given.has_value() && rna_path_given->empty() && !rna_path_generated.has_value()) {
    return;
  }

  std::cerr << "RNA paths do not match:" << std::endl;
  if (rna_path_given) {
    std::cerr << "   Given    : " << *rna_path_given << std::endl;
  }
  else {
    std::cerr << "   Given    : no path" << std::endl;
  }
  if (rna_path_generated) {
    std::cerr << "   Generated: " << *rna_path_generated << std::endl;
  }
  else {
    std::cerr << "   Generated: no path" << std::endl;
  }
  BLI_assert_unreachable();
#endif
}

static InterpolationSummary combine_interpolation(const InterpolationSummary a,
                                                  const InterpolationSummary b)
{
  if (a == b) {
    return a;
  }
  if (a == InterpolationSummary::NONE) {
    return b;
  }
  if (b == InterpolationSummary::NONE) {
    return a;
  }
  return InterpolationSummary::MIXED;
}

void KeyPoint::merge(const KeyPoint &other)
{
  has_motion |= other.has_motion;
  interpolation = combine_interpolation(interpolation, other.interpolation);

  if (other.frame > this->frame) {
    return;
  }

  if (other.frame == this->frame) {
    is_selected |= other.is_selected;
    return;
  }

  /* Always use the lowest frame value. */
  BLI_assert(other.frame < this->frame);
  frame = other.frame;
  is_selected = other.is_selected;
}

ChannelTag::ChannelTag(const ChannelTagType type,
                       const StringRef label,
                       const PointerRNA &source_data,
                       const std::optional<ParsedSourcePath> source_rna_path,
                       const int sort_priority)
    : tag_type_(type),
      label_(label),
      source_data_(source_data),
      source_rna_path_(source_rna_path),
      priority_(sort_priority)
{
  assert_same_rna_path(*source_rna_path_, source_data_);
}

/* ===== AnimationChannel ===== */

AnimationChannel::AnimationChannel(const AnimationChannelType type, const StringRef label)
    : type_(type), label_(label)
{
}

AnimationChannel::AnimationChannel(const AnimationChannel &other)
    : type_(other.type_),
      /* Mark as dirty so that data points don't have to be copied. */
      flags_(other.flags_ | ANIM_CHANNEL_DIRTY),
      label_(other.label_),
      source_(other.source_)
{
}

AnimationChannel::AnimationChannel(AnimationChannel &&other)
    : type_(other.type_),
      flags_(other.flags_),
      label_(std::move(other.label_)),
      source_(std::move(other.source_)),
      datapoints_(std::move(other.datapoints_))
{
  /* Reset the non-std::move'd fields. */
  other.type_ = AnimationChannelType::UNKNOWN;
  other.flags_ = ANIM_CHANNEL_DIRTY;
}

int64_t AnimationChannel::hash() const
{
  return get_default_hash(
      this->type_, this->source_.type, this->source_.owner_id, this->source_.data);
}

Span<KeyPoint> AnimationChannel::datapoints()
{
  if (this->flags_ & ANIM_CHANNEL_DIRTY) {
    this->gather_channeldata();
    this->flags_ &= ~ANIM_CHANNEL_DIRTY;
  }
  return this->datapoints_;
}

/* ===== SummaryChannel ===== */

SummaryChannel::SummaryChannel(const StringRef label)
    : AnimationChannel(AnimationChannelType::SUMMARY, label)
{
}

SummaryChannel::SummaryChannel(const StringRef label,
                               const PointerRNA &source_ptr,
                               std::optional<ParsedSourcePath> source_rna_path,
                               int sorting_priority)
    : AnimationChannel(AnimationChannelType::SUMMARY, label)
{
  this->source_ = source_ptr;
  this->source_rna_path_ = source_rna_path;
  sorting_priority_ = sorting_priority;
}

void SummaryChannel::change_selection(const SelectionMode mode, const Bounds<float> range)
{
  /* Any modifications to the range would happen here. E.g. for time remapping. */
  for (AnimationChannel *child : children_) {
    child->change_selection(mode, range);
  }
}

static bool point_less_than(const KeyPoint &p, const float frame)
{
  return p.frame < frame;
}

const KeyPoint *AnimationChannel::closest_datapoint(const float frame, Bounds<float> range)
{
  Span<KeyPoint> datapoints = this->datapoints();
  if (datapoints.is_empty()) {
    return nullptr;
  }
  const KeyPoint *closest = std::lower_bound(
      datapoints.begin(), datapoints.end(), frame, point_less_than);
  if (closest == datapoints.end()) {
    if (closest != datapoints.begin() && (closest - 1)->frame >= range.min) {
      return closest - 1;
    }
    return nullptr;
  }
  if (closest + 1 != datapoints.end()) {
    if (abs((closest + 1)->frame - frame) < abs(closest->frame - frame)) {
      closest = closest + 1;
    }
  }
  if (closest != datapoints.begin()) {
    if (abs((closest - 1)->frame - frame) < abs(closest->frame - frame)) {
      closest = closest - 1;
    }
  }
  if (closest->frame < range.min || closest->frame > range.max) {
    return nullptr;
  }
  return closest;
}

void SummaryChannel::ui_state_map_set(ChannelUIStateMap *ui_state_map)
{
  this->ui_state_map_ = ui_state_map;
}

void SummaryChannel::ui_state_ensure()
{
  if (this->is_ui_state_map_used_ || !this->ui_state_map_) {
    /* The internal properties have already been updated for the UI state. */
    /* Also having no UI state to read from is fine, it'll just mean using the defaults. */
    return;
  }
  this->is_ui_state_map_used_ = true;

  const SAction_AnimationChannelState *ui_state = this->ui_state_map_->lookup_ptr(
      this->identifier());
  if (!ui_state) {
    /* No UI state stored, so just use the defaults. */
    this->is_collapsed_.reset();
    return;
  }

  /* Update properties from the found UI state. */
  this->is_collapsed_ = ui_state->is_collapsed;
}

bool SummaryChannel::is_collapsed() const
{
  /* Cast away constness so that the UI state can be loaded if necessary. */
  const_cast<SummaryChannel *>(this)->ui_state_ensure();

  if (!this->is_collapsed_.has_value()) {
    /* TODO: let the channel determine its default state based on its source type. */
    return false;
  }
  return this->is_collapsed_.value();
}

void SummaryChannel::gather_channeldata()
{
  PRF_scope_with_name("ed::animfilter::SummaryChannel::gather_channeldata",
                      ProfileCategory::Editor);

  this->datapoints_.clear();

  /* Combine the datapoints of all children by iterating them front to back. Assumes that the data
   * is sorted. */
  Array<int> channel_keypoint_indices(this->children_.size());
  Array<Span<KeyPoint>> channel_keypoints(this->children_.size());
  for (const int channel_index : this->children_.index_range()) {
    channel_keypoints[channel_index] = this->children_[channel_index]->datapoints();
    channel_keypoint_indices[channel_index] = 0;
  }

  /* Used to track if any child channel can be iterated further. */
  bool has_data = true;

  /* Store the index of the child channels for which we need to step to the next KeyPoint. */
  Vector<int> channels_to_advance;
  channels_to_advance.reserve(this->children_.size());
  while (has_data) {
    has_data = false;

    channels_to_advance.clear();
    KeyPoint next_keypoint;

    for (const int channel_index : channel_keypoint_indices.index_range()) {
      Span<KeyPoint> child_keypoints = channel_keypoints[channel_index];
      const int keypoint_index = channel_keypoint_indices[channel_index];
      if (keypoint_index >= child_keypoints.size()) {
        continue;
      }

      has_data = true;
      const KeyPoint &child_keypoint = child_keypoints[keypoint_index];
      const float frame_before_merge = next_keypoint.frame;
      next_keypoint.merge(child_keypoint);
      if (child_keypoint.frame > next_keypoint.frame) {
        continue;
      }

      if (child_keypoint.frame < frame_before_merge) {
        channels_to_advance.clear();
        channels_to_advance.append(channel_index);
      }
      else {
        BLI_assert(child_keypoint.frame == next_keypoint.frame);
        channels_to_advance.append(channel_index);
      }
    }

    for (const int channel_index : channels_to_advance) {
      channel_keypoint_indices[channel_index]++;
    }

    if (has_data) {
      this->datapoints_.append(std::move(next_keypoint));
    }
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

static bool animation_channel_compare_function(const AnimationChannel *channel_a,
                                               const AnimationChannel *channel_b)
{
  if (channel_a->sorting_priority() != channel_b->sorting_priority()) {
    return channel_a->sorting_priority() > channel_b->sorting_priority();
  }

  return false;
}

void SummaryChannel::sort_children()
{
  std::ranges::sort(this->children_, animation_channel_compare_function);
}

bool SummaryChannel::is_selected() const
{
  if (!this->source_.has_data()) {
    return false;
  }

  /* TODO: do we need sub-types for the various source types, or is this enough? */
  /* TODO: sigh, if only C++ supported switch statements here. */
  if (this->source_.type == RNA_Action) {
    animrig::Action *action = this->source_.data_as<animrig::Action>();
    return flag_is_set(action->flag, ACT_SELECTED);
  }

  return false;
}

void SummaryChannel::set_selected(const bool is_selected)
{
  if (!this->source_.has_data()) {
    return;
  }

  if (this->source_.type == RNA_Action) {
    animrig::Action *action = this->source_.data_as<animrig::Action>();
    flag_update(action->flag, ACT_SELECTED, is_selected);
    return;
  }
}

/* ===== DataChannel ===== */

DataChannel::DataChannel(const StringRef label,
                         const DataChannelType source_type,
                         const ParsedSourcePath &source_rna_path)
    : AnimationChannel(AnimationChannelType::DATA, label), source_type_(source_type)
{
  source_rna_path_ = source_rna_path;
}

static std::optional<StringRefNull> last_rna_path_member(const ParsedRNAPathRef path)
{
  if (path.is_empty()) {
    return std::nullopt;
  }
  const rna_path::Member *property = std::get_if<rna_path::Member>(&path.last());
  if (!property) {
    return std::nullopt;
  }
  return property->identifier.ref();
}

FCurveChannel::FCurveChannel(const PointerRNA &fcurve_ptr, const ParsedSourcePath source_rna_path)
    : DataChannel(
          /* Store the RNA path as the default label, so that if the animated ID cannot be found,
             there is at least something recognisable to show. */
          fcurve_ptr.data_as<FCurve>()->rna_path(),
          DataChannelType::FCURVE,
          source_rna_path)
{
  assert_same_rna_path(*source_rna_path_, fcurve_ptr);

  this->source_ = fcurve_ptr;

  FCurve &fcurve = *fcurve_ptr.data_as<FCurve>();
  const ParsedRNAPathRef rna_path = fcurve.rna_path_parsed();
  const std::optional<StringRefNull> last_member = last_rna_path_member(rna_path);
  if (!last_member) {
    return;
  }

  if (last_member.value() == "location") {
    sorting_priority_ = 300 - fcurve.array_index;
  }
  else if (animrig::is_rotation_path(rna_path)) {
    sorting_priority_ = 200 - fcurve.array_index;
  }
  else if (last_member.value() == "scale") {
    sorting_priority_ = 100 - fcurve.array_index;
  }
  else {
    sorting_priority_ = -fcurve.array_index;
  }
}

template<> FCurve *DataChannel::get_source<FCurve>() const
{
  BLI_assert(this->source_type_ == DataChannelType::FCURVE);
  return static_cast<FCurve *>(this->source_.data);
}

template<> GreasePencilLayer *DataChannel::get_source<GreasePencilLayer>() const
{
  BLI_assert(this->source_type_ == DataChannelType::GREASE_PENCIL);
  return static_cast<GreasePencilLayer *>(this->source_.data);
}

/* ===== F-Curve Channel ===== */

void FCurveChannel::gather_channeldata()
{
  PRF_scope_with_name("ed::animfilter::FCurveChannel::gather_channeldata",
                      ProfileCategory::Editor);

  BLI_assert(this->source_type_ == DataChannelType::FCURVE);
  const FCurve *fcurve = this->get_source<FCurve>();
  if (!fcurve || !fcurve->bezt) {
    /* Sampled FCurves cannot be edited and don't show up in the dopesheet. */
    return;
  }

  this->datapoints_.resize(fcurve->totvert);
  float prev_y;
  for (int i = 0; i < fcurve->totvert; i++) {
    BezTriple &key = fcurve->bezt[i];
    KeyPoint p(key.vec[1][0]);
    p.is_selected = BEZT_ISSEL_ANY(&key);
    switch (key.ipo) {
      case BEZT_IPO_CONST:
        p.interpolation = InterpolationSummary::CONSTANT;
        break;
      case BEZT_IPO_LIN:
        p.interpolation = InterpolationSummary::LINEAR;
        break;
      case BEZT_IPO_BEZ:
        p.interpolation = InterpolationSummary::BEZIER;
        break;
      default:
        p.interpolation = InterpolationSummary::MIXED;
        break;
    }
    this->datapoints_[i] = p;

    if (i > 0) {
      this->datapoints_[i - 1].has_motion = prev_y != key.vec[1][1];
    }
    prev_y = key.vec[1][1];
  }
}

void FCurveChannel::change_selection(const SelectionMode mode, const Bounds<float> range)
{
  FCurve *fcurve = this->get_source<FCurve>();
  if (!fcurve || !fcurve->bezt || fcurve->totvert == 0) {
    return;
  }
  if (fcurve->bezt[0].vec[1][0] > range.max ||
      fcurve->bezt[fcurve->totvert - 1].vec[1][0] < range.min)
  {
    return;
  }
  bool frame_on_key;
  const int start_index = BKE_fcurve_bezt_binarysearch_index(
      fcurve->bezt, range.min, fcurve->totvert, &frame_on_key);
  for (int i = start_index; i < fcurve->totvert; i++) {
    BezTriple &key = fcurve->bezt[i];
    if (key.vec[1][0] > range.max) {
      break;
    }
    switch (mode) {
      case SelectionMode::ADD:
        BEZT_SEL_ALL(&key);
        break;
      case SelectionMode::SUBTRACT:
        BEZT_DESEL_ALL(&key);
        break;
      case SelectionMode::INVERT:
        BEZT_SEL_INVERT(&key);
        break;
    }
  }
}

bool FCurveChannel::is_selected() const
{
  const FCurve *fcu = this->get_source<FCurve>();
  return flag_is_set(fcu->flag, FCURVE_SELECTED);
}

void FCurveChannel::set_selected(const bool is_selected)
{
  FCurve *fcu = this->get_source<FCurve>();
  flag_update(fcu->flag, FCURVE_SELECTED, is_selected);
}

StringRef FCurveChannel::label() const
{
  this->hash();
  const ID *animated_id = this->find_animated_id_from_tags();
  if (!animated_id) {
    // TODO: getname_anim_fcurve_for_slot() can come to the rescue here.
    return this->label_;
  }

  char local_label[1024];
  if (getname_anim_fcurve(local_label, const_cast<ID *>(animated_id), this->get_source<FCurve>()))
  {
    this->label_ = local_label;
  }

  return this->label_;
}

const ID *FCurveChannel::find_animated_id_from_tags() const
{
  const ChannelTag *id_tag = this->tags().first_by_type(ChannelTagType::ID);
  if (!id_tag) {
    return nullptr;
  }
  return id_tag->source_data().data_as<ID>();
}

/* ===== GreasePencil Channel ===== */

GreasePencilChannel::GreasePencilChannel(const PointerRNA &gp_layer_ptr,
                                         const ParsedSourcePath source_rna_path)
    : DataChannel(gp_layer_ptr.data_as<GreasePencilLayer>()->wrap().name(),
                  DataChannelType::GREASE_PENCIL,
                  source_rna_path)
{
  this->source_ = gp_layer_ptr;
}

void GreasePencilChannel::gather_channeldata()
{
  PRF_scope_with_name("ed::animfilter::GreasePencilChannel::gather_channeldata",
                      ProfileCategory::Editor);

  BLI_assert(this->source_type_ == DataChannelType::GREASE_PENCIL);

  GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer>();

  bke::greasepencil::Layer &layer = gp_layer->wrap();
  this->datapoints_.resize(layer.frames().size());
  int frame_index = 0;
  int prev_drawing_index = -1;
  for (const auto &[frame, gp_frame] : layer.frames().items()) {
    KeyPoint &p = this->datapoints_[frame_index];
    p.frame = frame;
    p.is_selected = gp_frame.is_selected();
    /* Grease Pencil data has no useful interpolation information. */
    p.interpolation = InterpolationSummary::MIXED;
    if (frame_index > 0) {
      this->datapoints_[frame_index - 1].has_motion = gp_frame.drawing_index != prev_drawing_index;
    }
    prev_drawing_index = gp_frame.drawing_index;
    frame_index++;
  }
}

void GreasePencilChannel::change_selection(const SelectionMode mode, const Bounds<float> range)
{
  GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer>();
  bke::greasepencil::Layer &layer = gp_layer->wrap();
  /* not using ed::greasepencil::select_frames_range because that uses inclusive/exclusive. for its
   * selection range. */
  for (auto [frame, gp_frame] : layer.frames_for_write().items()) {
    if (frame < range.min) {
      continue;
    }
    if (frame > range.max) {
      break;
    }
    switch (mode) {
      case SelectionMode::ADD:
        gp_frame.flag |= GP_FRAME_SELECTED;
        break;

      case SelectionMode::SUBTRACT:
        gp_frame.flag &= ~GP_FRAME_SELECTED;
        break;

      case SelectionMode::INVERT:
        gp_frame.flag ^= GP_FRAME_SELECTED;
        break;
    }
    layer.tag_frames_map_changed();
  }
}

bool GreasePencilChannel::is_selected() const
{
  GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer>();
  bke::greasepencil::Layer &layer = gp_layer->wrap();
  return layer.is_selected();
}

void GreasePencilChannel::set_selected(const bool is_selected)
{
  GreasePencilLayer *gp_layer = this->get_source<GreasePencilLayer>();
  bke::greasepencil::Layer &layer = gp_layer->wrap();
  layer.set_selected(is_selected);
}

/* ===== ChannelTagStack ======= */

void ChannelTagStack::push(const ChannelTag &tag)
{
  tags_.append(tag);
}
void ChannelTagStack::pop()
{
  tags_.pop_last();
};

bool ChannelTagStack::is_empty() const
{
  return tags_.is_empty();
}
/** \return the number of tags in the set. */
int64_t ChannelTagStack::size() const
{
  return tags_.size();
}

const ChannelTag *ChannelTagStack::find_type(const ChannelTagType t) const
{
  for (const ChannelTag &tag : this->tags_) {
    if (tag.tag_type() == t) {
      return &tag;
    }
  }
  return nullptr;
}

Span<ChannelTag> ChannelTagStack::snapshot() const
{
  return {tags_};
}

/* ===== ChannelTagSet ======= */

void ChannelTagSet::set(const Span<ChannelTag> tags)
{
  this->tags_ = tags;
  this->reset_bookkeeping();
}

void ChannelTagSet::update_intersect(const ChannelTagSet &other)
{
  /* Remove all tags that 'other' doesn't have. */
  for (int i = tags_.size() - 1; i >= 0; i--) {
    if (other.tags_.contains(tags_[i])) {
      continue;
    }
    tags_.remove(i);
  }
  this->reset_bookkeeping();
}

Span<ChannelTag> ChannelTagSet::all() const
{
  return this->tags_;
}

bool ChannelTagSet::is_empty() const
{
  return this->tags_.is_empty();
}

const ChannelTagSet &ChannelTagSet::const_this() const
{
  return *this;
}

const ChannelTag *ChannelTagSet::first_by_type(const ChannelTagType tag_type) const
{
  /* TODO store in map so we don't have to search here. */
  for (const ChannelTag &tag : this->tags_) {
    if (tag.tag_type() == tag_type) {
      return &tag;
    }
  }
  return nullptr;
}

ChannelTag *ChannelTagSet::first_by_type(const ChannelTagType tag_type)
{
  /* Defer to the const version, then cast the constness away. */
  return const_cast<ChannelTag *>(const_this().first_by_type(tag_type));
}

Span<const ChannelTag> ChannelTagSet::by_type(const ChannelTagType tag_type) const
{
  /* Find the first tag of the given type. */
  int found_tag_index = -1;
  const ChannelTag *found_tag = nullptr;

  for (const int tag_index : this->tags_.index_range()) {
    found_tag = &this->tags_[tag_index];
    if (found_tag->tag_type() != tag_type) {
      continue;
    }
    found_tag_index = tag_index;
    break;
  }

  if (found_tag_index < 0) {
    BLI_assert(!found_tag);
    /* No tag of this type found. */
    return {};
  }
  BLI_assert(found_tag);

  /* Find the last tag of the given type. */
  for (int tag_index = found_tag_index + 1; tag_index < this->tags_.size(); tag_index++) {
    const ChannelTag &tag = this->tags_[tag_index];
    if (tag.tag_type() != tag_type) {
      /* Found a subsequent tag of another type, we've exhausted the tags of this type. */
      return Span<const ChannelTag>(found_tag, tag_index - found_tag_index + 1);
    }
  }

  return Span<const ChannelTag>(found_tag, this->tags_.size() - found_tag_index);
}

Span<ChannelTag> ChannelTagSet::by_type(const ChannelTagType tag_type)
{
  /* Defer to the const version, then cast the constness away. */
  Span<const ChannelTag> found = const_this().by_type(tag_type);
  return Span<ChannelTag>(const_cast<ChannelTag *>(found.data()), found.size());
}

/**
 * Return the first not-yet-returned tag with the given tag type.
 * The returned boolean indicates whether there is a subsequent tag of the same type.
 *
 * So in a case of tags [A1, L1, L2, L3, G1], calling this function for tag type 'L' will first
 * return (L1, true), the subsequent call will return (L2, true), followed by (L3, false).
 *
 * \see reset_bookkeeping()
 */
std::pair<const ChannelTag *, bool> ChannelTagSet::by_type_for_grouping(
    const ChannelTagType tag_type)
{
  /* Find the first tag of the given type. */
  int found_tag_index = -1;
  for (const int tag_index : this->tags_.index_range()) {
    const ChannelTag &tag = this->tags_[tag_index];
    if (tag.tag_type() != tag_type) {
      continue;
    }
    found_tag_index = tag_index;
    break;
  }

  if (found_tag_index < 0) {
    /* No tag of this type found. */
    return {};
  }

  /* Found the first tag of the right type, now find the first tag (if there's any left) that
   * hasn't been used for grouping yet. */
  for (int tag_index = found_tag_index; tag_index < this->tags_.size(); tag_index++) {
    const ChannelTag &tag = this->tags_[tag_index];
    if (tag.tag_type() != tag_type) {
      /* Found a subsequent tag of another type, we've exhausted the tags of this type. */
      return {};
    }

    if (this->used_for_grouping_[tag_index]) {
      /* This one has been used already, go to the next one. */
      continue;
    }
    this->used_for_grouping_[tag_index] = true;

    /* Found it! Now let's see if the next tag is also usable. This is basically the same check
     * as the one above, except for the consequences of a mis-match. Here 'mismatching tag
     * type' and 'already being used for grouping' have the same conclusion: 'not usable',
     * whereas above these need to be handled separately. */
    const bool has_usable_successor = tag_index + 1 < this->tags_.size() &&
                                      !this->used_for_grouping_[tag_index + 1] &&
                                      this->tags_[tag_index + 1].tag_type() == tag_type;
    return {&tag, has_usable_successor};
  }

  return {};
}

/**
 * Reset the bookkeeping of which tag has been used for grouping.
 *
 * This is implicitly called by #set() and #update_intersect().
 */
void ChannelTagSet::reset_bookkeeping()
{
  used_for_grouping_.resize(tags_.size());
  used_for_grouping_.fill(false);
}

}  // namespace blender::ed::animfilter
