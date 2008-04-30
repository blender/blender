//
//  Filename         : PythonInterpreter.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Python Interpreter
//  Date of creation : 17/04/2003
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

#ifndef  PYTHON_INTERPRETER_H
# define PYTHON_INTERPRETER_H

# include <iostream>
# include <Python.h>
# include "StringUtils.h"
# include "Interpreter.h"

class LIB_SYSTEM_EXPORT PythonInterpreter : public Interpreter
{
 public:

  PythonInterpreter() {
    _language = "Python";
    Py_Initialize();
  }

  virtual ~PythonInterpreter() {
    Py_Finalize();
  }

  int interpretCmd(const string& cmd) {
    initPath();
    char* c_cmd = strdup(cmd.c_str());
    int err = PyRun_SimpleString(c_cmd);
    free(c_cmd);
    return err;
  }

  int interpretFile(const string& filename) {
    initPath();
    string cmd("execfile(\"" + filename + "\")");
    char* c_cmd = strdup(cmd.c_str());
    int err = PyRun_SimpleString(c_cmd);
    free(c_cmd);
    return err;
  }

  struct Options
  {
    static void setPythonPath(const string& path) {
      _path = path;
    }

    static string getPythonPath() {
      return _path;
    }
  };

  void reset() {
    Py_Finalize();
    Py_Initialize();
    _initialized = false;
  }

private:

  static void initPath() {
    if (_initialized)
      return;
    PyRun_SimpleString("import sys");
    vector<string> pathnames;
    StringUtils::getPathName(_path, "", pathnames);
    string cmd;
    char* c_cmd;
    for (vector<string>::const_iterator it = pathnames.begin();
	 it != pathnames.end();
	 ++it) {
      cmd = "sys.path.append(\"" + *it + "\")";
      c_cmd = strdup(cmd.c_str());
      PyRun_SimpleString(c_cmd);
      free(c_cmd);
    }
    //    PyRun_SimpleString("from Freestyle import *");
    _initialized = true;
  }

  static bool	_initialized;
  static string _path;
};

#endif // PYTHON_INTERPRETER_H
