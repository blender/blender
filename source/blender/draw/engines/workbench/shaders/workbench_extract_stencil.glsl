/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_depth_info.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_extract_stencil)

void main()
{
  out_stencil_value = 0xFF;
}
