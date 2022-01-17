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

#include <memory>

struct ListBase;
struct SpaceOutliner;
struct TreeElement;

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

  static std::unique_ptr<AbstractTreeElement> createFromType(int type,
                                                             TreeElement &legacy_te,
                                                             void *idv);

  /**
   * Check if the type is expandable in current context.
   */
  virtual bool expandPoll(const SpaceOutliner &) const
  {
    return true;
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

  friend void tree_element_expand(const AbstractTreeElement &tree_element,
                                  SpaceOutliner &space_outliner);

 protected:
  /* Pseudo-abstract: Only allow creation through derived types. */
  AbstractTreeElement(TreeElement &legacy_te) : legacy_te_(legacy_te)
  {
  }

  /**
   * Let the type add its own children.
   */
  virtual void expand(SpaceOutliner &) const
  {
  }
};

/**
 * TODO: this function needs to be split up! It's getting a bit too large...
 *
 * \note "ID" is not always a real ID.
 * \note If child items are only added to the tree if the item is open,
 * the `TSE_` type _must_ be added to #outliner_element_needs_rebuild_on_open_change().
 */
struct TreeElement *outliner_add_element(SpaceOutliner *space_outliner,
                                         ListBase *lb,
                                         void *idv,
                                         struct TreeElement *parent,
                                         short type,
                                         short index);

void tree_element_expand(const AbstractTreeElement &tree_element, SpaceOutliner &space_outliner);

/**
 * Get actual warning data of a tree element, if any.
 *
 * \param r_icon The icon to display as warning.
 * \param r_message The message to display as warning.
 * \return true if there is a warning, false otherwise.
 */
bool tree_element_warnings_get(struct TreeElement *te, int *r_icon, const char **r_message);

}  // namespace blender::ed::outliner
