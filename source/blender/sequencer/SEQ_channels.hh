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

ListBase *SEQ_channels_displayed_get(Editing *ed);
void SEQ_channels_displayed_set(Editing *ed, ListBase *channels);
void SEQ_channels_ensure(ListBase *channels);
void SEQ_channels_duplicate(ListBase *channels_dst, ListBase *channels_src);
void SEQ_channels_free(ListBase *channels);

SeqTimelineChannel *SEQ_channel_get_by_index(const ListBase *channels, int channel_index);
char *SEQ_channel_name_get(ListBase *channels, int channel_index);
bool SEQ_channel_is_locked(const SeqTimelineChannel *channel);
bool SEQ_channel_is_muted(const SeqTimelineChannel *channel);
int SEQ_channel_index_get(const SeqTimelineChannel *channel);
ListBase *SEQ_get_channels_by_seq(ListBase *seqbase, ListBase *channels, const Strip *strip);
