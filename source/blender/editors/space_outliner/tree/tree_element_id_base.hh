/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender {

namespace ed::outliner {

struct TreeElementIDBase final : public AbstractTreeElement {

 public:
  static constexpr eTreeStoreElemType element_type = TSE_ID_BASE;
  /**
   * ID base elements are usually identified by a pointer the caller nominates (the #ListBase of
   * the IDs, the #Main, ...), but some are pure placeholders with nothing to identify them by.
   */
  static constexpr bool allow_null_identity = true;

  TreeElementIDBase(TreeElement &legacy_te);
};
}  // namespace ed::outliner
}  // namespace blender
