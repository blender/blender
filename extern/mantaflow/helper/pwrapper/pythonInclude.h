/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Base class for particle systems
 *
 ******************************************************************************/

#ifndef _PYTHONINCLUDE_H
#define _PYTHONINCLUDE_H

#if defined(WIN32) || defined(_WIN32)

// note - we have to include these first!
#  include <string>
#  include <vector>
#  include <iostream>

#endif

// the PYTHON_DEBUG_WITH_RELEASE define enables linking with python debug libraries
#if (defined(_DEBUG) || (DEBUG == 1)) && defined(DEBUG_PYTHON_WITH_RELEASE)

// special handling, disable linking with debug version of python libs
#  undef _DEBUG
#  define NDEBUG
#  include <Python.h>
#  if NUMPY == 1
#    define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#    include "numpy/arrayobject.h"
#  endif
#  define _DEBUG
#  undef NDEBUG

#else
#  include <Python.h>
#  if NUMPY == 1
#    define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#    include "numpy/arrayobject.h"
#  endif
#endif

#endif
