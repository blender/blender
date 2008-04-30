//
//  Filename         : BaseObject.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Base Class for most shared objects (Node, Rep).
//                     Defines the addRef, release system.
//                     Inspired by COM IUnknown system.
//  Date of creation : 06/02/2002
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

#ifndef  BASEOBJECT_H
# define BASEOBJECT_H

#include "FreestyleConfig.h"

class LIB_SYSTEM_EXPORT BaseObject
{
public:

  inline BaseObject() {
    _ref_counter = 0;
  }

  virtual ~BaseObject() {}

  /*! At least makes a release on this.
   *  The BaseObject::destroy method must be 
   *  explicitely called at the end of any 
   *  overloaded destroy
   */
  virtual int destroy() {
    return release();
  }

  /*! Increments the reference counter */
  inline int addRef() {
    return ++_ref_counter;
  }

  /*! Decrements the reference counter */
  inline int release() { 
    if (_ref_counter)
      _ref_counter--;
    return _ref_counter;
  }

private:

  unsigned _ref_counter;  
};

#endif // BASEOBJECT_H
