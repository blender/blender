/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <memory>
#include <optional>

#include "BLI_string_ref.hh"
#include "UI_resources.h"

struct ListBase;
struct SpaceOutliner;

namespace blender::ed::outliner {

struct TreeElement;

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

  /**
   * Just while transitioning to the new tree-element design: Some types are only partially ported,
   * and the expanding isn't done yet.
   */
  virtual bool isExpandValid() const
  {
    return true;
  }

  TreeElement &getLegacyElement()
  {
    return legacy_te_;
  }

  /**
   * By letting this return a warning message, the tree element will display a warning icon with
   * the message in the tooltip.
   */
  virtual StringRefNull getWarning() const;

  /**
   * Define the icon to be displayed for this element. If this returns an icon, this will be
   * displayed. Otherwise, #tree_element_get_icon() may still determine an icon. By default no
   * value is returned (#std::nullopt).
   *
   * All elements should be ported to use this over #tree_element_get_icon().
   */
  virtual std::optional<BIFIconID> getIcon() const;

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
  virtual void expand(SpaceOutliner &) const {}
};

/**
 * TODO: this function needs to be split up! It's getting a bit too large...
 *
 * \note "ID" is not always a real ID.
 * \note If child items are only added to the tree if the item is open,
 * the `TSE_` type _must_ be added to #outliner_element_needs_rebuild_on_open_change().
 *
 * \param expand: If true, the element may add its own sub-tree. E.g. objects will list their
 *                animation data, object data, constraints, modifiers, ... This often adds visual
 *                noise, and can be expensive to add in big scenes. So prefer setting this to
 *                false.
 */
struct TreeElement *outliner_add_element(SpaceOutliner *space_outliner,
                                         ListBase *lb,
                                         void *idv,
                                         struct TreeElement *parent,
                                         short type,
                                         short index,
                                         const bool expand = true);

void tree_element_expand(const AbstractTreeElement &tree_element, SpaceOutliner &space_outliner);

}  // namespace blender::ed::outliner
