/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

namespace blender {

struct Editing;
struct SeqTimelineChannel;
struct Strip;

namespace seq {

/** The active displayed channels list, either from the root sequence or from a meta-strip. */
ListBaseT<SeqTimelineChannel> *channels_displayed_get(const Editing *ed);
void channels_ensure(ListBaseT<SeqTimelineChannel> *channels);
void channels_duplicate(ListBaseT<SeqTimelineChannel> *channels_dst,
                        ListBaseT<SeqTimelineChannel> *channels_src);
void channels_free(ListBaseT<SeqTimelineChannel> *channels);

/**
 * Returns SeqTimelineChannel by index
 * NOTE: `Strip::channel` and `SeqTimelineChannel::index` are both counted from 0, but index of 0
 * is never used. Therefore, it is valid to call `SeqTimelineChannel(channels, strip->channel)` to
 * get channel corresponding to strip position.
 */
SeqTimelineChannel *channel_get_by_index(const ListBaseT<SeqTimelineChannel> *channels,
                                         int channel_index);
char *channel_name_get(ListBaseT<SeqTimelineChannel> *channels, int channel_index);
bool channel_is_locked(const SeqTimelineChannel *channel);
bool channel_is_muted(const SeqTimelineChannel *channel);
int channel_index_get(const SeqTimelineChannel *channel);
ListBaseT<SeqTimelineChannel> *get_channels_by_strip(Editing *ed, const Strip *strip);

}  // namespace seq
}  // namespace blender
