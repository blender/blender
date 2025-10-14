/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstddef>

#include "IMB_colormanagement_intern.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"

void IMB_init()
{
  imb_filetypes_init();
  colormanagement_init();
}

void IMB_exit()
{
  imb_filetypes_exit();
  colormanagement_exit();
}
