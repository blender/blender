//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef RENDERMONITOR_H
#define RENDERMONITOR_H

extern "C" {
#include "render_types.h"
}

class RenderMonitor
{
public:

  inline RenderMonitor(Render *re) {
    _re = re;
  }

  virtual ~RenderMonitor() {}

  inline bool testBreak() {
    return _re && _re->test_break(_re->tbh);
  }
 
protected:

  Render *_re;
};

#endif // RENDERMONITOR_H
