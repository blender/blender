/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_vector.hh"

namespace blender {

struct bContext;

namespace ed::animfilter {
class DataChannel;
class AnimationChannel;
class SummaryTree;
}  // namespace ed::animfilter

namespace ed::action {

Span<ed::animfilter::DataChannel> current_data_channels();
/**
 * Return the buffered data or build up the tree if required.
 */
ed::animfilter::SummaryTree get_summary_tree(const bContext &C);

}  // namespace ed::action

}  // namespace blender
