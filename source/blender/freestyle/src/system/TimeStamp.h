//
//  Filename         : TimeStamp.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class defining a singleton used as timestamp
//  Date of creation : 12/12/2002
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

#ifndef  TIMESTAMP_H
# define TIMESTAMP_H

# include "FreestyleConfig.h"

class LIB_SYSTEM_EXPORT TimeStamp 
{
 public:

  static inline TimeStamp* instance() {
    if (_instance == 0)
      _instance = new TimeStamp;
    return _instance;
  }

  inline unsigned getTimeStamp() const {
    return _time_stamp;
  }

  inline void increment() {
    ++_time_stamp;
  }

  inline void reset() {
    _time_stamp = 1;
  }

 protected:
  
  TimeStamp() {
    _time_stamp = 1;
  }

  TimeStamp(const TimeStamp&) {}

 private:

  static TimeStamp* _instance;
  unsigned _time_stamp;
};

#endif // TIMESTAMP_H
