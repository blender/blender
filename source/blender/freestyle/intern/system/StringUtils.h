//
//  Filename         : StringUtils.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : String utilities
//  Date of creation : 20/05/2003
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

#ifndef  STRING_UTILS_H
# define STRING_UTILS_H

# include <cstring>
# include <vector>
# include <string>
# include <sstream>
# include <iostream>
# include "FreestyleConfig.h"

//soc
extern "C" {
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
}

using namespace std;

namespace StringUtils {

  LIB_SYSTEM_EXPORT 
  void getPathName(const string& path, const string& base, vector<string>& pathnames);
  string toAscii( const string &str );
  const char* toAscii( const char *str );

  // STL related
  struct ltstr{
  bool operator()(const char* s1, const char* s2) const{
    return strcmp(s1, s2) < 0;
  }
};

} // end of namespace StringUtils

#endif // STRING_UTILS_H
