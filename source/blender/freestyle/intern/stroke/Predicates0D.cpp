/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "Predicates0D.h"

#include "../python/Director.h"

namespace Freestyle {

int UnaryPredicate0D::operator()(Interface0DIterator &it)
{
  return Director_BPy_UnaryPredicate0D___call__(this, it);
}

int BinaryPredicate0D::operator()(Interface0D &inter1, Interface0D &inter2)
{
  return Director_BPy_BinaryPredicate0D___call__(this, inter1, inter2);
}

} /* namespace Freestyle */
