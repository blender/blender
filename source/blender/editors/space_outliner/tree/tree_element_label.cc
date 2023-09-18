/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_listBase.h"

#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_label.hh"

namespace blender::ed::outliner {

TreeElementLabel::TreeElementLabel(TreeElement &legacy_te, const char *label)
    : AbstractTreeElement(legacy_te), label_(label)
{
  BLI_assert(legacy_te_.store_elem->type == TSE_GENERIC_LABEL);
  /* The draw string is actually accessed via #TreeElement.name, so make sure this always points to
   * our string. */
  legacy_te_.name = label_.c_str();
}

void TreeElementLabel::set_icon(const BIFIconID icon)
{
  icon_ = icon;
}

std::optional<BIFIconID> TreeElementLabel::get_icon() const
{
  return icon_;
}

}  // namespace blender::ed::outliner
