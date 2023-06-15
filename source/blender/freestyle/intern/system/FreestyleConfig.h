/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Configuration definitions
 */

#include <string>

using namespace std;

namespace Freestyle {

namespace Config {

/* Directory separators. */

/* TODO: Use Blender's stuff for such things! */
#ifdef WIN32
static const string DIR_SEP("\\");
static const string PATH_SEP(";");
#else
static const string DIR_SEP("/");
static const string PATH_SEP(":");
#endif  // WIN32

}  // end of namespace Config

} /* namespace Freestyle */
