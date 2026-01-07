/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* WORKAROUND: Needed for resources that are declared in MSL. */
#include "ocio_shader_shared.hh"

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos.xy, 0.0f, 1.0f);
  texCoord_interp = texCoord;
}
