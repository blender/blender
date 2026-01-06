/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes defining the basic "Iterator" design pattern
 */

#include "render_types.h"

#include "MEM_guardedalloc.h"

namespace Freestyle {

class RenderMonitor {
 public:
  inline RenderMonitor(blender::Render *re)
  {
    _re = re;
  }

  virtual ~RenderMonitor() {}

  inline void setInfo(std::string info)
  {
    if (_re && !info.empty()) {
      _re->i.infostr = info.c_str();
      _re->display->stats_draw(&_re->i);
      _re->i.infostr = nullptr;
    }
  }

  inline void progress(float i)
  {
    if (_re) {
      _re->display->progress(i);
    }
  }

  inline bool testBreak()
  {
    return _re && _re->display->test_break();
  }

 protected:
  blender::Render *_re;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:RenderMonitor")
};

} /* namespace Freestyle */
