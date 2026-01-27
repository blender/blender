/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "sequencer.hh"

#include "SEQ_channels.hh"
#include "SEQ_sequencer.hh"

namespace blender::seq {

ListBaseT<SeqTimelineChannel> *channels_displayed_get(const Editing *ed)
{
  return ed ? ed->current_channels() : nullptr;
}

void channels_ensure(ListBaseT<SeqTimelineChannel> *channels)
{
  /* Allocate channels. Channel 0 is never used, but allocated to prevent off by 1 issues. */
  for (int i = 0; i < MAX_CHANNELS + 1; i++) {
    SeqTimelineChannel *channel = MEM_new<SeqTimelineChannel>("seq timeline channel");
    SNPRINTF_UTF8(channel->name, DATA_("Channel %d"), i);
    channel->index = i;
    BLI_addtail(channels, channel);
  }
}

void channels_duplicate(ListBaseT<SeqTimelineChannel> *channels_dst,
                        ListBaseT<SeqTimelineChannel> *channels_src)
{
  for (SeqTimelineChannel &channel : *channels_src) {
    SeqTimelineChannel *channel_duplicate = static_cast<SeqTimelineChannel *>(
        MEM_dupalloc(&channel));
    BLI_addtail(channels_dst, channel_duplicate);
  }
}

void channels_free(ListBaseT<SeqTimelineChannel> *channels)
{
  for (SeqTimelineChannel &channel : channels->items_mutable()) {
    MEM_delete(&channel);
  }
}

SeqTimelineChannel *channel_get_by_index(const ListBaseT<SeqTimelineChannel> *channels,
                                         const int channel_index)
{
  return static_cast<SeqTimelineChannel *>(BLI_findlink(channels, channel_index));
}

char *channel_name_get(ListBaseT<SeqTimelineChannel> *channels, const int channel_index)
{
  SeqTimelineChannel *channel = channel_get_by_index(channels, channel_index);
  return channel->name;
}

int channel_index_get(const SeqTimelineChannel *channel)
{
  return channel->index;
}

bool channel_is_locked(const SeqTimelineChannel *channel)
{
  return (channel->flag & SEQ_CHANNEL_LOCK) != 0;
}

bool channel_is_muted(const SeqTimelineChannel *channel)
{
  return (channel->flag & SEQ_CHANNEL_MUTE) != 0;
}

ListBaseT<SeqTimelineChannel> *get_channels_by_strip(Editing *ed, const Strip *strip)
{
  Strip *strip_owner = lookup_meta_by_strip(ed, strip);
  if (strip_owner != nullptr) {
    return &strip_owner->channels;
  }

  return &ed->channels;
}

}  // namespace blender::seq
