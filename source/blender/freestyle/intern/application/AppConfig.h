/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Configuration file
 */

#include <algorithm>
#include <string>

#include "../system/Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

namespace Config {

class Path {
 protected:
  static Path *_pInstance;
  string _ProjectDir;
  string _ModelsPath;
  string _PatternsPath;
  string _BrushesPath;
  string _EnvMapDir;
  string _MapsDir;
  string _HomeDir;

 public:
  Path();
  virtual ~Path();
  static Path *getInstance();

  void setRootDir(const string &iRootDir);
  void setHomeDir(const string &iHomeDir);

  const string &getProjectDir() const
  {
    return _ProjectDir;
  }
  const string &getModelsPath() const
  {
    return _ModelsPath;
  }
  const string &getPatternsPath() const
  {
    return _PatternsPath;
  }
  const string &getBrushesPath() const
  {
    return _BrushesPath;
  }
  const string &getEnvMapDir() const
  {
    return _EnvMapDir;
  }
  const string &getMapsDir() const
  {
    return _MapsDir;
  }
  const string &getHomeDir() const
  {
    return _HomeDir;
  }

  static string getEnvVar(const string &iEnvVarName);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Config:Path")
#endif
};

//
// Configuration, default values
//
//////////////////////////////////////////////////////////////

// Application
static const string APPLICATION_NAME("APPNAME");
static const string APPLICATION_VERSION("APPVERSION");

// ViewMap
static const string VIEWMAP_EXTENSION("vm");
static const string VIEWMAP_MAGIC("ViewMap File");
static const string VIEWMAP_VERSION("1.9");

// Style modules
static const string STYLE_MODULE_EXTENSION("py");
static const string STYLE_MODULES_LIST_EXTENSION("sml");

// Options
static const string OPTIONS_DIR("." + APPLICATION_NAME);
static const string OPTIONS_FILE("options.xml");
static const string OPTIONS_CURRENT_DIRS_FILE("current_dirs.xml");
static const string OPTIONS_QGLVIEWER_FILE("qglviewer.xml");

}  // namespace Config

} /* namespace Freestyle */
