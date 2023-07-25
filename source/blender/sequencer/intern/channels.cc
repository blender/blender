/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"

#include "SEQ_channels.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"

ListBase *SEQ_channels_displayed_get(Editing *ed)
{
  return ed->displayed_channels;
}

void SEQ_channels_displayed_set(Editing *ed, ListBase *channels)
{
  ed->displayed_channels = channels;
}

void SEQ_channels_ensure(ListBase *channels)
{
  /* Allocate channels. Channel 0 is never used, but allocated to prevent off by 1 issues. */
  for (int i = 0; i < MAXSEQ + 1; i++) {
    SeqTimelineChannel *channel = static_cast<SeqTimelineChannel *>(
        MEM_callocN(sizeof(SeqTimelineChannel), "seq timeline channel"));
    SNPRINTF(channel->name, "Channel %d", i);
    channel->index = i;
    BLI_addtail(channels, channel);
  }
}

void SEQ_channels_duplicate(ListBase *channels_dst, ListBase *channels_src)
{
  LISTBASE_FOREACH (SeqTimelineChannel *, channel, channels_src) {
    SeqTimelineChannel *channel_duplicate = static_cast<SeqTimelineChannel *>(
        MEM_dupallocN(channel));
    BLI_addtail(channels_dst, channel_duplicate);
  }
}

void SEQ_channels_free(ListBase *channels)
{
  LISTBASE_FOREACH_MUTABLE (SeqTimelineChannel *, channel, channels) {
    MEM_freeN(channel);
  }
}

SeqTimelineChannel *SEQ_channel_get_by_index(const ListBase *channels, const int channel_index)
{
  return static_cast<SeqTimelineChannel *>(BLI_findlink(channels, channel_index));
}

char *SEQ_channel_name_get(ListBase *channels, const int channel_index)
{
  SeqTimelineChannel *channel = SEQ_channel_get_by_index(channels, channel_index);
  return channel->name;
}

int SEQ_channel_index_get(const SeqTimelineChannel *channel)
{
  return channel->index;
}

bool SEQ_channel_is_locked(const SeqTimelineChannel *channel)
{
  return (channel->flag & SEQ_CHANNEL_LOCK) != 0;
}

bool SEQ_channel_is_muted(const SeqTimelineChannel *channel)
{
  return (channel->flag & SEQ_CHANNEL_MUTE) != 0;
}

ListBase *SEQ_get_channels_by_seq(ListBase *seqbase, ListBase *channels, const Sequence *seq)
{
  ListBase *lb = nullptr;

  LISTBASE_FOREACH (Sequence *, iseq, seqbase) {
    if (seq == iseq) {
      return channels;
    }
    if ((lb = SEQ_get_channels_by_seq(&iseq->seqbase, &iseq->channels, seq))) {
      return lb;
    }
  }

  return nullptr;
}
