//
//  Filename         : Module.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Set the type of the module
//  Date of creation : 01/07/2003
//
///////////////////////////////////////////////////////////////////////////////


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

#ifndef  MODULE_H
# define MODULE_H

# include "Canvas.h"
# include "StyleModule.h"

class Module
{
public:

  static void setAlwaysRefresh(bool b = true) {
    getCurrentStyleModule()->setAlwaysRefresh(b);
  }

  static void setCausal(bool b = true) {
    getCurrentStyleModule()->setCausal(b);
  }

  static void setDrawable(bool b = true) {
    getCurrentStyleModule()->setDrawable(b);
  }

  static bool getAlwaysRefresh() {
    return getCurrentStyleModule()->getAlwaysRefresh();
  }

  static bool getCausal() {
    return getCurrentStyleModule()->getCausal();
  }

  static bool getDrawable() {
    return getCurrentStyleModule()->getDrawable();
  }

private:

  static StyleModule* getCurrentStyleModule() {
    Canvas* canvas = Canvas::getInstance();
    return canvas->getCurrentStyleModule();
  }
};

#endif // MODULE_H
