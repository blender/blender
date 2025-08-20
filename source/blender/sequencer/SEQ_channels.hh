/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Editing;
struct ListBase;
struct SeqTimelineChannel;
struct Strip;

namespace blender::seq {

ListBase *channels_displayed_get(const Editing *ed);
void channels_displayed_set(Editing *ed, ListBase *channels);
void channels_ensure(ListBase *channels);
void channels_duplicate(ListBase *channels_dst, ListBase *channels_src);
void channels_free(ListBase *channels);

/**
 * Returns SeqTimelineChannel by index
 * Note: `Strip::channel` and `SeqTimelineChannel::index` are both counted from 0, but index of 0
 * is never used. Therefore, it is valid to call `SeqTimelineChannel(channels, strip->channel)` to
 * get channel corresponding to strip position.
 */
SeqTimelineChannel *channel_get_by_index(const ListBase *channels, int channel_index);
char *channel_name_get(ListBase *channels, int channel_index);
bool channel_is_locked(const SeqTimelineChannel *channel);
bool channel_is_muted(const SeqTimelineChannel *channel);
int channel_index_get(const SeqTimelineChannel *channel);
ListBase *get_channels_by_strip(Editing *ed, const Strip *strip);

}  // namespace blender::seq
