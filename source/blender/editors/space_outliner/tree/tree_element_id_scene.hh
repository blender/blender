/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

namespace blender {

struct Scene;

namespace ed::outliner {

class TreeElementIDScene final : public TreeElementID {
  Scene &scene_;

 public:
  TreeElementIDScene(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner & /*soops*/) const override;

 private:
  void expand_view_layers() const;
  void expand_world() const;
  void expand_collections() const;
  void expand_objects() const;
};

}  // namespace ed::outliner
}  // namespace blender
