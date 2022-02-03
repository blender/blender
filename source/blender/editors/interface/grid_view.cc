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
 * \ingroup edinterface
 */

#include "WM_types.h"

#include "UI_interface.h"

#include "UI_grid_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */

bool AbstractGridView::listen(const wmNotifier &) const
{
  /* Nothing by default. */
  return false;
}

}  // namespace blender::ui

using namespace blender::ui;

/* ---------------------------------------------------------------------- */

bool UI_grid_view_listen_should_redraw(const uiGridViewHandle *view_handle,
                                       const wmNotifier *notifier)
{
  const AbstractGridView &view = *reinterpret_cast<const AbstractGridView *>(view_handle);
  return view.listen(*notifier);
}
