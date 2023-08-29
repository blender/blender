/* SPDX-FileCopyrightText: 2012-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "Predicates1D.h"

#include "../python/Director.h"

namespace Freestyle {

int UnaryPredicate1D::operator()(Interface1D &inter)
{
  return Director_BPy_UnaryPredicate1D___call__(this, inter);
}

int BinaryPredicate1D::operator()(Interface1D &inter1, Interface1D &inter2)
{
  return Director_BPy_BinaryPredicate1D___call__(this, inter1, inter2);
}

} /* namespace Freestyle */
