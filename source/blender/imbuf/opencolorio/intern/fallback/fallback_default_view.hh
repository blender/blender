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

  bool is_hdr() const override
  {
    return false;
  }

  bool is_wide_gamut() const override
  {
    return false;
  }
};

}  // namespace blender::ocio
