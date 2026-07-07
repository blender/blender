/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender {

struct MetaBall;

namespace ed::outliner {

class TreeElementIDMetaBall final : public TreeElementID {
  MetaBall &metaball_;

 public:
  TreeElementIDMetaBall(TreeElement &legacy_te, MetaBall &metaball);

  void expand(SpaceOutliner & /*soops*/) const override;

 private:
  void expand_materials() const;
};

}  // namespace ed::outliner
}  // namespace blender
