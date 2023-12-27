/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup openimageio
 */

#include "openimageio_api.h"

#include <OpenImageIO/imageio.h>

#include "BLI_threads.h"

OIIO_NAMESPACE_USING

extern "C" {

void OIIO_init()
{
  /* Make OIIO thread pool follow Blender number of threads override. */
  const int threads_override = BLI_system_num_threads_override_get();
  if (threads_override) {
    OIIO::attribute("threads", threads_override);
  }

  /* As of OpenEXR 3.2.1 there are still issues related to the use of OpenEXR Core. */
  OIIO::attribute("openexr:core", 0);
}

int OIIO_getVersionHex()
{
  return openimageio_version();
}

} /* extern "C" */
