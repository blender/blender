/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions for backward compatibility with the legacy Action API.
 *
 * It should be possible to remove these functions (and their callers) in
 * Blender 5.0, when we can remove the legacy API altogether.
 */
#pragma once

namespace blender::animrig {
class Action;
class KeyframeStrip;
}  // namespace blender::animrig

namespace blender::animrig::legacy {

/**
 * Return the ChannelBag for compatibility with the legacy Python API.
 *
 * \return the ChannelBag for the first slot, of the first KeyframeStrip on the
 * bottom layer, or nullptr if that doesn't exist.
 */
ChannelBag *channelbag_get(Action &action);

/**
 * Ensure a ChannelBag exists, for compatibility with the legacy Python API.
 *
 * This basically is channelbag_get(action), additionally creating the necessary
 * slot, layer, and keyframe strip if necessary.
 */
ChannelBag &channelbag_ensure(Action &action);

}  // namespace blender::animrig::legacy
