/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <iostream>
#include <string>
#include <string_view>

#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "UI_resources.hh"

#include "tree_display.hh"
#include "tree_element_id.hh"

#include "../outliner_intern.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

StringRefNull AbstractTreeElement::get_warning() const
{
  return "";
}

std::optional<BIFIconID> AbstractTreeElement::get_icon() const
{
  return {};
}

void AbstractTreeElement::print_path()
{
  std::string path = legacy_te_.name;

  for (TreeElement *parent = legacy_te_.parent; parent; parent = parent->parent) {
    path = parent->name + std::string_view("/") + path;
  }

  std::cout << path << std::endl;
}

void AbstractTreeElement::uncollapse_by_default(TreeElement *legacy_te)
{
  if (!TREESTORE(legacy_te)->used) {
    TREESTORE(legacy_te)->flag &= ~TSE_CLOSED;
  }
}

AbstractTreeDisplay *AbstractTreeElement::display_for_adding(TreeElementAddParams &params) const
{
  if (!display_) {
    BLI_assert_msg(false,
                   "Element not registered properly through AbstractTreeDisplay::add_element(), "
                   "cannot expand the tree further");
    return nullptr;
  }

  /* Default to adding a child to this element itself. Note that the sub-tree to add to is derived
   * from the parent by #AbstractTreeDisplay::add_element(), so it doesn't have to be filled in
   * here (which would need a complete #TreeElement in the header). */
  if (!params.parent) {
    params.parent = &legacy_te_;
  }

  return display_;
}

TreeElement *AbstractTreeElement::add_id_element(const TreeElementAddParams &params, ID *id) const
{
  TreeElementAddParams resolved_params = params;
  AbstractTreeDisplay *display = display_for_adding(resolved_params);
  return display ? display->add_id_element(resolved_params, id) : nullptr;
}

void tree_element_expand(const AbstractTreeElement &tree_element, SpaceOutliner &space_outliner)
{
  /* Most types can just expand. IDs optionally expand (hence the poll) and do additional, common
   * expanding. Could be done nicer, we could request a small "expander" helper object from the
   * element type, that the IDs have a more advanced implementation for. */
  if (!tree_element.expand_poll(space_outliner)) {
    return;
  }
  tree_element.expand(space_outliner);
}

}  // namespace blender::ed::outliner
