/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_implicit_sharing.h"

typedef struct PackedFile {
  int size;
  int seek;
  /**
   * Raw data from the shared file. This data is const because it uses implicit sharing and may be
   * shared with e.g. the undo system.
   */
  const void *data;
  /** Sharing info corresponding to the data above. This is run-time data. */
  const ImplicitSharingInfoHandle *sharing_info;
} PackedFile;
