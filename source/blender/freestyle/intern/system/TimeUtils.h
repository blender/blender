/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to measure elapsed time
 */

#include <time.h>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Chronometer {
 public:
  inline Chronometer() {}
  inline ~Chronometer() {}

  inline clock_t start()
  {
    _start = clock();
    return _start;
  }

  inline double stop()
  {
    clock_t stop = clock();
    return (double)(stop - _start) / CLOCKS_PER_SEC;
  }

 private:
  clock_t _start;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Chronometer")
#endif
};

} /* namespace Freestyle */
