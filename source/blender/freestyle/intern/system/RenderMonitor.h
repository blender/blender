/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FREESTYLE_RENDER_MONITOR_H__
#define __FREESTYLE_RENDER_MONITOR_H__

/** \file
 * \ingroup freestyle
 * \brief Classes defining the basic "Iterator" design pattern
 */

extern "C" {
#include "render_types.h"
}

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

  virtual ~RenderMonitor()
  {
  }

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

#endif  // __FREESTYLE_RENDER_MONITOR_H__
