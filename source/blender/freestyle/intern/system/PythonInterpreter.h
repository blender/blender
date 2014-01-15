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

#ifndef __FREESTYLE_PYTHON_INTERPRETER_H__
#define __FREESTYLE_PYTHON_INTERPRETER_H__

/** \file blender/freestyle/intern/system/PythonInterpreter.h
 *  \ingroup freestyle
 *  \brief Python Interpreter
 *  \author Emmanuel Turquin
 *  \date 17/04/2003
 */

#include <iostream>
#include <Python.h>

#include "StringUtils.h"
#include "Interpreter.h"

//soc
extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_text.h"

#include "BPY_extern.h"
}

namespace Freestyle {

class LIB_SYSTEM_EXPORT PythonInterpreter : public Interpreter
{
public:
	PythonInterpreter()
	{
		_language = "Python";
		_context = 0;
		memset(&_freestyle_bmain, 0, sizeof(Main));
		//Py_Initialize();
	}

	virtual ~PythonInterpreter()
	{
		//Py_Finalize();
	}

	void setContext(bContext *C)
	{
		_context = C;
	}

	int interpretFile(const string& filename)
	{
		initPath();

		ReportList *reports = CTX_wm_reports(_context);
		BKE_reports_clear(reports);
		char *fn = const_cast<char*>(filename.c_str());
#if 0
		int status = BPY_filepath_exec(_context, fn, reports);
#else
		int status;
		Text *text = BKE_text_load(&_freestyle_bmain, fn, G.main->name);
		if (text) {
			status = BPY_text_exec(_context, text, reports, false);
			BKE_text_unlink(&_freestyle_bmain, text);
			BKE_libblock_free(&_freestyle_bmain, text);
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "Cannot open file: %s", fn);
			status = 0;
		}
#endif

		if (status != 1) {
			cerr << "\nError executing Python script from PythonInterpreter::interpretFile" << endl;
			cerr << "File: " << fn << endl;
			cerr << "Errors: " << endl;
			BKE_reports_print(reports, RPT_ERROR);
			return 1;
		}

		// cleaning up
		BKE_reports_clear(reports);

		return 0;
	}

	int interpretText(struct Text *text, const string& name)
	{
		initPath();

		ReportList *reports = CTX_wm_reports(_context);

		BKE_reports_clear(reports);

		if (!BPY_text_exec(_context, text, reports, false)) {
			cerr << "\nError executing Python script from PythonInterpreter::interpretText" << endl;
			cerr << "Name: " << name << endl;
			cerr << "Errors: " << endl;
			BKE_reports_print(reports, RPT_ERROR);
			return 1;
		}

		BKE_reports_clear(reports);

		return 0;
	}

	struct Options
	{
		static void setPythonPath(const string& path)
		{
			_path = path;
		}

		static string getPythonPath()
		{
			return _path;
		}
	};

	void reset()
	{
		Py_Finalize();
		Py_Initialize();
		_initialized = false;
	}

private:
	bContext *_context;
	Main _freestyle_bmain;

	void initPath()
	{
		if (_initialized)
			return;

		vector<string> pathnames;
		StringUtils::getPathName(_path, "", pathnames);

		struct Text *text = BKE_text_add(&_freestyle_bmain, "tmp_freestyle_initpath.txt");
		string cmd = "import sys\n";
		txt_insert_buf(text, const_cast<char*>(cmd.c_str()));

		for (vector<string>::const_iterator it = pathnames.begin(); it != pathnames.end(); ++it) {
			if (!it->empty()) {
				if (G.debug & G_DEBUG_FREESTYLE) {
					cout << "Adding Python path: " << *it << endl;
				}
				cmd = "sys.path.append(r\"" + *it + "\")\n";
				txt_insert_buf(text, const_cast<char *>(cmd.c_str()));
			}
		}

		BPY_text_exec(_context, text, NULL, false);

		// cleaning up
		BKE_text_unlink(&_freestyle_bmain, text);
		BKE_libblock_free(&_freestyle_bmain, text);

		_initialized = true;
	}

	static bool _initialized;
	static string _path;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_PYTHON_INTERPRETER_H__
