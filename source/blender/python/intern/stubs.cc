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
void BPY_pyconstraint_exec(struct bPythonConstraint * /*con*/,
                           struct bConstraintOb * /*cob*/,
                           struct ListBase * /*targets*/)
{
}
void BPY_pyconstraint_target(struct bPythonConstraint * /*con*/, struct bConstraintTarget * /*ct*/)
{
}
bool BPY_is_pyconstraint(struct Text * /*text*/)
{
  return 0;
}
void BPY_pyconstraint_update(struct Object * /*owner*/, struct bConstraint * /*con*/) {}
