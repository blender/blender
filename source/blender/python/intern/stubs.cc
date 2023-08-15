/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"

#include "BPY_extern.h"

/* python, will come back */
// void BPY_script_exec(void) {}
// void BPY_python_start(void) {}
void BPY_pyconstraint_exec(bPythonConstraint * /*con*/,
                           bConstraintOb * /*cob*/,
                           ListBase * /*targets*/)
{
}
void BPY_pyconstraint_target(bPythonConstraint * /*con*/, bConstraintTarget * /*ct*/) {}
bool BPY_is_pyconstraint(Text * /*text*/)
{
  return false;
}
void BPY_pyconstraint_update(Object * /*owner*/, bConstraint * /*con*/) {}
