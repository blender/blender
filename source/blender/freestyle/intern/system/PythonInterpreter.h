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

# include <Python.h>
# include <iostream>
# include "StringUtils.h"
# include "Interpreter.h"

//soc
extern "C" {
#include "MEM_guardedalloc.h"
#include "DNA_text_types.h"
#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_text.h"
#include "BKE_library.h"
#include "BPY_extern.h"
}

class LIB_SYSTEM_EXPORT PythonInterpreter : public Interpreter
{
 public:

  PythonInterpreter() {
    _language = "Python";
	_context = 0;
    //Py_Initialize();
  }

  virtual ~PythonInterpreter() {
    //Py_Finalize();
  }

  void setContext(bContext *C) {
	_context = C;
  }

  int interpretFile(const string& filename) {

	initPath();
	
	ReportList* reports = CTX_wm_reports(_context);
	BKE_reports_clear(reports);
	char *fn = const_cast<char*>(filename.c_str());
#if 0
	int status = BPY_filepath_exec(_context, fn, reports);
#else
	int status;
	Text *text = BKE_text_load(fn, G.main->name);
	if (text) {
		status = BPY_text_exec(_context, text, reports, false);
		BKE_text_unlink(G.main, text);
		BKE_libblock_free(&G.main->text, text);
	} else {
		BKE_reportf(reports, RPT_ERROR, "Cannot open file: %s", fn);
		status = 0;
	}
#endif

	if (status != 1) {
		cout << "\nError executing Python script from PythonInterpreter::interpretFile" << endl;
		cout << "File: " << fn << endl;
		cout << "Errors: " << endl;
		BKE_reports_print(reports, RPT_ERROR);
		return 1;
	}

	// cleaning up
	BKE_reports_clear(reports);
	
	return 0;
  }

  int interpretText(struct Text *text, const string& name) {

	initPath();

	ReportList* reports = CTX_wm_reports(_context);

	BKE_reports_clear(reports);

	if (!BPY_text_exec(_context, text, reports, false)) {
		cout << "\nError executing Python script from PythonInterpreter::interpretText" << endl;
		cout << "Name: " << name << endl;
		cout << "Errors: " << endl;
		BKE_reports_print(reports, RPT_ERROR);
		return 1;
	}

	BKE_reports_clear(reports);

	return 0;
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

  bContext* _context;

  void initPath() {
	if (_initialized)
		return;

	vector<string> pathnames;
	StringUtils::getPathName(_path, "", pathnames);
	
	struct Text *text = BKE_text_add("tmp_freestyle_initpath.txt");
	string cmd = "import sys\n";
	txt_insert_buf(text, const_cast<char*>(cmd.c_str()));
	
	for (vector<string>::const_iterator it = pathnames.begin(); it != pathnames.end();++it) {
		if ( !it->empty() ) {
			cout << "Adding Python path: " << *it << endl;
			cmd = "sys.path.append(r\"" + *it + "\")\n";
			txt_insert_buf(text, const_cast<char*>(cmd.c_str()));
		}
	}
	
	BPY_text_exec(_context, text, NULL, false);
	
	// cleaning up
	BKE_text_unlink(G.main, text);
	BKE_libblock_free(&G.main->text, text);
	
	//PyRun_SimpleString("from Freestyle import *");
    _initialized = true;
  }

  static bool	_initialized;
  static string _path;
};

#endif // PYTHON_INTERPRETER_H
