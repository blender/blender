/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_mask_invert)

void main()
{
  /* Blend mode does the inversion. */
  fragRevealage = frag_color = float4(1.0f);
}
