/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include <string>

#  include "OCIO_look.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOLook : public Look {
  OCIO_NAMESPACE::ConstLookRcPtr ocio_look_;

  /* View and interface name for view specific look. */
  /* TODO(sergey): Use StringRef when all users supports non-null-terminated strings. */
  std::string view_;
  std::string ui_name_;

 public:
  LibOCIOLook(int index, const OCIO_NAMESPACE::ConstLookRcPtr &ocio_look);

  StringRefNull name() const override
  {
    if (ocio_look_) {
      return ocio_look_->getName();
    }
    return "None";
  }

  StringRefNull ui_name() const override
  {
    if (ui_name_.empty()) {
      return name();
    }
    return ui_name_;
  }

  StringRefNull description() const override
  {
    if (ocio_look_) {
      return ocio_look_->getDescription();
    }
    return "";
  }

  StringRefNull view() const override
  {
    return view_;
  }

  StringRefNull process_space() const override
  {
    if (ocio_look_) {
      return ocio_look_->getProcessSpace();
    }
    return "";
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOLook");
};

}  // namespace blender::ocio

#endif
