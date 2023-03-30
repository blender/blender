/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes defining the basic "Iterator" design pattern
 */

#include "render_types.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

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
      _re->stats_draw(_re->sdh, &_re->i);
      _re->i.infostr = NULL;
    }
  }

  inline void progress(float i)
  {
    if (_re) {
      _re->progress(_re->prh, i);
    }
  }

  inline bool testBreak()
  {
    return _re && _re->test_break(_re->tbh);
  }

 protected:
  Render *_re;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:RenderMonitor")
#endif
};

} /* namespace Freestyle */
