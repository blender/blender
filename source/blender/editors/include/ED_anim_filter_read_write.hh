/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */
#pragma once

#include "BLI_listbase.hh"

#include "BLO_read_write.hh"

namespace blender {
class ResourceScope;
struct SpaceChannelDNAIdentifier;
struct SpaceChannelDNAState;
}  // namespace blender

namespace blender::ed::animfilter {

class ChannelIdentifier;
struct AnimfilterSpaceRuntime;

/**
 * Convert the animfilter runtime space data into DNA data for writing to a blend file.
 */
void write_space_runtime_data(BlendWriter &writer,
                              ResourceScope &scope,
                              const AnimfilterSpaceRuntime &animfilter_runtime,
                              ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                              SpaceChannelDNAIdentifier &active_channel_identifier);
/**
 * Convert DNA fields into the animfilter runtime space data.
 */
void read_space_runtime_data(BlendDataReader &reader,
                             ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                             SpaceChannelDNAIdentifier &active_channel_identifier);
void read_space_runtime_data_after_liblink(BlendLibReader &reader,
                                           AnimfilterSpaceRuntime &animfilter_runtime,
                                           ListBaseT<SpaceChannelDNAState> &channel_ui_states,
                                           SpaceChannelDNAIdentifier &active_channel_identifier);

}  // namespace blender::ed::animfilter
