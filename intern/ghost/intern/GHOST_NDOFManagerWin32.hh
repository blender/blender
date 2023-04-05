/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GHOST_NDOFManager.hh"

class GHOST_NDOFManagerWin32 : public GHOST_NDOFManager {
 public:
  GHOST_NDOFManagerWin32(GHOST_System &);
  bool available();
};
