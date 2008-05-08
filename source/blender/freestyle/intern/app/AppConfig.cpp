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
using namespace std;
namespace Config{
  Path* Path::_pInstance = 0;
  Path::Path(){
    // get the home directory
    _HomeDir = getEnvVar("HOME");
    // get the root directory
    setRootDir(getEnvVar("FREESTYLE_DIR"));
    //setRootDir(QString("."));
    _pInstance = this;
  }
  void Path::setRootDir(const QString& iRootDir){
    _ProjectDir = iRootDir;
    _ModelsPath = "";
    _PatternsPath = _ProjectDir +
					     QString(DIR_SEP.c_str()) +
					     "data" +
					     QString(DIR_SEP.c_str()) +
					     "textures" +
					     QString(DIR_SEP.c_str()) +
					     "variation_patterns" +
					     QString(DIR_SEP.c_str());
    _BrushesPath = _ProjectDir +
					     QString(DIR_SEP.c_str()) +
					     "data" +
					     QString(DIR_SEP.c_str()) +
					     "textures" +
					     QString(DIR_SEP.c_str()) +
					     "brushes" +
					     QString(DIR_SEP.c_str());
    _PythonPath = _ProjectDir + 
              QString(DIR_SEP.c_str()) + 
              "python" + 
              QString(PATH_SEP.c_str()) +
              _ProjectDir +
					    QString(DIR_SEP.c_str()) +
              "style_modules" +
              QString(DIR_SEP.c_str()) ;
    if (getenv("PYTHONPATH")) {
      _PythonPath += QString(PATH_SEP.c_str()) + QString(getenv("PYTHONPATH"));
    }
#ifdef WIN32
    _BrowserCmd = "C:\\Program Files\\Internet Explorer\\iexplore.exe %s";
#else
    _BrowserCmd = "mozilla %s";
#endif
    _HelpIndexPath = _ProjectDir +
					  QString(DIR_SEP.c_str()) +
					  "doc" +
					  QString(DIR_SEP.c_str()) +
					  "html" +
					  QString(DIR_SEP.c_str()) +
					  "index.html";
    _PapersDir = _ProjectDir +
					  QString(DIR_SEP.c_str()) +
					  "data" +
					  QString(DIR_SEP.c_str()) +
					  "textures" +
					  QString(DIR_SEP.c_str()) +
					  "papers" +
					  QString(DIR_SEP.c_str());
    _EnvMapDir = _ProjectDir +
				   QString(DIR_SEP.c_str()) +
				   "data" +
				   QString(DIR_SEP.c_str()) +
				   "env_map" +
				   QString(DIR_SEP.c_str());
    _MapsDir = _ProjectDir +
				   QString(DIR_SEP.c_str()) +
				   "data" +
           QString(DIR_SEP.c_str()) +
				   "maps" +
				   QString(DIR_SEP.c_str());
  }
  void Path::setHomeDir(const QString& iHomeDir){
    _HomeDir = iHomeDir;
  }
  Path::~Path(){
    _pInstance = 0;
  }
  Path* Path::getInstance() {
    return _pInstance;
  }
  QString Path::getEnvVar(const QString& iEnvVarName){
    QString value;
    if (!getenv(iEnvVarName.toAscii().data())) {
      cerr << "Warning: You may want to set the $"<< iEnvVarName.toAscii().data()
		  << " environment variable to use " << QString(Config::APPLICATION_NAME).toAscii().data() << "." << endl
	    << "         Otherwise, the current directory will be used instead." << endl;
      value = ".";
    }else{
      value = getenv(iEnvVarName.toAscii().data());
    } 
    return value;
  }

} // End of namepace Config

