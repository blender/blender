/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup openimageio
 */

#include "openimageio_api.h"
#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

extern "C" {

int OIIO_getVersionHex(void)
{
  return openimageio_version();
}

} /* extern "C" */
