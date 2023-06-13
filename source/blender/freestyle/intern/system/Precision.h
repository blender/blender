/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Define the float precision used in the program
 */

namespace Freestyle {

typedef double real;

#ifndef SWIG
static const real M_EPSILON = 0.00000001;
#endif  // SWIG

} /* namespace Freestyle */
