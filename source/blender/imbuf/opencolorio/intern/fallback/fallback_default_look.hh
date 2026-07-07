/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "OCIO_look.hh"

namespace blender::ocio {

class FallbackDefaultLook : public Look {
 public:
  FallbackDefaultLook()
  {
    this->index = 0;
    this->is_noop = true;
  }

  StringRefNull name() const override
  {
    return "None";
  }

  StringRefNull ui_name() const override
  {
    return name();
  }

  StringRefNull description() const override
  {
    return "";
  }

  StringRefNull view() const override
  {
    return "";
  }

  StringRefNull process_space() const override
  {
    return "";
  }
};

}  // namespace blender::ocio
