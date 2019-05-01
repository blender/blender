/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"

#include "BPY_extern.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic error "-Wmissing-prototypes"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/* python, will come back */
// void BPY_script_exec(void) {}
// void BPY_python_start(void) {}
void BPY_pyconstraint_exec(struct bPythonConstraint *con,
                           struct bConstraintOb *cob,
                           struct ListBase *targets)
{
}
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct)
{
}
int BPY_is_pyconstraint(struct Text *text)
{
  return 0;
}
void BPY_pyconstraint_update(struct Object *owner, struct bConstraint *con)
{
}
