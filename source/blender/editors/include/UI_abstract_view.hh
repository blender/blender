/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * Base for all views (UIs to display data sets), supporting common features.
 * https://wiki.blender.org/wiki/Source/Interface/Views
 *
 * The base class manages reconstruction, most importantly keeping state over reconstructions.
 */

#pragma once

struct wmNotifier;

namespace blender::ui {

class AbstractView {
  bool is_reconstructed_ = false;

 public:
  virtual ~AbstractView() = default;

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

 protected:
  AbstractView() = default;

  virtual void update_children_from_old(const AbstractView &old_view) = 0;

  /**
   * Match the view and its items against an earlier version of itself (if any) and copy the old UI
   * state (e.g. collapsed, active, selected, renaming, etc.) to the new one. See
   * #AbstractViewItem.update_from_old().
   * After this, reconstruction is complete (see #is_reconstructed()).
   */
  void update_from_old(uiBlock &new_block);

  /**
   * Check if the view is fully (re-)constructed. That means, both the build function and
   * #update_from_old() have finished.
   */
  bool is_reconstructed() const;
};

}  // namespace blender::ui
