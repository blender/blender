/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Computes and returns a lower resolution byte version of the given input after applying the
 * appropriate color management specified in the given context. */
ImBuf *compute_preview(Context &context, const Result &input);

}  // namespace blender::compositor
