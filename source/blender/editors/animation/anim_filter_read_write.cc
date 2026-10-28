/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>

#include "BLI_listbase.hh"

#include "BLO_read_write.hh"

#include "BKE_lib_id.hh"
#include "BKE_main_idmap.hh"

#include "ED_anim_filter.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"anim.animfilter_io"};

namespace blender::ed::animfilter {

AnimationChannelUIState &AnimfilterSpaceRuntime::channel_state_for_update(
    const ChannelIdentifier &channel_key)
{
  AnimationChannelUIState &channel_state = this->channel_states.lookup_or_add_default(channel_key);
  channel_state.last_used = this->last_used;
  return channel_state;
}

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

void write_channel_states(BlendWriter &writer,
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

    ID *owner_id = BKE_main_idmap_lookup_uid(id_map, key_value.key.owner_session_uid);
    ID *parent_id = BKE_main_idmap_lookup_uid(id_map, key_value.key.parent_session_uid);

    if (!owner_id) {
      /* Stale entry, skip it. */
      continue;
    }

    /* Store the UI state map key. */
    state->channel_owner_id = owner_id;
    state->channel_parent_id = parent_id;
    if (!key_value.key.source_rna_path.items.is_empty()) {
      rna_paths[array_index] = key_value.key.source_rna_path.to_string();
      state->source_rna_path = rna_paths[array_index].c_str();
    }
    else {
      state->source_rna_path = nullptr;
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
    if (state.source_rna_path) {
      writer.write_string(state.source_rna_path);
    }
  }
}

void read_channel_states_data(BlendDataReader &reader,
                              ListBaseT<SpaceChannelDNAState> &channel_dna_states)
{
  /* Read the UI state. This has to be converted to runtime data with
   * #read_channel_states_after_liblink() below. */
  BLO_read_struct_list(&reader, SpaceChannelDNAState, &channel_dna_states);
  for (SpaceChannelDNAState &state : channel_dna_states) {
    BLO_read_string(&reader, &state.source_rna_path);
  }
}

void read_channel_states_after_liblink(BlendLibReader &reader,
                                       ListBaseT<SpaceChannelDNAState> &channel_dna_states,
                                       ChannelUIStateMap &channel_states)
{
  if (BLI_listbase_is_empty(&channel_dna_states)) {
    return;
  }

  if (G.fileflags & G_FILE_NO_UI) {
    /* The UI state should not be loaded, but still the UI state should be freed to avoid leaking
     * memory. */
    /* TODO does this not have to call `MEM_SAFE_DELETE` on `state.source_rna_path`. */
    BLI_freelistN(&channel_dna_states);
    return;
  }

  bool has_shown_warning_header = false;
  for (SpaceChannelDNAState &state : channel_dna_states) {
    /* Create the UI state map key. */
    ChannelIdentifier chan_ident;
    if (state.channel_owner_id) {
      BLI_assert_msg(
          !(state.channel_owner_id->flag & ID_FLAG_EMBEDDED_DATA),
          "ed::animfilter::ChannelIdentifier can only work on real IDs, not embedded ones");
      BKE_lib_libblock_session_uid_ensure(state.channel_owner_id);
      chan_ident.owner_session_uid = state.channel_owner_id->session_uid;
    }
    if (state.channel_parent_id) {
      if (state.channel_parent_id->flag & ID_FLAG_EMBEDDED_DATA) {
        CLOG_WARN(
            &LOG,
            "ed::animfilter::ChannelIdentifier can only work on real IDs, not embedded ones (%s)",
            state.channel_parent_id->name);
      }
      else {
        BKE_lib_libblock_session_uid_ensure(state.channel_parent_id);
        chan_ident.parent_session_uid = state.channel_parent_id->session_uid;
      }
    }

    if (state.source_rna_path) {
      chan_ident.source_rna_path =
          ParsedSourcePath::from_string(state.source_rna_path).value_or(ParsedSourcePath());
    }

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
    MEM_SAFE_DELETE(state.source_rna_path);
  }
  BLI_freelistN(&channel_dna_states);
}

}  // namespace blender::ed::animfilter
