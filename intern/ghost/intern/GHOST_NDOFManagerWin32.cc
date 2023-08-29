/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_NDOFManagerWin32.hh"

GHOST_NDOFManagerWin32::GHOST_NDOFManagerWin32(GHOST_System &sys) : GHOST_NDOFManager(sys)
{
  /* pass */
}

/* Whether multi-axis functionality is available (via the OS or driver)
 * does not imply that a device is plugged in or being used. */
bool GHOST_NDOFManagerWin32::available()
{
  /* Always available since RawInput is built into Windows. */
  return true;
}
