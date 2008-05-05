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

# include <qstring.h>
# include "../system/FreestyleConfig.h"
# include "../system/Precision.h"

using namespace std;

namespace Config {

  class Path{
  protected:
    static Path * _pInstance;
    QString _ProjectDir;
    QString _ModelsPath;
    QString _PatternsPath;
    QString _BrushesPath;
    QString _PythonPath;
    QString _BrowserCmd;
    QString _HelpIndexPath;
    QString _PapersDir;
    QString _EnvMapDir;
    QString _MapsDir;
    QString _HomeDir;
  public:
    Path();
    virtual ~Path();
    static Path* getInstance();

    void setRootDir(const QString& iRootDir) ;
    void setHomeDir(const QString& iHomeDir) ;

    const QString& getProjectDir() const {return _ProjectDir;}
    const QString& getModelsPath() const {return _ModelsPath;}
    const QString& getPatternsPath() const {return _PatternsPath;}
    const QString& getBrushesPath() const {return _BrushesPath;}
    const QString& getPythonPath() const {return _PythonPath;}
    const QString& getBrowserCmd() const {return _BrowserCmd;}
    const QString& getHelpIndexpath() const {return _HelpIndexPath;}
    const QString& getPapersDir() const {return _PapersDir;}
    const QString& getEnvMapDir() const {return _EnvMapDir;}
    const QString& getMapsDir() const {return _MapsDir;}
    const QString& getHomeDir() const {return _HomeDir;}

    static QString getEnvVar(const QString& iEnvVarName);
    
  };

  //
  // Configuration, default values
  //
  //////////////////////////////////////////////////////////////

  // Application
  static const QString APPLICATION_NAME(APPNAME);
  static const QString APPLICATION_VERSION(APPVERSION);

  // ViewMap
  static const QString VIEWMAP_EXTENSION("vm");
  static const QString VIEWMAP_MAGIC("ViewMap File");
  static const QString VIEWMAP_VERSION("1.9");

  // Style modules
  static const QString STYLE_MODULE_EXTENSION("py");
  static const QString STYLE_MODULES_LIST_EXTENSION("sml");

  // Options
  static const QString OPTIONS_DIR("." + APPLICATION_NAME);
  static const QString OPTIONS_FILE("options.xml");
  static const QString OPTIONS_CURRENT_DIRS_FILE("current_dirs.xml");
  static const QString OPTIONS_QGLVIEWER_FILE("qglviewer.xml");

  // Default options
  static const real DEFAULT_SPHERE_RADIUS = 1.0;
  static const real DEFAULT_DKR_EPSILON = 0.0;

  // Papers
  static const QString DEFAULT_PAPER_TEXTURE("whitepaper.jpg");

  // Help & About texts
  static const QString HELP_FILE("help.html");
  static const QString ABOUT_STRING
    (
     "<CENTER><H2>" + APPLICATION_NAME + " " + APPLICATION_VERSION + "</H2>"
     "<P>A programmable line drawing system</P></CENTER>"
     "<UL>"
     "<LI>Frédo Durand"
     "<LI>Stéphane Grabli"
     "<LI>François Sillion"
     "<LI>Emmanuel Turquin"
     "</UL>"
     "<CENTER><B>(C) Artis 2003</B></CENTER>"
     );

} // End of namepace Config

#endif // APP_CONFIG_H
