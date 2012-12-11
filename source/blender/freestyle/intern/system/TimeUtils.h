//
//  Filename         : TimeUtils.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to measure ellapsed time
//  Date of creation : 10/04/2002
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

#ifndef  TIMEUTILS_H
# define TIMEUTILS_H

#include <time.h>
#include "FreestyleConfig.h"

class Chronometer 
{
 public:

  inline Chronometer() {}
  inline ~Chronometer() {}

  inline clock_t start() {
    _start = clock();
    return _start;
  }

  inline double stop() {
    clock_t stop = clock();
    return (double)(stop - _start) / CLOCKS_PER_SEC ;
  }

 private:

  clock_t _start;
};

#endif // TIMEUTILS_H
