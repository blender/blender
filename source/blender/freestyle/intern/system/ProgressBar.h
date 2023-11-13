/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to encapsulate a progress bar
 */

#include <string>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

class ProgressBar {
 public:
  inline ProgressBar()
  {
    _numtotalsteps = 0;
    _progress = 0;
  }

  virtual ~ProgressBar() {}

  virtual void reset()
  {
    _numtotalsteps = 0;
    _progress = 0;
  }

  virtual void setTotalSteps(uint n)
  {
    _numtotalsteps = n;
  }

  virtual void setProgress(uint i)
  {
    _progress = i;
  }

  virtual void setLabelText(const string &s)
  {
    _label = s;
  }

  /** accessors */
  inline uint getTotalSteps() const
  {
    return _numtotalsteps;
  }

  inline uint getProgress() const
  {
    return _progress;
  }

  inline string getLabelText() const
  {
    return _label;
  }

 protected:
  uint _numtotalsteps;
  uint _progress;
  string _label;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ProgressBar")
#endif
};

} /* namespace Freestyle */
