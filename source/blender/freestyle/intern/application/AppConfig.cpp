/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/application/AppConfig.cpp
 *  \ingroup freestyle
 */

#include "AppConfig.h"
#include <iostream>

#include "../system/FreestyleConfig.h"
#include "../system/StringUtils.h"

using namespace std;

extern "C" {
#include "BKE_appdir.h"
}

namespace Freestyle {

namespace Config {

Path *Path::_pInstance = 0;
Path::Path()
{
	// get the root directory
	// soc
	setRootDir(BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, NULL));

	_pInstance = this;
}

void Path::setRootDir(const string& iRootDir)
{
	_ProjectDir = iRootDir + string(DIR_SEP.c_str()) + "freestyle";
	_ModelsPath = "";
	_PatternsPath = _ProjectDir + string(DIR_SEP.c_str()) + "data" + string(DIR_SEP.c_str()) + "textures" +
	                string(DIR_SEP.c_str()) + "variation_patterns" + string(DIR_SEP.c_str());
	_BrushesPath = _ProjectDir + string(DIR_SEP.c_str()) + "data" + string(DIR_SEP.c_str()) + "textures" +
	               string(DIR_SEP.c_str()) + "brushes" + string(DIR_SEP.c_str());
	_PythonPath = _ProjectDir + string(DIR_SEP.c_str()) + "modules" + string(DIR_SEP.c_str());
	if (getenv("PYTHONPATH")) {
		_PythonPath += string(PATH_SEP.c_str()) + string(getenv("PYTHONPATH"));
	}
	_EnvMapDir = _ProjectDir + string(DIR_SEP.c_str()) + "data" + string(DIR_SEP.c_str()) + "env_map" +
	             string(DIR_SEP.c_str());
	_MapsDir = _ProjectDir + string(DIR_SEP.c_str()) + "data" + string(DIR_SEP.c_str()) + "maps" +
	           string(DIR_SEP.c_str());
}

void Path::setHomeDir(const string& iHomeDir)
{
	_HomeDir = iHomeDir;
}

Path::~Path()
{
	_pInstance = 0;
}

Path *Path::getInstance()
{
	return _pInstance;
}

string Path::getEnvVar(const string& iEnvVarName)
{
	string value;
	if (!getenv(iEnvVarName.c_str())) {
		cerr << "Warning: You may want to set the $" << iEnvVarName <<
		        " environment variable to use Freestyle." << endl <<
		        "         Otherwise, the current directory will be used instead." << endl;
		value = ".";
	}
	else {
		value = getenv(iEnvVarName.c_str());
	}
	return value;
}

} // End of namepace Config

} /* namespace Freestyle */
