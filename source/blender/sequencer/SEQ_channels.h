/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Editing;
struct ListBase;
struct SeqTimelineChannel;
struct Sequence;

struct ListBase *SEQ_channels_displayed_get(struct Editing *ed);
void SEQ_channels_displayed_set(struct Editing *ed, struct ListBase *channels);
void SEQ_channels_ensure(struct ListBase *channels);
void SEQ_channels_duplicate(struct ListBase *channels_dst, struct ListBase *channels_src);
void SEQ_channels_free(struct ListBase *channels);

struct SeqTimelineChannel *SEQ_channel_get_by_index(const struct ListBase *channels,
                                                    int channel_index);
char *SEQ_channel_name_get(struct ListBase *channels, int channel_index);
bool SEQ_channel_is_locked(const struct SeqTimelineChannel *channel);
bool SEQ_channel_is_muted(const struct SeqTimelineChannel *channel);
int SEQ_channel_index_get(const struct SeqTimelineChannel *channel);
ListBase *SEQ_get_channels_by_seq(struct ListBase *seqbase,
                                  struct ListBase *channels,
                                  const struct Sequence *seq);

#ifdef __cplusplus
}
#endif
