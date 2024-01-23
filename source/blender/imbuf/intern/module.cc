/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstddef>

#include "BLI_utildefines.h"

#include "IMB_allocimbuf.hh"
#include "IMB_colormanagement_intern.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"

void IMB_init()
{
  imb_refcounter_lock_init();
  imb_mmap_lock_init();
  imb_filetypes_init();
  colormanagement_init();
}

void IMB_exit()
{
  imb_filetypes_exit();
  colormanagement_exit();
  imb_mmap_lock_exit();
  imb_refcounter_lock_exit();
}
