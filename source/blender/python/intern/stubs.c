/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"

#include "BPY_extern.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic error "-Wmissing-prototypes"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
/* Suppress unreferenced formal parameter warning. */
#  pragma warning(disable : 4100)
#endif

/* python, will come back */
// void BPY_script_exec(void) {}
// void BPY_python_start(void) {}
void BPY_pyconstraint_exec(struct bPythonConstraint *con,
                           struct bConstraintOb *cob,
                           struct ListBase *targets)
{
}
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct) {}
bool BPY_is_pyconstraint(struct Text *text)
{
  return 0;
}
void BPY_pyconstraint_update(struct Object *owner, struct bConstraint *con) {}
