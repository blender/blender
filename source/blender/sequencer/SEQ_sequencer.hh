/* SPDX-FileCopyrightText: 2004-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BKE_sound_types.hh"
#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_session_uid_types.h"

namespace blender {

struct BlendDataReader;
struct BlendWriter;
struct Depsgraph;
struct Editing;
struct Main;
struct MetaStack;
struct MovieReader;
struct Scene;
struct SeqTimelineChannel;
struct Strip;
struct SequencerToolSettings;

namespace seq {

constexpr int MAX_CHANNELS = 128;

/* RNA enums, just to be more readable */
enum {
  SIDE_MOUSE = -1,
  SIDE_NONE = 0,
  SIDE_LEFT,
  SIDE_RIGHT,
  SIDE_BOTH,
  SIDE_NO_CHANGE,
};

/* strip_duplicate' flags */
enum class StripDuplicate : uint8_t {
  /* Note: Technically, the selected strips are duplicated when `All` is not set. */
  Selected = 0,
  /* Ensure strips have a unique name. */
  UniqueName = (1 << 0),
  /* Duplicate strips and the IDs they reference. */
  Data = (1 << 1),
  /* If this is set, duplicate all strips. If not set, duplicate selected strips. */
  All = (1 << 3),
};
ENUM_OPERATORS(StripDuplicate);

enum class StripRuntimeFlag {
  None = 0,
  ClampedLH = (1 << 0),
  ClampedRH = (1 << 1),
  Overlap = (1 << 2),
  MarkForDelete = (1 << 4),
  IgnoreChannelLock = (1 << 5), /* For #SEQUENCER_OT_duplicate_move macro. */
  ShowOffsets = (1 << 6),       /* Set during #SEQUENCER_OT_slip. */
};
ENUM_OPERATORS(StripRuntimeFlag);

struct StripRuntime {
  ~StripRuntime();

  SessionUID session_uid = {};
  StripRuntimeFlag flag = StripRuntimeFlag::None;
  AUD_SequenceEntry scene_sound;
  AUD_Sound sound_time_stretch;
  float sound_time_stretch_fps = 0.0f;

  Vector<MovieReader *, 1> movie_readers;
  /* To detect the removal of a sound modifier. */
  int sound_modifiers_count = 0;

  [[nodiscard]] MovieReader *movie_reader_get(int64_t index = 0) const
  {
    if (index < 0 || index >= movie_readers.size()) {
      return nullptr;
    }
    return movie_readers[index];
  }

  void clear_sound_time_stretch();
  void remove_scene_sound(Scene *scene);
};

SequencerToolSettings *tool_settings_init();
SequencerToolSettings *tool_settings_ensure(Scene *scene);
void tool_settings_free(SequencerToolSettings *tool_settings);
eSeqImageFitMethod tool_settings_fit_method_get(Scene *scene);
void tool_settings_fit_method_set(Scene *scene, eSeqImageFitMethod fit_method);
short tool_settings_snap_flag_get(Scene *scene);
short tool_settings_snap_mode_get(Scene *scene);
int tool_settings_snap_distance_get(Scene *scene);
eSeqOverlapMode tool_settings_overlap_mode_get(Scene *scene);
int tool_settings_pivot_point_get(Scene *scene);
SequencerToolSettings *tool_settings_copy(SequencerToolSettings *tool_settings);
Editing *editing_get(const Scene *scene);
Editing *editing_ensure(Scene *scene);
void editing_free(Scene *scene, bool do_id_user);
/**
 * Get seqbase that is being viewed currently. This can be main seqbase or meta strip seqbase
 *
 * \param ed: sequence editor data
 * \return pointer to active seqbase. returns NULL if ed is NULL
 */
ListBaseT<Strip> *active_seqbase_get(const Editing *ed);
Strip *strip_alloc(ListBaseT<Strip> *lb, int timeline_frame, int channel, StripType type);
void strip_free(Scene *scene, Strip *strip);
/**
 * Get #MetaStack that corresponds to current level that is being viewed
 *
 * \return pointer to meta stack
 */
MetaStack *meta_stack_active_get(const Editing *ed);
/**
 * Open Meta strip content for editing.
 *
 * \param ed: sequence editor data
 * \param dst: meta strip or NULL for top level view
 */
void meta_stack_set(const Scene *scene, Strip *dst);
/**
 * Close last Meta strip open for editing.
 *
 * \param ed: sequence editor data
 */
Strip *meta_stack_pop(Editing *ed);
Strip *strip_duplicate_recursive(Main *bmain,
                                 const Scene *scene_src,
                                 Scene *scene_dst,
                                 ListBaseT<Strip> *seqbase_dst,
                                 Strip *strip,
                                 StripDuplicate dupe_flag);
void seqbase_duplicate_recursive(Main *bmain,
                                 const Scene *scene_src,
                                 Scene *scene_dst,
                                 ListBaseT<Strip> *seqbase_dst,
                                 const ListBaseT<Strip> *seqbase_src,
                                 StripDuplicate dupe_flag,
                                 int copy_flag);
bool is_valid_strip_channel(const Strip *strip);

/**
 * Read and Write functions for `.blend` file data.
 */
void blend_write(BlendWriter *writer, ListBaseT<Strip> *seqbase);
void blend_read(BlendDataReader *reader, ListBaseT<Strip> *seqbase);

void doversion_250_sound_proxy_update(Main *bmain, Editing *ed);

/* Depsgraph update function. */

/**
 * Evaluate parts of strips which needs to be done as a part of a dependency graph evaluation.
 * This does NOT include actual rendering of the strips, but rather makes them up-to-date for
 * animation playback and makes them ready for the sequencer's rendering pipeline to render them.
 */
void eval_strips(Depsgraph *depsgraph, Scene *scene, ListBaseT<Strip> *seqbase);

/**
 * Find a strip with a given name.
 * If lookup hash doesn't exist, it will be created. If hash is tagged as invalid, it will be
 * rebuilt.
 *
 * \param ed: Editing that owns lookup hash
 * \param key: Strip name without SQ prefix (strip->name + 2)
 *
 * \return pointer to Strip
 */
Strip *lookup_strip_by_name(Editing *ed, const char *key);

/**
 * Find a strips using provided scene as input
 *
 * \param ed: Editing that owns lookup hash
 * \param key: Input Scene pointer
 *
 * \return Span of strips
 */
Span<Strip *> lookup_strips_by_scene(Editing *ed, const Scene *key);

/**
 * Returns Map of scenes to scene strips
 *
 * \param ed: Editing that owns lookup hash
 */
Map<const Scene *, VectorSet<Strip *>> &lookup_strips_by_scene_map_get(Editing *ed);

/**
 * Find all strips using provided compositor node tree as a modifier
 *
 * \param ed: Editing that owns lookup hash
 * \param key: Node tree pointer
 *
 * \return Span of strips
 */
Span<Strip *> lookup_strips_by_compositor_node_group(Editing *ed, const bNodeTree *key);

/**
 * Find which meta strip the given timeline channel belongs to. Returns nullptr if it is a global
 * channel.
 */
Strip *lookup_strip_by_channel_owner(Editing *ed, const SeqTimelineChannel *channel);
/**
 * Find meta strip, that contains strip `key`.
 * If lookup hash doesn't exist, it will be created. If hash is tagged as invalid, it will be
 * rebuilt.
 *
 * \param key: pointer to Strip inside of meta strip
 *
 * \return pointer to meta strip
 */
Strip *lookup_meta_by_strip(Editing *ed, const Strip *key);
/**
 * Free lookup hash data.
 */
void strip_lookup_free(Editing *ed);

/**
 * Mark strip lookup as invalid (i.e. will need rebuilding).
 */
void strip_lookup_invalidate(const Editing *ed);

}  // namespace seq
}  // namespace blender
