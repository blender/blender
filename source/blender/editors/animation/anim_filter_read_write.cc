/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.hh"

#include "BLO_read_write.hh"

#include "BKE_main_idmap.hh"

#include "ED_anim_filter.hh"

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
  using namespace ed::animfilter;

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
                                 Array<SpaceChannelUIState> &ui_state_array,
                                 Array<std::string> &rna_paths,
                                 const ed::animfilter::ChannelUIStateMap &channel_states,
                                 ListBaseT<SpaceChannelUIState> &channel_ui_states)
{
  BLI_assert_msg(channel_ui_states.is_empty(),
                 "Previous writes should have cleaned up after themselves");
  IDNameLib_Map *id_map = writer.idmap();
  BLI_assert_msg(id_map, "Write without bmain, writing UI state will not be possible");
  /* Construct the SpaceAction::channel_ui_states listbase. */
  int64_t array_index = 0;
  for (const auto key_value : channel_states.items()) {
    SpaceChannelUIState *state = &ui_state_array[array_index];

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
  for (SpaceChannelUIState &state : channel_ui_states) {
    writer.write_struct(&state);
    if (state.source_rna_path) {
      writer.write_string(state.source_rna_path);
    }
  }
}

}  // namespace blender::ed::animfilter
