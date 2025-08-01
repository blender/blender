/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Strip;

namespace blender::seq {

bool modifier_persistent_uids_are_valid(const Strip &strip);

}  // namespace blender::seq
