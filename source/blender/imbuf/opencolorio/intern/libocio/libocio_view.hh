/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include "OCIO_view.hh"

#  include "BLI_string_ref.hh"

namespace blender::ocio {

class LibOCIOView : public View {
  StringRefNull name_;

 public:
  LibOCIOView(const int index, const StringRefNull name) : name_(name)
  {
    this->index = index;
  }

  StringRefNull name() const override
  {
    return name_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOView");
};

}  // namespace blender::ocio

#endif
