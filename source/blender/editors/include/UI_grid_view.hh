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
 * \ingroup editorui
 *
 * API for simple creation of grid UIs, supporting typically needed features.
 * https://wiki.blender.org/wiki/Source/Interface/Views/Grid_Views
 */

#pragma once

struct wmNotifier;

namespace blender::ui {

class AbstractGridViewItem {
 public:
  virtual ~AbstractGridViewItem() = default;

 protected:
  AbstractGridViewItem() = default;
};

class AbstractGridView {
 public:
  virtual ~AbstractGridView() = default;

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

  // protected:
  virtual void build() = 0;
};

}  // namespace blender::ui
