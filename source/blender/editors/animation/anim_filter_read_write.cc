/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>

#include "ED_anim_filter_read_write.hh"

#include "BLI_listbase.hh"
#include "BLI_resource_scope.hh"

#include "BKE_lib_id.hh"
#include "BKE_main_idmap.hh"

#include "ED_anim_filter.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"anim.animfilter_io"};

namespace blender::ed::animfilter {

void channel_state_trim(ChannelUIStateMap &channel_states)
{
  /* It may be tempting to use some dynamic limit, like "3x the number of channels shown", but this
   * has to cover historical use as well. When Only Show Selected is enabled, and an animator is
   * animating bone-by-bone, it means the number of channels visible at any one time is limited,
   * while the history of channels should be kept for significantly longer.
   *
   * This value should be tweaked based on animator experience (probably larger is better, but
   * auto-expand/collapse can make explicit storage less important) vs. performance (smaller is
   * better). */
  constexpr int N = 1000;

  if (channel_states.size() <= N) {
    return;
  }

  Vector<std::pair<ChannelIdentifier, int>> key_last_used;
  key_last_used.reserve(channel_states.size());
  BLI_assert(key_last_used.size() > N);
  for (const ChannelUIStateMap::Item item : channel_states.items()) {
    key_last_used.append({item.key, item.value.last_used});
  }

  /* Sort by last_used. */
  std::ranges::sort(
      key_last_used,
      [&](const std::pair<ChannelIdentifier, int> &a, const std::pair<ChannelIdentifier, int> &b) {
        return a.second < b.second;
      });

  /* Erase the oldest items (those beyond the first N). */
  for (int i = N; i < static_cast<int>(key_last_used.size()); i++) {
    channel_states.remove(key_last_used[i].first);
  }
}

/**
 * Convert the ChannelIdentifier to a SpaceChannelDNAIdentifier, and write to a blend file.
 *
 * \returns true when ok, false when any of the identifier's session_uids could not be resolved to
 * an ID*.
 */
static bool write_channel_identifier_data(BlendWriter &writer,
                                          SpaceChannelDNAIdentifier &dna_ident,
                                          std::optional<ChannelIdentifier> chan_ident,
                                          std::string &resource_scope_storage)
{
  /* Make sure the DNA data is clean by default. */
  dna_ident.channel_owner_id = nullptr;
  dna_ident.channel_parent_id = nullptr;
  dna_ident.source_rna_path = nullptr;

  if (!chan_ident) {
    return true;
  }

  IDNameLib_Map *id_map = writer.idmap();
  ID *owner_id = BKE_main_idmap_lookup_uid(id_map, chan_ident->owner_session_uid);
  if (!owner_id && chan_ident->owner_session_uid) {
    /* Stale identifier, don't write this. */
    return false;
  }

  ID *parent_id = BKE_main_idmap_lookup_uid(id_map, chan_ident->parent_session_uid);
  if (!parent_id && chan_ident->parent_session_uid) {
    /* Stale identifier, don't write this. */
    return false;
  }

  dna_ident.channel_owner_id = owner_id;
  dna_ident.channel_parent_id = parent_id;

  if (!chan_ident->source_rna_path.items.is_empty()) {
    resource_scope_storage = chan_ident->source_rna_path.to_string();
    dna_ident.source_rna_path = resource_scope_storage.c_str();
  }

  writer.write_string(dna_ident.source_rna_path);
  return true;
}

/**
 * Convert the given `channel_states` to a listbase and write them into a blendfile.
 * \param channel_dna_states has to come from the shallow copy of a `BlendStructWriter`.
 */
static void write_channel_states(BlendWriter &writer,
                                 Array<SpaceChannelDNAState> &ui_state_array,
                                 Array<std::string> &rna_paths,
                                 const ChannelUIStateMap &channel_states,
                                 ListBaseT<SpaceChannelDNAState> &channel_ui_states)
{
  BLI_assert_msg(channel_ui_states.is_empty(),
                 "Previous writes should have cleaned up after themselves");
  IDNameLib_Map *id_map = writer.idmap();
  BLI_assert_msg(id_map, "Write without bmain, writing UI state will not be possible");
  /* Construct the SpaceAction::channel_ui_states listbase. */
  int64_t array_index = 0;
  for (const auto key_value : channel_states.items()) {
    SpaceChannelDNAState *state = &ui_state_array[array_index];

    /* Store the UI state map key. */
    if (!write_channel_identifier_data(
            writer, state->identifier, key_value.key, rna_paths[array_index]))
    {
      /* Session UID lookup failed, stale entry, skip it. */
      continue;
    }

    /* Store the UI state map value. */
    state->is_collapsed = key_value.value.is_collapsed;
    state->last_used = key_value.value.last_used;

    BLI_addtail(&channel_ui_states, state);
    array_index++;
  }

  /* Write the structs in the listbase. */
  for (SpaceChannelDNAState &state : channel_ui_states) {
    writer.write_struct(&state);
  }
}

void write_space_runtime_data(struct BlendWriter &writer,
                              ResourceScope &scope,
                              const AnimfilterSpaceRuntime &animfilter_runtime,
                              ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                              SpaceChannelDNAIdentifier &active_channel_identifier)
{
  /* Allocate memory for the UI state info in DNA. This is using a ResourceScope to prevent pointer
   * reuse (which would corrupt the blend file). */
  const int64_t num_channel_states = animfilter_runtime.channel_states.size();
  Array<SpaceChannelDNAState> &ui_state_array = scope.construct<Array<SpaceChannelDNAState>>(
      num_channel_states);
  /* Store RNA paths for the channel states + the active channel identifier. */
  Array<std::string> &rna_paths = scope.construct<Array<std::string>>(num_channel_states + 1);

  /* Convert the channel UI states to DNA. */
  write_channel_states(
      writer, ui_state_array, rna_paths, animfilter_runtime.channel_states, channel_ui_states);

  /* Store the active channel identifier to DNA. */
  if (animfilter_runtime.channel_arena) {
    write_channel_identifier_data(writer,
                                  active_channel_identifier,
                                  animfilter_runtime.channel_arena->active_channel_identifier(),
                                  rna_paths[rna_paths.size() - 1]);
  }
  else {
    write_channel_identifier_data(
        writer, active_channel_identifier, {}, rna_paths[rna_paths.size() - 1]);
  }
}

static void read_channel_identifier_data(BlendDataReader &reader,
                                         SpaceChannelDNAIdentifier &chan_ident)
{
  BLO_read_string(&reader, &chan_ident.source_rna_path);
}

static void read_channel_identifier_data_after_liblink(SpaceChannelDNAIdentifier &dna_ident,
                                                       ChannelIdentifier &chan_ident)
{
  if (dna_ident.channel_owner_id) {
    BLI_assert_msg(
        !(dna_ident.channel_owner_id->flag & ID_FLAG_EMBEDDED_DATA),
        "ed::animfilter::ChannelIdentifier can only work on real IDs, not embedded ones");
    BKE_lib_libblock_session_uid_ensure(dna_ident.channel_owner_id);
    chan_ident.owner_session_uid = dna_ident.channel_owner_id->session_uid;
  }
  if (dna_ident.channel_parent_id) {
    if (dna_ident.channel_parent_id->flag & ID_FLAG_EMBEDDED_DATA) {
      CLOG_WARN(
          &LOG,
          "ed::animfilter::ChannelIdentifier can only work on real IDs, not embedded ones (%s)",
          dna_ident.channel_parent_id->name);
    }
    else {
      BKE_lib_libblock_session_uid_ensure(dna_ident.channel_parent_id);
      chan_ident.parent_session_uid = dna_ident.channel_parent_id->session_uid;
    }
  }

  if (dna_ident.source_rna_path) {
    chan_ident.source_rna_path =
        ParsedSourcePath::from_string(dna_ident.source_rna_path).value_or(ParsedSourcePath());
  }

  /* Clean up the data so that there are no more dangling pointers.. */
  dna_ident.channel_owner_id = nullptr;
  dna_ident.channel_parent_id = nullptr;
  MEM_SAFE_DELETE(dna_ident.source_rna_path);
}

static void read_channel_states_data(BlendDataReader &reader,
                                     ListBaseT<SpaceChannelDNAState> &channel_dna_states)
{
  /* Read the UI state. This has to be converted to runtime data with
   * #read_channel_states_after_liblink() below. */
  BLO_read_struct_list(&reader, SpaceChannelDNAState, &channel_dna_states);
  for (SpaceChannelDNAState &state : channel_dna_states) {
    read_channel_identifier_data(reader, state.identifier);
  }
}

static void read_channel_states_after_liblink(BlendLibReader &reader,
                                              ListBaseT<SpaceChannelDNAState> &channel_dna_states,
                                              ChannelUIStateMap &channel_states)
{
  if (BLI_listbase_is_empty(&channel_dna_states)) {
    return;
  }

  if (G.fileflags & G_FILE_NO_UI) {
    /* The UI state should not be loaded, but still the UI state should be freed to avoid leaking
     * memory. */
    for (SpaceChannelDNAState &state : channel_dna_states) {
      MEM_SAFE_DELETE(state.identifier.source_rna_path);
    }
    BLI_freelistN(&channel_dna_states);
    return;
  }

  bool has_shown_warning_header = false;
  for (SpaceChannelDNAState &state : channel_dna_states) {
    /* Create the UI state map key. */
    ChannelIdentifier chan_ident;
    read_channel_identifier_data_after_liblink(state.identifier, chan_ident);

    /* Create the UI state map value. */
    const AnimationChannelUIState ui_state{
        bool(state.is_collapsed),
        state.last_used,
    };

    /* The original map ensured uniqueness, but the ListBase written to DNA does not. So better to
     * use add() rather than add_new(). Also newer Blenders may write more information than what is
     * loaded here, and that missing data may cause channel identifier collissions, so better
     * handle those without crashing. */
    if (!channel_states.add(chan_ident, ui_state)) {
      if (!has_shown_warning_header) {
        CLOG_WARN(&LOG, "reading animation channel UI state:");
        has_shown_warning_header = true;
      }
      std::stringstream ss;
      chan_ident.stream_to(ss, reader.main);
      CLOG_WARN(&LOG, "  - duplicate channel %s found", ss.str().c_str());
    }

    /* Discard the RNA path, as BLI_freelistN() below won't do that. */
    MEM_SAFE_DELETE(state.identifier.source_rna_path);
  }
  BLI_freelistN(&channel_dna_states);
}

void read_space_runtime_data(BlendDataReader &reader,
                             ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                             SpaceChannelDNAIdentifier &active_channel_identifier)
{
  read_channel_states_data(reader, channel_ui_states);
  read_channel_identifier_data(reader, active_channel_identifier);
}

void read_space_runtime_data_after_liblink(BlendLibReader &reader,
                                           AnimfilterSpaceRuntime &animfilter_runtime,
                                           ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                                           SpaceChannelDNAIdentifier &active_channel_identifier)
{
  read_channel_states_after_liblink(reader, channel_ui_states, animfilter_runtime.channel_states);

  ChannelIdentifier active_channel_ident;
  read_channel_identifier_data_after_liblink(active_channel_identifier, active_channel_ident);
  if (active_channel_ident != ChannelIdentifier{}) {
    animfilter_runtime.active_channel_ident = active_channel_ident;
  }
}

}  // namespace blender::ed::animfilter
