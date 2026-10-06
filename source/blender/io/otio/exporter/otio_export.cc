/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include <algorithm>
#include <map>
#include <tuple>

#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_index_range.hh"
#include "BLI_listbase_iterator.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_vector.hh"

#include "CLG_log.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "opentimelineio/clip.h"
#include "opentimelineio/color.h"
#include "opentimelineio/effect.h"
#include "opentimelineio/externalReference.h"
#include "opentimelineio/freezeFrame.h"
#include "opentimelineio/gap.h"
#include "opentimelineio/generatorReference.h"
#include "opentimelineio/imageSequenceReference.h"
#include "opentimelineio/linearTimeWarp.h"
#include "opentimelineio/marker.h"
#include "opentimelineio/missingReference.h"
#include "opentimelineio/stack.h"
#include "opentimelineio/timeline.h"
#include "opentimelineio/track.h"
#include "opentimelineio/transition.h"

#include "otio_export.hh"
#include "otio_export_resolve.hh"
#include "otio_metadata.hh"

namespace blender::io::otio {

static CLG_LogRef LOG = {"io.otio"};

struct StripExportContext {
  Main *bmain;
  Scene *scene;
  const OTIOExportParams &params;
  double fps;
  int unsupported_scene_strips = 0;
  int unsupported_image_sequences = 0;
  Map<const Strip *, int64_t> resolve_link_groups;
};

struct PlacedItem {
  const Strip *strip;
  int start;
  int end;
  SerializableObject::Retainer<Composable> composable;
};

struct ChannelItems {
  Vector<PlacedItem> video;
  Vector<PlacedItem> audio;
};

struct Tracks {
  Vector<SerializableObject::Retainer<Track>> video;
  Vector<SerializableObject::Retainer<Track>> audio;
};

static RationalTime frames(const StripExportContext &ctx, const double count)
{
  return RationalTime(count, ctx.fps);
}

static TimeRange frame_range(const StripExportContext &ctx, const double start, const int duration)
{
  return TimeRange(frames(ctx, start), frames(ctx, duration));
}

static bool is_single_input_effect(const Strip &strip)
{
  return ELEM(strip.type, STRIP_TYPE_GAUSSIAN_BLUR, STRIP_TYPE_GLOW, STRIP_TYPE_SPEED);
}

static bool is_transition(const Strip &strip)
{
  /* Compositor strips are not yet supported, if we support them then remove this function. */
  return seq::effect_is_transition(strip.type) && strip.type != STRIP_TYPE_COMPOSITOR;
}

static bool is_nested(const Strip &strip)
{
  return strip.type == STRIP_TYPE_META ||
         (strip.type == STRIP_TYPE_SCENE && (strip.flag & SEQ_SCENE_STRIPS));
}

static int media_length(const StripExportContext &ctx, const Strip &strip)
{
  if (seq::transform_single_image_check(&strip)) {
    return strip.right_handle(ctx.scene) - strip.left_handle();
  }
  return strip.anim_startofs + strip.content_length() + strip.anim_endofs;
}

static std::string media_filepath(const StripExportContext &ctx, const Strip &strip)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), strip.data->dirpath, strip.data->stripdata->filename);
  BLI_path_abs(filepath, BKE_main_blendfile_path(ctx.bmain));
  return filepath;
}

static std::string strip_type_identifier(const StripExportContext &ctx, const Strip &strip)
{
  PointerRNA ptr = RNA_pointer_create_discrete(
      &ctx.scene->id, RNA_Strip, const_cast<Strip *>(&strip));
  const char *identifier = "";
  RNA_property_enum_identifier(
      nullptr, &ptr, RNA_struct_find_property(&ptr, "type"), strip.type, &identifier);
  return identifier;
}

static void add_strip_metadata(const StripExportContext &ctx,
                               const Strip &strip,
                               SerializableObjectWithMetadata &object)
{
  PointerRNA ptr = RNA_pointer_create_discrete(
      &ctx.scene->id, RNA_Strip, const_cast<Strip *>(&strip));
  AnyDictionary data;
  data["strip"] = rna_to_dictionary(ptr);
  object.metadata()["blender"] = data;
  add_foreign_metadata(strip.prop, object);
}

/* Return `false` if the image sequence in Blender cannot be represented natively in the
 * OTIO specification (e.g. arbitrarily named filepaths with no digits). */
static bool image_sequence_pattern_get(const StripElem *elems,
                                       const int elems_num,
                                       char *r_name_prefix,
                                       char *r_name_suffix,
                                       int &r_start_frame,
                                       int &r_frame_step,
                                       int &r_padding)
{
  unsigned short digits_len;
  r_start_frame = BLI_path_sequence_decode(
      elems[0].filename, r_name_prefix, FILE_MAX, r_name_suffix, FILE_MAX, &digits_len);
  r_padding = digits_len;
  if (digits_len == 0) {
    return false;
  }

  r_frame_step = 1;
  if (elems_num > 1) {
    r_frame_step = BLI_path_sequence_decode(elems[1].filename, nullptr, 0, nullptr, 0, nullptr) -
                   r_start_frame;
    if (r_frame_step < 1) {
      return false;
    }
  }

  for (const int i : IndexRange(elems_num)) {
    char filename[FILE_MAX];
    BLI_path_sequence_encode(filename,
                             sizeof(filename),
                             r_name_prefix,
                             r_name_suffix,
                             digits_len,
                             r_start_frame + i * r_frame_step);
    if (!STREQ(filename, elems[i].filename)) {
      return false;
    }
  }
  return true;
}

static SerializableObject::Retainer<MediaReference> image_sequence_reference(
    StripExportContext &ctx, const Strip &strip)
{
  char name_prefix[FILE_MAX], name_suffix[FILE_MAX];
  int start_frame, frame_step, padding;
  if (!image_sequence_pattern_get(strip.data->stripdata,
                                  strip.data->stripdata_num,
                                  name_prefix,
                                  name_suffix,
                                  start_frame,
                                  frame_step,
                                  padding))
  {
    ctx.unsupported_image_sequences++;
    return new MissingReference(strip.name + 2);
  }

  char dirpath[FILE_MAX];
  BLI_strncpy(dirpath, strip.data->dirpath, sizeof(dirpath));
  BLI_path_abs(dirpath, BKE_main_blendfile_path(ctx.bmain));

  /* OTIO expects the rate of the full sequence, counting file numbers that don't exist. */
  const double rate = ctx.fps * frame_step;
  return new ImageSequenceReference(dirpath,
                                    name_prefix,
                                    name_suffix,
                                    start_frame,
                                    frame_step,
                                    rate,
                                    padding,
                                    ImageSequenceReference::MissingFramePolicy::error,
                                    frame_range(ctx, 0, media_length(ctx, strip)));
}

static SerializableObject::Retainer<MediaReference> media_reference(StripExportContext &ctx,
                                                                    const Strip &strip)
{
  const bool has_media = strip.data && strip.data->stripdata;

  switch (strip.type) {
    case STRIP_TYPE_MOVIE:
    case STRIP_TYPE_SOUND:
      if (!has_media) {
        return new MissingReference(strip.name + 2);
      }
      return new ExternalReference(media_filepath(ctx, strip),
                                   frame_range(ctx, 0, media_length(ctx, strip)));

    case STRIP_TYPE_IMAGE:
      if (!has_media) {
        return new MissingReference(strip.name + 2);
      }
      if (seq::transform_single_image_check(&strip)) {
        return new ExternalReference(media_filepath(ctx, strip),
                                     frame_range(ctx, 0, media_length(ctx, strip)));
      }
      return image_sequence_reference(ctx, strip);

    case STRIP_TYPE_ADJUSTMENT:
      if (ctx.params.use_resolve_metadata) {
        return new MissingReference(strip.name + 2);
      }
      [[fallthrough]];
    case STRIP_TYPE_COLOR:
    case STRIP_TYPE_TEXT:
    case STRIP_TYPE_ADD:
    case STRIP_TYPE_SUB:
    case STRIP_TYPE_MUL:
    case STRIP_TYPE_ALPHAOVER:
    case STRIP_TYPE_ALPHAUNDER:
    case STRIP_TYPE_COLORMIX: {
      const TimeRange available_range = frame_range(ctx, 0, media_length(ctx, strip));
      if (ctx.params.use_resolve_metadata && ELEM(strip.type, STRIP_TYPE_COLOR, STRIP_TYPE_TEXT)) {
        return resolve_generator_reference(strip, available_range);
      }
      return new GeneratorReference(
          strip.name + 2, strip_type_identifier(ctx, strip), available_range);
    }

    case STRIP_TYPE_SCENE:
      ctx.unsupported_scene_strips++;
      return new MissingReference(strip.name + 2);

    default:
      return nullptr;
  }
}

static float speed_factor(const StripExportContext &ctx,
                          const Strip &input,
                          const Strip &speed_strip)
{
  const SpeedControlVars *speed = static_cast<const SpeedControlVars *>(speed_strip.effectdata);
  switch (speed->speed_control_type) {
    case SEQ_SPEED_STRETCH: {
      const float speed_length = speed_strip.right_handle(ctx.scene) - speed_strip.left_handle();
      return speed_length != 0.0f ? (input.length(ctx.scene) - input.startofs) / speed_length :
                                    0.0f;
    }
    case SEQ_SPEED_MULTIPLY:
      return speed->speed_fader;
    default:
      return 0.0f;
  }
}

static SerializableObject::Retainer<otio::Effect> strip_to_effect(const StripExportContext &ctx,
                                                                  const Strip &input,
                                                                  const Strip &effect_strip)
{
  const std::string name = effect_strip.name + 2;
  SerializableObject::Retainer<otio::Effect> effect;
  if (effect_strip.type != STRIP_TYPE_SPEED) {
    effect = new otio::Effect(name, strip_type_identifier(ctx, effect_strip));
  }
  else if (const float factor = speed_factor(ctx, input, effect_strip); factor != 0.0f) {
    effect = new LinearTimeWarp(name, "", factor);
  }
  else {
    effect = new FreezeFrame(name);
  }
  add_strip_metadata(ctx, effect_strip, *effect);
  return effect;
}

static void add_strip_data(const StripExportContext &ctx,
                           Editing &editing,
                           const Strip &strip,
                           Item &item)
{
  item.set_enabled(!(strip.flag & SEQ_MUTE));
  add_strip_metadata(ctx, strip, item);
  for (const Strip *effect_strip : seq::lookup_effects_by_strip(&editing, &strip)) {
    if (is_single_input_effect(*effect_strip)) {
      item.effects().push_back(strip_to_effect(ctx, strip, *effect_strip));
    }
  }
}

static SerializableObject::Retainer<Clip> strip_to_clip(StripExportContext &ctx,
                                                        Editing &editing,
                                                        const Strip &strip)
{
  SerializableObject::Retainer<MediaReference> reference = media_reference(ctx, strip);
  if (!reference) {
    CLOG_WARN(&LOG, "Skipping strip '%s' of unsupported type", strip.name + 2);
    BKE_reportf(ctx.params.reports,
                RPT_WARNING,
                "Strip '%s' was not exported, its type is currently unsupported",
                strip.name + 2);
    return nullptr;
  }

  double source_start = strip.left_handle() - int(strip.content_start()) + strip.anim_startofs;
  const int duration = strip.right_handle(ctx.scene) - strip.left_handle();
  if (seq::transform_single_image_check(&strip)) {
    source_start = 0;
  }
  if (strip.type == STRIP_TYPE_SOUND) {
    source_start -= strip.sound_offset * ctx.fps;
  }

  auto clip = SerializableObject::Retainer<Clip>(
      new Clip(strip.name + 2, reference, frame_range(ctx, source_start, duration)));
  add_strip_data(ctx, editing, strip, *clip);
  if (ctx.params.use_resolve_metadata) {
    resolve_add_clip_data(*ctx.scene, strip, ctx.resolve_link_groups, *clip);
  }
  return clip;
}

static Tracks export_strips(StripExportContext &ctx,
                            Editing &editing,
                            ListBaseT<Strip> &seqbase,
                            const ListBaseT<SeqTimelineChannel> *channels,
                            int origin);

static bool add_nested_items(StripExportContext &ctx,
                             Editing &editing,
                             const Strip &strip,
                             ChannelItems &items)
{
  ListBaseT<SeqTimelineChannel> *channels;
  int origin;
  ListBaseT<Strip> *seqbase = seq::get_seqbase_from_strip(
      const_cast<Strip *>(&strip), &channels, &origin);
  if (!seqbase) {
    return false;
  }

  Editing &nested_editing = strip.type == STRIP_TYPE_SCENE ? *seq::editing_get(strip.scene) :
                                                             editing;
  const Tracks tracks = export_strips(ctx, nested_editing, *seqbase, channels, origin);
  const int start = strip.left_handle();
  const int end = strip.right_handle(ctx.scene);
  const TimeRange range = frame_range(ctx, start - int(strip.content_start()), end - start);

  for (const bool is_audio : {false, true}) {
    const Vector<SerializableObject::Retainer<Track>> &nested = is_audio ? tracks.audio :
                                                                           tracks.video;
    if (nested.is_empty()) {
      continue;
    }
    auto stack = SerializableObject::Retainer<Stack>(new Stack(strip.name + 2, range));
    for (const SerializableObject::Retainer<Track> &track : nested) {
      stack->append_child(track);
    }
    add_strip_data(ctx, editing, strip, *stack);
    (is_audio ? items.audio : items.video).append({&strip, start, end, stack.value});
  }
  return true;
}

static PlacedItem *find_item(Vector<PlacedItem> &items, const Strip *strip)
{
  for (PlacedItem &item : items) {
    if (item.strip == strip) {
      return &item;
    }
  }
  return nullptr;
}

static void extend_source_range(PlacedItem &item,
                                const RationalTime before,
                                const RationalTime after)
{
  Item *otio_item = dynamic_cast<Item *>(item.composable.value);
  const TimeRange range = otio_item->trimmed_range();
  otio_item->set_source_range(
      TimeRange(range.start_time() - before, range.duration() + before + after));
}

static void add_transition(const StripExportContext &ctx,
                           const Strip &transition,
                           std::map<int, ChannelItems> &channel_items)
{
  const Strip *input1 = transition.input1;
  const Strip *input2 = transition.input2;
  PlacedItem *from = nullptr;
  PlacedItem *to = nullptr;
  if (input1 && input2 && input1->channel == input2->channel) {
    Vector<PlacedItem> &items = channel_items[input1->channel].video;
    from = find_item(items, input1);
    to = find_item(items, input2);
  }
  if (from && to && from->start > to->start) {
    std::swap(from, to);
  }
  if (!from || !to || from->end >= to->start) {
    CLOG_WARN(&LOG, "Skipping transition strip '%s'", transition.name + 2);
    BKE_reportf(ctx.params.reports,
                RPT_WARNING,
                "Transition strip '%s' was not exported, its inputs must be on the same channel",
                transition.name + 2);
    return;
  }

  const int length = to->start - from->end;
  const int in_offset = length / 2;
  const int out_offset = length - in_offset;
  extend_source_range(*from, frames(ctx, 0), frames(ctx, in_offset));
  extend_source_range(*to, frames(ctx, out_offset), frames(ctx, 0));
  from->end += in_offset;
  to->start -= out_offset;

  auto otio_transition = SerializableObject::Retainer<Transition>(
      new Transition(transition.name + 2,
                     transition.type == STRIP_TYPE_WIPE ? Transition::Type::Custom :
                                                          Transition::Type::SMPTE_Dissolve,
                     frames(ctx, in_offset),
                     frames(ctx, out_offset)));
  add_strip_metadata(ctx, transition, *otio_transition);
  if (ctx.params.use_resolve_metadata) {
    resolve_add_transition_data(*ctx.scene, transition, *otio_transition);
  }

  const int cut = from->end;
  channel_items[input1->channel].video.append({&transition, cut, cut, otio_transition.value});
}

static SerializableObject::Retainer<Track> build_track(const StripExportContext &ctx,
                                                       Vector<PlacedItem> &items,
                                                       const std::string &kind,
                                                       const SeqTimelineChannel *channel,
                                                       const int channel_index,
                                                       const int origin)
{
  std::ranges::stable_sort(items, [](const PlacedItem &a, const PlacedItem &b) {
    return std::tie(a.start, a.end) < std::tie(b.start, b.end);
  });

  auto track = SerializableObject::Retainer<Track>(
      new Track(channel ? channel->name : "", std::nullopt, kind));
  track->set_enabled(!(channel && channel->is_muted()));
  AnyDictionary data;
  data["channel"] = int64_t(channel_index);
  track->metadata()["blender"] = data;
  if (ctx.params.use_resolve_metadata) {
    resolve_add_track_data(channel, *track);
  }

  int cursor = origin;
  for (PlacedItem &item : items) {
    if (item.start > cursor) {
      track->append_child(new Gap(frames(ctx, item.start - cursor)));
    }
    track->append_child(item.composable);
    cursor = item.end;
  }
  return track;
}

static Tracks export_strips(StripExportContext &ctx,
                            Editing &editing,
                            ListBaseT<Strip> &seqbase,
                            const ListBaseT<SeqTimelineChannel> *channels,
                            const int origin)
{
  std::map<int, ChannelItems> channel_items;
  for (const Strip &strip : seqbase) {
    if (is_single_input_effect(strip) || is_transition(strip)) {
      continue;
    }
    ChannelItems &items = channel_items[strip.channel];
    if (is_nested(strip) && add_nested_items(ctx, editing, strip, items)) {
      continue;
    }
    if (SerializableObject::Retainer<Clip> clip = strip_to_clip(ctx, editing, strip)) {
      (strip.type == STRIP_TYPE_SOUND ? items.audio : items.video)
          .append({&strip, strip.left_handle(), strip.right_handle(ctx.scene), clip.value});
    }
  }

  for (const Strip &strip : seqbase) {
    if (is_transition(strip)) {
      add_transition(ctx, strip, channel_items);
    }
  }

  Tracks tracks;
  for (auto &[channel_index, items] : channel_items) {
    const SeqTimelineChannel *channel = channels ?
                                            seq::channel_get_by_index(channels, channel_index) :
                                            nullptr;
    if (!items.video.is_empty()) {
      tracks.video.append(
          build_track(ctx, items.video, Track::Kind::video, channel, channel_index, origin));
    }
    if (!items.audio.is_empty()) {
      tracks.audio.append(
          build_track(ctx, items.audio, Track::Kind::audio, channel, channel_index, origin));
    }
  }
  return tracks;
}

static void export_markers(const StripExportContext &ctx, const int origin, Stack &stack)
{
  for (const TimeMarker &marker : ctx.scene->markers) {
    stack.markers().push_back(
        new Marker(marker.name, frame_range(ctx, marker.frame - origin, 1), Color::white));
  }
}

void exporter_main(bContext *C, const OTIOExportParams &params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *editing = seq::editing_get(scene);
  StripExportContext ctx{bmain, scene, params, scene->frames_per_second()};
  if (params.use_resolve_metadata) {
    ctx.resolve_link_groups = resolve_link_groups(editing->seqbase);
  }

  int origin = scene->r.sfra;
  for (const Strip &strip : editing->seqbase) {
    origin = std::min(origin, strip.left_handle());
  }

  const Tracks tracks = export_strips(ctx, *editing, editing->seqbase, &editing->channels, origin);

  auto timeline = SerializableObject::Retainer<Timeline>(
      new Timeline(scene->id.name + 2, frames(ctx, origin)));
  for (const SerializableObject::Retainer<Track> &track : tracks.video) {
    timeline->tracks()->append_child(track);
  }
  for (const SerializableObject::Retainer<Track> &track : tracks.audio) {
    timeline->tracks()->append_child(track);
  }
  export_markers(ctx, origin, *timeline->tracks());
  AnyDictionary data;
  data["version"] = int64_t(BLENDER_FILE_VERSION);
  data["subversion"] = int64_t(BLENDER_FILE_SUBVERSION);
  timeline->metadata()["blender"] = data;
  add_foreign_metadata(scene->id.properties, *timeline);
  if (params.use_resolve_metadata) {
    resolve_add_timeline_metadata(*timeline);
  }

  opentimelineio::OPENTIMELINEIO_VERSION_NS::ErrorStatus error_status;
  /* Some versions of editors like DaVinci Resolve will abort import of the entire timeline if
   * there is at least one newer Marker.3 object; for better compatibility, write older schema
   * instead. */
  const schema_version_map schema_version_targets = {{"Marker", 2}};
  if (!timeline->to_json_file(params.filepath, &error_status, &schema_version_targets)) {
    CLOG_ERROR(&LOG,
               "Failed to write OTIO file '%s': %s",
               params.filepath,
               error_status.full_description.c_str());
    BKE_reportf(params.reports, RPT_ERROR, "Failed to write OTIO file '%s'", params.filepath);
    return;
  }

  BKE_report(params.reports, RPT_INFO, "File exported successfully");

  if (ctx.unsupported_scene_strips > 0) {
    BKE_reportf(params.reports,
                RPT_WARNING,
                "%d scene strip(s) found, this is currently unsupported",
                ctx.unsupported_scene_strips);
  }
  if (ctx.unsupported_image_sequences > 0) {
    BKE_reportf(params.reports,
                RPT_WARNING,
                "%d image sequence(s) with non-sequential filenames found, this is currently "
                "unsupported",
                ctx.unsupported_image_sequences);
  }
}

}  // namespace blender::io::otio
