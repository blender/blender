/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. All rights reserved. */

#include "openvdb_capi.h"
#include <openvdb/openvdb.h>

int OpenVDB_getVersionHex()
{
  return openvdb::OPENVDB_LIBRARY_VERSION;
}
