/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

namespace blender::bke {

/**
 * A set of anonymous attribute names that is passed around in geometry nodes.
 */
class AnonymousAttributeSet {
 public:
  /**
   * This uses `std::shared_ptr` because attributes sets are passed around by value during geometry
   * nodes evaluation, and this makes it very small if there is no name. Also it makes copying very
   * cheap.
   */
  std::shared_ptr<Set<std::string>> names;
};

/**
 * Can be passed to algorithms which propagate attributes. It can tell the algorithm which
 * anonymous attributes should be propagated and can be skipped.
 */
class AnonymousAttributePropagationInfo {
 public:
  /**
   * This uses `std::shared_ptr` because it's usually initialized from an #AnonymousAttributeSet
   * and then the set doesn't have to be copied.
   */
  std::shared_ptr<Set<std::string>> names;

  /**
   * Propagate all anonymous attributes even if the set above is empty.
   */
  bool propagate_all = true;

  /**
   * Return true when the anonymous attribute should be propagated and false otherwise.
   */
  bool propagate(StringRef anonymous_id) const;
};

}  // namespace blender::bke
