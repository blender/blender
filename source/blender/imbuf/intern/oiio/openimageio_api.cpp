/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup openimageio
 */

#include "openimageio_api.h"
#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

extern "C" {

int OIIO_getVersionHex()
{
  return openimageio_version();
}

} /* extern "C" */
