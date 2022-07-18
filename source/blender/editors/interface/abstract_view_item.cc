/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "UI_abstract_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */
/** \name View Reconstruction
 * \{ */

void AbstractViewItem::update_from_old(const AbstractViewItem &old)
{
  is_active_ = old.is_active_;
}

/** \} */

}  // namespace blender::ui
