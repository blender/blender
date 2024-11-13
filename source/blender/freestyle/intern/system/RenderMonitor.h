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
  inline RenderMonitor(Render *re)
  {
    _re = re;
  }

  virtual ~RenderMonitor() {}

  inline void setInfo(string info)
  {
    if (_re && !info.empty()) {
      _re->i.infostr = info.c_str();
      _re->stats_draw(&_re->i);
      _re->i.infostr = nullptr;
    }
  }

  inline void progress(float i)
  {
    if (_re) {
      _re->progress(i);
    }
  }

  inline bool testBreak()
  {
    return _re && _re->test_break();
  }

 protected:
  Render *_re;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:RenderMonitor")
};

} /* namespace Freestyle */
