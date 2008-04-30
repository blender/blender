//
//  Filename         : Exception.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Singleton to manage exceptions
//  Date of creation : 10/01/2003
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

#ifndef  EXCEPTION_H
# define EXCEPTION_H

# include "FreestyleConfig.h"

class LIB_SYSTEM_EXPORT Exception
{
public:

  typedef enum {
    NO_EXCEPTION,
    UNDEFINED
  } exception_type;

  static int getException() {
    exception_type e = _exception;
    _exception = NO_EXCEPTION;
    return e;
  } 

  static int raiseException(exception_type exception = UNDEFINED) {
    _exception = exception;
    return _exception;
  }

  static void reset() {
    _exception = NO_EXCEPTION;
  }

private:

  static exception_type _exception;
};

#endif // EXCEPTION_H
