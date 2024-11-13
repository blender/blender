/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class defining a singleton used as timestamp
 */

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

namespace Freestyle {

class TimeStamp {
 public:
  static inline TimeStamp *instance()
  {
    return &_instance;
  }

  inline uint getTimeStamp() const
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
  uint _time_stamp;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:TimeStamp")
};

} /* namespace Freestyle */
