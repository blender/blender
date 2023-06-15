/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "AppConfig.h"
#include <iostream>

#include "../system/FreestyleConfig.h"
#include "../system/StringUtils.h"

using namespace std;

#include "BKE_appdir.h"

namespace Freestyle::Config {

Path *Path::_pInstance = nullptr;
Path::Path()
{
  // get the root directory
  // soc
  setRootDir(BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, nullptr));

  _pInstance = this;
}

void Path::setRootDir(const string &iRootDir)
{
  _ProjectDir = iRootDir + string(DIR_SEP) + "freestyle";
  _ModelsPath = "";
  _PatternsPath = _ProjectDir + string(DIR_SEP) + "data" + string(DIR_SEP) + "textures" +
                  string(DIR_SEP) + "variation_patterns" + string(DIR_SEP);
  _BrushesPath = _ProjectDir + string(DIR_SEP) + "data" + string(DIR_SEP) + "textures" +
                 string(DIR_SEP) + "brushes" + string(DIR_SEP);
  _EnvMapDir = _ProjectDir + string(DIR_SEP) + "data" + string(DIR_SEP) + "env_map" +
               string(DIR_SEP);
  _MapsDir = _ProjectDir + string(DIR_SEP) + "data" + string(DIR_SEP) + "maps" + string(DIR_SEP);
}

void Path::setHomeDir(const string &iHomeDir)
{
  _HomeDir = iHomeDir;
}

Path::~Path()
{
  _pInstance = nullptr;
}

Path *Path::getInstance()
{
  return _pInstance;
}

string Path::getEnvVar(const string &iEnvVarName)
{
  string value;
  if (!getenv(iEnvVarName.c_str())) {
    cerr << "Warning: You may want to set the $" << iEnvVarName
         << " environment variable to use Freestyle." << endl
         << "         Otherwise, the current directory will be used instead." << endl;
    value = ".";
  }
  else {
    value = getenv(iEnvVarName.c_str());
  }
  return value;
}

}  // namespace Freestyle::Config
