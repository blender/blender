/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class representing a style module
 */

#include <iostream>
#include <string>

#include "Operators.h"
#include "StrokeLayer.h"
#include "StrokeShader.h"

#include "../system/Interpreter.h"
#include "../system/StringUtils.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

class StyleModule {
 public:
  StyleModule(const string &file_name, Interpreter *inter) : _file_name(file_name)
  {
    _always_refresh = false;
    _causal = false;
    _drawable = true;
    _modified = true;
    _displayed = true;
    _inter = inter;
  }

  virtual ~StyleModule() {}

  StrokeLayer *execute()
  {
    if (!_inter) {
      cerr << "Error: no interpreter was found to execute the script" << endl;
      return NULL;
    }

    if (!_drawable) {
      cerr << "Error: not drawable" << endl;
      return NULL;
    }

    Operators::reset();

    if (interpret()) {
      cerr << "Error: interpretation failed" << endl;
      Operators::reset();
      return NULL;
    }

    Operators::StrokesContainer *strokes_set = Operators::getStrokesSet();
    if (strokes_set->empty()) {
      cerr << "Error: strokes set empty" << endl;
      Operators::reset();
      return NULL;
    }

    StrokeLayer *sl = new StrokeLayer;
    for (Operators::StrokesContainer::iterator it = strokes_set->begin(); it != strokes_set->end();
         ++it)
    {
      sl->AddStroke(*it);
    }

    Operators::reset();
    return sl;
  }

 protected:
  virtual int interpret()
  {
    return _inter->interpretFile(_file_name);
  }

 public:
  // accessors
  const string getFileName() const
  {
    return _file_name;
  }

  bool getAlwaysRefresh() const
  {
    return _always_refresh;
  }

  bool getCausal() const
  {
    return _causal;
  }

  bool getDrawable() const
  {
    return _drawable;
  }

  bool getModified() const
  {
    return _modified;
  }

  bool getDisplayed() const
  {
    return _displayed;
  }

  // modifiers
  void setFileName(const string &file_name)
  {
    _file_name = file_name;
  }

  void setAlwaysRefresh(bool b = true)
  {
    _always_refresh = b;
  }

  void setCausal(bool b = true)
  {
    _causal = b;
  }

  void setDrawable(bool b = true)
  {
    _drawable = b;
  }

  void setModified(bool b = true)
  {
    if (_always_refresh) {
      return;
    }
    _modified = b;
  }

  void setDisplayed(bool b = true)
  {
    _displayed = b;
  }

 private:
  string _file_name;
  bool _always_refresh;
  bool _causal;
  bool _drawable;
  bool _modified;
  bool _displayed;

 protected:
  Interpreter *_inter;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StyleModule")
#endif
};

} /* namespace Freestyle */
