/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Configuration definitions
 */

#include <string>

/* Part of `BLI_sys_types.h`, declare here as BLI is not in the include path. */
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char uchar;

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
