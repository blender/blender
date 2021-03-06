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

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementID : public AbstractTreeElement {
 public:
  TreeElementID(TreeElement &legacy_te, const ID &id);

  static TreeElementID *createFromID(TreeElement &legacy_te, const ID &id);

  /**
   * Expanding not implemented for all types yet. Once it is, this can be set to true or
   * `AbstractTreeElement::expandValid()` can be removed altogether.
   */
  bool isExpandValid() const override
  {
    return false;
  }
};

class TreeElementIDLibrary final : public TreeElementID {
 public:
  TreeElementIDLibrary(TreeElement &legacy_te, const ID &id);
};

}  // namespace blender::ed::outliner
