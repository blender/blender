
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

//soc #include <qfileinfo.h>
#include "FreestyleConfig.h"
#include "StringUtils.h"

namespace StringUtils {

  void getPathName(const string& path, const string& base, vector<string>& pathnames) {
    string dir;
	string res;
	char cleaned[FILE_MAX];
    unsigned size = path.size();

    pathnames.push_back(base);

    for (	unsigned pos = 0, sep = path.find(Config::PATH_SEP, pos);
			pos < size;
	 		pos = sep + 1, sep = path.find(Config::PATH_SEP, pos)) {

		if (sep == (unsigned)string::npos)
			sep = size;
		
		dir = path.substr(pos, sep - pos);

		BLI_strncpy(cleaned, dir.c_str(), FILE_MAX);
		BLI_cleanup_file(NULL, cleaned);
		res = toAscii( string(cleaned) );
	
		if (!base.empty())
			res += Config::DIR_SEP + base;
		
		pathnames.push_back(res);
    }
  }

	string toAscii( const string &str ){
		stringstream out("");
		char s;
		
		for(unsigned int i=0; i < str.size() ; i++){
			s =  ((char)(str.at(i) & 0x7F));
			out << s;
		}	
		
		return out.str();
	}
	
	const char* toAscii( const char *str ){
		return toAscii(string(str)).c_str();
	}

  

} // end of namespace StringUtils
