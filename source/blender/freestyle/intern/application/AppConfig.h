//
//  Filename         : AppConfig.h
//  Author           : Emmanuel Turquin
//  Purpose          : Configuration file
//  Date of creation : 26/02/2003
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

#ifndef  APP_CONFIG_H
# define APP_CONFIG_H

# include <string>
# include "../system/FreestyleConfig.h"
# include "../system/Precision.h"

using namespace std;

namespace Config {

  class Path{
  protected:
    static Path * _pInstance;
    string _ProjectDir;
    string _ModelsPath;
    string _PatternsPath;
    string _BrushesPath;
    string _PythonPath;
    string _BrowserCmd;
    string _HelpIndexPath;
    string _EnvMapDir;
    string _MapsDir;
    string _HomeDir;
  public:
    Path();
    virtual ~Path();
    static Path* getInstance();

    void setRootDir(const string& iRootDir) ;
    void setHomeDir(const string& iHomeDir) ;

    const string& getProjectDir() const {return _ProjectDir;}
    const string& getModelsPath() const {return _ModelsPath;}
    const string& getPatternsPath() const {return _PatternsPath;}
    const string& getBrushesPath() const {return _BrushesPath;}
    const string& getPythonPath() const {return _PythonPath;}
    const string& getBrowserCmd() const {return _BrowserCmd;}
    const string& getHelpIndexpath() const {return _HelpIndexPath;}
    const string& getEnvMapDir() const {return _EnvMapDir;}
    const string& getMapsDir() const {return _MapsDir;}
    const string& getHomeDir() const {return _HomeDir;}

    static string getEnvVar(const string& iEnvVarName);
    
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

  // Default options
  static const real DEFAULT_SPHERE_RADIUS = 1.0;
  static const real DEFAULT_DKR_EPSILON = 0.0;

} // End of namepace Config

#endif // APP_CONFIG_H
