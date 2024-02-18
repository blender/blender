/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Python Interpreter
 */

#include <iostream>

#include "Interpreter.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_text.h"

#include "BPY_extern_run.h"

#include "bpy_capi_utils.h"

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
    char *fn = const_cast<char *>(filename.c_str());
#if 0
    bool ok = BPY_run_filepath(_context, fn, nullptr);
#else
    bool ok;
    Text *text = BKE_text_load(&_freestyle_bmain, fn, G_MAIN->filepath);
    if (text) {
      ok = BPY_run_text(_context, text, nullptr, false);
      BKE_id_delete(&_freestyle_bmain, text);
    }
    else {
      cerr << "Cannot open file" << endl;
      ok = false;
    }
#endif

    if (ok == false) {
      cerr << "\nError executing Python script from PythonInterpreter::interpretFile" << endl;
      cerr << "File: " << fn << endl;
      return 1;
    }

    return 0;
  }

  int interpretString(const string &str, const string &name)
  {
    if (!BPY_run_string_eval(_context, nullptr, str.c_str())) {
      cerr << "\nError executing Python script from PythonInterpreter::interpretString" << endl;
      cerr << "Name: " << name << endl;
      return 1;
    }

    return 0;
  }

  int interpretText(struct Text *text, const string &name)
  {
    if (!BPY_run_text(_context, text, nullptr, false)) {
      cerr << "\nError executing Python script from PythonInterpreter::interpretText" << endl;
      cerr << "Name: " << name << endl;
      return 1;
    }
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
