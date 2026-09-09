/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <memory>
#include <optional>

#include "BLI_string_ref.hh"
#include "DNA_listBase.h"
#include "UI_resources.hh"

namespace blender {

struct ID;
struct SpaceOutliner;

namespace ed::outliner {

class AbstractTreeDisplay;
struct TreeElement;
struct TreeElementAddParams;

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
  /**
   * Reference back to the tree display used for building this tree.
   */
  AbstractTreeDisplay *display_;

  friend class AbstractTreeDisplay;

 public:
  virtual ~AbstractTreeElement() = default;

  /**
   * Check if the type is expandable in current context.
   */
  virtual bool expand_poll(const SpaceOutliner & /*soops*/) const
  {
    return true;
  }

  TreeElement &get_legacy_element()
  {
    return legacy_te_;
  }

  /**
   * By letting this return a warning message, the tree element will display a warning icon with
   * the message in the tooltip.
   */
  virtual StringRefNull get_warning() const;

  /**
   * Define the icon to be displayed for this element. If this returns an icon, this will be
   * displayed. Otherwise, #tree_element_get_icon() may still determine an icon. By default no
   * value is returned (#std::nullopt).
   *
   * All elements should be ported to use this over #tree_element_get_icon().
   */
  virtual std::optional<BIFIconID> get_icon() const;

  /**
   * Debugging helper: Print effective path of this tree element, constructed out of the
   * #TreeElement.name of each element. E.g.:
   * - Lorem
   *   - ipsum dolor sit
   *     - amet
   * will print: Lorem/ipsum dolor sit/amet.
   */
  void print_path();

  /**
   * Expand this tree element if it is displayed for the first time (as identified by its
   * tree-store element).
   *
   * Static for now to allow doing this from the legacy tree element.
   */
  static void uncollapse_by_default(TreeElement *legacy_te);

  friend void tree_element_expand(const AbstractTreeElement &tree_element,
                                  SpaceOutliner &space_outliner);

 protected:
  /* Pseudo-abstract: Only allow creation through derived types. */
  AbstractTreeElement(TreeElement &legacy_te) : legacy_te_(legacy_te) {}

  /**
   * Let the type add its own children.
   */
  virtual void expand(SpaceOutliner & /*soops*/) const {}

  /**
   * See #AbstractTreeDisplay::add_element(). Adds a child to this element's own sub-tree by
   * default, so #TreeElementAddParams.lb and #TreeElementAddParams.parent can usually be left
   * unset:
   * \code
   * this->add_element<TreeElementConstraintBase>({}, object_);
   * \endcode
   *
   * \note Defined in `tree_display.hh`, which is where the complete #AbstractTreeDisplay this
   *       forwards to becomes available. Include it to use this.
   */
  template<typename ElementType, typename... Args>
  TreeElement *add_element(const TreeElementAddParams &params, Args &&...args) const;

  /** See #AbstractTreeDisplay::add_id_element() (which this forwards to). */
  TreeElement *add_id_element(const TreeElementAddParams &params, ID *id) const;

 private:
  /**
   * The tree-display to add children through, or null (with an assert) if this element wasn't
   * registered through #AbstractTreeDisplay::add_element(). Fills in \a params defaults that refer
   * to this element, shared by both `add_*_element()` above.
   */
  AbstractTreeDisplay *display_for_adding(TreeElementAddParams &params) const;
};

void tree_element_expand(const AbstractTreeElement &tree_element, SpaceOutliner &space_outliner);

}  // namespace ed::outliner
}  // namespace blender
