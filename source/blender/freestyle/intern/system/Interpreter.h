/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Base Class of all script interpreters
 */

#include <string>

#include "MEM_guardedalloc.h"

using namespace std;

namespace Freestyle {

class Interpreter {
 public:
  Interpreter()
  {
    _language = "Unknown";
  }

  virtual ~Interpreter() {}

  virtual int interpretFile(const string &filename) = 0;

  virtual string getLanguage() const
  {
    return _language;
  }

  virtual void reset() = 0;

 protected:
  string _language;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Interpreter")
};

} /* namespace Freestyle */
