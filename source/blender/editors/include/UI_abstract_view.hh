/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * Base for all views (UIs to display data sets), supporting common features.
 * https://wiki.blender.org/wiki/Source/Interface/Views
 *
 * One of the most important responsibilities of the base class is managing reconstruction,
 * enabling state that is persistent over reconstructions/redraws.
 */

#pragma once

#include <array>
#include <memory>

#include "BLI_span.hh"

struct wmNotifier;

namespace blender::ui {

class AbstractView {
  bool is_reconstructed_ = false;
  /**
   * Only one item can be renamed at a time. So rather than giving each item an own rename buffer
   * (which just adds unused memory in most cases), have one here that is managed by the view.
   *
   * This fixed-size buffer is needed because that's what the rename button requires. In future we
   * may be able to bind the button to a `std::string` or similar.
   */
  std::unique_ptr<std::array<char, MAX_NAME>> rename_buffer_;

 public:
  virtual ~AbstractView() = default;

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

  /** Only one item can be renamed at a time. */
  bool is_renaming() const;
  /** \return If renaming was started successfully. */
  bool begin_renaming();
  void end_renaming();
  Span<char> get_rename_buffer() const;
  MutableSpan<char> get_rename_buffer();

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
