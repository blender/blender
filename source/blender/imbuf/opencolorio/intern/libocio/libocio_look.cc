/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_look.hh"

#if defined(WITH_OPENCOLORIO)

#  include "../view_specific_look.hh"

namespace blender::ocio {

LibOCIOLook::LibOCIOLook(const int index, const OCIO_NAMESPACE::ConstLookRcPtr &ocio_look)
    : ocio_look_(ocio_look)
{
  this->index = index;
  this->is_noop = (ocio_look == nullptr);

  if (ocio_look_) {
    const StringRefNull look_name = ocio_look_->getName();

    StringRef view, ui_name;
    if (split_view_specific_look(look_name, view, ui_name)) {
      view_ = view;
      ui_name_ = ui_name;
    }
  }
}

}  // namespace blender::ocio

#endif
