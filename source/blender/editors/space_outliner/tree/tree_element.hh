/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.h"

namespace blender::ed::outliner {

/* -------------------------------------------------------------------- */
/* Tree-Display Interface */

class AbstractTreeElement {
 protected:
  /**
   * Reference back to the owning legacy TreeElement.
   * Most concrete types need access to this, so storing here. Eventually the type should be
   * replaced by AbstractTreeElement and derived types.
   */
  TreeElement &legacy_te_;

 public:
  virtual ~AbstractTreeElement() = default;

  /**
   * Check if the type is expandable in current context.
   */
  virtual bool expandPoll(const SpaceOutliner &) const
  {
    return true;
  }
  /**
   * Let the type add its own children.
   */
  virtual void expand(SpaceOutliner &) const
  {
  }
  virtual void postExpand(SpaceOutliner &) const
  {
  }

  /**
   * Just while transitioning to the new tree-element design: Some types are only partially ported,
   * and the expanding isn't done yet.
   */
  virtual bool isExpandValid() const
  {
    return true;
  }

 protected:
  /* Pseudo-abstract: Only allow creation through derived types. */
  AbstractTreeElement(TreeElement &legacy_te) : legacy_te_(legacy_te)
  {
  }
};

}  // namespace blender::ed::outliner
