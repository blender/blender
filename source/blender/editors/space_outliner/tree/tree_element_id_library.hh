/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct Library;

namespace blender::ed::outliner {

class TreeElementIDLibrary final : public TreeElementID {
 public:
  TreeElementIDLibrary(TreeElement &legacy_te, Library &library);

  bool isExpandValid() const override;

  blender::StringRefNull getWarning() const override;
};

}  // namespace blender::ed::outliner
