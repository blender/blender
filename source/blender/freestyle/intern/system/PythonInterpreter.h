/*
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
 */

#ifndef __FREESTYLE_PYTHON_INTERPRETER_H__
#define __FREESTYLE_PYTHON_INTERPRETER_H__

/** \file
 * \ingroup freestyle
 * \brief Python Interpreter
 */

#include <iostream>

extern "C" {
#include <Python.h>
}

#include "StringUtils.h"
#include "Interpreter.h"

// soc
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

#include "bpy_capi_utils.h"
}

namespace Freestyle {

class PythonInterpreter : public Interpreter {
 public:
  PythonInterpreter()
  {
    _language = "Python";
    _context = 0;
    memset(&_freestyle_bmain, 0, sizeof(Main));
  }

  void setContext(bContext *C)
  {
    _context = C;
  }

  int interpretFile(const string &filename)
  {
    ReportList *reports = CTX_wm_reports(_context);
    BKE_reports_clear(reports);
    char *fn = const_cast<char *>(filename.c_str());
#if 0
    bool ok = BPY_execute_filepath(_context, fn, reports);
#else
    bool ok;
    Text *text = BKE_text_load(&_freestyle_bmain, fn, G_MAIN->name);
    if (text) {
      ok = BPY_execute_text(_context, text, reports, false);
      BKE_id_delete(&_freestyle_bmain, text);
    }
    else {
      BKE_reportf(reports, RPT_ERROR, "Cannot open file: %s", fn);
      ok = false;
    }
#endif

    if (ok == false) {
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

  int interpretString(const string &str, const string &name)
  {
    ReportList *reports = CTX_wm_reports(_context);

    BKE_reports_clear(reports);

    if (!BPY_execute_string(_context, NULL, str.c_str())) {
      BPy_errors_to_report(reports);
      cerr << "\nError executing Python script from PythonInterpreter::interpretString" << endl;
      cerr << "Name: " << name << endl;
      cerr << "Errors: " << endl;
      BKE_reports_print(reports, RPT_ERROR);
      return 1;
    }

    BKE_reports_clear(reports);

    return 0;
  }

  int interpretText(struct Text *text, const string &name)
  {
    ReportList *reports = CTX_wm_reports(_context);

    BKE_reports_clear(reports);

    if (!BPY_execute_text(_context, text, reports, false)) {
      cerr << "\nError executing Python script from PythonInterpreter::interpretText" << endl;
      cerr << "Name: " << name << endl;
      cerr << "Errors: " << endl;
      BKE_reports_print(reports, RPT_ERROR);
      return 1;
    }

    BKE_reports_clear(reports);

    return 0;
  }

  void reset()
  {
    // nothing to do
  }

 private:
  bContext *_context;
  Main _freestyle_bmain;
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_PYTHON_INTERPRETER_H__
