// Copyright 2013 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_CAPI_H_
#define OPENSUBDIV_CAPI_H_

#include "opensubdiv_capi_type.h"

#ifdef __cplusplus
extern "C" {
#endif

// Global initialization/deinitialization.
//
// Supposed to be called from main thread.
void openSubdiv_init(void);
void openSubdiv_cleanup(void);

// Bitmask of eOpenSubdivEvaluator.
int openSubdiv_getAvailableEvaluators(void);

int openSubdiv_getVersionHex(void);

#ifdef __cplusplus
}
#endif

#endif  // OPENSUBDIV_CAPI_H_
