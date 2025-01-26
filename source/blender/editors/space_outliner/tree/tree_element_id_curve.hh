/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

struct Curve;

namespace blender::ed::outliner {

class TreeElementIDCurve final : public TreeElementID {
  Curve &curve_;

 public:
  TreeElementIDCurve(TreeElement &legacy_te, Curve &curve);

  void expand(SpaceOutliner & /*soops*/) const override;

 private:
  void expand_materials() const;
};

}  // namespace blender::ed::outliner
