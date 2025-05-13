/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "OCIO_view.hh"

namespace blender::ocio {

class FallbackDefaultView : public View {
 public:
  FallbackDefaultView()
  {
    this->index = 0;
  }

  StringRefNull name() const override
  {
    return "Standard";
  }
};

}  // namespace blender::ocio
