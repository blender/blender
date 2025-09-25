/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_outline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_outline_prepass)

void main()
{
  out_object_id = interp.ob_id;
}
