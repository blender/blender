/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender {

struct Library;

namespace ed::outliner {

class TreeElementIDLibrary final : public TreeElementID {
 public:
  TreeElementIDLibrary(TreeElement &legacy_te, Library &library);

  StringRefNull get_warning() const override;
};

}  // namespace ed::outliner
}  // namespace blender
