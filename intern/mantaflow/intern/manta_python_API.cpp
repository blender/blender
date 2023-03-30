/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup intern_mantaflow
 */

#include "manta_python_API.h"
#include "manta.h"

PyObject *Manta_initPython(void)
{
  return Pb::PyInit_manta_main();
}
