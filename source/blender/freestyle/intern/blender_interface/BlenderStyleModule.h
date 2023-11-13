/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include "../stroke/StyleModule.h"
#include "../system/PythonInterpreter.h"

#include "BLI_utildefines.h"  // BLI_assert()

struct Text;

namespace Freestyle {

class BufferedStyleModule : public StyleModule {
 public:
  BufferedStyleModule(const string &buffer, const string &file_name, Interpreter *inter)
      : StyleModule(file_name, inter)
  {
    _buffer = buffer;
  }

  virtual ~BufferedStyleModule() {}

 protected:
  virtual int interpret()
  {
    PythonInterpreter *py_inter = dynamic_cast<PythonInterpreter *>(_inter);
    BLI_assert(py_inter != 0);
    return py_inter->interpretString(_buffer, getFileName());
  }

 private:
  string _buffer;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BufferedStyleModule")
#endif
};

class BlenderStyleModule : public StyleModule {
 public:
  BlenderStyleModule(struct Text *text, const string &name, Interpreter *inter)
      : StyleModule(name, inter)
  {
    _text = text;
  }

  virtual ~BlenderStyleModule() {}

 protected:
  virtual int interpret()
  {
    PythonInterpreter *py_inter = dynamic_cast<PythonInterpreter *>(_inter);
    BLI_assert(py_inter != 0);
    return py_inter->interpretText(_text, getFileName());
  }

 private:
  struct Text *_text;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderStyleModule")
#endif
};

} /* namespace Freestyle */
