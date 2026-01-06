/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_implicit_sharing.h"

namespace blender {

struct PackedFile {
  int size = 0;
  int seek = 0;
  /**
   * Raw data from the shared file. This data is const because it uses implicit sharing and may be
   * shared with e.g. the undo system.
   */
  const void *data = nullptr;
  /** Sharing info corresponding to the data above. This is run-time data. */
  const ImplicitSharingInfoHandle *sharing_info = nullptr;
};

}  // namespace blender
