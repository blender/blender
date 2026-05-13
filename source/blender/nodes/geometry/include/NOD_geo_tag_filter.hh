/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_set.hh"
#include "BLI_string_ref.hh"

namespace blender::nodes {

bool tag_filter_matches(const StringRef tag_filter, const Set<std::string> &tags);

}  // namespace blender::nodes
