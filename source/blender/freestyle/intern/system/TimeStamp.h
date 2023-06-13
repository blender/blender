/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class defining a singleton used as timestamp
 */

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class TimeStamp {
 public:
  static inline TimeStamp *instance()
  {
    return &_instance;
  }

  inline unsigned getTimeStamp() const
  {
    return _time_stamp;
  }

  inline void increment()
  {
    ++_time_stamp;
  }

  inline void reset()
  {
    _time_stamp = 1;
  }

 protected:
  TimeStamp()
  {
    _time_stamp = 1;
  }

  TimeStamp(const TimeStamp &) {}

 private:
  static TimeStamp _instance;
  unsigned _time_stamp;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:TimeStamp")
#endif
};

} /* namespace Freestyle */
