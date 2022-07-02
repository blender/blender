/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "interface_intern.h"

#include "UI_abstract_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */
/** \name View Reconstruction
 * \{ */

bool AbstractView::is_reconstructed() const
{
  return is_reconstructed_;
}

void AbstractView::update_from_old(uiBlock &new_block)
{
  uiBlock *old_block = new_block.oldblock;
  if (!old_block) {
    is_reconstructed_ = true;
    return;
  }

  const uiViewHandle *old_view_handle = ui_block_view_find_matching_in_old_block(
      &new_block, reinterpret_cast<uiViewHandle *>(this));
  if (old_view_handle == nullptr) {
    /* Initial construction, nothing to update. */
    is_reconstructed_ = true;
    return;
  }

  update_children_from_old(reinterpret_cast<const AbstractView &>(*old_view_handle));

  /* Finished (re-)constructing the tree. */
  is_reconstructed_ = true;
}

/** \} */

bool AbstractView::listen(const wmNotifier & /*notifier*/) const
{
  /* Nothing by default. */
  return false;
}

}  // namespace blender::ui
