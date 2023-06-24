/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_DisplayManagerNULL class.
 */

#pragma once

#include "GHOST_DisplayManager.hh"
#include "GHOST_SystemHeadless.hh"

class GHOST_SystemHeadless;

class GHOST_DisplayManagerNULL : public GHOST_DisplayManager {
 public:
  GHOST_DisplayManagerNULL() : GHOST_DisplayManager()
  { /* nop */
  }
  GHOST_TSuccess getNumDisplays(uint8_t & /*numDisplays*/) const override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess getNumDisplaySettings(uint8_t /*display*/,
                                       int32_t & /*numSettings*/) const override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess getDisplaySetting(uint8_t /*display*/,
                                   int32_t /*index*/,
                                   GHOST_DisplaySetting & /*setting*/) const override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess getCurrentDisplaySetting(uint8_t display,
                                          GHOST_DisplaySetting &setting) const override
  {
    return getDisplaySetting(display, int32_t(0), setting);
  }
  GHOST_TSuccess setCurrentDisplaySetting(uint8_t /*display*/,
                                          const GHOST_DisplaySetting & /*setting*/) override
  {
    return GHOST_kSuccess;
  }
};
