/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_virtual_array.hh"

#include <iostream>

void blender::internal::print_mutable_varray_span_warning()
{
  std::cout << "Warning: Call `save()` to make sure that changes persist in all cases.\n";
}
