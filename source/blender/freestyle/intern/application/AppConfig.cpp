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

#include "AppConfig.h"
#include <iostream>

#include "../system/StringUtils.h"
using namespace std;

extern "C" {
	#include "BLI_path_util.h"
}

namespace Config {
Path* Path::_pInstance = 0;
Path::Path() {
	// get the root directory
	//soc
	setRootDir( BLI_get_folder(BLENDER_SYSTEM_SCRIPTS, NULL) );

	_pInstance = this;
}
void Path::setRootDir(const string& iRootDir) {
	_ProjectDir = iRootDir + string(DIR_SEP.c_str()) + "freestyle";
	_ModelsPath = "";
	_PatternsPath = _ProjectDir + string(DIR_SEP.c_str()) + "data"
			+ string(DIR_SEP.c_str()) + "textures" + string(DIR_SEP.c_str())
			+ "variation_patterns" + string(DIR_SEP.c_str());
	_BrushesPath = _ProjectDir + string(DIR_SEP.c_str()) + "data"
			+ string(DIR_SEP.c_str()) + "textures" + string(DIR_SEP.c_str())
			+ "brushes" + string(DIR_SEP.c_str());
	_PythonPath = _ProjectDir + string(DIR_SEP.c_str())
+ "style_modules" + string(DIR_SEP.c_str()) ;
	if (getenv("PYTHONPATH")) {
		_PythonPath += string(PATH_SEP.c_str()) + string(getenv("PYTHONPATH"));
	}
#ifdef WIN32
	_BrowserCmd = "C:\\Program Files\\Internet Explorer\\iexplore.exe %s";
#else
	_BrowserCmd = "mozilla %s";
#endif
	_HelpIndexPath = _ProjectDir + string(DIR_SEP.c_str()) + "doc"
			+ string(DIR_SEP.c_str()) + "html" + string(DIR_SEP.c_str())
			+ "index.html";
	_EnvMapDir = _ProjectDir + string(DIR_SEP.c_str()) + "data"
			+ string(DIR_SEP.c_str()) + "env_map" + string(DIR_SEP.c_str());
	_MapsDir = _ProjectDir + string(DIR_SEP.c_str()) + "data"
			+ string(DIR_SEP.c_str()) + "maps" + string(DIR_SEP.c_str());
}
void Path::setHomeDir(const string& iHomeDir) {
	_HomeDir = iHomeDir;
}
Path::~Path() {
	_pInstance = 0;
}
Path* Path::getInstance() {
	return _pInstance;
}
string Path::getEnvVar(const string& iEnvVarName) {
	string value;
	if (!getenv(StringUtils::toAscii(iEnvVarName).c_str() ) ) {
		cerr << "Warning: You may want to set the $"
				<< StringUtils::toAscii(iEnvVarName)
				<< " environment variable to use Freestyle." << endl
				<< "         Otherwise, the current directory will be used instead."
				<< endl;
		value = ".";
	} else {
		value = getenv(StringUtils::toAscii(iEnvVarName).c_str() );
	}
	return value;
}

} // End of namepace Config

