/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Singleton to manage exceptions
 */

#include "MEM_guardedalloc.h"

namespace Freestyle {

class Exception {
 public:
  typedef enum {
    NO_EXCEPTION,
    UNDEFINED,
  } exception_type;

  static int getException()
  {
    exception_type e = _exception;
    _exception = NO_EXCEPTION;
    return e;
  }

  static int raiseException(exception_type exception = UNDEFINED)
  {
    _exception = exception;
    return _exception;
  }

  static void reset()
  {
    _exception = NO_EXCEPTION;
  }

 private:
  static exception_type _exception;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Exception")
};

} /* namespace Freestyle */
