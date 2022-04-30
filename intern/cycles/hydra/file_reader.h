/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "hydra/config.h"

#include "session/session.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesFileReader {
 public:
  static void read(Session *session, const char *filepath, const bool use_camera = true);
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
