/* SPDX-FileCopyrightText: 2002-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GHOST_NDOFManager.hh"

/* Event capture is handled within the NDOF manager on Linux,
 * so there's no need for SystemX11 to look for them. */

class GHOST_NDOFManagerUnix : public GHOST_NDOFManager {
 public:
  GHOST_NDOFManagerUnix(GHOST_System &);
  ~GHOST_NDOFManagerUnix();
  bool available();
  bool processEvents();

 private:
  bool available_;
};
